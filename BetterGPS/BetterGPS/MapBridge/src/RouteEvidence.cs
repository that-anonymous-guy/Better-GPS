using System.Security.Cryptography;
using System.IO.Compression;
using System.Text.Json;
namespace BetterGPSNavigation;

// Local immutable evidence keeps large routes below the pipe frame limit.
internal static class RouteEvidence
{
    private static readonly Dictionary<string,LiveRoute> loaded=[];
    internal static void Retain(IEnumerable<string> paths)
    {var keep=paths.Select(Path.GetFullPath).ToHashSet(StringComparer.OrdinalIgnoreCase);lock(loaded)foreach(var key in loaded.Keys.Where(k=>!keep.Contains(Path.GetFullPath(k))).ToArray())loaded.Remove(key);}
    internal static LiveRoute Pack(LiveRoute route,string directory)
    {
        var bytes=JsonSerializer.SerializeToUtf8Bytes(route,LiveProtocol.Json);
        if(bytes.Length<LiveProtocol.MaxPayloadBytes-4096)return route;
        if(bytes.Length>256*1024*1024)throw new InvalidDataException("Compiled route evidence exceeds the supported 256 MiB bound.");
        using(var stream=new MemoryStream()){
            using(var compressed=new BrotliStream(stream,CompressionLevel.Fastest,true))compressed.Write(bytes);
            bytes=stream.ToArray();
        }
        Directory.CreateDirectory(directory);var hash=Convert.ToHexString(SHA256.HashData(bytes));
        var file=Path.GetFullPath(Path.Combine(directory,hash+".br"));
        bool valid=false;
        if(File.Exists(file))
        {
            using var existing=new FileStream(file,FileMode.Open,FileAccess.Read,FileShare.Read|FileShare.Delete);
            valid=existing.Length==bytes.Length&&Convert.ToHexString(SHA256.HashData(existing))==hash;
        }
        if(!valid)
        {
            StorageBudget.Write(file,bytes,required:true);
        }
        return new(route.RouteId,[],[]){IsPartial=route.IsPartial,EvidenceFile=file,EvidenceSha256=hash};
    }
    internal static LiveRoute Unpack(LiveRoute route)
    {
        if(route.EvidenceFile is null)return route;
        var file=route.EvidenceFile;var hash=route.EvidenceSha256;
        if(hash is null||hash.Length!=64||!hash.All(Uri.IsHexDigit)||!Path.IsPathRooted(file)||(Path.GetFileName(file)!=hash+".json"&&Path.GetFileName(file)!=hash+".br")||
            route.Edges.Length!=0||route.Actions.Length!=0||route.RoadIdentities is not null||route.ServiceAreas is not null)
            throw new InvalidDataException("Invalid local route evidence reference.");
        LiveRoute full;
        lock(loaded)
        {
            if(!loaded.TryGetValue(file,out full!))
            {
                if(new FileInfo(file).Length>256*1024*1024)throw new InvalidDataException("Route evidence file exceeds its size bound.");
                var bytes=File.ReadAllBytes(file);
                if(Convert.ToHexString(SHA256.HashData(bytes))!=hash)throw new InvalidDataException("Route evidence integrity mismatch.");
                if(file.EndsWith(".br",StringComparison.OrdinalIgnoreCase)){
                    using var source=new MemoryStream(bytes);using var compressed=new BrotliStream(source,CompressionMode.Decompress);using var decoded=new MemoryStream();
                    var buffer=new byte[65536];int count;
                    while((count=compressed.Read(buffer))>0){if(decoded.Length+count>256L*1024*1024)throw new InvalidDataException("Decompressed evidence exceeds its size bound.");decoded.Write(buffer,0,count);}
                    bytes=decoded.ToArray();
                }
                full=JsonSerializer.Deserialize<LiveRoute>(bytes,LiveProtocol.Json)??throw new InvalidDataException("Empty route evidence.");
                if(full.EvidenceFile is not null)throw new InvalidDataException("Nested route evidence references are forbidden.");
                if(loaded.Count>=2)loaded.Clear();loaded[file]=full;
            }
        }
        if(full.RouteId!=route.RouteId)
        {
            var ids=full.Actions.Select((a,index)=>(a.Id,NewId:route.RouteId+":"+index)).ToDictionary(a=>a.Id,a=>a.NewId);
            full=full with{RouteId=route.RouteId,Actions=full.Actions.Select(a=>a with{Id=ids[a.Id]}).ToArray(),
                RoadIdentities=full.RoadIdentities is {} data?data with{Contexts=data.Contexts.Select(c=>c with{ActionId=ids[c.ActionId]}).ToArray()}:null};
        }
        return full with{IsPartial=route.IsPartial};
    }
}
