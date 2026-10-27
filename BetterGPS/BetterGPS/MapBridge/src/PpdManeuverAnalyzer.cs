using System.Globalization;
using System.Numerics;
using TruckLib.Models.Ppd;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal enum PpdTravelDirection
{
    Unknown,
    Forward,
    Backward
}

internal sealed record PpdPathCandidate(
    PpdTravelDirection Direction,
    IReadOnlyList<int> CurveIds,
    double LengthM,
    int StartLane,
    int EndLane,
    IReadOnlyList<uint> NavNodeIndices,
    double TurnAngleDeg);

internal sealed record PpdManeuverResult(
    bool PpdResolved,
    string PpdPath,
    uint? PpdVersion,
    int PpdNodeCount,
    int NavCurveCount,
    int MapEntryIndex,
    int MapExitIndex,
    int PpdEntryIndex,
    int PpdExitIndex,
    int PathCount,
    PpdPathCandidate? BestPath,
    int ReachableExitCount,
    IReadOnlyList<int> ReachablePpdExitIndices,
    bool IsDecisionCandidate,
    string GeometryClass,
    string? Error);

internal sealed record PpdNativeNodeResult(
    int PpdNavNodeCount,
    bool NativeNodeIdInRange,
    string NativeNavNodeType,
    int? NativeNavNodeIndexValue,
    int NativeNavNodeConnectionCount,
    string NativeNavNodeConnections,
    IReadOnlyList<int> NavNodeListIndicesWhoseIndexEqualsNativeId,
    bool NativeIdAppearsInSelectedPathNavNodeIndices,
    int SelectedPathCurveOverlapCount,
    IReadOnlyList<int> SelectedPathCurveOverlapIds,
    string? Error);

internal sealed class PpdManeuverAnalyzer
{
    private readonly PpdResolver resolver;

    public PpdManeuverAnalyzer(PpdResolver resolver)
    {
        this.resolver = resolver;
    }

    public PpdManeuverResult Analyze(Prefab prefab, int mapEntryIndex, int mapExitIndex)
    {
        var token = prefab.Model.ToString();
        if (!resolver.TryResolve(token, out var ppdPath, out var descriptor, out var error) || descriptor is null)
        {
            return new PpdManeuverResult(
                false, ppdPath ?? "", null, 0, 0,
                mapEntryIndex, mapExitIndex, -1, -1,
                0, null, 0, [], false, "PPD_UNRESOLVED", error);
        }

        if (descriptor.Nodes.Count == 0)
        {
            return new PpdManeuverResult(
                true, ppdPath, descriptor.Version, 0, descriptor.NavCurves.Count,
                mapEntryIndex, mapExitIndex, -1, -1,
                0, null, 0, [], false, "NO_CONTROL_NODES", null);
        }

        var ppdEntry = MapNodeIndexToPpdIndex(prefab, mapEntryIndex, descriptor.Nodes.Count);
        var ppdExit = MapNodeIndexToPpdIndex(prefab, mapExitIndex, descriptor.Nodes.Count);
        if (ppdEntry < 0 || ppdExit < 0 || ppdEntry >= descriptor.Nodes.Count || ppdExit >= descriptor.Nodes.Count)
        {
            return new PpdManeuverResult(
                true, ppdPath, descriptor.Version, descriptor.Nodes.Count, descriptor.NavCurves.Count,
                mapEntryIndex, mapExitIndex, ppdEntry, ppdExit,
                0, null, 0, [], false, "BAD_NODE_MAPPING", "Map node index could not be mapped to a PPD control node.");
        }

        var candidates = FindPaths(descriptor, ppdEntry, ppdExit);
        var best = candidates.OrderBy(p => p.LengthM).ThenBy(p => p.CurveIds.Count).FirstOrDefault();

        var reachable = best is null
            ? new List<int>()
            : FindReachableExits(descriptor, ppdEntry, best.Direction);

        var decision = reachable.Count > 1;
        var geometryClass = best is null ? "NO_PATH" : ClassifyAngle(best.TurnAngleDeg, decision);

        return new PpdManeuverResult(
            true, ppdPath, descriptor.Version, descriptor.Nodes.Count, descriptor.NavCurves.Count,
            mapEntryIndex, mapExitIndex, ppdEntry, ppdExit,
            candidates.Count, best, reachable.Count, reachable,
            decision, geometryClass,
            best is null ? "No directed NavCurve path found between selected entry and exit." : null);
    }

