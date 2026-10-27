using System.Numerics;
using BetterGPSNavigation;

internal static class SignRobustnessTests
{
    internal static void Audit(string file)
    {
        var net=System.Text.Json.JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(file),LiveProtocol.Json)!;
        var graph=new DirectedRoadNetworkGraph(net.Region,net.Portions,net.Connections,net.Errors);
        foreach(var s in net.Sources.Where(s=>s.Source=="UNCLASSIFIED_STREET_SIGN"))
        {
            var across=StreetNameLinker.Associate(graph,s with{Source="STREET_NAME_SIGN"});
            var along=StreetNameLinker.Associate(graph,s with{Source="STREET_CONFIRMATION_SIGN"});
            Console.WriteLine(System.Text.Json.JsonSerializer.Serialize(new{s.Identity,lateral=across.Owners,facing=along.Owners},LiveProtocol.Json));
        }
    }
    internal static void Run()
    {
        int count=0;void Check(bool value,string reason){count++;if(!value)throw new Exception(reason);}
        ExtractedSignPanel Panel(string area,string path,float x,float y)=>new("1",area,"",path,"OK","",
            new("ACTIVE_MODEL_LOCATOR_UNCALIBRATED",[x,y,0],[0,0,0,1],1,null),[],[]);
        ParsedSignArea Area(string name,string path,float x,float y,string road="",string card="",string arrow="")=>
            new(name,[],[],"",0,0,[],[],road.Length==0?[]:[road],card.Length==0?[]:[card],[],arrow.Length==0?[]:[arrow]){Extraction=Panel(name,path,x,y)};
        var left=Area("shield-left","/def/sign/board/shield/us.sii",-1,0,"US-50");
        var right=Area("shield-right","/def/sign/board/shield/is.sii",1,0,"I-5");
        const string cards="/def/sign/board/shield/card_dir_2.sii",arrows="/def/sign/board/shield/arrow_dir.sii";
        var north=Area("north",cards,-1,.5f,card:"NORTH");var south=Area("south",cards,1,.5f,card:"SOUTH");
        var arrow=Area("arrow",arrows,1,-.5f,arrow:"RIGHT");
        ParsedSignArea[] assembly=[left,right,north,south,arrow];
        Check(ShieldAssemblyLinker.Companions(assembly,left,cards).Single().Cardinals.Single()=="NORTH","Left shield borrows right cardinal");
        Check(ShieldAssemblyLinker.Companions(assembly,right,cards).Single().Cardinals.Single()=="SOUTH","Right shield borrows left cardinal");
        Check(ShieldAssemblyLinker.Companions(assembly,left,arrows).Length==0,"Left shield borrows right arrow");
        Check(ShieldAssemblyLinker.Companions(assembly,right,arrows).Single().AreaName=="arrow","Right shield lost its arrow");
        Check(ShieldAssemblyLinker.Companions([left,right,north with{Extraction=Panel("north",cards,0,.5f)}],left,cards).Length==0,"Equidistant direction assigned arbitrarily");
        Check(ShieldAssemblyLinker.Companions([left,right with{Extraction=right.Extraction! with{Anchor=right.Extraction.Anchor with{LocalPosition=null}}},north],left,cards).Length==0,"Missing competitor placement ignored");
        Check(ShieldAssemblyLinker.Companions([left,north],left,cards).Length==1,"Single-shield assembly regression");
        var road=new NetworkPortion("r","road","ROAD","a","b",[[[-20,0,0],[20,0,0]]],[]);
        var graph=new DirectedRoadNetworkGraph(new(-100,100,-100,100),[road],[],[]);
        var info=Panel("name","/def/sign/board/info/hwy_info_1a.sii",0,0);
        Check(!StreetNameLinker.OverheadInformationPlate(graph,info,new(0,6,0)),"Height alone classifies an unknown support");
        info=info with{SupportModel="/model/sign/navigation/tx_side_board_green_213_152_122x61.pmd"};
        Check(StreetNameLinker.OverheadInformationPlate(graph,info,new(0,6,0)),"Inspected suspended family not recognized");
        Check(!StreetNameLinker.OverheadInformationPlate(graph,info,new(0,1,0)),"Inspected support incorrectly mounted at ground level accepted");
        Check(!StreetNameLinker.OverheadInformationPlate(graph,info with{FollowRoadDir=true},new(0,6,0)),"Unsupported transform accepted");
        Check(!StreetNameLinker.OverheadInformationPlate(graph,info with{BoardOffset=[1,0]},new(0,6,0)),"Unresolved offset accepted");
        var source=new NetworkNameSource("s","1","UNCLASSIFIED_STREET_SIGN","CrossStreet","Main St","Main St","StreetName","","",
            [0,6,0],[0,0,1],"MODEL_CANDIDATE","MEDIUM","",["r"],[]);
        var unresolved=NamedRoadNetwork.Reassociate(graph,source);
        Check(unresolved.Owners.Length==0&&unresolved.AssociationStatus=="STREET_INTERPRETATION_UNRESOLVED","Unknown interpretation retains stale owner");
        Check(NamedRoadNetwork.Solve(graph,[unresolved]).All(n=>n.Identities.Length==0),"Unknown support becomes a resolved route name");
        foreach(var pair in new[]{("Oak Lane","Oak Ln"),("Oak Court","Oak Ct"),("Oak Parkway","Oak Pkwy"),("Oak Circle","Oak Cir"),("Oak Place","Oak Pl"),("Oak Terrace","Oak Ter"),("Oak Trail","Oak Trl")})
        {
            Check(StreetNameLinker.Canonical(pair.Item1)==StreetNameLinker.Canonical(pair.Item2),"Equivalent suffixes conflict: "+pair.Item1);
            Check(SignGroupResolver.IsSignedStreetName(pair.Item1)&&SignGroupResolver.IsSignedStreetName(pair.Item2),"Street suffix not recognized: "+pair.Item1);
        }
        var recorded=System.Text.Json.JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(
            "validation/sign-robustness/final/live_000007_FD38E35A2E1D41EF_named-network.json"),LiveProtocol.Json)!;
        var recordedGraph=new DirectedRoadNetworkGraph(recorded.Region,recorded.Portions,recorded.Connections,recorded.Errors);
        var carpenter=recorded.Sources.Single(s=>s.Identity=="Carpenter Rd");
        var inferred=StreetNameLinker.InterpretWithIndependentNames(recordedGraph,carpenter,recorded.Sources,recorded.Names);
        Check(StreetNameLinker.InterpretWithIndependentNames(recordedGraph,carpenter with{Identity="Place Name"},recorded.Sources,recorded.Names).Source=="UNCLASSIFIED_STREET_SIGN",
            "A bare place label is promoted to a street using negative evidence");
        Check(inferred.Source=="CORROBORATED_STREET_INTERPRETATION"&&inferred.StreetAxisMode=="LATERAL", "Carpenter axis lacks independent disambiguation");
        Check(inferred.InterpretationProofIds.Length>0&&inferred.InterpretationProofIds.All(id=>recorded.Sources.Single(s=>s.Id==id).ObjectId!=carpenter.ObjectId),"Panel corroborates itself");
        var withoutProof=recorded.Names.Select(n=>n with{Identities=[]}).ToArray();
        Check(StreetNameLinker.InterpretWithIndependentNames(recordedGraph,carpenter,recorded.Sources,withoutProof).Source=="UNCLASSIFIED_STREET_SIGN", "Unknown axis accepted after removing independent evidence");
        var resolvedSources=recorded.Sources.Select(s=>s.Id==carpenter.Id?inferred:s).ToArray();
        var resolved=NamedRoadNetwork.Prepare(recordedGraph,resolvedSources);NamedNetworkValidation.Validate(resolved,recordedGraph);
        var route=System.Text.Json.JsonSerializer.Deserialize<LiveMessage>(File.ReadAllText(
            "validation/sign-robustness/final/live_000007_FD38E35A2E1D41EF_route.json"),LiveProtocol.Json)!.Route!;
        var joined=NamedRoadNetwork.Join(resolved,route.RoadIdentities!.Spans,route);
        Check(joined.Spans.Where(s=>s.SegmentIndex is >=16 and <=20).All(s=>s.Identities.Any(i=>i.Identity=="Carpenter Rd")),"Independent interpretation loses Carpenter continuation");
        Console.WriteLine($"PASS: {count} sign-family and multi-shield ambiguity assertions.");
    }
}
