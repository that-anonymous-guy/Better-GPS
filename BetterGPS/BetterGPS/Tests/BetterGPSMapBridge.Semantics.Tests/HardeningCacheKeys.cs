using BetterGPSNavigation;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
internal static class HardeningCacheKeys
{
    internal static void Run(string docs,string previous,string output)
    {
        var resources=new List<string>();
        foreach(var mount in GameAssets.ReadMounts(Path.Combine(docs,"game.log.txt")))
        {
            resources.Add(mount.Pool+":"+Path.GetFullPath(mount.Path));
            IEnumerable<string> paths=Directory.Exists(mount.Path)?Directory.EnumerateFiles(mount.Path,"*",SearchOption.AllDirectories).Order(StringComparer.Ordinal):[mount.Path];
            foreach(var path in paths)resources.Add(AssetIndexCache.ResourceIdentity(path,mount.Pool=="mod"||Directory.Exists(mount.Path)));
        }
        string Key(string dll)=>Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(string.Join('\n',
            new[]{"asset-index-v2",Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(dll)))}.Concat(resources)))));
        File.WriteAllText(output,JsonSerializer.Serialize(new{oldKey=Key(previous),newKey=Key(typeof(LiveGuidanceEngine).Assembly.Location),
            resourceSignature=Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(string.Join('\n',resources)))),
            policy="Only raw readers unchanged by lease-lifetime fix; no routes or solved networks"},LiveProtocol.Json));
    }
}
