using System.Globalization;
using TruckLib.Models.Ppd;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal enum ExactNavGraphOrientation
{
    None,
    EntryToExit,
    ExitToEntry
}

internal sealed record ExactNavGraphEdge(
    int SourceNodeId,
    int TargetNodeId,
    double LengthM,
    IReadOnlyList<int> CurveIds);

internal sealed record DirectedNavGraphPath(
    IReadOnlyList<int> NodeIds,
    IReadOnlyList<ExactNavGraphEdge> Edges,
    double TotalLengthM)
{
    public IReadOnlyList<int> CurveIds => Edges.SelectMany(e => e.CurveIds).ToList();
}

internal sealed record ExactNavGraphResult(
    bool PpdResolved,
    string PpdPath,
    int PpdEntryIndex,
    int PpdExitIndex,
    IReadOnlyList<int> EntryPhysicalNavNodeIds,
    IReadOnlyList<int> ExitPhysicalNavNodeIds,
    int PpdNavNodeCount,
    bool NativeNodeIdInRange,
    string NativeNavNodeType,
    int? NativeNavNodeIndexValue,
    int CandidatePathCount,
    int CandidateOrientationCount,
    ExactNavGraphOrientation Orientation,
    bool ExactPathFound,
    IReadOnlyList<int> DirectedNodePath,
    IReadOnlyList<int> DirectedCurveIds,
    IReadOnlyList<int> VehicleNodePath,
    IReadOnlyList<int> VehicleCurveIds,
    string DirectedEdges,
    string VehicleEdges,
    int NativeVehiclePathPosition,
    int? VehicleIncomingSourceNodeId,
    IReadOnlyList<int> VehicleIncomingCurveIds,
    int? VehicleOutgoingTargetNodeId,
    IReadOnlyList<int> VehicleOutgoingCurveIds,
    double GraphEntryToNativeM,
    double GraphNativeToExitM,
    double GraphTotalM,
    double RouteEntryToNativeM,
    double RouteNativeToExitM,
    double RouteTotalM,
    double EntryDistanceDeltaM,
    double ExitDistanceDeltaM,
    double TotalDistanceDeltaM,
    bool DistanceMatch2m,
    bool NativeIndexCurveOnExactPath,
    bool NativeOnUnconstrainedShortestPath,
    bool ConstrainedEqualsUnconstrainedShortestPath,
    IReadOnlyList<int> UnconstrainedNodePath,
    IReadOnlyList<int> UnconstrainedCurveIds,
    int HeuristicCurveOverlapCount,
    bool HeuristicCurveSequenceMatch,
    string ValidationStatus,
    string? Error);

internal sealed class ExactNavGraphResolver
{
    private readonly PpdResolver resolver;

    public ExactNavGraphResolver(PpdResolver resolver)
    {
        this.resolver = resolver;
    }

