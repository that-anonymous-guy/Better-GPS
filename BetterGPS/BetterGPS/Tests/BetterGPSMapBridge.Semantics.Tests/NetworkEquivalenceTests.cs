using BetterGPSNavigation;
using System.Diagnostics;
using System.Text.Json;
internal static class NetworkEquivalenceTests
{
    internal static void Run(string baseline,string output)
    {
        var data=JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(baseline),LiveProtocol.Json)!;
        var graph=new DirectedRoadNetworkGraph(data.Region,data.Portions,data.Connections,data.Errors);
        var clock=Stopwatch.StartNew();var actual=NamedRoadNetwork.Solve(graph,data.Sources);clock.Stop();
        var expected=data.Names.ToDictionary(p=>p.PortionId);
        var differences=actual.Where(p=>JsonSerializer.Serialize(p,LiveProtocol.Json)!=JsonSerializer.Serialize(expected[p.PortionId],LiveProtocol.Json)).Select(p=>p.PortionId).ToArray();
        bool SameLabel(NetworkIdentity a,NetworkIdentity b)=>a.Identity==b.Identity&&a.Kind==b.Kind&&a.Cardinal==b.Cardinal;
        var regressions=actual.Where(p=>expected[p.PortionId].Status=="RESOLVED"&&
            (p.Status!="RESOLVED"||expected[p.PortionId].Identities.Any(old=>!p.Identities.Any(now=>SameLabel(old,now))))).Select(p=>p.PortionId).ToArray();
        var additions=actual.Where(p=>p.Status=="RESOLVED"&&expected[p.PortionId].Status=="UNKNOWN").ToArray();
        var sourceIndex=data.Sources.ToDictionary(s=>s.Id);var actualIndex=actual.ToDictionary(p=>p.PortionId);
        bool JustifiedRemoval(string id,NetworkIdentity label)
        {
            if(!label.Rule.StartsWith("BRACKETED_"))return false;
            var paths=new List<string[]>();
            foreach(var a in label.SourceIds.Select(s=>sourceIndex[s]))foreach(var b in label.SourceIds.Select(s=>sourceIndex[s]))
            {
                if(a.ObjectId==b.ObjectId||b.Role=="GuideTo"||((a.Role=="GuideTo")!=label.Rule.Contains("SIGNED")))continue;
                foreach(var start in a.Owners)foreach(var end in b.Owners)
                {
                    var path=NamedRoadNetwork.MandatoryCorridor(graph,start,end,double.PositiveInfinity,true);
                    if(path?.Contains(id)==true)paths.Add(path);
                }
            }
            return paths.Count>0&&paths.All(path=>path.Any(p=>actualIndex[p].Identities.Any(l=>l.Kind==label.Kind&&l.Identity!=label.Identity&&
                l.Rule=="BRACKETED_MANDATORY_NETWORK_CORRIDOR"&&l.SourceIds.Select(s=>sourceIndex[s].ObjectId).Distinct().Count()>=2)));
        }
        var unjustified=regressions.Where(id=>expected[id].Identities.Any(label=>!actualIndex[id].Identities.Any(now=>SameLabel(label,now))&&!JustifiedRemoval(id,label))).ToArray();
        bool Reach(string start,string end,string? excluded)
        {
            if(start==excluded||end==excluded)return false;
            var seen=new HashSet<string>{start};var pending=new Stack<string>();pending.Push(start);
            while(pending.TryPop(out var p)){if(p==end)return true;foreach(var next in graph.Next(p))if(next!=excluded&&seen.Add(next))pending.Push(next);}
            return false;
        }
        int independentlyChecked=0;
        foreach(var p in additions.GroupBy(p=>p.Identities[0].Identity).SelectMany(g=>g.Take(2)).Take(64))
        foreach(var label in p.Identities)
        {
            if(!label.Rule.StartsWith("BRACKETED_")||label.SourceIds.Length<2)throw new Exception("New road name lacks independent bracketing proof");
            bool proved=false;
            foreach(var a in label.SourceIds.Select(s=>sourceIndex[s]))foreach(var b in label.SourceIds.Select(s=>sourceIndex[s]))
            {
                if(a.ObjectId==b.ObjectId)continue;
                foreach(var start in a.Owners)foreach(var end in b.Owners)
                    if(Reach(start,end,null)&&!Reach(start,end,p.PortionId)){proved=true;break;}
            }
            if(!proved)throw new Exception("New name fails independent vertex-removal test: "+p.PortionId);
            independentlyChecked++;
        }
        var explanations=regressions.Take(3).Select(id=>new{portion=id,paths=expected[id].Identities.SelectMany(label=>
            label.SourceIds.SelectMany(a=>label.SourceIds.Where(b=>b!=a).SelectMany(b=>sourceIndex[a].Owners.SelectMany(start=>sourceIndex[b].Owners.Select(end=>
            {
                var path=NamedRoadNetwork.MandatoryCorridor(graph,start,end,double.PositiveInfinity,true);
                return new{source=a,target=b,start,end,length=path?.Length,contains=path?.Contains(id),
                    conflicts=path?.Where(p=>actualIndex[p].Identities.Any(l=>l.Kind==label.Kind&&l.Identity!=label.Identity)).Take(3).Select(p=>actualIndex[p]).ToArray()};
            }))))).ToArray()}).ToArray();
        Directory.CreateDirectory(output);File.WriteAllText(Path.Combine(output,"actual-names.json"),JsonSerializer.Serialize(actual,LiveProtocol.Json));
        File.WriteAllText(Path.Combine(output,"network-equivalence.json"),JsonSerializer.Serialize(new{status=unjustified.Length==0?"PASS":"FAIL",portions=actual.Length,sources=data.Sources.Length,seconds=clock.Elapsed.TotalSeconds,unjustifiedRegressions=unjustified,conflictingOldAttributionsRemoved=regressions.Length,independentlyChecked,newlyNamed=additions.Length,differences,
            explanations,examples=actual.Where(p=>differences.Contains(p.PortionId)).Take(10).Select(p=>new{before=expected[p.PortionId],after=p})},LiveProtocol.Json));
        if(unjustified.Length>0)throw new Exception("Expanded reachability lost an uncontradicted road identity");
        Console.WriteLine($"PASS: {actual.Length} portions checked, {additions.Length} newly named, {regressions.Length} conflicting old attributions removed, {independentlyChecked} independent graph proofs; solve={clock.Elapsed.TotalSeconds:0.000}s.");
    }
}
