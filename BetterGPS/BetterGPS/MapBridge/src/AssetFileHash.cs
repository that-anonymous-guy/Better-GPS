using System.Security.Cryptography;
using System.Text;
using TruckLib;
namespace BetterGPSNavigation;
internal static class AssetFileHash
{
    internal static string Read(IFileSystem fs,string path)
    {
        var key=Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(path)));
        var use=AssetIndexCache.GetShared(fs,"asset-file-hash-"+key,()=>{
            using var stream=fs.Open(path);
            return new GameAssets.ResourceUse(path,GameAssets.SourceOf(fs,path),Convert.ToHexString(SHA256.HashData(stream)));
        });
        GameAssets.RecordCached(fs,use);return use.Sha256;
    }
}
