using System.Diagnostics;
using System.Numerics;
using System.Text;
using System.Text.Json;
using BetterGPSNavigation;
internal static class MemoryOptimizationTests
{
    internal static void Units(string output)
    {
        Directory.CreateDirectory(output);void Check(bool ok,string why){if(!ok)throw new Exception(why);}
        var used=new System.Collections.Concurrent.ConcurrentDictionary<string,GameAssets.ResourceUse>();var shared=new AssetReadCache(2*1024*1024);
        var original=new byte[1024*1024];original[0]=41;var source=new ByteFiles(original);var first=new TrackedFileSystem(source,"a",used,shared);var second=new TrackedFileSystem(source,"b",used,shared);
        using(var s=first.Open("/def/test")){Check(!s.CanWrite,"Cached stream writable");try{((MemoryStream)s).GetBuffer();throw new Exception("Cached storage publicly exposed");}catch(UnauthorizedAccessException){}}
        var copy=first.ReadAllBytes("/def/test");copy[0]=0;using(var s=first.Open("/def/test"))Check(s.ReadByte()==41,"ReadAllBytes mutated shared cached bytes");
        using(var s=second.Open("/def/test"))Check(s.ReadByte()==41,"Mount namespaces collided");for(int i=0;i<12;i++){using var s=first.Open("/def/test"+i);}
        Check(shared.ResidentBytes<=2*1024*1024,"Per-mount cache budgets multiplied");
        using(var s=first.Open("/def/repeated")){}long allocated=GC.GetAllocatedBytesForCurrentThread();for(int i=0;i<100;i++){using var s=first.Open("/def/repeated");Check(s.ReadByte()==41,"Cached content changed");}long hitBytes=GC.GetAllocatedBytesForCurrentThread()-allocated;Check(hitBytes<100000,"Open cloned large cached buffers");
        var fingerprint=new string('A',64);var value=Enumerable.Range(0,1000).ToDictionary(i=>i.ToString(),i=>new string('X',1000));var encoded=CompressedAssetCache.Encode(value,fingerprint);
        using(var s=new MemoryStream(encoded)){Check(CompressedAssetCache.TryRead<Dictionary<string,string>>(s,fingerprint,out var restored)&&restored.Count==value.Count&&restored["12"]==value["12"],"Compressed cache changed content");}
        encoded[^1]^=1;try{using var s=new MemoryStream(encoded);CompressedAssetCache.TryRead<Dictionary<string,string>>(s,fingerprint,out _);throw new Exception("Corrupt compressed payload accepted");}catch(InvalidDataException){}
        string root=Path.GetFullPath(output);RouteDiagnostics.CompactOutput=root;bool evaluated=false;RouteDiagnostics.WriteText(Path.Combine(root,"disabled.json"),()=>{evaluated=true;return "large payload";});Check(!evaluated,"Disabled diagnostics still allocated payloads");RouteDiagnostics.CompactOutput=null;
        File.WriteAllText(Path.Combine(root,"fragment.sui"),"model_desc: \"/a/very/long/model/resource/path.pmd\"\nvalues[]: \"Long street name with unicode é and spaces\"\nvalues[]: \"second long name # // unmodified\"\n");
        File.WriteAllText(Path.Combine(root,"test.sii"),"// comment\nSiiNunit {\nsign_model : sign.example {\n@include \"fragment.sui\"\nnumber: 3\nvector: (1.0, 2.0, 3.0)\nflag: true\n}\n}\n");
        File.AppendAllText(Path.Combine(root,"fragment.sui"),"leading: \"   A long value with leading whitespace\"\ntrailing: \"A long value with trailing whitespace   \"\n");
        var fs=new MountedDirectoryFileSystem(root);var normal=TruckLib.Sii.SiiFile.Open("/test.sii",fs);var compact=CompactSiiReader.Open("/test.sii",fs);
        Check(normal.Serialize()==compact.Serialize()&&normal.Includes.SetEquals(compact.Includes),"Compact text changed strings, includes, arrays or types");
        File.WriteAllText(Path.Combine(root,"marker.sii"),"SiiNunit { model : a { value: \"\uE000 original marker string\"\n} }");Check(CompactSiiReader.Open("/marker.sii",fs).Serialize()==TruckLib.Sii.SiiFile.Open("/marker.sii",fs).Serialize(),"Marker fallback changed real strings");
        Console.WriteLine($"PASS: global byte budget, immutable zero-copy streams, defensive byte copies, compressed cache integrity, lazy diagnostics and SII equivalence; 100 cache-hit streams allocate {hitBytes} bytes.");
    }
    private sealed class ByteFiles(byte[] data):TruckLib.IFileSystem
    {
        public char DirectorySeparator=>'/';public bool FileExists(string path)=>true;public bool DirectoryExists(string path)=>true;public IList<string> GetFiles(string path)=>[];public string? GetParent(string path)=>"/";public byte[] ReadAllBytes(string path)=>(byte[])data.Clone();public Stream Open(string path)=>new MemoryStream(ReadAllBytes(path));public string ReadAllText(string path)=>Encoding.UTF8.GetString(data);public string ReadAllText(string path,Encoding encoding)=>encoding.GetString(data);
    }
    internal static void RealSii(string game,string docs,string output)
    {
        Directory.CreateDirectory(output);using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."});var files=assets.Loader.GetFiles("/def/world/").Where(p=>p.EndsWith(".sii")&&(Path.GetFileName(p).StartsWith("sign")||Path.GetFileName(p).StartsWith("prefab")||Path.GetFileName(p).StartsWith("road_look"))).Order().ToArray();
        var results=new List<object>();foreach(var path in files){var old=TruckLib.Sii.SiiFile.Open(path,assets.Loader);var optimized=CompactSiiReader.Open(path,assets.Loader);if(old.Serialize()!=optimized.Serialize()||!old.Includes.SetEquals(optimized.Includes)){File.WriteAllText(Path.Combine(output,"original.sii"),old.Serialize());File.WriteAllText(Path.Combine(output,"optimized.sii"),optimized.Serialize());throw new Exception("SII differential mismatch: "+path);}results.Add(new{path,units=old.Units.Count,includes=old.Includes.Count});}
        File.WriteAllText(Path.Combine(output,"equivalence.json"),JsonSerializer.Serialize(results));Console.WriteLine($"PASS: {files.Length} real definition documents preserve all units, attributes, types and includes.");
    }
    internal static void RouteBenchmark(string game,string docs,string capture,string output)
    {
        Directory.CreateDirectory(output);RouteDiagnostics.CompactOutput=output;
        var raw=File.ReadLines(capture).Where(s=>s.Length>0).Select(s=>NativeCaptureProtocol.Decode(Encoding.UTF8.GetBytes(s))).First(m=>m.Type=="RawRouteSnapshot"&&m.Route?.Nodes is {Length:>12}).Route!.WithoutZeroDistanceDuplicates();
        var route=RouteMapLoader.Prefix(raw,12);var options=new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."};
        var details=new Dictionary<string,(double Seconds,long Bytes,int Count)>();PerformanceTrace.DetailObserver=(stage,seconds,bytes)=>{var old=details.GetValueOrDefault(stage);details[stage]=(old.Seconds+seconds,old.Bytes+bytes,old.Count+1);};
        long allocated=GC.GetTotalAllocatedBytes(true);var watch=Stopwatch.StartNew();using var assets=GameAssets.Open(options);
        AssetIndexCache.Configure(assets.Loader,Path.Combine(output,"cache"),GameAssets.ReadMounts(Path.Combine(docs,"game.log.txt")));WorldTileCache.Configure(assets.Loader,options.MapPath);
        var loader=new RouteMapLoader(assets,options,output,null);double firstReady=0;var plan=loader.Compile(route,"memory-route",_=>{if(firstReady==0)firstReady=watch.Elapsed.TotalSeconds;});
        File.WriteAllText(Path.Combine(output,"details.json"),JsonSerializer.Serialize(details.OrderByDescending(p=>p.Value.Seconds).Select(p=>new{stage=p.Key,p.Value.Seconds,p.Value.Bytes,p.Value.Count}),new JsonSerializerOptions{WriteIndented=true}));PerformanceTrace.DetailObserver=null;
        var elapsed=watch.Elapsed.TotalSeconds;var full=RouteEvidence.Unpack(plan);
        var semantic=new{full.Edges,actions=full.Actions.Select(a=>new{a.Instruction,a.SegmentIndex,a.NavStationM,a.BranchId,a.RoundaboutExitNumber,a.SignTargets}),roads=full.RoadIdentities?.Spans.Select(s=>new{s.SegmentIndex,s.Status,s.Identities})};
        File.WriteAllText(Path.Combine(output,"semantics.json"),JsonSerializer.Serialize(semantic,LiveProtocol.Json));
        bool workingMap=typeof(RouteMapLoader).GetField("bridge",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic)!.GetValue(loader)is not null;
        GC.Collect();GC.WaitForPendingFinalizers();GC.Collect();
        var result=new{firstReady,elapsed,allocatedBytes=GC.GetTotalAllocatedBytes(true)-allocated,retainedManagedBytes=GC.GetTotalMemory(true),privateBytes=Process.GetCurrentProcess().PrivateMemorySize64,workingMap};
        File.WriteAllText(Path.Combine(output,"measurement.json"),JsonSerializer.Serialize(result,new JsonSerializerOptions{WriteIndented=true}));Console.WriteLine(JsonSerializer.Serialize(result));
    }
    internal static void Benchmark(string game,string docs,string capture,string output)
    {
        Directory.CreateDirectory(output);var cache=Path.Combine(output,"cache");
        var route=File.ReadLines(capture).Where(s=>s.Length>0).Select(s=>NativeCaptureProtocol.Decode(Encoding.UTF8.GetBytes(s))).First(m=>m.Type=="RawRouteSnapshot"&&m.Route?.Nodes is {Length:>2}).Route!;
        var origin=route.Nodes![0];var cell=WorldTileCache.Cell(new Vector3(origin.RawX/256f,0,origin.RawZ/256f));
        RouteDiagnostics.CompactOutput=output;
        var stageAllocations=new List<object>();PerformanceTrace.AllocationObserver=(stage,bytes)=>stageAllocations.Add(new{stage,bytes});
        var options=new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."};
        long allocated=GC.GetTotalAllocatedBytes(true),cpu=Process.GetCurrentProcess().TotalProcessorTime.Ticks;var clock=Stopwatch.StartNew();
        using var assets=GameAssets.Open(options);if(assets.LoadErrors.Count>0)throw new Exception("Incomplete mounted assets");
        AssetIndexCache.Configure(assets.Loader,cache,GameAssets.ReadMounts(Path.Combine(docs,"game.log.txt")));WorldTileCache.Configure(assets.Loader,options.MapPath);
        var openedSeconds=clock.Elapsed.TotalSeconds;
        WorldMapPreparation.Build(assets,options,output,CancellationToken.None,only:[cell]);
        var builtSeconds=clock.Elapsed.TotalSeconds;
        GC.Collect();GC.WaitForPendingFinalizers();GC.Collect();
        var result=new{cell,openedSeconds,builtSeconds,allocatedBytes=GC.GetTotalAllocatedBytes(true)-allocated,retainedManagedBytes=GC.GetTotalMemory(true),privateBytes=Process.GetCurrentProcess().PrivateMemorySize64,peakWorkingBytes=Process.GetCurrentProcess().PeakWorkingSet64,cpuSeconds=(Process.GetCurrentProcess().TotalProcessorTime.Ticks-cpu)/(double)TimeSpan.TicksPerSecond};
        File.WriteAllText(Path.Combine(output,"measurement.json"),JsonSerializer.Serialize(result,new JsonSerializerOptions{WriteIndented=true,IncludeFields=true}));File.WriteAllText(Path.Combine(output,"stage-allocations.json"),JsonSerializer.Serialize(stageAllocations));Console.WriteLine(JsonSerializer.Serialize(result));
    }
}
