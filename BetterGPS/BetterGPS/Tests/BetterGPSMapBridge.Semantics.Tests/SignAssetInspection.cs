using System.Text.Json;
using BetterGPSNavigation;
using TruckLib.Models;
using TruckLib.HashFs;

internal static class SignAssetInspection
{
    // Read-only investigation of the mounted assets. Does not load or change the map.
    public static void Run(string game, string docs, string pathsFile, string output)
    {
        Directory.CreateDirectory(output);
        using var assets = GameAssets.Open(new Options { GameRoot=game, DocumentsRoot=docs, ProbePath=".", OutputRoot=output });
        File.WriteAllLines(Path.Combine(output, "asset_sources.txt"), assets.Sources);
        var paths = JsonSerializer.Deserialize<string[]>(File.ReadAllText(pathsFile)) ?? [];
        var results = new List<object>();
        foreach (var path in paths.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            try
            {
                if (!path.StartsWith('/') || path.Split('/').Any(p => p is "." or ".."))
                    throw new ArgumentException("Invalid virtual asset path");
                if (path.EndsWith(".pmd", StringComparison.OrdinalIgnoreCase))
                {
                    var model = Model.Open(path, assets.Loader);
                    var parts = model.Parts.Select(p => new {
                        name=p.Name.ToString(),
                        locators=p.Locators.Select(l => new { name=l.Name.ToString(),
                            position=new[]{l.Position.X,l.Position.Y,l.Position.Z},
                            rotation=new[]{l.Rotation.X,l.Rotation.Y,l.Rotation.Z,l.Rotation.W}, scale=l.Scale })
                    }).ToArray();
                    var variants=model.Variants.Select(v=>new {name=v.Name.ToString(),
                        attributes=v.Attributes.Select(a=>new {tag=a.Tag.ToString(),type=a.Type,value=a.Value}).ToArray()}).ToArray();
                    results.Add(new { path, status="OK", parts, variants });
                }
                else
                {
                    var destination = Path.GetFullPath(Path.Combine(output, path.TrimStart('/').Replace('/', Path.DirectorySeparatorChar)));
                    var root = Path.GetFullPath(output).TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
                    if (!destination.StartsWith(root,StringComparison.OrdinalIgnoreCase)) throw new ArgumentException("Path outside output");
                    Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                    using var source = assets.Loader.Open(path);
                    using var target = File.Create(destination);
                    source.CopyTo(target);
                    if (path.EndsWith(".tobj", StringComparison.OrdinalIgnoreCase))
                        ExportPackedTexture(assets, path, Path.ChangeExtension(destination,"dds"));
                    results.Add(new { path, status="OK" });
                }
            }
            catch (Exception ex) { results.Add(new { path, status="ERROR", detail=ex.Message }); }
        }
        File.WriteAllText(Path.Combine(output,"inspection.json"), JsonSerializer.Serialize(results,new JsonSerializerOptions { WriteIndented=true }));
        Console.WriteLine($"Inspected {results.Count} mounted sign assets. Results: {Path.GetFullPath(output)}");
    }

    private static void ExportPackedTexture(GameAssets assets, string path, string destination)
    {
        File.WriteAllBytes(destination,assets.ReadTexture(path));
    }
}
