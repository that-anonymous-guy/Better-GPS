using System.Text.Json;
using System.Numerics;
using BetterGPSNavigation;

internal static class RoadIdentityTests
{
    public static void Run()
    {
        int count=0;
        void Check(bool ok,string why){count++;if(!ok)throw new Exception(why);}
        void Reject(Action action,string why){try{action();}catch(InvalidDataException){Check(true,why);return;}throw new Exception(why);}
        var probe=new ProbeRoute{Stem="r",PhysicalPath="",LogicalPath="",Logical=[],
            Physical=Enumerable.Range(0,5).Select(i=>new PhysicalRouteRow(i,(ulong)i+1,100,0,400-i*100,0)).ToArray()};
        var edges=Enumerable.Range(0,4).Select(i=>new RouteCorridorEdge(i,i,"Road",(ulong)i+100,"",new(i*100,0,0),new((i+1)*100,0,0),i*100,(i+1)*100,i*1000,(i+1)*1000,90)).ToArray();
        var segments=edges.Select(e=>new RouteCorridorSegment(e.SegmentIndex,"Road",e.ItemUid,e.StartStationM,e.EndStationM,e.StartNavStationM,e.EndNavStationM,e.EdgeIndex,1,"",null)).ToArray();
        var corridor=new RouteCorridor(edges,segments);
        RoadIdentityAnchor Anchor(string id,int segment,string identity="I-5",string cardinal="",string confidence="MEDIUM")=>
            new(id,"ROAD_OVERLAY",id,identity,identity,"RouteReference",cardinal,confidence,
                confidence=="HIGH"?"VERIFIED_BRANCH":"UNIQUE_ROAD_CANDIDATE",confidence,(segment+100).ToString("X16"),segment,
                segment*100+50,segment*1000+500,[]);
        var anchors=new[]{Anchor("a",0),Anchor("b",3)};
        var spans=RoadIdentityResolver.Solve(corridor,probe,anchors,[]);
        Check(spans.Length==4&&spans.All(s=>s.Identities.Any(i=>i.Identity=="I-5")),"Whole chain bracketed without arrows");
        Check(spans.All(s=>s.Identities.All(i=>i.IdentityConfidence=="MEDIUM")),"Geometric candidates never become HIGH from repetition");
        var single=RoadIdentityResolver.Solve(corridor,probe,[anchors[0]],[]);
        Check(single.Skip(1).All(s=>s.Identities.Length==0&&s.Candidates.All(i=>i.IdentityConfidence=="LOW")),"One anchor cannot confirm long interval");
        var sameObject=RoadIdentityResolver.Solve(corridor,probe,[anchors[0] with{SourceId="same:area1"},anchors[1] with{SourceId="same:area2"}],[]);
        Check(sameObject[1].Identities.Length==0,"Two readings of one object cannot bracket a chain");
        var weak=anchors.Select(a=>a with{AssociationConfidence="LOW",AssociationStatus="AMBIGUOUS_ROADS",Cardinal="NORTH"}).ToArray();
        var hypotheses=RoadIdentityResolver.Solve(corridor,probe,weak,[]);
        Check(hypotheses.All(s=>s.Identities.Length==0&&s.Candidates.Length==1&&s.Candidates[0].IdentityConfidence=="LOW"&&s.Candidates[0].CardinalConfidence=="LOW"),"Ambiguous corroboration stays LOW for every field");
        var fork=RoadIdentityResolver.Solve(corridor,probe,[anchors[0]],[new(0,1,"KEEP_LEFT","KEEP_LEFT",150,1500,"",false,"HIGH")]);
        Check(fork[2].Candidates.Length==0,"Single source cannot leak across a fork");
        var boundaries=new[]{new V09ManeuverEvent(0,1,"TURN_LEFT","TURN_LEFT",150,1500,"",false,"HIGH")};
        var split=RoadIdentityResolver.Solve(corridor,probe,anchors,boundaries);
        Check(split[2].Identities.Length==0,"No bracketing across identity reset");
        var conflict=RoadIdentityResolver.Solve(corridor,probe,[Anchor("n",0,"I-5","NORTH"),Anchor("s",0,"I-5","SOUTH")],[]);
        Check(conflict[0].Status=="CONFLICT"&&conflict[0].Identities.Length==0,"Opposite official directions stay conflicting");
        var aliases=RoadIdentityResolver.Solve(corridor,probe,[Anchor("five",0,"I-5",confidence:"HIGH"),Anchor("ten",0,"I-10",confidence:"HIGH")],[]);
        Check(aliases[0].Identities.Length==2,"Independently verified concurrency retained");
        var competing=RoadIdentityResolver.Solve(corridor,probe,[Anchor("five",0),Anchor("ten",0,"I-10")],[]);
        Check(competing[0].Status=="AMBIGUOUS","Two weak owners cannot establish concurrency");
        Check(RoadNetworkIndex.Associate([new("roadA",5,0,""),new("roadB",7,0,"")]).Status=="AMBIGUOUS_ROADS","Parallel roads compared before GPS restriction");
        Check(RoadNetworkIndex.Associate([new("roadB",1,0,""),new("gpsRoad",35,0,"")]).Owner=="roadB","Closest non-GPS road not replaced by GPS preference");
        Check(!RoadIdentityRules.ReferenceOnly("TO I-5")&&!RoadIdentityRules.ReferenceOnly("JCT I-5"),"References not confirmation");
        Check(RoadIdentityRules.Valid("StreetName","Main Street")&&!RoadIdentityRules.Reference("Main Street"),"Street name separate category");
        Check(SignTextClassifier.ExplicitCardinals("North Las Vegas").Count==0,"City compass word not cardinal");
        Check(RoadNetworkIndex.Project([new(0,0,0),new(100,0,0)],new(50,15,0)).Height==15,"Height diagnostic available without rejecting suspended sign");
        var liveEdges=edges.Select(e=>new LiveEdge(e.SegmentIndex,e.ItemUid.ToString("X16"),new(e.Start.X,0,0),new(e.End.X,0,0),e.StartStationM,e.EndStationM,e.StartNavStationM,e.EndNavStationM)).ToArray();
        var action=new LiveAction("action",0,1,1500,"KEEP_LEFT","branch","HIGH",[new("action","KEEP_LEFT","HIGH","PPD",liveEdges[1].ItemUid,"branch","exact")]);
        var route=new LiveRoute("r",liveEdges,[action]);
        route=route with{RoadIdentities=new(1,"test-map",anchors,spans,RoadIdentityResolver.Contexts(route,spans,[]))};
        var message=new LiveMessage(2,"RouteSnapshot","test",1,0,1,route);
        Check(LiveProtocol.Decode(LiveProtocol.Encode(message)).Route!.RoadIdentities!.Spans.Length==4,"v2 roundtrip");
        Reject(()=>LiveProtocol.Validate(message with{Version=1}),"v1 cannot silently carry v2 data");
        Reject(()=>LiveProtocol.Validate(message with{Route=route with{RoadIdentities=route.RoadIdentities! with{Spans=spans.Take(3).ToArray()}}}),"Missing segment rejected");
        var forged=spans.Select((s,i)=>i==0?s with{Identities=[s.Identities[0] with{IdentityConfidence="HIGH"}]}:s).ToArray();
        Reject(()=>LiveProtocol.Validate(message with{Route=route with{RoadIdentities=route.RoadIdentities! with{Spans=forged}}}),"Forged HIGH on MEDIUM anchors rejected");
        Reject(()=>LiveProtocol.Validate(message with{Route=route with{RoadIdentities=route.RoadIdentities! with{Contexts=[new("action","wrong",[],[],[],null,"")]}}}),"Wrong outgoing branch rejected");
        var legacy=route with{RoadIdentities=null};
        Check(LiveProtocol.Decode(LiveProtocol.Encode(message with{Version=1,Route=legacy})).Route!.RoadIdentities is null,"Old recording remains unnamed without invention");
        LiveGuidance Replay()
        {
            var engine=new LiveGuidanceEngine();engine.Accept(new(2,"Hello","test",0,0,0));engine.Accept(message);
            return engine.Accept(new(2,"VehicleState","test",2,10,1,Vehicle:new(new(50,0,0),0,10,false)));
        }
        var guidance=Replay();
        Check(guidance.Road=="I-5"&&guidance.CurrentRoad!.Labels.Single().Identity=="I-5"&&guidance.TargetRoad!.Labels.Single().Confidence=="MEDIUM","Identity shown independently of sign fields");
        Check(guidance.Destination is null&&guidance.Cardinal is null,"Route name cannot invent direction/destination");
        Check(JsonSerializer.Serialize(Replay(),LiveProtocol.Json)==JsonSerializer.Serialize(guidance,LiveProtocol.Json),"Identity replay deterministic");
        var shared=spans.Select(s=>s.SegmentIndex==2?s with{Identities=[
            new("I-5","RouteReference","","MEDIUM","NONE",["a"],"shared"),
            new("I-10","RouteReference","","MEDIUM","NONE",["b"],"shared")]}:s).ToArray();
        var futureContext=RoadIdentityResolver.Contexts(route,shared,[]).Single();
        Check(futureContext.PreferredIdentity=="I-5"&&futureContext.TargetSpanIds.Single()=="segment:2","Future route differentiates concurrency without replacing immediate segment");
        var stopped=RoadIdentityResolver.Contexts(route,shared,[new(1,2,"TURN_RIGHT","TURN_RIGHT",250,2500,"",false,"HIGH")]).Single();
        Check(stopped.PreferredIdentity is null,"Do not borrow identity after a later turn");
        var rotated=RoadNetworkIndex.Project([new(0,0,0),new(0,0,100)],new(5,0,50));
        var reflected=RoadNetworkIndex.Project([new(0,0,0),new(-100,0,0)],new(-50,0,5));
        Check(Math.Abs(rotated.Distance-5)<1e-6&&Math.Abs(reflected.Distance-5)<1e-6,"Rotation and reflection keep independent distance result");
        var engineReset=new LiveGuidanceEngine();engineReset.Accept(new(2,"Hello","test",0,0,0));engineReset.Accept(message);
        engineReset.Accept(new(2,"VehicleState","test",2,10,1,Vehicle:new(new(50,0,0),0,10,false)));
        var clear=engineReset.Accept(new(2,"RouteCleared","test",3,20,2));
        Check(clear.CurrentRoad is null&&clear.TargetRoad is null&&clear.Road is null,"Cleared route removes all identities");
        Reject(()=>LiveProtocol.Validate(message with{Route=route with{RoadIdentities=route.RoadIdentities! with{Anchors=[null!]}}}),"Null anchor rejected as protocol error");
        Reject(()=>LiveProtocol.Validate(message with{Route=route with{RoadIdentities=route.RoadIdentities! with{Anchors=[anchors[0] with{OwnerUid="wrong"},anchors[1]]}}}),"Owner must match occurrence UID");
        Reject(()=>LiveProtocol.Validate(message with{Route=route with{RoadIdentities=route.RoadIdentities! with{Spans=spans.Select((s,i)=>i==1?s with{EntryNodeUid="0000000000000BAD"}:s).ToArray()}}}),"Directed spans must share native nodes");
        Reject(()=>LiveProtocol.Validate(message with{Route=route with{RoadIdentities=route.RoadIdentities! with{Spans=spans.Select((s,i)=>i==0?s with{EntryNodeUid="ZZZZZZZZZZZZZZZZ"}:s).ToArray()}}}),"Non-hex native node rejected");
        Console.WriteLine($"PASS: {count} road identity assertions.");
    }
}
