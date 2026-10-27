using System.Globalization;
using System.Numerics;
using TruckLib;
using TruckLib.Models.Ppd;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal sealed record LaneBranchMapping(int InputLaneOrdinal, int OutputLaneOrdinal);
internal sealed record LanePathGeometry(int InputLaneOrdinal,int OutputLaneOrdinal,int BoundaryLaneId,
    IReadOnlyList<int> CurveIds,IReadOnlyList<Vector3> Points);

internal sealed record DecisionBranchGeometry(
    string BranchId,
    int TargetPpdNodeIndex,
    bool Selected,
    IReadOnlyList<int> LaneOrdinals,
    IReadOnlyList<int> CurveIds,
    IReadOnlyList<LaneBranchMapping> LaneMappings,
    double RepresentativeAngleDeg,
    string Side,
    IReadOnlyList<IReadOnlyList<Vector3>> LanePolylines,
    ulong TargetMapNodeUid,
    ulong DownstreamItemUid,
    string DownstreamItemType)
{
    public IReadOnlyList<LanePathGeometry> ExactLanePaths { get; init; } = [];
    public double TransformMaxErrorM {get;init;}
    public IReadOnlyList<int> NativeLanePolylineIndices {get;init;}=[];
}

internal sealed record DecisionBranchSet(
    int EventIndex,
    int SegmentIndex,
    ulong PrefabUid,
    string PrefabModel,
    int PpdEntryIndex,
    int PpdSelectedExitIndex,
    int InputLaneCount,
    IReadOnlyList<int> SelectedLaneOrdinals,
    bool AllInputLanesReachSelectedExit,
    IReadOnlyList<DecisionBranchGeometry> Branches,
    string SelectedBranchId,
    string Status,
    string? Error);

internal sealed record BranchProjection(
    string BranchId,
    double AlongM,
    double LateralM,
    double HeadingDeg,
    double OrientationPerpendicularErrorDeg,
    int PolylineIndex,
    int EdgeIndex);

/// <summary>
/// Converts all PPD lane branches of a selected route prefab into world-space branch
/// corridors. Unlike the selected RouteCorridor, this includes alternatives ATS did not
/// choose, which allows v0.11 to prove that a board matches the selected branch better
/// than competing branches.
/// </summary>
internal sealed class DecisionBranchResolver
{
    private readonly Map map;
    private readonly PpdResolver ppdResolver;
    private readonly ManeuverFeatureExtractor featureExtractor;

    public DecisionBranchResolver(Map map, PpdResolver ppdResolver, ManeuverFeatureExtractor featureExtractor)
    {
        this.map = map;
        this.ppdResolver = ppdResolver;
        this.featureExtractor = featureExtractor;
    }

    public DecisionBranchSet Resolve(ProbeRoute route, V09ManeuverEvent ev)
    {
        var seg = ev.SegmentIndex;
        if (seg < 0 || seg + 1 >= route.Physical.Count)
            return Empty(ev, 0, "BAD_ROUTE_SEGMENT", "Event segment is outside physical route bounds.");
        if (!map.Nodes.TryGetValue(route.Physical[seg].NodeUid, out var a) ||
            !map.Nodes.TryGetValue(route.Physical[seg + 1].NodeUid, out var b))
            return Empty(ev, 0, "ROUTE_NODE_MISSING", "Route entry/exit node missing from map.");

        var common = RouteMapItemSelector.Resolve(map,a,b,route,seg);
        if (common is not Prefab prefab)
            return Empty(ev, common?.Uid ?? 0, "NOT_PREFAB", "Maneuver event segment is not a prefab.");

        var mapEntry = FindNodeIndex(prefab, a.Uid);
        var mapExit = FindNodeIndex(prefab, b.Uid);
        return ResolvePorts(prefab,mapEntry,mapExit,ev,route:route);
    }

    // Network extraction has no selected GPS exit. Enumerate every directed entry independently.
    public DecisionBranchSet ResolveEntry(Prefab prefab,int mapEntry)
        =>ResolvePorts(prefab,mapEntry,mapEntry,new(-1,-1,"","",0,0,"",false,"LOW"),true);

