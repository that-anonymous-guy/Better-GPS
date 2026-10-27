using System.Diagnostics;

namespace BetterGPSNavigation;

// A single compiler owns the map caches. Reception and console output remain responsive.
internal sealed class NativeCaptureSession
{
    private readonly Func<NativeRoute,string,LiveRoute> compile;
    private readonly Action<LiveMessage> record;
    private readonly Func<bool>? sourcesChanged;
    private readonly Func<NativeRoute,string,Action<LiveRoute>,CancellationToken,LiveRoute>? progressiveCompile;
    private CancellationTokenSource? jobCancellation;
    private readonly System.Collections.Concurrent.ConcurrentQueue<LiveRoute> readyParts=new();
    private LiveGuidanceEngine engine = new();
    private string? session;
    private long sequence=-1, time=-1, generation=-1, revision=0, liveSequence;
    private long activeGeneration=-1;
    private NativeRoute? installedRaw;
    private bool installedPartial;
    private NativeRoute? preparedRaw;
    private LiveRoute? preparedRoute;
    private string? installedEvidence;
    internal string[] RetainedEvidence=>new[]{installedEvidence,preparedRoute?.EvidenceFile}.OfType<string>().Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
    private (NativeRoute Route,long Generation)? pending;
    private Task<LiveRoute>? job;
    private (NativeRoute Route,long Generation) running;
    private readonly Stopwatch wall=new();
    private string status="WAITING_DLL", detail="Waiting for native capture.";
    private bool waitingVehicle;
    private bool prepareRequested;
    private (NativeRoute Route,long Generation)? retry;
    private long retryAt;private int retryCount;
    private string? preparationFailure;
    private readonly Func<long> retryClock;
    public bool Compiling => job is not null || pending is not null || retry is not null;
    public NativeCaptureSession(Func<NativeRoute,string,LiveRoute> compile,Action<LiveMessage> record,
        Func<NativeRoute,string,Action<LiveRoute>,CancellationToken,LiveRoute>? progressiveCompile=null,Func<bool>? sourcesChanged=null,Func<long>? retryClock=null)
    { this.compile=compile; this.record=record; this.progressiveCompile=progressiveCompile;this.sourcesChanged=sourcesChanged;this.retryClock=retryClock??(()=>wall.ElapsedMilliseconds); }
    private static void ValidatePlan(LiveRoute route)=>LiveProtocol.Encode(new(2,"RouteSnapshot","validation",0,0,1,route));
    private LiveGuidance Install(LiveRoute route,NativeRoute raw,bool partial)
    {
        ValidatePlan(route);
        var m=new LiveMessage(2,"RouteSnapshot",session!,liveSequence++,Math.Max(0,time),revision+1,route);
        record(m);var result=engine.Accept(m);
        revision++;installedEvidence=route.EvidenceFile;installedRaw=raw;installedPartial=partial;activeGeneration=generation;waitingVehicle=true;
        status="WAITING_POSITION";detail="Validated route installed; awaiting a fresh matching pose.";
        return result;
    }
    private void ResetRecovery(){retry=null;retryCount=0;preparationFailure=null;}
    private LiveGuidance WithPreparation(LiveGuidance guidance)=>preparationFailure is null?guidance:guidance with
    {PreparationStatus=retry is not null||job is not null?"RETRYING":"PARTIAL_GUIDANCE",PreparationDetail=preparationFailure,
        Status=guidance.Status=="PREPARING_ROUTE"&&retry is null&&job is null?"ROUTE_UNAVAILABLE":guidance.Status,
        Detail=guidance.Status=="PREPARING_ROUTE"&&retry is null&&job is null?"Verified portion exhausted. "+preparationFailure:guidance.Detail};

    private LiveGuidance Feed(string type,LiveRoute? route=null,LiveVehicle? vehicle=null)
    {
        var m=new LiveMessage(2,type,session!,liveSequence++,Math.Max(0,time),revision,route,vehicle);
        record(m); return engine.Accept(m);
    }
    private LiveGuidance Stop(string state,string reason)
    {
        status=state; detail=reason;
        if(session is null) return engine.Suspend(Math.Max(0,time),state,reason);
        var m=new LiveMessage(2,"SourceState",session,liveSequence++,Math.Max(0,time),revision,Status:state,Detail:reason);
        record(m); return engine.Accept(m);
    }
    public LiveGuidance Disconnect()
    {
        jobCancellation?.Cancel();
        activeGeneration=-1; pending=null; prepareRequested=false;ResetRecovery();
        return Stop("DISCONNECTED","DLL disconnected; guidance suppressed.");
    }

