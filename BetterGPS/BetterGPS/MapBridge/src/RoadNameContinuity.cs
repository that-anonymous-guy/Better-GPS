using System.Numerics;

namespace BetterGPSNavigation;

// A local continuation is reciprocal: the selected exit must also select this
// approach. This lets confirmations cross a merge without spilling onto ramps.
internal static class RoadNameContinuity
{
    internal static string[] Corridor(DirectedRoadNetworkGraph graph,string start,bool forwardOnly=false,Func<string,bool>? canEnter=null,bool followShallowExits=true)
    {
        if(canEnter is null&&graph.NameContinuityCorridors.TryGetValue((start,forwardOnly,followShallowExits),out var cached))return cached;
        var result=new HashSet<string>{start};
        foreach(bool forward in forwardOnly?new[]{true}:new[]{true,false})
        {
            var current=start;var physical=new HashSet<string>{graph.Get(start).ItemUid};
            while(true)
            {
                var choices=forward?graph.Next(current):graph.Previous(current);
                var next=Choose(graph,current,choices,forward,followShallowExits);
                if(next is null)break;
                var reverseChoices=forward?graph.Previous(next):graph.Next(next);
                if(Choose(graph,next,reverseChoices,!forward,followShallowExits)!=current)break;
                var p=graph.Get(next);
                if(!physical.Add(p.ItemUid)||canEnter is not null&&!canEnter(p.Id))break;
                result.Add(next);current=next;
            }
        }
        var corridor=result.Order(StringComparer.Ordinal).ToArray();
        if(canEnter is null)graph.NameContinuityCorridors[(start,forwardOnly,followShallowExits)]=corridor;
        return corridor;
    }
    internal static string[] SignedCorridor(DirectedRoadNetworkGraph graph,NetworkNameSource source,Func<string,bool>? canEnter=null)
    {
        if(canEnter is null&&graph.SignedContinuityCorridors.TryGetValue(source.Id,out var cached))return cached;
        if(source.Role!="GuideTo"||source.AssociationStatus!="DIRECTIONAL_BRANCH_TARGET"||source.Owners.Length!=1||
            !RoadIdentityRules.ReferenceOnly(source.Raw))return [];
        var branch=source.Owners[0];var next=graph.Next(branch);
        if(next.Length!=1||graph.Get(next[0]).Kind!="ROAD")return [branch];
        var result=new HashSet<string>{branch};var current=branch;
        var items=new HashSet<string>{graph.Get(branch).ItemUid};
        // A signed access may merge with another access to the same road.
        // Follow a sole connection or a distinct through carriageway. Do not
        // demand that an access also be the dominant approach at its merge.
        while(true)
        {
            next=graph.Next(current);
            // The named branch can itself turn sharply. Its sole native road
            // connection is the signed access, not a through-road inference.
            var continuation=current==branch&&next.Length==1?next[0]:Choose(graph,current,next,true,source.Kind=="RouteReference");
            if(continuation is null)break;
            var p=graph.Get(continuation);
            if(p.Blockers.Any(b=>b!="LANE_WIDTH_4_5M_ASSUMED")||!items.Add(p.ItemUid)||canEnter is not null&&!canEnter(p.Id))break;
            result.Add(p.Id);current=p.Id;
        }
        var corridor=result.Order(StringComparer.Ordinal).ToArray();
        if(canEnter is null)graph.SignedContinuityCorridors[source.Id]=corridor;
        return corridor;
    }
    private static string? Choose(DirectedRoadNetworkGraph graph,string current,string[] choices,bool forward,bool followShallowExits=true)
    {
        var key=(current,forward,followShallowExits);
        if(graph.ContinuityChoices.TryGetValue(key,out var cached))return cached;
        return graph.ContinuityChoices[key]=ComputeChoice(graph,current,choices,forward,followShallowExits);
    }
    private static string? ComputeChoice(DirectedRoadNetworkGraph graph,string current,string[] choices,bool forward,bool followShallowExits)
    {
        if(choices.Length==0)return null;
        var p=graph.Get(current);
        if(p.Blockers.Any(b=>b!="LANE_WIDTH_4_5M_ASSUMED"))return null;
        var ranked=choices.Select(id=>(Id:id,Angle:Angle(forward?p:graph.Get(id),forward?graph.Get(id):p,
            choices.Length>1&&!forward,choices.Length>1&&forward)))
            .Where(c=>graph.Get(c.Id).Blockers.All(b=>b=="LANE_WIDTH_4_5M_ASSUMED")).OrderBy(c=>c.Angle).ToArray();
        if(ranked.Length!=choices.Length||ranked.Length==0)return null;
        // At a sole native port the road may curve freely. At a junction we
        // require a distinct straight continuation; a near-parallel fork stops.
        if(ranked.Length==1)return ranked[0].Angle<35?ranked[0].Id:null;
        // A shallow single-lane exit is not a competing through carriageway
        // when the near-straight branch preserves multiple distinct native lanes.
        // Count port positions rather than paths: duplicate lane curves do not
        // create extra evidence. The reverse check in Corridor still applies.
        int PortLanes(NetworkPortion portion,bool entry)
        {
            var points=new List<Vector3>();
            foreach(var path in portion.Paths)
            {
                var point=NetworkPortion.V(entry?path[0]:path[^1]);
                if(points.All(v=>Vector3.Distance(v,point)>.5))points.Add(point);
            }
            return points.Count;
        }
        var lanes=PortLanes(graph.Get(ranked[0].Id),forward);
        if(followShallowExits&&ranked[0].Angle<=5&&lanes>=2&&lanes>=PortLanes(p,!forward)-1&&
            ranked.Skip(1).All(c=>PortLanes(graph.Get(c.Id),forward)<lanes))return ranked[0].Id;
        return ranked[0].Angle<=25&&ranked[1].Angle-ranked[0].Angle>=25?ranked[0].Id:null;
    }
    private static double Angle(NetworkPortion from,NetworkPortion to,bool wholeFrom,bool wholeTo)
    {
        // Prefab entry tangents are shared by branches. Use the whole prefab
        // displacement to distinguish a straight carriageway from a turn.
        Vector3 Direction(NetworkPortion p,bool entry,float[][] path,bool whole)
        {
            var d=whole&&p.Kind=="PREFAB"?NetworkPortion.V(path[^1])-NetworkPortion.V(path[0]):entry?
                NetworkPortion.V(path[Math.Min(3,path.Length-1)])-NetworkPortion.V(path[0]):
                NetworkPortion.V(path[^1])-NetworkPortion.V(path[Math.Max(0,path.Length-4)]);
            d.Y=0;return d.LengthSquared()>.001?Vector3.Normalize(d):Vector3.Zero;
        }
        return from.Paths.Max(a=>to.Paths.Max(b=>Math.Acos(Math.Clamp(Vector3.Dot(Direction(from,false,a,wholeFrom),Direction(to,true,b,wholeTo)),-1,1))*180/Math.PI));
    }
}
