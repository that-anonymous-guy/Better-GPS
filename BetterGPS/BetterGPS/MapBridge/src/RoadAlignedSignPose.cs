using System.Numerics;

namespace BetterGPSNavigation;

internal static class RoadAlignedSignPose
{
    // SCS FollowRoadDir aligns the sign with the closest road lane. Keep the
    // facing direction of the instance, then rotate the complete attachment.
    internal static (Vector3 Position,Vector3 Normal)? Align(DirectedRoadNetworkGraph graph,Vector3 origin,Vector3 position,Vector3 normal)
    {
        var horizontal=new Vector3(normal.X,0,normal.Z);
        if(horizontal.LengthSquared()<.8)return null;
        horizontal=Vector3.Normalize(horizontal);
        var hits=new List<(string Item,double Distance,Vector3 Direction)>();
        foreach(var p in graph.Query(origin,80).Where(p=>p.Kind=="ROAD"&&p.Blockers.All(b=>b=="LANE_WIDTH_4_5M_ASSUMED")))
        foreach(var path in p.Paths)
        for(int i=1;i<path.Length;i++)
        {
            var a=NetworkPortion.V(path[i-1]);var b=NetworkPortion.V(path[i]);var d=b-a;d.Y=0;
            if(d.LengthSquared()<.001)continue;
            var delta=origin-a;delta.Y=0;
            var t=Vector3.Dot(delta,d)/d.LengthSquared();if(t<0||t>1)continue;
            var q=Vector3.Lerp(a,b,t);var direction=Vector3.Normalize(d);
            if(Vector3.Dot(horizontal,direction)>-.5||position.Y-q.Y is < -1 or >15)continue;
            hits.Add((p.ItemUid,Vector3.Distance(origin,q),direction));
        }
        var ranked=hits.GroupBy(h=>h.Item).Select(g=>g.OrderBy(h=>h.Distance).First()).OrderBy(h=>h.Distance).ToArray();
        if(ranked.Length==0||ranked[0].Distance>30||ranked.Skip(1).Any(h=>h.Distance<ranked[0].Distance+3))return null;
        var desired=-ranked[0].Direction;
        var yaw=MathF.Atan2(desired.X,desired.Z)-MathF.Atan2(horizontal.X,horizontal.Z);
        var rotation=Quaternion.CreateFromAxisAngle(Vector3.UnitY,yaw);
        return (origin+Vector3.Transform(position-origin,rotation),Vector3.Normalize(Vector3.Transform(normal,rotation)));
    }
}
