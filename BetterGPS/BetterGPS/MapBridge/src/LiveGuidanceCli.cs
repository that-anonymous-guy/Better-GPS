using System.IO.Pipes;
using System.Text;
using System.Text.Json;

namespace BetterGPSNavigation;

public static class LiveGuidanceCli
{
    public static async Task<int> Run(string[] args)
    {
        try
        {
            if (args.Length == 0 || args[0] == "help")
            {
                Console.WriteLine("""
BetterGPS live protocol v2 (v1 recordings remain readable)
  --live pack --directory DIR --stem STEM --output route.jsonl
  --live simulate --directory DIR --stem STEM --output synthetic.jsonl
  --live replay --input messages.jsonl --output guidance.jsonl
  --live audit --input route.json --output DIR
  --live preview --input route.json --output preview.html
  --live serve --pipe BetterGPS-v1 --output guidance.jsonl
  --live send --pipe BetterGPS-v1 --input messages.jsonl [--pace]
pack creates Hello + compiled RouteSnapshot; append real VehicleState frames for replay.
simulate creates synthetic vehicle poses along the captured corridor, for demonstration only.
send transmits exactly the same JSONL messages as replay, using length-prefixed pipe frames.
""");
                return 0;
            }
            var mode = args[0];
            var options = new Dictionary<string,string>(StringComparer.Ordinal);
            for (int i = 1; i < args.Length; i++)
            {
                if (args[i] == "--pace") { options.Add("--pace","true"); continue; }
                if (!args[i].StartsWith("--") || i+1 >= args.Length) throw new ArgumentException("Expected --option value.");
                options.Add(args[i],args[++i]);
            }
            string Get(string key) => options.TryGetValue(key,out var v) ? v : throw new ArgumentException($"Missing {key}.");
            if(mode=="preview")
            {
                var snapshot=LiveProtocol.Decode(File.ReadAllBytes(Get("--input")));
                if(snapshot.Type!="RouteSnapshot"||snapshot.Route is null)throw new InvalidDataException("Preview requires a route snapshot.");
                NavigationPreview.Write(snapshot.Route,Get("--output"));return 0;
            }
            if(mode=="audit")
            {
                var snapshot=LiveProtocol.Decode(File.ReadAllBytes(Get("--input")));
                if(snapshot.Type!="RouteSnapshot"||snapshot.Route?.RoadIdentities is null)
                    throw new InvalidDataException("Audit requires a v2 route snapshot with segment identities.");
                Directory.CreateDirectory(Get("--output"));
                RoadIdentityView.Write(Get("--output"),snapshot.Route);
                Console.WriteLine("Identity audit: "+Path.Combine(Get("--output"),snapshot.Route.RouteId+"_identites.html"));
                return 0;
            }
            if (mode is "pack" or "simulate")
            {
                var route = LiveCaptureCompiler.Compile(Get("--directory"),Get("--stem"));
                using var output = Output(Get("--output"));
                var session = Guid.NewGuid().ToString("N");
                var wireVersion=LiveProtocol.VersionFor(route);
                output.WriteLine(Encoding.UTF8.GetString(LiveProtocol.Encode(new(wireVersion,"Hello",session,0,0,0))));
                output.WriteLine(Encoding.UTF8.GetString(LiveProtocol.Encode(new(wireVersion,"RouteSnapshot",session,1,0,1,route))));
                if (mode == "simulate")
                {
                    long seq = 2;
                    foreach (var edge in route.Edges)
                    {
                        // Sample every ten world metres; these are not captured truck positions.
                        int samples = Math.Max(1,(int)Math.Ceiling((edge.WorldEndM-edge.WorldStartM)/10));
                        for (int sample = 0; sample < samples; sample++)
                        {
                            double t = (sample+0.5)/samples;
                            var position = new LivePoint(edge.Start.X+(edge.End.X-edge.Start.X)*t,
                                edge.Start.Y+(edge.End.Y-edge.Start.Y)*t,edge.Start.Z+(edge.End.Z-edge.Start.Z)*t);
                            double world = edge.WorldStartM+(edge.WorldEndM-edge.WorldStartM)*t;
                            double heading = Math.Atan2(edge.End.Z-edge.Start.Z,edge.End.X-edge.Start.X)*180/Math.PI;
                            var vehicle = new LiveVehicle(position,heading,10,false);
                            output.WriteLine(Encoding.UTF8.GetString(LiveProtocol.Encode(new(wireVersion,"VehicleState",session,seq++,
                                (long)(world*100),1,Vehicle:vehicle))));
                        }
                    }
                }
                Console.WriteLine($"Compiled {route.Actions.Length} actions, {route.Edges.Length} edges.");
                return 0;
            }
            if (mode == "replay")
            {
                if (string.Equals(Path.GetFullPath(Get("--input")),Path.GetFullPath(Get("--output")),
                    OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
                    throw new ArgumentException("Input and output must differ.");
                using var output = Output(Get("--output"));
                var engine = new LiveGuidanceEngine();
                int count = 0;
                string? previousDisplay = null;
                foreach (var message in ReadJsonLines(Get("--input")))
                {
                    var result = engine.Accept(message);
                    Emit(output,result); count++;
                    var display = JsonSerializer.Serialize(new{result.ActionId,result.Status,result.CurrentRoad,result.TargetRoad,result.Destination},LiveProtocol.Json);
                    if (display != previousDisplay) Console.WriteLine(Describe(result));
                    previousDisplay = display;
                }
                Console.WriteLine($"Replayed {count} messages deterministically.");
                return 0;
            }
            var pipeName = Get("--pipe");
            if (pipeName.Length > 100 || pipeName.Any(c => !char.IsLetterOrDigit(c) && c is not '-' and not '_'))
                throw new ArgumentException("Invalid pipe name.");
            if (mode == "send")
            {
                await using var client = new NamedPipeClientStream(".",pipeName,PipeDirection.Out,PipeOptions.Asynchronous);
                using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(10));
                await client.ConnectAsync(timeout.Token);
                long previous = 0;
                foreach (var message in ReadJsonLines(Get("--input")))
                {
                    if (options.ContainsKey("--pace"))
                    {
                        long delay = message.TimestampMs-previous;
                        if (delay < 0 || delay > 60000) throw new InvalidDataException("Paced gaps must be between 0 and 60 seconds.");
                        await Task.Delay((int)delay);
                    }
                    await LiveProtocol.WriteAsync(client,message);
                    previous = message.TimestampMs;
                }
                return 0;
            }
            if (mode != "serve") throw new ArgumentException("Unknown live command.");
            using var writer = Output(Get("--output"));
            using var stop = new CancellationTokenSource();
            ConsoleCancelEventHandler cancel = (_,e) => { e.Cancel = true; stop.Cancel(); };
            Console.CancelKeyPress += cancel;
            try
            {
                while (!stop.IsCancellationRequested)
                {
                    await using var pipe = new NamedPipeServerStream(pipeName,PipeDirection.In,1,
                        PipeTransmissionMode.Byte,PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
                    Console.WriteLine($"Listening on {pipeName}; Ctrl+C to stop.");
                    await pipe.WaitForConnectionAsync(stop.Token);
                    var engine = new LiveGuidanceEngine();
                    var clock = System.Diagnostics.Stopwatch.StartNew();
                    long producerTime = 0, receivedTime = 0;
                    try
                    {
                        var pending = LiveProtocol.ReadAsync(pipe,stop.Token);
                        while (!stop.IsCancellationRequested)
                        {
                            if (await Task.WhenAny(pending,Task.Delay(500,stop.Token)) != pending)
                            {
                                Emit(writer,engine.Tick(producerTime + clock.ElapsedMilliseconds-receivedTime));
                                continue;
                            }
                            var message = await pending;
                            if (message is null) { Emit(writer,engine.Tick(producerTime + LiveGuidanceEngine.StaleAfterMs+1)); break; }
                            producerTime = message.TimestampMs; receivedTime = clock.ElapsedMilliseconds;
                            Emit(writer,engine.Accept(message));
                            pending = LiveProtocol.ReadAsync(pipe,stop.Token);
                        }
                    }
                    catch (Exception ex) when (ex is IOException or JsonException or InvalidDataException)
                    {
                        Emit(writer,new LiveGuidance(Math.Max(0,producerTime),"PROTOCOL_ERROR",null,0,
                            null,null,null,null,null,null,null,null,null,null,[],null,null,ex.Message));
                        Console.Error.WriteLine(ex.Message);
                    }
                }
            }
            catch (OperationCanceledException) when (stop.IsCancellationRequested) { }
            finally { Console.CancelKeyPress -= cancel; }
            return 0;
        }
        catch (Exception ex) { Console.Error.WriteLine("Live guidance: " + ex.Message); return 1; }
    }

    internal static IEnumerable<LiveMessage> ReadJsonLines(string path)
    {
        if (new FileInfo(path).Length > 128L*1024*1024) throw new InvalidDataException("Replay file exceeds 128 MiB; split into sessions.");
        using var reader = new StreamReader(path,Encoding.UTF8);
        int line = 0;
        while (reader.ReadLine() is { } json)
        {
            line++;
            if (string.IsNullOrWhiteSpace(json)) continue;
            LiveMessage m;
            try { m = LiveProtocol.Decode(Encoding.UTF8.GetBytes(json)); }
            catch (Exception ex) when (ex is JsonException or InvalidDataException)
            { throw new InvalidDataException($"Line {line}: {ex.Message}",ex); }
            yield return m;
        }
    }
    private static StreamWriter Output(string path)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
        return new StreamWriter(path,false,new UTF8Encoding(false));
    }
    private static void Emit(StreamWriter writer,LiveGuidance result)
    {
        writer.WriteLine(JsonSerializer.Serialize(result,LiveProtocol.Json)); writer.Flush();
    }
    private static string Describe(LiveGuidance r) => NativeBridgeCli.Describe(r);
}
