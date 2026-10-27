using System.Globalization;
using System.Numerics;
using TruckLib.Models.Ppd;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal sealed record LaneBranchFeature(
    int LaneOrdinal,
    int StartCurveId,
    int BoundaryLaneId,
    int TargetPpdNodeIndex,
    bool Selected,
    IReadOnlyList<int> CurveIds,
    double LengthM,
    double AngleDeg,
    IReadOnlyList<string> Blinkers,
    IReadOnlyList<string> TrafficRules,
    IReadOnlyList<string> AllowedVehicles,
    bool LowProbability,
    bool LimitDisplacement,
    bool CycleEncountered);

internal sealed record MapPointFeature(
    int Count,
    int PrefabExitCount,
    int NoArrowCount,
    double? EntryNearestDistanceM,
    string EntryNearestRoadSize,
    string EntryNearestRoadOffset,
    bool? EntryNearestPrefabExit,
    bool? EntryNearestArrow,
    double? ExitNearestDistanceM,
    string ExitNearestRoadSize,
    string ExitNearestRoadOffset,
    bool? ExitNearestPrefabExit,
    bool? ExitNearestArrow);

internal sealed record ManeuverFeatureResult(
    bool PpdResolved,
    string PpdPath,
    uint? PpdVersion,
    int MapEntryIndex,
    int MapExitIndex,
    int PpdEntryIndex,
    int PpdExitIndex,
    int InputLaneCount,
    int SelectedLaneCount,
    IReadOnlyList<int> SelectedLaneOrdinals,
    bool SelectedTouchesDividerSide,
    bool SelectedTouchesOuterSide,
    int BranchCount,
    int SelectedBranchCount,
    int ReachableExitCount,
    IReadOnlyList<int> ReachableExitIndices,
    int AlternativeExitCount,
    int OtherEntryNodesToSelectedExit,
    bool AllInputLanesReachSelectedExit,
    double? RepresentativeAngleDeg,
    double? SelectedPhysicalLengthM,
    string SelectedGeometry,
    string SelectedSide,
    IReadOnlyList<int> SelectedCurveIds,
    IReadOnlyList<string> SelectedBlinkers,
    IReadOnlyList<string> SelectedTrafficRules,
    IReadOnlyList<string> SelectedAllowedVehicles,
    bool SelectedHasLeftBlinker,
    bool SelectedHasRightBlinker,
    bool SelectedHasForcedNoBlinker,
    bool AnyCycleEncountered,
    MapPointFeature MapPoints,
    string PreliminaryClass,
    string Confidence,
    IReadOnlyList<LaneBranchFeature> Branches,
    string? Error)
{public string NativePathStatus {get;init;}="PORTS_ONLY";} 

/// <summary>
/// Lane feature extractor. Live routes corroborate their selected path with native node_id.
/// It mirrors the prefab lane model used by the game and public navigation projects:
/// a control node's InputLines are lanes entering the prefab, NextLines describe the
/// directed lane tree, and another control node's OutputLines terminate the path.
/// </summary>
internal sealed class ManeuverFeatureExtractor
{
    private readonly PpdResolver resolver;
    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<TruckLib.IFileSystem,Dictionary<(string,int,int,int,int),ManeuverFeatureResult>> Shared=new();

    public ManeuverFeatureExtractor(PpdResolver resolver)
    {
        this.resolver = resolver;
    }

