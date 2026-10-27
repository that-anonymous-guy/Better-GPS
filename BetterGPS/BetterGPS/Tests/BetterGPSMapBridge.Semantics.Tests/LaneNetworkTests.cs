using System.Numerics;
using BetterGPSNavigation;

internal static class LaneNetworkTests
{
    public static void Run()
    {
        int count=0;void Check(bool b,string m){count++;if(!b)throw new Exception(m);}
        Check(ManeuverFeatureExtractor.MergeSideFromEndpoints([new(6.75f,0,-100)],[new(2.25f,0,-100),new(-2.25f,0,-100),new(-6.75f,0,-100)],new(0,0,-1))=="LEFT","Actual right ramp classified from bend angle");
        Check(ManeuverFeatureExtractor.MergeSideFromEndpoints([new(-6.75f,0,-100)],[new(2.25f,0,-100)],new(0,0,-1))=="RIGHT","Left ramp side reversed");
        Check(ManeuverFeatureExtractor.MergeSideFromEndpoints([new(0,0,-100)],[new(0,0,-100)],new(0,0,-1))=="","Shared output lane used to invent side");
        Dictionary<string,object?> A(int l,int r)=>new(){["lanes_left"]=Enumerable.Repeat((object)"lane",l).ToArray(),["lanes_right"]=Enumerable.Repeat((object)"lane",r).ToArray()};
        RoadLaneLook L(Dictionary<string,object?> a)=>RoadLaneGeometryResolver.Parse("test","fixture",a);
        var look=L(A(1,1));Check(look.Left.SequenceEqual(new[]{-2.25f})&&look.Right.SequenceEqual(new[]{2.25f}),"Two-way centers");
        Check(L(A(0,2)).Right.SequenceEqual(new[]{-2.25f,2.25f}),"Right one-way");
        Check(L(A(2,0)).Left.SequenceEqual(new[]{2.25f,-2.25f}),"Left one-way");
        Check(L(A(0,3)).Right.SequenceEqual(new[]{-2.25f,2.25f,6.75f}),"ETS2LA odd one-way convention");
        Check(L(A(0,0)).Blockers.Contains("NO_ROAD_LANES"),"Empty lanes");
        var attrs=A(1,1);attrs["road_offset"]=3f;attrs["lane_offsets_left"]=new object[]{new Vector2(1,0)};attrs["lane_offsets_right"]=new object[]{(2,0)};
        look=L(attrs);Check(look.Left[0]==-6.25&&look.Right[0]==7.25,"Per-side and individual offsets");
        attrs["lane_offsets_left"]=new object[]{((object)(-1),(object)0)};
        attrs["lane_offsets_right"]=new object[]{((object)2,(object)0)};
        look=L(attrs);Check(look.Left[0]==-4.25&&look.Right[0]==7.25&&!look.Blockers.Contains("LANE_OFFSET_TYPE_UNSUPPORTED"),
            "Actual TruckLib SII boxed tuple lane offsets decode with their signed value");
        attrs["road_offset"]=float.NaN;Check(L(attrs).Blockers.Contains("NON_FINITE_LANE_OFFSET"),"Nonfinite offset accepted");
        attrs=A(0,2);attrs["lane_offsets_right"]=new object[]{new Vector2(1,0)};Check(L(attrs).Blockers.Contains("LANE_OFFSET_COUNT_MISMATCH"),"Mismatched offset count");
        attrs["lane_offsets_right"]=new object[]{"bad","bad"};Check(L(attrs).Blockers.Contains("LANE_OFFSET_TYPE_UNSUPPORTED"),"Unknown offset type");
        // Native SCS tangent is -Z. The former +Z sampler travelled backwards at each endpoint.
        var points=DecisionBranchResolver.SampleNativeCurve(new(0,0,0),Quaternion.Identity,new(0,0,-100),Quaternion.Identity);
        Check(points[1].Z<0&&points[^2].Z>-100&&points[0].Z==0&&points[^1].Z==-100,"Native PPD tangent or endpoints incorrect");
        RouteLanePiece P(string id,int seg,float x,float z,string branch="",bool selected=true)=>new(id,seg,branch==""?"ROAD_LOOK_SAMPLED":"PPD_SAMPLED",id+":in",id+":out",branch,selected,0,"","",[new(x,0,z),new(x,0,z+10)],[]);
        var road=P("road",0,0,0);var entry=P("entry",1,0,10,"gps");
        var match=LaneRouteGraph.Match([road],[entry]);Check(match.Single().Status=="UNIQUE_GEOMETRIC_CANDIDATE","Unique boundary rejected");
        Check(LaneRouteGraph.Match([road],[entry,entry with{Id="dup",StartPort="dup:in"}]).All(m=>m.Status=="AMBIGUOUS_BOUNDARY"),"Ambiguous boundary silently chosen");
        Check(LaneRouteGraph.Match([road],[entry with{Points=[new(0,3,10),new(0,3,20)]}]).Single().Status=="REJECTED_HEIGHT","Stacked roads linked");
        Check(LaneRouteGraph.Match([road],[entry with{Points=[new(0,0,10),new(0,0,0)]}]).Single().Status=="REJECTED_DIRECTION","Reverse traffic linked");
        Check(LaneRouteGraph.Match([road],[entry with{Points=[new(float.NaN,0,10),new(0,0,20)]}]).Count==0,"NaN boundary linked");
        DecisionBranchSet Set(int segment,string selected)=>new(-1,segment,0,"",0,1,2,[0],false,
            [new(selected,1,true,[0],[],[],0,"STRAIGHT",[],0,0,""),new("other"+segment,2,false,[1],[],[],0,"STRAIGHT",[],0,0,"")],selected,"OK",null);
        var nextRoad=P("next",2,0,20);var future=P("future",3,0,30,"futureGps");
        var pieces=new[]{road,entry,nextRoad,future};var boundaries=LaneRouteGraph.Match([road],[entry]).Concat(LaneRouteGraph.Match([entry],[nextRoad])).Concat(LaneRouteGraph.Match([nextRoad],[future])).ToArray();
        var sets=new Dictionary<int,DecisionBranchSet>{{1,Set(1,"gps")},{3,Set(3,"futureGps")}};
        var graph=new LaneRouteGraph(pieces,boundaries,sets);var chain=graph.Trace("road",1);
        Check(chain.Status=="GPS_CONTINUATION_CANDIDATE"&&chain.FutureSelectedBranches.SequenceEqual(new[]{"futureGps"}),"Future branch was not tracked");
        Check(chain.CurrentExits.SequenceEqual(new[]{"gps"})&&chain.PieceIds.Count==4,"Current exit or chain provenance lost");
        var blocked=new LaneRouteGraph(pieces,boundaries.Where(b=>b.FromPort!=nextRoad.EndPort).ToArray(),sets).Trace("road",1);
        Check(blocked.Status=="CHAIN_BLOCKED"&&blocked.Blockers.Contains("BOUNDARY_NOT_MATCHED"),"Missing boundary fabricated");
        Check(graph.Trace("road",1,1).Status=="LOOKAHEAD_LIMIT","Lookahead cap ignored");
        var wrong=future with{Selected=false,BranchId="wrong"};
        Check(new LaneRouteGraph([road,entry,nextRoad,wrong],boundaries,sets).Trace("road",1).Status=="CHAIN_BLOCKED","Wrong future exit accepted");
        var badRoad=road with{Blockers=["LANE_COUNT_TRANSITION_UNRESOLVED"]};
        Check(new LaneRouteGraph([badRoad,entry,nextRoad,future],boundaries,sets).Trace("road",1).Status=="CHAIN_BLOCKED","Lane-count transition invented");
        var lanes=new[]{new AuditLane(0,"ROAD_LOOK_SAMPLED",road.Points,["gps"]){LaneId="road",Chain=chain},
            new AuditLane(0,"ROAD_LOOK_SAMPLED",road.Points,["gps"]){LaneId="another",Chain=chain}};
        var link=PanelLaneLinker.Link(new(0,5,5),lanes,"gps",["UNCALIBRATED"]);
        Check(link.Hits.Count==2,"Different road lane IDs collapsed");
        var group=new SignSemanticGroup("g","COMPLETE_CANDIDATE","test","I-5","NORTH","Sacramento",[1],[1],[],[]);
        var assignment=GroupLaneAssigner.Resolve(group,[new(1,"left",link)]);
        Check(assignment.Status=="GPS_CONTINUATION_CANDIDATE"&&assignment.Confidence=="LOW"&&assignment.Blockers.Contains("SIGN_OWNERSHIP_NOT_CALIBRATED"),"Group promoted without sign calibration");
        Check(GroupLaneAssigner.Resolve(group,[new(2,"wrongArrow",link)]).Status=="NO_ARROW_GEOMETRY","Arrow crossed semantic groups");
        var opposite=link with{Hits=link.Hits.Select(h=>h with{Chain=chain with{Status="OTHER_EXIT_ONLY"}}).ToArray()};
        Check(GroupLaneAssigner.Resolve(group,[new(1,"left",link),new(1,"center",opposite)]).Status=="AMBIGUOUS_PLACEMENT","Contradictory placement selected by GPS");
        Console.WriteLine($"PASS: {count} road lane, boundary, directed lookahead and group assignment regressions.");
    }
}
