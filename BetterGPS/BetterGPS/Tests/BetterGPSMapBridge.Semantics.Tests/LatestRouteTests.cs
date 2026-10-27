using System.Text.Json;
using BetterGPSNavigation;

internal static class LatestRouteTests
{
    internal static void Run()
    {
        int count=0;void Check(bool ok,string why){count++;if(!ok)throw new Exception(why);}
        Check(RoadOverlayResolver.NormalizeRoadToken("ca_r180")=="CA-180","Native state-highway overlay ignored");
        Check(RoadOverlayResolver.NormalizeRoadToken("ca_r99")=="CA-99","California highway overlay ignored");
        Check(RoadOverlayResolver.NormalizeRoadToken("nv_r160")=="NV-160","Other state route overlay ignored");
        Check(RoadOverlayResolver.NormalizeRoadToken("xx_r180")=="","Unknown state prefix invented");
        foreach(var (raw,expected) in new[]{("bus25","I-25-BUS"),("bus40","I-40-BUS"),("bus80","I-80-BUS"),
            ("bus99","I-99-BUS"),("txl323","TX-LOOP-323"),("txl338","TX-LOOP-338"),("txl999","TX-LOOP-999"),
            ("us93alt","US-93-ALT"),("us42alt","US-42-ALT"),("ca_r267","CA-267"),("mt200s","MT-200S"),("is35e","I-35E")})
        {
            Check(RoadOverlayResolver.NormalizeRoadToken(raw)==expected,"Native overlay family failed: "+raw);
            Check(SignTextClassifier.IsRoadIdentity(expected),"Decoded overlay identity rejected: "+raw);
        }
        foreach(var raw in new[]{"toll_ico","agri_check","border_ico","bus_stop","txl","xx_r12","usalt","business_district"})
            Check(RoadOverlayResolver.NormalizeRoadToken(raw)=="","Non-road overlay invented highway: "+raw);
        var panels=JsonSerializer.Deserialize<ExtractedSignPanel[]>(File.ReadAllText(
            "validation/ingame-navigation/20261011_171033_bd01bd82/live_000007_B66C58C3B6EB8EB2_panels.json"))!;
        var plate=panels.Single(p=>p.SignUid=="5109D9FD701B0001"&&p.AreaName=="board1");
        var streetPlates=panels.Where(p=>p.BoardPath=="/def/sign/board/info/hwy_info_1a.sii").ToArray();
        var avenue=streetPlates.Single(p=>p.SignUid=="5109BDD8FB1B0001");
        Check(StreetNameLinker.IsStreetPlate(avenue,"")&&StreetNameLinker.Names(avenue).SequenceEqual(new[]{"Avenue 12"}),"Real generic street plate ignored");
        var numbered=streetPlates.Single(p=>p.SignUid=="5109BF72AC1B0000");
        Check(StreetNameLinker.IsStreetPlate(numbered,"")&&StreetNameLinker.Names(numbered).SequenceEqual(new[]{"Route 41"}),"Literal local road name lost");
        Check(!StreetNameLinker.IsStreetPlate(avenue with{BoardPath="/def/sign/board/info/info_entering_city.sii"},""),"City board promoted to street plate");
        var clovis=streetPlates.Single(p=>p.SignUid=="50F5E74BA51B0000");
        Check(StreetNameLinker.IsStreetPlate(clovis,"")&&StreetNameLinker.Names(clovis).SequenceEqual(new[]{"Clovis"}),"Bare street plate lost");
        var ninth=streetPlates.Single(p=>p.SignUid=="507B6EF2E19B0000");
        Check(StreetNameLinker.Names(ninth).SequenceEqual(new[]{"9th St"}),"Superscript street ordinal lost");
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/small_arrow_12.sii","9145A494FD7A1DBCCB5B552F157496B9E6CEA132B81F533C308947130F4FED3D",0,false,false)=="LEFT","Inspected route arrow reversed");
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/small_arrow_12.sii","9145A494FD7A1DBCCB5B552F157496B9E6CEA132B81F533C308947130F4FED3D",0,true,false)=="RIGHT","Route arrow ignored effective horizontal flip");
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/small_arrow_12.sii","modified",0,false,false)=="","Modified unverified route arrow accepted");
        var shield=panels.Single(p=>p.SignUid=="50EC361028180001"&&p.BoardPath=="/def/sign/board/shield/ca_hw_shield_1x.sii");
        var shieldObjects=shield.Objects.ToList();SignPanelExtractor.DecodeTextShield(shield.BoardPath,shieldObjects);
        Check(shieldObjects.Single().Road=="CA-99","Number on audited California shield not decoded");
        var speed=shield.Objects.ToList();SignPanelExtractor.DecodeTextShield("/def/sign/board/traffic/us_speed_limit_30x36.sii",speed);
        Check(speed.All(o=>o.Road.Length==0),"Numeric speed sign promoted to route");
        var unreliable=shield.Objects.Select(o=>o with{Reliable=false}).ToList();SignPanelExtractor.DecodeTextShield(shield.BoardPath,unreliable);
        Check(unreliable.All(o=>o.Road.Length==0),"Unreliable numeric shield promoted to route");
        var groups=plate.SemanticGroups;
        Check(groups.Count==2,"Recorded CA41 direction plate did not split");
        Check(groups.All(g=>g.Road=="CA-41"&&g.Method=="SHARED_SHIELD_DIRECTION_ROWS"),"Shared shield lost");
        string Arrow(SignSemanticGroup g)=>plate.Objects.Single(o=>g.ArrowObjectIds.Contains(o.Id)).Arrow;
        Check(Arrow(groups.Single(g=>g.Cardinal=="NORTH"))=="LEFT","North acquired south arrow");
        Check(Arrow(groups.Single(g=>g.Cardinal=="SOUTH"))=="RIGHT","South acquired north arrow");
        var broken=plate with{Objects=plate.Objects.Select(o=>o.Road.Length>0?o with{Reliable=false}:o).ToArray()};
        Check(broken.SemanticGroups.All(g=>g.Method!="SHARED_SHIELD_DIRECTION_ROWS"),"Unreliable shared shield accepted");
        var wrongRow=plate with{Objects=plate.Objects.Select(o=>o.Arrow=="RIGHT"?o with{Effective=new Dictionary<string,string>(o.Effective){["coords_b"]="1.2",["coords_t"]="1.6"}}:o).ToArray()};
        Check(wrongRow.SemanticGroups.All(g=>g.Method!="SHARED_SHIELD_DIRECTION_ROWS"),"Ambiguous direction rows split");
        NetworkPortion P(int item,int entry,int exit,string kind,float[][][] paths)=>new(DirectedRoadNetworkGraph.Id($"{item:X16}",$"{entry:X16}",$"{exit:X16}"),$"{item:X16}",kind,$"{entry:X16}",$"{exit:X16}",paths,[]);
        var road=P(1,10,11,"ROAD",[[[-2,0,-30],[-2,0,-15],[-2,0,0]],[[2,0,-30],[2,0,-15],[2,0,0]]]);
        var left=P(2,11,12,"PREFAB",[[[-2,0,0],[-2,0,3],[-10,0,10]]]);
        var right=P(2,11,13,"PREFAB",[[[2,0,0],[2,0,3],[10,0,10]]]);
        var outRoad=P(3,12,14,"ROAD",[[[-10,0,10],[-20,0,10]]]);
        var graph=new DirectedRoadNetworkGraph(new(-100,100,-100,100),[road,left,right,outRoad],
            [new(road.Id,left.Id,"NATIVE_PORT"),new(road.Id,right.Id,"NATIVE_PORT"),new(left.Id,outRoad.Id,"NATIVE_PORT")],[]);
        var beforeCurve=P(4,15,16,"ROAD",[[[0,0,0],[0,0,10]]]);
        var behindCurve=P(5,17,18,"ROAD",[[[-10,0,-90],[0,0,-80]]]);
        var loopGraph=new DirectedRoadNetworkGraph(new(-100,100,-100,100),[beforeCurve,behindCurve],[],[]);
        Check(!NamedRoadNetwork.BracketDirectionCompatible(loopGraph,beforeCurve.Id,behindCurve.Id),"Curved return loop accepted as road confirmation");
        var f1=P(70,70,71,"ROAD",[[[-2,0,0],[-2,0,10]],[[2,0,0],[2,0,10]]]);
        var f2=P(71,71,72,"ROAD",[[[-2,0,10],[-2,0,20]],[[2,0,10],[2,0,20]]]);
        var r1=P(170,75,76,"ROAD",[[[5,0,20],[5,0,10]],[[9,0,20],[9,0,10]]]);
        var r2=P(171,76,77,"ROAD",[[[5,0,10],[5,0,0]],[[9,0,10],[9,0,0]]]);
        var divided= new DirectedRoadNetworkGraph(new(-100,100,-100,100),[f1,f2,r1,r2],
            [new(f1.Id,f2.Id,"NATIVE_PORT"),new(r1.Id,r2.Id,"NATIVE_PORT")],[]);
        NetworkNameSource Mark(string id,string[] owners)=>new(id,id,"ROAD_OVERLAY","MapAnnotation","ca_r180","CA-180","RouteReference","","",
            [3,0,5],null,"UI_ANNOTATION","MEDIUM","AMBIGUOUS_OWNERS",owners,[]);
        var m1=Mark("map1",[f1.Id,r2.Id]);var m2=Mark("map2",[f2.Id,r1.Id]);
        Check(NamedRoadNetwork.CorroboratedMapOverlay(divided,m1,[m1,m2]),"Repeated native map labels failed to corroborate both carriageways");
        Check(!NamedRoadNetwork.CorroboratedMapOverlay(divided,m1,[m1]),"Single ambiguous map label became road ownership");
        Check(!NamedRoadNetwork.CorroboratedMapOverlay(divided,m1,[m1,m2 with{ObjectId=m1.ObjectId}]),"Same map annotation counted twice");
        Check(!NamedRoadNetwork.CorroboratedMapOverlay(divided,m1,[m1,m2 with{Identity="CA-99"}]),"Different highway label supplied corroboration");
        Check(!NamedRoadNetwork.CorroboratedMapOverlay(divided,m1,[m1,m2 with{Owners=[f2.Id]}]),"Uncovered opposing carriageway inherited map label");
        Check(NamedRoadNetwork.Solve(divided,[m1,m2]).All(n=>n.Identities.Single() is{Identity:"CA-180",Cardinal:"",CardinalConfidence:"NONE"}),"Map corroboration borrowed a direction");
        NetworkNameSource S(string id,string raw="CA-41")=>new(id,id,"SIGN_GROUP","GuideTo",raw,"CA-41","RouteReference","NORTH","",[-2,5,-20],[0,0,-1],"MODEL_FACE_CANDIDATE_NOT_RENDER_VERIFIED","MEDIUM","",[],[]);
        var lane=S("lane") with{Arrow="DOWN_LANE",LanePoints=[[-2,5,-20],[-2,6,-20]]};
        var linked=GuideSignLinker.Associate(graph,lane);
        Check(linked.Owners.SequenceEqual(new[]{left.Id}),"Down arrow failed to carry physical lane to fork");
        Check(!linked.Owners.Contains(road.Id),"Exit road name assigned to approach");
        Check(GuideSignLinker.Associate(graph,lane with{LanePoints=[[-2,5,-20],[2,5,-20]]}).Owners.Length==0,"Mirrored placement disagreement accepted");
        Check(GuideSignLinker.Associate(graph,lane with{LanePoints=[]}).Owners.Length==0,"Down arrow without placement guessed");
        Check(GuideSignLinker.Associate(graph,lane with{Normal=[0,0,1]}).Owners.Length==0,"Reverse-facing arrow accepted");
        var guide=S("exit","CA-41 | EXIT 166") with{Position=[-2,5,-5],Arrow="LEFT",AssociationStatus="DIRECTIONAL_BRANCH_TARGET",Owners=[left.Id]};
        var notice=S("advance","CA-41 | EXIT 166 | JCT 1 MILE");
        var notices=AdvanceExitLinker.Link(graph,[notice,guide]);
        Check(notices[0].AssociationStatus=="ADVANCE_EXIT_BRANCH_TARGET"&&notices[0].Owners.SequenceEqual(guide.Owners),"Numbered advance notice lost");
        Check(AdvanceExitLinker.Valid(graph,notices[0],notices.ToDictionary(s=>s.Id)),"Advance provenance failed");
        Check(AdvanceExitLinker.Link(graph,[notice with{Raw="CA-41 | EXIT 167"},guide])[0].Owners.Length==0,"Wrong exit number linked");
        Check(AdvanceExitLinker.Link(graph,[notice with{Position=[-2,5,20]},guide])[0].Owners.Length==0,"Past exit notice linked backwards");
        Check(NamedRoadNetwork.Solve(graph,[notices[0],guide]).All(n=>n.Identities.Length==0),"Advance exit renamed current road");
        var street=guide with{Identity="Washington Street",Kind="StreetName",Raw="Washington Street"};
        Check(NamedRoadNetwork.LocalTurnScope(graph,street).Contains(outRoad.Id),"Signed street lost immediate outgoing road");
        Check(NamedRoadNetwork.LocalTurnScope(graph,street with{Raw="TO Washington Street"}).Length==0,"TO guide became current street");
        var shared=guide with{Raw="CA-41 | NORTH | SOUTH",GroupMethod="SHARED_SHIELD_DIRECTION_ROWS"};
        Check(NamedRoadNetwork.SharedRouteArmScope(graph,shared).Contains(outRoad.Id),"Shared shield failed to name its physical road arm");
        Check(!NamedRoadNetwork.SharedRouteArmScope(graph,shared).Contains(road.Id),"Shared shield renamed perpendicular approach");
        Check(NamedRoadNetwork.SharedRouteArmScope(graph,guide).Length==0,"Ordinary guide treated as shared shield");
        var recorded=JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(
            "validation/latest-route-fix/replay-v6/live_000007_B66C58C3B6EB8EB2_named-network.json"),LiveProtocol.Json)!;
        var recordedGraph=new DirectedRoadNetworkGraph(recorded.Region,recorded.Portions,recorded.Connections,recorded.Errors);
        var compositePlate=recorded.Sources.Single(s=>s.Id=="507B87289E1B0000:board1:D St");
        var compositeLinked=NamedRoadNetwork.Reassociate(recordedGraph,compositePlate);
        Check(compositeLinked.Role=="StreetAxis"&&compositeLinked.Owners.Length>0,"Native-connected prefab intersection rejected as competing junctions");
        var bare=compositeLinked with{Identity="Clovis",Raw="Clovis"};
        Check(!NamedRoadNetwork.StreetPlateConfirmed(recordedGraph,bare,[bare]),"Single generic place label became street name");
        var second=bare with{Id="independent",ObjectId="independent"};
        Check(NamedRoadNetwork.StreetPlateConfirmed(recordedGraph,bare,[bare,second]),"Independent same-axis street plates rejected");
        Check(!NamedRoadNetwork.StreetPlateConfirmed(recordedGraph,bare,[bare,second with{ObjectId=bare.ObjectId}]),"Same sign counted as independent bare street plate");
        Check(!NamedRoadNetwork.StreetPlateConfirmed(recordedGraph,bare,[bare,second with{Owners=[]}]),"Nearby unrelated street plate accepted");
        Console.WriteLine($"PASS: {count} latest-route regression assertions.");
    }
}
