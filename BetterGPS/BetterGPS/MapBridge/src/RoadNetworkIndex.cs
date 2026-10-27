using System.Numerics;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

/// <summary>All neighbouring roads, including non-GPS alternatives. No ownership is selected from GPS agreement.</summary>
internal sealed class RoadNetworkIndex
{
    private const double Cell=500;
    private readonly Dictionary<(int,int),List<Road>> cells=[];
    private readonly Dictionary<ulong,Vector3[]> geometry=[];
    private readonly Dictionary<string,Road> byUid=[];
    public RoadNetworkIndex(Map map)
    {
        foreach(var r in map.MapItems.Values.OfType<Road>())
        {
            if(r.Node is null||r.ForwardNode is null)continue;
            byUid[r.Uid.ToString("X16")]=r;
            var p=Enumerable.Range(0,9).Select(i=>r.InterpolateCurve(i/8f).Position).ToArray();
            if(p.Any(v=>!RoadLaneGeometryResolver.Finite(v)))continue;
            // Conservative padding between spline samples; candidate projection uses a finer polyline.
            var margin=Math.Max(10,r.Length/4);
            int x0=Bin(p.Min(v=>v.X)-margin),x1=Bin(p.Max(v=>v.X)+margin),z0=Bin(p.Min(v=>v.Z)-margin),z1=Bin(p.Max(v=>v.Z)+margin);
            for(int x=x0;x<=x1;x++)for(int z=z0;z<=z1;z++)
            {if(!cells.TryGetValue((x,z),out var list))cells[(x,z)]=list=[];list.Add(r);}
        }
    }
    private static int Bin(double x)=>(int)Math.Floor(x/Cell);
    public RoadOwnershipCandidate[] Query(Vector3 position,double radius=180)
    {
        var seen=new HashSet<ulong>();var result=new List<RoadOwnershipCandidate>();
        for(int x=Bin(position.X-radius);x<=Bin(position.X+radius);x++)
        for(int z=Bin(position.Z-radius);z<=Bin(position.Z+radius);z++)
        {
            if(!cells.TryGetValue((x,z),out var roads))continue;
            foreach(var road in roads)
            {
                if(!seen.Add(road.Uid))continue;
                if(!geometry.TryGetValue(road.Uid,out var points))
                {int steps=Math.Clamp((int)Math.Ceiling(road.Length/5),2,4000);geometry[road.Uid]=points=Enumerable.Range(0,steps+1).Select(i=>road.InterpolateCurve(i/(float)steps).Position).ToArray();}
                var (distance,height)=Project(points,position);
                if(distance<=radius)result.Add(new(road.Uid.ToString("X16"),distance,height,"ROAD_SPLINE; height diagnostic, no UI/sign-height assumption"));
            }
        }
        var ordered=result.OrderBy(c=>c.DistanceM).ThenBy(c=>c.ItemUid,StringComparer.Ordinal).ToArray();
        if(ordered.Length>0)
        {
            var root=byUid[ordered[0].ItemUid];
            ordered=ordered.Select(c=>c.ItemUid!=ordered[0].ItemUid&&Contiguous(root,byUid[c.ItemUid])?
                c with{Reason="CONNECTED_ROAD_CHAIN:"+ordered[0].ItemUid}:c).ToArray();
        }
        return ordered;
    }
    private static bool Contiguous(Road a,Road b)
    {
        bool common=a.Node.Uid==b.Node.Uid||a.Node.Uid==b.ForwardNode.Uid||a.ForwardNode.Uid==b.Node.Uid||a.ForwardNode.Uid==b.ForwardNode.Uid;
        if(!common)return false;
        float ta=a.Node.Uid==b.Node.Uid||a.Node.Uid==b.ForwardNode.Uid?0:1;
        float tb=b.Node.Uid==a.Node.Uid||b.Node.Uid==a.ForwardNode.Uid?0:1;
        var u=a.InterpolateCurve(Math.Clamp(ta+.01f,0,1)).Position-a.InterpolateCurve(Math.Clamp(ta-.01f,0,1)).Position;
        var v=b.InterpolateCurve(Math.Clamp(tb+.01f,0,1)).Position-b.InterpolateCurve(Math.Clamp(tb-.01f,0,1)).Position;
        u.Y=v.Y=0;if(u.LengthSquared()<1e-8||v.LengthSquared()<1e-8)return false;
        return Math.Abs(Vector3.Dot(Vector3.Normalize(u),Vector3.Normalize(v)))>=Math.Cos(20*Math.PI/180);
    }
    internal static (double Distance,double Height) Project(IReadOnlyList<Vector3> points,Vector3 p)
    {
        double best=double.PositiveInfinity,height=0;
        for(int i=1;i<points.Count;i++)
        {
            var a=points[i-1];var b=points[i];var v=new Vector2(b.X-a.X,b.Z-a.Z);
            if(v.LengthSquared()<1e-8)continue;
            var t=Math.Clamp(Vector2.Dot(new(p.X-a.X,p.Z-a.Z),v)/v.LengthSquared(),0,1);
            var q=Vector3.Lerp(a,b,t);double d=Vector2.Distance(new(p.X,p.Z),new(q.X,q.Z));
            if(d<best){best=d;height=Math.Abs(p.Y-q.Y);}
        }
        return(best,height);
    }
    internal static (string Status,string Owner,string Confidence) Associate(RoadOwnershipCandidate[] candidates)
    {
        if(candidates.Length==0)return("NO_ROAD","","NONE");
        var best=candidates[0];
        // Adjacent roads meeting at their endpoints are deliberately ambiguous here.
        if(best.DistanceM>75)return("DISTANT",best.ItemUid,"LOW");
        if(candidates.Skip(1).Any(c=>c.DistanceM<=best.DistanceM+15&&c.Reason!="CONNECTED_ROAD_CHAIN:"+best.ItemUid))return("AMBIGUOUS_ROADS","","LOW");
        return("UNIQUE_ROAD_CANDIDATE",best.ItemUid,"MEDIUM");
    }
}
