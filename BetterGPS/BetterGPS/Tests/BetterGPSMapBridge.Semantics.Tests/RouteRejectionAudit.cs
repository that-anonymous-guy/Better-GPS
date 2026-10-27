using BetterGPSNavigation;
using System.Text.Json;

internal static class RouteRejectionAudit
{
    internal static async Task Run(string output)
    {
        Directory.CreateDirectory(output);var findings=new List<object>();
        string U(int i)=>i.ToString("X16");
        var raw=new NativeRoute(U(1),[new(U(1),0,200,0,20),new(U(2),100,100,10,10),new(U(3),100,0,10,0)],
            [new(0,0,U(1),0,200,0,20),new(0,0,U(2),100,100,10,10),new(0,0,U(3),100,0,10,0)]);
        var action=new LiveAction("a",0,0,50,"TURN_LEFT","b","HIGH",[new("action","TURN_LEFT","HIGH","PPD",U(1),"b","exact")]);
        var full=new LiveRoute("test",[new(0,U(1),new(0,0,0),new(100,0,0),0,100,0,100),new(1,U(2),new(100,0,0),new(200,0,0),100,200,100,200)],[action]);
        NativeMessage M(string type,long seq,long gen=1)=>new(1,type,"audit",seq,seq*100,gen,type=="RawRouteSnapshot"?raw:null,
            type=="TelemetryFrame"?new(new(10,0,0),.75,10,false,true,true):null);
        int attempts=0;
        var io=new NativeCaptureSession((r,id)=>{attempts++;throw new IOException("temporary evidence write failure");},_=>{});
        io.Accept(M("Hello",0,0));io.Accept(M("RawRouteSnapshot",1));var failed=await io.PollCompile(true);
        var after=io.Accept(M("TelemetryFrame",2),false);io.Tick();
        findings.Add(new{scenario="Temporary disk error classified as route rejection; no automatic recovery",failure=failed?.Status=="ROUTE_REJECTED"&&after.Status=="ROUTE_REJECTED"&&attempts==1,status=after.Status,attempts});
        using var gate=new ManualResetEventSlim();var ready=new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var part=full with{Edges=full.Edges.Take(1).ToArray(),IsPartial=true};
        var progressive=new NativeCaptureSession((r,id)=>full,_=>{},(r,id,emit,token)=>{emit(part);ready.SetResult();gate.Wait();throw new InvalidDataException("distant unresolved branch");});
        progressive.Accept(M("Hello",0,0));progressive.Accept(M("RawRouteSnapshot",1));await ready.Task;
        await progressive.PollCompile();progressive.Accept(M("TelemetryFrame",2),false);gate.Set();var rejected=await progressive.PollCompile(true);
        var resumed=progressive.Accept(M("TelemetryFrame",3),false);var beyond=progressive.Accept(new(1,"TelemetryFrame","audit",4,400,1,Vehicle:new(new(90,0,0),.75,10,false,true,true)),false);
        findings.Add(new{scenario="Full-plan failure after valid prefix produces rejection then resumes prefix, without rebuilding remainder",failure=rejected?.Status=="ROUTE_REJECTED"&&resumed.Instruction=="TURN_LEFT"&&!progressive.Compiling,rejectionStatus=rejected?.Status,resumedStatus=resumed.Status,beyondStatus=beyond.Status,backgroundRecovery=progressive.Compiling});
        var invalid=full with{Edges=[new(0,U(1),new(0,0,0),new(0,0,0),0,100,0,100)]};
        var recorded=new List<LiveMessage>();var broken=new NativeCaptureSession((r,id)=>invalid,recorded.Add);
        broken.Accept(M("Hello",0,0));broken.Accept(M("RawRouteSnapshot",1));string? escaped=null;
        try{await broken.PollCompile(true);}catch(InvalidDataException e){escaped=e.Message;}
        var reuse=broken.Accept(M("RawRouteSnapshot",2,2),false);var noRoute=broken.Accept(M("TelemetryFrame",3,2),false);
        findings.Add(new{scenario="Invalid completed plan escapes failure handling after state mutation; identical suffix appears installed",failure=escaped is not null&&reuse.Status=="WAITING_POSITION"&&noRoute.Status=="NO_ROUTE",escapedException=escaped,reuseStatus=reuse.Status,vehicleStatus=noRoute.Status,rejectionRecorded=recorded.Any(m=>m.Status=="ROUTE_REJECTED")});
        using var partialGate=new ManualResetEventSlim();var partialReady=new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var badPartial=new NativeCaptureSession((r,id)=>full,_=>{},(r,id,emit,token)=>{emit(invalid with{IsPartial=true});partialReady.SetResult();partialGate.Wait();return full;});
        badPartial.Accept(M("Hello",0,0));badPartial.Accept(M("RawRouteSnapshot",1));await partialReady.Task;string? partialError=null;
        try{await badPartial.PollCompile();}catch(InvalidDataException e){partialError=e.Message;}finally{partialGate.Set();await badPartial.Drain();}
        findings.Add(new{scenario="Invalid prefix escapes compiler-failure boundary and can disconnect capture",failure=partialError is not null,escapedException=partialError});
        var resources=Path.Combine(output,"cache-fixture-resources");Directory.CreateDirectory(resources);
        var stamp=Path.Combine(resources,"asset.txt");File.WriteAllText(stamp,"fixture");
        var fs=new MountedDirectoryFileSystem(resources);var cache=Path.Combine(output,"cache-fixture");
        AssetIndexCache.Configure(fs,cache,[new(stamp,"mod")]);
        var key="audit-invalid-route-"+Guid.NewGuid().ToString("N");int rebuilds=0;
        AssetIndexCache.Get(fs,key,()=>invalid);
        var cached=AssetIndexCache.Get(fs,key,()=>{rebuilds++;return full;});string? cachedError=null;
        try{LiveProtocol.Validate(new(2,"RouteSnapshot","cache",0,0,1,cached));}catch(InvalidDataException e){cachedError=e.Message;}
        findings.Add(new{scenario="Checksummed but invalid compiled route is returned from cache rather than rebuilt",failure=cachedError is not null&&rebuilds==0,validationError=cachedError,rebuilds});
        var hash=new string('A',64);var missing=full with{Edges=[],Actions=[],EvidenceFile=Path.GetFullPath(Path.Combine(cache,hash+".json")),EvidenceSha256=hash};
        key="audit-missing-evidence-"+Guid.NewGuid().ToString("N");AssetIndexCache.Get(fs,key,()=>missing);int evidenceRebuilds=0;string? evidenceError=null;
        try{AssetIndexCache.Get(fs,key,()=>{evidenceRebuilds++;return full;});}catch(InvalidDataException e){evidenceError=e.Message;}
        findings.Add(new{scenario="Missing cached large-route evidence escapes cache rebuild handling",failure=evidenceError is not null&&evidenceRebuilds==0,exceptionBase=typeof(InvalidDataException).BaseType!.FullName,escapedException=evidenceError,evidenceRebuilds});
        File.WriteAllText(Path.Combine(output,"failure-probes.json"),JsonSerializer.Serialize(findings,LiveProtocol.Json));
        Console.WriteLine($"Audited {findings.Count} deterministic failure-handling scenarios; evidence retained.");
    }
}
