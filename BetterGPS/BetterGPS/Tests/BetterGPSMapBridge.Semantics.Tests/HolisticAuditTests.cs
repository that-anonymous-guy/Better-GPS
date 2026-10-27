using BetterGPSNavigation;
using System.Text.Json;
internal static class HolisticAuditTests
{
    internal static async Task Run(string output)
    {
        Directory.CreateDirectory(output);var rows=new List<object>();string U(int n)=>n.ToString("X16");
        LiveAction Action(string id,int segment,double station,string instruction,int eventIndex=0)=>new(id,eventIndex,segment,station,instruction,"branch:"+id,"HIGH",
            [new("action",instruction,"HIGH","PPD",U(segment+1),"branch:"+id,"exact")]);
        LiveGuidanceEngine Engine(LiveRoute route)
        {var e=new LiveGuidanceEngine();e.Accept(new(2,"Hello","audit",0,0,0));e.Accept(new(2,"RouteSnapshot","audit",1,0,1,route));return e;}
        var endRoute=new LiveRoute("end-action",[new(0,U(1),new(0,0,0),new(100,0,0),0,100,0,100)],[Action("last",0,50,"TURN_RIGHT")]){NavigationRemainingAtOriginM=100};
        var end=Engine(endRoute).Accept(new(2,"VehicleState","audit",2,100,1,Vehicle:new(new(100,0,0),0,10,false,0)));
        rows.Add(new{scenario="Last maneuver occupies last route segment",failure=end.Status!="COMPLETE",observed=end.Status,end.Instruction,end.DistanceM,expected="COMPLETE at exact endpoint with native remaining zero"});
        var zero=new LiveRoute("zero-nav",[new(0,U(1),new(0,0,0),new(100,0,0),0,100,0,0),new(1,U(2),new(100,0,0),new(200,0,0),100,200,0,0),new(2,U(3),new(200,0,0),new(300,0,0),200,300,0,100)],
            [Action("first",0,0,"TURN_LEFT"),Action("second",1,0,"TURN_RIGHT",1)]){NavigationRemainingAtOriginM=100};
        var zeroG=Engine(zero).Accept(new(2,"VehicleState","audit",2,100,1,Vehicle:new(new(150,0,0),0,10,false,100)));
        rows.Add(new{scenario="Distinct physical segments share zero native navigation station",failure=zeroG.ActionId!="second",observed=zeroG.ActionId,zeroG.ThenInstruction,expected="second action after entering second physical segment",nativeInputSeenInDrivingCapture=false});
        var huge=endRoute with{RouteId="blob-corrupt",Actions=[Action("first",0,20,"TURN_LEFT") with{Proofs=Action("first",0,20,"TURN_LEFT").Proofs.Append(new LiveProof("audit",new string('x',9*1024*1024),"LOW","DIAGNOSTIC","fixture","branch:first","retained")).ToArray()},Action("second",0,80,"TURN_RIGHT",1)]};
        var blobFolder=Path.Combine(output,"blob-"+Guid.NewGuid().ToString("N"));var original=RouteEvidence.Pack(huge,blobFolder);File.WriteAllText(original.EvidenceFile!,"corrupt");
        var rebuilt=RouteEvidence.Pack(huge,blobFolder);string? blobError=null;try{RouteEvidence.Unpack(rebuilt);}catch(InvalidDataException e){blobError=e.Message;}
        rows.Add(new{scenario="Rebuilt large plan reuses an existing corrupt proof file",failure=blobError is not null,observed=blobError,expected="atomic replacement or successful proof-file repair"});
        var rebindFolder=Path.Combine(output,"rebind-"+Guid.NewGuid().ToString("N"));
        var sharedEvent=huge with{RouteId="shared-event",Actions=[huge.Actions[0],huge.Actions[1] with{EventIndex=0}]};
        LiveProtocol.Validate(new(2,"RouteSnapshot","valid",0,0,1,sharedEvent));var shared=RouteEvidence.Pack(sharedEvent,rebindFolder);string? rebindError=null;
        try{LiveProtocol.Validate(new(2,"RouteSnapshot","rebind",0,0,1,shared with{RouteId="other"}));}catch(InvalidDataException e){rebindError=e.Message;}
        rows.Add(new{scenario="Two legal actions of one event receive duplicate IDs after sidecar rebinding",failure=rebindError is not null,observed=rebindError,expected="distinct action IDs preserved",observedInDrivingCapture=false});
        var manifest=Path.Combine(output,"locked-manifest.json");int builds=0;
        using(var locked=new FileStream(manifest,FileMode.Create,FileAccess.ReadWrite,FileShare.None))
        {
            var raw=new NativeRoute(U(1),[new(U(1),0,100,0,10),new(U(2),100,0,10,0)],[new(0,0,U(1),0,100,0,10),new(0,0,U(2),100,0,10,0)]);
            var state=new NativeCaptureSession((r,id)=>{try{builds++;return endRoute;}finally{RouteDiagnostics.Try(()=>File.WriteAllText(manifest,"manifest"));}},_=>{});
            state.Accept(new(1,"Hello","manifest",0,0,0));state.Accept(new(1,"RawRouteSnapshot","manifest",1,100,1,raw));await state.Drain();
            var g=state.Accept(new(1,"TelemetryFrame","manifest",2,200,1,Vehicle:new(new(10,0,0),.75,10,false,true,true)),false);
            rows.Add(new{scenario="Mandatory manifest export in finally suppresses a valid compiled plan",failure=g.Status=="ROUTE_UNAVAILABLE",observed=g.Status,builds,expected="guidance despite optional manifest write failure",scope="Same callback pattern as NativeBridgeCli asset manifest"});
        }
        var resources=Path.Combine(output,"resources");Directory.CreateDirectory(resources);var asset=Path.Combine(resources,"asset.txt");File.WriteAllText(asset,"assetA");var stamp=File.GetLastWriteTimeUtc(asset);
        var fs=new MountedDirectoryFileSystem(resources);var cache=Path.Combine(output,"metadata-cache");AssetIndexCache.Configure(fs,cache,[new(asset,"mod")]);AssetIndexCache.Get(fs,"fixture",()=>"assetA");
        File.WriteAllText(asset,"assetB");File.SetLastWriteTimeUtc(asset,stamp);var changed=new MountedDirectoryFileSystem(resources);AssetIndexCache.Configure(changed,cache,[new(asset,"mod")]);int reads=0;
        var stale=AssetIndexCache.Get(changed,"fixture",()=>{reads++;return File.ReadAllText(asset);});
        rows.Add(new{scenario="Changed resource bytes with identical path, length and modification time",failure=stale!="assetB",observed=stale,reads,sameFingerprint=AssetIndexCache.Fingerprint(fs)==AssetIndexCache.Fingerprint(changed),expected="invalidate changed resource or explicitly exclude preserved-metadata updates"});
        var largeAsset=Path.Combine(resources,"large-mod.bin");File.WriteAllBytes(largeAsset,new byte[2*1024*1024]);
        var largeStamp=File.GetLastWriteTimeUtc(largeAsset);var largeBefore=new MountedDirectoryFileSystem(resources);
        AssetIndexCache.Configure(largeBefore,cache,[new(largeAsset,"mod")]);
        using(var modified=new FileStream(largeAsset,FileMode.Open,FileAccess.Write,FileShare.None)){modified.WriteByte(123);}
        File.SetLastWriteTimeUtc(largeAsset,largeStamp);var largeAfter=new MountedDirectoryFileSystem(resources);
        AssetIndexCache.Configure(largeAfter,cache,[new(largeAsset,"mod")]);
        if(AssetIndexCache.Fingerprint(largeBefore)==AssetIndexCache.Fingerprint(largeAfter))throw new Exception("Preserved-metadata large mod update remained cached.");
        var notArrived=Engine(endRoute).Accept(new(2,"VehicleState","audit",2,100,1,Vehicle:new(new(100,0,0),0,10,false,1360)));
        if(notArrived.Status=="COMPLETE")throw new Exception("Arrival ignored native remaining final approach.");
        var thenZero=Engine(zero).Accept(new(2,"VehicleState","audit",2,100,1,Vehicle:new(new(50,0,0),0,10,false,100)));
        if(thenZero.ThenInstruction!="TURN_RIGHT")throw new Exception("Then lost a same-station action on the next segment.");
        var cacheRoute=new NativeRoute(U(1),[new(U(1),0,100,0,10),new(U(2),100,0,10,0)],
            [new(0,0,U(1),0,100,0,10),new(0,0,U(2),100,0,10,0)]);
        var clockChanged=cacheRoute with{Fingerprint=U(2),CapturedAtTickMs=500,
            Physical=cacheRoute.Physical.Select(p=>p with{TimeFromPreviousS=p.TimeFromPreviousS+1,TotalTimeTillEndS=p.TotalTimeTillEndS+1}).ToArray()};
        if(RouteMapLoader.CacheKey(cacheRoute,"usa",null)!=RouteMapLoader.CacheKey(clockChanged,"usa",null))
            throw new Exception("ETA/session changes invalidate identical route geometry.");
        if(RouteMapLoader.CacheKey(cacheRoute,"usa",null)==RouteMapLoader.CacheKey(cacheRoute with{Physical=cacheRoute.Physical.Select(p=>p with{TotalDistanceTillEndM=p.TotalDistanceTillEndM+20}).ToArray()},"usa",null))
            throw new Exception("Changed native distance axis reused route cache.");
        var fork=new DecisionBranchGeometry("fork",1,true,[0,1],[1,2],[],0,"",[
            new System.Numerics.Vector3[]{new(0,0,0),new(100,0,0)},
            new System.Numerics.Vector3[]{new(0,0,100),new(100,0,100)}],1,2,"Road")
            {NativeLanePolylineIndices=[1]};
        var signProjection=DecisionBranchResolver.Project(new System.Numerics.Vector3(50,0,0),90,fork);
        if(signProjection?.PolylineIndex!=1||signProjection.LateralM<99)
            throw new Exception("Sign matched a different internal path sharing the selected exit.");
        var journalFolder=Path.Combine(output,"journal");Directory.CreateDirectory(journalFolder);
        using(var journal=new GuidanceJournal(journalFolder))
        {
            var first=end with{TimestampMs=1000,Status="PREPARE",RouteId="same",Revision=1,ActionId="a",Road="CA-99",
                Proofs=[new("road","CA-99","MEDIUM","SIGN_BOARD",U(1),"branch:a","first session")]};
            journal.Write(first);journal.Write(first with{TimestampMs=2000,Road="I-5",Proofs=[new("road","I-5","MEDIUM","SIGN_BOARD",U(1),"branch:a","second session")]});
        }
        var entries=File.ReadLines(Path.Combine(journalFolder,"guidance.jsonl")).Select(l=>JsonSerializer.Deserialize<LiveGuidance>(l,LiveProtocol.Json)!).ToArray();
        var proofCount=File.ReadLines(Path.Combine(journalFolder,"guidance-proofs.jsonl")).Count();
        rows.Add(new{scenario="Reused route/revision/action IDs deduplicate distinct proof packets across capture sessions",failure=proofCount==1&&entries[0].ProofReference==entries[1].ProofReference,proofPackets=proofCount,expected="distinct session or proof-content references",scope="GuidanceJournal survives pipe reconnections; revisions restart in NativeCaptureSession"});
        File.WriteAllText(Path.Combine(output,"failure-probes.json"),JsonSerializer.Serialize(rows,LiveProtocol.Json));
        using(var results=JsonDocument.Parse(JsonSerializer.Serialize(rows,LiveProtocol.Json)))
            if(results.RootElement.EnumerateArray().Any(r=>r.GetProperty("failure").GetBoolean()))
                throw new Exception("Holistic regression failed; see failure-probes.json.");
        Console.WriteLine($"PASS: Audited {rows.Count} terminal-action, cache, rebinding, export and provenance scenarios.");
    }
}
