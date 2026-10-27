using BetterGPSNavigation;
using System.Text;
static class RoadNameRegressionTests
{
    internal static void Run()
    {
        int checks=0;void Check(bool value,string why){checks++;if(!value)throw new Exception(why);}
        string U(int i)=>i.ToString("X16");
        NetworkPortion P(int uid,int entry,int exit,float z,bool reverse=false,int lanes=2,float y=0)=>new(
            DirectedRoadNetworkGraph.Id(U(uid),U(entry),U(exit)),U(uid),"ROAD",U(entry),U(exit),Enumerable.Range(0,lanes)
                .Select(l=>new[]{new[]{reverse?100f:0,y,z+l*4},new[]{reverse?0f:100,y,z+l*4}}).ToArray(),[]);
        var a=P(1,11,12,-12);var b=P(2,21,22,8,true);
        DirectedRoadNetworkGraph G(NetworkPortion[] p)=>new(new(-500,500,-500,500),p,[],[]);
        NetworkNameSource S(string identity="I-95")=>new("overlay","object","ROAD_OVERLAY","MapAnnotation",identity,identity,"RouteReference","","",
            [50,0,0],null,"UI_ANNOTATION","MEDIUM","AMBIGUOUS_OWNERS",[a.Id,b.Id],[]);
        foreach(var identity in new[]{"I-95","US-101","CA-120","NV-160","IL-13"})
        {
            var graph=G([a,b]);var source=S(identity);
            Check(DividedRoadOverlay.CoversPair(graph,source),"Divided carriageway icon rejected: "+identity);
            var solved=NamedRoadNetwork.Solve(graph,[source]);
            Check(solved.All(p=>p.Status=="RESOLVED"&&p.Identities.Single().Identity==identity&&p.Identities[0].Cardinal.Length==0),"Route lost or travel direction invented");
            NamedNetworkValidation.Validate(new(1,"test","test",graph.Region,graph.Portions,graph.Connections,graph.Errors,[source],solved),graph);
        }
        Check(!DividedRoadOverlay.CoversPair(G([a,P(2,21,22,8)]),S()),"Same-direction parallel road accepted");
        Check(!DividedRoadOverlay.CoversPair(G([a,b with{Paths=b.Paths.Select(path=>path.Select(v=>new[]{v[0],10f,v[2]}).ToArray()).ToArray()}]),S()),"Elevated crossing accepted");
        Check(!DividedRoadOverlay.CoversPair(G([a,P(2,21,22,8,true,1)]),S()),"Single-lane ramp accepted");
        Check(!DividedRoadOverlay.CoversPair(G([a,b]),S() with{Position=[50,0,50]}),"Icon outside pair accepted");
        Check(!DividedRoadOverlay.CoversPair(G([a,b]),S() with{Role="GuideTo"}),"Destination guide accepted as own-road reference");
        Check(!DividedRoadOverlay.CoversPair(G([a,b]),S() with{ReadingConfidence="LOW"}),"Unreadable icon promoted");
        Check(!DividedRoadOverlay.CoversPair(G([a,b]),S() with{Raw="TO I-95"}),"TO reference became own road");
        var independent=S("I-80") with{Id="independent",ObjectId="independent",AssociationStatus="UNIQUE_PHYSICAL_ROAD_BOTH_SENSES",Owners=[a.Id]};
        var constrained=NamedRoadNetwork.Solve(G([a,b]),[independent,S()]);
        Check(constrained.Single(n=>n.PortionId==a.Id).Identities.All(i=>i.Identity=="I-80")&&constrained.Single(n=>n.PortionId==b.Id).Identities.Length==0,
            "Median icon overrode independent road or leaked onto another parallel highway");
        var label=new SegmentRoadIdentity("US-101","RouteReference","","LOW","NONE",[],"DIRECT_NETWORK_ANCHOR");
        var span=new RoadIdentitySpan("s",0,U(1),U(11),U(12),0,100,0,100,"UNKNOWN",[],[label],"test");
        Check(RouteConnectionTargets.Unproven(span),"Low hypothesis blocked connector destination");
        Check(!RouteConnectionTargets.Unproven(span with{Candidates=[label with{IdentityConfidence="MEDIUM"}]}),"Readable candidate skipped");
        Check(!RouteConnectionTargets.Unproven(span with{Status="CONFLICT"}),"Conflict skipped");
        var destination=new LiveRoadContext("RESOLVED",[new("NV-160","RouteReference","NORTH","MEDIUM","MEDIUM",["p"])],[],"destination");
        var guidance=new LiveGuidance(0,"PREPARE","route",1,"a","MERGE",1000,1000/1609.344,null,null,null,null,null,null,[],0,0,"")
            {TargetRoad=new("CANDIDATE",[new("US-101","RouteReference","","LOW","NONE",[])],[],"unproven"),JoinedRoad=destination};
        Check(NativeBridgeCli.Describe(guidance).Contains("toward NV-160 NORTH"),"Candidate target hid joined-road shield");
        guidance=guidance with{Status="CRUISE",CurrentRoad=new("UNKNOWN",[],[],"unknown"),ContinueToward=destination};
        var bytes=PhoneGuidancePublisher.Encode(guidance,1);var text=Encoding.UTF8.GetString(bytes,2368,512).TrimEnd('\0');
        Check(text.StartsWith("Continue toward NV-160 NORTH"),"Phone snapshot lost connector destination");
        Console.WriteLine($"PASS: {checks} divided-road and connector assertions, 5 route families, no guessed cardinal.");
    }
}
