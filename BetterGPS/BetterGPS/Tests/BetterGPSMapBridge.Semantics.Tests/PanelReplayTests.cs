using System.Text;
using System.Text.Json;
using BetterGPSNavigation;
using TruckLib.ScsMap;

internal static class PanelReplayTests
{
    public static void Run(string game,string docs,string capture,string output,bool auditLanes=false,bool exportScene=false)
    {
        Directory.CreateDirectory(output);
        var snapshots=new List<NativeMessage>();
        var files=Directory.Exists(capture)?Directory.GetFiles(capture,"capture.jsonl",SearchOption.AllDirectories):[capture];
        foreach(var input in files.Order(StringComparer.Ordinal))
        using(var reader=new StreamReader(new FileStream(input,FileMode.Open,FileAccess.Read,FileShare.ReadWrite)))
        {
            while(reader.ReadLine() is {} line)
            {
                if(!line.Contains("\"type\":\"RawRouteSnapshot\""))continue;
                var m=NativeCaptureProtocol.Decode(Encoding.UTF8.GetBytes(line));
                if(snapshots.Any(s=>m.Route!.IsSuffixOf(s.Route!)))continue;
                snapshots.Add(m);
            }
        }
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,ProbePath=".",OutputRoot=output});
        Console.WriteLine("Loading installed map for recorded GPS routes...");
        var map=Map.Open("/map/usa.mbd",assets.Loader);
        if(exportScene)BoardCalibrationScene.Write(map,output);
        var bridge=new MapBridge(map,assets.Loader,output,false,textureHash:assets.TextureSha256);
        var summaries=new List<object>();
        foreach(var m in snapshots)
        {
            var stem=$"live_{m.Generation:D6}_{m.Route!.Fingerprint}";
            LiveRoute plan;
            try{plan=bridge.CompileLive(m.Route.ToProbe(stem));}
            catch(InvalidDataException ex) when(Directory.Exists(capture))
            {summaries.Add(new{routeId=stem,status="REJECTED",reason=ex.Message});Console.WriteLine($"REJECTED: {stem}: {ex.Message}");continue;}
            var bytes=LiveProtocol.Encode(new(LiveProtocol.VersionFor(plan),"RouteSnapshot",m.SessionId,0,m.TimestampMs,1,plan));
            File.WriteAllBytes(Path.Combine(output,stem+"_route.json"),bytes);
            if(auditLanes)LaneAuditExport.Write(bridge,assets.Loader,plan,output,Path.Combine(output,stem+"_panels.json"));
            var high=plan.Actions.Count(a=>a.Proofs.Any(p=>p.Source=="SIGN_BOARD" && p.Confidence=="HIGH"));
            summaries.Add(new {plan.RouteId,status="COMPILED",actions=plan.Actions.Length,actionsWithHighFields=high,
                spans=plan.RoadIdentities?.Spans.Length,named=plan.RoadIdentities?.Spans.Count(s=>s.Status=="RESOLVED")});
            Console.WriteLine($"PASS: {stem}: {plan.Actions.Length} actions, {high} with HIGH sign fields; protocol validated.");
        }
        File.WriteAllText(Path.Combine(output,"summary.json"),JsonSerializer.Serialize(summaries,new JsonSerializerOptions{WriteIndented=true}));
        assets.WriteManifest(Path.Combine(output,"asset_manifest.json"));
    }
}
