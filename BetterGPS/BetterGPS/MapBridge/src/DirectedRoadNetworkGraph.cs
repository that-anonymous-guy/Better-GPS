using System.Numerics;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal sealed record NetworkRegion(double MinX,double MaxX,double MinZ,double MaxZ)
{
    public bool Contains(Vector3 p)=>p.X>=MinX&&p.X<=MaxX&&p.Z>=MinZ&&p.Z<=MaxZ;
}
internal sealed record NetworkPortion(string Id,string ItemUid,string Kind,string EntryNodeUid,string ExitNodeUid,
    float[][][] Paths,string[] Blockers)
{
    public double LengthM=>Paths.Length==0?0:Paths.Min(p=>p.Zip(p.Skip(1),(a,b)=>(double)Vector3.Distance(V(a),V(b))).Sum());
    internal static Vector3 V(float[] p)=>new(p[0],p[1],p[2]);
}
internal sealed record NetworkConnection(string From,string To,string Status);

/// <summary>Local map extraction independent of the chosen route and of names. SCS Y up.</summary>
internal sealed class DirectedRoadNetworkGraph
{
    public NetworkRegion Region {get;}
    public NetworkPortion[] Portions {get;}
    public NetworkConnection[] Connections {get;}
    public string[] Errors {get;}
    private readonly Dictionary<string,NetworkPortion> byId;
    private readonly Dictionary<string,string[]> next;
    private readonly Dictionary<string,string[]> previous;
    internal readonly Dictionary<string,NetworkReachability> Reachability=[];
    internal readonly Dictionary<string,string[]> SignedContinuityCorridors=[];
    private readonly Dictionary<string,double> lengths;
    internal readonly Dictionary<string,int> Ordinals;
    internal readonly int[][] NextIndices;
    internal readonly int[][] PreviousIndices;
    internal readonly double[] IndexedLengths;
    internal readonly Dictionary<(string Start,string End,double Limit,bool SingleRoadPass),string[]?> MandatoryCorridors=[];
    internal readonly Dictionary<(string,bool,bool),string[]> NameContinuityCorridors=[];
    internal readonly Dictionary<(string,bool,bool),string?> ContinuityChoices=[];
    internal readonly Dictionary<string,string[]> UnbranchedCorridors=[];
    private readonly Dictionary<(int,int),List<NetworkPortion>> cells=[];
    public DirectedRoadNetworkGraph(NetworkRegion region,NetworkPortion[] portions,NetworkConnection[] connections,string[] errors)
    {
        Region=region;Portions=portions;Connections=connections;Errors=errors;
        byId=portions.ToDictionary(p=>p.Id,StringComparer.Ordinal);
        lengths=portions.ToDictionary(p=>p.Id,p=>p.LengthM,StringComparer.Ordinal);
        next=connections.Where(c=>c.Status=="NATIVE_PORT").GroupBy(c=>c.From).ToDictionary(g=>g.Key,g=>g.Select(c=>c.To).Distinct().Order().ToArray());
        previous=connections.Where(c=>c.Status=="NATIVE_PORT").GroupBy(c=>c.To).ToDictionary(g=>g.Key,g=>g.Select(c=>c.From).Distinct().ToArray());
        Ordinals=portions.Select((p,i)=>(p.Id,i)).ToDictionary(p=>p.Id,p=>p.i,StringComparer.Ordinal);
        NextIndices=portions.Select(p=>Next(p.Id).Select(id=>Ordinals[id]).ToArray()).ToArray();
        PreviousIndices=portions.Select(p=>Previous(p.Id).Select(id=>Ordinals[id]).ToArray()).ToArray();
        IndexedLengths=portions.Select(p=>lengths[p.Id]).ToArray();
        foreach(var p in portions)
        {
            var points=p.Paths.SelectMany(x=>x).ToArray();if(points.Length==0)continue;
            for(int x=Bin(points.Min(v=>v[0])-5);x<=Bin(points.Max(v=>v[0])+5);x++)
            for(int z=Bin(points.Min(v=>v[2])-5);z<=Bin(points.Max(v=>v[2])+5);z++)
            {if(!cells.TryGetValue((x,z),out var list))cells[(x,z)]=list=[];list.Add(p);}
        }
    }
    private static int Bin(double n)=>(int)Math.Floor(n/250);
    public string[] Next(string id)=>next.GetValueOrDefault(id,[]);
    public string[] Previous(string id)=>previous.GetValueOrDefault(id,[]);
    public NetworkPortion Get(string id)=>byId[id];
    public double Length(string id)=>lengths[id];
    public static string Id(string uid,string entry,string exit)=>uid+":"+entry+">"+exit;
    public NetworkPortion[] Query(Vector3 position,double radius=180)
    {
        var found=new Dictionary<string,NetworkPortion>();
        for(int x=Bin(position.X-radius);x<=Bin(position.X+radius);x++)
        for(int z=Bin(position.Z-radius);z<=Bin(position.Z+radius);z++)
            if(cells.TryGetValue((x,z),out var list))foreach(var p in list)found.TryAdd(p.Id,p);
        return found.Values.OrderBy(p=>p.Id,StringComparer.Ordinal).ToArray();
    }
    public static DirectedRoadNetworkGraph Build(Map map,NetworkRegion region,RoadLaneGeometryResolver roads,DecisionBranchResolver branches,WorldTileCache? cache=null)
    {
        var portions=new List<NetworkPortion>();var errors=new List<string>();
        foreach(var road in map.MapItems.Values.OfType<Road>().OrderBy(r=>r.Uid))
        {
            if(road.Node is null||road.ForwardNode is null)continue;
            var samples=Enumerable.Range(0,9).Select(i=>road.InterpolateCurve(i/8f).Position).ToArray();
            if(samples.Max(p=>p.X)<region.MinX||samples.Min(p=>p.X)>region.MaxX||samples.Max(p=>p.Z)<region.MinZ||samples.Min(p=>p.Z)>region.MaxZ)continue;
            if(cache?.TryGeometry(road.Node.Position,road.Uid,out var cached)==true)
            {portions.AddRange(cached.Portions);errors.AddRange(cached.Errors);continue;}
            foreach(var pair in new[]{(road.Node.Uid,road.ForwardNode.Uid),(road.ForwardNode.Uid,road.Node.Uid)})
            {
                var lanes=roads.Resolve(road,-1,pair.Item1,pair.Item2);
                if(lanes.Count==0){errors.Add($"{road.Uid:X16}:{pair.Item1:X16}:NO_RESOLVED_LANES");continue;}
                if(lanes.Any(l=>l.Points.Any(p=>!RoadLaneGeometryResolver.Finite(p)))){errors.Add($"{road.Uid:X16}:NON_FINITE");continue;}
                var uid=road.Uid.ToString("X16");var entry=pair.Item1.ToString("X16");var exit=pair.Item2.ToString("X16");
                // Definition-derived offsets remain candidates. Both directions are explicitly separate.
                portions.Add(new(Id(uid,entry,exit),uid,"ROAD",entry,exit,
                    lanes.Select(l=>l.Points.Select(BoardModelResolver.V).ToArray()).ToArray(),
                    lanes.SelectMany(l=>l.Blockers).Distinct().Order().ToArray()));
            }
        }
        foreach(var prefab in map.MapItems.Values.OfType<Prefab>().Where(p=>p.Nodes.Count>0&&
            p.Nodes.Max(n=>n.Position.X)>=region.MinX&&p.Nodes.Min(n=>n.Position.X)<=region.MaxX&&
            p.Nodes.Max(n=>n.Position.Z)>=region.MinZ&&p.Nodes.Min(n=>n.Position.Z)<=region.MaxZ).OrderBy(p=>p.Uid))
        {
        if(cache?.TryGeometry(prefab.Nodes[0].Position,prefab.Uid,out var cached,requiredVersion:2)==true)
        {portions.AddRange(cached.Portions);errors.AddRange(cached.Errors);continue;}
        for(int entryIndex=0;entryIndex<prefab.Nodes.Count;entryIndex++)
        {
            var set=branches.ResolveEntry(prefab,entryIndex);
            if(set.Status!="NETWORK_ENTRY")
            {if(set.Status is not "PPD_UNRESOLVED" || set.InputLaneCount>0)errors.Add($"{prefab.Uid:X16}:{entryIndex}:{set.Status}:{set.Error}");continue;}
            foreach(var b in set.Branches)
            {
                var paths=b.ExactLanePaths.Where(p=>p.Points.Count>=2&&p.OutputLaneOrdinal>=0).ToArray();
                if(b.TargetMapNodeUid==0||paths.Length==0){errors.Add($"{b.BranchId}:NO_EXACT_OUTPUT_PATH");continue;}
                var uid=prefab.Uid.ToString("X16");var entry=prefab.Nodes[entryIndex].Uid.ToString("X16");var exit=b.TargetMapNodeUid.ToString("X16");
                portions.Add(new(Id(uid,entry,exit),uid,"PREFAB",entry,exit,
                    paths.Select(p=>p.Points.Select(BoardModelResolver.V).ToArray()).ToArray(),
                    b.TransformMaxErrorM>1?["PPD_TRANSFORM_RESIDUAL_OVER_1M"]:[]));
            }
        }
        }
        var ordered=portions.OrderBy(p=>p.Id,StringComparer.Ordinal).ToArray();
        var entries=ordered.GroupBy(p=>p.EntryNodeUid).ToDictionary(g=>g.Key,g=>g.ToArray());
        var links=new List<NetworkConnection>();
        foreach(var p in ordered)
        foreach(var q in entries.GetValueOrDefault(p.ExitNodeUid,[]))
        {
            if(p.ItemUid==q.ItemUid)continue; // No invented U-turn, or jump inside a prefab.
            var node=map.Nodes[Convert.ToUInt64(p.ExitNodeUid,16)];
            var items=new[]{node.ForwardItem?.Uid.ToString("X16"),node.BackwardItem?.Uid.ToString("X16")};
            if(!items.Contains(p.ItemUid)||!items.Contains(q.ItemUid))continue;
            links.Add(new(p.Id,q.Id,p.Blockers.Concat(q.Blockers).Any(b=>b!="LANE_WIDTH_4_5M_ASSUMED")?"UNRESOLVED_GEOMETRY":"NATIVE_PORT"));
        }
        return new(region,ordered,links.OrderBy(c=>c.From).ThenBy(c=>c.To).ToArray(),errors.Concat(roads.LoadErrors).Distinct().Order().ToArray());
    }
}