    public ManeuverFeatureResult Analyze(Prefab prefab, int mapEntryIndex, int mapExitIndex)
    {
        var key=(prefab.Model.ToString(),(int)prefab.Origin,prefab.Nodes.Count,mapEntryIndex,mapExitIndex);
        var cache=Shared.GetValue(resolver.FileSystem,_=>new());
        if(cache.TryGetValue(key,out var feature))return feature;
        var result=AnalyzeFresh(prefab,mapEntryIndex,mapExitIndex);
        if(cache.Count>=4096)cache.Clear();
        return cache[key]=result;
    }
    private readonly System.Runtime.CompilerServices.ConditionalWeakTable<ProbeRoute,Dictionary<int,ManeuverFeatureResult>> routeFeatures=new();
    internal ManeuverFeatureResult AnalyzeRoute(Prefab prefab,int entry,int exit,ProbeRoute route,int segment)
    {
        var cache=routeFeatures.GetValue(route,_=>new());
        if(cache.TryGetValue(segment,out var cached))return cached;
        var feature=Analyze(prefab,entry,exit);
        var rows=new List<LogicalRouteRow>();int index=-1;
        foreach(var row in route.Logical)
        {if(row.Type==0)index++;else if(index==segment&&row.Type==1&&row.ItemUid==prefab.Uid)rows.Add(row);}
        if(rows.Count==0)return cache[segment]=feature;
        var graph=new ExactNavGraphResolver(resolver);
        var exact=rows.Select(row=>graph.Resolve(prefab,entry,exit,row.NodeId,
            route.Physical[segment].TotalDistanceTillEndM-row.TotalDistanceTillEndM,
            row.TotalDistanceTillEndM-route.Physical[segment+1].TotalDistanceTillEndM,null,forwardOnly:true)).ToArray();
        // Require a forward lane path through the captured native node. Native distances
        // rank candidates but include costs that need not equal physical curve length.
        // All native rows within this occurrence must agree on the same curve sequence.
        var valid=exact.All(e=>e.ExactPathFound&&
            e.Orientation==ExactNavGraphOrientation.EntryToExit&&e.ValidationStatus=="VALIDATED_NODE_ID_AS_NAVNODE_INDEX");
        if(!valid)return cache[segment]=feature with{NativePathStatus="NATIVE_GRAPH_NOT_CORROBORATED"};
        var curves=exact[0].VehicleCurveIds;
        if(!exact.All(e=>e.VehicleCurveIds.SequenceEqual(curves))||
            !feature.Branches.Any(b=>b.Selected&&b.CurveIds.SequenceEqual(curves)))
            return cache[segment]=feature with{NativePathStatus="NATIVE_GRAPH_LANE_SEQUENCE_UNCONFIRMED"};
        return cache[segment]=AnalyzeFresh(prefab,entry,exit,curves) with{NativePathStatus="NATIVE_GRAPH_AND_LANE_SEQUENCE_VALIDATED"};
    }
    private ManeuverFeatureResult AnalyzeFresh(Prefab prefab, int mapEntryIndex, int mapExitIndex,IReadOnlyList<int>? preferred=null)
    {
        var token = prefab.Model.ToString();
        if (!resolver.TryResolve(token, out var ppdPath, out var descriptor, out var error) || descriptor is null)
        {
            return Empty(false, ppdPath ?? "", null, mapEntryIndex, mapExitIndex, error ?? "PPD unresolved.");
        }

        var ppdEntry = MapNodeIndexToPpdIndex(prefab, mapEntryIndex, descriptor.Nodes.Count);
        var ppdExit = MapNodeIndexToPpdIndex(prefab, mapExitIndex, descriptor.Nodes.Count);
        if (ppdEntry < 0 || ppdExit < 0 || ppdEntry >= descriptor.Nodes.Count || ppdExit >= descriptor.Nodes.Count)
        {
            return Empty(true, ppdPath, descriptor.Version, mapEntryIndex, mapExitIndex,
                "Map prefab node could not be mapped to a PPD control node.", ppdEntry, ppdExit);
        }

        var endingCurveToNode = BuildEndingCurveMap(descriptor);
        var startCurves = ValidCurveIds(descriptor.Nodes[ppdEntry].InputLines, descriptor.NavCurves.Count);
        var branches = new List<LaneBranchFeature>();
        var anyCycle = false;

        for (var laneOrdinal = 0; laneOrdinal < startCurves.Count; laneOrdinal++)
        {
            var startCurveId = startCurves[laneOrdinal];
            var paths = EnumeratePaths(descriptor, startCurveId, endingCurveToNode, out var laneCycle);
            anyCycle |= laneCycle;

            foreach (var path in paths)
            {
                if (path.CurveIds.Count == 0) continue;
                var first = descriptor.NavCurves[path.CurveIds[0]];
                var length = path.CurveIds.Sum(id => (double)descriptor.NavCurves[id].Length);
                var angle = CalculateNetTurnAngle(descriptor, path.CurveIds);
                var blinkers = path.CurveIds
                    .Select(id => descriptor.NavCurves[id].Blinker.ToString())
                    .Distinct(StringComparer.Ordinal)
                    .ToList();
                var rules = path.CurveIds
                    .Select(id => descriptor.NavCurves[id].TrafficRule.ToString())
                    .Where(s => !string.IsNullOrWhiteSpace(s))
                    .Distinct(StringComparer.Ordinal)
                    .ToList();
                var vehicles = path.CurveIds
                    .Select(id => descriptor.NavCurves[id].AllowedVehicles.ToString())
                    .Distinct(StringComparer.Ordinal)
                    .ToList();
                var lowProbability = path.CurveIds.Any(id => descriptor.NavCurves[id].LowProbability);
                var limitDisplacement = path.CurveIds.Any(id => descriptor.NavCurves[id].LimitDisplacement);

                branches.Add(new LaneBranchFeature(
                    laneOrdinal,
                    startCurveId,
                    first.LeadsToNodes.StartLane,
                    path.TargetNodeIndex,
                    path.TargetNodeIndex == ppdExit,
                    path.CurveIds,
                    length,
                    angle,
                    blinkers,
                    rules,
                    vehicles,
                    lowProbability,
                    limitDisplacement,
                    path.CycleEncountered));
            }
        }

        var selected = branches.Where(b => b.Selected).ToList();
        var selectedLanes = selected.Select(b => b.LaneOrdinal).Distinct().OrderBy(x => x).ToList();
        var reachableExits = branches.Select(b => b.TargetPpdNodeIndex).Distinct().OrderBy(x => x).ToList();
        var allInputLanesReachSelected = startCurves.Count > 0 && selectedLanes.Count == startCurves.Count;
        var selectedAngle = selected.Count == 0 ? (double?)null : Median(selected.Where(b=>preferred is null||b.CurveIds.SequenceEqual(preferred)).Select(b => b.AngleDeg));
        var representative = selected
            .Where(b=>preferred is null||b.CurveIds.SequenceEqual(preferred))
            .OrderBy(b => b.LengthM)
            .ThenBy(b => Math.Abs(b.AngleDeg))
            .FirstOrDefault();

        var selectedCurveIds = representative?.CurveIds ?? [];
        var selectedBlinkers = selected.Where(b=>preferred is null||b.CurveIds.SequenceEqual(preferred)).SelectMany(b => b.Blinkers).Distinct(StringComparer.Ordinal).OrderBy(x => x).ToList();
        var selectedRules = selected.Where(b=>preferred is null||b.CurveIds.SequenceEqual(preferred)).SelectMany(b => b.TrafficRules).Distinct(StringComparer.Ordinal).OrderBy(x => x).ToList();
        var selectedVehicles = selected.Where(b=>preferred is null||b.CurveIds.SequenceEqual(preferred)).SelectMany(b => b.AllowedVehicles).Distinct(StringComparer.Ordinal).OrderBy(x => x).ToList();
        var hasLeftBlinker = selected.Where(b=>preferred is null||b.CurveIds.SequenceEqual(preferred)).Any(b => b.Blinkers.Contains(TruckLib.Models.Ppd.BlinkerType.LeftBlinker.ToString(), StringComparer.Ordinal));
        var hasRightBlinker = selected.Where(b=>preferred is null||b.CurveIds.SequenceEqual(preferred)).Any(b => b.Blinkers.Contains(TruckLib.Models.Ppd.BlinkerType.RightBlinker.ToString(), StringComparer.Ordinal));
        var hasForcedNoBlinker = selected.Where(b=>preferred is null||b.CurveIds.SequenceEqual(preferred)).Any(b => b.Blinkers.Contains(TruckLib.Models.Ppd.BlinkerType.NoBlinkerForced.ToString(), StringComparer.Ordinal));

        var otherEntriesToSelected = 0;
        for (var i = 0; i < descriptor.Nodes.Count; i++)
        {
            if (i == ppdEntry) continue;
            if (EntryCanReachExit(descriptor, i, ppdExit, endingCurveToNode))
                otherEntriesToSelected++;
        }

        var mapPoints = AnalyzeMapPoints(descriptor, ppdEntry, ppdExit);
        var geometry = selectedAngle.HasValue ? GeometryClass(selectedAngle.Value) : "NO_SELECTED_PATH";
        var side = selectedAngle.HasValue ? SideFromAngle(selectedAngle.Value) : "";
        var classification = Classify(
            geometry,
            side,
            reachableExits.Count,
            allInputLanesReachSelected,
            startCurves.Count,
            selectedLanes.Count,
            otherEntriesToSelected,
            descriptor.Nodes.Count,
            selected.Count,
            anyCycle);
        if(classification.StartsWith("MERGE_",StringComparison.Ordinal))
        {
            var selectedEnds=selected.Select(b=>descriptor.NavCurves[b.CurveIds[^1]].EndPosition).Distinct().ToArray();
            var otherEnds=new List<Vector3>();
            for(int entry=0;entry<descriptor.Nodes.Count;entry++)
            {
                if(entry==ppdEntry)continue;
                foreach(var curve in ValidCurveIds(descriptor.Nodes[entry].InputLines,descriptor.NavCurves.Count))
                foreach(var p in EnumeratePaths(descriptor,curve,endingCurveToNode,out _).Where(p=>p.TargetNodeIndex==ppdExit))
                    otherEnds.Add(descriptor.NavCurves[p.CurveIds[^1]].EndPosition);
            }
            var end=representative is null?null:descriptor.NavCurves[representative.CurveIds[^1]];
            var mergeSide=end is null?"":MergeSideFromEndpoints(selectedEnds,otherEnds.Distinct().ToArray(),
                -Vector3.Transform(Vector3.UnitZ,end.EndRotation));
            classification=mergeSide==""?"MERGE":"MERGE_"+mergeSide;
        }
        var confidence = ConfidenceFor(classification, selected.Count, startCurves.Count, reachableExits.Count);

        return new ManeuverFeatureResult(
            true,
            ppdPath,
            descriptor.Version,
            mapEntryIndex,
            mapExitIndex,
            ppdEntry,
            ppdExit,
            startCurves.Count,
            selectedLanes.Count,
            selectedLanes,
            selectedLanes.Contains(0),
            startCurves.Count > 0 && selectedLanes.Contains(startCurves.Count - 1),
            branches.Count,
            selected.Count,
            reachableExits.Count,
            reachableExits,
            Math.Max(0, reachableExits.Count - (reachableExits.Contains(ppdExit) ? 1 : 0)),
            otherEntriesToSelected,
            allInputLanesReachSelected,
            selectedAngle,
            representative?.LengthM,
            geometry,
            side,
            selectedCurveIds,
            selectedBlinkers,
            selectedRules,
            selectedVehicles,
            hasLeftBlinker,
            hasRightBlinker,
            hasForcedNoBlinker,
            anyCycle || branches.Any(b => b.CycleEncountered),
            mapPoints,
            classification,
            confidence,
            branches,
            selected.Count == 0 ? "No directed lane branch from PPD entry to selected PPD exit." : null);
    }

