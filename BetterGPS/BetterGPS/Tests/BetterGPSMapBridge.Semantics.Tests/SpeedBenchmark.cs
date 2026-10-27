using System.Diagnostics;
using System.Text.Json;
using BetterGPSNavigation;

internal static class SpeedBenchmark
{
    internal static void Run(string directory,string output)
    {
        var rows=new List<object>();
        foreach(var file in Directory.GetFiles(directory,"*_named-network.json").Order())
        {
            var data=JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(file),LiveProtocol.Json)!;
            var clock=Stopwatch.StartNew();
            var graph=new DirectedRoadNetworkGraph(data.Region,data.Portions,data.Connections,data.Errors);
            var geometry=clock.Elapsed.TotalSeconds;clock.Restart();
            var names=NamedRoadNetwork.Solve(graph,data.Sources);var solve=clock.Elapsed.TotalSeconds;
            if(JsonSerializer.Serialize(names,LiveProtocol.Json)!=JsonSerializer.Serialize(data.Names,LiveProtocol.Json))throw new Exception("Changed names: "+file);
            clock.Restart();NamedNetworkValidation.Validate(data,graph);var validation=clock.Elapsed.TotalSeconds;
            rows.Add(new{route=Path.GetFileName(file),portions=data.Portions.Length,geometrySeconds=geometry,solveSeconds=solve,validationSeconds=validation,identicalNames=true});
            Console.WriteLine($"PASS: {Path.GetFileName(file)}: solve={solve:F3}s validation={validation:F3}s; identical names and proofs.");
        }
        if(rows.Count==0)throw new Exception("No recorded networks");
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        File.WriteAllText(output,JsonSerializer.Serialize(rows,LiveProtocol.Json));
    }
}
