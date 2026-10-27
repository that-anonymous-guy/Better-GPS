using System.Text.Json;
using BetterGPSNavigation;

internal static class TrafficStreetTests
{
    internal static void Run()
    {
        const string folder="validation/ingame-navigation/20261011_212219_3b19c6af";
        int count=0;void Check(bool ok,string why){count++;if(!ok)throw new Exception(why);}
        using var inspectedObjects=JsonDocument.Parse(File.ReadAllText("validation/street-accuracy/mounted/nearby-map-items.json"));
        var inspectedModels=inspectedObjects.RootElement.GetProperty("signDefinitions").EnumerateArray().ToDictionary(
            o=>o.GetProperty("Uid").GetUInt64().ToString("X16"),o=>o.GetProperty("ModelDesc").GetString()!);
        foreach(var stem in new[]{"live_000007_FD38E35A2E1D41EF","live_000011_0E37E5592B5B5DCA"})
        {
            var saved=JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText($"{folder}/{stem}_named-network.json"),LiveProtocol.Json)!;
            var panels=JsonSerializer.Deserialize<ExtractedSignPanel[]>(File.ReadAllText($"{folder}/{stem}_panels.json"))!;
            var graph=new DirectedRoadNetworkGraph(saved.Region,saved.Portions,saved.Connections,saved.Errors);
            var sources=saved.Sources.Select(s=>
            {
                var panel=panels.FirstOrDefault(p=>p.SignUid==s.ObjectId&&p.Status=="OK"&&p.AreaName==s.Id.Split(':')[1]);
                // Old panel recordings predate support metadata. Retrieve it
                // from the independent mounted-object inspection, not the
                // expected route labels or a height-based assumption.
                if(panel is not null&&panel.SupportModel.Length==0)
                {
                    if(inspectedModels.TryGetValue(s.ObjectId,out var model))panel=panel with{SupportModel=model};
                }
                bool overhead=s.Source=="STREET_NAME_SIGN"&&s.GroupMethod!="EXPLICIT_INTERSECTION_STREET_HEADER"&&panel is not null&&
                    StreetNameLinker.OverheadInformationPlate(graph,panel,NetworkPortion.V(s.Position));
                return NamedRoadNetwork.Reassociate(graph,overhead?s with{Source="STREET_CONFIRMATION_SIGN"}:s);
            }).ToArray();
            var solved=NamedRoadNetwork.Prepare(graph,sources);NamedNetworkValidation.Validate(solved,graph);
            var route=JsonSerializer.Deserialize<LiveMessage>(File.ReadAllText($"{folder}/{stem}_route.json"),LiveProtocol.Json)!.Route!;
            var joined=NamedRoadNetwork.Join(solved,route.RoadIdentities!.Spans,route);
            bool Named(int segment,string name)=>joined.Spans.Single(s=>s.SegmentIndex==segment) is {Status:"RESOLVED"} span&&
                span.Identities.Any(i=>i.Identity==name);
            if(stem.Contains("000007"))
            {
                Check(Named(3,"Paradise Rd"),"Left-turn outgoing carriageway missing Paradise Rd");
                Check(Named(4,"Paradise Rd"),"Paradise Rd not carried to the northbound road");
                Check(joined.Contexts.Single(c=>c.ActionId==route.Actions[0].Id).TargetSpanIds.Contains("segment:3"),"Turn target skips its actual exit");
            }
            else
            {
                Check(Named(6,"9th St"),"Approach street differs from its overhead sign");
                Check(Named(8,"L St"),"Right turn names the crossed street instead of overhead L Street");
                Check(Named(16,"I St"),"L Street name carried beyond the overhead I Street boundary");
                Check(!joined.Spans.Single(s=>s.SegmentIndex==16).Identities.Any(i=>i.Identity=="9th St"),"Crossed 9th street used as current road");
                var engine=new LiveGuidanceEngine();engine.Accept(new(2,"Hello","streets",0,0,0));
                engine.Accept(new(2,"RouteSnapshot","streets",1,0,1,route with{RoadIdentities=joined}));
                var edge=route.Edges.First(e=>e.SegmentIndex==6);
                var heading=Math.Atan2(edge.End.Z-edge.Start.Z,edge.End.X-edge.Start.X)*180/Math.PI;
                var guidance=engine.Accept(new(2,"VehicleState","streets",2,100,1,Vehicle:new(edge.Start,heading,10,false)));
                Check(guidance.Road=="L St", "Actual live right-turn instruction lost L Street");
            }
        }
        Console.WriteLine($"PASS: {count} recorded overhead-street and divided-junction assertions.");
    }
}