    internal static string MergeSideFromEndpoints(IReadOnlyList<Vector3> joining,IReadOnlyList<Vector3> other,Vector3 exitDirection)
    {
        if(joining.Count==0||other.Count==0||joining.Concat(other).Any(p=>!RoadLaneGeometryResolver.Finite(p))||
            !RoadLaneGeometryResolver.Finite(exitDirection))return "";
        var d=new Vector2(exitDirection.X,exitDirection.Z);if(d.LengthSquared()<1e-8)return "";d=Vector2.Normalize(d);
        // Looking along travel in X/Z, right is (-Z,+X). A ramp on the right joins toward the left.
        var right=new Vector2(-d.Y,d.X);
        double Coordinate(Vector3 p)=>Vector2.Dot(new(p.X,p.Z),right);
        var a=joining.Select(Coordinate).ToArray();var b=other.Select(Coordinate).ToArray();
        if(a.Min()>b.Max()+0.5)return "LEFT";
        if(a.Max()<b.Min()-0.5)return "RIGHT";
        return ""; // Overlapping terminal lanes do not establish a sided insertion.
    }

    private static ManeuverFeatureResult Empty(
        bool resolved,
        string path,
        uint? version,
        int mapEntry,
        int mapExit,
        string? error,
        int ppdEntry = -1,
        int ppdExit = -1)
    {
        return new ManeuverFeatureResult(
            resolved, path, version, mapEntry, mapExit, ppdEntry, ppdExit,
            0, 0, [], false, false, 0, 0, 0, [], 0, 0, false,
            null, null, "NO_SELECTED_PATH", "", [], [], [], [], false, false, false, false,
            new MapPointFeature(0, 0, 0, null, "", "", null, null, null, "", "", null, null),
            "UNRESOLVED", "NONE", [], error);
    }