    public ExactNavGraphResult Resolve(
        Prefab prefab,
        int mapEntryIndex,
        int mapExitIndex,
        uint nativeNodeId,
        double routeEntryToNativeM,
        double routeNativeToExitM,
        PpdPathCandidate? heuristicPath,bool forwardOnly=false)
    {
        var token = prefab.Model.ToString();
        if (!resolver.TryResolve(token, out var ppdPath, out var descriptor, out var error) || descriptor is null)
        {
            return Empty(ppdPath ?? "", mapEntryIndex, mapExitIndex,
                routeEntryToNativeM, routeNativeToExitM,
                "PPD_UNRESOLVED", error ?? "PPD could not be resolved.");
        }

        var ppdEntry = MapNodeIndexToPpdIndex(prefab, mapEntryIndex, descriptor.Nodes.Count);
        var ppdExit = MapNodeIndexToPpdIndex(prefab, mapExitIndex, descriptor.Nodes.Count);
        var entryPhysical = FindPhysicalNavNodes(descriptor, ppdEntry);
        var exitPhysical = FindPhysicalNavNodes(descriptor, ppdExit);
        var inRange = nativeNodeId < (uint)descriptor.NavNodes.Count;
        var nativeType = inRange ? descriptor.NavNodes[(int)nativeNodeId].Type.ToString() : "";
        var nativeIndexValue = inRange ? (int)descriptor.NavNodes[(int)nativeNodeId].Index : (int?)null;

        if (ppdEntry < 0 || ppdExit < 0)
        {
            return BaseFailure(ppdPath, ppdEntry, ppdExit, entryPhysical, exitPhysical,
                descriptor, inRange, nativeType, nativeIndexValue,
                routeEntryToNativeM, routeNativeToExitM,
                "BAD_NODE_MAPPING", "Map prefab node could not be mapped to a PPD control node.");
        }
        if (!inRange)
        {
            return BaseFailure(ppdPath, ppdEntry, ppdExit, entryPhysical, exitPhysical,
                descriptor, false, "", null,
                routeEntryToNativeM, routeNativeToExitM,
                "NATIVE_NODE_ID_OUT_OF_RANGE", $"native node_id {nativeNodeId} is outside NavNodes[0..{descriptor.NavNodes.Count - 1}].");
        }
        if (entryPhysical.Count == 0 || exitPhysical.Count == 0)
        {
            return BaseFailure(ppdPath, ppdEntry, ppdExit, entryPhysical, exitPhysical,
                descriptor, true, nativeType, nativeIndexValue,
                routeEntryToNativeM, routeNativeToExitM,
                "PHYSICAL_NAVNODE_NOT_FOUND", "Could not find PhysicalNode NavNodes for one or both PPD control nodes.");
        }

        var nativeId = (int)nativeNodeId;
        var candidates = new List<Candidate>();
        AddCandidates(descriptor, candidates, ExactNavGraphOrientation.EntryToExit,
            entryPhysical, nativeId, exitPhysical,
            routeEntryToNativeM, routeNativeToExitM);
        if(!forwardOnly)AddCandidates(descriptor, candidates, ExactNavGraphOrientation.ExitToEntry,
            exitPhysical, nativeId, entryPhysical,
            routeEntryToNativeM, routeNativeToExitM);

        if (candidates.Count == 0)
        {
            return BaseFailure(ppdPath, ppdEntry, ppdExit, entryPhysical, exitPhysical,
                descriptor, true, nativeType, nativeIndexValue,
                routeEntryToNativeM, routeNativeToExitM,
                "NO_DIRECTED_NAVGRAPH_PATH_VIA_NATIVE_NODE",
                "No directed PPD NavNode path between the physical entry/exit nodes passes through native node_id.");
        }

        // Route-task distances give us an independent way to choose between the two possible
        // directed orientations if both exist. Prefer the candidate whose two halves best match
        // entry->native and native->exit; then total length; then fewer graph edges.
        var best = candidates
            .OrderBy(c => c.DistanceErrorScore)
            .ThenBy(c => c.VehicleEntryToNativeM + c.VehicleNativeToExitM)
            .ThenBy(c => c.Directed.Edges.Count)
            .First();

        var vehicleEdges = ToVehicleEdges(best.Directed, best.Orientation);
        var vehicleNodes = best.Orientation == ExactNavGraphOrientation.EntryToExit
            ? best.Directed.NodeIds.ToList()
            : best.Directed.NodeIds.Reverse().ToList();
        var vehicleCurves = vehicleEdges.SelectMany(e => e.CurveIds).ToList();
        var nativeVehiclePos = vehicleNodes.IndexOf(nativeId);

        ExactNavGraphEdge? incoming = null;
        ExactNavGraphEdge? outgoing = null;
        if (nativeVehiclePos > 0 && nativeVehiclePos - 1 < vehicleEdges.Count)
            incoming = vehicleEdges[nativeVehiclePos - 1];
        if (nativeVehiclePos >= 0 && nativeVehiclePos < vehicleEdges.Count)
            outgoing = vehicleEdges[nativeVehiclePos];

        var routeTotal = routeEntryToNativeM + routeNativeToExitM;
        var graphEntryToNative = best.VehicleEntryToNativeM;
        var graphNativeToExit = best.VehicleNativeToExitM;
        var graphTotal = graphEntryToNative + graphNativeToExit;
        var entryDelta = graphEntryToNative - routeEntryToNativeM;
        var exitDelta = graphNativeToExit - routeNativeToExitM;
        var totalDelta = graphTotal - routeTotal;
        var distanceMatch2m = Math.Abs(entryDelta) <= 2.0 && Math.Abs(exitDelta) <= 2.0;

        var nativeIndexCurveOnPath = nativeIndexValue.HasValue && vehicleCurves.Contains(nativeIndexValue.Value);

        // Independent graph check: does the ordinary shortest directed route between the
        // same physical NavNodes pass through ATS's node_id even when we do NOT force it?
        var unconstrained = FindShortestDirectedPath(
            descriptor,
            best.Directed.NodeIds[0],
            best.Directed.NodeIds[^1]);
        var nativeOnUnconstrained = unconstrained?.NodeIds.Contains(nativeId) == true;
        var constrainedEqualsUnconstrained = unconstrained is not null &&
            unconstrained.NodeIds.SequenceEqual(best.Directed.NodeIds) &&
            unconstrained.CurveIds.SequenceEqual(best.Directed.CurveIds);

        var heuristicCurves = heuristicPath?.CurveIds?.ToList() ?? [];
        var exactSet = vehicleCurves.ToHashSet();
        var heuristicOverlap = heuristicCurves.Distinct().Count(exactSet.Contains);
        var sequenceMatch = heuristicCurves.SequenceEqual(vehicleCurves);

        // The key validation is structural: ATS node_id is a valid index into PPD.NavNodes,
        // that indexed node is an internal AI nav-node, and the directed physical-entry/exit
        // route selected by the PPD graph can be reconstructed through that exact list index.
        // NavNode.Index (AI-curve endpoint reverse mapping) and route-task distances are independent
        // corroborating checks, not prerequisites for the node_id interpretation itself.
        var status = nativeType == NavNodeType.AiNode.ToString()
            ? "VALIDATED_NODE_ID_AS_NAVNODE_INDEX"
            : "EXACT_PATH_NATIVE_NOT_AI_NODE";

        return new ExactNavGraphResult(
            true,
            ppdPath,
            ppdEntry,
            ppdExit,
            entryPhysical,
            exitPhysical,
            descriptor.NavNodes.Count,
            true,
            nativeType,
            nativeIndexValue,
            candidates.Count,
            candidates.Select(c => c.Orientation).Distinct().Count(),
            best.Orientation,
            true,
            best.Directed.NodeIds,
            best.Directed.CurveIds,
            vehicleNodes,
            vehicleCurves,
            FormatEdges(best.Directed.Edges),
            FormatEdges(vehicleEdges),
            nativeVehiclePos,
            incoming?.SourceNodeId,
            incoming?.CurveIds ?? [],
            outgoing?.TargetNodeId,
            outgoing?.CurveIds ?? [],
            graphEntryToNative,
            graphNativeToExit,
            graphTotal,
            routeEntryToNativeM,
            routeNativeToExitM,
            routeTotal,
            entryDelta,
            exitDelta,
            totalDelta,
            distanceMatch2m,
            nativeIndexCurveOnPath,
            nativeOnUnconstrained,
            constrainedEqualsUnconstrained,
            unconstrained?.NodeIds ?? [],
            unconstrained?.CurveIds ?? [],
            heuristicOverlap,
            sequenceMatch,
            status,
            null);
    }

