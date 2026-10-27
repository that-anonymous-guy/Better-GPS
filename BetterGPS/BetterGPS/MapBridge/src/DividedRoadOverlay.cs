using System.Numerics;
namespace BetterGPSNavigation;

// A map shield placed in the median can describe two opposite one-way
// carriageways. Neither the selected GPS route nor a guessed cardinal is used.
internal static class DividedRoadOverlay
{
    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<DirectedRoadNetworkGraph,Dictionary<string,bool>> checks=new();
    internal static bool CoversPair(DirectedRoadNetworkGraph graph,NetworkNameSource source)
    {
        var cache=checks.GetValue(graph,_=>new());
        if(cache.TryGetValue(source.Id,out var known))return known;
        return cache[source.Id]=Compute(graph,source);
    }
    private static bool Compute(DirectedRoadNetworkGraph graph,NetworkNameSource source)
    {
        if(source.Source!="ROAD_OVERLAY"||source.Role!="MapAnnotation"||source.Kind!="RouteReference"||
            source.AssociationStatus!="AMBIGUOUS_OWNERS"||source.ReadingConfidence is not ("HIGH" or "MEDIUM")||
            source.Owners.Length!=2||!RoadIdentityRules.ReferenceOnly(source.Raw))return false;
        var roads=source.Owners.Select(graph.Get).ToArray();
        if(roads[0].ItemUid==roads[1].ItemUid||roads.Any(p=>p.Kind!="ROAD"||p.Blockers.Any(b=>b!="LANE_WIDTH_4_5M_ASSUMED")||
            NativeAccessRoads.Lanes(p,true)<2||NativeAccessRoads.Lanes(p,false)<2||
            graph.Portions.Count(other=>other.ItemUid==p.ItemUid)!=1))return false;
        var position=NetworkPortion.V(source.Position);
        var a=Section(roads[0],position);var b=Section(roads[1],position);
        if(a is null||b is null||Math.Abs(a.Value.Point.Y-b.Value.Point.Y)>2)return false;
        var delta=b.Value.Point-a.Value.Point;delta.Y=0;var gap=delta.Length();
        if(gap<4||gap>60||Vector3.Dot(a.Value.Direction,b.Value.Direction)>-.97f)return false;
        // The icon must lie in the median/cross-section, not next to one road
        // with an unrelated parallel road somewhere inside the search radius.
        if(Math.Abs(Vector3.Dot(delta,a.Value.Direction))>4||Math.Abs(Vector3.Dot(delta,b.Value.Direction))>4)return false;
        var fromA=position-a.Value.Point;fromA.Y=0;var fromB=position-b.Value.Point;fromB.Y=0;
        if(Vector3.Dot(fromA,fromB)>0||fromA.Length()>gap||fromB.Length()>gap)return false;
        return Math.Abs(Vector3.Dot(fromA,a.Value.Direction))<=4&&Math.Abs(Vector3.Dot(fromB,b.Value.Direction))<=4;
    }
    private static (Vector3 Point,Vector3 Direction)? Section(NetworkPortion portion,Vector3 position)
    {
        var points=new List<Vector3>();var directions=new List<Vector3>();
        foreach(var path in portion.Paths)
        {
            float best=float.PositiveInfinity;Vector3 point=default,direction=default;float location=0;
            for(int i=1;i<path.Length;i++)
            {
                var start=NetworkPortion.V(path[i-1]);var end=NetworkPortion.V(path[i]);var delta=end-start;delta.Y=0;
                var offset=position-start;offset.Y=0;if(delta.LengthSquared()<.001)continue;
                float t=Math.Clamp(Vector3.Dot(offset,delta)/delta.LengthSquared(),0,1);var projected=Vector3.Lerp(start,end,t);
                var separation=position-projected;separation.Y=0;float distance=separation.LengthSquared();
                if(distance<best){best=distance;point=projected;direction=Vector3.Normalize(delta);location=(i-1+t)/(path.Length-1);}
            }
            // An endpoint-only match provides no evidence of a shared section.
            if(!float.IsFinite(best)||location<.05||location>.95)return null;
            points.Add(point);directions.Add(direction);
        }
        if(points.Count<2||directions.Any(d=>Vector3.Dot(d,directions[0])<.97f))return null;
        return (points.Aggregate(Vector3.Zero,(sum,p)=>sum+p)/points.Count,
            Vector3.Normalize(directions.Aggregate(Vector3.Zero,(sum,p)=>sum+p)));
    }
}
