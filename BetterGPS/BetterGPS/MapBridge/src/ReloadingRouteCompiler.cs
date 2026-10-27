using System.Security.Cryptography;
using System.Text;
namespace BetterGPSNavigation;

// Compile jobs are serialized by NativeCaptureSession. Replace an asset bundle
// only between jobs, so a profile/mod change cannot mix two maps in one plan.
internal sealed class ReloadingRouteCompiler : IDisposable
{
    private readonly Options options;private readonly string cache,output;private readonly string? calibration;
    private readonly bool fullMap,routeCache,worldCache;private string signature;
    private string observedSignature;private long lastSourceCheck;
    private GameAssets? owned;private RouteMapLoader loader;private WorldMapPreparation? warmer;
    internal ReloadingRouteCompiler(GameAssets initial,RouteMapLoader loader,Options options,string cache,string output,string? calibration,
        bool fullMap,bool routeCache,bool worldCache,WorldMapPreparation? initialWarmer=null)
    {
        this.loader=loader;this.options=options;this.cache=cache;this.output=output;this.calibration=calibration;
        this.fullMap=fullMap;this.routeCache=routeCache;this.worldCache=worldCache;warmer=initialWarmer;signature=MountSignature(options.DocumentsRoot);observedSignature=signature;
    }
    internal bool SourcesChanged()
    {
        var now=Environment.TickCount64;if(lastSourceCheck!=0&&now-lastSourceCheck<2000)return false;lastSourceCheck=now;
        var next=MountSignature(options.DocumentsRoot);
        bool changed=next!=observedSignature;observedSignature=next;return changed;
    }
    internal static string MountSignature(string docs)
    {
        var rows=new List<string>();
        foreach(var mount in GameAssets.ReadMounts(Path.Combine(docs,"game.log.txt")))
        {
            rows.Add(mount.Pool+":"+mount.Path);
            IEnumerable<string> files=Directory.Exists(mount.Path)?Directory.EnumerateFiles(mount.Path,"*",SearchOption.AllDirectories).Order(StringComparer.Ordinal):[mount.Path];
            foreach(var file in files)rows.Add(AssetIndexCache.ResourceIdentity(file,mount.Pool=="mod"||Directory.Exists(mount.Path)));
        }
        return Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(string.Join('\n',rows))));
    }
    internal LiveRoute Compile(NativeRoute route,string id,Action<LiveRoute>? ready=null,CancellationToken token=default)
    {
        var next=MountSignature(options.DocumentsRoot);
        if(next!=signature)
        {
            token.ThrowIfCancellationRequested();
            var assets=GameAssets.Open(options);
            if(assets.LoadErrors.Count>0){assets.Dispose();throw new InvalidDataException("Changed mounted resources could not be loaded.");}
            AssetIndexCache.Configure(assets.Loader,cache,GameAssets.ReadMounts(Path.Combine(options.DocumentsRoot,"game.log.txt")));
            if(worldCache)WorldTileCache.Configure(assets.Loader,options.MapPath);
            var replacement=new RouteMapLoader(assets,options,output,calibration,fullMap,routeCache);
            warmer?.Dispose();owned?.Dispose();owned=assets;loader=replacement;signature=next;
            warmer=null; // No second asset bundle or sector-builder during gameplay.
            RouteDiagnostics.Try(()=>assets.WriteManifest(Path.Combine(output,"asset_manifest_reloaded.json")));
            Console.WriteLine("Carte et mods rechargés pour le nouvel itinéraire ; les anciens résultats ne sont pas réutilisés.");
        }
        using var priority=warmer?.Prioritize(route);
        return loader.Compile(route,id,ready,token);
    }
    public void Dispose(){warmer?.Dispose();owned?.Dispose();}
}
