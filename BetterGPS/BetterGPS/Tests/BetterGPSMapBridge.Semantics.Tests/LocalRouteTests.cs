using System.Text.Json;
using BetterGPSNavigation;

internal static class LocalRouteTests
{
    internal static void Run()
    {
        var panels=JsonSerializer.Deserialize<ExtractedSignPanel[]>(File.ReadAllText("validation/ingame-navigation/20261011_202408_6960f860/live_000009_ACFE4A1C4EDD40C8_panels.json"))!;
        int count=0;void Check(bool ok,string why){count++;if(!ok)throw new Exception(why);}
        var ruralPanels=JsonSerializer.Deserialize<ExtractedSignPanel[]>(File.ReadAllText("validation/local-route-diagnostic/replay/live_000007_4903590BBBD1E6BD_panels.json"))!;
        var header=ruralPanels.Single(p=>p.SignUid=="4E573074C21B0001"&&p.Status=="OK");
        Check(StreetNameLinker.IntersectionHeaders(header).SequenceEqual(new[]{"Ave 24"}),"Cross-street header erased by landmark arrow below");
        Check(StreetNameLinker.IntersectionHeaders(header with{Objects=header.Objects.Where(o=>o.Kind!="sign_template_frame").ToArray()}).Length==0,"Header guessed without a separator");
        Check(StreetNameLinker.IntersectionHeaders(header with{BoardPath="/def/sign/board/overhead/exit.sii"}).Length==0,"Highway exit header treated as a current cross street");
        foreach(var id in new[]{"50D610D6C19B0001","50D612284B9B0000"})
        {
            var panel=panels.Single(p=>p.SignUid==id&&p.Status=="OK");
            var groups=SignGroupResolver.Resolve(panel);
            Check(groups.Count==2&&groups.All(g=>g.Method=="EXPLICIT_HORIZONTAL_DIVIDER"),"Explicit street rows not separated");
            Check(groups.Select(g=>g.Road).Order().SequenceEqual(new[]{"Gates Rd","Paradise Rd"}),"Street names lost or merged");
            Check(groups.All(g=>g.Kind=="StreetName"&&g.Status=="COMPLETE_CANDIDATE"&&SignGroupGuidance.ReadableGroupRoad(g,panel)),"Separated street rows unreadable");
            Check(groups.All(g=>g.ArrowObjectIds.Count==1)&&groups.SelectMany(g=>g.ObjectIds).Distinct().Count()==4,"Shared objects contaminated two directions");
            Check(groups.Single(g=>g.Road=="Paradise Rd").ArrowObjectIds.Select(a=>panel.Objects.Single(o=>o.Id==a).Arrow).Single()==(id=="50D610D6C19B0001"?"LEFT":"RIGHT"),"Street destination arrow swapped");
            var noDivider=panel with{Objects=panel.Objects.Where(o=>o.Kind!="sign_template_frame").ToArray()};
            Check(SignGroupResolver.Resolve(noDivider).All(g=>g.Status=="UNRESOLVED"),"No separator guessed two street destinations");
        }
        var frames=File.ReadLines("validation/local-route-diagnostic/guidance-snapshot.jsonl")
            .Select(l=>JsonSerializer.Deserialize<LiveGuidance>(l,LiveProtocol.Json)!).Where(g=>g.Status=="CRUISE"&&g.Road=="Carpenter Rd").ToArray();
        Check(frames.Length>0,"Missing recorded Carpenter announcement");
        Check(frames.All(g=>!NativeBridgeCli.Describe(g).StartsWith("Continue toward Carpenter")),"Distant maneuver target described as present-road continuation");
        Check(frames.Any(g=>NativeBridgeCli.Describe(g).Contains("onto Carpenter Rd")),"Upcoming Carpenter action disappeared");
        var saved=JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText("validation/local-route-diagnostic/replay/live_000009_ACFE4A1C4EDD40C8_named-network.json"),LiveProtocol.Json)!;
        var graph=new DirectedRoadNetworkGraph(saved.Region,saved.Portions,saved.Connections,saved.Errors);
        var paired=saved.Sources.Where(s=>s.Kind=="StreetName"&&s.GroupMethod=="EXPLICIT_HORIZONTAL_DIVIDER")
            .GroupBy(s=>s.ObjectId).Where(g=>g.Count()==2&&g.Select(s=>s.Arrow).Order().SequenceEqual(new[]{"LEFT","RIGHT"}))
            .SelectMany(g=>g).Select(s=>s.Id).ToHashSet();
        var sources=saved.Sources.Select(s=>NamedRoadNetwork.Reassociate(graph,paired.Contains(s.Id)?s with{Source="DIRECTIONAL_STREET_ROWS"}:s)).ToArray();
        var maze=sources.Single(s=>s.ObjectId=="51B2CD02841B0001"&&s.Identity=="Maze Blvd");
        Check(maze.Role=="StreetAxis"&&maze.Owners.Any(id=>graph.Get(id).ItemUid=="4E1C1C54301AA9E4")&&maze.Owners.All(id=>graph.Get(id).ItemUid is "4E1C1C54301AA9E4" or "4E1C1C54185AA879"),"Maze assigned to a remote junction instead of adjacent west arm");
        Check(StreetNameLinker.DirectionalArm(graph,maze with{Normal=null}).Owners.Length==0,"Missing board facing guessed an arm");
        Check(StreetNameLinker.Canonical("L Street")==StreetNameLinker.Canonical("L St"),"Street suffix aliases treated as different roads");
        foreach(var name in new[]{"Paradise Rd","Gates Rd"})
            Check(sources.Any(s=>s.Identity==name&&s.Source=="DIRECTIONAL_STREET_ROWS"&&s.Role=="StreetAxis"&&s.Owners.Length>0),"Angled local street arm not recovered: "+name);
        var solved=NamedRoadNetwork.Prepare(graph,sources);NamedNetworkValidation.Validate(solved,graph);
        Check(solved.Names.Single(n=>n.PortionId=="4E1C1C54185AA879:4E1C1C5CFCDAAD22>4E1C1C5C4C1AAD21").Identities.Any(i=>i.Identity=="Maze Blvd"),"Maze outgoing crossing remains ambiguous");
        foreach(var item in new[]{"4E1C1C54301AA9E4","4E1C1C54AE5AA9D5"})
            Check(solved.Names.Where(n=>graph.Get(n.PortionId).ItemUid==item).All(n=>n.Identities.Any(i=>i.Identity=="Maze Blvd")&&!n.Identities.Any(i=>i.Identity=="L St")),"Old street name crossed Maze boundary");
        Check(solved.Names.Where(n=>graph.Get(n.PortionId).ItemUid=="4E1C1C549FDAA87A").All(n=>n.Identities.Any(i=>i.Identity=="L St")),"L Street lost before its boundary");
        Console.WriteLine($"PASS: {count} local-street grouping and current-road display assertions.");
    }
}
