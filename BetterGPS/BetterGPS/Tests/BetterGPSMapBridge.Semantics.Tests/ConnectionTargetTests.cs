using System.Text.Json;
using BetterGPSNavigation;

internal static class ConnectionTargetTests
{
    internal static void Run()
    {
        var directory="validation/overlay-census/replay";
        var route=JsonSerializer.Deserialize<LiveMessage>(File.ReadAllText(Directory.GetFiles(directory,"*_route.json").Single()),LiveProtocol.Json)!.Route!;
        var network=JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(Directory.GetFiles(directory,"*_named-network.json").Single()),LiveProtocol.Json)!;
        var spans=route.RoadIdentities!.Spans;
        DirectedRoadNetworkGraph Graph(NetworkConnection[]? connections=null)=>new(network.Region,network.Portions,connections??network.Connections,network.Errors);
        var contexts=RouteConnectionTargets.Build(Graph(),route,spans);
        int count=0;void Check(bool ok,string why){count++;if(!ok)throw new Exception(why);}
        ActionRoadContext At(ActionRoadContext[] all,int segment)=>all.Single(c=>c.ActionId==route.Actions.Single(a=>a.SegmentIndex==segment).Id);
        string[] Labels(ActionRoadContext c)=>spans.Where(s=>c.JoinedSpanIds.Contains(s.Id)).SelectMany(s=>s.Identities).Select(i=>i.Identity).Distinct().ToArray();
        Check(Labels(At(contexts,11)).SequenceEqual(new[]{"CA-99"}),"Entrance failed to follow chosen merge chain");
        Check(Labels(At(contexts,24)).SequenceEqual(new[]{"CA-180"}),"Exit failed to follow chosen KEEP_RIGHT fork");
        Check(Labels(At(contexts,104)).SequenceEqual(new[]{"CA-99"}),"Second entrance failed");
        Check(At(contexts,127).JoinedSpanIds.Length==0,"Exit skipped downstream city turn");
        Check(At(contexts,85).JoinedSpanIds.Length==0,"Local unnamed road became distant highway");
        Check(At(contexts,0).JoinedSpanIds.Length==0,"Access skipped next turn");
        Check(At(contexts,59).JoinedSpanIds.Length==0,"Known outgoing street replaced by later road");
        Check(contexts.Where(c=>c.JoinedSpanIds.Length>0).All(c=>c.TargetSpanIds.All(id=>spans.Single(s=>s.Id==id).Status=="UNKNOWN")),"Destination renamed ramp");
        Check(RouteConnectionTargets.Build(Graph([]),route,spans).All(c=>c.JoinedSpanIds.Length==0),"Disconnected geometry supplied destination");
        var conflict=spans.Select(s=>s.SegmentIndex==14?s with{Status="CONFLICT"}:s).ToArray();
        Check(At(RouteConnectionTargets.Build(Graph(),route,conflict),11).JoinedSpanIds.Length==0,"Conflict skipped");
        var candidate=spans.Select(s=>s.SegmentIndex==14?s with{Candidates=spans[16].Identities}:s).ToArray();
        Check(At(RouteConnectionTargets.Build(Graph(),route,candidate),11).JoinedSpanIds.Length==0,"Candidate skipped");
        var longLeg=spans.Select(s=>s.SegmentIndex==16?s with{WorldEndM=s.WorldEndM+2000}:s).ToArray();
        Check(At(RouteConnectionTargets.Build(Graph(),route,longLeg),11).JoinedSpanIds.Length==0,"Distant road borrowed");
        var joined=route with{RoadIdentities=route.RoadIdentities with{Contexts=contexts}};
        RoadIdentityValidation.Validate(joined,joined.RoadIdentities!);
        Check(true,"Joined contexts failed wire validation");
        var sample=File.ReadLines(Path.Combine(directory,"guidance.jsonl")).Select(l=>JsonSerializer.Deserialize<LiveGuidance>(l,LiveProtocol.Json)!)
            .First(g=>g.Instruction is not null);
        var unknown=new LiveRoadContext("UNKNOWN",[],[],"OUTGOING_SEGMENT");
        var destination=new LiveRoadContext("RESOLVED",[new("CA-99","RouteReference","NORTH","MEDIUM","MEDIUM",["test"])],[],"SELECTED_NATIVE_CONNECTION_DESTINATION");
        var display=NativeBridgeCli.Describe(sample with{CurrentRoad=unknown,TargetRoad=unknown,JoinedRoad=destination,Road=null,Cardinal=null,SignTargets=[]});
        Check(display.Contains("toward CA-99 NORTH")&&!display.Contains("onto CA-99"),"Connector displayed as highway ownership");
        var direct=NativeBridgeCli.Describe(sample with{CurrentRoad=unknown,TargetRoad=destination,JoinedRoad=unknown,SignTargets=[]});
        Check(direct.Contains("onto CA-99 NORTH"),"Direct road target stopped working");
        Console.WriteLine($"PASS: {count} selected-connection destination assertions.");
    }
}