    private static void AddCandidates(
        PrefabDescriptor descriptor,
        List<Candidate> output,
        ExactNavGraphOrientation orientation,
        IReadOnlyList<int> starts,
        int nativeId,
        IReadOnlyList<int> ends,
        double routeEntryToNativeM,
        double routeNativeToExitM)
    {
        foreach (var start in starts)
        {
            var toNative = FindShortestDirectedPath(descriptor, start, nativeId);
            if (toNative is null) continue;
            foreach (var end in ends)
            {
                var fromNative = FindShortestDirectedPath(descriptor, nativeId, end);
                if (fromNative is null) continue;

                var combined = Combine(toNative, fromNative);
                double vehicleEntryToNative;
                double vehicleNativeToExit;
                if (orientation == ExactNavGraphOrientation.EntryToExit)
                {
                    vehicleEntryToNative = toNative.TotalLengthM;
                    vehicleNativeToExit = fromNative.TotalLengthM;
                }
                else
                {
                    // Directed graph runs exit -> native -> entry. Vehicle travels the reverse.
                    vehicleEntryToNative = fromNative.TotalLengthM;
                    vehicleNativeToExit = toNative.TotalLengthM;
                }
                var score = Math.Abs(vehicleEntryToNative - routeEntryToNativeM) +
                            Math.Abs(vehicleNativeToExit - routeNativeToExitM);
                output.Add(new Candidate(orientation, combined,
                    vehicleEntryToNative, vehicleNativeToExit, score));
            }
        }
    }