    private sealed record RawPath(int TargetNodeIndex, IReadOnlyList<int> CurveIds, bool CycleEncountered);

    private static List<RawPath> EnumeratePaths(
        PrefabDescriptor descriptor,
        int startCurveId,
        IReadOnlyDictionary<int, int> endingCurveToNode,
        out bool anyCycle)
    {
        var result = new List<RawPath>();
        var cycleDetected = false;
        var stack = new List<int>();
        var onStack = new HashSet<int>();
        const int pathLimit = 1024;
        bool bounded=false;

        void Dfs(int curveId)
        {
            if(result.Count>=pathLimit||stack.Count>=512){bounded=true;return;}
            if (curveId < 0 || curveId >= descriptor.NavCurves.Count) return;
            if (!onStack.Add(curveId))
            {
                cycleDetected = true;
                return;
            }

            stack.Add(curveId);
            if (endingCurveToNode.TryGetValue(curveId, out var targetNode))
            {
                result.Add(new RawPath(targetNode, stack.ToArray(), false));
            }
            else
            {
                var nextIds = descriptor.NavCurves[curveId].NextLines
                    .Where(id => id >= 0 && id < descriptor.NavCurves.Count)
                    .Distinct()
                    .ToList();
                if (nextIds.Count == 0)
                {
                    // Dead-end navigation curves are retained only as diagnostics; they do
                    // not constitute a branch to a control node.
                }
                foreach (var next in nextIds)
                {
                    if (onStack.Contains(next))
                    {
                        cycleDetected = true;
                        continue;
                    }
                    Dfs(next);
                }
            }

            stack.RemoveAt(stack.Count - 1);
            onStack.Remove(curveId);
        }

        Dfs(startCurveId);
        if(bounded)
        {
            // Preserve every reachable output instead of silently retaining the
            // first 1024 routes. Positive-length shortest paths avoid cycle explosion.
            result.Clear();var distance=new Dictionary<int,double>{{startCurveId,0}};
            var predecessor=new Dictionary<int,int>();var queue=new PriorityQueue<int,double>();queue.Enqueue(startCurveId,0);
            while(queue.TryDequeue(out var current,out var cost))
            {
                if(cost>distance[current])continue;
                if(endingCurveToNode.TryGetValue(current,out var target))
                {
                    var path=new List<int>{current};while(predecessor.TryGetValue(path[^1],out var previous))path.Add(previous);
                    path.Reverse();result.Add(new RawPath(target,path.ToArray(),false));continue;
                }
                foreach(var next in descriptor.NavCurves[current].NextLines.Where(id=>id>=0&&id<descriptor.NavCurves.Count).Distinct())
                {
                    var length=descriptor.NavCurves[next].Length;if(!float.IsFinite(length)||length<0)continue;
                    var nextCost=cost+Math.Max(0.00001,length);
                    if(distance.TryGetValue(next,out var known)&&known<=nextCost)continue;
                    distance[next]=nextCost;predecessor[next]=current;queue.Enqueue(next,nextCost);
                }
            }
        }
        anyCycle = cycleDetected;
        return result
            .GroupBy(p => p.TargetNodeIndex + ":" + string.Join('-', p.CurveIds))
            .Select(g => g.First())
            .ToList();
    }

