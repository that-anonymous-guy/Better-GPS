using System.Runtime.CompilerServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using TruckLib;
namespace BetterGPSNavigation;
internal static class AssetIndexCache
{
    private sealed record Location(string Directory,string Fingerprint,FileStream? Lease);
    private sealed record Envelope(string Fingerprint,string Sha256,string Payload);
    private static readonly ConditionalWeakTable<IFileSystem,Location> Locations=new();
    private static readonly ConditionalWeakTable<IFileSystem,Dictionary<(Type,string),object?>> Shared=new();
    internal static T GetShared<T>(IFileSystem fs,string name,Func<T> build)
    {
        var values=Shared.GetValue(fs,_=>new());
        lock(values)
        {
            var key=(typeof(T),name);
            if(values.TryGetValue(key,out var value))return (T)value!;
            var result=Get(fs,name,build);values[key]=result;return result;
        }
    }
    internal static void Configure(IFileSystem fs,string directory,IEnumerable<GameAssets.Mount> mounts)
    {
        var rows=new List<string>{"asset-index-v2",Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(typeof(AssetIndexCache).Assembly.Location)))};
        foreach(var mount in mounts)
        {
            rows.Add(mount.Pool+":"+Path.GetFullPath(mount.Path));
            IEnumerable<string> paths=System.IO.Directory.Exists(mount.Path)?System.IO.Directory.EnumerateFiles(mount.Path,"*",SearchOption.AllDirectories).Order(StringComparer.Ordinal):new[]{mount.Path};
            foreach(var path in paths)rows.Add(ResourceIdentity(path,mount.Pool=="mod"||System.IO.Directory.Exists(mount.Path)));
        }
        var fingerprint=Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(string.Join('\n',rows))));
        System.IO.Directory.CreateDirectory(directory);
        Locations.Add(fs,new(directory,fingerprint,GameAssets.IsOwned(fs)?CacheMaintenance.Register(directory,fingerprint):null));
    }
    internal static void Release(IFileSystem fs)
    {
        if(Locations.TryGetValue(fs,out var location))location.Lease?.Dispose();
        Locations.Remove(fs);Shared.Remove(fs);SiiDocumentCache.Release(fs);
    }
    internal static string ResourceIdentity(string path,bool contentRequired)
    {
        var info=new FileInfo(path);var row=path+":"+info.Length+":"+info.LastWriteTimeUtc.Ticks;
        using var content=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read,1024*1024,FileOptions.SequentialScan);
        // NTFS change-time survives LastWriteTime restoration without reading an entire
        // archive. Other filesystems
        // fall back to full hashing where content verification is required.
        bool stamped=ResourceChangeStamp.TryRead(content.SafeFileHandle,out var change);
        if(stamped)row+=":change:"+change;
        if(contentRequired&&!stamped)row+=":"+Convert.ToHexString(SHA256.HashData(content));
        return row;
    }
    internal static string? Fingerprint(IFileSystem fs)=>Locations.TryGetValue(fs,out var location)?location.Fingerprint:null;
    internal static string? DirectoryFor(IFileSystem fs)=>Locations.TryGetValue(fs,out var location)?location.Directory:null;
    public static T Get<T>(IFileSystem fs,string name,Func<T> build)
    {
        if(!Locations.TryGetValue(fs,out var location))return build();
        var path=Path.Combine(location.Directory,location.Fingerprint+"-"+name+".json");
        try
        {
            if(File.Exists(path))
            {
                using var input=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read|FileShare.Delete);
                bool valid=CompressedAssetCache.TryRead<T>(input,location.Fingerprint,out var cached);
                if(!valid){
                    using var text=new StreamReader(input);var envelope=JsonSerializer.Deserialize<Envelope>(text.ReadToEnd());
                    valid=envelope is not null&&envelope.Payload is not null&&envelope.Fingerprint==location.Fingerprint&&envelope.Sha256==Hash(envelope.Payload);
                    if(valid)cached=JsonSerializer.Deserialize<T>(envelope!.Payload!,LiveProtocol.Json)!;
                }
                if(valid)
                {
                    if(cached is LiveRoute route)
                    {
                        if(route.EvidenceFile is not null&&!File.Exists(route.EvidenceFile))throw new InvalidDataException("Cached route evidence is missing.");
                        LiveProtocol.Validate(new(2,"RouteSnapshot","cache-validation",0,0,1,route));
                    }
                    return cached;
                }
            }
        }
        catch(Exception e) when(e is IOException or InvalidDataException or UnauthorizedAccessException or JsonException or ArgumentException){ }
        var value=build();
        if(value is LiveRoute built)LiveProtocol.Validate(new(2,"RouteSnapshot","cache-build-validation",0,0,1,built));
        try
        {
            StorageBudget.Write(path,CompressedAssetCache.Encode(value,location.Fingerprint));
        }
        catch(Exception e) when(e is IOException or UnauthorizedAccessException){ }
        return value;
    }
    private static string Hash(string text)=>Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(text)));
}
