using BetterGPSNavigation;
using TruckLib.ScsMap;
using System.Text;
using System.Text.Json;
internal static class NativePathFixTests
{
    internal static void Run(string game,string docs,string capture,string output)
    {
        Directory.CreateDirectory(output);
        using var captureReader=new StreamReader(new FileStream(capture,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete));
        var text=captureReader.ReadToEnd();
        var native=text[..(text.LastIndexOf('\n')+1)].Split('\n',StringSplitOptions.RemoveEmptyEntries).Select(l=>NativeCaptureProtocol.Decode(Encoding.UTF8.GetBytes(l)))
            .Last(m=>m.Type=="RawRouteSnapshot").Route!.WithoutZeroDistanceDuplicates();
        var options=new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."};
        using var assets=GameAssets.Open(options);
        var loader=new RouteMapLoader(assets,options,output,null);
        var map=Map.Open(options.MapPath,assets.Loader,loader.Select(native)?.ToArray());
        var route=native.ToProbe("native-path-audit");
        var ppd=new PpdResolver(assets.Loader);var extractor=new ManeuverFeatureExtractor(ppd);
        var results=new List<object>();int segment=-1,validated=0,changed=0;
        foreach(var row in route.Logical)
        {
            if(row.Type==0){segment++;continue;}
            if(row.Type!=1||segment+1>=route.Physical.Count)continue;
            var prefab=(Prefab)map.MapItems[row.ItemUid];
            int Index(ulong uid)=>prefab.Nodes.FindIndex(n=>n.Uid==uid);
            var entry=Index(route.Physical[segment].NodeUid);var exit=Index(route.Physical[segment+1].NodeUid);
            var boundary=map.Nodes[route.Physical[segment].NodeUid];
            var loop=new ProbeRoute{Stem="loop",PhysicalPath="",LogicalPath="",
                Physical=[new(0,boundary.Uid,0,0,100,10),new(1,boundary.Uid,100,10,0,0)],
                Logical=[new(0,0,0,boundary.Uid,0,0,0,0,100,10),new(1,1,row.NodeId,prefab.Uid,50,5,0,0,50,5),new(2,0,0,boundary.Uid,50,5,0,0,0,0)]};
            if(RouteMapItemSelector.Resolve(map,boundary,boundary,loop,0)?.Uid!=prefab.Uid)
                throw new Exception("Repeated boundary ignored its native prefab and selected the adjacent road.");
            var ports=extractor.Analyze(prefab,entry,exit);
            var live=extractor.AnalyzeRoute(prefab,entry,exit,route,segment);
            if(live.NativePathStatus=="NATIVE_GRAPH_AND_LANE_SEQUENCE_VALIDATED")
            {
                validated++;
                if(!live.SelectedCurveIds.SequenceEqual(ports.SelectedCurveIds))changed++;
                if(!ports.Branches.Any(b=>b.Selected&&b.CurveIds.SequenceEqual(live.SelectedCurveIds)))
                    throw new Exception("Native override escaped the forward lane graph.");
                var graph=new ExactNavGraphResolver(ppd).Resolve(prefab,entry,exit,row.NodeId,
                    route.Physical[segment].TotalDistanceTillEndM-row.TotalDistanceTillEndM,
                    row.TotalDistanceTillEndM-route.Physical[segment+1].TotalDistanceTillEndM,null,forwardOnly:true);
                if(!graph.ExactPathFound||graph.Orientation!=ExactNavGraphOrientation.EntryToExit||!graph.VehicleCurveIds.SequenceEqual(live.SelectedCurveIds))
                    throw new Exception("Live geometry disagrees with independent native graph.");
            }
            var independent=new ExactNavGraphResolver(ppd).Resolve(prefab,entry,exit,row.NodeId,
                route.Physical[segment].TotalDistanceTillEndM-row.TotalDistanceTillEndM,
                row.TotalDistanceTillEndM-route.Physical[segment+1].TotalDistanceTillEndM,null,forwardOnly:true);
            results.Add(new{segment,row.NodeId,live.NativePathStatus,independent.Orientation,independent.ValidationStatus,independent.CandidatePathCount,independent.CandidateOrientationCount,independent.DistanceMatch2m,independent.EntryDistanceDeltaM,independent.ExitDistanceDeltaM,independent.VehicleCurveIds,ports.SelectedCurveIds,changed=!live.SelectedCurveIds.SequenceEqual(ports.SelectedCurveIds)});
        }
        File.WriteAllText(Path.Combine(output,"native-paths.json"),JsonSerializer.Serialize(new{validated,changed,results},LiveProtocol.Json));
        if(validated==0)throw new Exception("No real native path was corroborated; inspect native-paths.json.");
        Console.WriteLine($"PASS: {validated} real native path occurrences corroborated; {changed} representative paths corrected.");
    }
}
