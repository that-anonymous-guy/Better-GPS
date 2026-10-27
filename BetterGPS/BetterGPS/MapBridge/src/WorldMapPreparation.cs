using System.Collections.Concurrent;
using System.Numerics;
using TruckLib.ScsMap;
namespace BetterGPSNavigation;

internal sealed class WorldMapPreparation:IDisposable
{
    private readonly CancellationTokenSource stop=new();
    private readonly ManualResetEventSlim idle=new(true);
    private readonly ConcurrentQueue<(int,int)> priority=new();
    private readonly AutoResetEvent requests=new(false);
    private readonly Task worker;
    private int busy;
    private WorldMapPreparation(Options options,string cache,string output)
    {
        worker=Task.Run(()=>{
            GameAssets? assets=null;
            try
            {
                // No whole-map sweep during gameplay; prepare only requested route neighbourhoods.
                while(!stop.IsCancellationRequested)
                {
                    WaitHandle.WaitAny([requests,stop.Token.WaitHandle]);
                    stop.Token.ThrowIfCancellationRequested();idle.Wait(stop.Token);
                    var repair=new HashSet<(int,int)>();while(priority.TryDequeue(out var cell))repair.Add(cell);
                    if(repair.Count>0&&assets is null){
                        Directory.CreateDirectory(output);assets=GameAssets.Open(options);
                        if(assets.LoadErrors.Count>0)throw new InvalidDataException("Incomplete mounted resources.");
                        AssetIndexCache.Configure(assets.Loader,cache,GameAssets.ReadMounts(Path.Combine(options.DocumentsRoot,"game.log.txt")));
                        WorldTileCache.Configure(assets.Loader,options.MapPath);
                    }
                    if(repair.Count>0)Build(assets!,options,output,stop.Token,idle,only:repair);
                }
            }
            catch(OperationCanceledException) when(stop.IsCancellationRequested){}
            catch(Exception e){Console.WriteLine($"Cache de carte interrompu (repris au prochain lancement) : {e.Message}");}
            finally{assets?.Dispose();}
        });
    }
    internal static WorldMapPreparation Start(Options options,string cache,string output)=>new(options,cache,output);
    internal IDisposable Prioritize(NativeRoute route)
    {
        if(Interlocked.Increment(ref busy)==1)idle.Reset();
        foreach(var cell in (route.Nodes??[]).Select(n=>WorldTileCache.Cell(new Vector3(n.RawX/256f,0,n.RawZ/256f))).Distinct())
            for(int x=-1;x<=1;x++)for(int z=-1;z<=1;z++)priority.Enqueue((cell.X+x,cell.Z+z));
        requests.Set();
        return new Lease(this);
    }
    private sealed class Lease(WorldMapPreparation owner):IDisposable
    {private bool disposed;public void Dispose(){if(disposed)return;disposed=true;if(Interlocked.Decrement(ref owner.busy)==0)owner.idle.Set();}}
    public void Dispose(){stop.Cancel();idle.Set();requests.Set();/* current immutable page finishes/cancels; never delay game shutdown */}
    internal static void Build(GameAssets assets,Options options,string output,CancellationToken token,
        ManualResetEventSlim? idle=null,ConcurrentQueue<(int,int)>? priority=null,HashSet<(int,int)>? only=null)
    {
        var cache=WorldTileCache.For(assets.Loader)??throw new InvalidOperationException("World cache not configured.");
        var directory=options.MapPath[..options.MapPath.LastIndexOf('.')];
        var sectors=assets.Loader.GetFiles(directory).Where(p=>p.EndsWith(".base",StringComparison.OrdinalIgnoreCase))
            .Select(Sector.SectorCoordsFromSectorFilePath).Distinct().ToArray();
        var catalogue=sectors.Select(c=>(c.X,c.Z)).ToArray();
        if(only is null&&cache.IsComplete(catalogue)){Console.WriteLine("Cache de carte existant : index ouvert, blocs chargés à la demande.");return;}
        var remaining=catalogue.Where(c=>only is null||only.Contains(c)).ToHashSet();
        var ordered=new Queue<(int,int)>(remaining.OrderBy(c=>c.Item1).ThenBy(c=>c.Item2));
        int total=remaining.Count,done=0,built=0;
        Console.WriteLine($"Cache de carte : {total} blocs ; préparation persistante et reprenable.");
        while(remaining.Count>0)
        {
            token.ThrowIfCancellationRequested();idle?.Wait(token);
            (int X,int Z) cell=default;bool selected=false;
            while(priority?.TryDequeue(out var preferred)==true)if(remaining.Contains(preferred)){cell=preferred;selected=true;break;}
            if(!selected){do{cell=ordered.Dequeue();}while(!remaining.Contains(cell));}
            remaining.Remove(cell);done++;
            // Check page integrity, not merely its existence, before reusing it.
            if(!cache.CanPrepare(cell.X,cell.Z)||cache.Read(cell.X,cell.Z) is not null)continue;
            var minX=cell.X*4000d-2000;var maxX=(cell.X+1)*4000d+2000;
            var minZ=cell.Z*4000d-2000;var maxZ=(cell.Z+1)*4000d+2000;
            var selectedSectors=sectors.Where(c=>(c.X+1)*4000d>=minX&&c.X*4000d<=maxX&&
                (c.Z+1)*4000d>=minZ&&c.Z*4000d<=maxZ).ToArray();
            var started=Environment.TickCount64;
            using(PerformanceTrace.Measure($"world-tile:{cell.X}:{cell.Z}"))
            {
                var map=Map.Open(options.MapPath,assets.Loader,selectedSectors);
                token.ThrowIfCancellationRequested();
                var bridge=new MapBridge(map,assets.Loader,output,false,textureHash:assets.TextureSha256);
                cache.Write(bridge.PrepareWorldTile(cell.X,cell.Z,token));
            }
            built++;
            // Yield between requested blocks. Never race gameplay with an unbounded sweep.
            if(idle is not null&&token.WaitHandle.WaitOne((int)Math.Clamp((Environment.TickCount64-started)*9,250,30000)))token.ThrowIfCancellationRequested();
            if(built%10==0)Console.WriteLine($"Cache de carte : {done}/{total} blocs vérifiés, {built} préparés.");
        }
        File.WriteAllText(Path.Combine(output,"world-cache-complete.json"),System.Text.Json.JsonSerializer.Serialize(
            new{fingerprint=AssetIndexCache.Fingerprint(assets.Loader),total,built,complete=only is null,utc=DateTime.UtcNow},LiveProtocol.Json));
        if(only is null)cache.MarkComplete(catalogue);
        Console.WriteLine($"Cache de carte prêt : {total} blocs vérifiés, {built} préparés.");
    }
}
