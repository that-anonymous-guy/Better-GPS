using System.Numerics;
using System.Text.Json;
using BetterGPSNavigation;

internal static class NamedNetworkTests
{
    public static void RealSupport(string game,string docs,string panelsPath,string output)
    {
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."});
        var panel=JsonSerializer.Deserialize<ExtractedSignPanel[]>(File.ReadAllText(panelsPath))!.First(p=>p.BoardPath=="/def/sign/board/shield/ca_fw_single_b.sii");
        var resolver=new SupportSignFaceResolver(assets.Loader,new BoardModelResolver(assets.Loader));
        var sign=new ParsedSign(1,1,Vector3.Zero,Quaternion.Identity,0,"2k09h","","","","","/model/sign/navigation/fwy_shield_single_b.pmd",[],[],[],[],[],[],[],[],false,false,false);
        var face=resolver.Resolve(sign,panel)??throw new Exception("Actual support shield face unresolved");
        // Expected normal from the independent raw mesh inspection; the text locator rotates 180 degrees and MUST NOT flip this support-space normal.
        if(Vector3.Distance(face.Normal,Vector3.UnitZ)>.001||Math.Abs(face.Position.Z-.03284984)>.001)throw new Exception("Support face differs from independently inspected mesh");
        var reversed=resolver.Resolve(sign with{Rotation=Quaternion.CreateFromAxisAngle(Vector3.UnitY,MathF.PI)},panel)!;
        if(Vector3.Distance(reversed.Normal,-Vector3.UnitZ)>.001)throw new Exception("World-facing normal inverted");
        Directory.CreateDirectory(output);assets.WriteManifest(Path.Combine(output,"support-face-assets.json"));
        File.WriteAllText(Path.Combine(output,"support-face-check.json"),JsonSerializer.Serialize(new{face,reversed,nativeMeshNormalMatches=true,locatorRotationNotDoubleApplied=true,renderCalibration=false},new JsonSerializerOptions(LiveProtocol.Json){IncludeFields=true}));
        Console.WriteLine("PASS: 3 actual support-mesh/rotation checks against independent raw model inspection; still no render calibration.");
    }
    public static void Run()
    {
        int count=0;void Check(bool ok,string reason){count++;if(!ok)throw new Exception(reason);}
        string U(int i)=>i.ToString("X16");
        NetworkPortion P(int uid,int entry,int exit,float[] a,float[] b,string kind="ROAD")=>new(
            DirectedRoadNetworkGraph.Id(U(uid),U(entry),U(exit)),U(uid),kind,U(entry),U(exit),[[a,b]],[]);
        var a=P(101,1,2,[0,0,0],[100,0,0]);var b=P(102,2,3,[100,0,0],[100,0,100],"PREFAB");
        var c=P(103,3,4,[100,0,100],[200,0,100]);var reverse=P(101,2,1,[100,0,0],[0,0,0]);
        var other=P(104,2,5,[100,0,0],[100,0,-100],"PREFAB");
        var region=new NetworkRegion(-500,500,-500,500);
        DirectedRoadNetworkGraph Graph(NetworkPortion[] p,NetworkConnection[] links)=>new(region,p,links,[]);
        var graph=Graph([a,b,c,reverse,other],[new(a.Id,b.Id,"NATIVE_PORT"),new(b.Id,c.Id,"NATIVE_PORT"),new(a.Id,other.Id,"NATIVE_PORT")]);
        NetworkNameSource S(string id,string owner,string label="I-5",string role="MapAnnotation",string cardinal="",string? objectId=null)=>
            new(id,objectId??id,"ROAD_OVERLAY",role,label,label,"RouteReference",cardinal,"",[50,0,0],null,"UI_ANNOTATION","MEDIUM",
                "UNIQUE_PHYSICAL_ROAD_BOTH_SENSES",[owner],[new(owner,0,0,null,"CANDIDATE")]);
        var anchors=new[]{S("first",a.Id),S("last",c.Id)};
        var signedBrackets=anchors.Select(s=>s with{Role="GuideTo",AssociationStatus="DIRECTIONAL_BRANCH_TARGET"}).ToArray();
        Check(NamedRoadNetwork.Solve(graph,signedBrackets).All(n=>n.Identities.Length==0),
            "Directional guides cannot confirm the incoming corridor of a downstream branch");
        Check(NamedRoadNetwork.Solve(graph,[anchors[0],signedBrackets[1]]).Single(n=>n.PortionId==b.Id).Identities.Length==0,
            "Own-road confirmation plus downstream destination must not rename the approach");
        Check(NamedRoadNetwork.Solve(graph,signedBrackets.Select(s=>s with{Raw="TO I-5"}).ToArray()).All(n=>n.Identities.Length==0),
            "TO signs cannot name an intervening access road");
        Check(NamedRoadNetwork.Solve(graph,[signedBrackets[0] with{Cardinal="NORTH"},signedBrackets[1] with{Cardinal="SOUTH"}]).All(n=>n.Identities.Length==0),
            "Opposite signed directions cannot bracket one named corridor");
        var west=P(201,11,12,[-100,0,0],[-10,0,0]);
        var east=P(202,13,14,[10,0,0],[100,0,0]);
        var north=P(203,15,16,[0,0,10],[0,0,100]);
        var straight=P(204,12,13,[-10,0,0],[10,0,0],"PREFAB");
        var turn=P(204,12,15,[-10,0,0],[0,0,10],"PREFAB");
        var streetGraph=Graph([west,east,north,straight,turn],[]);
        var streetPlate=S("street",west.Id) with{Role="CrossStreet",Source="STREET_NAME_SIGN",Identity="Washington Street",Kind="StreetName",
            Position=[0,5,0],Normal=[0,0,1],Owners=[]};
        var linked=NamedRoadNetwork.Reassociate(streetGraph,streetPlate);
        Check(linked.Role=="StreetAxis"&&linked.Owners.Contains(west.Id)&&linked.Owners.Contains(east.Id),"Plate plane names both collinear street arms");
        Check(!linked.Owners.Contains(north.Id)&&!linked.Owners.Contains(turn.Id),"Street plate never names the perpendicular approach or turning curve");
        Check(NamedRoadNetwork.Reassociate(streetGraph,streetPlate with{Normal=null}).Owners.Length==0,"Unresolved street orientation cannot assign a name");
        var turned=NamedRoadNetwork.Reassociate(streetGraph,streetPlate with{Normal=[1,0,0]});
        Check(turned.Owners.Contains(north.Id)&&!turned.Owners.Contains(east.Id),"Rotating plate selects the perpendicular street");
        var connector=P(205,2,3,[100,0,0],[200,0,0],"PREFAB");
        var onward=P(206,3,4,[200,0,0],[300,0,0]);
        var chain=Graph([a,connector,onward],[new(a.Id,connector.Id,"NATIVE_PORT"),new(connector.Id,onward.Id,"NATIVE_PORT")]);
        Check(NamedRoadNetwork.Solve(chain,[S("one",a.Id)]).Single(n=>n.PortionId==onward.Id).Identities.Single().Rule=="UNBRANCHED_NETWORK_CONTINUITY",
            "One sign covers a topologically unbranched continuation");
        Check(!NamedRoadNetwork.UnbranchedContinuity(graph,a.Id).Contains(b.Id),"Continuation stops where a branch choice exists");
        var forkLeft=P(211,21,22,[0,0,0],[100,0,-10],"PREFAB");
        var forkRight=P(211,21,23,[0,0,0],[100,0,-2],"PREFAB");
        var curvedFork=Graph([forkLeft,forkRight],[]);
        var forkSign=S("fork",a.Id) with{Role="GuideTo",Position=[-20,5,0],Normal=[-1,0,0],Arrow="DIAGONAL_UP_RIGHT",Owners=[]};
        Check(NamedRoadNetwork.Reassociate(curvedFork,forkSign).Owners.SequenceEqual(new[]{forkRight.Id}),
            "Signed right fork remains right when both carriageways curve left");
        Check(NamedRoadNetwork.Reassociate(curvedFork,forkSign with{Arrow="DIAGONAL_UP_LEFT"}).Owners.SequenceEqual(new[]{forkLeft.Id}),
            "Relative fork sides are distinguished without borrowing the GPS branch");
        Check(NamedRoadNetwork.Reassociate(curvedFork,forkSign with{Position=[40,5,-5]}).Owners.SequenceEqual(new[]{forkRight.Id}),
            "Sign within a long prefab remains associated to its directional branch");
        Check(NamedRoadNetwork.Reassociate(curvedFork,forkSign with{Normal=[1,0,0]}).Owners.Length==0,
            "A sign facing opposite traffic cannot name a branch");
        EffectiveSignObject Text(uint id,string text,bool visible=true)=>new(id,"sign_template_text",visible,true,
            new Dictionary<string,string>(),new Dictionary<string,string>(),[],[],text,"","","","");
        var plate=new ExtractedSignPanel("1","ml1","","","OK","",new("",null,null,1,null),
            [Text(1,"CITY OF COMMERCE"),Text(2,"Washington"),Text(3,"BLVD"),Text(4,"XXXX",false)],[]);
        Check(StreetNameLinker.Names(plate).SequenceEqual(new[]{"Washington Blvd"}),"Visible name and suffix combine without city label or hidden placeholder");
        Check(StreetNameLinker.Names(plate with{Objects=[Text(1,"XXXX")]}).Length==0,"Default placeholder never becomes street name");
        var streetObjects=new[]{Text(40,"Central Ave") with{Effective=new Dictionary<string,string>{["coords_l"]="1",["coords_r"]="4",["coords_b"]="1",["coords_t"]="2"}},
            Text(41,"") with{Kind="sign_template_image",Arrow="RIGHT",Effective=new Dictionary<string,string>{["coords_l"]="4",["coords_r"]="5",["coords_b"]="0",["coords_t"]="1"}}};
        var streetGuide=plate with{Objects=streetObjects,WidthM=6,HeightM=3};
        var streetGroup=streetGuide.SemanticGroups.Single();
        Check(streetGroup is {Kind:"StreetName",Road:"Central Ave",Cardinal:"",Destination:"",Status:"COMPLETE_CANDIDATE"},
            "A street and its directional arrow form a route target without requiring a shield, city or cardinal");
        Check(!SignGroupResolver.IsSignedStreetName("Blvd"),"A suffix alone is not a street name");
        Check((streetGuide with{Objects=streetObjects.Append(Text(42,"Newhall")).ToArray()}).SemanticGroups.Single().Road.Length==0,
            "A possibly wrapped street inscription must not announce only its suffix line");
        Check(SignGroupGuidance.ValidateFields(streetGroup,streetGuide,_=>false).IdentityConfidence=="HIGH",
            "Visible supported street text supplies its own field evidence");
        Check(!SignGroupResolver.IsSignedStreetName("RIGHT LANE")&&!SignGroupResolver.IsSignedStreetName("North Las Vegas"),
            "Lane instructions and city names never become street targets");
        Check(streetGuide with{Objects=streetObjects.Append(Text(42,"Main St")).ToArray()} is {} twoStreets&&
            twoStreets.SemanticGroups.Single().Road.Length==0,"Two competing street inscriptions remain unresolved");
        Check((streetGuide with{Objects=streetObjects.Select(o=>o.Id==40?o with{Visible=false}:o).ToArray()}).SemanticGroups.All(g=>g.Road.Length==0),
            "Hidden street text cannot supply a target");
        Check(!SignTextClassifier.ExplicitCardinals("North Main St").Any(),"North in a street name is not an official route cardinal");
        var leftTurn=P(210,20,21,[0,0,-10],[10,0,0],"PREFAB") with{Paths=[[[0,0,-10],[0,0,-5],[0,0,0],[5,0,0],[10,0,0]]]};
        var rightTurn=P(210,20,22,[0,0,-10],[-10,0,0],"PREFAB") with{Paths=[[[0,0,-10],[0,0,-5],[0,0,0],[-5,0,0],[-10,0,0]]]};
        var guideGraph=Graph([leftTurn,rightTurn],[]);
        var guidePlate=S("guide",a.Id) with{Role="GuideTo",Position=[0,5,-25],Normal=[0,0,-1],Arrow="LEFT",Owners=[]};
        var guideLinked=NamedRoadNetwork.Reassociate(guideGraph,guidePlate);
        Check(guideLinked.Owners.SequenceEqual(new[]{leftTurn.Id})&&guideLinked.AssociationStatus=="DIRECTIONAL_BRANCH_TARGET",
            "Signed left arrow targets exact left branch from facing approach");
        Check(NamedRoadNetwork.Reassociate(guideGraph,guidePlate with{Normal=[0,0,1]}).Owners.Length==0,"Opposite-facing guide cannot supply target");
        Check(NamedRoadNetwork.Solve(guideGraph,[guideLinked]).All(n=>n.Identities.Length==0),"Guide-to route never renames ramp or current road");
        var approach1=P(300,10,30,[-20,0,-20],[0,0,0],"PREFAB");
        var approach2=P(301,11,30,[20,0,-20],[0,0,0],"PREFAB");
        var trunk=P(302,30,31,[0,0,0],[0,0,100]);
        var trunkNext=P(303,31,32,[0,0,100],[0,0,200]);
        var branchMerge=Graph([approach1,approach2,trunk,trunkNext],[new(approach1.Id,trunk.Id,"NATIVE_PORT"),
            new(approach2.Id,trunk.Id,"NATIVE_PORT"),new(trunk.Id,trunkNext.Id,"NATIVE_PORT")]);
        var g1=S("g1",approach1.Id,"CA-14","GuideTo","NORTH") with{AssociationStatus="DIRECTIONAL_BRANCH_TARGET"};
        var g2=S("g2",approach2.Id,"CA-14","GuideTo","NORTH") with{AssociationStatus="DIRECTIONAL_BRANCH_TARGET"};
        var branchNames=NamedRoadNetwork.Solve(branchMerge,[g1,g2]);
        var remoteStart=P(304,9,10,[-200,0,-20],[-20,0,-20]);
        var remoteEnd=P(305,32,33,[0,0,200],[100,0,200]);
        var remoteGraph=Graph([remoteStart,approach1,approach2,trunk,trunkNext,remoteEnd],
            [new(remoteStart.Id,approach1.Id,"NATIVE_PORT"),new(approach1.Id,trunk.Id,"NATIVE_PORT"),
             new(approach2.Id,trunk.Id,"NATIVE_PORT"),new(trunk.Id,trunkNext.Id,"NATIVE_PORT"),new(trunkNext.Id,remoteEnd.Id,"NATIVE_PORT")]);
        var localNames=NamedRoadNetwork.Solve(remoteGraph,[g1,g2,S("remote1",remoteStart.Id),S("remote2",remoteEnd.Id)]);
        Check(localNames.Single(n=>n.PortionId==trunk.Id).Identities.Single() is {Identity:"CA-14",Cardinal:"NORTH"},
            "Remote same-road endpoints cannot override independent local signed branch confirmations");
        Check(branchNames.Single(n=>n.PortionId==trunk.Id).Identities.Single() is {Identity:"CA-14",Cardinal:"NORTH"},
            "Independent branch references corroborate the common downstream road");
        Check(branchNames.Where(n=>n.PortionId==approach1.Id||n.PortionId==approach2.Id).All(n=>n.Identities.Length==0),
            "Common downstream name never spreads onto the individual approach ramps");
        Check(NamedRoadNetwork.Solve(branchMerge,[g1,g2 with{ObjectId=g1.ObjectId}]).All(n=>n.Identities.Length==0),
            "Two groups on one sign cannot corroborate downstream ownership");
        Check(NamedRoadNetwork.Solve(branchMerge,[g1,g2 with{Raw="TO CA-14"}]).All(n=>n.Identities.Length==0),
            "Explicit TO route is a future destination, not downstream ownership");
        Check(NamedRoadNetwork.Solve(branchMerge,[g1,g2 with{Cardinal="SOUTH"}]).All(n=>n.Identities.Length==0),
            "Opposite official directions cannot corroborate downstream ownership");
        var main=P(310,40,41,[0,0,0],[0,0,100]);
        var through=P(311,41,42,[0,0,100],[0,0,200],"PREFAB");
        var sideTurn=P(311,41,43,[0,0,100],[100,0,100],"PREFAB");
        var ahead=P(312,42,44,[0,0,200],[0,0,300]);
        var crossing=Graph([main,through,sideTurn,ahead],[new(main.Id,through.Id,"NATIVE_PORT"),new(main.Id,sideTurn.Id,"NATIVE_PORT"),new(through.Id,ahead.Id,"NATIVE_PORT")]);
        Check(RoadNameContinuity.Corridor(crossing,main.Id).Contains(ahead.Id)&&!RoadNameContinuity.Corridor(crossing,main.Id).Contains(sideTurn.Id),
            "Distinct through carriageway continues the name without renaming the turn");
        var aligned=RoadAlignedSignPose.Align(Graph([main],[]),new(2,0,50),new(2,7,50),Vector3.Normalize(new(.2f,0,-1)));
        Check(aligned is {} pose&&Vector3.Distance(pose.Normal,-Vector3.UnitZ)<.001,
            "FollowRoadDir aligns the complete sign pose with the closest facing lane");
        Check(RoadAlignedSignPose.Align(Graph([main,main with{Id="parallel",ItemUid=U(999),Paths=[[[4,0,0],[4,0,100]]]}],[]),
            new(2,0,50),new(2,7,50),-Vector3.UnitZ) is null,"Competing equally close roads keep automatic sign alignment unresolved");
        Check(RoadAlignedSignPose.Align(Graph([main with{Paths=[[[0,20,0],[0,20,100]]]}],[]),
            new(0,0,50),new(0,7,50),-Vector3.UnitZ) is null,"An overhead road cannot own a sign below its deck");
        var futureNames=branchNames.ToDictionary(n=>n.PortionId);
        Check(NamedRoadNetwork.ForwardTarget(branchMerge,futureNames,approach1.Id).Single().Identity=="CA-14",
            "Unnamed access uses the first downstream confirmed route as its target");
        Check(NamedRoadNetwork.ForwardTarget(crossing,NamedRoadNetwork.Solve(crossing,[S("next",ahead.Id)]).ToDictionary(n=>n.PortionId),main.Id).Length==0,
            "Future target cannot cross an unresolved fork to borrow a GPS-selected road");
        var legSpans=new[]{main,through,ahead}.Select((p,i)=>new RoadIdentitySpan("leg:"+i,i,p.ItemUid,p.EntryNodeUid,p.ExitNodeUid,
            i*100,(i+1)*100,i*100,(i+1)*100,"UNKNOWN",[],[],"")).ToArray();
        var legNames=crossing.Portions.ToDictionary(p=>p.Id,p=>new NamedNetworkPortion(p.Id,p.Id==ahead.Id?"RESOLVED":"UNKNOWN",
            p.Id==ahead.Id?[new("I-5","RouteReference","NORTH","MEDIUM","MEDIUM",["next"],"DIRECT_NETWORK_ANCHOR")]:[],[]));
        Check(NamedRoadNetwork.LegTarget(crossing,legNames,legSpans,0,3).Single().Identity=="I-5",
            "Exact captured leg can supply a future target without assigning names to connectors");
        Check(NamedRoadNetwork.LegTarget(crossing,legNames,legSpans,0,2).Length==0,
            "A future road beyond the following action is never the current maneuver target");
        Check(NamedRoadNetwork.LegTarget(crossing,legNames,legSpans.Select(s=>s.SegmentIndex==1?s with{ExitNodeUid=U(999)}:s).ToArray(),0,3).Length==0,
            "A disconnected GPS occurrence cannot supply a downstream target");
        var solved=NamedRoadNetwork.Solve(graph,anchors);
        Check(solved.Single(n=>n.PortionId==b.Id).Identities.Single().Identity=="I-5","Bracket includes a turn on directed graph without action-derived name boundary");
        Check(solved.Single(n=>n.PortionId==other.Id).Identities.Length==0,"Dead alternative exit never acquires chosen path name");
        Check(solved.Single(n=>n.PortionId==reverse.Id).Identities.Length==0,"Opposing directed portion remains separate");
        Check(solved.All(n=>n.Identities.All(l=>l.Confidence=="MEDIUM")),"Topology/repetition never promote HIGH");
        var shared=NamedRoadNetwork.Solve(graph,[S("first",a.Id,objectId:"one"),S("last",c.Id,objectId:"one")]);
        Check(shared.Single(n=>n.PortionId==b.Id).Identities.Length==0,"One physical object cannot count as two independent anchors");
        var single=NamedRoadNetwork.Solve(graph,[anchors[0]]);
        Check(single.Single(n=>n.PortionId==b.Id).Identities.Length==0,"Single anchor cannot name an interval");
        var second=P(105,2,3,[100,0,0],[100,0,100],"PREFAB");
        var fork=Graph([a,b,c,second],[new(a.Id,b.Id,"NATIVE_PORT"),new(b.Id,c.Id,"NATIVE_PORT"),new(a.Id,second.Id,"NATIVE_PORT"),new(second.Id,c.Id,"NATIVE_PORT")]);
        Check(NamedRoadNetwork.UniquePath(fork,a.Id,c.Id,5000) is null,"Two possible connecting paths stay unresolved");
        Check(NamedRoadNetwork.UniquePath(graph,a.Id,c.Id,50) is null,"Incomplete bounded search does not prove continuity");
        var last=P(109,4,6,[200,0,100],[300,0,100]);
        var diamond=Graph([a,b,second,c,last],[new(a.Id,b.Id,"NATIVE_PORT"),new(a.Id,second.Id,"NATIVE_PORT"),new(b.Id,c.Id,"NATIVE_PORT"),new(second.Id,c.Id,"NATIVE_PORT"),new(c.Id,last.Id,"NATIVE_PORT")]);
        Check(NamedRoadNetwork.MandatoryCorridor(diamond,a.Id,last.Id,double.PositiveInfinity)!.SequenceEqual(new[]{a.Id,c.Id,last.Id}),"Reconverging alternatives preserve only shared mandatory portion");
        var consensus=NamedRoadNetwork.Solve(diamond,[S("first",a.Id),S("last",last.Id)]);
        Check(consensus.Single(n=>n.PortionId==c.Id).Identities.Single().Identity=="I-5"&&consensus.Where(n=>n.PortionId==b.Id||n.PortionId==second.Id).All(n=>n.Identities.Length==0),"Naming consensus never names either divergent branch");
        var far=P(110,2,3,[100,0,0],[6100,0,0]);
        var distant=Graph([a,far,c],[new(a.Id,far.Id,"NATIVE_PORT"),new(far.Id,c.Id,"NATIVE_PORT")]);
        Check(NamedRoadNetwork.MandatoryCorridor(distant,a.Id,c.Id,double.PositiveInfinity)!.Contains(far.Id),"Two anchors can corroborate mandatory portions beyond old five-kilometre cutoff");
        var bypass=Graph([a,b,c,far],[new(a.Id,b.Id,"NATIVE_PORT"),new(b.Id,c.Id,"NATIVE_PORT"),new(a.Id,far.Id,"NATIVE_PORT"),new(far.Id,c.Id,"NATIVE_PORT")]);
        Check(!NamedRoadNetwork.MandatoryCorridor(bypass,a.Id,c.Id,double.PositiveInfinity)!.Contains(b.Id),"Long detour is considered and prevents false mandatory ownership");
        var mixed=NamedRoadNetwork.Solve(graph,[S("map",a.Id),S("north",c.Id,cardinal:"NORTH")]);
        Check(mixed.Single(n=>n.PortionId==b.Id).Identities.Single() is {Identity:"I-5",Cardinal:"",CardinalConfidence:"NONE"},"A map reference corroborates number without borrowing an official cardinal");
        var incompatible=NamedRoadNetwork.Solve(graph,[S("north",a.Id,cardinal:"NORTH"),S("south",c.Id,cardinal:"SOUTH")]);
        Check(incompatible.Single(n=>n.PortionId==b.Id).Identities.Length==0,"Opposing official directions never bracket an identity");
        Check(NamedRoadNetwork.MandatoryCorridor(graph,c.Id,a.Id,double.PositiveInfinity) is null,"Unreachable endpoint is not bracketing evidence");
        Check(NamedRoadNetwork.MandatoryCorridor(graph,a.Id,a.Id,double.PositiveInfinity)!.SequenceEqual(new[]{a.Id}),"Same portion creates no artificial interval");
        var returnRoad=P(101,4,8,[100,0,0],[0,0,0]);
        var turnback=Graph([a,b,returnRoad,c],[new(a.Id,b.Id,"NATIVE_PORT"),new(b.Id,returnRoad.Id,"NATIVE_PORT"),new(returnRoad.Id,c.Id,"NATIVE_PORT")]);
        Check(NamedRoadNetwork.MandatoryCorridor(turnback,a.Id,c.Id,double.PositiveInfinity,true) is null,"Same physical road traversed twice cannot corroborate reference continuity");
        Check(NamedRoadNetwork.Solve(turnback,[S("first",a.Id),S("last",c.Id)]).Single(n=>n.PortionId==b.Id).Identities.Length==0,"Turnback bracketing cannot propagate an official direction onto return carriageway");
        var throughPanel=new ExtractedSignPanel("1","area","token","/def/sign/board/overhead/ca_ohb_570x270_noep_1shld_1dir_1dest_pt_1.sii","OK","OK",new("TEST",[0,0,0],[0,0,0,1],1,null),[],[]);
        var throughArea=new ParsedSignArea("area",[],[],"token",0,0,["I-5","NORTH","Sacramento"],[],["I-5"],["NORTH"],["Sacramento"],[]);
        var distancePanel=throughPanel with{BoardPath="/mods/custom/distance.sii",Objects=[Text(900,"XX") with{Kind="sign_template_mileage"}]};
        Check(NamedRoadNetwork.IsRouteDistanceBoard(distancePanel,throughArea),"Mileage row plus one route header confirms current road independently of board file name");
        Check(!NamedRoadNetwork.IsRouteDistanceBoard(distancePanel,throughArea with{RawStrings=["TO I-5"]}),"TO mileage board does not name current road");
        Check(!NamedRoadNetwork.IsRouteDistanceBoard(distancePanel,throughArea with{ArrowTokens=["RIGHT"]}),"Arrow mileage board is a target, not current road");
        Check(!NamedRoadNetwork.IsRouteDistanceBoard(distancePanel,throughArea with{RoadShields=["I-5","I-10"]}),"Competing mileage headers cannot name one road");
        foreach(var state in SignTextClassifier.Prefix.Split('|'))
        {
            Check(SignPanelExtractor.DecodeRoad(state.ToLowerInvariant()+" 72 guide")==state+"-72","Every USA state atlas prefix survives extraction: "+state);
            Check(SignTextClassifier.RoadReferences(state+"-72 NORTH").SequenceEqual(new[]{state+"-72"}),"Every state text shares the same decoder: "+state);
        }
        foreach(var pair in new[]{("interstate 5","I-5"),("us 395 ca guide 2","US-395"),("ia county d20 guide","IA-CR-D20"),("tx farm road 1281 guide","TX-FM-1281"),("mo ab guide","MO-AB"),
            ("business us 87","US-87-BUS"),("interstate 35e wide","I-35E"),("la interstate 10","I-10"),("ks turnpike guide","KS-TURNPIKE")})
            Check(SignPanelExtractor.DecodeRoad(pair.Item1)==pair.Item2&&RoadIdentityRules.Reference(pair.Item2),"Special road family preserves its identity: "+pair.Item1);
        Check(SignPanelExtractor.DecodeRoad("type d guide 30")==""&&SignPanelExtractor.DecodeRoad("sign farm")=="","Arrow and farm warning never become numbered roads");
        Check(SignTextClassifier.RoadReferences("IN 2 LANES").Length==0&&SignTextClassifier.RoadReferences("IN 2 MI").Length==0,
            "Indiana's IN prefix does not turn lane instructions and distances into state roads");
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/mods/custom.sii","MOD",2,true,false,"arrow right")=="LEFT","Mod atlas entry and horizontal flip determine provisional arrow");
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/mods/custom.sii","MOD",2,false,false,"unknown artwork")=="","Unclassified mod artwork stays unresolved");
        Check(NamedRoadNetwork.IsPullThrough(throughPanel,throughArea),"Explicit pull-through family supports a number/cardinal/destination observation without an arrow");
        Check(!NamedRoadNetwork.IsPullThrough(throughPanel with{BoardPath=throughPanel.BoardPath.Replace("_pt_","_adv_")},throughArea),"Advance guide is not a current-road confirmation");
        Check(!NamedRoadNetwork.IsPullThrough(throughPanel,throughArea with{ArrowTokens=["right"]}),"Directional arrow blocks no-arrow pull-through confirmation");
        Check(!NamedRoadNetwork.IsPullThrough(throughPanel,throughArea with{ArrowTokens=["DOWN_LANE"]}),
            "Down-lane guidance alone cannot establish the current road's identity");
        Check(!NamedRoadNetwork.IsPullThrough(throughPanel,throughArea with{ArrowTokens=["DIAGONAL_UP_RIGHT"]}),
            "A pull-through template overridden to an exit arrow remains a directional target");
        Check(!NamedRoadNetwork.IsPullThrough(throughPanel,throughArea with{RoadShields=["I-5","I-10"]}),"Multiple references are not fused into one pull-through observation");
        Check(!NamedRoadNetwork.IsPullThrough(throughPanel,throughArea with{RawStrings=["TO I-5","NORTH","Sacramento"]}),"TO reference never names current road");
        Check(!NamedRoadNetwork.IsPullThrough(throughPanel,throughArea with{ExitNumbers=["42"]}),"Exit panel cannot supply a pull-through owner");
        Check(!NamedRoadNetwork.IsPullThrough(throughPanel with{Status="NO_BOARD"},throughArea),"Missing reconstructed board cannot supply its semantic role");
        var guide=NamedRoadNetwork.Solve(graph,[S("guide",a.Id,"I-5","GuideTo") with{Owners=[]}]);
        Check(guide.All(n=>n.Identities.Length==0&&n.Candidates.Length==0),"GuideTo not converted into current road name");
        var street=NamedRoadNetwork.Solve(graph,[S("street",a.Id,"I-5","CrossStreet") with{Owners=[]}]);
        Check(street.All(n=>n.Identities.Length==0),"Cross-street not assigned to supporting road");
        var ambiguous=NamedRoadNetwork.Solve(graph,[S("ambiguous",a.Id) with{AssociationStatus="AMBIGUOUS_OWNERS"}]);
        Check(ambiguous.Single(n=>n.PortionId==a.Id).Candidates.Single().Confidence=="LOW","Ambiguous owner stays hypothesis");
        var aliases=NamedRoadNetwork.Solve(graph,[S("five",a.Id),S("ten",a.Id,"I-10")]);
        Check(aliases.Single(n=>n.PortionId==a.Id).Status=="AMBIGUOUS","Two unverified names do not establish concurrency");
        var cards=NamedRoadNetwork.Solve(graph,[S("north",a.Id,cardinal:"NORTH"),S("south",a.Id,cardinal:"SOUTH")]);
        Check(cards.Single(n=>n.PortionId==a.Id).Status=="CONFLICT","Official cardinal conflict cannot be erased");
        var face=NamedRoadNetwork.Associate(graph,new(50,5,0),new(-1,0,0),"OwnRoute","MODEL_FACE_CANDIDATE");
        Check(face.Single(x=>x.PortionId==a.Id).Status=="CANDIDATE"&&face.Single(x=>x.PortionId==reverse.Id).Status=="FACE_NOT_APPROACH_CANDIDATE","Facing normal separates opposite senses");
        var rotatedA=P(101,1,2,[0,0,0],[0,0,100]);var rotatedR=P(101,2,1,[0,0,100],[0,0,0]);
        var rotated=NamedRoadNetwork.Associate(Graph([rotatedA,rotatedR],[]),new(0,5,50),new(0,0,-1),"OwnRoute","MODEL_FACE_CANDIDATE");
        Check(rotated.Single(x=>x.PortionId==rotatedA.Id).Status=="CANDIDATE"&&rotated.Single(x=>x.PortionId==rotatedR.Id).Status=="FACE_NOT_APPROACH_CANDIDATE","90 degree rotation preserves physical facing decision");
        var noface=NamedRoadNetwork.Associate(graph,new(50,5,0),null,"MapAnnotation","UI_ANNOTATION");
        Check(noface.Single(x=>x.PortionId==reverse.Id).Status=="CANDIDATE","UI annotation has no physical facing rule");
        Check(Math.Abs(face.Single(x=>x.PortionId==a.Id).HeightM-5)<.001,"Suspended sign altitude remains diagnostic");
        var beforeSign=P(106,6,7,[0,0,0],[49,0,0]);
        var beyondSign=P(107,7,8,[51,0,0],[100,0,0]);
        var crossSection=NamedRoadNetwork.Associate(Graph([a,beforeSign,beyondSign],[]),new(50,5,0),new(-1,0,0),"OwnRoute","MODEL_FACE_CANDIDATE");
        Check(crossSection.Single(x=>x.PortionId==a.Id).Status=="CANDIDATE"&&crossSection.Where(x=>x.PortionId!=a.Id).All(x=>x.Status=="OUTSIDE_SIGN_CROSS_SECTION"),"Nearby endpoints cannot steal ownership from road crossing sign section");
        var parallel=P(108,9,10,[0,0,2],[100,0,2]);
        var unresolved=NamedRoadNetwork.Reassociate(Graph([a,parallel],[]),S("parallel",a.Id,"I-5","OwnRoute") with{Normal=[-1,0,0]});
        Check(unresolved.AssociationStatus=="AMBIGUOUS_OWNERS","Two parallel carriageways crossing section remain ambiguous");
        var snapshot=new NamedNetworkSnapshot(1,"test","REBUILD",region,graph.Portions,graph.Connections,[],anchors,solved);
        NamedNetworkValidation.Validate(snapshot);Check(true,"Valid named network proof scopes accepted");
        try{NamedNetworkValidation.Validate(snapshot with{Names=solved.Select(n=>n.PortionId==a.Id?n with{Identities=[n.Identities[0] with{Confidence="HIGH"}]}:n).ToArray()});throw new Exception("Forged HIGH accepted");}
        catch(InvalidDataException){Check(true,"Forged HIGH rejected");}
        var json=JsonSerializer.Serialize(snapshot,LiveProtocol.Json);
        Check(JsonSerializer.Serialize(JsonSerializer.Deserialize<NamedNetworkSnapshot>(json,LiveProtocol.Json),LiveProtocol.Json)==json,"Persistent database roundtrip");
        var solvedAgain=NamedRoadNetwork.Solve(graph,anchors.Reverse().ToArray());
        Check(JsonSerializer.Serialize(solvedAgain,LiveProtocol.Json)==JsonSerializer.Serialize(solved,LiveProtocol.Json),"Anchor enumeration order cannot select the name");
        var physical=new[]{1,2,3,4}.Select((v,i)=>new PhysicalRouteRow(i,(ulong)v,0,0,300-i*100,0)).ToArray();
        var probe=new ProbeRoute{Stem="test",PhysicalPath="",LogicalPath="",Physical=physical,Logical=[]};
        var portions=new[]{a,b,c};var edges=portions.Select((p,i)=>new RouteCorridorEdge(i,i,p.Kind=="ROAD"?"Road":"Prefab",Convert.ToUInt64(p.ItemUid,16),"",NetworkPortion.V(p.Paths[0][0]),NetworkPortion.V(p.Paths[0][1]),i*100,(i+1)*100,i*100,(i+1)*100,0)).ToArray();
        var corridor=new RouteCorridor(edges,edges.Select(e=>new RouteCorridorSegment(e.SegmentIndex,e.ItemType,e.ItemUid,e.StartStationM,e.EndStationM,e.StartNavStationM,e.EndNavStationM,e.EdgeIndex,1,"",null)).ToArray());
        var live=new LiveRoute("test",edges.Select(e=>new LiveEdge(e.SegmentIndex,e.ItemUid.ToString("X16"),new(e.Start.X,e.Start.Y,e.Start.Z),new(e.End.X,e.End.Y,e.End.Z),e.StartStationM,e.EndStationM,e.StartNavStationM,e.EndNavStationM)).ToArray(),[new("action",0,0,0,"KEEP_LEFT","branch","HIGH",[])]);
        var joined=NamedRoadNetwork.Join(snapshot,probe,corridor,live);
        var streetWire=live with{Actions=live.Actions.Select(action=>action with{SignTargets=[new("Central Ave","","","MEDIUM","street-object"){Kind="StreetName"}]}).ToArray()};
        LiveProtocol.Validate(new(1,"RouteSnapshot","street",1,0,1,streetWire));
        Check(true,"Street target kind survives the same live validation as numbered routes");
        Check(joined.Spans.All(s=>s.Status=="RESOLVED")&&joined.Spans[1].Identities.Single().Identity=="I-5","Directed GPS join reads independently prepared database");
        var unknown=joined.Spans.Select((s,i)=>i==1?s with{Status="UNKNOWN",Identities=[],Candidates=[]}:s).ToArray();
        Check(LiveGuidanceEngine.ContinueDistance(joined.Spans,joined.Spans[0],20,250)==230,"Continue distance spans same named road up to next action");
        Check(LiveGuidanceEngine.ContinueDistance(unknown,unknown[0],20,250)==80,"Continue-on instruction stops before unknown road");
        Check(LiveGuidanceEngine.ContinueDistance(unknown,unknown[1],120,250) is null,"Unknown current road cannot produce named continue instruction");
        var duplicateCardinal=joined.Spans.Select(s=>s with{Identities=[..s.Identities,
            s.Identities[0] with{Cardinal="NORTH",CardinalConfidence="MEDIUM"}]}).ToArray();
        duplicateCardinal[1]=duplicateCardinal[1] with{Identities=[duplicateCardinal[1].Identities[^1]]};
        Check(LiveGuidanceEngine.ContinueDistance(duplicateCardinal,duplicateCardinal[0],20,250)==230,
            "Reference-only and signed anchors on the same road do not shorten the continue distance");
        duplicateCardinal[1]=duplicateCardinal[1] with{Identities=[duplicateCardinal[1].Identities[0] with{Cardinal="SOUTH"}]};
        Check(LiveGuidanceEngine.ContinueDistance(duplicateCardinal,duplicateCardinal[0],20,250)==80,
            "Opposing explicit cardinals stop the named continue interval");
        var context=RoadIdentityResolver.Contexts(live,unknown,[]).Single();
        Check(context.TargetSpanIds.Single()=="segment:1"&&context.Reason=="TARGET_UNKNOWN"&&context.PreferredIdentity is null,"Unknown immediate target cannot borrow downstream identity");
        Check(joined.Spans.All(s=>s.Identities.All(l=>l.Cardinal.Length==0)),"Road reference cannot invent official cardinal");
        var shown=NativeBridgeCli.Label(new("RESOLVED",[new("I-5","RouteReference","NORTH","HIGH","MEDIUM",[])],[],"test"));
        Check(shown.Contains("I-5 [confirmee]")&&shown.Contains("NORTH [provisoire]"),"Console separates name quality and provisional official direction");
        Check(!NativeBridgeCli.Label(new("CANDIDATE",[new("I-5","RouteReference","SOUTH","LOW","LOW",[])],[],"test")).Contains("SOUTH"),"Weak cardinal is not announced as driving direction");
        // A shield face can be a non-rectangular polygon drawn on its support, with no separate board model.
        var vertices=new[]{new TruckLib.Models.Vertex(0,0,0),new(1,0,0),new(.5f,1,0)};
        foreach(var v in vertices)v.Normal=Vector3.UnitZ;
        Check(SupportSignFaceResolver.Plane(vertices)?.Normal==Vector3.UnitZ,"Non-rectangular shield mesh face recognized without board dimensions");
        vertices[2].Position=new(.5f,1,.1f);
        Check(SupportSignFaceResolver.Plane(vertices) is null,"Non-planar support piece not accepted as sign face");
        vertices[2].Position=new(.5f,1,0);vertices[2].Normal=-Vector3.UnitZ;
        Check(SupportSignFaceResolver.Plane(vertices) is null,"Mixed front/back normals stay unresolved");
        var longAlternative=Enumerable.Range(0,130).Select(i=>P(500+i,i+10,i+11,[i,0,0],[i+1,0,0])).ToArray();
        var cappedGraph=Graph(new[]{a,c}.Concat(longAlternative).ToArray(),new[]{new NetworkConnection(a.Id,c.Id,"NATIVE_PORT"),
            new(a.Id,longAlternative[0].Id,"NATIVE_PORT"),new(longAlternative[^1].Id,c.Id,"NATIVE_PORT")}.Concat(longAlternative.Zip(longAlternative.Skip(1),(x,y)=>new NetworkConnection(x.Id,y.Id,"NATIVE_PORT"))).ToArray());
        Check(NamedRoadNetwork.UniquePath(cappedGraph,a.Id,c.Id,5000) is null,"Truncated long alternative cannot make a short path appear unique");
        Console.WriteLine($"PASS: {count} named network assertions.");
    }

