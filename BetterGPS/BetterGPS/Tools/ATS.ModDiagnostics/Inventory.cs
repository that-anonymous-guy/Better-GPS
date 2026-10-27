using System.Buffers.Binary;
using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;

record Mount(string Path,string Pool,int Order,string Name,bool Directory);
record ZipRange(string Name,long Start,long End,long ExpandedBytes,long CompressedBytes);
static class Inventory
{
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern uint QueryDosDevice(string device,StringBuilder target,int capacity);
    static readonly Dictionary<string,string> Devices=DeviceNames();
    static Dictionary<string,string> DeviceNames()
    {
        var result=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        foreach(var drive in System.IO.Directory.GetLogicalDrives())
        {var buffer=new StringBuilder(1024);if(QueryDosDevice(drive[..2],buffer,buffer.Capacity)>0)result[buffer.ToString()]=drive[..2];}
        return result;
    }
    internal static string Normalize(string path)
    {
        path=path.Replace('/','\\');if(path.StartsWith("\\??\\"))path=path[4..];
        foreach(var device in Devices)if(path.StartsWith(device.Key+"\\",StringComparison.OrdinalIgnoreCase))return device.Value+path[device.Key.Length..];
        return path.TrimEnd('\\');
    }
    internal static string ReadLog(string docs)
    {
        var path=System.IO.Path.Combine(docs,"game.log.txt");if(!File.Exists(path))return "";
        using var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete);
        if(stream.Length>16*1024*1024)
        {
            // Keep startup mounts as well as recent errors in an unlimited game log.
            var head=new byte[4*1024*1024];stream.ReadExactly(head);stream.Position=stream.Length-12*1024*1024;
            using var tail=new StreamReader(stream);return Encoding.UTF8.GetString(head)+"\n[ATSModDiagnostics: middle log omitted]\n"+tail.ReadToEnd();
        }
        using var reader=new StreamReader(stream);return reader.ReadToEnd();
    }
    internal static List<string> Issues(string log)=>log.Split('\n').Where(l=>l.Contains("<ERROR>")||l.Contains("<WARNING>")).Select(l=>l.Trim()).Distinct().Take(500).ToList();
    internal static List<Mount> Mounts(string log)
    {
        var result=new List<Mount>();
        foreach(Match match in Regex.Matches(log,@"\[fs\] device (.+?) mounted to (core|scs|mod) pool\."))
        {
            string path=Normalize(match.Groups[1].Value.Trim().Trim('"')),pool=match.Groups[2].Value;
            result.RemoveAll(m=>m.Path.Equals(path,StringComparison.OrdinalIgnoreCase));
            string name=System.IO.Path.GetFileName(path);var workshop=Regex.Match(path,@"[\\/]workshop[\\/]content[\\/]270880[\\/](\d+)",RegexOptions.IgnoreCase);
            if(workshop.Success)name="Workshop "+workshop.Groups[1].Value+" · "+name;
            result.Add(new(path,pool,result.Count,name,System.IO.Directory.Exists(path)));
        }
        return result;
    }
    internal static Mount? Owner(IEnumerable<Mount> mounts,string physical)=>mounts.Where(m=>physical.Equals(m.Path,StringComparison.OrdinalIgnoreCase)||
        m.Directory&&physical.StartsWith(m.Path+"\\",StringComparison.OrdinalIgnoreCase)).OrderByDescending(m=>m.Path.Length).FirstOrDefault();
    internal static string DisplayName(Mount mount)
    {
        try
        {
            string? text=null;
            if(mount.Directory)
            {
                var path=System.IO.Path.Combine(mount.Path,"manifest.sii");if(File.Exists(path)&&new FileInfo(path).Length<1024*1024)text=File.ReadAllText(path);
            }
            else
            {
                using var stream=new FileStream(mount.Path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite);
                using var zip=new ZipArchive(stream,ZipArchiveMode.Read);var entry=zip.GetEntry("manifest.sii");
                if(entry is {Length:<1048576}){using var reader=new StreamReader(entry.Open());text=reader.ReadToEnd();}
            }
            if(text is not null){var match=Regex.Match(text,"display_name\\s*:\\s*\"([^\"]+)\"");if(match.Success)return match.Groups[1].Value+" · "+mount.Name;}
        }
        catch(Exception e) when(e is IOException or InvalidDataException or UnauthorizedAccessException){}
        return mount.Name;
    }
    internal static List<ZipRange> ZipIndex(string path)
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        using var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite);using var reader=new BinaryReader(stream,Encoding.UTF8,true);
        if(stream.Length<22)throw new InvalidDataException("Pas une archive ZIP indexable (HashFS SCS ou format non reconnu)");
        int tailSize=(int)Math.Min(stream.Length,65557);stream.Position=stream.Length-tailSize;var tail=reader.ReadBytes(tailSize);int eocd=-1;
        for(int i=tail.Length-22;i>=0;i--)if(BinaryPrimitives.ReadUInt32LittleEndian(tail.AsSpan(i))==0x06054b50&&i+22+BinaryPrimitives.ReadUInt16LittleEndian(tail.AsSpan(i+20))==tail.Length){eocd=i;break;}
        if(eocd<0)throw new InvalidDataException("Archive non ZIP : attribution au conteneur seulement");
        long count=BinaryPrimitives.ReadUInt16LittleEndian(tail.AsSpan(eocd+10)),central=BinaryPrimitives.ReadUInt32LittleEndian(tail.AsSpan(eocd+16));
        if(BinaryPrimitives.ReadUInt16LittleEndian(tail.AsSpan(eocd+4))!=0||BinaryPrimitives.ReadUInt16LittleEndian(tail.AsSpan(eocd+6))!=0)throw new InvalidDataException("ZIP multi-volume non pris en charge");
        if(count==ushort.MaxValue||central==uint.MaxValue)
        {
            long locator=stream.Length-tailSize+eocd-20;if(locator<0)throw new InvalidDataException("ZIP64 tronqué");stream.Position=locator;
            if(reader.ReadUInt32()!=0x07064b50||reader.ReadUInt32()!=0)throw new InvalidDataException("Localisateur ZIP64 invalide");long offset=checked((long)reader.ReadUInt64());
            if(offset<0||offset>stream.Length-56)throw new InvalidDataException("Offset ZIP64 invalide");stream.Position=offset;
            if(reader.ReadUInt32()!=0x06064b50)throw new InvalidDataException("Entête ZIP64 invalide");stream.Position=offset+32;count=checked((long)reader.ReadUInt64());stream.Position=offset+48;central=checked((long)reader.ReadUInt64());
        }
        if(count>200000||central<0||central>stream.Length)throw new InvalidDataException("Index ZIP hors budget ou invalide");
        stream.Position=central;var rows=new List<(string Name,long Offset,long Expanded,long Compressed)>();
        for(long i=0;i<count;i++)
        {
            if(stream.Position>stream.Length-46||reader.ReadUInt32()!=0x02014b50)throw new InvalidDataException("Annuaire ZIP tronqué");var fixedPart=reader.ReadBytes(42);
            var flags=BinaryPrimitives.ReadUInt16LittleEndian(fixedPart.AsSpan(4));long compressed=BinaryPrimitives.ReadUInt32LittleEndian(fixedPart.AsSpan(16)),expanded=BinaryPrimitives.ReadUInt32LittleEndian(fixedPart.AsSpan(20)),offset=BinaryPrimitives.ReadUInt32LittleEndian(fixedPart.AsSpan(38));
            int nameSize=BinaryPrimitives.ReadUInt16LittleEndian(fixedPart.AsSpan(24)),extraSize=BinaryPrimitives.ReadUInt16LittleEndian(fixedPart.AsSpan(26)),commentSize=BinaryPrimitives.ReadUInt16LittleEndian(fixedPart.AsSpan(28));
            if(nameSize+extraSize+commentSize>stream.Length-stream.Position)throw new InvalidDataException("Nom ZIP tronqué");
            var name=((flags&0x800)!=0?Encoding.UTF8:Encoding.GetEncoding(437)).GetString(reader.ReadBytes(nameSize));var extra=reader.ReadBytes(extraSize);stream.Position+=commentSize;
            for(int pos=0;pos+4<=extra.Length;)
            {
                int id=BinaryPrimitives.ReadUInt16LittleEndian(extra.AsSpan(pos)),size=BinaryPrimitives.ReadUInt16LittleEndian(extra.AsSpan(pos+2));pos+=4;if(pos+size>extra.Length)throw new InvalidDataException("Champ ZIP tronqué");
                if(id==1)
                {
                    int cursor=pos;long Wide(){if(cursor+8>pos+size)throw new InvalidDataException("Champ ZIP64 tronqué");var value=checked((long)BinaryPrimitives.ReadUInt64LittleEndian(extra.AsSpan(cursor)));cursor+=8;return value;}
                    if(expanded==uint.MaxValue)expanded=Wide();if(compressed==uint.MaxValue)compressed=Wide();if(offset==uint.MaxValue)offset=Wide();
                }
                pos+=size;
            }
            if(offset<0||offset>=central)throw new InvalidDataException("Plage ZIP invalide");rows.Add((name,offset,expanded,compressed));
        }
        rows.Sort((a,b)=>a.Offset.CompareTo(b.Offset));var result=new List<ZipRange>();
        for(int i=0;i<rows.Count;i++){var row=rows[i];long end=i+1<rows.Count?rows[i+1].Offset:central;if(end<=row.Offset)throw new InvalidDataException("Entrées ZIP superposées");result.Add(new(row.Name,row.Offset,end,row.Expanded,row.Compressed));}
        return result;
    }
    internal static IEnumerable<ZipRange> Overlaps(List<ZipRange> ranges,long offset,int size)
    {
        if(offset<0||size<=0)yield break;long end=offset>long.MaxValue-size?long.MaxValue:offset+size;
        int low=0,high=ranges.Count;
        while(low<high){int mid=low+(high-low)/2;if(ranges[mid].End<=offset)low=mid+1;else high=mid;}
        for(int i=low;i<ranges.Count&&ranges[i].Start<end;i++)yield return ranges[i];
    }
}
