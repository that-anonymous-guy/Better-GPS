namespace BetterGPSNavigation;

// Regenerable map data persists between sessions; disk writes have a budget.
// Only known cache formats are evicted. Evidence in use by the current session
// is retained; shutdown removes it after the compiler and phone have stopped.
internal static class StorageBudget
{
    internal const long CacheBytes=576L*1024*1024;
    private sealed class Budget(string root,long limit)
    {
        internal readonly string Root=root;
        internal readonly long Limit=limit;
        internal readonly Dictionary<string,(long Size,DateTime Time)> Files=new(StringComparer.OrdinalIgnoreCase);
        internal bool Scanned;internal long Used;
    }
    private static readonly Dictionary<string,Budget> roots=new(StringComparer.OrdinalIgnoreCase);
    private static readonly object gate=new();
    internal static void Register(string root,long limit=CacheBytes)
    {lock(gate){root=Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));if(!roots.ContainsKey(root))roots[root]=new(root,limit);}}
    private static IEnumerable<string> PlainFiles(string root)
    {
        if(!Directory.Exists(root)||(File.GetAttributes(root)&FileAttributes.ReparsePoint)!=0)yield break;
        foreach(var file in Directory.EnumerateFiles(root))if((File.GetAttributes(file)&FileAttributes.ReparsePoint)==0)yield return file;
        foreach(var dir in Directory.EnumerateDirectories(root))if((File.GetAttributes(dir)&FileAttributes.ReparsePoint)==0)
            foreach(var file in PlainFiles(dir))yield return file;
    }
    private static bool Evictable(string path,string root)
    {
        var rel=Path.GetRelativePath(root,path).Replace('\\','/');var name=Path.GetFileName(path);
        if(rel.StartsWith("live-route-evidence/",StringComparison.OrdinalIgnoreCase)||rel.StartsWith("phone-shields/",StringComparison.OrdinalIgnoreCase))return false;
        if(rel.StartsWith("world-v1/",StringComparison.OrdinalIgnoreCase))return name.EndsWith(".wgc")||name=="complete.txt"||name.EndsWith(".tmp");
        return name.Length>69&&name[..64].All(Uri.IsHexDigit)&&name[64]=='-'&&(name.EndsWith(".json")||name.EndsWith(".tmp"));
    }
    internal static bool Write(string path,byte[] data,bool required=false,bool allowEviction=true)
    {
        path=Path.GetFullPath(path);
        lock(gate)
        {
            var budget=roots.Values.Where(b=>path.StartsWith(b.Root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)).OrderByDescending(b=>b.Root.Length).FirstOrDefault();
            if(budget is not null)
            {
                for(var parent=Path.GetDirectoryName(path)!;;parent=Path.GetDirectoryName(parent)!){
                    if(Directory.Exists(parent)&&(File.GetAttributes(parent)&FileAttributes.ReparsePoint)!=0){if(required)throw new IOException("Linked cache directory is not writable by maintenance.");return false;}
                    if(parent.Equals(budget.Root,StringComparison.OrdinalIgnoreCase))break;
                }
                if(!budget.Scanned){foreach(var f in PlainFiles(budget.Root)){var info=new FileInfo(f);budget.Files[f]=(info.Length,info.LastWriteTimeUtc);budget.Used+=info.Length;}budget.Scanned=true;}
                if(data.Length>budget.Limit){if(required)throw new IOException("Evidence exceeds the local cache budget.");return false;}
                long used=budget.Used;
                // Include the temporary copy while replacing an existing file.
                if(allowEviction&&used+data.Length>budget.Limit)foreach(var f in budget.Files.Where(f=>f.Key!=path&&Evictable(f.Key,budget.Root))
                    .OrderBy(f=>f.Key.Contains("-route-")||f.Key.Contains("-network-region-")?0:1).ThenBy(f=>f.Value.Time).ToArray())
                {
                    if(used+data.Length<=budget.Limit)break;
                    try{File.Delete(f.Key);used-=f.Value.Size;budget.Used=used;budget.Files.Remove(f.Key);if(f.Key.EndsWith(".wgc",StringComparison.OrdinalIgnoreCase)){
                        var complete=Path.Combine(Path.GetDirectoryName(f.Key)!,"complete.txt");File.Delete(complete);if(budget.Files.Remove(complete,out var marker)){used-=marker.Size;budget.Used=used;}
                    }}catch(IOException){}catch(UnauthorizedAccessException){}
                }
                if(used+data.Length>budget.Limit){if(required)throw new IOException("BetterGPS cache budget exhausted by protected session evidence; clear the itinerary to release it.");return false;}
            }
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);var temp=path+"."+Guid.NewGuid().ToString("N")+".tmp";
            try{File.WriteAllBytes(temp,data);File.Move(temp,path,true);if(budget is not null){budget.Used=usedFor(budget,path,data.Length);budget.Files[path]=(data.Length,DateTime.UtcNow);}}
            finally{if(File.Exists(temp))File.Delete(temp);}
            return true;
        }
    }
    private static long usedFor(Budget b,string path,long bytes)=>b.Used-(b.Files.TryGetValue(path,out var old)?old.Size:0)+bytes;
    internal static void RetainEvidence(string root,IEnumerable<string> retained)
    {
        var retainedPaths=retained.ToArray();RouteEvidence.Retain(retainedPaths);
        lock(gate){
            root=Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));if(!roots.ContainsKey(root))return;
            var directory=Path.Combine(root,"live-route-evidence");if(!Directory.Exists(directory)||(File.GetAttributes(directory)&FileAttributes.ReparsePoint)!=0)return;
            var keep=retainedPaths.Select(Path.GetFullPath).ToHashSet(StringComparer.OrdinalIgnoreCase);
            foreach(var path in Directory.EnumerateFiles(directory).Where(p=>Path.GetExtension(p) is ".json" or ".br")){
                var name=Path.GetFileNameWithoutExtension(path);
                if(name.Length!=64||!name.All(Uri.IsHexDigit)||keep.Contains(Path.GetFullPath(path))||(File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0)continue;
                try{File.Delete(path);if(roots[root].Files.Remove(path,out var old))roots[root].Used-=old.Size;}catch(IOException){}catch(UnauthorizedAccessException){}
            }
        }
    }
    internal static void Rescan(string root){lock(gate)if(roots.TryGetValue(Path.TrimEndingDirectorySeparator(Path.GetFullPath(root)),out var b)){b.Files.Clear();b.Used=0;b.Scanned=false;}}
}
