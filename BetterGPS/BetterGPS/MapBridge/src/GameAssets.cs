using TruckLib;
using TruckLib.HashFs;
using TruckLib.Models;

namespace BetterGPSNavigation;

internal sealed class GameAssets : IDisposable
{
    private readonly List<IDisposable> disposables = [];
    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<IFileSystem,GameAssets> Owners=new();
    internal static bool IsOwned(IFileSystem fs)=>Owners.TryGetValue(fs,out _);
    internal static string SourceOf(IFileSystem fs,string path)
    {
        if(!Owners.TryGetValue(fs,out var owner))return "";
        for(int i=0;i<owner.resourceFileSystems.Count;i++)if(owner.resourceFileSystems[i].FileExists(path))return owner.Sources[i];
        return "";
    }
    internal static void RecordCached(IFileSystem fs,ResourceUse use)
    {if(Owners.TryGetValue(fs,out var owner))owner.used[use.Path]=use;}
    private AssetLoader? loader;
    private int disposed;
    private IReadOnlyList<IFileSystem> resourceFileSystems=[];
    public AssetLoader Loader => LoaderOverride??loader??throw new ObjectDisposedException(nameof(GameAssets));
    public IReadOnlyList<string> Sources { get; }
    public List<string> LoadErrors { get; } = [];
    private readonly System.Collections.Concurrent.ConcurrentDictionary<string,ResourceUse> used = new();
    internal sealed record ResourceUse(string Path,string Source,string Sha256);
    public void WriteManifest(string path) => File.WriteAllText(path,System.Text.Json.JsonSerializer.Serialize(
        new {sources=Sources,errors=LoadErrors,resources=used.Values.OrderBy(r=>r.Path)},LiveProtocol.Json));

    private GameAssets(AssetLoader loader, List<string> sources, List<IDisposable> disposables)
    {
        this.loader = loader;
        Sources = sources;
        this.disposables = disposables;
    }

    public static GameAssets Open(Options options)
    {
        if (!Directory.Exists(options.GameRoot))
            throw new DirectoryNotFoundException($"ATS game root not found: {options.GameRoot}");

        var fileSystems = new List<IFileSystem>();
        var sources = new List<string>();
        var disposables = new List<IDisposable>();

        var mounts=ReadMounts(Path.Combine(options.DocumentsRoot,"game.log.txt"));
        var failures=new List<string>();
        if(mounts.Count==0) throw new InvalidDataException("No resource mounts in game.log.txt. Start ATS and load your profile first.");
        foreach(var mount in mounts.Where(m=>!options.IgnoreMods || m.Pool!="mod"))
        {
            int before=fileSystems.Count;
            TryAddArchive(mount.Path, fileSystems, sources, disposables, isMod: mount.Pool=="mod");
            if(fileSystems.Count==before)failures.Add(mount.Path);
        }

        if (fileSystems.Count == 0)
            throw new InvalidOperationException("No readable ATS .scs archives were found.");

        fileSystems.Reverse();
        sources.Reverse();
        var result=new GameAssets(new AssetLoader(fileSystems.ToArray()),sources,disposables);
        result.resourceFileSystems=fileSystems.ToArray();
        result.LoadErrors.AddRange(failures);
        var readCache=new AssetReadCache();
        for(int i=0;i<fileSystems.Count;i++)fileSystems[i]=new TrackedFileSystem(fileSystems[i],sources[i],result.used,readCache);
        result.LoaderOverride=new AssetLoader(fileSystems.ToArray());
        Owners.Add(result.LoaderOverride,result);
        return result;
    }
    private AssetLoader? LoaderOverride;

    internal byte[] ReadTexture(string tobjPath)
    {
        foreach(var fs in resourceFileSystems)
        {
            if(!fs.FileExists(tobjPath))continue;
            if(fs is IHashFsReader hash)
            {
                var parts=hash.Extract(tobjPath);if(parts.Length==2)return parts[1];
            }
            using var header=fs.Open(tobjPath);var tobj=Tobj.Load(header);
            var path=tobj.TexturePath.StartsWith('/')?tobj.TexturePath:tobjPath[..(tobjPath.LastIndexOf('/')+1)]+tobj.TexturePath;
            using var texture=Loader.Open(path);using var data=new MemoryStream();texture.CopyTo(data);return data.ToArray();
        }
        throw new FileNotFoundException("Mounted texture source not found",tobjPath);
    }

