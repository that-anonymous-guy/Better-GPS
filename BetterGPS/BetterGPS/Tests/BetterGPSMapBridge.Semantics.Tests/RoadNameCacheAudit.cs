using BetterGPSNavigation;
using System.Text.Json;
static class RoadNameCacheAudit
{
    public static void Batch(string directory,string output,string? baseline=null)
    {
        Directory.CreateDirectory(output);var rows=new List<object>();int lost=0;
        foreach(var path in Directory.GetFiles(directory,"*network-region*.json").Order())
        {
            var text=File.ReadAllText(path);using(var doc=JsonDocument.Parse(text))
                if(doc.RootElement.TryGetProperty("payload",out var payload)||doc.RootElement.TryGetProperty("Payload",out payload))text=payload.GetString()!;
            var data=JsonSerializer.Deserialize<NamedNetworkSnapshot>(text,LiveProtocol.Json)!;
            var graph=new DirectedRoadNetworkGraph(data.Region,data.Portions,data.Connections,data.Errors);
            var watch=System.Diagnostics.Stopwatch.StartNew();var after=NamedRoadNetwork.Solve(graph,data.Sources);watch.Stop();
            var expected=baseline is null?data.Names:JsonSerializer.Deserialize<NamedNetworkPortion[]>(File.ReadAllText(Path.Combine(baseline,"names",Path.GetFileName(path))),LiveProtocol.Json)!;
            var previous=expected.ToDictionary(v=>v.PortionId);
            var losses=after.Where(v=>previous[v.PortionId].Status=="RESOLVED"&&previous[v.PortionId].Identities.Any(i=>
                !v.Identities.Any(next=>next.Identity==i.Identity&&next.Kind==i.Kind&&next.Cardinal==i.Cardinal)))
                .Select(v=>new{v.PortionId,before=previous[v.PortionId],after=v}).ToArray();lost+=losses.Length;
            var updated=data with{Names=after};NamedNetworkValidation.Validate(updated,graph);
            Directory.CreateDirectory(Path.Combine(output,"names"));File.WriteAllText(Path.Combine(output,"names",Path.GetFileName(path)),JsonSerializer.Serialize(after,LiveProtocol.Json));
            var row=new{file=Path.GetFileName(path),portions=data.Portions.Length,sources=data.Sources.Length,
                before=data.Names.Count(v=>v.Status=="RESOLVED"),after=after.Count(v=>v.Status=="RESOLVED"),ms=watch.Elapsed.TotalMilliseconds,losses};rows.Add(row);
            Console.WriteLine($"{rows.Count}: {row.before}->{row.after}/{row.portions}; {row.ms:0} ms; lost={losses.Length}");
        }
        File.WriteAllText(Path.Combine(output,"regression.json"),JsonSerializer.Serialize(rows,LiveProtocol.Json));
        if(lost>0)throw new Exception($"Lost {lost} existing named portions; inspect regression.json");
        Console.WriteLine($"PASS: {rows.Count} saved network regions, no lost identities/cardinals.");
    }
    public static void Run(string networkPath,string routePath,string output)
    {
        var data=JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(networkPath),LiveProtocol.Json)!;
        var route=JsonSerializer.Deserialize<LiveRoute>(File.ReadAllText(routePath),LiveProtocol.Json)!;
        var graph=new DirectedRoadNetworkGraph(data.Region,data.Portions,data.Connections,data.Errors);
        var diagnostics=data.Sources.Where(s=>s.Role=="MapAnnotation"&&s.AssociationStatus=="AMBIGUOUS_OWNERS").Select(s=>new{
            s.Id,s.Identity,owners=s.Owners.Select(owner=>new{owner,corroborations=data.Sources.Where(o=>o.ObjectId!=s.ObjectId&&o.Identity==s.Identity&&
                (o.Role=="GuideTo"&&o.AssociationStatus=="DIRECTIONAL_BRANCH_TARGET"?RoadNameContinuity.SignedCorridor(graph,o).Contains(owner):
                o.Role=="OwnRoute"&&o.Source!="DISTANCE_BOARD_REFERENCE"&&RoadIdentityRules.ReferenceOnly(o.Raw)&&o.Owners.Any(id=>RoadNameContinuity.Corridor(graph,id).Contains(owner))))
                .Select(o=>new{o.Id,o.Source,o.Role,o.Raw}).ToArray()}).ToArray()}).ToArray();
        var updated=data with{Names=NamedRoadNetwork.Solve(graph,data.Sources)};NamedNetworkValidation.Validate(updated,graph);
        var joined=NamedRoadNetwork.Join(updated,route.RoadIdentities!.Spans,route);
        var replay=NamedRoadNetwork.AttachSignTargets(updated,route with{RoadIdentities=joined});
        RoadIdentityValidation.Validate(replay,joined);
        var packets=new List<object>();
        foreach(int index in new[]{0,8,14,16,21,22,23,39,40,65,83,87,92}.Where(i=>route.Edges.Any(e=>e.SegmentIndex==i)))
        {
            var edge=replay.Edges.First(e=>e.SegmentIndex==index);var engine=new LiveGuidanceEngine();
            engine.Accept(new(1,"Hello","regression",0,0,0));
            engine.Accept(new(2,"RouteSnapshot","regression",1,0,1,replay));
            var point=new LivePoint((edge.Start.X+edge.End.X)/2,(edge.Start.Y+edge.End.Y)/2,(edge.Start.Z+edge.End.Z)/2);
            double heading=Math.Atan2(edge.End.Z-edge.Start.Z,edge.End.X-edge.Start.X)*180/Math.PI;
            var guidance=engine.Accept(new(2,"VehicleState","regression",2,100,1,Vehicle:new(point,heading,15,false,
                replay.NavigationRemainingAtOriginM-(edge.NavStartM+edge.NavEndM)/2)));
            var text=NativeBridgeCli.Describe(guidance);packets.Add(new{segment=index,guidance.Status,guidance.CurrentRoad,guidance.TargetRoad,guidance.ContinueToward,text});
            if(index==21&&replay.RouteId=="live_000009_F40217BE75A05892"&&!text.StartsWith("Continue toward I-880"))
                throw new Exception("Screenshot corridor still has no signed connector destination: "+text);
        }
        Directory.CreateDirectory(output);File.WriteAllText(Path.Combine(output,"audit.json"),JsonSerializer.Serialize(diagnostics,LiveProtocol.Json));
        File.WriteAllText(Path.Combine(output,"phone-packets.json"),JsonSerializer.Serialize(packets,LiveProtocol.Json));
        File.WriteAllText(Path.Combine(output,"route.json"),JsonSerializer.Serialize(route with{RoadIdentities=joined},LiveProtocol.Json));
        Console.WriteLine($"Resolved before={route.RoadIdentities.Spans.Count(s=>s.Status=="RESOLVED")} after={joined.Spans.Count(s=>s.Status=="RESOLVED")}/{joined.Spans.Length}");
    }
}
