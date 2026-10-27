static class CaptureRetention
{
    static readonly HashSet<string> Allowed=new(StringComparer.OrdinalIgnoreCase){"capture.owner","capture.json.gz","rapport.json","rapport.html"};
    internal static void Apply(string root,string active,int countLimit=8,long byteLimit=128L*1024*1024)
    {
        root=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);active=Path.GetFullPath(active).TrimEnd(Path.DirectorySeparatorChar);
        if(!Directory.Exists(root)||!string.Equals(Path.GetDirectoryName(active),root,StringComparison.OrdinalIgnoreCase))return;
        bool Owned(string path)
        {
            if(!string.Equals(Path.GetDirectoryName(Path.GetFullPath(path)),root,StringComparison.OrdinalIgnoreCase)||(File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0)return false;
            var marker=Path.Combine(path,"capture.owner");if(!File.Exists(marker)||new FileInfo(marker).Length>128||File.ReadAllText(marker)!="ATSModDiagnostics:1")return false;
            return Directory.EnumerateFileSystemEntries(path).All(p=>File.Exists(p)&&Allowed.Contains(Path.GetFileName(p))&&(File.GetAttributes(p)&FileAttributes.ReparsePoint)==0);
        }
        try
        {
            var dirs=Directory.GetDirectories(root).Where(Owned).OrderBy(Directory.GetCreationTimeUtc).ToList();
            long Size(string path)=>Directory.GetFiles(path).Sum(p=>new FileInfo(p).Length);
            long size=dirs.Sum(Size);int count=dirs.Count;
            foreach(var dir in dirs)
            {
                if(count<=countLimit&&size<=byteLimit)break;if(string.Equals(dir,active,StringComparison.OrdinalIgnoreCase))continue;
                if(!Owned(dir))continue;long bytes=Size(dir);
                // Flat, known files only; never recursively delete or traverse links.
                foreach(var file in Directory.GetFiles(dir))File.Delete(file);Directory.Delete(dir,false);size-=bytes;count--;
            }
        }
        catch(IOException){}catch(UnauthorizedAccessException){}
    }
}