    private DecisionBranchSet ResolvePorts(Prefab prefab,int mapEntry,int mapExit,V09ManeuverEvent ev,bool network=false,ProbeRoute? route=null)
    {
        var feature = route is null?featureExtractor.Analyze(prefab, mapEntry, mapExit):featureExtractor.AnalyzeRoute(prefab,mapEntry,mapExit,route,ev.SegmentIndex);
        if (!feature.PpdResolved || (network?feature.Branches.Count==0:feature.SelectedBranchCount==0))
            return Empty(ev, prefab.Uid, "PPD_UNRESOLVED", feature.Error ?? "No selected PPD branch.", prefab.Model.ToString(), feature.PpdEntryIndex, feature.PpdExitIndex, feature.InputLaneCount, feature.SelectedLaneOrdinals, feature.AllInputLanesReachSelectedExit);

        if (!ppdResolver.TryResolve(prefab.Model.ToString(), out _, out var descriptor, out var error) || descriptor is null)
            return Empty(ev, prefab.Uid, "PPD_UNRESOLVED", error ?? "PPD descriptor unavailable.", prefab.Model.ToString(), feature.PpdEntryIndex, feature.PpdExitIndex, feature.InputLaneCount, feature.SelectedLaneOrdinals, feature.AllInputLanesReachSelectedExit);
        if (!TryBuildPrefabTransform(prefab, descriptor, out var transform))
            return Empty(ev, prefab.Uid, "TRANSFORM_FAILED", "Could not derive prefab local->world transform.", prefab.Model.ToString(), feature.PpdEntryIndex, feature.PpdExitIndex, feature.InputLaneCount, feature.SelectedLaneOrdinals, feature.AllInputLanesReachSelectedExit);

        var branches = new List<DecisionBranchGeometry>();
        foreach (var group in feature.Branches.GroupBy(x => x.TargetPpdNodeIndex).OrderBy(g => g.Key))
        {
            var lanePaths = new List<IReadOnlyList<Vector3>>();
            var curveUnion = new HashSet<int>();
            var exactPaths=new List<LanePathGeometry>();
            var nativeIndices=new List<int>();
            foreach (var branch in group.OrderBy(x => x.LaneOrdinal).ThenBy(x => x.LengthM))
            {
                var points = BuildBranchPolyline(descriptor, transform, branch.CurveIds);
                if (points.Count < 2) continue;
                var outputs=descriptor.Nodes[group.Key].OutputLines.Where(id=>id>=0 && id<descriptor.NavCurves.Count).ToList();
                exactPaths.Add(new(branch.LaneOrdinal,outputs.IndexOf(branch.CurveIds[^1]),branch.BoundaryLaneId,
                    branch.CurveIds.ToArray(),points.ToArray()));
                AppendDownstream(prefab, descriptor, branch.TargetPpdNodeIndex, points);
                if(branch.Selected&&feature.NativePathStatus=="NATIVE_GRAPH_AND_LANE_SEQUENCE_VALIDATED"&&branch.CurveIds.SequenceEqual(feature.SelectedCurveIds))
                    nativeIndices.Add(lanePaths.Count);
                lanePaths.Add(points);
                foreach (var curveId in branch.CurveIds) curveUnion.Add(curveId);
            }
            if (lanePaths.Count == 0) continue;

            var targetMapNode = GetMapNodeForPpdNode(prefab, descriptor, group.Key);
            var downstream = targetMapNode is null ? null : ExternalItem(targetMapNode, prefab.Uid);
            var angles = group.Select(x => x.AngleDeg).OrderBy(x => x).ToArray();
            var angle = !network&&group.Key==feature.PpdExitIndex&&feature.NativePathStatus=="NATIVE_GRAPH_AND_LANE_SEQUENCE_VALIDATED"?feature.RepresentativeAngleDeg!.Value:Median(angles);
            var lanes = group.Select(x => x.LaneOrdinal).Distinct().OrderBy(x => x).ToList();
            // Lane ordinals in ManeuverFeatureExtractor are compact ordinals over
            // valid control-node lane IDs (the PPD arrays are padded with -1). Use
            // the same compact convention for output lanes so future exact
            // cross-item lane mapping cannot be shifted by padding slots.
            var targetOutputs = descriptor.Nodes[group.Key].OutputLines
                .Where(id => id >= 0 && id < descriptor.NavCurves.Count)
                .ToList();
            var mappings = group
                .Where(x => x.CurveIds.Count > 0)
                .Select(x => new LaneBranchMapping(x.LaneOrdinal, targetOutputs.IndexOf(x.CurveIds[^1])))
                .Where(x => x.OutputLaneOrdinal >= 0)
                .Distinct()
                .OrderBy(x => x.InputLaneOrdinal)
                .ThenBy(x => x.OutputLaneOrdinal)
                .ToList();
            var id = $"{prefab.Uid:X16}:{feature.PpdEntryIndex}->{group.Key}";
            branches.Add(new DecisionBranchGeometry(
                id,
                group.Key,
                !network && group.Key == feature.PpdExitIndex,
                lanes,
                curveUnion.OrderBy(x => x).ToList(),
                mappings,
                angle,
                SideFromAngle(angle),
                lanePaths,
                targetMapNode?.Uid ?? 0,
                downstream?.Uid ?? 0,
                downstream?.ItemType.ToString() ?? ""){ExactLanePaths=exactPaths,TransformMaxErrorM=transform.MaxErrorM,NativeLanePolylineIndices=nativeIndices});
        }

        var selected = branches.Where(x => x.Selected).ToList();
        var status = network?"NETWORK_ENTRY":selected.Count == 1 ? "OK" : selected.Count == 0 ? "NO_SELECTED_BRANCH" : "MULTIPLE_SELECTED_BRANCHES";
        return new DecisionBranchSet(
            ev.EventIndex,
            ev.SegmentIndex,
            prefab.Uid,
            prefab.Model.ToString(),
            feature.PpdEntryIndex,
            feature.PpdExitIndex,
            feature.InputLaneCount,
            feature.SelectedLaneOrdinals,
            feature.AllInputLanesReachSelectedExit,
            branches,
            selected.Count == 1 ? selected[0].BranchId : "",
            status,
            status == "OK" ? null : status);
    }

