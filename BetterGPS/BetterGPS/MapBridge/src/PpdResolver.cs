using System.Globalization;
using TruckLib;
using TruckLib.Models.Ppd;
using TruckLib.Sii;

namespace BetterGPSNavigation;

internal sealed class PpdResolver
{
    private readonly IFileSystem fs;
    private readonly Dictionary<string, string> prefabPaths = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, PrefabDescriptor?> descriptors = new(StringComparer.OrdinalIgnoreCase);
    private bool indexed;
    internal IFileSystem FileSystem=>fs;

    public PpdResolver(IFileSystem fs)
    {
        this.fs = fs;
    }

    public int IndexedPrefabCount
    {
        get
        {
            EnsureIndex();
            return prefabPaths.Count;
        }
    }
    internal bool TryGetPath(string token,out string path){EnsureIndex();return prefabPaths.TryGetValue(token,out path!);}

    public bool TryResolve(string prefabToken, out string ppdPath, out PrefabDescriptor? descriptor, out string? error)
    {
        EnsureIndex();
        descriptor = null;
        error = null;
        if (!prefabPaths.TryGetValue(prefabToken, out var resolvedPath))
        {
            ppdPath = "";
            error = $"Prefab token '{prefabToken}' not found in /def/world/prefab*.sii";
            return false;
        }
        ppdPath = resolvedPath;

        if (descriptors.TryGetValue(prefabToken, out descriptor))
        {
            if (descriptor is null) error = $"PPD previously failed to load: {ppdPath}";
            return descriptor is not null;
        }

        try
        {
            descriptor = PrefabDescriptor.Open(ppdPath, fs);
            descriptors[prefabToken] = descriptor;
            return true;
        }
        catch (Exception ex)
        {
            descriptors[prefabToken] = null;
            error = $"{ex.GetType().Name}: {ex.Message}";
            return false;
        }
    }

    private void EnsureIndex()
    {
        if (indexed) return;
        indexed = true;
        var paths=AssetIndexCache.GetShared(fs,"prefab-paths",LoadPaths);
        foreach(var pair in paths)prefabPaths[pair.Key]=pair.Value;
    }

    private Dictionary<string,string> LoadPaths()
    {

        if (!fs.DirectoryExists("/def/world/"))
            throw new DirectoryNotFoundException("/def/world/ not found in ATS assets.");

        var files = fs.GetFiles("/def/world/")
            .Where(f => f.EndsWith(".sii", StringComparison.OrdinalIgnoreCase))
            .Where(f => f.StartsWith("/def/world/prefab.", StringComparison.OrdinalIgnoreCase))
            .OrderBy(f => f, StringComparer.OrdinalIgnoreCase)
            .ToList();

        foreach (var file in files)
        {
            SiiFile sii;
            try
            {
                sii = SiiDocumentCache.Open(file, fs);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[WARN] Could not parse prefab definition '{file}': {ex.Message}");
                continue;
            }

            foreach (var unit in sii.Units)
            {
                if (!unit.Attributes.TryGetValue("prefab_desc", out var raw)) continue;
                var path = Convert.ToString(raw, CultureInfo.InvariantCulture)?.Trim().Trim('"');
                if (string.IsNullOrWhiteSpace(path)) continue;

                var token = unit.Name.Split('.').LastOrDefault();
                if (string.IsNullOrWhiteSpace(token)) continue;

                // This mirrors the strategy used by ETS2LA's PpdFileHandler. AssetLoader
                // already resolves overrides for each virtual SII path.
                prefabPaths.TryAdd(token, NormalizeVirtualPath(path));
            }
        }
        return prefabPaths;
    }

    private static string NormalizeVirtualPath(string path)
    {
        var result = path.Replace('\\', '/');
        return result.StartsWith('/') ? result : "/" + result;
    }
}
