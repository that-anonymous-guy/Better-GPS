using System.IO.Pipes;
using System.Text;
using System.Text.Json;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

public static class NativeBridgeCli
{
    public static async Task<int> Run(string[] args)
    {
        try
        {
            if(args.Length==0 || args[0]=="help")
            {
                Console.WriteLine("""
Native capture v1/v2 (ATS 1.61 x64)
  --capture serve --game-root DIR --documents DIR --output DIR [--pipe BetterGPSProbe-v1]
  --capture replay --game-root DIR --documents DIR --output DIR --input capture.jsonl
  --capture prepare-map --game-root DIR --documents DIR --cache DIR --output DIR
  --capture fixture --probe DIR --stem STEM --poses synthetic.jsonl --output native.jsonl
  --capture send --input native.jsonl [--pipe BetterGPSProbe-v1] [--pace]
serve is ready after mounting resources; route regions are loaded on demand.
serve also prepares persistent world pages in the background, yielding priority to route guidance.
prepare-map finishes all mounted map pages; optional --input restricts an offline benchmark to recorded corridors.
--no-world-cache retains the original readers for independent comparison.
Prepared routes and asset indexes are reused only for the same installed game/mod set.
--no-route-cache benchmarks a fresh route; --full-map retains complete-map diagnostics.
--sign-calibration FILE loads render/editor profiles pinned to model, board and effective texture hashes.
Each compiled route also writes *_groups.html for group, placement, lane and GPS evidence.
fixture uses synthetic poses and is only a test. Live recordings are capture.jsonl + compiled.jsonl.
"""); return 0;
            }
            var mode=args[0]; var opts=new Dictionary<string,string>(StringComparer.Ordinal);
            if(mode=="serve")RuntimeDiagnostics.Start();
            for(int i=1;i<args.Length;i++)
            {
                if(args[i] is "--pace" or "--once" or "--require-node-evidence" or "--full-map" or "--no-route-cache" or "--no-world-cache" or "--warm-start" or "--diagnostics") { opts.Add(args[i],"true"); continue; }
                if(!args[i].StartsWith("--") || i+1>=args.Length) throw new ArgumentException("Expected --option value.");
                opts.Add(args[i],args[++i]);
            }
            string Need(string name)=>opts.TryGetValue(name,out var v)?v:throw new ArgumentException($"Missing {name}.");
            if(mode=="fixture")
            {
                var probe=ProbeRoute.LoadAll(Need("--probe")).Single(p=>p.Stem==Need("--stem"));
                var raw=new NativeRoute("0000000000000001",
                    probe.Physical.Select(p=>new NativePhysical(p.NodeUid.ToString("X16"),p.DistanceFromPreviousM,
                        p.TotalDistanceTillEndM,p.TimeFromPreviousS,p.TotalTimeTillEndS)).ToArray(),
                    probe.Logical.Select(p=>new NativeLogical(p.Type,p.NodeId,p.ItemUid.ToString("X16"),p.DistanceFromPreviousM,
                        p.TotalDistanceTillEndM,p.TimeFromPreviousS,p.TotalTimeTillEndS)).ToArray());
                using var file=Output(Need("--output")); string session=Guid.NewGuid().ToString("N"); long seq=0;
                void Write(NativeMessage m)=>file.WriteLine(Encoding.UTF8.GetString(NativeCaptureProtocol.Encode(m)));
                Write(new(1,"Hello",session,seq++,0,0,Status:"SYNTHETIC_FIXTURE"));
                Write(new(1,"RawRouteSnapshot",session,seq++,0,1,raw));
                foreach(var line in File.ReadLines(Need("--poses")).Where(l=>!string.IsNullOrWhiteSpace(l)))
                {
                    var message=LiveProtocol.Decode(Encoding.UTF8.GetBytes(line));
                    if(message.Vehicle is not { } v) continue;
                    double heading=(((-90-v.HeadingDeg)/360)%1+1)%1;
                    Write(new(1,"TelemetryFrame",session,seq++,message.TimestampMs,1,Vehicle:
                        new(v.Position,heading,v.SpeedMps,v.Paused,true,true,v.NavigationRemainingM)));
                }
                Console.WriteLine($"Synthetic fixture: {seq} frames; not real truck telemetry."); return 0;
            }
            string pipe=opts.GetValueOrDefault("--pipe","BetterGPSProbe-v1");
            if(pipe.Length>100 || pipe.Any(c=>!char.IsLetterOrDigit(c)&&c is not '-' and not '_')) throw new ArgumentException("Invalid pipe name.");
            if(mode=="send")
            {
                using var timeout=new CancellationTokenSource(TimeSpan.FromSeconds(20));
                await using var client=new NamedPipeClientStream(".",pipe,PipeDirection.Out,PipeOptions.Asynchronous);
                await client.ConnectAsync(timeout.Token); long previous=0;
                foreach(var m in Read(Need("--input")))
                {
                    if(opts.ContainsKey("--pace") && m.TimestampMs>previous)
                        await Task.Delay(TimeSpan.FromMilliseconds(m.TimestampMs-previous));
                    await NativeCaptureProtocol.WriteAsync(client,m); previous=m.TimestampMs;
                }
                return 0;
            }
            if(mode is not ("serve" or "replay" or "prepare-map")) throw new ArgumentException("Unknown native mode.");
            bool firstHost=true;
            using var hostMarker=mode=="serve"?new Mutex(false,"Local\\BetterGPSNavigation-host-v1",out firstHost):null;
            if(!firstHost){Console.WriteLine("The GPS host is already running.");return 0;}
            using var phone=mode=="serve"&&OperatingSystem.IsWindows()?new PhoneGuidancePublisher():null;
            string output=Path.GetFullPath(Need("--output"));
            if(mode=="serve") output=Path.Combine(output,DateTime.Now.ToString("yyyyMMdd_HHmmss")+"_"+Guid.NewGuid().ToString("N")[..8]);
            Directory.CreateDirectory(output);
            bool compact=mode=="serve"&&opts.ContainsKey("--compact-session");
            RouteDiagnostics.CompactOutput=compact?output:null;
            var gameBinary=Path.Combine(Need("--game-root"),"bin","win_x64","amtrucks.exe");
            var installedProbe=Path.Combine(Need("--game-root"),"bin","win_x64","plugins","BetterGPS","BetterGPSProbe.dll");
            string? Hash(string path){if(!File.Exists(path))return null;using var s=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete);return Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(s));}
            RouteDiagnostics.Try(()=>File.WriteAllText(Path.Combine(output,"binary_provenance.json"),JsonSerializer.Serialize(new {
                gameBinary,gameSha256=Hash(gameBinary),installedProbe,probeSha256=Hash(installedProbe),
                bridgeSha256=Hash(typeof(NativeBridgeCli).Assembly.Location),
                gameLogSha256=Hash(Path.Combine(Need("--documents"),"game.log.txt")),
                priorityPolicy="last logged mount first; validate native/map agreement",createdUtc=DateTime.UtcNow},LiveProtocol.Json)));
            if(mode=="replay" && Path.GetDirectoryName(Path.GetFullPath(Need("--input")))==output)
                throw new ArgumentException("Replay output must use a different directory from its recording.");
            var options=new Options { GameRoot=Need("--game-root"), DocumentsRoot=Need("--documents"),
                ProbePath=output,OutputRoot=output,MapPath=opts.GetValueOrDefault("--map","/map/usa.mbd"),DiagnosticReports=opts.ContainsKey("--diagnostics") };
            Console.OutputEncoding=Encoding.UTF8;
            Console.WriteLine("Loading game archives and mounted mods from game.log.txt...");
            PerformanceTrace.Output=compact?null:Path.Combine(output,"performance.jsonl");
            GameAssets assets; using(PerformanceTrace.Measure("archives")) assets=GameAssets.Open(options);
            using var assetLifetime=assets;
            using(PerformanceTrace.Measure("asset-cache-key"))AssetIndexCache.Configure(assets.Loader,
                opts.GetValueOrDefault("--cache",Path.Combine(Path.GetDirectoryName(output)!,"gps-asset-cache")),
                GameAssets.ReadMounts(Path.Combine(options.DocumentsRoot,"game.log.txt")));
            if(!opts.ContainsKey("--no-world-cache"))WorldTileCache.Configure(assets.Loader,options.MapPath);
            if(!compact)RouteDiagnostics.Try(()=>assets.WriteManifest(Path.Combine(output,"asset_manifest.json")));
            if(assets.LoadErrors.Count>0)throw new InvalidDataException("Mounted resources could not be loaded; see asset_manifest.json.");
            if(mode=="prepare-map")
            {
                HashSet<(int,int)>? only=null;
                if(opts.TryGetValue("--input",out var input))
                    only=Read(input).Where(m=>m.Route?.Nodes is not null).SelectMany(m=>m.Route!.Nodes!).Select(n=>
                        WorldTileCache.Cell(new System.Numerics.Vector3(n.RawX/256f,0,n.RawZ/256f))).SelectMany(c=>
                        Enumerable.Range(-1,3).SelectMany(x=>Enumerable.Range(-1,3).Select(z=>(c.X+x,c.Z+z)))).ToHashSet();
                WorldMapPreparation.Build(assets,options,output,CancellationToken.None,only:only);
                if(!compact)RouteDiagnostics.Try(()=>assets.WriteManifest(Path.Combine(output,"asset_manifest.json")));return 0;
            }
            // Serving reads existing world pages, but never builds speculative sectors
            // alongside ATS. Explicit offline world-cache builds remain available.
            WorldMapPreparation? warmer=null;
            var bridge=new RouteMapLoader(assets,options,output,opts.GetValueOrDefault("--sign-calibration"),opts.ContainsKey("--full-map"),!opts.ContainsKey("--no-route-cache"));
            using var compiler=new ReloadingRouteCompiler(assets,bridge,options,opts.GetValueOrDefault("--cache",Path.Combine(Path.GetDirectoryName(output)!,"gps-asset-cache")),
                output,opts.GetValueOrDefault("--sign-calibration"),opts.ContainsKey("--full-map"),!opts.ContainsKey("--no-route-cache"),!opts.ContainsKey("--no-world-cache"),warmer);
            Task? localPreload=mode=="serve"||opts.ContainsKey("--warm-start")?bridge.Preload():null;
            if(mode=="replay"&&localPreload is not null)await localPreload;
            RouteDiagnostics.Try(()=>File.WriteAllLines(Path.Combine(output,"asset_sources.txt"),assets.Sources));
            Console.WriteLine($"READY — pipe {pipe}; carte chargee a la demande du trajet.");
            Console.WriteLine($"Recording: {output}");
            Console.WriteLine("Extraction des plaques active — preuves détaillées dans les fichiers *_panels.json.");
            using var rawLog=compact?new StreamWriter(Stream.Null):Output(Path.Combine(output,"capture.jsonl"));
            using var compiled=compact?new StreamWriter(Stream.Null):Output(Path.Combine(output,"compiled.jsonl"));
            using var guidance=new GuidanceJournal(output,compact);
            string? previousDisplay=null; long previousSecond=-1;
            void Emit(LiveGuidance g)
            {
                phone?.Publish(g);
                RouteDiagnostics.Try(()=>guidance.Write(g));
                string key=$"{g.Status}:{g.ActionId}:{g.Road}:{g.Cardinal}:{g.Destination}:{g.Detail}:{g.PreparationStatus}:{g.PreparationDetail}";
                if(key!=previousDisplay || (g.Instruction is not null && g.TimestampMs/1000!=previousSecond))
                {
                    Console.WriteLine(Describe(g)); previousDisplay=key; previousSecond=g.TimestampMs/1000;
                }
            }
            NativeCaptureSession Session()=>new((route,id)=>{
                try {
                    if(opts.ContainsKey("--require-node-evidence")&&route.Nodes is null)
                        throw new InvalidDataException("Nouvelle DLL 0.3.0 requise : preuves natives absentes.");
                    using var timing=PerformanceTrace.Measure("route:"+id);
                    return compiler.Compile(route,id);
                } finally {if(!compact)RouteDiagnostics.Try(()=>assets.WriteManifest(Path.Combine(output,"asset_manifest.json")));}
            },
                m=>{if(!compact){var bytes=LiveProtocol.Encode(m);RouteDiagnostics.Try(()=>compiled.WriteLine(Encoding.UTF8.GetString(bytes)));}},
                (route,id,ready,token)=>{
                    try {
                        if(opts.ContainsKey("--require-node-evidence")&&route.Nodes is null)
                            throw new InvalidDataException("Nouvelle DLL 0.3.0 requise : preuves natives absentes.");
                        using var timing=PerformanceTrace.Measure("route:"+id);
                    return compiler.Compile(route,id,ready,token);
                    }finally{if(!compact)RouteDiagnostics.Try(()=>assets.WriteManifest(Path.Combine(output,"asset_manifest.json")));}
                },compiler.SourcesChanged);
            if(mode=="replay")
            {
                NativeCaptureSession? replay=null;
                foreach(var m in Read(Need("--input")))
                {
                    if(m.Type=="Hello") { if(replay is not null) await replay.Drain(); replay=Session(); }
                    if(replay is null) throw new InvalidDataException("Hello required.");
                    if(!compact)RouteDiagnostics.Try(()=>rawLog.WriteLine(Encoding.UTF8.GetString(NativeCaptureProtocol.Encode(m))));
                    Emit(replay.Accept(m,false));
                    if(await replay.PollCompile(true) is { } done) Emit(done);
                }
                if(replay is not null) await replay.Drain();
                Console.WriteLine("Raw replay complete. compiled.jsonl can be replayed without loading the map."); return 0;
            }
            using var stop=new CancellationTokenSource();
            ConsoleCancelEventHandler cancel=(_,e)=>{e.Cancel=true;stop.Cancel();}; Console.CancelKeyPress+=cancel;
            try
            {
                while(!stop.IsCancellationRequested)
                {
                    await using var server=new NamedPipeServerStream(pipe,PipeDirection.In,1,PipeTransmissionMode.Byte,
                        PipeOptions.Asynchronous|PipeOptions.CurrentUserOnly,65536,65536);
                    Console.WriteLine("Waiting for DLL. Ctrl+C to stop.");
                    await server.WaitForConnectionAsync(stop.Token);
                    var state=Session();string? retainedKey=null;
                    using var connection=CancellationTokenSource.CreateLinkedTokenSource(stop.Token);
                    Task<NativeMessage?> read=NativeCaptureProtocol.ReadAsync(server,connection.Token);
                    try
                    {
                        while(!stop.IsCancellationRequested)
                        {
                            await Task.WhenAny(read,Task.Delay(100,stop.Token));
                            if(read.IsCompleted)
                            {
                                var m=await read; if(m is null) break;
                                if(!compact)RouteDiagnostics.Try(()=>rawLog.WriteLine(Encoding.UTF8.GetString(NativeCaptureProtocol.Encode(m))));
                                Emit(state.Accept(m));
                                read=NativeCaptureProtocol.ReadAsync(server,connection.Token);
                            }
                            if(await state.PollCompile() is { } done) Emit(done);
                            if(!read.IsCompleted) Emit(state.Tick());
                            if(compact&&!state.Compiling){
                                var keep=state.RetainedEvidence;var key=string.Join('|',keep);
                                if(key!=retainedKey){StorageBudget.RetainEvidence(opts.GetValueOrDefault("--cache",Path.Combine(Path.GetDirectoryName(output)!,"gps-asset-cache")),keep);retainedKey=key;}
                            }
                        }
                    }
                    catch(Exception e) when(e is IOException or InvalidDataException or JsonException)
                    { Console.WriteLine($"Capture disconnected/rejected: {e.Message}"); }
                    finally
                    {
                        Emit(state.Disconnect());
                        connection.Cancel(); try { await read; } catch(Exception) { }
                        await state.Drain();
                    }
                    if(opts.ContainsKey("--once")) break;
                }
            }
            catch(OperationCanceledException) when(stop.IsCancellationRequested) { }
            finally { Console.CancelKeyPress-=cancel;if(localPreload is not null)await localPreload; }
            return 0;
        }
        catch(Exception e) { Console.Error.WriteLine($"Native bridge: {e.Message}"); return 1; }
    }
    private static IEnumerable<NativeMessage> Read(string path)=>File.ReadLines(path).Where(l=>!string.IsNullOrWhiteSpace(l))
        .Select(l=>NativeCaptureProtocol.Decode(Encoding.UTF8.GetBytes(l)));
    private static StreamWriter Output(string path)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
        try{return new StreamWriter(path,false,new UTF8Encoding(false)) { AutoFlush=true };}
        catch(Exception e) when(e is IOException or UnauthorizedAccessException)
        {RouteDiagnostics.Try(()=>throw new IOException("Recording unavailable: "+path,e));return new StreamWriter(Stream.Null); }
    }
    public static string Describe(LiveGuidance g)
    {
        if(g.Instruction is null) return $"[{g.Status}] {g.Detail}";
        string RoadText(LiveRoadContext? context)=>context is {Status:"RESOLVED"}?
            string.Join(" / ",context.Labels.Where(l=>l.Confidence is "HIGH" or "MEDIUM").Select(l=>
                l.Identity+(l.Cardinal.Length>0&&l.CardinalConfidence is "HIGH" or "MEDIUM"?" "+l.Cardinal:"")+
                (l.Confidence=="MEDIUM"?" [provisoire]":""))):"";
        var current=RoadText(g.CurrentRoad);
        if(g.Status=="SERVICE_AREA")return ServiceAreaGuidance.Instruction(g.ServiceAreaKind??"REST_AREA");
        if(g.Status=="APPROACHING_ROUTE")return "Join the planned route"+(RoadText(g.ContinueToward) is {Length:>0} entry?" toward "+entry:"");
        if(g.Status=="FINAL_APPROACH")return "Continue to the destination entrance";
        if(g.Instruction=="CONTINUE"&&g.ActionId is null)return current.Length>0?$"Continue on {current}":
            RoadText(g.ContinueToward) is {Length:>0} connection?$"Continue toward {connection}":"Continue";
        var target=RoadText(g.TargetRoad);
        bool connectionTarget=target.Length==0&&g.TargetRoad?.Status is "UNKNOWN" or "AMBIGUOUS" or "CANDIDATE"&&g.JoinedRoad?.Status=="RESOLVED";
        if(connectionTarget)target=RoadText(g.JoinedRoad);
        if(target.Length==0&&(g.TargetRoad is null||g.TargetRoad.Status=="UNKNOWN"))
            target=string.Join(" ",new[]{g.Road,g.Cardinal}.Where(s=>!string.IsNullOrWhiteSpace(s)));
        var action=g.Instruction.Replace('_',' ').ToLowerInvariant();
        if(g.Instruction=="ROUNDABOUT")action=g.RoundaboutExitNumber is {} ordinal?
            $"at the roundabout, take exit {ordinal}":"at the roundabout, follow the selected exit";
        var distance=g.DistanceM is >0 and <160.9344?$"{Math.Max(10,Math.Round(g.DistanceM.Value/0.3048/10)*10):0} ft":$"{g.DistanceMi:0.0} mi";
        var sameRoad=g.CurrentRoad?.Labels.Any(l=>g.TargetRoad?.Labels.Any(t=>t.Identity==l.Identity)==true)==true;
        var text=g.Status=="EXECUTE"?$"{action} now":g.Status=="WAITING_AT_MANEUVER"?$"{action} when ready":$"{action} in {distance}";
        text+=target.Length>0?$" {(connectionTarget?"toward":sameRoad?"on":"onto")} {target}":"";
        if(g.Status=="CRUISE"&&current.Length>0)
            text=(g.ContinueDistanceM is >=160.9344?$"Continue for {g.ContinueDistanceM/1609.344:0.0} mi on {current}":
                $"Continue on {current}")+" | "+text;
        else if(g.Status=="CRUISE"&&RoadText(g.ContinueToward) is {Length:>0} continuing)
            text="Continue toward "+continuing+" | "+text;
        if(g.Destination is not null)text+=$" toward {g.Destination}";
        if(g.SignTargets is {Length:>0})
        {
            var destinations=g.SignTargets.Select(t=>g.TargetRoad is {Status:"RESOLVED"}&&g.TargetRoad.Labels.Any(l=>
                l.Identity==t.Road&&(t.Cardinal.Length==0||l.Cardinal==t.Cardinal))?t.Destination:
                t.Road+(t.Cardinal.Length>0?" "+t.Cardinal:"")+(t.Destination.Length>0?" — "+t.Destination:"")).Where(t=>t.Length>0).Distinct().ToArray();
            if(destinations.Length>0)text+=" toward "+string.Join(" / ",destinations)+" [provisoire]";
        }
        if(g.Exit is not null)text+=$" (exit {g.Exit})";
        if(g.ThenInstruction is not null)text+=$"; then {SimpleAction(g.ThenInstruction)}";
        if(g.PreparationStatus is not null)text+=" [guidage partiel]";
        return text;
    }
    internal static string SimpleAction(string instruction)=>instruction.StartsWith("U_TURN",StringComparison.Ordinal)?"turn around":
        instruction.Contains("LEFT",StringComparison.Ordinal)?"left":
        instruction.Contains("RIGHT",StringComparison.Ordinal)?"right":instruction is "CONTINUE" or "STAY" or "GO_STRAIGHT" or "STRAIGHT" or "CONTINUE_AT_FORK" or "FORK_STRAIGHT"?"stay":
        instruction.Replace('_',' ').ToLowerInvariant();
    internal static string Label(LiveRoadContext context)=>context.Labels.Length==0?context.Status.ToLowerInvariant():
        string.Join(" / ",context.Labels.Select(l=>l.Identity+Quality(l.Confidence)+
            (l.Cardinal.Length>0&&l.CardinalConfidence is "HIGH" or "MEDIUM"?" "+l.Cardinal+Quality(l.CardinalConfidence):"")));
    private static string Quality(string confidence)=>confidence=="HIGH"?" [confirmee]":confidence=="LOW"?" [hypothese]":" [provisoire]";
}


