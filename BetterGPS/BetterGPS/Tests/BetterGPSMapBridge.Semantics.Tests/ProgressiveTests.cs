using BetterGPSNavigation;
internal static class ProgressiveTests
{
    public static async Task Run()
    {
        int count=0;void Check(bool value,string why){count++;if(!value)throw new Exception(why);}
        string U(int i)=>i.ToString("X16");
        var raw=new NativeRoute(U(1),Enumerable.Range(1,20).Select(i=>new NativePhysical(U(i),i==1?0:100,2100-i*100,0,0)).ToArray(),
            Enumerable.Range(1,20).SelectMany(i=>i==20?new[]{new NativeLogical(0,(uint)i,U(i),0,0,0,0)}:
                new[]{new NativeLogical(0,(uint)i,U(i),0,0,0,0),new NativeLogical(1,0,U(i+100),0,0,0,0)}).ToArray());
        var prefix=RouteMapLoader.Prefix(raw,12);
        Check(prefix.Physical.Length==12&&prefix.Logical.Length==23,"Prefix ends at the exact native node, excluding the next prefab");
        Check(prefix.Logical.Where(l=>l.Type==0).Select(l=>l.ItemUid).SequenceEqual(prefix.Physical.Select(p=>p.NodeUid)),"Prefix retains physical/logical agreement");
        Check(prefix.Physical[0].TotalDistanceTillEndM==raw.Physical[0].TotalDistanceTillEndM,"Original navigation-distance origin is preserved");
        var dense=raw with{Nodes=Enumerable.Range(0,200).Select(i=>new NativeNode(i*1280,0,0,0,1,0,0,0,U(0),U(0),false,false,"CONNECTION_UNAVAILABLE")).ToArray()};
        Check(RouteMapLoader.PreviewCounts(dense).SequenceEqual(new[]{121}),"Dense city nodes use an actual 600 metre horizon rather than a dozen tiny segments");
        var action=new LiveAction("action",0,0,50,"TURN_LEFT","branch","HIGH",[new("action","TURN_LEFT","HIGH","PPD",U(101),"branch","exact")]);
        var full=new LiveRoute("test",[new(0,U(101),new(0,0,0),new(100,0,0),0,100,0,100),new(1,U(102),new(100,0,0),new(200,0,0),100,200,100,200)],[action]);
        var part=full with{Edges=full.Edges.Take(1).ToArray(),IsPartial=true};
        var roundtrip=LiveProtocol.Decode(LiveProtocol.Encode(new(2,"RouteSnapshot","test",0,0,1,part))).Route!;
        Check(roundtrip.IsPartial,"Partial marker survives wire roundtrip");
        Check(!System.Text.Json.JsonSerializer.Serialize(full,LiveProtocol.Json).Contains("isPartial"),"Complete route wire format remains unchanged");
        NativeMessage M(string type,long seq,long gen=1,NativeRoute? route=null)=>new(1,type,"session",seq,seq*100,gen,route,
            type=="TelemetryFrame"?new(new(10,0,0),.75,10,false,true,true):null);
        var events=new List<LiveMessage>();using var finish=new ManualResetEventSlim();
        var published=new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var state=new NativeCaptureSession((r,id)=>full,events.Add,(r,id,ready,token)=>{ready(part);published.SetResult();finish.Wait();return full;});
        state.Accept(M("Hello",0));state.Accept(M("RawRouteSnapshot",1,route:raw));await published.Task;
        Check((await state.PollCompile())?.Status=="WAITING_POSITION"&&state.Compiling,"Prefix installs while the full compiler is still blocked");
        Check(state.Accept(M("TelemetryFrame",2),false).Instruction=="TURN_LEFT","Real guidance works before the full route is ready");
        var suffix=raw with{Physical=raw.Physical.Skip(1).ToArray(),Logical=raw.Logical.Skip(2).ToArray()};
        state.Accept(M("RawRouteSnapshot",3,2,suffix));
        Check(state.Accept(M("TelemetryFrame",4,2),false).Instruction=="TURN_LEFT","A native suffix retains the partial guidance during compilation");
        finish.Set();await state.PollCompile(true);
        Check(events.Count(e=>e.Type=="RouteSnapshot")==2&&events.Last(e=>e.Type=="RouteSnapshot").Route?.IsPartial==false,"Full plan replaces the partial plan for the newest suffix generation");
        Check(state.Accept(M("TelemetryFrame",5,2),false).Instruction=="TURN_LEFT","Fresh matching pose resumes guidance after replacement");
        var engine=new LiveGuidanceEngine();engine.Accept(new(2,"Hello","empty",0,0,0));
        engine.Accept(new(2,"RouteSnapshot","empty",1,0,1,part with{Actions=[]}));
        var cruising=engine.Accept(new(2,"VehicleState","empty",2,100,1,Vehicle:new(new(10,0,0),0,10,false)));
        Check(cruising.Instruction=="CONTINUE"&&cruising.DistanceM is null,"Loaded road without another decision displays continue without inventing a turn distance");
        Check(NativeBridgeCli.Describe(cruising)=="Continue","Cruise display omits fictional distance and next action");
        Check(engine.Accept(new(2,"VehicleState","empty",3,200,1,Vehicle:new(new(90,0,0),0,10,false))).Status=="PREPARING_ROUTE","End of a partial plan never claims destination reached");
        using var clearedFinish=new ManualResetEventSlim();var clearedReady=new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var clearedEvents=new List<LiveMessage>();var cleared=new NativeCaptureSession((r,id)=>full,clearedEvents.Add,
            (r,id,ready,token)=>{ready(part);clearedReady.SetResult();clearedFinish.Wait();return full;});
        cleared.Accept(M("Hello",0));cleared.Accept(M("RawRouteSnapshot",1,route:raw));await clearedReady.Task;
        cleared.Accept(M("RouteCleared",2));await cleared.PollCompile();clearedFinish.Set();await cleared.Drain();
        Check(clearedEvents.All(e=>e.Type!="RouteSnapshot"),"Neither queued prefix nor background completion can revive a cleared route");
        var started=new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);int builds=0;bool cancelled=false;
        var replacementEvents=new List<LiveMessage>();var replacement=new NativeCaptureSession((r,id)=>full,replacementEvents.Add,
            (r,id,ready,token)=>{
                if(Interlocked.Increment(ref builds)==1){ready(part);started.SetResult();token.WaitHandle.WaitOne();cancelled=token.IsCancellationRequested;token.ThrowIfCancellationRequested();}
                return full;
            });
        replacement.Accept(M("Hello",0));replacement.Accept(M("RawRouteSnapshot",1,route:raw));await started.Task;
        await replacement.PollCompile();
        replacement.Accept(new(1,"CaptureStatus","session",2,200,2,Status:"RECALCULATING"));
        replacement.Accept(M("RawRouteSnapshot",3,2,suffix));
        await replacement.PollCompile(true);await replacement.PollCompile(true);
        Check(cancelled&&builds==2,"A cancelled compiler is restarted even when the next native snapshot is a suffix");
        Check(replacementEvents.Last(e=>e.Type=="RouteSnapshot").Route?.IsPartial==false,"Cancelled background work cannot prevent the replacement full route from installing");
        var direct=new NetworkIdentity("I-5","RouteReference","NORTH","MEDIUM","MEDIUM",["proof"],"DIRECT_NETWORK_ANCHOR");
        var network=new NamedNetworkSnapshot(1,"fingerprint","policy",new(-1,1,-1,1),[],[],[],[],
            [new("a","RESOLVED",[direct,direct with{Rule="BRACKETED_MANDATORY_NETWORK_CORRIDOR"}],[])]);
        var filtered=PartialRoadEvidence.Filter(network);
        Check(filtered.Names[0].Identities.Length==1&&network.Names[0].Identities.Length==2,"Partial proof filtering keeps local evidence and leaves the cached complete graph intact");
        Console.WriteLine($"PASS: {count} progressive preparation and generation assertions.");
    }
}
