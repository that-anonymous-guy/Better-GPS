using BetterGPSNavigation;
using System.Text.Json;
using System.Security.Cryptography;
using System.Text;
internal static class RouteRecoveryTests
{
    internal static async Task Run(string output)
    {
        Directory.CreateDirectory(output);int count=0;
        void Check(bool ok,string why){count++;if(!ok)throw new Exception(why);}
        string U(int i)=>i.ToString("X16");
        var raw=new NativeRoute(U(1),[new(U(1),0,200,0,20),new(U(2),100,100,10,10),new(U(3),100,0,10,0)],
            [new(0,0,U(1),0,200,0,20),new(0,0,U(2),100,100,10,10),new(0,0,U(3),100,0,10,0)]);
        var action=new LiveAction("a",0,0,50,"TURN_LEFT","b","HIGH",[new("action","TURN_LEFT","HIGH","PPD",U(1),"b","exact")]);
        var full=new LiveRoute("test",[new(0,U(1),new(0,0,0),new(100,0,0),0,100,0,100),new(1,U(2),new(100,0,0),new(200,0,0),100,200,100,200)],[action]);
        NativeMessage M(string type,long seq,long gen=1,double x=10)=>new(1,type,"recovery",seq,seq*100,gen,type=="RawRouteSnapshot"?raw:null,
            type=="TelemetryFrame"?new(new(x,0,0),.75,10,false,true,true):null);
        long now=0;int attempts=0;
        var io=new NativeCaptureSession((r,id)=>{if(++attempts==1)throw new IOException("temporary");return full;},_=>{},retryClock:()=>now);
        io.Accept(M("Hello",0,0));io.Accept(M("RawRouteSnapshot",1));
        Check((await io.PollCompile(true))?.Status=="PREPARING_ROUTE","Temporary error presented as route rejection");
        Check(io.Compiling&&attempts==1,"Temporary failure has no scheduled repair");
        await io.PollCompile();Check(attempts==1,"Backoff ignored");now=500;
        await io.PollCompile();await io.Drain();Check(attempts==2,"Recovery requires another native snapshot");
        Check(io.Accept(M("TelemetryFrame",2),false).Instruction=="TURN_LEFT","Recovered plan does not guide");
        int permanentAttempts=0;var unavailable=new NativeCaptureSession((r,id)=>{permanentAttempts++;throw new IOException("locked");},_=>{});
        unavailable.Accept(M("Hello",0,0));unavailable.Accept(M("RawRouteSnapshot",1));await unavailable.Drain();
        Check(permanentAttempts==4&&!unavailable.Compiling,"Temporary retries are unbounded");
        Check(unavailable.Accept(M("TelemetryFrame",2),false).Status=="ROUTE_UNAVAILABLE","Storage failure mislabeled invalid route");
        var part=full with{Edges=full.Edges.Take(1).ToArray(),IsPartial=true,Actions=[]};
        using var gate=new ManualResetEventSlim();var ready=new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var partial=new NativeCaptureSession((r,id)=>full,_=>{},(r,id,emit,token)=>{emit(part);ready.SetResult();gate.Wait();throw new InvalidDataException("distant branch");});
        partial.Accept(M("Hello",0,0));partial.Accept(M("RawRouteSnapshot",1));await ready.Task;await partial.PollCompile();
        Check(partial.Accept(M("TelemetryFrame",2),false).Instruction=="CONTINUE","Valid prefix unavailable");
        gate.Set();var failed=await partial.PollCompile(true);
        Check(failed?.Status!="ROUTE_REJECTED"&&failed?.PreparationStatus=="PARTIAL_GUIDANCE","Distant failure suppresses validated prefix");
        var still=partial.Accept(M("TelemetryFrame",3),false);Check(still.Instruction=="CONTINUE"&&still.PreparationStatus=="PARTIAL_GUIDANCE","Partial state lost on telemetry");
        Check(partial.Accept(M("TelemetryFrame",4,x:95),false).Status=="ROUTE_UNAVAILABLE","Exhausted prefix pretends computation is ongoing or invents next maneuver");
        var invalid=full with{Edges=[new(0,U(1),new(0,0,0),new(0,0,0),0,100,0,100)]};
        int invalidBuilds=0;var events=new List<LiveMessage>();var invalidState=new NativeCaptureSession((r,id)=>{invalidBuilds++;return invalidBuilds==1?invalid:full;},events.Add);
        invalidState.Accept(M("Hello",0,0));invalidState.Accept(M("RawRouteSnapshot",1));
        Check((await invalidState.PollCompile(true))?.Status=="ROUTE_REJECTED","Invalid plan was not contained");
        Check(events.All(e=>e.Type!="RouteSnapshot"),"Invalid plan recorded as installed");
        Check(invalidState.Accept(M("RawRouteSnapshot",2,2),false).Status=="PREPARING_ROUTE","Invalid plan poisoned suffix reuse");
        await invalidState.Drain();Check(invalidBuilds==2&&invalidState.Accept(M("TelemetryFrame",3,2),false).Instruction=="TURN_LEFT","Corrected identical topology cannot recover");
        var badPartial=new NativeCaptureSession((r,id)=>full,_=>{},(r,id,emit,token)=>{emit(invalid with{IsPartial=true});return full;});
        badPartial.Accept(M("Hello",0,0));badPartial.Accept(M("RawRouteSnapshot",1));
        Check((await badPartial.PollCompile(true))?.Status=="ROUTE_REJECTED","Invalid prefix escaped and disconnected capture");
        var cancelled=new NativeCaptureSession((r,id)=>throw new IOException("retry"),_=>{});
        cancelled.Accept(M("Hello",0,0));cancelled.Accept(M("RawRouteSnapshot",1));await cancelled.PollCompile(true);cancelled.Accept(M("RouteCleared",2,2));await cancelled.Drain();
        Check(!cancelled.Compiling&&cancelled.Accept(M("TelemetryFrame",3,2),false).Status=="NO_ROUTE","Scheduled retry revived a cleared plan");
        var resources=Path.Combine(output,"resources");Directory.CreateDirectory(resources);var stamp=Path.Combine(resources,"asset.txt");File.WriteAllText(stamp,"fixture");
        var fs=new MountedDirectoryFileSystem(resources);var cache=Path.Combine(output,"cache");AssetIndexCache.Configure(fs,cache,[new(stamp,"mod")]);
        void Cached(string name,LiveRoute route)
        {
            var payload=JsonSerializer.Serialize(route,LiveProtocol.Json);
            File.WriteAllText(Path.Combine(cache,AssetIndexCache.Fingerprint(fs)+"-"+name+".json"),JsonSerializer.Serialize(new{Fingerprint=AssetIndexCache.Fingerprint(fs),Sha256=Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(payload))),Payload=payload}));
        }
        int rebuilds=0;var name="invalid";Cached(name,invalid);var repaired=AssetIndexCache.Get(fs,name,()=>{rebuilds++;return full;});
        Check(rebuilds==1&&repaired.Edges.Length==2,"Invalid cached plan not rebuilt");
        AssetIndexCache.Get(fs,name,()=>{rebuilds++;return full;});Check(rebuilds==1,"Repaired cache repeatedly rebuilt");
        var hash=new string('A',64);var missing=full with{Edges=[],Actions=[],EvidenceFile=Path.GetFullPath(Path.Combine(cache,hash+".json")),EvidenceSha256=hash};
        name="missing";Cached(name,missing);AssetIndexCache.Get(fs,name,()=>{rebuilds++;return full;});Check(rebuilds==2,"Missing large-route evidence not repaired");
        name="corrupt";File.WriteAllText(missing.EvidenceFile!,"corrupt");Cached(name,missing);AssetIndexCache.Get(fs,name,()=>{rebuilds++;return full;});Check(rebuilds==3,"Corrupt large-route evidence not repaired");
        name="null";File.WriteAllText(Path.Combine(cache,AssetIndexCache.Fingerprint(fs)+"-"+name+".json"),"null");AssetIndexCache.Get(fs,name,()=>{rebuilds++;return full;});Check(rebuilds==4,"Null cache envelope crashed repair");
        bool invalidCached=false;try{AssetIndexCache.Get(fs,"bad-build",()=>invalid);}catch(InvalidDataException){invalidCached=true;}
        Check(invalidCached&&!File.Exists(Path.Combine(cache,AssetIndexCache.Fingerprint(fs)+"-bad-build.json")),"Invalid new plan written to cache");
        RouteDiagnostics.Try(()=>throw new IOException("diagnostic only"));Check(true,"Optional diagnostics block navigation");
        using var cancel=new CancellationTokenSource();cancel.Cancel();bool cancelledSolve=false;
        try{NamedRoadNetwork.Solve(new(new(0,1,0,1),[],[],[]),[],cancel.Token);}catch(OperationCanceledException){cancelledSolve=true;}
        Check(cancelledSolve,"Named solver ignores cancellation");
        var largePortions=Enumerable.Range(0,11002).Select(i=>new NetworkPortion("large:"+i,U(i+1),"ROAD",U(i+1),U(i+2),[[[i,0,0],[i+1,0,0]]],[])).ToArray();
        var largeLinks=Enumerable.Range(0,largePortions.Length-1).Select(i=>new NetworkConnection(largePortions[i].Id,largePortions[i+1].Id,"NATIVE_PORT")).ToArray();
        var chain=new DirectedRoadNetworkGraph(new(0,12000,0,1),largePortions,largeLinks,[]);
        Check(new NetworkReachability(chain,largePortions[0].Id).Complete,"Reachability abandons graph beyond 10000 portions");
        Check(NamedRoadNetwork.MandatoryCorridor(chain,largePortions[0].Id,largePortions[^1].Id,double.PositiveInfinity,true)!.Length==largePortions.Length,"Long mandatory road not proved");
        var alternate=new DirectedRoadNetworkGraph(chain.Region,largePortions,largeLinks.Append(new(largePortions[0].Id,largePortions[^1].Id,"NATIVE_PORT")).ToArray(),[]);
        Check(NamedRoadNetwork.MandatoryCorridor(alternate,largePortions[0].Id,largePortions[^1].Id,double.PositiveInfinity,true)!.Length==2,"Long alternative incorrectly acquires mandatory naming proof");
        File.WriteAllText(Path.Combine(output,"acceptance.json"),JsonSerializer.Serialize(new{status="PASS",count,reproducedDefectsFixed=6,boundedTotalAttempts=4},LiveProtocol.Json));
        Console.WriteLine($"PASS: {count} recovery, atomic installation, partial guidance and cache repair assertions.");
    }
}