    public static BranchProjection? Project(ParsedSign sign, DecisionBranchGeometry branch)
        => Project(sign.Position, sign.YawDeg, branch);

    public static BranchProjection? Project(Vector3 position, double yawDeg, DecisionBranchGeometry branch)
    {
        BranchProjection? best = null;
        for (var pi = 0; pi < branch.LanePolylines.Count; pi++)
        {
            if(branch.NativeLanePolylineIndices.Count>0&&!branch.NativeLanePolylineIndices.Contains(pi))continue;
            var p = branch.LanePolylines[pi];
            double accumulated = 0;
            for (var i = 0; i + 1 < p.Count; i++)
            {
                var a = new Vector2(p[i].X, p[i].Z);
                var b = new Vector2(p[i + 1].X, p[i + 1].Z);
                var x = new Vector2(position.X, position.Z);
                var ab = b - a;
                var len = ab.Length();
                if (len < 1e-6f) continue;
                var t = Math.Clamp(Vector2.Dot(x - a, ab) / (len * len), 0f, 1f);
                var q = a + ab * t;
                var lateral = Vector2.Distance(x, q);
                var along = accumulated + len * t;
                var heading = Math.Atan2(ab.X, ab.Y) * 180.0 / Math.PI;
                var orient = PerpendicularOrientationError(yawDeg, heading);
                var candidate = new BranchProjection(branch.BranchId, along, lateral, heading, orient, pi, i);
                if (best is null || candidate.LateralM < best.LateralM - 1e-6 ||
                    (Math.Abs(candidate.LateralM - best.LateralM) <= 1e-6 && candidate.OrientationPerpendicularErrorDeg < best.OrientationPerpendicularErrorDeg))
                    best = candidate;
                accumulated += len;
            }
        }
        return best;
    }

    private List<Vector3> BuildBranchPolyline(PrefabDescriptor descriptor, PrefabRigidTransform transform, IReadOnlyList<int> curveIds)
    {
        var result = new List<Vector3>();
        foreach (var id in curveIds)
        {
            if (id < 0 || id >= descriptor.NavCurves.Count) continue;
            var c = descriptor.NavCurves[id];
            foreach (var p in SampleNativeCurve(c.StartPosition, c.StartRotation, c.EndPosition, c.EndRotation))
            {
                var world = transform.Transform(p);
                if (result.Count == 0 || DistanceXZ(result[^1], world) > 0.02) result.Add(world);
            }
        }
        return result;
    }