    private static bool EntryCanReachExit(
        PrefabDescriptor descriptor,
        int entryNodeIndex,
        int exitNodeIndex,
        IReadOnlyDictionary<int, int> endingCurveToNode)
    {
        var starts = ValidCurveIds(descriptor.Nodes[entryNodeIndex].InputLines, descriptor.NavCurves.Count);
        foreach (var start in starts)
        {
            var paths = EnumeratePaths(descriptor, start, endingCurveToNode, out _);
            if (paths.Any(p => p.TargetNodeIndex == exitNodeIndex)) return true;
        }
        return false;
    }

    private static Dictionary<int, int> BuildEndingCurveMap(PrefabDescriptor descriptor)
    {
        var result = new Dictionary<int, int>();
        for (var nodeIndex = 0; nodeIndex < descriptor.Nodes.Count; nodeIndex++)
        {
            foreach (var output in ValidCurveIds(descriptor.Nodes[nodeIndex].OutputLines, descriptor.NavCurves.Count))
                result[output] = nodeIndex;
        }
        return result;
    }

    private static List<int> ValidCurveIds(IEnumerable<int> values, int curveCount) =>
        values.Where(v => v >= 0 && v < curveCount).Distinct().ToList();

    private static int MapNodeIndexToPpdIndex(Prefab prefab, int mapNodeIndex, int ppdNodeCount)
    {
        if (mapNodeIndex < 0 || mapNodeIndex >= prefab.Nodes.Count || ppdNodeCount <= 0) return -1;
        return (mapNodeIndex + prefab.Origin) % ppdNodeCount;
    }

