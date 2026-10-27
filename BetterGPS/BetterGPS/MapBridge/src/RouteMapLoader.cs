using TruckLib.ScsMap;
namespace BetterGPSNavigation;
internal sealed class RouteMapLoader(GameAssets assets,Options options,string output,string? calibration,bool fullMap=false,bool routeCache=true)
{
    private MapBridge? bridge;
    private readonly Dictionary<ulong,ParsedSignArea[]> signAreas=new();
    private HashSet<SectorCoordinate>? loaded;
    private readonly object gate=new();
    private Task? preload;
    private string? OriginPath=>AssetIndexCache.DirectoryFor(assets.Loader) is {} root?
        Path.Combine(root,"last-origin-"+Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(options.MapPath)))[..16]+".json"):null;
    private sealed record Origin(double X,double Z);
    internal Task Preload()
    {
        return preload??=Task.Run(()=>{
            try
            {
                if(OriginPath is not {} path||!File.Exists(path))return;
                var origin=System.Text.Json.JsonSerializer.Deserialize<Origin>(File.ReadAllText(path),LiveProtocol.Json);
                if(origin is null||!double.IsFinite(origin.X+origin.Z))return;
                var directory=options.MapPath[..options.MapPath.LastIndexOf('.')];
                var wanted=assets.Loader.GetFiles(directory).Where(p=>p.EndsWith(".base",StringComparison.OrdinalIgnoreCase))
                    .Select(Sector.SectorCoordsFromSectorFilePath).Where(c=>(c.X+1)*4000d>=origin.X-3000&&c.X*4000d<=origin.X+3000&&
                        (c.Z+1)*4000d>=origin.Z-3000&&c.Z*4000d<=origin.Z+3000).ToHashSet();
                if(wanted.Count==0)return;
                lock(gate)
                {
                    if(bridge is not null)return;
                    using var timing=PerformanceTrace.Measure("career-map-preload");
                    var map=Map.Open(options.MapPath,assets.Loader,wanted.ToArray());
                    bridge=new MapBridge(map,assets.Loader,output,false,calibration,assets.TextureSha256,signAreas){RichDiagnostics=options.DiagnosticReports};loaded=wanted;
                    Console.WriteLine("Départ précédent préparé en mémoire ; les trajets restent vérifiés contre le GPS natif.");
                }
            }
            catch(Exception e) when(e is IOException or InvalidDataException or System.Text.Json.JsonException or ArgumentException)
            {Console.WriteLine($"Préchargement local ignoré : {e.Message}");}
        });
    }
    internal LiveRoute Compile(NativeRoute route,string id,Action<LiveRoute>? ready=null,CancellationToken token=default)
    {lock(gate){try{return CompileLocked(route.WithoutZeroDistanceDuplicates(),id,ready,token);}finally{
        // The live guidance owns its validated immutable plan, not the compilation map.
        // Parsed definitions have compact shared indexes; raw SII documents need not stay resident.
        bridge=null;loaded=null;signAreas.Clear();SiiDocumentCache.Release(assets.Loader);
    }}}
    private LiveRoute CompileLocked(NativeRoute route,string id,Action<LiveRoute>? ready,CancellationToken token)
    {
        token.ThrowIfCancellationRequested();
        if(route.Nodes is {Length:>0} nodes&&OriginPath is {} path)
        {
            try{var temporary=path+".tmp";RouteDiagnostics.WriteText(temporary,()=>System.Text.Json.JsonSerializer.Serialize(new Origin(nodes[0].RawX/256d,nodes[0].RawZ/256d),LiveProtocol.Json));File.Move(temporary,path,true);}
            catch(Exception e) when(e is IOException or UnauthorizedAccessException){}
        }
        var key=CacheKey(route,options.MapPath,calibration is null?null:File.ReadAllText(calibration));
        LiveRoute Build()
        {
            if(ready is null||route.Nodes is null||route.Physical.Length<2)return CompileFresh(route,id,false,token);
            var counts=PreviewCounts(route);
            LiveRoute Preview(int count)
            {
                using var timing=PerformanceTrace.Measure("ready-prefix:"+id+":"+(count==0?"all":count));
                var prefix=count==0?route:Prefix(route,count);
                var part=CompileFresh(prefix,id+(count==0?"_head_all":"_head_"+count),true,token,count==0||count==counts[0]) with{RouteId=id,IsPartial=true};
                LiveProtocol.Validate(new(2,"RouteSnapshot","partial-ready",0,0,1,part));return part;
            }
            return PrepareProgressively(counts,Preview,()=>CompileFresh(route,id,false,token),ready,token);
        }
        var plan=routeCache?AssetIndexCache.Get(assets.Loader,"route-"+key,Build):Build();
        if(plan is null)plan=CompileFresh(route,id);
        plan=plan with{RouteId=id};
        LiveProtocol.Validate(new(2,"RouteSnapshot","cache-verify",0,0,1,plan));
        RouteDiagnostics.WriteBytes(Path.Combine(output,id+"_route.json"),()=>LiveProtocol.Encode(new(2,"RouteSnapshot","map-bridge-diagnostic",0,0,1,plan)));
        return plan;
    }
    // Publish the verified 600 m departure, then spend work on the complete plan.
    // Additional intermediate prefixes are useful for recovery, not on every successful trip.
    internal static LiveRoute PrepareProgressively(int[] counts,Func<int,LiveRoute> preview,Func<LiveRoute> full,Action<LiveRoute> ready,CancellationToken token=default)
    {
        token.ThrowIfCancellationRequested();ready(preview(counts.Length==0?0:counts[0]));
        try{return full();}
        catch(Exception error) when(error is IOException or InvalidDataException or ArgumentException)
        {
            foreach(var count in counts.Skip(1))
            {
                token.ThrowIfCancellationRequested();
                try{ready(preview(count));}
                catch(Exception prefixError) when(prefixError is IOException or InvalidDataException or ArgumentException){break;}
            }
            throw;
        }
    }
    internal static string CacheKey(NativeRoute route,string map,string? calibration)
    {
        // Route-task pointers, capture ticks and ETA estimates do not change the
        // compiled geometry or signs. Preserve all native distance and node evidence.
        var stable=new{map,calibration,route.Nodes,
            physical=route.Physical.Select(p=>new{p.NodeUid,p.DistanceFromPreviousM,p.TotalDistanceTillEndM}),
            logical=route.Logical.Select(p=>new{p.Type,p.NodeId,p.ItemUid,p.DistanceFromPreviousM,p.TotalDistanceTillEndM})};
        return JsonContentHash.Compute(stable);
    }
    internal static int[] PreviewCounts(NativeRoute route)
    {
        if(route.Nodes is not {} nodes)return [];
        int Horizon(double metres,int minimum)
        {
            double distance=0;
            for(int i=1;i<nodes.Length;i++)
            {
                double x=(nodes[i].RawX-(double)nodes[i-1].RawX)/256,z=(nodes[i].RawZ-(double)nodes[i-1].RawZ)/256;
                distance+=Math.Sqrt(x*x+z*z);
                if(i+1>=minimum&&distance>=metres)return i+1;
            }
            return nodes.Length;
        }
        var first=Horizon(600,Math.Min(12,nodes.Length));
        var next=Horizon(2400,Math.Max(48,first+1));
        var extended=Horizon(8000,Math.Max(96,next+1));
        return new[]{first,next,extended}.Distinct().Where(n=>n<nodes.Length).ToArray();
    }
    internal static NativeRoute Prefix(NativeRoute route,int count)
    {
        if(count<2||count>=route.Physical.Length)throw new ArgumentOutOfRangeException(nameof(count));
        int nodes=0;int end=0;
        for(;end<route.Logical.Length;end++)if(route.Logical[end].Type==0&&++nodes==count){end++;break;}
        return route with{Physical=route.Physical.Take(count).ToArray(),Logical=route.Logical.Take(end).ToArray(),
            Nodes=route.Nodes?.Take(count).ToArray()};
    }
    private LiveRoute CompileFresh(NativeRoute route,string id,bool preview=false,CancellationToken token=default,bool fastStart=false)
    {
        var wanted=Select(route,preview?2000:6000);
        token.ThrowIfCancellationRequested();
        if(bridge is null||loaded is not null&&(wanted is null||!wanted.All(loaded.Contains)))
        {
            Map map;
            using(PerformanceTrace.Measure("route-map"))map=Map.Open(options.MapPath,assets.Loader,wanted?.ToArray());
            token.ThrowIfCancellationRequested();
            // Older capture formats and unexpected sector ownership retain a
            // complete-map fallback rather than guessing missing native nodes.
            if(route.Physical.Any(p=>!map.Nodes.ContainsKey(NativeCaptureProtocol.Uid(p.NodeUid)))||
                route.Logical.Where(p=>p.Type==1).Any(p=>!map.MapItems.ContainsKey(NativeCaptureProtocol.Uid(p.ItemUid))))
            {
                using(PerformanceTrace.Measure("route-map-fallback"))map=Map.Open(options.MapPath,assets.Loader);
                wanted=null;
            }
            using(PerformanceTrace.Measure("route-indexes"))bridge=new MapBridge(map,assets.Loader,output,false,calibration,assets.TextureSha256,signAreas){RichDiagnostics=options.DiagnosticReports};
            loaded=wanted;
            Console.WriteLine($"Carte du trajet : {map.Sectors.Count} secteurs, {map.Nodes.Count:N0} noeuds.");
        }
        return bridge.CompileLive(route.ToProbe(id),preview,token,fastStart);
    }
    internal HashSet<SectorCoordinate>? Select(NativeRoute route,double halo=6000)
    {
        if(fullMap||route.Nodes is not {Length:>0} nodes)return null;
        // Keep the complete rectangular naming region and a full sector halo:
        // naming proofs still see neighbouring roads, not just the GPS line.
        var minX=nodes.Min(n=>n.RawX)/256d-halo;var maxX=nodes.Max(n=>n.RawX)/256d+halo;
        var minZ=nodes.Min(n=>n.RawZ)/256d-halo;var maxZ=nodes.Max(n=>n.RawZ)/256d+halo;
        var directory=options.MapPath[..options.MapPath.LastIndexOf('.')];
        var result=new HashSet<SectorCoordinate>();
        foreach(var path in assets.Loader.GetFiles(directory).Where(p=>p.EndsWith(".base",StringComparison.OrdinalIgnoreCase)))
        {
            var c=Sector.SectorCoordsFromSectorFilePath(path);
            if((c.X+1)*4000d>=minX&&c.X*4000d<=maxX&&(c.Z+1)*4000d>=minZ&&c.Z*4000d<=maxZ)result.Add(c);
        }
        return result;
    }
}
