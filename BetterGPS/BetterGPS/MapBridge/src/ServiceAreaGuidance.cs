using System.Numerics;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;
internal static class ServiceAreaGuidance
{
    internal static readonly HashSet<string> Kinds=["WEIGH_STATION","TOLL","FERRY_TERMINAL","DEPOT","FUEL_STOP","REST_AREA"];
    internal static string? Kind(string path)=>path.Contains("weigh_station",StringComparison.OrdinalIgnoreCase)?"WEIGH_STATION":
        path.Contains("toll",StringComparison.OrdinalIgnoreCase)?"TOLL":path.Contains("/ferry/",StringComparison.OrdinalIgnoreCase)?"FERRY_TERMINAL":
        path.Contains("/depots/",StringComparison.OrdinalIgnoreCase)?"DEPOT":path.Contains("/gas/",StringComparison.OrdinalIgnoreCase)?"FUEL_STOP":
        path.Contains("rest_stop",StringComparison.OrdinalIgnoreCase)||path.Contains("truck_stop",StringComparison.OrdinalIgnoreCase)?"REST_AREA":null;
    internal static LiveServiceArea[] Build(Map map,DirectedRoadNetworkGraph graph,PpdResolver resolver,LiveRoute route)
    {
        var portions=graph.Portions.GroupBy(p=>p.ItemUid).ToDictionary(g=>g.Key,g=>g.ToArray());
        var used=route.Edges.Select(e=>e.ItemUid).ToHashSet();var result=new List<LiveServiceArea>();
        foreach(var p in map.MapItems.Values.OfType<Prefab>())
        {
            if(!resolver.TryGetPath(p.Model.ToString(),out var path)||Kind(path) is not {} kind||p.Nodes.Count==0)continue;
            var id=p.Uid.ToString("X16");if(!portions.TryGetValue(id,out var geometry))continue;
            var points=geometry.SelectMany(g=>g.Paths).SelectMany(g=>g).Select(NetworkPortion.V).ToArray();
            if(points.Length==0)points=p.Nodes.Select(n=>n.Position).ToArray();
            var minX=points.Min(v=>v.X)-10;var maxX=points.Max(v=>v.X)+10;
            var minZ=points.Min(v=>v.Z)-10;var maxZ=points.Max(v=>v.Z)+10;
            if(maxX-minX>520||maxZ-minZ>520)continue;
            result.Add(new(id,kind,minX,maxX,points.Min(v=>v.Y)-3,points.Max(v=>v.Y)+6,minZ,maxZ,used.Contains(id)));
            if(result.Count==2048)break;
        }
        return result.ToArray();
    }
    internal static LiveServiceArea? Locate(LiveRoute route,LiveVehicle vehicle)=>route.ServiceAreas?
        .Where(a=>vehicle.Position.X>=a.MinX&&vehicle.Position.X<=a.MaxX&&vehicle.Position.Z>=a.MinZ&&vehicle.Position.Z<=a.MaxZ&&
            vehicle.Position.Y>=a.MinY&&vehicle.Position.Y<=a.MaxY)
        .OrderBy(a=>(a.MaxX-a.MinX)*(a.MaxZ-a.MinZ)).FirstOrDefault();
    internal static string Instruction(string kind)=>kind switch
    {
        "WEIGH_STATION"=>"Weigh station — follow the game's entry or bypass instruction, then rejoin the planned route",
        "TOLL"=>"Toll plaza — follow the available passage, then continue on the planned route",
        "FERRY_TERMINAL"=>"Ferry terminal — follow the game's crossing instructions; guidance resumes after disembarking",
        "DEPOT"=>"Delivery area — follow the loading or parking marker, then join the planned route",
        "FUEL_STOP"=>"Fuel stop — rejoin the planned route when finished",
        _=>"Rest area — rejoin the planned route when finished"
    };
}