    private static DirectedNavGraphPath? FindShortestDirectedPath(PrefabDescriptor descriptor, int start, int target)
    {
        if (start < 0 || target < 0 || start >= descriptor.NavNodes.Count || target >= descriptor.NavNodes.Count)
            return null;
        if (start == target)
            return new DirectedNavGraphPath([start], [], 0.0);

        var queue = new PriorityQueue<int, double>();
        var distance = new Dictionary<int, double> { [start] = 0.0 };
        var parent = new Dictionary<int, (int Source, ExactNavGraphEdge Edge)>();
        queue.Enqueue(start, 0.0);

        while (queue.Count > 0)
        {
            queue.TryDequeue(out var current, out var cost);
            if (!distance.TryGetValue(current, out var known) || cost > known + 1e-6) continue;
            if (current == target) break;

            var node = descriptor.NavNodes[current];
            foreach (var connection in node.Connections)
            {
                var next = (int)connection.TargetNodeIndex;
                if (next < 0 || next >= descriptor.NavNodes.Count) continue;

                var curves = connection.CurveIndices
                    .Select(x => (int)x)
                    .Where(x => x >= 0 && x < descriptor.NavCurves.Count)
                    .ToList();
                var edgeLength = ConnectionLength(descriptor, connection.Length, curves);
                var nextCost = cost + edgeLength;
                if (distance.TryGetValue(next, out var old) && nextCost >= old - 1e-6) continue;

                var edge = new ExactNavGraphEdge(current, next, edgeLength, curves);
                distance[next] = nextCost;
                parent[next] = (current, edge);
                queue.Enqueue(next, nextCost);
            }
        }

        if (!distance.ContainsKey(target)) return null;

        var nodes = new List<int> { target };
        var edges = new List<ExactNavGraphEdge>();
        var cursor = target;
        while (cursor != start)
        {
            if (!parent.TryGetValue(cursor, out var p)) return null;
            edges.Add(p.Edge);
            cursor = p.Source;
            nodes.Add(cursor);
        }
        nodes.Reverse();
        edges.Reverse();
        return new DirectedNavGraphPath(nodes, edges, distance[target]);
    }

    private static double ConnectionLength(PrefabDescriptor descriptor, float declaredLength, IReadOnlyList<int> curves)
    {
        if (float.IsFinite(declaredLength) && declaredLength >= 0 && declaredLength < 1_000_000f)
            return declaredLength;
        return curves.Sum(id => (double)descriptor.NavCurves[id].Length);
    }

