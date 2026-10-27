using System.Numerics;
using System.Text.Json;
using BetterGPSNavigation;
using TruckLib.ScsMap;

internal static class RemainingMapInspection
{
    internal static void Run(string game,string docs,string capture,string result,string output,int first=85,int last=102)
    {
        Directory.CreateDirectory(output);
        var network=JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(Directory.GetFiles(result,"*_named-network.json").Single()),LiveProtocol.Json)!;
        var route=JsonSerializer.Deserialize<LiveMessage>(File.ReadAllBytes(Directory.GetFiles(result,"*_route.json").Single()),LiveProtocol.Json)!.Route!;
        var points=route.RoadIdentities!.Spans.Where(s=>s.SegmentIndex>=first&&s.SegmentIndex<=last).SelectMany(s=>
            network.Portions.Single(p=>p.Id==DirectedRoadNetworkGraph.Id(s.ItemUid,s.EntryNodeUid,s.ExitNodeUid)).Paths.SelectMany(p=>p)).Select(NetworkPortion.V).ToArray();
        var options=new Options{GameRoot=game,DocumentsRoot=docs,ProbePath=".",OutputRoot=output};
        using var assets=GameAssets.Open(options);
        var native=File.ReadLines(capture).Where(l=>l.Contains("\"type\":\"RawRouteSnapshot\""))
            .Select(l=>NativeCaptureProtocol.Decode(System.Text.Encoding.UTF8.GetBytes(l)).Route!).First();
        var loader=new RouteMapLoader(assets,options,output,null);
        var map=Map.Open(options.MapPath,assets.Loader,loader.Select(native)?.ToArray());
        var rows=new List<object>();var tokens=new HashSet<string>();var nearUids=new HashSet<ulong>();
        foreach(var item in EnumerateItems(map.MapItems.Values))
        {
            var node=item.GetType().GetProperty("Node")?.GetValue(item);
            if(node?.GetType().GetProperty("Position")?.GetValue(node) is not Vector3 position)continue;
            var distance=points.Min(p=>Vector2.Distance(new(p.X,p.Z),new(position.X,position.Z)));
            if(distance>200)continue;
            nearUids.Add(item.Uid);
            var fields=new Dictionary<string,string>();
            foreach(var key in new[]{"Model","Look","Variant","SignTemplate","Text","Name","Type"})
            {
                var value=item.GetType().GetProperty(key)?.GetValue(item)?.ToString();
                if(!string.IsNullOrEmpty(value))fields[key]=value;
            }
            if(item.GetType().Name=="Model"&&fields.TryGetValue("Name",out var model))tokens.Add(model);
            rows.Add(new{uid=item.Uid.ToString("X16"),kind=item.GetType().Name,position=BoardModelResolver.V(position),distance,fields});
        }
        var definitions=new List<object>();var index=new SiiClassIndex(assets.Loader,"model_def");
        foreach(var path in assets.Loader.GetFiles("/def/world/").Where(p=>p.EndsWith(".sii")&&index.MayContain(p)))
        {
            try
            {
                foreach(var unit in SiiDocumentCache.Open(path,assets.Loader).Units.Where(u=>u.Class=="model_def"&&tokens.Any(t=>u.Name=="model."+t)))
                    definitions.Add(new{unit.Name,path,unit.Attributes});
            }
            catch(Exception e) when(e is IOException or ArgumentException){definitions.Add(new{path,error=e.Message});}
        }
        // Flatten only this diagnostic's in-memory map so compound signs are also inspected.
        foreach(var sign in EnumerateItems(map.MapItems.Values).OfType<Sign>().ToArray())
            map.MapItems.TryAdd(sign.Uid,sign);
        var signDefinitions=new SignRoadIdentityResolver(map,assets.Loader).Signs.Where(s=>nearUids.Contains(s.Uid))
            .Select(s=>new{s.Uid,s.Model,s.DefinitionName,s.ModelDesc,s.RawStrings,s.LegacyRoads,s.Areas}).ToArray();
        File.WriteAllText(Path.Combine(output,"nearby-map-items.json"),JsonSerializer.Serialize(new{radiusM=200,segments=Enumerable.Range(first,last-first+1),rows,definitions,signDefinitions},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine($"Inspected {rows.Count} map objects and {definitions.Count} mounted model definitions near portions {first}..{last}.");
    }

    private static IEnumerable<MapItem> EnumerateItems(IEnumerable<MapItem> items)
    {
        foreach(var item in items)
        {
            yield return item;
            if(item is Compound compound)
                foreach(var child in EnumerateItems(compound.MapItems.Values))yield return child;
        }
    }
}