    // In ATS prefab X/Z space, the standard signed 2D cross gives the same
    // convention used by truckermudgeon: positive = clockwise/right on the map,
    // negative = counter-clockwise/left.
    private static double CalculateNetTurnAngle(PrefabDescriptor descriptor, IReadOnlyList<int> path)
    {
        if (path.Count == 0) return 0;
        var first = descriptor.NavCurves[path[0]];
        var last = descriptor.NavCurves[path[^1]];
        var startVec = first.EndPosition - first.StartPosition;
        var endVec = last.EndPosition - last.StartPosition;
        var a = new Vector2(startVec.X, startVec.Z);
        var b = new Vector2(endVec.X, endVec.Z);
        if (a.LengthSquared() < 1e-8f || b.LengthSquared() < 1e-8f) return 0;
        a = Vector2.Normalize(a);
        b = Vector2.Normalize(b);
        var dot = Math.Clamp(Vector2.Dot(a, b), -1f, 1f);
        var signedCross = a.X * b.Y - a.Y * b.X;
        return Math.Atan2(signedCross, dot) * (180.0 / Math.PI);
    }

    private static string GeometryClass(double angleDeg)
    {
        var abs = Math.Abs(angleDeg);
        if (abs <= 2) return "THROUGH";
        if (abs <= 70) return "SLIGHT";
        if (abs <= 110) return "TURN";
        if (abs <= 160) return "SHARP";
        return "U_TURN";
    }

    private static string SideFromAngle(double angleDeg)
    {
        if (Math.Abs(angleDeg) <= 2) return "STRAIGHT";
        return angleDeg > 0 ? "RIGHT" : "LEFT";
    }

