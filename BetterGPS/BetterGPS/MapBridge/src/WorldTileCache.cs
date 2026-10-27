using System.IO.Compression;
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using TruckLib;

namespace BetterGPSNavigation;

internal sealed record WorldSignReading(ulong Uid,ParsedSignArea[] Areas);
internal sealed record WorldGeometry(string ItemUid,NetworkPortion[] Portions,string[] Errors)
{public int GeometryVersion {get;init;}}
internal sealed record WorldTile(int X,int Z,WorldSignReading[] Signs,WorldGeometry[] Geometry);

// Small immutable pages, not one multi-gigabyte JSON document at startup.
// No route ownership is persisted: cached sources are reassociated to each graph.
internal sealed class WorldTileCache
{
    private static readonly ConditionalWeakTable<IFileSystem,WorldTileCache> Instances=new();
    private readonly string directory;
    private readonly byte[] fingerprint;
    private sealed record Page(WorldTile Tile,long Size,Dictionary<ulong,WorldSignReading> Signs,Dictionary<string,WorldGeometry> Geometry);
    private readonly Dictionary<(int,int),Page> pages=[];
    private readonly Queue<(int,int)> order=[];
    private readonly Dictionary<(int,int),long> missing=[];
    private long residentSize;
    private readonly Dictionary<(int,int),long> writeBackoff=[];
    internal WorldTileCache(string directory,string fingerprint)
    {this.directory=directory;this.fingerprint=Convert.FromHexString(fingerprint);Directory.CreateDirectory(directory);}
    internal static void Configure(IFileSystem fs,string map)
    {
        if(AssetIndexCache.DirectoryFor(fs) is not {} root||AssetIndexCache.Fingerprint(fs) is not {} key)return;
        var mapKey=Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(map)))[..16];
        Instances.Add(fs,new(Path.Combine(root,"world-v1",mapKey,key),key));
    }
    internal static WorldTileCache? For(IFileSystem fs)=>Instances.TryGetValue(fs,out var cache)?cache:null;
    internal static (int X,int Z) Cell(Vector3 p)=>((int)Math.Floor(p.X/4000d),(int)Math.Floor(p.Z/4000d));
    internal string PathFor(int x,int z)=>Path.Combine(directory,$"{x}_{z}.wgc");
    private static string CatalogueKey(IEnumerable<(int,int)> cells)=>Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(
        string.Join('|',cells.OrderBy(c=>c.Item1).ThenBy(c=>c.Item2).Select(c=>$"{c.Item1}:{c.Item2}")))));
    internal bool IsComplete(IEnumerable<(int,int)> cells)
    {try{return File.ReadAllText(Path.Combine(directory,"complete.txt"))==CatalogueKey(cells);}catch(IOException){return false;}}
    internal void MarkComplete(IEnumerable<(int,int)> cells)
    {
        var file=Path.Combine(directory,"complete.txt");var temporary=file+"."+Guid.NewGuid().ToString("N")+".tmp";
        var catalogue=cells.ToArray();
        if(catalogue.Any(c=>!File.Exists(PathFor(c.Item1,c.Item2)))){if(File.Exists(file))File.Delete(file);return;}
        File.WriteAllText(temporary,CatalogueKey(catalogue));File.Move(temporary,file,true);
    }
    internal bool TryGet(Vector3 position,ulong uid,out WorldSignReading reading)
    {
        var (x,z)=Cell(position);var tile=Read(x,z);
        reading=tile is null?null!:pages.TryGetValue((x,z),out var page)?page.Signs.GetValueOrDefault(uid)!:tile.Signs.FirstOrDefault(s=>s.Uid==uid)!;
        return reading is not null;
    }
    internal bool TryGeometry(Vector3 position,ulong uid,out WorldGeometry geometry,int requiredVersion=0)
    {
        var (x,z)=Cell(position);var tile=Read(x,z);
        geometry=tile is null?null!:pages.TryGetValue((x,z),out var page)?page.Geometry.GetValueOrDefault(uid.ToString("X16"))!:tile.Geometry.FirstOrDefault(g=>g.ItemUid==uid.ToString("X16"))!;
        return geometry is not null&&geometry.GeometryVersion>=requiredVersion;
    }
    internal WorldTile? Read(int x,int z)
    {
        var key=(x,z);
        WorldTile? Rejected(){missing[key]=Environment.TickCount64;return null;}
        // Missing pages are deliberately not cached: the warmer may publish one
        // while a route is being prepared.
        if(pages.TryGetValue(key,out var known))return known.Tile;
        if(missing.TryGetValue(key,out var failedAt)&&Environment.TickCount64-failedAt<500)return null;
        try
        {
            var bytes=File.ReadAllBytes(PathFor(x,z));
            if(bytes.Length<68||!bytes.AsSpan(0,4).SequenceEqual("WGC1"u8)||!bytes.AsSpan(4,32).SequenceEqual(fingerprint)||
                !bytes.AsSpan(36,32).SequenceEqual(SHA256.HashData(bytes.AsSpan(68))))return Rejected();
            using var stream=new MemoryStream(bytes,68,bytes.Length-68,false);
            using var compressed=new BrotliStream(stream,CompressionMode.Decompress);
            using var counted=new CountingReadStream(compressed);
            var tile=JsonSerializer.Deserialize<WorldTile>(counted,LiveProtocol.Json);
            if(tile is null||tile.X!=x||tile.Z!=z||tile.Signs.Select(s=>s.Uid).Distinct().Count()!=tile.Signs.Length||
                tile.Geometry.Select(g=>g.ItemUid).Distinct().Count()!=tile.Geometry.Length)return Rejected();
            // Conservative resident estimate: expanded JSON x4 covers objects, strings and lookup tables.
            long footprint=checked(counted.BytesRead*4+bytes.Length+4096);
            if(footprint>32*1024*1024)return tile;
            while(order.Count>0&&(residentSize+footprint>32*1024*1024||order.Count>=32))
            {var old=order.Dequeue();residentSize-=pages[old].Size;pages.Remove(old);}
            pages[key]=new(tile,footprint,tile.Signs.ToDictionary(s=>s.Uid),tile.Geometry.ToDictionary(g=>g.ItemUid));
            order.Enqueue(key);residentSize+=footprint;
            return tile;
        }
        catch(Exception e) when(e is IOException or UnauthorizedAccessException or JsonException or ArgumentException)
        {return Rejected();}
    }
    internal bool CanPrepare(int x,int z)=>!writeBackoff.TryGetValue((x,z),out var at)||Environment.TickCount64-at>=300000;
    internal void Write(WorldTile tile)
    {
        using var stream=new MemoryStream();
        using(var compressed=new BrotliStream(stream,CompressionLevel.Fastest,true))JsonSerializer.Serialize(compressed,tile,LiveProtocol.Json);
        var payload=stream.ToArray();var file=PathFor(tile.X,tile.Z);
        using var output=new MemoryStream();output.Write("WGC1"u8);output.Write(fingerprint);output.Write(SHA256.HashData(payload));output.Write(payload);
        if(!StorageBudget.Write(file,output.ToArray(),allowEviction:false)){writeBackoff[(tile.X,tile.Z)]=Environment.TickCount64;return;}
        writeBackoff.Remove((tile.X,tile.Z));
        missing.Remove((tile.X,tile.Z));
        if(pages.Remove((tile.X,tile.Z),out var previous)){residentSize-=previous.Size;var retained=order.Where(k=>k!=(tile.X,tile.Z)).ToArray();order.Clear();foreach(var k in retained)order.Enqueue(k);}

    }
}

internal sealed class CountingReadStream(Stream inner,long maximumBytes=long.MaxValue):Stream
{
    internal long BytesRead {get;private set;}
    public override int Read(byte[] buffer,int offset,int count){var n=inner.Read(buffer,offset,count);BytesRead+=n;if(BytesRead>maximumBytes)throw new InvalidDataException("Expanded cache exceeds its supported size.");return n;}
    public override int Read(Span<byte> buffer){var n=inner.Read(buffer);BytesRead+=n;if(BytesRead>maximumBytes)throw new InvalidDataException("Expanded cache exceeds its supported size.");return n;}
    public override bool CanRead=>true;public override bool CanSeek=>false;public override bool CanWrite=>false;
    public override long Length=>throw new NotSupportedException();public override long Position {get=>BytesRead;set=>throw new NotSupportedException();}
    public override void Flush(){}public override long Seek(long offset,SeekOrigin origin)=>throw new NotSupportedException();
    public override void SetLength(long value)=>throw new NotSupportedException();public override void Write(byte[] buffer,int offset,int count)=>throw new NotSupportedException();
}
