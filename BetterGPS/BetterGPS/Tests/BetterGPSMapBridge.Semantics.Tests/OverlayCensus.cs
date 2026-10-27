using System.Text.Json;
using BetterGPSNavigation;
using TruckLib.ScsMap;

internal static class OverlayCensus
{
    internal static void Verify(string folder)
    {
        using var json=JsonDocument.Parse(File.ReadAllText(Path.Combine(folder,"overlay-census.json")));
        var tokens=json.RootElement.GetProperty("tokens").EnumerateArray().Select(t=>new
        {token=t.GetProperty("token").GetString()!,count=t.GetProperty("count").GetInt32(),before=t.GetProperty("identity").GetString()!,
            identity=RoadOverlayResolver.NormalizeRoadToken(t.GetProperty("token").GetString()!)}).ToArray();
        var definitions=json.RootElement.GetProperty("definitions").EnumerateArray()
            .Where(d=>d.TryGetProperty("Attributes",out var a)&&a.TryGetProperty("road_names",out _))
            .SelectMany(d=>d.GetProperty("Attributes").GetProperty("road_names").EnumerateArray().Select(t=>t.GetString()!)).Distinct()
            .Select(t=>new{token=t,identity=RoadOverlayResolver.NormalizeRoadToken(t)}).ToArray();
        if(tokens.Any(t=>t.identity.Length>0&&!SignTextClassifier.IsRoadIdentity(t.identity)))throw new Exception("Decoded invalid road identity");
        var missing=tokens.Where(t=>t.identity.Length==0&&t.token is not ("" or "toll_ico" or "agri_check" or "border_ico")).ToArray();
        if(missing.Length>0)throw new Exception("Placed road token still unsupported: "+string.Join(',',missing.Select(t=>t.token)));
        File.WriteAllText(Path.Combine(folder,"decoder-verification.json"),JsonSerializer.Serialize(new
            {total=tokens.Sum(t=>t.count),decoded=tokens.Where(t=>t.identity.Length>0).Sum(t=>t.count),
                added=tokens.Where(t=>t.before.Length==0&&t.identity.Length>0).ToArray(),tokens,definitions},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine($"PASS: all {tokens.Count(t=>t.identity.Length>0)} distinct placed road identities decoded; {tokens.Where(t=>t.identity.Length>0).Sum(t=>t.count)} markers. Icons and empty labels excluded.");
    }
    internal static void Textures(string game,string docs,string output)
    {
        Directory.CreateDirectory(output);
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."});
        foreach(var token in new[]{"bus25","bus40","bus80","txl323","txl338","us93alt","can_1","can_bc3"})
            File.WriteAllBytes(Path.Combine(output,token+".dds"),assets.ReadTexture("/material/ui/map/road/road_"+token+".tobj"));
        assets.WriteManifest(Path.Combine(output,"asset_manifest.json"));
    }
    internal static void Run(string game,string docs,string output)
    {
        Directory.CreateDirectory(output);
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."});
        var definitions=new List<object>();
        foreach(var path in assets.Loader.GetFiles("/def/world/").Where(p=>p.Contains("overlay",StringComparison.OrdinalIgnoreCase)&&p.EndsWith(".sii")))
        {
            try
            {
                foreach(var unit in SiiDocumentCache.Open(path,assets.Loader).Units)
                    definitions.Add(new{path,unit.Class,unit.Name,unit.Attributes});
            }
            catch(Exception e){definitions.Add(new{path,error=e.Message});}
        }
        var materials=assets.Loader.GetFiles("/material/ui/map/road/").ToArray();
        Console.WriteLine($"Mounted definitions: {definitions.Count}; road materials: {materials.Length}. Loading full mounted map for census only...");
        var map=Map.Open("/map/usa.mbd",assets.Loader);
        var resolver=new RoadOverlayResolver(map);
        resolver.WriteGlobalIndex(Path.Combine(output,"ROAD_OVERLAY_INDEX.csv"));
        var tokens=resolver.Overlays.GroupBy(o=>o.RawToken).OrderBy(g=>g.Key)
            .Select(g=>new{token=g.Key,identity=g.First().Identity,count=g.Count()}).ToArray();
        var result=new{objects=map.MapItems.Count,overlays=resolver.Overlays.Count,normalized=resolver.NormalizedCount,tokens,definitions,materials};
        File.WriteAllText(Path.Combine(output,"overlay-census.json"),JsonSerializer.Serialize(result,new JsonSerializerOptions{WriteIndented=true}));
        assets.WriteManifest(Path.Combine(output,"asset_manifest.json"));
        Console.WriteLine($"Census complete: {resolver.Overlays.Count} RoadName overlays, {resolver.NormalizedCount} decoded, {tokens.Length} distinct tokens.");
    }
}
