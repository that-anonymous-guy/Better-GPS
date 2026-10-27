using System.Globalization;
using System.Numerics;
using TruckLib.Models.Ppd;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal sealed record RouteCorridorEdge(
    int EdgeIndex,
    int SegmentIndex,
    string ItemType,
    ulong ItemUid,
    string SourceSubId,
    Vector3 Start,
    Vector3 End,
    double StartStationM,
    double EndStationM,
    double StartNavStationM,
    double EndNavStationM,
    double HeadingDeg);

internal sealed record RouteCorridorSegment(
    int SegmentIndex,
    string ItemType,
    ulong ItemUid,
    double StartStationM,
    double EndStationM,
    double StartNavStationM,
    double EndNavStationM,
    int EdgeStartIndex,
    int EdgeCount,
    string SourceIds,
    double? PrefabTransformMaxErrorM);

internal sealed record RouteProjection(
    int EdgeIndex,
    int SegmentIndex,
    string ItemType,
    ulong ItemUid,
    double StationM,
    double NavStationM,
    double LateralM,
    double HeadingDeg,
    Vector3 Position,
    double EdgeT);

internal sealed class RouteCorridor
{
    public RouteCorridor(IReadOnlyList<RouteCorridorEdge> edges, IReadOnlyList<RouteCorridorSegment> segments)
    {
        Edges = edges;
        Segments = segments;
        if (edges.Count == 0)
        {
            MinX = MinZ = MaxX = MaxZ = 0;
            TotalStationM = TotalNavStationM = 0;
        }
        else
        {
            MinX = edges.Min(e => Math.Min(e.Start.X, e.End.X));
            MaxX = edges.Max(e => Math.Max(e.Start.X, e.End.X));
            MinZ = edges.Min(e => Math.Min(e.Start.Z, e.End.Z));
            MaxZ = edges.Max(e => Math.Max(e.Start.Z, e.End.Z));
            TotalStationM = edges.Max(e => e.EndStationM);
            TotalNavStationM = edges.Max(e => e.EndNavStationM);
        }
    }

    public IReadOnlyList<RouteCorridorEdge> Edges { get; }
    public IReadOnlyList<RouteCorridorSegment> Segments { get; }
    public double MinX { get; }
    public double MaxX { get; }
    public double MinZ { get; }
    public double MaxZ { get; }
    public double TotalStationM { get; }
    public double TotalNavStationM { get; }

    public RouteProjection? Project(Vector3 point)
    {
        if (Edges.Count == 0) return null;
        var p = new Vector2(point.X, point.Z);
        RouteProjection? best = null;
        var bestSq = double.MaxValue;

        foreach (var edge in Edges)
        {
            var a = new Vector2(edge.Start.X, edge.Start.Z);
            var b = new Vector2(edge.End.X, edge.End.Z);
            var ab = b - a;
            var lenSq = (double)ab.LengthSquared();
            if (lenSq < 1e-10) continue;
            var t = Math.Clamp(Vector2.Dot(p - a, ab) / (float)lenSq, 0f, 1f);
            var q = a + ab * t;
            var d = p - q;
            var sq = (double)d.LengthSquared();
            if (sq >= bestSq) continue;
            bestSq = sq;
            var y = edge.Start.Y + (edge.End.Y - edge.Start.Y) * t;
            best = new RouteProjection(
                edge.EdgeIndex,
                edge.SegmentIndex,
                edge.ItemType,
                edge.ItemUid,
                Lerp(edge.StartStationM, edge.EndStationM, t),
                Lerp(edge.StartNavStationM, edge.EndNavStationM, t),
                Math.Sqrt(sq),
                edge.HeadingDeg,
                new Vector3(q.X, y, q.Y),
                t);
        }
        return best;
    }

    public RouteCorridorSegment? Segment(int segmentIndex) => Segments.FirstOrDefault(s => s.SegmentIndex == segmentIndex);

