using System.Globalization;

namespace BetterGPSNavigation;

internal sealed record PhysicalRouteRow(
    int Index,
    ulong NodeUid,
    double DistanceFromPreviousM,
    double TimeFromPreviousS,
    double TotalDistanceTillEndM,
    double TotalTimeTillEndS);

internal sealed record LogicalRouteRow(
    int Index,
    uint Type,
    uint NodeId,
    ulong ItemUid,
    double DistanceFromPreviousM,
    double TimeFromPreviousS,
    uint Unknown20,
    uint Unknown22,
    double TotalDistanceTillEndM,
    double TotalTimeTillEndS);

internal sealed class ProbeRoute
{
    public NativeRoute? NativeEvidence { get; init; }
    public required string Stem { get; init; }
    public required string PhysicalPath { get; init; }
    public required string LogicalPath { get; init; }
    public string? MetaPath { get; init; }
    public double? SnapshotTripDistanceM { get; init; }
    public required IReadOnlyList<PhysicalRouteRow> Physical { get; init; }
    public required IReadOnlyList<LogicalRouteRow> Logical { get; init; }

    public static IReadOnlyList<ProbeRoute> LoadAll(string probePath)
    {
        var baseDir = LocateProbeDirectory(probePath);
        var physicalFiles = Directory.GetFiles(baseDir, "route_*_physical.csv", SearchOption.TopDirectoryOnly)
            .OrderBy(p => p, StringComparer.OrdinalIgnoreCase)
            .ToList();
        if (physicalFiles.Count == 0)
            throw new FileNotFoundException($"No route_*_physical.csv files found in {baseDir}");

        var routes = new List<ProbeRoute>();
        foreach (var physical in physicalFiles)
        {
            var fileStem = Path.GetFileNameWithoutExtension(physical);
            var stem = fileStem[..(fileStem.Length - "_physical".Length)];
            var logical = Path.Combine(baseDir, stem + "_logical.csv");
            if (!File.Exists(logical))
            {
                Console.WriteLine($"[WARN] Missing logical snapshot for {stem}; skipped.");
                continue;
            }
            var meta = Path.Combine(baseDir, stem + "_meta.txt");
            routes.Add(new ProbeRoute
            {
                Stem = stem,
                PhysicalPath = physical,
                LogicalPath = logical,
                MetaPath = File.Exists(meta) ? meta : null,
                SnapshotTripDistanceM = File.Exists(meta) ? ReadMetaDouble(meta, "trip_distance_m") : null,
                Physical = ReadPhysical(physical),
                Logical = ReadLogical(logical)
            });
        }
        return routes;
    }

    private static string LocateProbeDirectory(string path)
    {
        if (Directory.Exists(path))
        {
            if (Directory.GetFiles(path, "route_*_physical.csv").Length > 0) return path;
            var child = Path.Combine(path, "BetterGPSProbe");
            if (Directory.Exists(child) && Directory.GetFiles(child, "route_*_physical.csv").Length > 0) return child;
        }
        throw new DirectoryNotFoundException($"Probe directory not found or contains no route snapshots: {path}");
    }

    private static List<PhysicalRouteRow> ReadPhysical(string path)
    {
        var result = new List<PhysicalRouteRow>();
        foreach (var line in File.ReadLines(path).Skip(1))
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            var c = line.Split(',');
            if (c.Length < 7) throw new InvalidDataException($"Malformed physical CSV line: {line}");
            result.Add(new PhysicalRouteRow(
                int.Parse(c[0], CultureInfo.InvariantCulture),
                ParseHex(c[2]),
                D(c[3]), D(c[4]), D(c[5]), D(c[6])));
        }
        return result;
    }

    private static List<LogicalRouteRow> ReadLogical(string path)
    {
        var result = new List<LogicalRouteRow>();
        foreach (var line in File.ReadLines(path).Skip(1))
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            var c = line.Split(',');
            if (c.Length < 11) throw new InvalidDataException($"Malformed logical CSV line: {line}");
            result.Add(new LogicalRouteRow(
                int.Parse(c[0], CultureInfo.InvariantCulture),
                uint.Parse(c[1], CultureInfo.InvariantCulture),
                uint.Parse(c[2], CultureInfo.InvariantCulture),
                ParseHex(c[4]),
                D(c[5]), D(c[6]),
                uint.Parse(c[7], CultureInfo.InvariantCulture),
                uint.Parse(c[8], CultureInfo.InvariantCulture),
                D(c[9]), D(c[10])));
        }
        return result;
    }

    private static double? ReadMetaDouble(string path, string key)
    {
        foreach (var line in File.ReadLines(path))
        {
            var idx = line.IndexOf('=');
            if (idx <= 0) continue;
            if (!line[..idx].Trim().Equals(key, StringComparison.OrdinalIgnoreCase)) continue;
            var value = line[(idx + 1)..].Trim();
            if (double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out var result))
                return result;
        }
        return null;
    }

    private static double D(string value) => double.Parse(value, NumberStyles.Float, CultureInfo.InvariantCulture);

    private static ulong ParseHex(string value)
    {
        var s = value.Trim();
        if (s.StartsWith("0x", StringComparison.OrdinalIgnoreCase)) s = s[2..];
        return ulong.Parse(s, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
    }
}