    public static void VerifySaved(string directory)
    {
        int checkedFiles=0;
        foreach(var path in Directory.GetFiles(directory,"*_named-network.json"))
        {
            var data=JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(path),LiveProtocol.Json)!;
            NamedNetworkValidation.Validate(data);
            var graph=new DirectedRoadNetworkGraph(data.Region,data.Portions,data.Connections,data.Errors);
            var names=NamedRoadNetwork.Solve(graph,data.Sources);
            if(JsonSerializer.Serialize(names,LiveProtocol.Json)!=JsonSerializer.Serialize(data.Names,LiveProtocol.Json))
                throw new Exception("Saved names differ from current independent network solve: "+path);
            checkedFiles++;
        }
        if(checkedFiles==0)throw new Exception("No saved named network");
        Console.WriteLine($"PASS: {checkedFiles} saved networks validate and reproduce all names without the game, map loader or GPS selection.");
    }
    public static void ExportCorridorFixtures(string output)
    {
        var random=new Random(32187);var results=new List<object>();
        for(int test=0;test<150;test++)
        {
            var portions=Enumerable.Range(0,7).Select(i=>new NetworkPortion(i.ToString(),i.ToString("X16"),"ROAD","0000000000000001","0000000000000001",[[new float[]{i,0,0},new float[]{i+1,0,0}]],[])).ToArray();
            var links=(from a in portions from b in portions where a!=b&&random.NextDouble()<.24 select new NetworkConnection(a.Id,b.Id,"NATIVE_PORT")).ToArray();
            var graph=new DirectedRoadNetworkGraph(new(-100,100,-100,100),portions,links,[]);
            results.Add(new{test,vertices=portions.Select(p=>p.Id),edges=links.Select(l=>new[]{l.From,l.To}),start="0",end="6",mandatory=NamedRoadNetwork.MandatoryCorridor(graph,"0","6",double.PositiveInfinity)});
        }
        File.WriteAllText(output,JsonSerializer.Serialize(results,LiveProtocol.Json));
        Console.WriteLine("Exported 150 deterministic directed-graph cases for independent exhaustive comparison.");
    }

    public static void RefreshSaved(string directory,bool upgradePullThrough=false)
    {
        foreach(var path in Directory.GetFiles(directory,"*_named-network.json"))
        {
            var previous=JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(path),LiveProtocol.Json)!;
            var graph=new DirectedRoadNetworkGraph(previous.Region,previous.Portions,previous.Connections,previous.Errors);
            var stem=Path.GetFileName(path)[..^"_named-network.json".Length];
            var original=previous.Sources;
            var savedPanels=JsonSerializer.Deserialize<ExtractedSignPanel[]>(File.ReadAllText(Path.Combine(directory,stem+"_panels.json")))!;
            original=original.Select(s=>
            {
                if(s.Role!="GuideTo")return s;
                var panel=savedPanels.FirstOrDefault(p=>p.SignUid==s.ObjectId&&p.SemanticGroups.Any(g=>g.Id==s.Id));
                if(panel is null)return s;
                var group=panel.SemanticGroups.Single(g=>g.Id==s.Id);
                var arrows=panel.Objects.Where(o=>o.Visible&&o.Reliable&&group.ArrowObjectIds.Contains(o.Id)).Select(o=>o.Arrow).Distinct().ToArray();
                return s with{Arrow=arrows.Length==1?arrows[0]:"",ReadingConfidence=SignGroupGuidance.ReadableGroupRoad(group,panel)?"MEDIUM":"LOW"};
            }).ToArray();
            // Reclassify source roles from the saved effective board objects.
            // World poses remain those extracted from mounted model data.
            {
                var panels=JsonSerializer.Deserialize<ExtractedSignPanel[]>(File.ReadAllText(Path.Combine(directory,stem+"_panels.json")))!;
                original=original.Select(s=>
                {
                    if(s.Role!="GuideTo")return s;
                    var panel=panels.FirstOrDefault(p=>p.SignUid==s.ObjectId&&p.SemanticGroups.Any(g=>g.Id==s.Id));if(panel is null)return s;
                    var groups=panel.SemanticGroups;var area=new ParsedSignArea(panel.AreaName,[],[],panel.BoardToken,0,0,[s.Raw],[],
                        groups.Select(g=>g.Road).Where(x=>x.Length>0).Distinct().ToArray(),groups.Select(g=>g.Cardinal).Where(x=>x.Length>0).Distinct().ToArray(),
                        groups.Select(g=>g.Destination).Where(x=>x.Length>0).Distinct().ToArray(),panel.Objects.Where(o=>o.Visible&&o.Arrow.Length>0).Select(o=>o.Arrow).ToArray());
                    var distanceBoard=NamedRoadNetwork.IsRouteDistanceBoard(panel,area);
                    if(!distanceBoard&&(!upgradePullThrough||!NamedRoadNetwork.IsPullThrough(panel,area)))return s;
                    var fields=SignGroupGuidance.ValidateFields(groups.Single(g=>g.Id==s.Id),panel,_=>false);
                    return s with{Id=$"{s.ObjectId}:{panel.AreaName}:own",Source=distanceBoard?"DISTANCE_BOARD_REFERENCE":"PULL_THROUGH_REFERENCE",Role="OwnRoute",ReadingConfidence=fields.IdentityConfidence,
                        Cardinal=fields.CardinalConfidence=="HIGH"?s.Cardinal:""};
                }).OrderBy(s=>s.Id,StringComparer.Ordinal).ToArray();
            }
            var data=NamedRoadNetwork.Prepare(graph,AdvanceExitLinker.Link(graph,original.Select(s=>NamedRoadNetwork.Reassociate(graph,s)).ToArray()));
            NamedNetworkValidation.Validate(data);
            var routePath=Path.Combine(directory,stem+"_route.json");
            var envelope=JsonSerializer.Deserialize<LiveMessage>(File.ReadAllText(routePath),LiveProtocol.Json)!;
            var route=envelope.Route!;
            var joined=NamedRoadNetwork.Join(data,route.RoadIdentities!.Spans,route);
            route=NamedRoadNetwork.AttachSignTargets(data,route with{RoadIdentities=joined});envelope=envelope with{Route=route};
            LiveProtocol.Validate(envelope);
            File.WriteAllText(routePath,JsonSerializer.Serialize(envelope,LiveProtocol.Json));
            File.WriteAllText(Path.Combine(directory,stem+"_road-identities.json"),JsonSerializer.Serialize(joined,LiveProtocol.Json));
            NamedNetworkView.Write(directory,stem,data,joined,route);RoadIdentityView.Write(directory,route);
            Console.WriteLine($"REFRESH: {stem}: {joined.Spans.Count(s=>s.Status=="RESOLVED")}/{joined.Spans.Length} named, no map loading or GPS ownership selection.");
        }
    }
}