    internal static List<Vector3> SampleNativeCurve(Vector3 start,Quaternion startRotation,Vector3 end,Quaternion endRotation)
    {
        var a=new Node{Position=start,Rotation=startRotation};var b=new Node{Position=end,Rotation=endRotation};
        var steps=Math.Clamp((int)Math.Ceiling(Vector3.Distance(start,end)/5),2,2000);
        return Enumerable.Range(0,steps+1).Select(i=>HermiteSpline.InterpolatePolyline(a,b,i/(float)steps)).ToList();
    }

    private void AppendDownstream(Prefab prefab, PrefabDescriptor descriptor, int targetPpdNodeIndex, List<Vector3> points)
    {
        var target = GetMapNodeForPpdNode(prefab, descriptor, targetPpdNodeIndex);
        if (target is null) return;
        var item = ExternalItem(target, prefab.Uid);
        if (item is Road road)
        {
            var other = road.Node.Uid == target.Uid ? road.ForwardNode : road.ForwardNode.Uid == target.Uid ? road.Node : null;
            if (other is null) return;
            var roadPoints = BuildRoadPoints(road, target, other);
            foreach (var p in roadPoints)
                if (points.Count == 0 || DistanceXZ(points[^1], p) > 0.02) points.Add(p);
        }
        else if (item is Prefab nextPrefab)
        {
            // One control-node chord is deliberately used only as a short downstream
            // discriminator. We do not invent a path through the next prefab because ATS
            // has not selected a branch for the alternative route.
            var candidate = nextPrefab.Nodes.Where(n => n.Uid != target.Uid).OrderBy(n => DistanceXZ(n.Position, target.Position)).FirstOrDefault();
            if (candidate is not null && (points.Count == 0 || DistanceXZ(points[^1], candidate.Position) > 0.02)) points.Add(candidate.Position);
        }
    }

    private static INode? GetMapNodeForPpdNode(Prefab prefab, PrefabDescriptor descriptor, int ppdIndex)
    {
        if (descriptor.Nodes.Count == 0 || prefab.Nodes.Count == 0 || ppdIndex < 0 || ppdIndex >= descriptor.Nodes.Count) return null;
        var mapIndex = ppdIndex - (int)prefab.Origin;
        while (mapIndex < 0) mapIndex += descriptor.Nodes.Count;
        mapIndex %= descriptor.Nodes.Count;
        return mapIndex >= 0 && mapIndex < prefab.Nodes.Count ? prefab.Nodes[mapIndex] : null;
    }

