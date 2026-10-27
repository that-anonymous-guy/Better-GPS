using System.Text;
using TruckLib;
namespace BetterGPSNavigation;

internal sealed class MountedDirectoryFileSystem(string root) : IFileSystem
{
    private readonly string rootPath=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
    public char DirectorySeparator=>'/';
    private string Resolve(string path)
    {
        var resolved=Path.GetFullPath(Path.Combine(rootPath,path.Replace('\\','/').TrimStart('/').Replace('/',Path.DirectorySeparatorChar)));
        if(resolved!=rootPath&&!resolved.StartsWith(rootPath+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Virtual path escapes mounted directory.");
        return resolved;
    }
    public bool FileExists(string path)=>File.Exists(Resolve(path));
    public bool DirectoryExists(string path)=>Directory.Exists(Resolve(path));
    public IList<string> GetFiles(string path)=>Directory.GetFiles(Resolve(path)).Select(p=>"/"+Path.GetRelativePath(rootPath,p).Replace('\\','/')).ToArray();
    public Stream Open(string path)=>File.Open(Resolve(path),FileMode.Open,FileAccess.Read,FileShare.ReadWrite);
    public byte[] ReadAllBytes(string path)=>File.ReadAllBytes(Resolve(path));
    public string ReadAllText(string path)=>ReadAllText(path,Encoding.UTF8);
    public string ReadAllText(string path,Encoding encoding)=>File.ReadAllText(Resolve(path),encoding);
    public string? GetParent(string path){var p=path.Replace('\\','/').TrimEnd('/');return p.Length==0?null:p.LastIndexOf('/')<=0?"/":p[..p.LastIndexOf('/')];}
}

internal sealed class TrackedFileSystem(IFileSystem inner,string source,
    System.Collections.Concurrent.ConcurrentDictionary<string,GameAssets.ResourceUse> used,AssetReadCache? shared=null) : IFileSystem
{
    private readonly AssetReadCache cache=shared??new();
    public char DirectorySeparator=>inner.DirectorySeparator;
    public bool FileExists(string path)=>inner.FileExists(path);
    public bool DirectoryExists(string path)=>inner.DirectoryExists(path);
    public IList<string> GetFiles(string path)=>inner.GetFiles(path);
    public string? GetParent(string path)=>inner.GetParent(path);
    private byte[] ReadBuffer(string path,out bool sharedBuffer)
    {
        bool reusable=inner is not MountedDirectoryFileSystem&&!path.StartsWith("/map/",StringComparison.OrdinalIgnoreCase);
        if(reusable&&cache.TryGet(source,path,out var known)){sharedBuffer=true;return known;}
        var bytes=inner.ReadAllBytes(path);
        used[path]=new(path,source,Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(bytes)));
        sharedBuffer=reusable&&cache.Store(source,path,bytes);return bytes;
    }
    public byte[] ReadAllBytes(string path){var bytes=ReadBuffer(path,out var sharedBuffer);return sharedBuffer?(byte[])bytes.Clone():bytes;}
    public Stream Open(string path){var bytes=ReadBuffer(path,out _);return new MemoryStream(bytes,0,bytes.Length,false,false);}
    public string ReadAllText(string path)=>ReadAllText(path,Encoding.UTF8);
    public string ReadAllText(string path,Encoding encoding){using var reader=new StreamReader(Open(path),encoding,true);return reader.ReadToEnd();}
}
