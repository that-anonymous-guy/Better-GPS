using System.Numerics;

namespace BetterGPSNavigation;

internal static class NativeAccessRoads
{
    internal static int Lanes(NetworkPortion p,bool entry)
    {
        var points=new List<Vector3>();
        foreach(var path in p.Paths)
        {
            var point=NetworkPortion.V(entry?path[0]:path[^1]);
            if(points.All(v=>Vector3.Distance(v,point)>.5))points.Add(point);
        }
        return points.Count;
    }
    internal static HashSet<string> Find(DirectedRoadNetworkGraph graph)
    {
        var result=new HashSet<string>();
        foreach(bool forward in new[]{true,false})
        foreach(var p in graph.Portions.Where(p=>p.Kind=="ROAD"&&Lanes(p,!forward)>=2))
        {
            var branches=(forward?graph.Next(p.Id):graph.Previous(p.Id)).Select(graph.Get).Where(p=>p.Kind=="PREFAB").ToArray();
            if(branches.Length<2||branches.Select(b=>b.ItemUid).Distinct().Count()!=1||!branches.Any(b=>Lanes(b,forward)>=2))continue;
            foreach(var branch in branches.Where(b=>Lanes(b,forward)==1))
            {
                var a=p.Paths[0];var b=branch.Paths[0];
                var from=forward?NetworkPortion.V(a[^1])-NetworkPortion.V(a[Math.Max(0,a.Length-4)]):
                    NetworkPortion.V(a[0])-NetworkPortion.V(a[Math.Min(a.Length-1,3)]);
                var to=forward?NetworkPortion.V(b[^1])-NetworkPortion.V(b[0]):NetworkPortion.V(b[0])-NetworkPortion.V(b[^1]);from.Y=to.Y=0;
                if(from.LengthSquared()<.001||to.LengthSquared()<.001||Vector3.Dot(Vector3.Normalize(from),Vector3.Normalize(to))<.9)continue;
                var pending=new Queue<(string Id,double Distance)>();pending.Enqueue((branch.Id,0));var seen=new HashSet<string>();
                while(pending.TryDequeue(out var item))
                {
                    if(!seen.Add(item.Id)||item.Distance>1500)continue;
                    var current=graph.Get(item.Id);
                    if(current.Kind=="ROAD"&&Lanes(current,forward)>=2)continue;
                    result.Add(current.Id);
                    foreach(var next in forward?graph.Next(current.Id):graph.Previous(current.Id))pending.Enqueue((next,item.Distance+current.LengthM));
                }
            }
        }
        return result;
    }
}
