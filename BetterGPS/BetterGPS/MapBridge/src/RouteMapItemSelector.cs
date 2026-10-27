using TruckLib.ScsMap;
namespace BetterGPSNavigation;

internal static class RouteMapItemSelector
{
    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<ProbeRoute,Dictionary<int,ulong[]>> hints=new();
    internal static MapItem? Resolve(Map map,INode a,INode b,ProbeRoute route,int segment)
    {
        var items=new[]{a.ForwardItem?.Uid??0,a.BackwardItem?.Uid??0}
            .Intersect(new[]{b.ForwardItem?.Uid??0,b.BackwardItem?.Uid??0}).Where(uid=>uid!=0).ToArray();
        var native=hints.GetValue(route,Build).GetValueOrDefault(segment)??[];
        // A repeated boundary node belongs to both its road and its prefab. A
        // native loop through that prefab must not become a straight road segment.
        var selected=items.Where(native.Contains).Select(uid=>map.MapItems.GetValueOrDefault(uid))
            .OfType<Prefab>().Where(p=>p.Nodes.Any(n=>n.Uid==a.Uid)&&p.Nodes.Any(n=>n.Uid==b.Uid)).ToArray();
        if(selected.Length==1)return selected[0];
        if(selected.Length>1)throw new InvalidDataException("Ambiguous native prefab between route boundaries.");
        return items.Select(uid=>map.MapItems.GetValueOrDefault(uid)).FirstOrDefault(item=>item is not null);
    }
    private static Dictionary<int,ulong[]> Build(ProbeRoute route)
    {
        var bySegment=new Dictionary<int,HashSet<ulong>>();int segment=-1;
        foreach(var row in route.Logical)
        {
            if(row.Type==0){segment++;continue;}
            if(row.Type!=1||segment<0)continue;
            if(!bySegment.TryGetValue(segment,out var ids))bySegment[segment]=ids=[];
            ids.Add(row.ItemUid);
        }
        return bySegment.ToDictionary(pair=>pair.Key,pair=>pair.Value.ToArray());
    }
}