    public void WriteCsv(string path)
    {
        using var writer = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        writer.WriteLine("edge_index,segment_index,item_type,item_uid,source_sub_id,start_station_m,end_station_m,start_nav_station_m,end_nav_station_m,start_x,start_y,start_z,end_x,end_y,end_z,heading_deg");
        foreach (var e in Edges)
        {
            writer.WriteLine(string.Join(',',
                e.EdgeIndex,
                e.SegmentIndex,
                Csv(e.ItemType),
                Hex(e.ItemUid),
                Csv(e.SourceSubId),
                F(e.StartStationM),
                F(e.EndStationM),
                F(e.StartNavStationM),
                F(e.EndNavStationM),
                F(e.Start.X), F(e.Start.Y), F(e.Start.Z),
                F(e.End.X), F(e.End.Y), F(e.End.Z),
                F(e.HeadingDeg)));
        }
    }

    public void WriteSegmentsCsv(string path)
    {
        using var writer = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        writer.WriteLine("segment_index,item_type,item_uid,start_station_m,end_station_m,physical_length_m,start_nav_station_m,end_nav_station_m,nav_length_m,edge_start_index,edge_count,source_ids,prefab_transform_max_error_m");
        foreach (var s in Segments)
        {
            writer.WriteLine(string.Join(',',
                s.SegmentIndex,
                Csv(s.ItemType),
                Hex(s.ItemUid),
                F(s.StartStationM), F(s.EndStationM), F(s.EndStationM - s.StartStationM),
                F(s.StartNavStationM), F(s.EndNavStationM), F(s.EndNavStationM - s.StartNavStationM),
                s.EdgeStartIndex, s.EdgeCount, Csv(s.SourceIds),
                s.PrefabTransformMaxErrorM.HasValue ? F(s.PrefabTransformMaxErrorM.Value) : ""));
        }
    }

    private static double Lerp(double a, double b, double t) => a + (b - a) * t;
    private static string F(double value) => value.ToString("0.######", CultureInfo.InvariantCulture);
    private static string Hex(ulong value) => value.ToString("X16", CultureInfo.InvariantCulture);
    private static string Csv(string? value)
    {
        value ??= "";
        if (!value.Contains(',') && !value.Contains('"') && !value.Contains('\n') && !value.Contains('\r')) return value;
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}

internal sealed class RouteCorridorBuilder
{
    private readonly Map map;
    private readonly PpdResolver ppdResolver;
    private readonly ManeuverFeatureExtractor featureExtractor;

    public RouteCorridorBuilder(Map map, PpdResolver ppdResolver, ManeuverFeatureExtractor featureExtractor)
    {
        this.map = map;
        this.ppdResolver = ppdResolver;
        this.featureExtractor = featureExtractor;
    }

