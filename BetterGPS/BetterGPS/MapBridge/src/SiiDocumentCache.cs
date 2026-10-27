using System.Runtime.CompilerServices;
using TruckLib;
using TruckLib.Sii;
namespace BetterGPSNavigation;
// Parsed units are read-only to bridge consumers. The cache belongs to the
// mounted filesystem, so a new game/mod mount set never reuses old definitions.
internal static class SiiDocumentCache
{
    private static readonly ConditionalWeakTable<IFileSystem,Dictionary<string,SiiFile>> Documents=new();
    internal static void Release(IFileSystem fs)=>Documents.Remove(fs);
    public static SiiFile Open(string path,IFileSystem fs)
    {
        if(!path.StartsWith("/def/world/",StringComparison.Ordinal)&&!path.StartsWith("/def/sign/",StringComparison.Ordinal))return SiiFile.Open(path,fs);
        var documents=Documents.GetValue(fs,_=>new(StringComparer.Ordinal));
        if(documents.TryGetValue(path,out var known))return known;
        return documents[path]=CompactSiiReader.Open(path,fs);
    }
}
