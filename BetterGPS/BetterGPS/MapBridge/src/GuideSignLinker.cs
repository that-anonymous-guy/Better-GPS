using System.Numerics;

namespace BetterGPSNavigation;

internal static class GuideSignLinker
{
    internal static NetworkNameSource Associate(DirectedRoadNetworkGraph graph,NetworkNameSource source)
    {
        if(source.Arrow=="DOWN_LANE")return LaneOwner(graph,source);
        if(source.Normal is null||source.Arrow is not ("LEFT" or "RIGHT" or "UP" or "STRAIGHT" or "DIAGONAL_UP_LEFT" or "DIAGONAL_UP_RIGHT"))return source;
        var normal=NetworkPortion.V(source.Normal);normal.Y=0;
        if(normal.LengthSquared()<.8)return source;
        var forward=-Vector3.Normalize(normal);var position=NetworkPortion.V(source.Position);
        var choices=new List<(NetworkPortion[] Branches,double Distance)>();
        foreach(var entry in graph.Query(position,450).Where(p=>p.Kind=="PREFAB"&&p.Blockers.Length==0)
            .GroupBy(p=>(p.ItemUid,p.EntryNodeUid)).Where(g=>g.Select(p=>p.ExitNodeUid).Distinct().Count()>1))
        {
            var path=entry.First().Paths[0];var start=NetworkPortion.V(path[0]);
            var direction=Direction(path,true);var delta=start-position;delta.Y=0;
            double along=Vector3.Dot(delta,forward),lateral=(delta-forward*(float)along).Length();
            // A motorway sign can stand inside a long interchange, after its entry port.
            // Test the actual approach section rather than discarding it for a negative entry distance.
            var section=entry.SelectMany(p=>p.Paths).Select(path=>Section(path,position,forward)).OrderBy(s=>s.Distance).First();
            bool inside=along< -8&&section.Distance<=25&&section.Alignment>.85&&section.Height<=12&&
                entry.All(p=>p.Paths.Any(path=>{var s=Section(path,position,forward);return s.Distance<=25&&s.Alignment>.85&&s.Height<=12;}));
            if(!inside&&(along< -8||along>450||lateral>25||Math.Abs(start.Y-position.Y)>12||Vector3.Dot(direction,forward)<.85))continue;
            choices.Add((entry.ToArray(),Math.Sqrt(along*along+lateral*lateral)));
        }
        var ordered=choices.OrderBy(c=>c.Distance).ToArray();
        if(ordered.Length==0||ordered.Skip(1).Any(c=>c.Distance<ordered[0].Distance+15))return source;
        var matching=ordered[0].Branches.Where(p=>p.Paths.All(path=>
        {
            var incoming=Direction(path,true);var outgoing=Direction(path,false);
            var angle=Math.Atan2(incoming.X*outgoing.Z-incoming.Z*outgoing.X,Vector3.Dot(incoming,outgoing))*180/Math.PI;
            return source.Arrow switch{"LEFT"=>angle< -25&&angle> -150,"RIGHT"=>angle>25&&angle<150,
                "DIAGONAL_UP_LEFT"=>angle< -8&&angle> -85,"DIAGONAL_UP_RIGHT"=>angle>8&&angle<85,_=>Math.Abs(angle)<=25};
        })).ToArray();
        if(matching.Length!=1&&source.Arrow is "DIAGONAL_UP_LEFT" or "DIAGONAL_UP_RIGHT")
        {
            // A fork arrow is relative to the other branches. Both branches may bend left
            // while the signed right fork still occupies the right-hand carriageway.
            var right=new Vector3(-forward.Z,0,forward.X);
            var branches=ordered[0].Branches.Select(p=>(Portion:p,Side:p.Paths.Average(path=>
                Vector3.Dot(NetworkPortion.V(path[^1])-position,right)))).OrderBy(p=>p.Side).ToArray();
            if(branches.Length==2&&branches[1].Side-branches[0].Side>=4.5&&
                branches.All(p=>p.Portion.Paths.All(path=>Vector3.Dot(Direction(path,false),forward)>.5)))
                matching=[source.Arrow=="DIAGONAL_UP_RIGHT"?branches[^1].Portion:branches[0].Portion];
        }
        if(matching.Length!=1)return source;
        return source with{AssociationStatus="DIRECTIONAL_BRANCH_TARGET",Owners=[matching[0].Id]};
    }
    private static NetworkNameSource LaneOwner(DirectedRoadNetworkGraph graph,NetworkNameSource source)
    {
        if(source.Normal is null||source.LanePoints.Length==0)return source;
        var normal=NetworkPortion.V(source.Normal);normal.Y=0;
        if(normal.LengthSquared()<.8)return source;
        var forward=-Vector3.Normalize(normal);string? owner=null;
        foreach(var point in source.LanePoints)
        {
            if(!BoardModelResolver.Valid(point,3))return source;
            var position=NetworkPortion.V(point);
            var hits=graph.Query(position,30).Where(p=>p.Blockers.Length==0||p.Blockers.All(b=>b=="LANE_WIDTH_4_5M_ASSUMED"))
                .Select(p=>(Portion:p,Section:p.Paths.Select(path=>Section(path,position,forward))
                    .Where(s=>s.Alignment>.85&&s.Height<=12).OrderBy(s=>s.Distance).FirstOrDefault((Distance:double.PositiveInfinity,Alignment:0d,Height:0d))))
                .Where(h=>h.Section.Distance<=2.3).OrderBy(h=>h.Section.Distance).ToArray();
            if(hits.Length==0||hits.Skip(1).Any(h=>h.Section.Distance<hits[0].Section.Distance+1))return source;
            var selected=hits[0].Portion;
            var path=selected.Paths.OrderBy(p=>Section(p,position,forward).Distance).First();
            var target=LaneBranch(graph,selected,path);
            if(target is null||owner is not null&&owner!=target)return source;
            owner=target;
        }
        return owner is null?source:source with{AssociationStatus="DIRECTIONAL_BRANCH_TARGET",Owners=[owner]};
    }
    private static string? LaneBranch(DirectedRoadNetworkGraph graph,NetworkPortion portion,float[][] path)
    {
        if(portion.Kind=="PREFAB")return portion.Id;
        var visited=new HashSet<string>();double distance=0;
        while(distance<=1000&&visited.Add(portion.Id))
        {
            var end=NetworkPortion.V(path[^1]);var direction=Direction(path,false);
            var next=graph.Next(portion.Id).Select(graph.Get).ToArray();
            var hits=next.SelectMany(p=>p.Paths.Select(q=>(Portion:p,Path:q,Gap:Vector3.Distance(end,NetworkPortion.V(q[0])),
                Alignment:Vector3.Dot(direction,Direction(q,true)))))
                .Where(h=>h.Gap<=1.5&&h.Alignment>.7).OrderBy(h=>h.Gap).ToArray();
            if(hits.Length==0||hits.Any(h=>h.Portion.Id!=hits[0].Portion.Id&&h.Gap<hits[0].Gap+1))return null;
            var selected=hits[0];
            // A road under a lane arrow is the approach, not the signed exit.
            // Carry its physical lane to a unique branch before exposing a target.
            if(selected.Portion.Kind=="PREFAB"&&next.Where(p=>p.ItemUid==selected.Portion.ItemUid&&
                p.EntryNodeUid==selected.Portion.EntryNodeUid).Select(p=>p.ExitNodeUid).Distinct().Count()>1)return selected.Portion.Id;
            portion=selected.Portion;path=selected.Path;distance+=portion.LengthM;
        }
        return null;
    }
    private static (double Distance,double Alignment,double Height) Section(float[][] path,Vector3 position,Vector3 forward)
    {
        var best=(Distance:double.PositiveInfinity,Alignment:0d,Height:0d);
        for(int i=1;i<path.Length;i++)
        {
            var a=NetworkPortion.V(path[i-1]);var b=NetworkPortion.V(path[i]);var d=b-a;d.Y=0;
            if(d.LengthSquared()<1e-6)continue;
            var delta=position-a;delta.Y=0;
            var t=Math.Clamp(Vector3.Dot(delta,d)/d.LengthSquared(),0,1);var q=Vector3.Lerp(a,b,t);
            var gap=q-position;gap.Y=0;
            if(gap.Length()<best.Distance)best=(gap.Length(),Vector3.Dot(Vector3.Normalize(d),forward),Math.Abs(q.Y-position.Y));
        }
        return best;
    }
    private static Vector3 Direction(float[][] path,bool start)
    {
        var d=start?NetworkPortion.V(path[Math.Min(2,path.Length-1)])-NetworkPortion.V(path[0]):
            NetworkPortion.V(path[^1])-NetworkPortion.V(path[Math.Max(0,path.Length-3)]);
        d.Y=0;return d.LengthSquared()>1e-6?Vector3.Normalize(d):Vector3.Zero;
    }
}