    private static MapItem? ExternalItem(INode node, ulong prefabUid)
    {
        if (node.ForwardItem is MapItem f && f.Uid != prefabUid) return f;
        if (node.BackwardItem is MapItem b && b.Uid != prefabUid) return b;
        return null;
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

    private static List<Vector3> BuildRoadPoints(Road road, INode routeStart, INode routeEnd)
    {
        var steps=Math.Clamp((int)Math.Ceiling(road.Length/5f),2,4000);
        var points=Enumerable.Range(0,steps+1).Select(i=>road.InterpolateCurve(i/(float)steps).Position).ToList();
        if (road.Node.Uid == routeStart.Uid && road.ForwardNode.Uid == routeEnd.Uid) return points;
        if (road.ForwardNode.Uid == routeStart.Uid && road.Node.Uid == routeEnd.Uid) { points.Reverse(); return points; }
        return [routeStart.Position, routeEnd.Position];
    }

    private static List<Vector3> SampleHermite(Vector3 start, Quaternion startRotation, Vector3 end, Quaternion endRotation)
    {
        var chord = Vector3.Distance(start, end);
        if (chord < 1e-5f) return [start, end];
        var startDir = Vector3.Transform(Vector3.UnitZ, startRotation);
        var endDir = Vector3.Transform(Vector3.UnitZ, endRotation);
        if (startDir.LengthSquared() < 1e-8f) startDir = Vector3.Normalize(end - start); else startDir = Vector3.Normalize(startDir);
        if (endDir.LengthSquared() < 1e-8f) endDir = Vector3.Normalize(end - start); else endDir = Vector3.Normalize(endDir);
        return SampleHermite(start, startDir * chord, end, endDir * chord, 8);
    }

    private static List<Vector3> SampleHermite(Vector3 p0, Vector3 m0, Vector3 p1, Vector3 m1, int steps)
    {
        var result = new List<Vector3>(steps + 1);
        for (var i = 0; i <= steps; i++)
        {
            var t = i / (double)steps;
            var t2 = t * t; var t3 = t2 * t;
            var h00 = 2 * t3 - 3 * t2 + 1;
            var h10 = t3 - 2 * t2 + t;
            var h01 = -2 * t3 + 3 * t2;
            var h11 = t3 - t2;
            result.Add(p0 * (float)h00 + m0 * (float)h10 + p1 * (float)h01 + m1 * (float)h11);
        }
        return result;
    }

    private static bool TryBuildPrefabTransform(Prefab prefab, PrefabDescriptor descriptor, out PrefabRigidTransform transform)
    {
        transform = default;
        if (prefab.Nodes.Count == 0 || descriptor.Nodes.Count == 0) return false;
        var origin = (int)prefab.Origin;
        if (origin < 0 || origin >= descriptor.Nodes.Count) return false;
        var map0 = prefab.Nodes[0].Position;
        var local0 = descriptor.Nodes[origin].Position;
        var descriptorRotation=MathEx.GetNodeRotation(descriptor.Nodes[origin].Direction);
        var rotation=Quaternion.Normalize(prefab.Nodes[0].Rotation*Quaternion.Conjugate(descriptorRotation));
        transform=new(local0,map0,rotation,0);
        double error=0;
        for(int i=0;i<prefab.Nodes.Count;i++)error=Math.Max(error,Vector3.Distance(
            transform.Transform(descriptor.Nodes[(i+origin)%descriptor.Nodes.Count].Position),prefab.Nodes[i].Position));
        transform=transform with{MaxErrorM=error};
        return double.IsFinite(error);
    }

    private readonly record struct PrefabRigidTransform(Vector3 LocalBase,Vector3 WorldBase,Quaternion Rotation,double MaxErrorM)
    {
        public Vector3 Transform(Vector3 local)
        {
            return WorldBase+Vector3.Transform(local-LocalBase,Rotation);
        }
    }

    private static double Median(IReadOnlyList<double> values)
    {
        if (values.Count == 0) return 0;
        var mid = values.Count / 2;
        return values.Count % 2 == 1 ? values[mid] : (values[mid - 1] + values[mid]) * 0.5;
    }

    private static string SideFromAngle(double angle)
    {
        if (Math.Abs(angle) <= 2) return "STRAIGHT";
        return angle > 0 ? "RIGHT" : "LEFT";
    }

    private static double PerpendicularOrientationError(double signYawDeg, double routeHeadingDeg)
    {
        var delta = Math.Abs(NormalizeDeg(signYawDeg - routeHeadingDeg));
        return Math.Abs(delta - 90.0);
    }

    private static double NormalizeDeg(double deg)
    {
        deg %= 360.0;
        if (deg > 180) deg -= 360;
        if (deg < -180) deg += 360;
        return deg;
    }

    private static double DistanceXZ(Vector3 a, Vector3 b)
    {
        var dx = (double)a.X - b.X;
        var dz = (double)a.Z - b.Z;
        return Math.Sqrt(dx * dx + dz * dz);
    }

    private static DecisionBranchSet Empty(
        V09ManeuverEvent ev,
        ulong prefabUid,
        string status,
        string error,
        string prefabModel = "",
        int ppdEntry = -1,
        int ppdExit = -1,
        int laneCount = 0,
        IReadOnlyList<int>? selectedLanes = null,
        bool allLanes = false)
        => new(ev.EventIndex, ev.SegmentIndex, prefabUid, prefabModel, ppdEntry, ppdExit, laneCount, selectedLanes ?? [], allLanes, [], "", status, error);
}