    public PpdNativeNodeResult AnalyzeNativeNode(
        Prefab prefab,
        uint nativeNodeId,
        PpdManeuverResult maneuver)
    {
        var token = prefab.Model.ToString();
        if (!resolver.TryResolve(token, out _, out var descriptor, out var error) || descriptor is null)
        {
            return new PpdNativeNodeResult(
                0, false, "", null, 0, "",
                [], false, 0, [], error ?? "PPD could not be resolved.");
        }

        var matchingIndexValues = descriptor.NavNodes
            .Select((node, index) => (node, index))
            .Where(x => x.node.Index == nativeNodeId)
            .Select(x => x.index)
            .ToList();

        var appearsInPath = maneuver.BestPath?.NavNodeIndices.Any(x => x == nativeNodeId) == true;

        if (nativeNodeId >= (uint)descriptor.NavNodes.Count)
        {
            return new PpdNativeNodeResult(
                descriptor.NavNodes.Count,
                false,
                "",
                null,
                0,
                "",
                matchingIndexValues,
                appearsInPath,
                0,
                [],
                null);
        }

        var navNode = descriptor.NavNodes[(int)nativeNodeId];
        var selectedPathCurves = maneuver.BestPath?.CurveIds.ToHashSet() ?? new HashSet<int>();
        var connectionCurves = navNode.Connections
            .SelectMany(c => c.CurveIndices)
            .Select(i => (int)i)
            .Distinct()
            .ToList();
        var overlap = connectionCurves
            .Where(selectedPathCurves.Contains)
            .OrderBy(i => i)
            .ToList();

        var connectionText = string.Join(';', navNode.Connections.Select(c =>
            $"target={c.TargetNodeIndex}:len={c.Length.ToString("0.###", CultureInfo.InvariantCulture)}:curves={string.Join('|', c.CurveIndices)}"));

        return new PpdNativeNodeResult(
            descriptor.NavNodes.Count,
            true,
            navNode.Type.ToString(),
            navNode.Index,
            navNode.Connections.Count,
            connectionText,
            matchingIndexValues,
            appearsInPath,
            overlap.Count,
            overlap,
            null);
    }

    private static int MapNodeIndexToPpdIndex(Prefab prefab, int mapNodeIndex, int ppdNodeCount)
    {
        if (mapNodeIndex < 0 || mapNodeIndex >= prefab.Nodes.Count || ppdNodeCount <= 0) return -1;
        return (mapNodeIndex + prefab.Origin) % ppdNodeCount;
    }

    private static List<PpdPathCandidate> FindPaths(PrefabDescriptor descriptor, int entry, int exit)
    {
        var result = new List<PpdPathCandidate>();
        var entryNode = descriptor.Nodes[entry];
        var exitNode = descriptor.Nodes[exit];

        AddDirectionCandidates(descriptor, result,
            PpdTravelDirection.Forward,
            ValidLines(entryNode.OutputLines, descriptor.NavCurves.Count),
            ValidLines(exitNode.InputLines, descriptor.NavCurves.Count));

        AddDirectionCandidates(descriptor, result,
            PpdTravelDirection.Backward,
            ValidLines(entryNode.InputLines, descriptor.NavCurves.Count),
            ValidLines(exitNode.OutputLines, descriptor.NavCurves.Count));

        // Same geometric lane path can occasionally be discoverable from multiple start/end
        // line pairs. Keep distinct curve sequences only.
        return result
            .GroupBy(p => p.Direction + ":" + string.Join('-', p.CurveIds))
            .Select(g => g.OrderBy(x => x.LengthM).First())
            .ToList();
    }

    private static void AddDirectionCandidates(
        PrefabDescriptor descriptor,
        List<PpdPathCandidate> output,
        PpdTravelDirection direction,
        IReadOnlyList<int> startLines,
        IReadOnlyList<int> targetLines)
    {
        foreach (var start in startLines)
        {
            foreach (var target in targetLines)
            {
                var path = FindShortestCurvePath(descriptor, start, target, direction);
                if (path.Count == 0) continue;

                var length = path.Sum(id => (double)descriptor.NavCurves[id].Length);
                var first = descriptor.NavCurves[path[0]];
                var last = descriptor.NavCurves[path[^1]];
                var startLane = direction == PpdTravelDirection.Forward
                    ? first.LeadsToNodes.StartLane
                    : first.LeadsToNodes.EndLane;
                var endLane = direction == PpdTravelDirection.Forward
                    ? last.LeadsToNodes.EndLane
                    : last.LeadsToNodes.StartLane;
                var navNodeIndices = path.Select(id => descriptor.NavCurves[id].NavNodeIndex).ToList();
                var angle = CalculateNetTurnAngle(descriptor, path, direction);

                output.Add(new PpdPathCandidate(direction, path, length, startLane, endLane, navNodeIndices, angle));
            }
        }
    }

    private static List<int> FindShortestCurvePath(
        PrefabDescriptor descriptor,
        int start,
        int target,
        PpdTravelDirection direction)
    {
        if (!ValidCurveId(start, descriptor.NavCurves.Count) || !ValidCurveId(target, descriptor.NavCurves.Count)) return [];
        if (start == target) return [start];

        // Weighted search over the undirected PPD topology. Direction is encoded by
        // which control-node line set we start/end from, not by traversing only one
        // of PreviousLines/NextLines.
        var queue = new PriorityQueue<int, double>();
        var distance = new Dictionary<int, double>();
        var parent = new Dictionary<int, int>();
        distance[start] = descriptor.NavCurves[start].Length;
        parent[start] = -1;
        queue.Enqueue(start, distance[start]);

        while (queue.Count > 0)
        {
            queue.TryDequeue(out var current, out var currentCost);
            if (!distance.TryGetValue(current, out var known) || currentCost > known + 1e-6) continue;
            if (current == target) break;

            var curve = descriptor.NavCurves[current];
            // IMPORTANT: PPD PreviousLines/NextLines are used by SCS as topology links,
            // but path reconstruction cannot treat only one side as a strictly directed
            // graph. ETS2LA traverses the union of both sets, then uses the selected
            // control-node Input/Output side to establish travel orientation.
            //
            // The v0.2 analyzer incorrectly followed only NextLines for Forward and only
            // PreviousLines for Backward. In the user's capture every native route_task
            // prefab landed in NO_PATH, which is a strong signature of that mistake.
            var nextIds = curve.PreviousLines.Concat(curve.NextLines).Distinct();
            foreach (var next in nextIds)
            {
                if (!ValidCurveId(next, descriptor.NavCurves.Count)) continue;
                var candidate = currentCost + descriptor.NavCurves[next].Length;
                if (distance.TryGetValue(next, out var old) && candidate >= old - 1e-6) continue;
                distance[next] = candidate;
                parent[next] = current;
                queue.Enqueue(next, candidate);
            }
        }

        if (!parent.ContainsKey(target)) return [];
        var path = new List<int>();
        var cursor = target;
        while (cursor >= 0)
        {
            path.Add(cursor);
            cursor = parent[cursor];
        }
        path.Reverse();
        return path;
    }

    private static List<int> FindReachableExits(PrefabDescriptor descriptor, int entry, PpdTravelDirection direction)
    {
        var exits = new List<int>();
        for (var i = 0; i < descriptor.Nodes.Count; i++)
        {
            if (i == entry) continue;
            var paths = FindPathsOneDirection(descriptor, entry, i, direction);
            if (paths > 0) exits.Add(i);
        }
        return exits;
    }

    private static int FindPathsOneDirection(PrefabDescriptor descriptor, int entry, int exit, PpdTravelDirection direction)
    {
        var entryNode = descriptor.Nodes[entry];
        var exitNode = descriptor.Nodes[exit];
        var starts = direction == PpdTravelDirection.Forward
            ? ValidLines(entryNode.OutputLines, descriptor.NavCurves.Count)
            : ValidLines(entryNode.InputLines, descriptor.NavCurves.Count);
        var targets = direction == PpdTravelDirection.Forward
            ? ValidLines(exitNode.InputLines, descriptor.NavCurves.Count)
            : ValidLines(exitNode.OutputLines, descriptor.NavCurves.Count);

        var count = 0;
        foreach (var start in starts)
        foreach (var target in targets)
            if (FindShortestCurvePath(descriptor, start, target, direction).Count > 0)
                count++;
        return count;
    }

    private static List<int> ValidLines(IEnumerable<int> lines, int curveCount) =>
        lines.Where(id => ValidCurveId(id, curveCount)).Distinct().ToList();

    private static bool ValidCurveId(int id, int count) => id >= 0 && id < count;

    private static double CalculateNetTurnAngle(PrefabDescriptor descriptor, IReadOnlyList<int> path, PpdTravelDirection direction)
    {
        if (path.Count == 0) return 0;
        var first = descriptor.NavCurves[path[0]];
        var last = descriptor.NavCurves[path[^1]];

        Vector3 startVec;
        Vector3 endVec;
        if (direction == PpdTravelDirection.Forward)
        {
            startVec = first.EndPosition - first.StartPosition;
            endVec = last.EndPosition - last.StartPosition;
        }
        else
        {
            startVec = first.StartPosition - first.EndPosition;
            endVec = last.StartPosition - last.EndPosition;
        }

        var a = new Vector2(startVec.X, startVec.Z);
        var b = new Vector2(endVec.X, endVec.Z);
        if (a.LengthSquared() < 1e-8f || b.LengthSquared() < 1e-8f) return 0;
        a = Vector2.Normalize(a);
        b = Vector2.Normalize(b);

        var dot = Math.Clamp(Vector2.Dot(a, b), -1f, 1f);
        // Clockwise-positive in the prefab X/Z plane: positive = right, negative = left.
        // This matches the lane-angle convention used by truckermudgeon's navigation model.
        var crossY = a.Y * b.X - a.X * b.Y;
        return Math.Atan2(crossY, dot) * (180.0 / Math.PI);
    }

    private static string ClassifyAngle(double angleDeg, bool decision)
    {
        var abs = Math.Abs(angleDeg);
        var side = angleDeg > 0 ? "RIGHT" : angleDeg < 0 ? "LEFT" : "";
        string geometry;
        if (abs <= 2) geometry = "THROUGH";
        else if (abs <= 70) geometry = "SLIGHT_" + side;
        else if (abs <= 110) geometry = side;
        else if (abs <= 160) geometry = "SHARP_" + side;
        else geometry = "U_TURN_" + side;

        return decision ? "DECISION_" + geometry : "TRANSPARENT_" + geometry;
    }
}