    public RouteCorridor Build(ProbeRoute route,CancellationToken token=default)
    {
        var edges = new List<RouteCorridorEdge>();
        var segments = new List<RouteCorridorSegment>();
        double station = 0;
        double navStation = 0;

        for (var i = 0; i + 1 < route.Physical.Count; i++)
        {
            token.ThrowIfCancellationRequested();
            if (!map.Nodes.TryGetValue(route.Physical[i].NodeUid, out var a) ||
                !map.Nodes.TryGetValue(route.Physical[i + 1].NodeUid, out var b))
                continue;

            var item = RouteMapItemSelector.Resolve(map,a,b,route,i);
            if (item is null) continue;

            var navLength = Math.Max(0, route.Physical[i + 1].DistanceFromPreviousM);
            var rawPoints = new List<(Vector3 Position, string SubId)>();
            double? transformError = null;
            string sourceIds = "";

            if (item is Road road)
            {
                var points = BuildRoadPoints(road, a, b);
                for (var p = 0; p < points.Count; p++) rawPoints.Add((points[p], "road"));
            }
            else if (item is Prefab prefab)
            {
                var entry = FindNodeIndex(prefab, a.Uid);
                var exit = FindNodeIndex(prefab, b.Uid);
                var feature = featureExtractor.AnalyzeRoute(prefab, entry, exit,route,i);
                sourceIds = string.Join('|', feature.SelectedCurveIds);
                if (feature.PpdResolved && feature.SelectedCurveIds.Count > 0 &&
                    ppdResolver.TryResolve(prefab.Model.ToString(), out _, out var descriptor, out _) && descriptor is not null &&
                    TryBuildPrefabTransform(prefab, descriptor, out var transform, out var maxError))
                {
                    transformError = maxError;
                    foreach (var curveId in feature.SelectedCurveIds)
                    {
                        if (curveId < 0 || curveId >= descriptor.NavCurves.Count) continue;
                        var curve = descriptor.NavCurves[curveId];
                        var local = DecisionBranchResolver.SampleNativeCurve(curve.StartPosition, curve.StartRotation, curve.EndPosition, curve.EndRotation);
                        foreach (var p in local)
                            rawPoints.Add((transform.Transform(p), curveId.ToString(CultureInfo.InvariantCulture)));
                    }
                }

                if (rawPoints.Count < 2)
                {
                    rawPoints.Clear();
                    rawPoints.Add((a.Position, "fallback"));
                    rawPoints.Add((b.Position, "fallback"));
                }
                else
                {
                    // Ensure exact physical route endpoints are represented. Nav-curve lane centers
                    // can be a few meters away from the prefab control node center.
                    if (DistanceXZ(rawPoints[0].Position, a.Position) > 0.25)
                        rawPoints.Insert(0, (a.Position, "entry"));
                    if (DistanceXZ(rawPoints[^1].Position, b.Position) > 0.25)
                        rawPoints.Add((b.Position, "exit"));
                }
            }
            else
            {
                rawPoints.Add((a.Position, "fallback"));
                rawPoints.Add((b.Position, "fallback"));
            }

            rawPoints = Deduplicate(rawPoints);
            if (rawPoints.Count < 2)
            {
                rawPoints = [(a.Position, "fallback"), (b.Position, "fallback")];
            }

            var localLengths = new double[rawPoints.Count - 1];
            var physicalLength = 0.0;
            for (var p = 0; p + 1 < rawPoints.Count; p++)
            {
                localLengths[p] = DistanceXZ(rawPoints[p].Position, rawPoints[p + 1].Position);
                physicalLength += localLengths[p];
            }
            if (physicalLength < 1e-6)
                physicalLength = DistanceXZ(a.Position, b.Position);

            var segmentStartStation = station;
            var segmentStartNav = navStation;
            var edgeStartIndex = edges.Count;
            var accumulated = 0.0;
            for (var p = 0; p + 1 < rawPoints.Count; p++)
            {
                var len = localLengths[p];
                if (len < 1e-6) continue;
                var edgeStartStation = segmentStartStation + accumulated;
                // Keep the same addition order as the next edge/segment boundary.
                // Otherwise full-precision live edges can overlap by a few floating-point ulps.
                var edgeEndStation = segmentStartStation + (accumulated + len);
                var startFraction = physicalLength > 1e-6 ? accumulated / physicalLength : 0;
                var endFraction = physicalLength > 1e-6 ? (accumulated + len) / physicalLength : 1;
                var edgeStartNav = segmentStartNav + navLength * startFraction;
                var edgeEndNav = segmentStartNav + navLength * endFraction;
                var start = rawPoints[p].Position;
                var end = rawPoints[p + 1].Position;
                var heading = HeadingDeg(start, end);
                edges.Add(new RouteCorridorEdge(
                    edges.Count,
                    i,
                    item.ItemType.ToString(),
                    item.Uid,
                    rawPoints[p].SubId,
                    start,
                    end,
                    edgeStartStation,
                    edgeEndStation,
                    edgeStartNav,
                    edgeEndNav,
                    heading));
                accumulated += len;
            }

            station = segmentStartStation + physicalLength;
            navStation = segmentStartNav + navLength;
            segments.Add(new RouteCorridorSegment(
                i,
                item.ItemType.ToString(),
                item.Uid,
                segmentStartStation,
                station,
                segmentStartNav,
                navStation,
                edgeStartIndex,
                edges.Count - edgeStartIndex,
                sourceIds,
                transformError));
        }

        return new RouteCorridor(edges, segments);
    }

