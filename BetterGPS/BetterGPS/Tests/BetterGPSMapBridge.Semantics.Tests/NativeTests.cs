using BetterGPSNavigation;
using System.Diagnostics;
using System.IO.Pipes;
using System.Text;
using System.Text.Json;

internal static class NativeTests
{
    public static async Task MapIntegration(string fixture,string game,string docs,string output)
    {
        string pipe="BetterGPSMapTest-"+Guid.NewGuid().ToString("N");
        var receiver=Task.Run(()=>NativeBridgeCli.Run(["serve","--once","--pipe",pipe,"--game-root",game,
            "--documents",docs,"--output",output]));
        using var timeout=new CancellationTokenSource(TimeSpan.FromMinutes(10));
        await using(var client=new NamedPipeClientStream(".",pipe,PipeDirection.Out,PipeOptions.Asynchronous))
        {
            await client.ConnectAsync(timeout.Token);
            var frames=File.ReadLines(fixture).Take(3).Select(l=>NativeCaptureProtocol.Decode(Encoding.UTF8.GetBytes(l))).ToArray();
            await NativeCaptureProtocol.WriteAsync(client,frames[0],timeout.Token);
            await NativeCaptureProtocol.WriteAsync(client,frames[1],timeout.Token);
            var clock=Stopwatch.StartNew();
            int posesAfterPreparation=0;
            for(int i=0;i<2400;i++)
            {
                await Task.Delay(100,timeout.Token);
                await NativeCaptureProtocol.WriteAsync(client,frames[2] with {Sequence=i+2,TimestampMs=clock.ElapsedMilliseconds},timeout.Token);
                if(i%10==0)
                {
                    var currentFolder=Directory.GetDirectories(output).OrderByDescending(d=>d).First();
                    var log=Path.Combine(currentFolder,"compiled.jsonl");
                    using var tail=new StreamReader(new FileStream(log,FileMode.Open,FileAccess.Read,FileShare.ReadWrite));
                    if((await tail.ReadToEndAsync()).Contains("\"type\":\"RouteSnapshot\"")) posesAfterPreparation++;
                    if(posesAfterPreparation>=3) break;
                }
            }
        }
        if(await receiver!=0) throw new Exception("Live map receiver failed.");
        VerifyMapRecording(output);
    }
    public static void VerifyMapRecording(string output)
    {
        var folder=Directory.GetDirectories(output).OrderByDescending(d=>d).First();
        var messages=File.ReadLines(Path.Combine(folder,"compiled.jsonl"))
            .Select(l=>LiveProtocol.Decode(Encoding.UTF8.GetBytes(l))).ToArray();
        var plan=messages.Single(m=>m.Type=="RouteSnapshot").Route!;
        if(plan.Actions.Length!=11) throw new Exception("Expected 11 prepared actions.");
        var guidance=File.ReadLines(Path.Combine(folder,"guidance.jsonl"))
            .Select(l=>JsonSerializer.Deserialize<LiveGuidance>(l,LiveProtocol.Json)!).ToArray();
        if(!guidance.Any(g=>g.Instruction=="TURN_RIGHT") || guidance[^1].Status!="DISCONNECTED")
            throw new Exception("Live console guidance did not activate/suppress on disconnect.");
        var engine=new LiveGuidanceEngine();
        LiveGuidance? replay=null;
        foreach(var m in messages) replay=engine.Accept(m);
        if(replay!.Status!="DISCONNECTED" || replay.Instruction is not null) throw new Exception("Compiled replay differs after disconnect.");
        Console.WriteLine("PASS: live pipe -> raw route -> installed map -> 11 prepared actions -> fresh pose -> console -> replay -> disconnect.");
    }
    public static async Task Run(string? nativeExecutable)
    {
        int count=0;
        void Check(bool ok,string why) { count++; if(!ok) throw new Exception(why); }
        void Reject(Action call,string why)
        { bool rejected=false; try { call(); } catch(InvalidDataException) { rejected=true; } Check(rejected,why); }
        var raw=new NativeRoute("0000000000000001",
            [new("0000000000000001",0,3000,0,30),new("0000000000000002",1000,2000,10,20),new("0000000000000003",1000,1000,10,10)],
            [new(0,0,"0000000000000001",0,3000,0,30),new(0,1,"0000000000000002",1000,2000,10,20),
                new(0,2,"0000000000000003",1000,1000,10,10)]);
        var route=new LiveRoute("test",
            Enumerable.Range(0,3).Select(i=>new LiveEdge(i,"0000000000000001",new(i*100,0,0),new((i+1)*100,0,0),i*100,(i+1)*100,i*1000,(i+1)*1000)).ToArray(),
            [new("action",1,1,1500,"TURN_LEFT","branch","HIGH",
                [new("action","TURN_LEFT","HIGH","PPD","0000000000000001","branch","validated")])]);
        NativeVehicle V(double x=50,double speed=10,bool paused=false)=>new(new(x,0,0),0.75,speed,paused,true,true);
        NativeMessage M(string type,long seq,long gen=1,NativeRoute? r=null,NativeVehicle? v=null,string? status=null)
            =>new(1,type,"native",seq,seq*100,gen,r,v,status);
        Check(V().ToLive().HeadingDeg==0,"SCS east -> engine east");
        Check((V() with {ScsHeading=0}).ToLive().HeadingDeg==270,"SCS north -> engine north");
        Check((V() with {ScsHeading=0.25}).ToLive().HeadingDeg==180,"SCS west -> engine west");
        Check((V() with {ScsHeading=0.5}).ToLive().HeadingDeg==90,"SCS south -> engine south");
        var snapshot=M("RawRouteSnapshot",1,r:raw);
        Check(NativeCaptureProtocol.Decode(NativeCaptureProtocol.Encode(snapshot)).Route!.Physical.Length==3,"Native JSON roundtrip");
        Reject(()=>NativeCaptureProtocol.Validate(snapshot with {Route=raw with {Physical=raw.Physical.Reverse().ToArray()}}),"Physical/logical mismatch");
        Reject(()=>NativeCaptureProtocol.Validate(snapshot with {Route=raw with {Fingerprint="NOT_A_HEX_VALUE!"}}),"Invalid fingerprint");
        Reject(()=>NativeCaptureProtocol.Validate(M("TelemetryFrame",2,v:V() with {ScsHeading=1})),"Invalid heading");
        Reject(()=>NativeCaptureProtocol.Validate(M("TelemetryFrame",2,v:V() with {SignedSpeedMps=double.NaN})),"Nonfinite speed");
        using(var stream=new MemoryStream())
        {
            await NativeCaptureProtocol.WriteAsync(stream,snapshot); stream.Position=0;
            Check((await NativeCaptureProtocol.ReadAsync(stream))!.Route!.Logical.Length==3,"Native framing");
            Check(await NativeCaptureProtocol.ReadAsync(stream) is null,"Clean EOF");
        }
        int compiles=0; var recorded=new List<LiveMessage>();
        var state=new NativeCaptureSession((r,id)=>{compiles++;return route with {RouteId=id};},recorded.Add);
        Check(state.Accept(M("Hello",0,0),false).Status=="WAITING_ROUTE","Native Hello");
        Check(state.Accept(snapshot,false).Status=="PREPARING_ROUTE","Raw triggers preparation");
        Check((await state.PollCompile(true))!.Status=="WAITING_POSITION","Fresh pose needed after preparation");
        var active=state.Accept(M("TelemetryFrame",2,v:V()),false);
        Check(active.Instruction=="TURN_LEFT" && active.DistanceM==1000,"Live route prepared and projected");
        var reverse=state.Accept(M("TelemetryFrame",3,v:V(45,speed:-2)),false);
        Check(reverse.Instruction=="TURN_LEFT"&&reverse.DistanceM==1050,"Reverse must keep maneuver and increase distance when backing away");
        Check(V(speed:-2).ToLive().Reversing&&V(speed:-2).ToLive().HeadingDeg==0,"Reverse must preserve truck heading and mark backward progress");
        Check(state.Accept(M("TelemetryFrame",4,v:V(paused:true)),false).Instruction is null,"Paused instruction suppressed");
        Check(state.Accept(M("TelemetryFrame",5,v:V() with {PoseValid=false}),false).Status=="NO_TELEMETRY","Null channel suppressed");
        Check(state.Accept(M("TelemetryFrame",6,v:V()),false).Instruction=="TURN_LEFT","Telemetry recovery");
        Check(state.Accept(M("CaptureStatus",7,2,status:"RECALCULATING"),false).Instruction is null,"Recalculation suppresses old guidance");
        Check(state.Accept(M("TelemetryFrame",8,2,v:V()),false).Instruction is null,"Changing route cannot resume old action");
        var suffix=raw with {Physical=raw.Physical.Skip(1).ToArray(),Logical=raw.Logical.Skip(1).ToArray()};
        Check(suffix.IsSuffixOf(raw),"Ordered suffix accepted");
        Check(!(suffix with {Logical=suffix.Logical.Reverse().ToArray()}).IsSuffixOf(raw),"Different branch not a suffix");
        Check(state.Accept(M("RawRouteSnapshot",9,3,r:suffix),false).Status=="WAITING_POSITION","Suffix retains station axis");
        Check(compiles==1,"Suffix does not recompile");
        Check(state.Accept(M("TelemetryFrame",10,3,v:V()),false).Instruction=="TURN_LEFT","Suffix resumes original action");
        var backing=new NativeCaptureSession((r,id)=>route with{RouteId=id},_=>{});
        backing.Accept(M("Hello",0,0),false);backing.Accept(snapshot,false);await backing.PollCompile(true);
        Check(backing.Accept(M("TelemetryFrame",2,v:V(145)),false).Status=="EXECUTE","Reverse fixture must start near maneuver");
        var withdrawn=backing.Accept(M("TelemetryFrame",3,v:V(135,speed:-2)),false);
        Check(withdrawn.Status=="PREPARE"&&withdrawn.DistanceM==150,"Backing away must relax urgency and refresh distance");
        backing.Accept(M("TelemetryFrame",4,v:V(205)),false);
        var restored=backing.Accept(M("TelemetryFrame",5,v:V(195,speed:-2)),false);
        Check(restored.Instruction=="TURN_LEFT","Reverse across a completed segment must restore the relevant maneuver");
        var replayReverse=LiveProtocol.Decode(LiveProtocol.Encode(new(2,"VehicleState","reverse",0,0,1,Vehicle:V(speed:-2).ToLive())));
        Check(replayReverse.Vehicle!.Reversing,"Reverse progress flag lost in compiled replay");
        bool sourceChanged=false;int refreshedCompiles=0;
        var refreshed=new NativeCaptureSession((r,id)=>{refreshedCompiles++;return route with{RouteId=id};},_=>{},sourcesChanged:()=>{bool changed=sourceChanged;sourceChanged=false;return changed;});
        refreshed.Accept(M("Hello",0,0),false);refreshed.Accept(snapshot,false);await refreshed.PollCompile(true);
        sourceChanged=true;
        Check(refreshed.Accept(M("RawRouteSnapshot",2,2,r:suffix),false).Status=="PREPARING_ROUTE","Changed map reused a prepared suffix");
        await refreshed.PollCompile(true);
        Check(refreshedCompiles==2,"Changed resources did not recompile identical topology");
        Check(state.Accept(M("RouteCleared",11,4),false).Status=="NO_ROUTE","Clear invalidates plan");
        Check(state.Accept(M("TelemetryFrame",12,4,v:V()),false).Instruction is null,"Clear never resumes old action");
        Reject(()=>state.Accept(M("TelemetryFrame",12,4,v:V()),false),"Duplicate native sequence");
        Reject(()=>state.Accept(M("TelemetryFrame",13,3,v:V()),false),"Old native generation");
        var replay=new LiveGuidanceEngine(); LiveGuidance? replayLast=null;
        foreach(var m in recorded) replayLast=replay.Accept(LiveProtocol.Decode(LiveProtocol.Encode(m)));
        Check(replayLast!.Instruction is null && replayLast.Status=="NO_ROUTE","Suspensions survive compiled replay");
        var tail=recorded[^1];
        replay.Accept(new(1,"SourceState",tail.SessionId,tail.Sequence+1,tail.TimestampMs+1,tail.Revision,
            Status:"GPS_UNAVAILABLE",Detail:"Lost GPS capture."));
        Check(replay.Accept(new(1,"Heartbeat",tail.SessionId,tail.Sequence+2,tail.TimestampMs+10000,tail.Revision)).Status=="GPS_UNAVAILABLE",
            "A heartbeat must retain explicit source suspension");

        using var gate=new ManualResetEventSlim(false); int running=0,maxRunning=0,number=0;
        var concurrent=new NativeCaptureSession((r,id)=>
        {
            int n=Interlocked.Increment(ref running); maxRunning=Math.Max(maxRunning,n);
            Interlocked.Increment(ref number); gate.Wait(); Interlocked.Decrement(ref running);
            return route with {RouteId=id};
        },_=>{});
        concurrent.Accept(M("Hello",0,0),false);
        concurrent.Accept(snapshot,false);
        // A paused frame must not cause the finished compiler result to be dropped.
        concurrent.Accept(M("TelemetryFrame",2,v:V(paused:true)),false);
        gate.Set(); Check((await concurrent.PollCompile(true))!.Status=="WAITING_POSITION","Preparation finishes even while paused");
        gate.Reset();
        concurrent.Accept(M("RawRouteSnapshot",3,2,r:raw with {Fingerprint="0000000000000002"}),false);
        // Same topology is legitimately retained; force new branch topology for the next compile.
        var changed=raw with {Physical=raw.Physical.Reverse().ToArray(),Logical=raw.Logical.Reverse().ToArray(),Fingerprint="0000000000000003"};
        concurrent.Accept(M("RawRouteSnapshot",4,3,r:changed),false);
        concurrent.Accept(M("RawRouteSnapshot",5,4,r:changed with {Fingerprint="0000000000000004"}),false);
        gate.Set(); Check((await concurrent.PollCompile(true))!.Status=="WAITING_POSITION","Identical latest route adopts pending preparation");
        Check(maxRunning==1 && number==2,"One compiler at a time; same topology avoids restart");
        gate.Reset();
        concurrent.Accept(M("RawRouteSnapshot",6,5,r:raw),false);
        concurrent.Accept(M("RawRouteSnapshot",7,6,r:changed),false);
        gate.Set();
        Check(await concurrent.PollCompile(true) is null,"Obsolete compiler result discarded");
        Check(concurrent.Disconnect().Status=="DISCONNECTED","Disconnect immediately suppresses guidance");

        gate.Reset(); int suffixCompiles=0;
        var shortening=new NativeCaptureSession((r,id)=>{suffixCompiles++;gate.Wait();return route;},_=>{});
        shortening.Accept(M("Hello",0,0),false); shortening.Accept(snapshot,false);
        shortening.Accept(M("RawRouteSnapshot",2,2,r:suffix),false);
        shortening.Accept(M("TelemetryFrame",3,2,v:V(paused:true)),false);
        gate.Set();
        Check((await shortening.PollCompile(true))!.Status=="WAITING_POSITION" && suffixCompiles==1,
            "A route shrinking during preparation adopts the original job");
        Check(shortening.Accept(M("TelemetryFrame",4,2,v:V(150)),false).Instruction=="TURN_LEFT",
            "A fresh suffix-generation pose resumes the original station axis");

        if(nativeExecutable is not null)
        {
            using var timeout=new CancellationTokenSource(TimeSpan.FromSeconds(15));
            var testPipe="BetterGPSNativeTest-"+Guid.NewGuid().ToString("N");
            await using var pipe=new NamedPipeServerStream(testPipe,PipeDirection.In,1,PipeTransmissionMode.Byte,
                PipeOptions.Asynchronous|PipeOptions.CurrentUserOnly,4096,4096);
            using var process=Process.Start(new ProcessStartInfo(Path.GetFullPath(nativeExecutable),testPipe) {UseShellExecute=false,
                RedirectStandardOutput=true,RedirectStandardError=true,CreateNoWindow=true})!;
            await pipe.WaitForConnectionAsync(timeout.Token);
            var hello=await NativeCaptureProtocol.ReadAsync(pipe,timeout.Token);
            Check(hello!.Type=="Hello","Native C++ worker sends Hello");
            var control=await NativeCaptureProtocol.ReadAsync(pipe,timeout.Token);
            Check(control!.Type=="RawRouteSnapshot" && control.Route!.Physical.Length==2,"C++ route decodes in .NET");
            var telemetry=await NativeCaptureProtocol.ReadAsync(pipe,timeout.Token);
            Check(telemetry!.Type=="TelemetryFrame" && telemetry.Vehicle!.ToLive().HeadingDeg==0,"Native C++ pose decodes in .NET");
            Check(control!.Version==2 && control.Route!.Nodes?.Length==2,"C++ V2 node evidence decodes");
            var events=new List<NativeMessage>();
            while(await NativeCaptureProtocol.ReadAsync(pipe,timeout.Token) is { } e)events.Add(e);
            Check(events.Any(e=>e.Type=="CaptureStatus"&&e.Generation==2)&&events.Any(e=>e.Type=="RawRouteSnapshot"&&e.Generation==3),"Native control transitions delivered in queue");
            Check(events.Any(e=>e.Type=="TelemetryFrame"&&e.Generation==3),"Fresh telemetry follows new route generation");
            await process.WaitForExitAsync(timeout.Token);
            Check(process.ExitCode==0,"Native worker shutdown and SDK ABI harness passed: "+await process.StandardError.ReadToEndAsync());
        }
        Console.WriteLine($"PASS: {count} native capture and preparation assertions.");
    }
}
