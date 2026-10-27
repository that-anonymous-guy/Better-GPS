using System.Buffers.Binary;
using System.Text;
using System.Text.Json;
using System.IO.Pipes;
using BetterGPSNavigation;

internal static class LiveTests
{
    public static async Task Run(string? artifactDirectory)
    {
        int count = 0;
        void Check(bool ok,string why) { count++; if (!ok) throw new Exception(why); }
        void Reject(Action action,string why)
        {
            bool rejected = false;
            try { action(); } catch (InvalidDataException) { rejected = true; }
            Check(rejected,why);
        }
        var edges = Enumerable.Range(0,5).Select(i => new LiveEdge(i,$"{i:X16}",new(i*100,0,0),
            new((i+1)*100,0,0),i*100,(i+1)*100,i*1000,(i+1)*1000)).ToArray();
        LiveAction ActionAt(string id,int segment,double station,string instruction) => new(id,segment,segment,station,instruction,
            $"branch{segment}","HIGH",[
                new("action",instruction,"HIGH","PPD",$"{segment:X16}",$"branch{segment}","EXACT entry=0 exit=1 active lane=0"),
                new("road","I-5","HIGH","SIGN_BOARD","sign:panel",$"branch{segment}","typed route shield"),
                new("cardinal","NORTH","HIGH","SIGN_BOARD","sign:panel",$"branch{segment}","explicit NORTH"),
                new("destination","Sacramento","HIGH","SIGN_BOARD","sign:panel",$"branch{segment}","known destination; selected branch"),
                new("exit","57A","HIGH","SIGN_BOARD","sign:panel",$"branch{segment}","explicit EXIT 57A")]);
        var route = new LiveRoute("routeA",edges,[ActionAt("a",1,1500,"TURN_LEFT"),ActionAt("b",3,3500,"KEEP_RIGHT")]);
        var messages = new List<LiveMessage>();
        LiveMessage M(string type,long seq,long time,long rev,LiveRoute? r=null,LiveVehicle? v=null) => new(1,type,"sessionA",seq,time,rev,r,v);
        LiveVehicle V(double x,bool pause=false,double y=0,double heading=0) => new(new(x,y,0),heading,10,pause,9000);
        var engine = new LiveGuidanceEngine();
        LiveGuidance Push(LiveMessage m) { messages.Add(m); return engine.Accept(m); }
        Check(Push(M("Hello",0,0,0)).Status == "WAITING_ROUTE","Hello");
        Check(Push(M("RouteSnapshot",1,0,1,route)).Status == "WAITING_POSITION","Route install");
        var first = Push(M("VehicleState",2,100,1,v:V(50)));
        Check(first.Instruction == "TURN_LEFT" && first.DistanceM == 1000,"Navigation station distance, not world distance");
        Check(Math.Abs(first.DistanceMi!.Value-1000/1609.344) < 1e-12,"Miles conversion");
        Check(first.Road == "I-5" && first.Cardinal == "NORTH" && first.Destination == "Sacramento" && first.Exit == "57A","Independent semantic fields");
        Check(first.ThenInstruction == "KEEP_RIGHT" && first.ThenDistanceM == 2000,"Then uses physical gap but reports nav distance");
        Check(first.Proofs.Length == 5,"Proofs retained");
        Check(NativeBridgeCli.SimpleAction("CONTINUE_AT_FORK")=="stay"&&NativeBridgeCli.SimpleAction("U_TURN_LEFT")=="turn around",
            "Then summary preserves straight forks and U-turn meaning");
        Check(NativeBridgeCli.Describe(first with{TargetRoad=new("UNKNOWN",[],[],"OUTGOING_UNKNOWN")}).Contains("onto I-5 NORTH"),
            "A verified selected sign remains visible when the road database lacks an outgoing name");
        Check(NativeBridgeCli.Describe(first with{DistanceM=30,DistanceMi=30/1609.344}).Contains("100 ft"),
            "Short countdown does not round a pending action to zero miles");
        Check(NativeBridgeCli.Describe(first with{Status="EXECUTE"}).Contains("turn left now"),
            "Execution phase displays an immediate instruction");
        Check(Push(M("VehicleState",3,200,1,v:V(135))).Status == "EXECUTE","Execution phase");
        var zero = Push(M("VehicleState",4,300,1,v:V(180)));
        Check(zero.ActionId == "a" && zero.DistanceM == 0,"Do not finish at zero distance inside prefab");
        Check(Push(M("VehicleState",5,400,1,v:V(220))).ActionId == "b","Finish on outgoing segment");
        Check(Push(M("VehicleState",6,500,1,v:V(225,true))).Status == "PAUSED","Pause suppresses instructions");
        Check(Push(M("Heartbeat",7,800,1)).Status == "PAUSED","Pause stable on heartbeat");
        Check(Push(M("VehicleState",8,900,2,v:V(225))).Status == "WAITING_ROUTE","Revision mismatch suppresses instruction");
        Check(Push(M("VehicleState",9,1000,1,v:V(230))).ActionId == "b","Correct revision resumes");
        Check(engine.Tick(3101).Status == "STALE","Stale timeout");
        Check(Push(M("VehicleState",10,3200,1,v:V(240))).ActionId == "b","Recovery after stale");
        Reject(() => engine.Accept(M("VehicleState",10,3300,1,v:V(245))),"Duplicate sequence");
        Reject(() => engine.Accept(M("VehicleState",11,3100,1,v:V(245))),"Producer clock backwards");
        Reject(() => engine.Accept(M("RouteSnapshot",11,3300,1,route)),"Old route revision");
        // Local timeout clock may run ahead of a delayed, but valid producer frame.
        engine.Tick(3500);
        Check(Push(M("VehicleState",11,3400,1,v:V(245))).ActionId == "b","Local ticking must not reject valid producer timestamps");
        var changed = route with { RouteId="routeB",Actions=[ActionAt("c",3,3500,"TURN_RIGHT")] };
        Check(Push(M("RouteSnapshot",12,3600,2,changed)).Status == "WAITING_POSITION","Same-size recalculation");
        Check(Push(M("VehicleState",13,3700,2,v:V(260))).Instruction == "TURN_RIGHT","New branch after recalculation");
        Check(Push(M("RouteCleared",14,3800,3)).Status == "NO_ROUTE","Clear");
        Check(Push(M("VehicleState",15,3900,3,v:V(265))).Instruction is null,"No stale action after clear");
        Check(Push(M("Hello",0,0,0) with {SessionId="sessionB"}).Status == "WAITING_ROUTE","New session resets sequence and clock");
        Check(Push(M("RouteSnapshot",1,0,1,route) with {SessionId="sessionB"}).Status == "WAITING_POSITION","New session route");
        Check(Push(M("VehicleState",2,100,1,v:V(50)) with {SessionId="sessionB"}).ActionId=="a","New session position");
        Check(Push(M("Heartbeat",3,2201,1) with {SessionId="sessionB"}).Status=="STALE","Replay heartbeat expires guidance");
        Check(Push(M("VehicleState",4,2300,1,v:V(60)) with {SessionId="sessionB"}).ActionId=="a","Replay stale recovery");

        LiveGuidance Evaluate(LiveRoute r,LiveVehicle v)
        {
            var e = new LiveGuidanceEngine(); e.Accept(M("Hello",0,0,0)); e.Accept(M("RouteSnapshot",1,0,1,r));
            return e.Accept(M("VehicleState",2,100,1,v:v));
        }
        var action = route.Actions[0];
        LiveRoute Replace(LiveProof[] proofs,string confidence="HIGH") => route with { Actions=[action with {Proofs=proofs,Confidence=confidence}] };
        Check(Evaluate(Replace(action.Proofs.Select(p => p.Field=="destination" ? p with {BranchId="other"} : p).ToArray()),V(50)).Destination is null,"Cross-branch destination blocked");
        Check(Evaluate(Replace(action.Proofs.Select(p => p.Field=="destination" ? p with {Value="LEFT LANE"} : p).ToArray()),V(50)).Destination is null,"Lane text never destination");
        Check(Evaluate(Replace(action.Proofs.Select(p => p.Field=="road" ? p with {Confidence="MEDIUM"} : p).ToArray()),V(50)).Cardinal is null,"Cardinal requires validated road");
        Check(Evaluate(Replace([..action.Proofs,new("road","US-50","HIGH","SIGN_BOARD","sign2:panel",action.BranchId,"conflict")]),V(50)).Road is null,"Conflicting roads suppressed");
        Check(Evaluate(Replace(action.Proofs,"MEDIUM"),V(50)).Status == "UNCERTAIN_ACTION","Unproven action suppressed");
        Check(Evaluate(route,V(50,y:15)).Status == "OFF_ROUTE","Altitude gating");
        Check(Evaluate(route,V(50,heading:180)).Status == "OFF_ROUTE","Opposite direction gating");
        var stacked = route with {Edges=[..edges.Select(e=>e with {Start=e.Start with {Y=10},End=e.End with {Y=10}})]};
        Check(Evaluate(stacked,V(50,y:10)).DistanceM==1000,"Correct elevated route");
        Check(Evaluate(route,V(450)).Instruction == "CONTINUE","Last maneuver passed still needs guidance to the destination");
        Check(Evaluate(route,V(500) with{NavigationRemainingM=0}).Status == "COMPLETE","Destination reached at the route endpoint");
        var noThen = route with {Actions=[action,ActionAt("far",4,4900,"KEEP_RIGHT")]};
        Check(Evaluate(noThen,V(50)).ThenInstruction == "KEEP_RIGHT","Next action stays available at long distances");
        var longRoute=route with {Edges=edges.Select(e=>e with{NavStartM=e.NavStartM*100,NavEndM=e.NavEndM*100}).ToArray(),
            Actions=route.Actions.Select(a=>a with{NavStationM=a.NavStationM*100}).ToArray()};
        var longGuidance=Evaluate(longRoute,V(50));
        Check(longGuidance.DistanceM==100000&&Math.Abs(longGuidance.DistanceMi!.Value-62.1371192237)<1e-8,
            "Long route preserves navigation metres and miles, independent of world scale");
        Check(longGuidance.ThenDistanceM==200000,"Long next-action gap preserves navigation units");
        var calibrated=longRoute with{NavigationRemainingAtOriginM=600000};
        var measured=Evaluate(calibrated,V(50) with{NavigationRemainingM=549000});
        Check(measured.DistanceM==99000&&measured.Detail.Contains("ATS_REMAINING_CALIBRATED"),
            "Long-distance countdown uses ATS metres with independently compatible world position");
        Check(Evaluate(calibrated,V(50) with{NavigationRemainingM=400000}).DistanceM==100000,
            "Unrelated remaining distance cannot relocate the truck or change the countdown");
        Reject(()=>LiveProtocol.Validate(M("RouteSnapshot",1,0,1,longRoute with{NavigationRemainingAtOriginM=double.NaN})),
            "Non-finite navigation origin rejected");
        Reject(()=>LiveProtocol.Validate(M("RouteSnapshot",1,0,1,longRoute with{NavigationRemainingAtOriginM=1})),
            "Navigation origin shorter than selected route rejected");
        Reject(()=>LiveProtocol.Validate(M("VehicleState",2,0,1,v:V(double.NaN))),"Non-finite pose");
        Reject(()=>LiveProtocol.Validate(M("Hello",0,0,0) with {Version=3}),"Unsupported version");
        Reject(()=>LiveProtocol.Validate(M("Hello",0,0,0,v:V(0))),"Wrong payload");
        Reject(()=>LiveProtocol.Validate(M("RouteSnapshot",1,0,1,route with {Actions=[action,action]})),"Duplicate event IDs");
        Reject(()=>LiveProtocol.Validate(M("RouteSnapshot",1,0,1,route with {Edges=edges.Reverse().ToArray()})),"Unordered corridor");
        await using var encoded = new MemoryStream();
        var sample = M("RouteSnapshot",1,0,1,route);
        await LiveProtocol.WriteAsync(encoded,sample); encoded.Position=0;
        var decoded = await LiveProtocol.ReadAsync(encoded);
        Check(JsonSerializer.Serialize(decoded,LiveProtocol.Json)==JsonSerializer.Serialize(sample,LiveProtocol.Json),"Pipe round trip");
        Check(await LiveProtocol.ReadAsync(encoded) is null,"Clean pipe EOF");
        await using var broken = new MemoryStream([4,0,0,0,123]);
        bool truncated=false; try { await LiveProtocol.ReadAsync(broken); } catch (EndOfStreamException) {truncated=true;}
        Check(truncated,"Truncated pipe rejected");
        var oversized = new byte[4]; BinaryPrimitives.WriteInt32LittleEndian(oversized,LiveProtocol.MaxPayloadBytes+1);
        await using var tooLarge = new MemoryStream(oversized);
        bool limited=false; try {await LiveProtocol.ReadAsync(tooLarge);} catch(InvalidDataException) {limited=true;}
        Check(limited,"Oversized pipe rejected before allocation");
        string Replay()
        {
            var e=new LiveGuidanceEngine();
            return string.Join('\n',messages.Select(m=>JsonSerializer.Serialize(e.Accept(m),LiveProtocol.Json)));
        }
        Check(Replay()==Replay(),"Deterministic replay");
        var pipeName = "BetterGPS-test-"+Guid.NewGuid().ToString("N");
        using var pipeTimeout = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        await using var server = new NamedPipeServerStream(pipeName,PipeDirection.In,1,
            PipeTransmissionMode.Byte,PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
        var received = Task.Run(async () =>
        {
            await server.WaitForConnectionAsync(pipeTimeout.Token);
            var pipeEngine = new LiveGuidanceEngine();
            var outputs = new List<string>();
            while (await LiveProtocol.ReadAsync(server,pipeTimeout.Token) is { } message)
                outputs.Add(JsonSerializer.Serialize(pipeEngine.Accept(message),LiveProtocol.Json));
            return string.Join('\n',outputs);
        });
        await using (var client = new NamedPipeClientStream(".",pipeName,PipeDirection.Out,PipeOptions.Asynchronous))
        {
            await client.ConnectAsync(pipeTimeout.Token);
            foreach (var message in messages) await LiveProtocol.WriteAsync(client,message,pipeTimeout.Token);
        }
        Check(await received == Replay(),"Actual named-pipe guidance equals file replay");
        var framedBytes = encoded.ToArray();
        await using var fragmented = new FragmentedStream(framedBytes);
        Check(JsonSerializer.Serialize(await LiveProtocol.ReadAsync(fragmented),LiveProtocol.Json) ==
            JsonSerializer.Serialize(sample,LiveProtocol.Json),"Fragmented pipe frame handled");
        if (artifactDirectory is not null)
        {
            Directory.CreateDirectory(artifactDirectory);
            File.WriteAllLines(Path.Combine(artifactDirectory,"scenarios.jsonl"),messages.Select(m=>Encoding.UTF8.GetString(LiveProtocol.Encode(m))));
            File.WriteAllText(Path.Combine(artifactDirectory,"scenarios.expected.jsonl"),Replay()+"\n");
        }
        Console.WriteLine($"PASS: {count} live protocol/progression assertions.");
    }

    private sealed class FragmentedStream(byte[] bytes) : MemoryStream(bytes)
    {
        public override ValueTask<int> ReadAsync(Memory<byte> buffer,CancellationToken token=default) =>
            base.ReadAsync(buffer[..Math.Min(buffer.Length,3)],token);
    }
}