    // HashFS v2 embeds DDS data in the TOBJ entry; hashing Open(tobj) alone hashes only its header.
    public string? TextureSha256(string tobjPath)
    {
        var key=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(tobjPath)));
        var use=AssetIndexCache.GetShared(Loader,"texture-content-hash-"+key,()=>TextureUseFresh(tobjPath));
        if(use is not null)used[use.Path]=use;
        return use?.Sha256;
    }
    private ResourceUse? TextureUseFresh(string tobjPath)
    {
        for(int i=0;i<resourceFileSystems.Count;i++)
        {
            var fs=resourceFileSystems[i];if(!fs.FileExists(tobjPath))continue;
            try
            {
                byte[]? packed=null;
                if(fs is IHashFsReader hash)
                {
                    var parts=hash.Extract(tobjPath);if(parts.Length==2)packed=parts[1];
                }
                string sha;var textureSource=Sources[i];
                if(packed is not null)sha=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(packed));
                else
                {
                    using var header=fs.Open(tobjPath);var tobj=Tobj.Load(header);
                    var path=tobj.TexturePath.StartsWith('/')?tobj.TexturePath:tobjPath[..(tobjPath.LastIndexOf('/')+1)]+tobj.TexturePath;
                    using var texture=Loader.Open(path);sha=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(texture));
                    if(used.TryGetValue(path,out var source))textureSource=source.Source;
                }
                return new(tobjPath+"#texture",textureSource,sha);
            }
            catch(Exception e) when(e is IOException or InvalidDataException or ArgumentException){return null;}
        }
        return null;
    }

    private static void TryAddArchive(
        string path,
        List<IFileSystem> fileSystems,
        List<string> sources,
        List<IDisposable> disposables,
        bool isMod)
    {
        path = Environment.ExpandEnvironmentVariables(path.Replace('/', Path.DirectorySeparatorChar));
        if (!File.Exists(path) && !Directory.Exists(path))
        {
            Console.WriteLine($"[WARN] {(isMod ? "Mounted mod" : "archive")} not found: {path}");
            return;
        }

        try
        {
            if(Directory.Exists(path))
            {
                fileSystems.Add(new MountedDirectoryFileSystem(path));
                sources.Add(path+" [DIRECTORY]");
            }
            else if (LooksLikeZip(path))
            {
                var zip = ZipScsFileSystem.OpenArchive(path);
                fileSystems.Add(zip);
                disposables.Add(zip);
                sources.Add(path + " [ZIP]");
            }
            else
            {
                var hash = HashFsReader.Open(path);
                fileSystems.Add(hash);
                disposables.Add(hash);
                sources.Add(path + " [HashFS]");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[WARN] Could not open {(isMod ? "mod" : "archive")} '{path}': {ex.Message}");
        }
    }

    private static bool LooksLikeZip(string path)
    {
        using var stream = File.Open(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        Span<byte> header = stackalloc byte[4];
        if (stream.Read(header) < 4) return false;
        return header[0] == 0x50 && header[1] == 0x4B &&
               ((header[2] == 0x03 && header[3] == 0x04) ||
                (header[2] == 0x05 && header[3] == 0x06) ||
                (header[2] == 0x07 && header[3] == 0x08));
    }

    internal sealed record Mount(string Path,string Pool);
    internal static IReadOnlyList<Mount> ParseMounts(IEnumerable<string> lines)
    {
        var result=new List<Mount>();
        foreach(var line in lines)
        {
            var match=System.Text.RegularExpressions.Regex.Match(line,@"\[fs\] device (.+?) mounted to (core|scs|mod) pool\.");
            if(!match.Success)continue;
            var path=match.Groups[1].Value.Trim().Trim('"');
            result.RemoveAll(m=>m.Path.Equals(path,StringComparison.OrdinalIgnoreCase));
            result.Add(new(path,match.Groups[2].Value));
        }
        return result;
    }
    internal static IReadOnlyList<Mount> ReadMounts(string logPath)
    {
        if (!File.Exists(logPath))
        {
            Console.WriteLine($"[WARN] game.log.txt not found: {logPath}; continuing without mounted mods.");
            return [];
        }

        string[] lines;
        using (var stream = File.Open(logPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
        using (var reader = new StreamReader(stream))
        {
            var text = reader.ReadToEnd();
            lines = text.Replace("\r\n", "\n").Split('\n');
        }

        return ParseMounts(lines);
    }

    public void Dispose()
    {
        if(Interlocked.Exchange(ref disposed,1)!=0)return;
        var previousLoader=Loader;AssetIndexCache.Release(previousLoader);Owners.Remove(previousLoader);
        foreach (var disposable in disposables.AsEnumerable().Reverse())
        {
            try { disposable.Dispose(); } catch { /* best effort */ }
        }
        resourceFileSystems=[];disposables.Clear();used.Clear();LoaderOverride=null;loader=null;
    }
}