    private List<Vector3> BuildRoadPoints(Road road, INode routeStart, INode routeEnd)
    {
        // Use the game's road curve, including its actual tangent lengths.
        // Reconstructing tangents from the endpoint chord displaced curved
        // roads by tens of metres and falsely lost the truck's corridor.
        var steps=Math.Clamp((int)Math.Ceiling(road.Length/5f),2,4000);
        var forwardPoints=Enumerable.Range(0,steps+1).Select(i=>road.InterpolateCurve(i/(float)steps).Position).ToList();
        if(forwardPoints.Any(p=>!RoadLaneGeometryResolver.Finite(p)))throw new InvalidDataException("Non-finite road curve.");
        if (road.Node.Uid == routeStart.Uid && road.ForwardNode.Uid == routeEnd.Uid)
            return forwardPoints;
        if (road.ForwardNode.Uid == routeStart.Uid && road.Node.Uid == routeEnd.Uid)
        {
            forwardPoints.Reverse();
            return forwardPoints;
        }
        // Defensive fallback for malformed references.
        return [routeStart.Position, routeEnd.Position];
    }

    private static List<Vector3> SampleHermite(Vector3 start, Quaternion startRotation, Vector3 end, Quaternion endRotation)
    {
        var chord = Vector3.Distance(start, end);
        if (chord < 1e-5f) return [start, end];
        var startDir = Vector3.Transform(Vector3.UnitZ, startRotation);
        var endDir = Vector3.Transform(Vector3.UnitZ, endRotation);
        if (startDir.LengthSquared() < 1e-8f) startDir = Vector3.Normalize(end - start);
        else startDir = Vector3.Normalize(startDir);
        if (endDir.LengthSquared() < 1e-8f) endDir = Vector3.Normalize(end - start);
        else endDir = Vector3.Normalize(endDir);
        return SampleHermite(start, startDir * chord, end, endDir * chord, 8);
    }

    private static List<Vector3> SampleHermite(Vector3 p0, Vector3 m0, Vector3 p1, Vector3 m1, int steps)
    {
        var result = new List<Vector3>(steps + 1);
        for (var i = 0; i <= steps; i++)
        {
            var t = i / (double)steps;
            var t2 = t * t;
            var t3 = t2 * t;
            var h00 = 2 * t3 - 3 * t2 + 1;
            var h10 = t3 - 2 * t2 + t;
            var h01 = -2 * t3 + 3 * t2;
            var h11 = t3 - t2;
            result.Add(p0 * (float)h00 + m0 * (float)h10 + p1 * (float)h01 + m1 * (float)h11);
        }
        return result;
    }

