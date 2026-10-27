using System.Text.RegularExpressions;
internal static class CaptureRetention
{
    internal static void Trim(string current)
    {
        var parent=Directory.GetParent(current);if(parent?.Name!="captures")return;
        var allowed=new HashSet<string>(StringComparer.OrdinalIgnoreCase){"capture.owner","rapport.json","rapport.html","events.jsonl","resources.jsonl","presentmon.csv"};
        var owned=parent.EnumerateDirectories().Where(d=>Regex.IsMatch(d.Name,@"^\d{8}_\d{6}_\d{3}$")&&(d.Attributes&FileAttributes.ReparsePoint)==0)
            .Where(d=>File.Exists(Path.Combine(d.FullName,"capture.owner"))&&(File.GetAttributes(Path.Combine(d.FullName,"capture.owner"))&FileAttributes.ReparsePoint)==0&&File.ReadAllText(Path.Combine(d.FullName,"capture.owner"))=="BetterGPSDiagnostics-v1")
            .Where(d=>d.EnumerateFileSystemInfos().All(f=>f is FileInfo&&(f.Attributes&FileAttributes.ReparsePoint)==0&&allowed.Contains(f.Name)))
            .OrderByDescending(d=>d.FullName==Path.GetFullPath(current)).ThenByDescending(d=>d.Name).ToList();
        long bytes=0;int retained=0;
        foreach(var directory in owned){var files=directory.EnumerateFiles().ToArray();long size=files.Sum(f=>f.Length);if(directory.FullName==Path.GetFullPath(current)||(retained<8&&bytes+size<=128L*1024*1024)){retained++;bytes+=size;continue;}foreach(var file in files)file.Delete();directory.Delete();}
    }
}