    private static string Classify(
        string geometry,
        string side,
        int reachableExitCount,
        bool allLanesReachSelected,
        int inputLaneCount,
        int selectedLaneCount,
        int otherEntriesToSelected,
        int controlNodeCount,
        int selectedBranchCount,
        bool cycleEncountered)
    {
        if (selectedBranchCount == 0) return cycleEncountered ? "CYCLE_UNRESOLVED" : "NO_SELECTED_PATH";

        // Merely having a reachable cycle is not enough to call a prefab a roundabout:
        // many junctions expose U-turn/loop AI curves. Keep cycle presence as a feature
        // and classify the actually selected branch normally.

        if (geometry is "TURN" or "SHARP" or "U_TURN")
            return geometry + "_" + side;

        var merge = geometry == "SLIGHT" && controlNodeCount == 3 && otherEntriesToSelected == 1;
        if (merge) return "MERGE_" + side;

        if (geometry == "THROUGH")
        {
            // Same criterion as modern public route-step builders: if every entry lane
            // can take the selected straight-through branch, there is no useful instruction.
            if (allLanesReachSelected) return "TRANSPARENT";
            if (reachableExitCount > 1 || selectedLaneCount < inputLaneCount) return "FORK_STRAIGHT";
            return "TRANSPARENT";
        }

        // A shallow bend with no alternative exit is road geometry, not a driver decision.
        if (geometry == "SLIGHT" && reachableExitCount <= 1)
            return "TRANSPARENT";

        if (geometry == "SLIGHT" && reachableExitCount > 1)
            return "BRANCH_" + side;

        return "UNCLASSIFIED";
    }

    private static string ConfidenceFor(string classification, int selectedBranchCount, int laneCount, int reachableExits)
    {
        if (selectedBranchCount == 0) return "NONE";
        if(classification=="MERGE")return "MEDIUM";
        if (classification.StartsWith("TURN_", StringComparison.Ordinal) ||
            classification.StartsWith("SHARP_", StringComparison.Ordinal) ||
            classification.StartsWith("U_TURN_", StringComparison.Ordinal) ||
            classification.StartsWith("MERGE_", StringComparison.Ordinal))
            return "HIGH";
        if (classification == "TRANSPARENT" && laneCount > 0) return "HIGH";
        if (classification.StartsWith("BRANCH_", StringComparison.Ordinal) || classification == "FORK_STRAIGHT")
            return reachableExits > 1 ? "MEDIUM" : "LOW";
        return "LOW";
    }

    private static MapPointFeature AnalyzeMapPoints(PrefabDescriptor descriptor, int ppdEntry, int ppdExit)
    {
        if (descriptor.MapPoints.Count == 0)
            return new MapPointFeature(0, 0, 0, null, "", "", null, null, null, "", "", null, null);

        static (MapPoint Point, double Distance)? Nearest(PrefabDescriptor d, int nodeIndex)
        {
            if (nodeIndex < 0 || nodeIndex >= d.Nodes.Count || d.MapPoints.Count == 0) return null;
            var p = d.Nodes[nodeIndex].Position;
            MapPoint? best = null;
            var bestSq = double.MaxValue;
            foreach (var mp in d.MapPoints)
            {
                var delta = mp.Position - p;
                var sq = (double)delta.LengthSquared();
                if (sq >= bestSq) continue;
                bestSq = sq;
                best = mp;
            }
            return best is null ? null : (best, Math.Sqrt(bestSq));
        }

        var entry = Nearest(descriptor, ppdEntry);
        var exit = Nearest(descriptor, ppdExit);
        return new MapPointFeature(
            descriptor.MapPoints.Count,
            descriptor.MapPoints.Count(p => p.PrefabExit),
            descriptor.MapPoints.Count(p => !p.Arrow),
            entry?.Distance,
            entry?.Point.RoadSize.ToString() ?? "",
            entry?.Point.RoadOffset.ToString() ?? "",
            entry?.Point.PrefabExit,
            entry?.Point.Arrow,
            exit?.Distance,
            exit?.Point.RoadSize.ToString() ?? "",
            exit?.Point.RoadOffset.ToString() ?? "",
            exit?.Point.PrefabExit,
            exit?.Point.Arrow);
    }

    private static double Median(IEnumerable<double> values)
    {
        var ordered = values.OrderBy(x => x).ToArray();
        if (ordered.Length == 0) return 0;
        var mid = ordered.Length / 2;
        return ordered.Length % 2 == 1 ? ordered[mid] : (ordered[mid - 1] + ordered[mid]) / 2.0;
    }
}
