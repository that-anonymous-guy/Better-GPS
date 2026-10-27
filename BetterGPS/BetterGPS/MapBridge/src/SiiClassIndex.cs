using System.Text.RegularExpressions;
using TruckLib;
namespace BetterGPSNavigation;
// A conservative text prefilter: includes are followed, and unknown encodings
// always go through the full parser. File names never determine a unit's type.
internal sealed class SiiClassIndex(IFileSystem fs,string unitClass)
{
    private readonly Dictionary<string,bool> cache=new(StringComparer.Ordinal);
    private readonly HashSet<string> reading=[];
    public bool MayContain(string path)
    {
        if(cache.TryGetValue(path,out var known))return known;
        if(!reading.Add(path))return true;
        try
        {
            var text=fs.ReadAllText(path);
            if(text.Contains('\0')||text.Contains('\uFFFD')||text.StartsWith("ScsC",StringComparison.Ordinal)||text.StartsWith("BSII",StringComparison.Ordinal))return cache[path]=true;
            if(text.Contains(unitClass,StringComparison.Ordinal))return cache[path]=true;
            var includes=Regex.Matches(text,"@include\\s+\"([^\"]+)\"");
            if(Regex.Matches(text,"@include").Count!=includes.Count)return cache[path]=true;
            foreach(Match include in includes)
            {
                var target=include.Groups[1].Value;
                if(!target.StartsWith('/'))target=fs.GetParent(path)+"/"+target;
                if(MayContain(target))return cache[path]=true;
            }
            return cache[path]=false;
        }
        catch(Exception e) when(e is IOException or ArgumentException){return true;}
        finally{reading.Remove(path);}
    }
}
