namespace BetterGPSNavigation;

// Only namespaces explicitly registered by this version are eligible. Historical
// recordings, evidence sidecars and unregistered old caches are never removed.
internal static class CacheMaintenance
{
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string,byte> started=new();
    internal static FileStream? Register(string root,string fingerprint)
    {
        StorageBudget.Register(root);
        FileStream? lease=null;
        RouteDiagnostics.Try(()=>
        {
            var directory=Path.Combine(root,"maintenance-v1");Directory.CreateDirectory(directory);
            var path=Path.Combine(directory,fingerprint+".lease");
            lease=new FileStream(path,FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.ReadWrite);
            File.SetLastWriteTimeUtc(path,DateTime.UtcNow);
        });
        if(started.TryAdd(Path.GetFullPath(root),0))
            _=Task.Run(()=>RouteDiagnostics.Try(()=>Prune(root,fingerprint,DateTime.UtcNow)));
        return lease;
    }
    internal static int Prune(string root,string current,DateTime now)
    {
        root=Path.GetFullPath(root);var markers=Path.Combine(root,"maintenance-v1");
        if(!Directory.Exists(markers))return 0;
        var candidates=Directory.GetFiles(markers,"*.lease").OrderByDescending(File.GetLastWriteTimeUtc).ToArray();
        int removed=0;
        foreach(var marker in candidates.Skip(3))
        {
            var key=Path.GetFileNameWithoutExtension(marker);
            if(key==current||key.Length!=64||!key.All(Uri.IsHexDigit)||now-File.GetLastWriteTimeUtc(marker)<TimeSpan.FromDays(30))continue;
            try
            {
                using(var exclusive=new FileStream(marker,FileMode.Open,FileAccess.ReadWrite,FileShare.None))
                {
                    foreach(var file in Directory.GetFiles(root,key+"-*.json"))File.Delete(file);
                    var worlds=Path.Combine(root,"world-v1");
                    if(Directory.Exists(worlds))foreach(var map in Directory.GetDirectories(worlds))
                    {
                        var target=Path.GetFullPath(Path.Combine(map,key));
                        if(!target.StartsWith(root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))
                            throw new IOException("Cache namespace escaped its root.");
                        if(Directory.Exists(target))
                        {
                            // Refuse symlinks/junctions before recursive removal.
                            CheckAncestors(target,root);CheckPlainTree(target);Directory.Delete(target,true);
                        }
                    }
                }
                File.Delete(marker);removed++;
            }
            catch(Exception e) when(e is IOException or UnauthorizedAccessException){ }
        }
        return removed;
    }
    private static void CheckAncestors(string target,string root)
    {
        for(var path=target;;path=Path.GetDirectoryName(path)!)
        {
            if((File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0)throw new IOException("Linked cache parent is not prunable.");
            if(path.Equals(root,StringComparison.OrdinalIgnoreCase))break;
        }
    }
    private static void CheckPlainTree(string path)
    {
        if((File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0)throw new IOException("Linked cache tree is not prunable.");
        foreach(var entry in Directory.EnumerateFileSystemEntries(path))
        {
            var attributes=File.GetAttributes(entry);
            if((attributes&FileAttributes.ReparsePoint)!=0)throw new IOException("Linked cache entry is not prunable.");
            if((attributes&FileAttributes.Directory)!=0)CheckPlainTree(entry);
        }
    }
}
