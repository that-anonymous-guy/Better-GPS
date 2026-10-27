namespace BetterGPSNavigation;

internal sealed class Options
{
    public required string GameRoot { get; init; }
    public required string DocumentsRoot { get; init; }
    public required string ProbePath { get; init; }
    public required string OutputRoot { get; init; }
    public string MapPath { get; init; } = "/map/usa.mbd";
    public bool IgnoreMods { get; init; }
    public bool DiagnosticReports {get;init;}

    public static Options Parse(string[] args)
    {
        var dict = new Dictionary<string, string?>(StringComparer.OrdinalIgnoreCase);
        for (var i = 0; i < args.Length; i++)
        {
            var arg = args[i];
            if (!arg.StartsWith("--", StringComparison.Ordinal))
                continue;

            var key = arg[2..];
            if (key.Equals("ignore-mods", StringComparison.OrdinalIgnoreCase))
            {
                dict[key] = "true";
                continue;
            }

            if (i + 1 >= args.Length)
                throw new ArgumentException($"Missing value for --{key}");
            dict[key] = args[++i];
        }

        string Need(string key) => dict.TryGetValue(key, out var value) && !string.IsNullOrWhiteSpace(value)
            ? Path.GetFullPath(Environment.ExpandEnvironmentVariables(value))
            : throw new ArgumentException($"Missing required argument --{key}");

        var docs = dict.TryGetValue("documents", out var docsValue) && !string.IsNullOrWhiteSpace(docsValue)
            ? Path.GetFullPath(Environment.ExpandEnvironmentVariables(docsValue))
            : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "American Truck Simulator");

        var probe = Need("probe");
        var output = dict.TryGetValue("output", out var outputValue) && !string.IsNullOrWhiteSpace(outputValue)
            ? Path.GetFullPath(Environment.ExpandEnvironmentVariables(outputValue))
            : Path.Combine(probe, "mapbridge");

        return new Options
        {
            GameRoot = Need("game-root"),
            DocumentsRoot = docs,
            ProbePath = probe,
            OutputRoot = output,
            MapPath = dict.TryGetValue("map", out var mapValue) && !string.IsNullOrWhiteSpace(mapValue)
                ? NormalizeVirtualPath(mapValue)
                : "/map/usa.mbd",
            IgnoreMods = dict.ContainsKey("ignore-mods")
        };
    }

    private static string NormalizeVirtualPath(string value)
    {
        var p = value.Replace('\\', '/');
        return p.StartsWith('/') ? p : "/" + p;
    }

    public static void PrintUsage()
    {
        Console.WriteLine("BetterGPSNavigation - BetterGPSProbe UID -> ATS map validator");
        Console.WriteLine();
        Console.WriteLine("Usage:");
        Console.WriteLine("  BetterGPSNavigation.exe --game-root \"C:\\...\\American Truck Simulator\" --probe \"C:\\...\\BetterGPSProbe\"");
        Console.WriteLine();
        Console.WriteLine("Optional:");
        Console.WriteLine("  --documents \"C:\\Users\\you\\Documents\\American Truck Simulator\"");
        Console.WriteLine("  --output    \"C:\\...\\mapbridge\"");
        Console.WriteLine("  --map       /map/usa.mbd");
        Console.WriteLine("  --ignore-mods");
    }
}
