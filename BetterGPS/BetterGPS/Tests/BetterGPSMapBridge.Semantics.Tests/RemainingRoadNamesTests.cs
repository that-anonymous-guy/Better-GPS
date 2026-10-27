using BetterGPSNavigation;

internal static class RemainingRoadNamesTests
{
    internal static void Run()
    {
        int count=0;void Check(bool ok,string message){count++;if(!ok)throw new Exception(message);}
        string U(int n)=>n.ToString("X16");
        NetworkPortion P(int n,int entry,int exit,float x,float end,float z=0)=>new(
            DirectedRoadNetworkGraph.Id(U(n),U(entry),U(exit)),U(n),"ROAD",U(entry),U(exit),
            [[[x,0,z],[end,0,z]]],[]);
        var a=P(1,1,2,0,100);var b=P(2,2,3,100,200) with{Kind="PREFAB",
            Paths=[[[100,0,0],[200,0,0]],[[100,0,4.5f],[200,0,4.5f]]]};
        var c=P(3,3,4,200,300);var exit=P(2,2,5,100,200) with{Kind="PREFAB",Paths=[[[100,0,9],[200,0,20]]]};
        a=a with{Paths=[[[0,0,0],[100,0,0]],[[0,0,4.5f],[100,0,4.5f]]]};
        var graph=new DirectedRoadNetworkGraph(new(-500,500,-500,500),[a,b,c,exit],
            [new(a.Id,b.Id,"NATIVE_PORT"),new(a.Id,exit.Id,"NATIVE_PORT"),new(b.Id,c.Id,"NATIVE_PORT")],[]);
        Check(RoadNameContinuity.Corridor(graph,a.Id).Contains(b.Id),"Multiple native through lanes cross a shallow exit");
        Check(!RoadNameContinuity.Corridor(graph,a.Id).Contains(exit.Id),"Shallow single-lane exit never inherits mainline name");
        Check(!RoadNameContinuity.Corridor(graph,a.Id,followShallowExits:false).Contains(b.Id),"Street names retain strict junction continuity");
        Check(!RoadNameContinuity.Corridor(graph,a.Id,canEnter:id=>id!=c.Id).Contains(c.Id),"A conflicting local inscription stops propagation");
        var duplicated=b with{Paths=[b.Paths[0],b.Paths[0]]};
        var duplicateGraph=new DirectedRoadNetworkGraph(graph.Region,[a,duplicated,c,exit],graph.Connections,[]);
        Check(!RoadNameContinuity.Corridor(duplicateGraph,a.Id).Contains(b.Id),"Duplicate curves never count as distinct lanes");
        Check(NativeAccessRoads.Find(graph).Contains(exit.Id),"A shallow single-lane fork is an access, not the main carriageway");
        Check(!NativeAccessRoads.Find(graph).Contains(b.Id),"Through carriageway never becomes an access road");
        RoadIdentityAnchor Anchor(string id,int segment)=>new(id,"SIGN_GROUP",id,"CA-99 NORTH","CA-99","RouteReference","NORTH",
            "MEDIUM","LOCAL_SIGN","MEDIUM",U(segment+1),segment,segment*100+50,segment*100+50,[]);
        var anchors=new[]{Anchor("first",0),Anchor("last",2)};
        SegmentRoadIdentity Label(string id,string cardinal="NORTH")=>new("CA-99","RouteReference",cardinal,"MEDIUM","MEDIUM",[id],"DIRECT_NETWORK_ANCHOR");
        var spans=new[]{a,b,c}.Select((p,i)=>new RoadIdentitySpan("s"+i,i,p.ItemUid,p.EntryNodeUid,p.ExitNodeUid,
            i*100,(i+1)*100,i*100,(i+1)*100,i==1?"UNKNOWN":"RESOLVED",i==1?[]:[Label(i==0?"first":"last")],[],"")).ToArray();
        var edges=new[]{a,b,c}.Select((p,i)=>new LiveEdge(i,p.ItemUid,new(i*100,0,0),new((i+1)*100,0,0),i*100,(i+1)*100,i*100,(i+1)*100)).ToArray();
        var route=new LiveRoute("test",edges,[]);
        Check(CapturedRoadContinuity.Fill(graph,spans,anchors,route)[1].Identities.Single() is {Identity:"CA-99",Cardinal:"NORTH",IdentityConfidence:"MEDIUM"},
            "Captured route fills a signed same-road gap without promoting confidence");
        var turn=new LiveAction("turn",0,1,150,"TURN_RIGHT","branch","HIGH",[]);
        Check(CapturedRoadContinuity.Fill(graph,spans,anchors,route with{Actions=[turn]})[1].Status=="UNKNOWN","A road-changing maneuver prevents gap interpolation");
        Check(CapturedRoadContinuity.Fill(graph,spans,anchors.Select(a=>a with{SourceId="one:group"}).ToArray(),route)[1].Status=="UNKNOWN",
            "One physical sign cannot bracket a missing name");
        Check(CapturedRoadContinuity.Fill(graph,spans.Select(s=>s.SegmentIndex==2?s with{Identities=[Label("last","SOUTH")]}:s).ToArray(),anchors,route)[1].Status=="UNKNOWN",
            "Opposite signed directions prevent interpolation");
        Check(CapturedRoadContinuity.Fill(graph,spans.Select(s=>s.SegmentIndex==1?s with{Candidates=[Label("first") with{Identity="I-5"}]}:s).ToArray(),anchors,route)[1].Status=="UNKNOWN",
            "A competing route candidate cannot be erased");
        var disconnected=new DirectedRoadNetworkGraph(graph.Region,graph.Portions,[],[]);
        Check(CapturedRoadContinuity.Fill(disconnected,spans,anchors,route)[1].Status=="UNKNOWN","GPS adjacency alone cannot replace native map connections");
        Check(NamedRoadNetwork.BracketDirectionCompatible(graph,a.Id,c.Id),"Forward same-route endpoints remain compatible");
        Check(!NamedRoadNetwork.BracketDirectionCompatible(graph,c.Id,a.Id),"Behind aligned endpoint cannot name a return loop");
        var snapshot=new RoadIdentitySnapshot(1,"fixture",anchors,CapturedRoadContinuity.Fill(graph,spans,anchors,route),[]);
        RoadIdentityValidation.Validate(route,snapshot);Check(true,"Captured continuity wire evidence validates");
        bool rejected=false;try{RoadIdentityValidation.Validate(route with{Actions=[turn]},snapshot with{Contexts=[new("turn","branch",["s0"],["s2"],["s2"],"CA-99","")]});}
        catch(InvalidDataException){rejected=true;}Check(rejected,"Forged continuity across a turn is rejected");
        var names=graph.Portions.ToDictionary(p=>p.Id,p=>new NamedNetworkPortion(p.Id,p.Id==c.Id?"RESOLVED":"UNKNOWN",
            p.Id==c.Id?[new("CA-180","RouteReference","WEST","MEDIUM","MEDIUM",["later180"],"DIRECT_NETWORK_ANCHOR")]:[],[]));
        var accessRoute=route with{RoadIdentities=new(1,"fixture",anchors,spans,[]),Actions=[turn with{Instruction="MERGE_LEFT"}]};
        Check(NamedRoadNetwork.AccessTarget(graph,names,accessRoute,0).Single() is {Identity:"CA-180",Cardinal:"WEST"},
            "A captured access chain can name its destination across a merge");
        Check(NamedRoadNetwork.AccessTarget(graph,names,accessRoute with{Actions=[turn]},0).Length==0,
            "Access target cannot skip a turn onto another road");
        Check(NamedRoadNetwork.AccessTarget(disconnected,names,accessRoute,0).Length==0,
            "Access target requires exact native connections");
        var local=new NetworkNameSource("route","route","SIGN","MapAnnotation","CA-99","CA-99","RouteReference","","",[0,0,0],null,"FIXTURE","MEDIUM",
            "UNIQUE_PHYSICAL_ROAD_BOTH_SENSES",[a.Id],[]);
        var alias=local with{Id="street",ObjectId="street",Raw="Washington Street",Identity="Washington Street",Kind="StreetName"};
        Check(NamedRoadNetwork.Solve(graph,[local,alias]).Single(n=>n.PortionId==a.Id) is {Status:"RESOLVED",Identities.Length:2},
            "A street name and a numbered route can coexist on the same pavement");
        Console.WriteLine($"PASS: {count} missing-name continuity and ownership assertions.");
    }
}
