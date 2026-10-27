using BetterGPSNavigation;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
internal static class RoutePreparationTests
{
    internal static void Signs(string game,string docs,string output,bool atlasesOnly=false)
    {
        Directory.CreateDirectory(output);using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."});
        var roots=atlasesOnly?assets.Loader.GetFiles("/def/sign/atlas/").Where(p=>p.EndsWith(".sii")).Order().ToArray():assets.Loader.GetFiles("/def/sign/").Where(p=>p.EndsWith(".sii")&&(Path.GetFileName(p).StartsWith("boards")||Path.GetFileName(p).StartsWith("templates"))).Order().ToArray();
        var paths=new HashSet<string>(roots);foreach(var path in roots)
        {
            var old=TruckLib.Sii.SiiFile.Open(path,assets.Loader);var compact=CompactSiiReader.Open(path,assets.Loader);
            if(old.Serialize()!=compact.Serialize()||!old.Includes.SetEquals(compact.Includes))throw new Exception("Sign registry changed: "+path);
            foreach(var unit in old.Units)if(unit.Attributes.TryGetValue("path",out var raw)&&raw is string text&&assets.Loader.FileExists(text))paths.Add(text);
        }
        int checkedCount=0;foreach(var path in paths.Order())
        {
            var old=TruckLib.Sii.SiiFile.Open(path,assets.Loader);var compact=CompactSiiReader.Open(path,assets.Loader);
            if(old.Serialize()!=compact.Serialize()||!old.Includes.SetEquals(compact.Includes))throw new Exception("Sign template changed: "+path);checkedCount++;
        }
        File.WriteAllText(Path.Combine(output,"equivalence.json"),JsonSerializer.Serialize(new{checkedCount,roots=roots.Length}));Console.WriteLine($"PASS: {checkedCount} real sign registries, boards and templates preserve all values, types and includes.");
    }
    internal static void Scheduling()
    {
        var route=new LiveRoute("fixture",[],[]);var seen=new List<int>();var ready=new List<LiveRoute>();
        var result=RouteMapLoader.PrepareProgressively([12,48,96],count=>{seen.Add(count);return route with{IsPartial=true};},()=>{seen.Add(-1);return route;},ready.Add);
        if(result!=route||!seen.SequenceEqual(new[]{12,-1})||ready.Count!=1)throw new Exception("Successful preparation repeated intermediate plans");
        seen.Clear();ready.Clear();var original=new InvalidDataException("unresolved downstream branch");
        try{RouteMapLoader.PrepareProgressively([12,48,96],count=>{seen.Add(count);return route with{IsPartial=true};},()=>throw original,ready.Add);throw new Exception("Invalid full route accepted");}
        catch(InvalidDataException error){if(!ReferenceEquals(error,original)||!seen.SequenceEqual(new[]{12,48,96})||ready.Count!=3)throw new Exception("Recovery lost verified intermediate guidance");}
        seen.Clear();using var cancellation=new CancellationTokenSource();cancellation.Cancel();
        try{RouteMapLoader.PrepareProgressively([12],count=>{seen.Add(count);return route;},()=>route,ready.Add,cancellation.Token);throw new Exception("Cancelled job accepted");}catch(OperationCanceledException){}
        if(seen.Count!=0)throw new Exception("Cancelled work still compiled");
        Console.WriteLine("PASS: departure publishes first; successful routes skip two redundant prefixes; failed full plans retain recovery prefixes and original failure; cancellation stops work.");
    }
    internal static void Hashes()
    {
        var fixture=new{Unicode="Illinois 13 / rue é",Empty=Array.Empty<int>(),Values=Enumerable.Range(0,10000).Select(i=>new{Id=i,Position=new[]{i*.25,1.0,-i*.3}}).ToArray()};
        var expected=Convert.ToHexString(SHA256.HashData(JsonSerializer.SerializeToUtf8Bytes(fixture,LiveProtocol.Json)));
        if(JsonContentHash.Compute(fixture)!=expected)throw new Exception("Stream hashing changed content identity");
        Console.WriteLine("PASS: streamed JSON preserves exact SHA256 identity for geometry and Unicode.");
    }
    internal static void Benchmark(string cache,string output)
    {
        Directory.CreateDirectory(output);var results=new List<object>();
        var files=Directory.GetFiles(cache,"*network-region*.json").OrderByDescending(File.GetLastWriteTimeUtc).Take(4);
        foreach(var file in files)
        {
            using var input=File.OpenRead(file);var header=new byte[68];input.ReadExactly(header);input.Position=0;
            if(!CompressedAssetCache.TryRead<NamedNetworkSnapshot>(input,Convert.ToHexString(header.AsSpan(4,32)),out var snapshot))continue;
            var graph=new DirectedRoadNetworkGraph(snapshot.Region,snapshot.Portions,snapshot.Connections,snapshot.Errors);
            long bytes=GC.GetAllocatedBytesForCurrentThread();var clock=Stopwatch.StartNew();var rebuilt=NamedRoadNetwork.Prepare(graph,snapshot.Sources);clock.Stop();bytes=GC.GetAllocatedBytesForCurrentThread()-bytes;
            string expected=JsonSerializer.Serialize(snapshot.Names,LiveProtocol.Json),actual=JsonSerializer.Serialize(rebuilt.Names,LiveProtocol.Json);
            if(expected!=actual||snapshot.Fingerprint!=rebuilt.Fingerprint)throw new Exception("Preparation changed names, proofs or fingerprint: "+file);
            results.Add(new{file=Path.GetFileName(file),portions=snapshot.Portions.Length,sources=snapshot.Sources.Length,seconds=clock.Elapsed.TotalSeconds,allocatedBytes=bytes,exactNamesProofsAndFingerprint=true});
        }
        if(results.Count==0)throw new Exception("No real network fixture tested");
        var json=JsonSerializer.Serialize(results,new JsonSerializerOptions{WriteIndented=true});File.WriteAllText(Path.Combine(output,"measurement.json"),json);Console.WriteLine(json);
    }
}
