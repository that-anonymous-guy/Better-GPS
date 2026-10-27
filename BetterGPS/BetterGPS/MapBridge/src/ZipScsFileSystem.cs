using System.IO.Compression;
using System.Text;
using TruckLib;

namespace BetterGPSNavigation;

/// <summary>
/// Minimal read-only IFileSystem for ZIP-based .scs mods. SCS virtual paths use '/'.
/// </summary>
internal sealed class ZipScsFileSystem : IFileSystem, IDisposable
{
    private readonly FileStream stream;
    private readonly ZipArchive archive;
    private readonly Dictionary<string, ZipArchiveEntry> files = new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> directories = new(StringComparer.OrdinalIgnoreCase) { "/" };

    public char DirectorySeparator => '/';
    public string SourcePath { get; }

    private ZipScsFileSystem(string path)
    {
        SourcePath = path;
        stream = File.Open(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        archive = new ZipArchive(stream, ZipArchiveMode.Read, leaveOpen: false);

        foreach (var entry in archive.Entries)
        {
            var virtualPath = Normalize(entry.FullName);
            if (entry.FullName.EndsWith('/') || string.IsNullOrEmpty(entry.Name))
            {
                AddParents(virtualPath);
                directories.Add(virtualPath.TrimEnd('/'));
                continue;
            }

            files[virtualPath] = entry;
            AddParents(virtualPath);
        }
    }

    public static ZipScsFileSystem OpenArchive(string path) => new(path);

    public bool FileExists(string path) => files.ContainsKey(Normalize(path));

    public bool DirectoryExists(string path)
    {
        var p = Normalize(path).TrimEnd('/');
        if (p.Length == 0) p = "/";
        return directories.Contains(p);
    }

    public IList<string> GetFiles(string path)
    {
        var dir = Normalize(path).TrimEnd('/');
        if (dir.Length == 0) dir = "/";
        if (!DirectoryExists(dir))
            throw new DirectoryNotFoundException(dir);

        var prefix = dir == "/" ? "/" : dir + "/";
        return files.Keys
            .Where(p => p.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
            .Where(p => !p.AsSpan(prefix.Length).Contains('/'))
            .ToList();
    }

    public byte[] ReadAllBytes(string path)
    {
        if(!files.TryGetValue(Normalize(path),out var entry))throw new FileNotFoundException(path);
        if(entry.Length>int.MaxValue)throw new IOException("ZIP resource exceeds supported size.");
        var bytes=GC.AllocateUninitializedArray<byte>((int)entry.Length);
        using var input=entry.Open();input.ReadExactly(bytes);return bytes;
    }

    public string ReadAllText(string path) => ReadAllText(path, Encoding.UTF8);

    public string ReadAllText(string path, Encoding encoding) => encoding.GetString(ReadAllBytes(path));

    public Stream Open(string path)
    {
        if (!files.TryGetValue(Normalize(path), out var entry))
            throw new FileNotFoundException(path);

        // Independent seekable view, with one exact-size allocation instead of
        // MemoryStream growth followed by another full ToArray copy.
        return new MemoryStream(ReadAllBytes(path),false);
    }

    public string? GetParent(string path)
    {
        var p = Normalize(path).TrimEnd('/');
        if (p == "/") return null;
        var index = p.LastIndexOf('/');
        return index <= 0 ? "/" : p[..index];
    }

    private void AddParents(string path)
    {
        var p = path.TrimEnd('/');
        while (true)
        {
            var index = p.LastIndexOf('/');
            if (index <= 0)
            {
                directories.Add("/");
                break;
            }
            p = p[..index];
            directories.Add(p);
        }
    }

    private static string Normalize(string path)
    {
        var p = path.Replace('\\', '/');
        if (!p.StartsWith('/')) p = "/" + p;
        while (p.Contains("//", StringComparison.Ordinal)) p = p.Replace("//", "/", StringComparison.Ordinal);
        return p;
    }

    public void Dispose()
    {
        archive.Dispose();
        // archive owns stream because leaveOpen=false; Dispose is harmless if already closed.
        stream.Dispose();
    }
}