    private static bool TryBuildPrefabTransform(Prefab prefab, PrefabDescriptor descriptor, out PrefabRigidTransform transform, out double maxError)
    {
        transform = default;
        maxError = double.PositiveInfinity;
        if (prefab.Nodes.Count == 0 || descriptor.Nodes.Count == 0) return false;
        var origin = (int)prefab.Origin;
        if (origin < 0 || origin >= descriptor.Nodes.Count) return false;

        var map0 = prefab.Nodes[0].Position;
        var local0 = descriptor.Nodes[origin].Position;
        var localBase = new Vector2(local0.X, local0.Z);
        var worldBase = new Vector2(map0.X, map0.Z);

        var angle = double.NaN;
        for (var mapIndex = 1; mapIndex < prefab.Nodes.Count; mapIndex++)
        {
            var ppdIndex = (mapIndex + origin) % descriptor.Nodes.Count;
            var local = descriptor.Nodes[ppdIndex].Position;
            var lv = new Vector2(local.X - local0.X, local.Z - local0.Z);
            var world = prefab.Nodes[mapIndex].Position;
            var wv = new Vector2(world.X - map0.X, world.Z - map0.Z);
            if (lv.LengthSquared() < 0.25f || wv.LengthSquared() < 0.25f) continue;
            var dot = Vector2.Dot(Vector2.Normalize(lv), Vector2.Normalize(wv));
            var cross = lv.X * wv.Y - lv.Y * wv.X;
            angle = Math.Atan2(cross, Vector2.Dot(lv, wv));
            break;
        }

        if (double.IsNaN(angle))
        {
            // One-node prefabs are unusual; fall back to control-node direction vs map node rotation.
            var localDir = descriptor.Nodes[origin].Direction;
            var localYaw = Math.Atan2(localDir.X, localDir.Z);
            var worldForward = Vector3.Transform(Vector3.UnitZ, prefab.Nodes[0].Rotation);
            var worldYaw = Math.Atan2(worldForward.X, worldForward.Z);
            angle = worldYaw - localYaw;
        }

        transform = new PrefabRigidTransform(localBase, worldBase, map0.Y - local0.Y, angle);
        maxError = 0;
        for (var mapIndex = 0; mapIndex < prefab.Nodes.Count; mapIndex++)
        {
            var ppdIndex = (mapIndex + origin) % descriptor.Nodes.Count;
            var predicted = transform.Transform(descriptor.Nodes[ppdIndex].Position);
            maxError = Math.Max(maxError, DistanceXZ(predicted, prefab.Nodes[mapIndex].Position));
        }
        return true;
    }

    private readonly record struct PrefabRigidTransform(Vector2 LocalBase, Vector2 WorldBase, float YOffset, double Angle)
    {
        public Vector3 Transform(Vector3 local)
        {
            var dx = local.X - LocalBase.X;
            var dz = local.Z - LocalBase.Y;
            var cos = Math.Cos(Angle);
            var sin = Math.Sin(Angle);
            var x = WorldBase.X + (float)(cos * dx - sin * dz);
            var z = WorldBase.Y + (float)(sin * dx + cos * dz);
            return new Vector3(x, local.Y + YOffset, z);
        }
    }

    private MapItem? ResolveCommonMapItem(INode a, INode b)
    {
        Span<ulong> aIds = stackalloc ulong[2] { a.ForwardItem?.Uid ?? 0, a.BackwardItem?.Uid ?? 0 };
        Span<ulong> bIds = stackalloc ulong[2] { b.ForwardItem?.Uid ?? 0, b.BackwardItem?.Uid ?? 0 };
        foreach (var aId in aIds)
        {
            if (aId == 0) continue;
            foreach (var bId in bIds)
                if (aId == bId && map.MapItems.TryGetValue(aId, out var item)) return item;
        }
        return null;
    }

    private static int FindNodeIndex(Prefab prefab, ulong uid)
    {
        for (var i = 0; i < prefab.Nodes.Count; i++) if (prefab.Nodes[i].Uid == uid) return i;
        return -1;
    }

    private static List<(Vector3 Position, string SubId)> Deduplicate(List<(Vector3 Position, string SubId)> points)
    {
        var result = new List<(Vector3 Position, string SubId)>();
        foreach (var point in points)
        {
            if (result.Count > 0 && DistanceXZ(result[^1].Position, point.Position) < 0.02) continue;
            result.Add(point);
        }
        return result;
    }

    private static double DistanceXZ(Vector3 a, Vector3 b)
    {
        var dx = (double)a.X - b.X;
        var dz = (double)a.Z - b.Z;
        return Math.Sqrt(dx * dx + dz * dz);
    }

    private static double HeadingDeg(Vector3 a, Vector3 b)
    {
        var dx = b.X - a.X;
        var dz = b.Z - a.Z;
        if (Math.Abs(dx) < 1e-8 && Math.Abs(dz) < 1e-8) return 0;
        return Math.Atan2(dx, dz) * 180.0 / Math.PI;
    }
}