    public LiveGuidance Accept(NativeMessage m,bool checkWallClock=true)
    {
        NativeCaptureProtocol.Validate(m);
        if(m.Type=="Hello")
        {
            if(session is not null) throw new InvalidDataException("One Hello per pipe connection.");
            session=m.SessionId; wall.Restart(); sequence=-1; time=-1; generation=-1;
            FeedHello(m.TimestampMs);
        }
        if(session!=m.SessionId || m.Sequence<=sequence || m.TimestampMs<time || m.Generation<generation)
            throw new InvalidDataException("Native session, sequence, clock or generation moved backwards.");
        sequence=m.Sequence; time=m.TimestampMs; generation=m.Generation;
        switch(m.Type)
        {
            case "Hello": return Stop("WAITING_ROUTE",m.Status??"Native DLL connected.");
            case "CaptureStatus":
                ResetRecovery();
                jobCancellation?.Cancel();
                activeGeneration=-1; pending=null; prepareRequested=false;
                return Stop(m.Status!,"Native route is unavailable or changing.");
            case "RouteCleared":
                ResetRecovery();
                jobCancellation?.Cancel();
                pending=null; installedEvidence=null;installedRaw=null; preparedRaw=null; preparedRoute=null;
                activeGeneration=-1; prepareRequested=false; revision++;
                status="NO_ROUTE"; detail="GPS route cleared."; return Feed("RouteCleared");
            case "RawRouteSnapshot":
                if(sourcesChanged?.Invoke()==true)
                {
                    ResetRecovery();
                    jobCancellation?.Cancel();installedRaw=null;preparedRaw=null;preparedRoute=null;
                    activeGeneration=-1;
                }
                if(installedRaw is not null && m.Route!.IsSuffixOf(installedRaw)&&
                    (!installedPartial||job is not null&&jobCancellation?.IsCancellationRequested!=true||retry is not null))
                {
                    if(job is not null&&m.Route.IsSuffixOf(running.Route))running=(running.Route,generation);
                    if(retry is not null)retry=(retry.Value.Route,generation);
                    // The game removed passed items. Keep the original station axis and completed actions.
                    activeGeneration=generation; pending=null; waitingVehicle=true;
                    prepareRequested=job is not null&&running.Generation==generation;
                    return Stop("WAITING_POSITION","Remaining route is a verified suffix; corridor retained.");
                }
                if(preparedRaw is not null && preparedRoute is not null && m.Route!.IsSuffixOf(preparedRaw))
                {
                    pending=null; prepareRequested=false;ResetRecovery();
                    return Install(preparedRoute,preparedRaw,false);
                }
                if(job is not null && jobCancellation?.IsCancellationRequested!=true&&m.Route!.IsSuffixOf(running.Route))
                {
                    // Driving can shorten the route faster than compilation completes.
                    // Adopt this job for the latest generation rather than restarting it.
                    running=(running.Route,generation); pending=null; prepareRequested=true;
                    activeGeneration=-1;
                    return Stop("PREPARING_ROUTE","Preparing original corridor; latest route is its verified suffix.");
                }
                jobCancellation?.Cancel();
                ResetRecovery();
                activeGeneration=-1; prepareRequested=true; pending=(m.Route!,generation); StartPending();
                return Stop("PREPARING_ROUTE","Preparing corridor, branches and validated sign fields.");
            case "TelemetryFrame":
                var v=m.Vehicle!;
                if(v.Paused) return Stop("PAUSED","Game telemetry paused.");
                if(!v.PoseValid || !v.SpeedValid) return Stop("NO_TELEMETRY","Pose or speed unavailable.");
                if(checkWallClock && wall.ElapsedMilliseconds-time>LiveGuidanceEngine.StaleAfterMs)
                    return Stop("STALE","Delayed native vehicle frame; waiting for fresh telemetry.");
                if(activeGeneration!=generation) return Stop(Compiling?"PREPARING_ROUTE":status,detail);
                waitingVehicle=false;
                var result=WithPreparation(Feed("VehicleState",vehicle:v.ToLive()));
                status="TRACKING"; detail="Tracking current native route."; return result;
            default: throw new InvalidDataException("Unhandled native capture type.");
        }
    }
    private void FeedHello(long timestamp)
    { time=timestamp; engine=new(); revision=0; liveSequence=0; Feed("Hello"); }
    private void StartPending()
    {
        if(job is not null || pending is null) return;
        running=pending.Value; pending=null;
        var snapshot=running;
        while(readyParts.TryDequeue(out _)){}
        var id=$"live_{snapshot.Generation:D6}_{snapshot.Route.Fingerprint}";
        jobCancellation=new();var token=jobCancellation.Token;
        job=Task.Run(()=>
        {
            var result=progressiveCompile is null?compile(snapshot.Route,id):
                progressiveCompile(snapshot.Route,id,part=>{token.ThrowIfCancellationRequested();ValidatePlan(part);readyParts.Enqueue(part);},token);
            token.ThrowIfCancellationRequested();ValidatePlan(result);return result;
        });
    }
    public async Task<LiveGuidance?> PollCompile(bool wait=false)
    {
        if(job is null&&retry is {} recovery&&(wait||retryClock()>=retryAt))
        {
            retry=null;
            if(recovery.Generation==generation){pending=recovery;prepareRequested=true;StartPending();}
        }
        if(!wait&&job is not null&&readyParts.TryDequeue(out var part))
        {
            if(running.Generation==generation&&pending is null&&prepareRequested)
            {
                try{return WithPreparation(Install(part,running.Route,true));}
                catch(Exception e) when(e is InvalidDataException or IOException or UnauthorizedAccessException)
                {jobCancellation?.Cancel();prepareRequested=false;return Failed(e);}
            }
        }
        if(job is null || (!wait && !job.IsCompleted)) return null;
        LiveRoute? route=null; Exception? error=null;
        try { route=await job; } catch(Exception e) { error=e; }
        if(jobCancellation?.IsCancellationRequested==true)route=null;
        jobCancellation?.Dispose();jobCancellation=null;
        job=null;
        LiveGuidance? output=null;
        if(running.Generation==generation && pending is null && prepareRequested)
        {
            prepareRequested=false;
            if(error is not null) output=Failed(error);
            else if(route is null)output=Stop("PREPARING_ROUTE","Cancelled plan was not installed.");
            else
            {
                try
                {
                    output=Install(route,running.Route,false);preparedRaw=running.Route;preparedRoute=route;ResetRecovery();
                }
                catch(Exception e) when(e is InvalidDataException or IOException or UnauthorizedAccessException){output=Failed(e);}
            }
        }
        StartPending(); return output;
    }
    private LiveGuidance Failed(Exception error)
    {
        var id=$"live_{running.Generation:D6}_{running.Route.Fingerprint}";
        bool temporary=error is IOException or UnauthorizedAccessException;
        preparationFailure=$"{(temporary?"TEMPORARY_FAILURE":"INVALID_PLAN")} route={id}: {error.Message}";
        if(temporary&&retryCount<3)
        {retryCount++;retry=running;retryAt=retryClock()+500*(1<<(retryCount-1));}
        bool partial=installedPartial&&activeGeneration==generation;
        if(partial)
        {status="TRACKING";detail=preparationFailure;return WithPreparation(engine.Tick(Math.Max(0,time)));}
        activeGeneration=-1;
        return Stop(retry is not null?"PREPARING_ROUTE":temporary?"ROUTE_UNAVAILABLE":"ROUTE_REJECTED",preparationFailure);
    }
    public LiveGuidance Tick()
    {
        long now=Math.Max(Math.Max(0,time),wall.ElapsedMilliseconds);
        if(activeGeneration<0 || waitingVehicle || status!="TRACKING")
            return engine.Suspend(now,status,detail);
        return WithPreparation(engine.Tick(now));
    }
    public async Task Drain() { while(job is not null||retry is not null) { await PollCompile(true); } }
}