    private static DirectedNavGraphPath Combine(DirectedNavGraphPath a, DirectedNavGraphPath b)
    {
        var nodes = a.NodeIds.Concat(b.NodeIds.Skip(1)).ToList();
        var edges = a.Edges.Concat(b.Edges).ToList();
        return new DirectedNavGraphPath(nodes, edges, a.TotalLengthM + b.TotalLengthM);
    }

    private static List<ExactNavGraphEdge> ToVehicleEdges(DirectedNavGraphPath directed, ExactNavGraphOrientation orientation)
    {
        if (orientation == ExactNavGraphOrientation.EntryToExit)
            return directed.Edges.ToList();

        return directed.Edges
            .Reverse()
            .Select(e => new ExactNavGraphEdge(
                e.TargetNodeId,
                e.SourceNodeId,
                e.LengthM,
                e.CurveIds.Reverse().ToList()))
            .ToList();
    }

    private static List<int> FindPhysicalNavNodes(PrefabDescriptor descriptor, int controlNodeIndex)
    {
        if (controlNodeIndex < 0) return [];
        return descriptor.NavNodes
            .Select((node, index) => (node, index))
            .Where(x => x.node.Type == NavNodeType.PhysicalNode && x.node.Index == controlNodeIndex)
            .Select(x => x.index)
            .ToList();
    }

    private static int MapNodeIndexToPpdIndex(Prefab prefab, int mapNodeIndex, int ppdNodeCount)
    {
        if (mapNodeIndex < 0 || mapNodeIndex >= prefab.Nodes.Count || ppdNodeCount <= 0) return -1;
        return (mapNodeIndex + prefab.Origin) % ppdNodeCount;
    }

    private static string FormatEdges(IEnumerable<ExactNavGraphEdge> edges) =>
        string.Join(';', edges.Select(e =>
            $"{e.SourceNodeId}>{e.TargetNodeId}:len={e.LengthM.ToString("0.###", CultureInfo.InvariantCulture)}:curves={string.Join('|', e.CurveIds)}"));

    private static ExactNavGraphResult Empty(
        string ppdPath,
        int mapEntryIndex,
        int mapExitIndex,
        double routeEntryToNativeM,
        double routeNativeToExitM,
        string status,
        string error) =>
        new(false, ppdPath, mapEntryIndex, mapExitIndex, [], [], 0, false, "", null,
            0, 0, ExactNavGraphOrientation.None, false, [], [], [], [], "", "", -1,
            null, [], null, [], 0, 0, 0,
            routeEntryToNativeM, routeNativeToExitM, routeEntryToNativeM + routeNativeToExitM,
            0, 0, 0, false, false, false, false, [], [], 0, false, status, error);

    private static ExactNavGraphResult BaseFailure(
        string ppdPath,
        int ppdEntry,
        int ppdExit,
        IReadOnlyList<int> entryPhysical,
        IReadOnlyList<int> exitPhysical,
        PrefabDescriptor descriptor,
        bool inRange,
        string nativeType,
        int? nativeIndexValue,
        double routeEntryToNativeM,
        double routeNativeToExitM,
        string status,
        string error) =>
        new(true, ppdPath, ppdEntry, ppdExit, entryPhysical, exitPhysical, descriptor.NavNodes.Count,
            inRange, nativeType, nativeIndexValue,
            0, 0, ExactNavGraphOrientation.None, false, [], [], [], [], "", "", -1,
            null, [], null, [], 0, 0, 0,
            routeEntryToNativeM, routeNativeToExitM, routeEntryToNativeM + routeNativeToExitM,
            0, 0, 0, false, false, false, false, [], [], 0, false, status, error);

    private sealed record Candidate(
        ExactNavGraphOrientation Orientation,
        DirectedNavGraphPath Directed,
        double VehicleEntryToNativeM,
        double VehicleNativeToExitM,
        double DistanceErrorScore);
}
