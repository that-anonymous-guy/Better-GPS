using System.Globalization;
using System.Numerics;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal sealed record V09ManeuverEvent(
    int EventIndex,
    int SegmentIndex,
    string PreliminaryClass,
    string Instruction,
    double DecisionStationM,
    double DecisionNavStationM,
    string ExitNumber,
    bool ExitAmbiguous,
    string Confidence);

internal sealed record V09RouteEvidence(
    int EvidenceIndex,
    string EvidenceType,
    string Identity,
    string Cardinal,
    double StationM,
    double NavStationM,
    int SegmentIndex,
    double LateralM,
    double Strength,
    string Confidence,
    string SourceUid,
    string SourceModel,
    string Notes);

internal sealed record V09Zone(
    int ZoneIndex,
    double StartStationM,
    double EndStationM,
    double StartNavStationM,
    double EndNavStationM,
    int StartSegmentIndex,
    int EndSegmentIndex,
    V09ManeuverEvent? BoundaryBefore,
    V09ManeuverEvent? BoundaryAfter);

internal sealed record V09SolvedZone(
    V09Zone Zone,
    string PrimaryIdentity,
    IReadOnlyList<string> Aliases,
    string IdentityConfidence,
    string IdentitySource,
    string CandidateScores,
    double WinningScore,
    bool HasStrongDirectEvidence,
    bool HasConcurrentEvidence);

internal sealed record V09SemanticBlock(
    int BlockIndex,
    int StartZoneIndex,
    int EndZoneIndex,
    int StartSegmentIndex,
    int EndSegmentIndex,
    double StartStationM,
    double EndStationM,
    double StartNavStationM,
    double EndNavStationM,
    string PrimaryIdentity,
    IReadOnlyList<string> Aliases,
    string Cardinal,
    string IdentityConfidence,
    string CardinalConfidence,
    string CardinalSource,
    double EastDisplacementM,
    double NorthDisplacementM,
    double NetDisplacementM,
    double CorridorPhysicalLengthM,
    double Straightness,
    string EvidenceSummary);

internal sealed record V09CruiseLeg(
    int LegIndex,
    int ZoneIndex,
    int BlockIndex,
    double StartNavStationM,
    double EndNavStationM,
    double DistanceM,
    double DistanceMi,
    string RoadIdentity,
    IReadOnlyList<string> Aliases,
    string Cardinal,
    string IdentityConfidence,
    string CardinalConfidence,
    string NextInstruction,
    string NextExitNumber,
    double? DistanceToNextEventM);

internal sealed record V09PlanEntry(
    int PlanIndex,
    double NavStationM,
    string EntryType,
    string Instruction,
    string RoadIdentity,
    string Cardinal,
    double? DistanceM,
    double? DistanceMi,
    string ExitNumber,
    string NextInstruction,
    string NextExitNumber,
    double? DistanceToNextEventM,
    string Confidence,
    string Notes);


internal sealed record V09SemanticRun(
    IReadOnlyList<V09ManeuverEvent> Events,
    IReadOnlyList<V09RouteEvidence> Evidence,
    IReadOnlyList<V09Zone> Zones,
    IReadOnlyList<V09SolvedZone> SolvedZones,
    IReadOnlyList<V09SemanticBlock> Blocks,
    IReadOnlyList<V09CruiseLeg> CruiseLegs,
    IReadOnlyList<V09PlanEntry> Plan);

internal sealed record V09SemanticSummary(
    int Zones,
    int IdentifiedZones,
    int UnknownZones,
    int SemanticBlocks,
    int NamedBlocks,
    int CardinalResolvedBlocks,
    int CruiseLegs,
    int NamedCruiseLegs,
    int PlanEntries,
    int ConcurrentEvidenceZones,
    double NamedNavCoverageM,
    double TotalNavCoverageM)
{
    public double NamedCoverageRatio => TotalNavCoverageM > 0 ? NamedNavCoverageM / TotalNavCoverageM : 0;
}

/// <summary>
/// v0.9 semantic route planner.
///
/// The route identity problem is treated as a sequence-labeling problem over zones
/// between real maneuver events. Strong map-overlay/confirmation-sign evidence is
/// combined with continuity costs. UNKNOWN is a first-class state, so the solver
/// never has to invent a route name through an unsupported connector.
/// </summary>
internal sealed class SemanticRoutePlanner
{
    private readonly Map map;
    private readonly ManeuverFeatureExtractor featureExtractor;
    private readonly SignRoadIdentityResolver signResolver;

    public V09SemanticRun? LastRun { get; private set; }

    public SemanticRoutePlanner(
        Map map,
        ManeuverFeatureExtractor featureExtractor,
        SignRoadIdentityResolver signResolver)
    {
        this.map = map;
        this.featureExtractor = featureExtractor;
        this.signResolver = signResolver;
    }

    public V09SemanticSummary Analyze(
        ProbeRoute route,
        RouteCorridor corridor,
        IReadOnlyList<RoadOverlayMatch> overlayMatches,
        string routeEvidencePath,
        string semanticBlocksPath,
        string cruiseLegsPath,
        string betterGpsPlanPath,
        bool writeReports = true)
    {
        var events = BuildManeuverEvents(route, corridor);
        var evidence = BuildEvidence(corridor, overlayMatches);
        var zones = BuildZones(corridor, events);
        var solvedZones = SolveZones(zones, events, evidence);
        var blocks = BuildSemanticBlocks(corridor, solvedZones, evidence);
        var cruiseLegs = BuildCruiseLegs(solvedZones, blocks);
        var plan = BuildBetterGpsPlan(corridor, events, solvedZones, blocks, cruiseLegs);
        LastRun = new V09SemanticRun(events, evidence, zones, solvedZones, blocks, cruiseLegs, plan);

        if (writeReports)
        {
            WriteEvidence(routeEvidencePath, zones, evidence, solvedZones);
            WriteBlocks(semanticBlocksPath, blocks);
            WriteCruiseLegs(cruiseLegsPath, cruiseLegs);
            WritePlan(betterGpsPlanPath, plan);
        }

        var namedNav = solvedZones
            .Where(z => !IsUnknown(z.PrimaryIdentity))
            .Sum(z => z.Zone.EndNavStationM - z.Zone.StartNavStationM);
        return new V09SemanticSummary(
            zones.Count,
            solvedZones.Count(z => !IsUnknown(z.PrimaryIdentity)),
            solvedZones.Count(z => IsUnknown(z.PrimaryIdentity)),
            blocks.Count,
            blocks.Count(b => !IsUnknown(b.PrimaryIdentity)),
            blocks.Count(b => !string.IsNullOrWhiteSpace(b.Cardinal)),
            cruiseLegs.Count,
            cruiseLegs.Count(l => !IsUnknown(l.RoadIdentity)),
            plan.Count,
            solvedZones.Count(z => z.HasConcurrentEvidence),
            namedNav,
            corridor.TotalNavStationM);
    }

    private IReadOnlyList<V09ManeuverEvent> BuildManeuverEvents(ProbeRoute route, RouteCorridor corridor)
    {
        var events = new List<V09ManeuverEvent>();
        foreach (var segment in corridor.Segments.OrderBy(s => s.SegmentIndex))
        {
            var i = segment.SegmentIndex;
            if (i < 0 || i + 1 >= route.Physical.Count) continue;
            if (!map.Nodes.TryGetValue(route.Physical[i].NodeUid, out var a) ||
                !map.Nodes.TryGetValue(route.Physical[i + 1].NodeUid, out var b))
                continue;
            if (!map.MapItems.TryGetValue(segment.ItemUid, out var item) || item is not Prefab prefab)
                continue;

            var entry = FindNodeIndex(prefab, a.Uid);
            var exit = FindNodeIndex(prefab, b.Uid);
            var feature = featureExtractor.AnalyzeRoute(prefab, entry, exit,route,i);
            if (!feature.PpdResolved || feature.SelectedBranchCount == 0 || feature.PreliminaryClass == "TRANSPARENT")
                continue;

            var decisionStation = (segment.StartStationM + segment.EndStationM) * 0.5;
            var decisionNav = (segment.StartNavStationM + segment.EndNavStationM) * 0.5;
            var center = PositionAtStation(corridor, decisionStation);
            var candidates = signResolver.FindAlongCorridor(corridor, decisionStation, center).Take(30).ToList();

            var exitScores = new Dictionary<string, double>(StringComparer.OrdinalIgnoreCase);
            foreach (var candidate in candidates.Where(SignRoadIdentityResolver.IsStrictExitCandidate))
            {
                var bonus = candidate.Sign.HasExplicitExitText ? 20.0 : 0.0;
                foreach (var number in candidate.Sign.ExitNumbers)
                {
                    var score = candidate.Score + bonus;
                    if (!exitScores.TryGetValue(number, out var existing) || score > existing)
                        exitScores[number] = score;
                }
            }

            var ordered = exitScores.OrderByDescending(kv => kv.Value).ToList();
            var selectedExit = "";
            var ambiguous = false;
            if (ordered.Count == 1) selectedExit = ordered[0].Key;
            else if (ordered.Count > 1)
            {
                if (ordered[0].Value - ordered[1].Value >= 18.0) selectedExit = ordered[0].Key;
                else ambiguous = true;
            }

            var hasStrongExit = !string.IsNullOrWhiteSpace(selectedExit) &&
                                feature.PreliminaryClass.StartsWith("BRANCH_", StringComparison.Ordinal);
            var instruction = SemanticInstruction(feature, hasStrongExit);
            var confidence = SemanticConfidence(feature, instruction, hasStrongExit);

            events.Add(new V09ManeuverEvent(
                events.Count,
                i,
                feature.PreliminaryClass,
                instruction,
                decisionStation,
                decisionNav,
                hasStrongExit ? selectedExit : "",
                feature.PreliminaryClass.StartsWith("BRANCH_", StringComparison.Ordinal) && ambiguous,
                confidence));
        }
        return events.OrderBy(e => e.DecisionNavStationM).ToList();
    }

    private IReadOnlyList<V09RouteEvidence> BuildEvidence(
        RouteCorridor corridor,
        IReadOnlyList<RoadOverlayMatch> overlayMatches)
    {
        var evidence = new List<V09RouteEvidence>();

        foreach (var match in overlayMatches.OrderBy(m => m.Projection.NavStationM))
        {
            var strength = match.Confidence switch
            {
                "HIGH" => 120.0,
                "MEDIUM" => 78.0,
                _ => 10.0
            };
            evidence.Add(new V09RouteEvidence(
                evidence.Count,
                "ROAD_OVERLAY",
                match.Overlay.Identity,
                "",
                match.Projection.StationM,
                match.Projection.NavStationM,
                match.Projection.SegmentIndex,
                match.Projection.LateralM,
                strength,
                match.Confidence,
                Hex(match.Overlay.Uid),
                match.Overlay.RawToken,
                "MapOverlay.RoadName projected onto selected corridor"));
        }

        // Gather shield-bearing signs by sampling the full selected corridor at regular
        // intervals. This avoids missing a confirmation shield near the end of a long
        // Road item while still keeping the spatial query set small; UIDs are de-duplicated
        // before every candidate is projected onto the exact corridor.
        var nearbySigns = new Dictionary<ulong, ParsedSign>();
        const double sampleStepM = 150.0;
        const double sampleRadiusM = 190.0;
        for (var station = 0.0; station <= corridor.TotalStationM + 1e-6; station += sampleStepM)
        {
            var center = PositionAtStation(corridor, Math.Min(station, corridor.TotalStationM));
            foreach (var sign in signResolver.Query(center, sampleRadiusM))
                if (sign.RoadShields.Count > 0) nearbySigns[sign.Uid] = sign;
        }
        if (corridor.TotalStationM > 0)
        {
            var end = PositionAtStation(corridor, corridor.TotalStationM);
            foreach (var sign in signResolver.Query(end, sampleRadiusM))
                if (sign.RoadShields.Count > 0) nearbySigns[sign.Uid] = sign;
        }

        foreach (var sign in nearbySigns.Values)
        {
            var projection = corridor.Project(sign.Position);
            if (projection is null || projection.LateralM > 80) continue;

            var confirmation = IsConfirmationRoadSign(sign);
            var singleShield = sign.RoadShields.Count == 1;
            foreach (var shield in sign.RoadShields.Distinct(StringComparer.OrdinalIgnoreCase))
            {
                // Confirmation signs also need field evidence. A compass word in a
                // different AreaName must never become the direction of this shield.
                var fields = sign.Areas.Where(a => a.RoadShields.Count == 1 && a.RoadShields[0].Equals(shield, StringComparison.OrdinalIgnoreCase))
                    .Select(a => (Area: a, Validation: SignSemanticValidator.Validate(a, null, shield,
                        a.Cardinals.Count == 1 ? a.Cardinals[0] : "", signResolver.IsKnownDestination))).ToList();
                var identityValidated = fields.Any(f => f.Validation.IdentityConfidence == "HIGH");
                var cardinals = fields.Where(f => f.Validation.CardinalConfidence == "HIGH").Select(f => f.Area.Cardinals[0])
                    .Distinct(StringComparer.OrdinalIgnoreCase).ToList();
                var singleCardinal = cardinals.Count == 1 ? cardinals[0] : "";
                double strength;
                string confidence;
                string type;
                if (confirmation && singleShield)
                {
                    strength = string.IsNullOrWhiteSpace(singleCardinal) ? 92.0 : 108.0;
                    strength -= Math.Min(25.0, projection.LateralM * 0.25);
                    // Legacy corridor-only ownership is provisional; HIGH belongs to the validated network association layer.
                    confidence = "MEDIUM";
                    type = "CONFIRMATION_SIGN";
                }
                else if (singleShield)
                {
                    // Large guide signs commonly mention a route belonging to another
                    // branch. They are retained, but intentionally too weak to overrule
                    // an overlay/continuity solution by themselves.
                    strength = 18.0 - Math.Min(8.0, projection.LateralM * 0.10);
                    confidence = "LOW";
                    type = "GUIDE_SIGN_WEAK";
                }
                else
                {
                    strength = 5.0;
                    confidence = "DIAGNOSTIC";
                    type = "MULTI_SHIELD_SIGN_DIAGNOSTIC";
                }

                evidence.Add(new V09RouteEvidence(
                    evidence.Count,
                    type,
                    shield,
                    confirmation && singleShield ? singleCardinal : "",
                    projection.StationM,
                    projection.NavStationM,
                    projection.SegmentIndex,
                    projection.LateralM,
                    strength,
                    confidence,
                    Hex(sign.Uid),
                    sign.Model,
                    string.Join('|', sign.RawStrings)));
            }
        }

        return evidence.OrderBy(e => e.NavStationM).ThenByDescending(e => e.Strength).ToList();
    }

    private static IReadOnlyList<V09Zone> BuildZones(RouteCorridor corridor, IReadOnlyList<V09ManeuverEvent> events)
    {
        var result = new List<V09Zone>();
        var startNav = 0.0;
        var startStation = 0.0;
        V09ManeuverEvent? before = null;

        for (var i = 0; i <= events.Count; i++)
        {
            var after = i < events.Count ? events[i] : null;
            var endNav = after?.DecisionNavStationM ?? corridor.TotalNavStationM;
            var endStation = after?.DecisionStationM ?? corridor.TotalStationM;
            if (endNav < startNav) endNav = startNav;
            if (endStation < startStation) endStation = startStation;

            var overlapping = corridor.Segments
                .Where(s => s.EndNavStationM >= startNav - 1e-6 && s.StartNavStationM <= endNav + 1e-6)
                .OrderBy(s => s.SegmentIndex)
                .ToList();

            result.Add(new V09Zone(
                result.Count,
                startStation,
                endStation,
                startNav,
                endNav,
                overlapping.FirstOrDefault()?.SegmentIndex ?? -1,
                overlapping.LastOrDefault()?.SegmentIndex ?? -1,
                before,
                after));

            if (after is null) break;
            startNav = endNav;
            startStation = endStation;
            before = after;
        }
        return result;
    }

    private static IReadOnlyList<V09SolvedZone> SolveZones(
        IReadOnlyList<V09Zone> zones,
        IReadOnlyList<V09ManeuverEvent> events,
        IReadOnlyList<V09RouteEvidence> evidence)
    {
        if (zones.Count == 0) return [];

        // Only a genuinely strong source may create a route-identity state. Weak
        // guide signs remain diagnostics/supporting evidence; repeated guide boards
        // must never manufacture a current-road identity on their own.
        var candidateIdentities = evidence
            .Where(e => e.Strength >= 60)
            .Select(e => e.Identity)
            .Where(x => !string.IsNullOrWhiteSpace(x))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase)
            .ToList();
        candidateIdentities.Add("UNKNOWN");

        if (candidateIdentities.Count == 1)
        {
            return zones.Select(z => new V09SolvedZone(
                z, "UNKNOWN", [], "NONE", "NO_ROUTE_IDENTITY_EVIDENCE", "UNKNOWN:0", 0, false, false)).ToList();
        }

        var directScores = new List<Dictionary<string, double>>(zones.Count);
        var sourceFlags = new List<Dictionary<string, HashSet<string>>>(zones.Count);
        for (var i = 0; i < zones.Count; i++)
        {
            var zone = zones[i];
            var score = new Dictionary<string, double>(StringComparer.OrdinalIgnoreCase);
            var sources = new Dictionary<string, HashSet<string>>(StringComparer.OrdinalIgnoreCase);
            var zoneEvidence = evidence
                .Where(e => InZone(e.NavStationM, zone, i == zones.Count - 1))
                .Where(e => !string.IsNullOrWhiteSpace(e.Identity))
                .GroupBy(e => e.Identity, StringComparer.OrdinalIgnoreCase);
            foreach (var group in zoneEvidence)
            {
                var strong = group
                    .Where(e => e.EvidenceType is not "GUIDE_SIGN_WEAK" and not "MULTI_SHIELD_SIGN_DIAGNOSTIC")
                    .Sum(e => e.Strength);
                var guide = Math.Min(45.0, group.Where(e => e.EvidenceType == "GUIDE_SIGN_WEAK").Sum(e => e.Strength));
                var multi = Math.Min(10.0, group.Where(e => e.EvidenceType == "MULTI_SHIELD_SIGN_DIAGNOSTIC").Sum(e => e.Strength));
                score[group.Key] = strong + guide + multi;
                sources[group.Key] = group.Select(e => e.EvidenceType).ToHashSet(StringComparer.OrdinalIgnoreCase);
            }
            directScores.Add(score);
            sourceFlags.Add(sources);
        }

        AddBracketEvidence(zones, events, evidence, directScores, sourceFlags);

        var states = candidateIdentities;
        var dp = new double[zones.Count, states.Count];
        var back = new int[zones.Count, states.Count];
        for (var s = 0; s < states.Count; s++)
            dp[0, s] = EmissionScore(states[s], directScores[0]);

        for (var i = 1; i < zones.Count; i++)
        {
            var boundary = zones[i].BoundaryBefore;
            for (var s = 0; s < states.Count; s++)
            {
                var best = double.NegativeInfinity;
                var bestPrev = 0;
                for (var p = 0; p < states.Count; p++)
                {
                    var value = dp[i - 1, p] + TransitionScore(states[p], states[s], boundary);
                    if (value > best)
                    {
                        best = value;
                        bestPrev = p;
                    }
                }
                dp[i, s] = best + EmissionScore(states[s], directScores[i]);
                back[i, s] = bestPrev;
            }
        }

        var lastState = 0;
        var lastScore = double.NegativeInfinity;
        for (var s = 0; s < states.Count; s++)
        {
            if (dp[zones.Count - 1, s] <= lastScore) continue;
            lastScore = dp[zones.Count - 1, s];
            lastState = s;
        }

        var selected = new int[zones.Count];
        selected[^1] = lastState;
        for (var i = zones.Count - 1; i > 0; i--)
            selected[i - 1] = back[i, selected[i]];

        var solved = new List<V09SolvedZone>(zones.Count);
        for (var i = 0; i < zones.Count; i++)
        {
            var primary = states[selected[i]];
            var ranked = directScores[i]
                .OrderByDescending(kv => kv.Value)
                .ThenBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase)
                .ToList();
            var directPrimary = directScores[i].TryGetValue(primary, out var primaryDirect) ? primaryDirect : 0.0;
            var aliases = ranked
                .Where(kv => !string.Equals(kv.Key, primary, StringComparison.OrdinalIgnoreCase))
                .Where(kv => kv.Value >= 70 && (primaryDirect < 1 || kv.Value >= primaryDirect - 25))
                .Select(kv => kv.Key)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .ToList();
            var concurrent = ranked.Count(kv => kv.Value >= 70) >= 2;
            var hasStrong = directPrimary >= 60;

            var confidence = IsUnknown(primary)
                ? "NONE"
                : concurrent ? "CONCURRENT"
                : directPrimary >= 100 ? "HIGH"
                : directPrimary >= 50 ? "MEDIUM"
                : "PROPAGATED";

            var source = "CONTINUITY_SOLVER";
            if (!IsUnknown(primary) && sourceFlags[i].TryGetValue(primary, out var flags) && flags.Count > 0)
                source = string.Join('|', flags.OrderBy(x => x, StringComparer.OrdinalIgnoreCase));
            if (concurrent) source += "|CONCURRENT_EVIDENCE";

            var candidateScores = string.Join('|', ranked.Take(8).Select(kv => $"{kv.Key}:{kv.Value.ToString("0.0", CultureInfo.InvariantCulture)}"));
            if (candidateScores.Length == 0) candidateScores = "UNKNOWN";

            solved.Add(new V09SolvedZone(
                zones[i],
                primary,
                aliases,
                confidence,
                source,
                candidateScores,
                dp[i, selected[i]],
                hasStrong,
                concurrent));
        }

        // Do not back-propagate a future road identity across a reset maneuver merely
        // because the global sequence prefers continuity. After EXIT/TURN/MERGE, the
        // first unsupported zone remains UNKNOWN until direct or bracketed evidence
        // establishes the road actually entered.
        for (var i = 0; i < solved.Count; i++)
        {
            var z = solved[i];
            if (IsUnknown(z.PrimaryIdentity) || z.HasStrongDirectEvidence) continue;
            if (z.IdentitySource.Contains("BRACKETED_STRONG_EVIDENCE", StringComparison.OrdinalIgnoreCase)) continue;
            var before = z.Zone.BoundaryBefore;
            if (before is null || !IsEntryReset(before.Instruction)) continue;
            solved[i] = z with
            {
                PrimaryIdentity = "UNKNOWN",
                Aliases = [],
                IdentityConfidence = "NONE",
                IdentitySource = "RESET_WITHOUT_POST_MANEUVER_EVIDENCE"
            };
        }
        return solved;
    }

    private static void AddBracketEvidence(
        IReadOnlyList<V09Zone> zones,
        IReadOnlyList<V09ManeuverEvent> events,
        IReadOnlyList<V09RouteEvidence> evidence,
        IReadOnlyList<Dictionary<string, double>> scores,
        IReadOnlyList<Dictionary<string, HashSet<string>>> sources)
    {
        var strong = evidence
            .Where(e => e.EvidenceType is "ROAD_OVERLAY" or "CONFIRMATION_SIGN")
            .Where(e => e.Strength >= 70)
            .GroupBy(e => e.Identity, StringComparer.OrdinalIgnoreCase);

        foreach (var group in strong)
        {
            var ordered = group.OrderBy(e => e.NavStationM).ToList();
            for (var p = 0; p + 1 < ordered.Count; p++)
            {
                var a = ordered[p];
                var b = ordered[p + 1];
                if (b.NavStationM - a.NavStationM > 120000) continue;
                if (events.Any(ev => ev.DecisionNavStationM > a.NavStationM && ev.DecisionNavStationM < b.NavStationM && IsHardIdentityReset(ev.Instruction)))
                    continue;

                for (var i = 0; i < zones.Count; i++)
                {
                    var z = zones[i];
                    if (z.StartNavStationM < a.NavStationM - 1e-6 || z.EndNavStationM > b.NavStationM + 1e-6) continue;
                    if (!scores[i].ContainsKey(group.Key)) scores[i][group.Key] = 0;
                    scores[i][group.Key] += 55.0;
                    if (!sources[i].TryGetValue(group.Key, out var set)) sources[i][group.Key] = set = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                    set.Add("BRACKETED_STRONG_EVIDENCE");
                }
            }
        }
    }

    private IReadOnlyList<V09SemanticBlock> BuildSemanticBlocks(
        RouteCorridor corridor,
        IReadOnlyList<V09SolvedZone> zones,
        IReadOnlyList<V09RouteEvidence> evidence)
    {
        var raw = new List<List<V09SolvedZone>>();
        foreach (var zone in zones)
        {
            if (raw.Count == 0 || !string.Equals(raw[^1][0].PrimaryIdentity, zone.PrimaryIdentity, StringComparison.OrdinalIgnoreCase))
                raw.Add([zone]);
            else
                raw[^1].Add(zone);
        }

        var result = new List<V09SemanticBlock>();
        foreach (var group in raw)
        {
            var first = group[0];
            var last = group[^1];
            var identity = first.PrimaryIdentity;
            var aliases = group.SelectMany(z => z.Aliases).Distinct(StringComparer.OrdinalIgnoreCase).Where(a => !string.Equals(a, identity, StringComparison.OrdinalIgnoreCase)).ToList();
            var blockEvidence = evidence
                .Where(e => e.NavStationM >= first.Zone.StartNavStationM - 1e-6 && e.NavStationM <= last.Zone.EndNavStationM + 1e-6)
                .Where(e => IsUnknown(identity) || string.Equals(e.Identity, identity, StringComparison.OrdinalIgnoreCase))
                .ToList();

            var cardinal = ResolveCardinal(
                corridor,
                identity,
                first.Zone.StartNavStationM,
                last.Zone.EndNavStationM,
                blockEvidence);

            var identityConfidence = IsUnknown(identity)
                ? "NONE"
                : group.Any(z => z.IdentityConfidence == "HIGH") ? "HIGH"
                : aliases.Count > 0 ? "CONCURRENT"
                : group.Any(z => z.IdentityConfidence == "MEDIUM") ? "MEDIUM"
                : "PROPAGATED";

            var evidenceSummary = string.Join('|', blockEvidence
                .Where(e => e.Strength >= 50)
                .Select(e => $"{e.EvidenceType}:{e.Identity}{(string.IsNullOrWhiteSpace(e.Cardinal) ? "" : " " + e.Cardinal)}@{e.NavStationM.ToString("0", CultureInfo.InvariantCulture)}")
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .Take(20));

            result.Add(new V09SemanticBlock(
                result.Count,
                first.Zone.ZoneIndex,
                last.Zone.ZoneIndex,
                first.Zone.StartSegmentIndex,
                last.Zone.EndSegmentIndex,
                first.Zone.StartStationM,
                last.Zone.EndStationM,
                first.Zone.StartNavStationM,
                last.Zone.EndNavStationM,
                identity,
                aliases,
                cardinal.Cardinal,
                identityConfidence,
                cardinal.Confidence,
                cardinal.Source,
                cardinal.EastM,
                cardinal.NorthM,
                cardinal.NetM,
                Math.Max(0, last.Zone.EndStationM - first.Zone.StartStationM),
                cardinal.Straightness,
                evidenceSummary));
        }
        return result;
    }

    private static IReadOnlyList<V09CruiseLeg> BuildCruiseLegs(
        IReadOnlyList<V09SolvedZone> zones,
        IReadOnlyList<V09SemanticBlock> blocks)
    {
        var result = new List<V09CruiseLeg>();
        foreach (var solved in zones)
        {
            if (IsUnknown(solved.PrimaryIdentity)) continue;
            var distance = solved.Zone.EndNavStationM - solved.Zone.StartNavStationM;
            if (distance < 250) continue;
            var block = blocks.FirstOrDefault(b => solved.Zone.ZoneIndex >= b.StartZoneIndex && solved.Zone.ZoneIndex <= b.EndZoneIndex);
            var cardinal = block?.Cardinal ?? "";
            var cardinalConfidence = block?.CardinalConfidence ?? "NONE";
            var next = solved.Zone.BoundaryAfter;
            result.Add(new V09CruiseLeg(
                result.Count,
                solved.Zone.ZoneIndex,
                block?.BlockIndex ?? -1,
                solved.Zone.StartNavStationM,
                solved.Zone.EndNavStationM,
                distance,
                distance / 1609.344,
                solved.PrimaryIdentity,
                block?.Aliases ?? [],
                cardinal,
                solved.IdentityConfidence,
                cardinalConfidence,
                next?.Instruction ?? "END",
                next?.ExitNumber ?? "",
                next is null ? null : Math.Max(0, next.DecisionNavStationM - solved.Zone.StartNavStationM)));
        }
        return result;
    }

    private static IReadOnlyList<V09PlanEntry> BuildBetterGpsPlan(
        RouteCorridor corridor,
        IReadOnlyList<V09ManeuverEvent> events,
        IReadOnlyList<V09SolvedZone> zones,
        IReadOnlyList<V09SemanticBlock> blocks,
        IReadOnlyList<V09CruiseLeg> cruiseLegs)
    {
        var entries = new List<V09PlanEntry>();

        foreach (var leg in cruiseLegs)
        {
            entries.Add(new V09PlanEntry(
                0,
                leg.StartNavStationM,
                "CRUISE",
                "STAY_ON",
                leg.RoadIdentity,
                leg.Cardinal,
                leg.DistanceM,
                leg.DistanceMi,
                "",
                leg.NextInstruction,
                leg.NextExitNumber,
                leg.DistanceToNextEventM,
                leg.IdentityConfidence,
                string.Join('|', new[] { "distance_from_route_task_navigation_station", leg.Aliases.Count > 0 ? "aliases=" + string.Join('|', leg.Aliases) : "" }.Where(x => x.Length > 0))));
        }

        for (var i = 0; i < events.Count; i++)
        {
            var ev = events[i];
            var before = zones.LastOrDefault(z => z.Zone.EndNavStationM <= ev.DecisionNavStationM + 1e-6 && z.Zone.BoundaryAfter?.EventIndex == ev.EventIndex);
            var after = zones.FirstOrDefault(z => z.Zone.StartNavStationM >= ev.DecisionNavStationM - 1e-6 && z.Zone.BoundaryBefore?.EventIndex == ev.EventIndex);
            var beforeBlock = before is null ? null : blocks.FirstOrDefault(b => before.Zone.ZoneIndex >= b.StartZoneIndex && before.Zone.ZoneIndex <= b.EndZoneIndex);
            var afterBlock = after is null ? null : blocks.FirstOrDefault(b => after.Zone.ZoneIndex >= b.StartZoneIndex && after.Zone.ZoneIndex <= b.EndZoneIndex);
            var next = i + 1 < events.Count ? events[i + 1] : null;
            entries.Add(new V09PlanEntry(
                0,
                ev.DecisionNavStationM,
                "MANEUVER",
                ev.Instruction,
                after?.PrimaryIdentity ?? before?.PrimaryIdentity ?? "UNKNOWN",
                afterBlock?.Cardinal ?? beforeBlock?.Cardinal ?? "",
                null,
                null,
                ev.ExitNumber,
                next?.Instruction ?? "END",
                next?.ExitNumber ?? "",
                next is null ? null : Math.Max(0, next.DecisionNavStationM - ev.DecisionNavStationM),
                ev.Confidence,
                IdentityChangeNote(before?.PrimaryIdentity, after?.PrimaryIdentity)));
        }

        entries.Add(new V09PlanEntry(
            0,
            corridor.TotalNavStationM,
            "ARRIVAL",
            "ARRIVE",
            zones.LastOrDefault()?.PrimaryIdentity ?? "UNKNOWN",
            blocks.LastOrDefault()?.Cardinal ?? "",
            null,
            null,
            "",
            "",
            "",
            null,
            "",
            "route_end"));

        return entries
            .OrderBy(e => e.NavStationM)
            .ThenBy(e => e.EntryType == "MANEUVER" ? 0 : e.EntryType == "CRUISE" ? 1 : 2)
            .Select((e, i) => e with { PlanIndex = i })
            .ToList();
    }

    private static (string Cardinal, string Confidence, string Source, double EastM, double NorthM, double NetM, double Straightness) ResolveCardinal(
        RouteCorridor corridor,
        string identity,
        double startNav,
        double endNav,
        IReadOnlyList<V09RouteEvidence> evidence)
    {
        if (IsUnknown(identity)) return ("", "NONE", "", 0, 0, 0, 0);

        var explicitCardinals = evidence
            .Where(e => e.EvidenceType == "CONFIRMATION_SIGN" && e.Strength >= 70)
            .Where(e => string.Equals(e.Identity, identity, StringComparison.OrdinalIgnoreCase))
            .Where(e => !string.IsNullOrWhiteSpace(e.Cardinal))
            .GroupBy(e => e.Cardinal, StringComparer.OrdinalIgnoreCase)
            .Select(g => (Cardinal: g.Key.ToUpperInvariant(), Score: g.Sum(x => x.Strength)))
            .OrderByDescending(x => x.Score)
            .ToList();
        if (explicitCardinals.Count == 1 || (explicitCardinals.Count > 1 && explicitCardinals[0].Score - explicitCardinals[1].Score >= 35))
        {
            var geom = GeometryDisplacement(corridor, startNav, endNav);
            return (explicitCardinals[0].Cardinal, "HIGH", "CONFIRMATION_SIGN", geom.EastM, geom.NorthM, geom.NetM, geom.Straightness);
        }

        var g = GeometryDisplacement(corridor, startNav, endNav);
        if (g.NetM < 1500 || endNav - startNav < 3000)
            return ("", "LOW", "GEOMETRY_TOO_SHORT", g.EastM, g.NorthM, g.NetM, g.Straightness);

        var expectedAxis = ExpectedAxis(identity);
        string cardinal;
        double dominance;
        if (expectedAxis == "NS")
        {
            cardinal = g.NorthM >= 0 ? "NORTH" : "SOUTH";
            dominance = Math.Abs(g.NorthM) / Math.Max(1.0, Math.Abs(g.EastM));
        }
        else if (expectedAxis == "EW")
        {
            cardinal = g.EastM >= 0 ? "EAST" : "WEST";
            dominance = Math.Abs(g.EastM) / Math.Max(1.0, Math.Abs(g.NorthM));
        }
        else if (Math.Abs(g.NorthM) >= Math.Abs(g.EastM))
        {
            cardinal = g.NorthM >= 0 ? "NORTH" : "SOUTH";
            dominance = Math.Abs(g.NorthM) / Math.Max(1.0, Math.Abs(g.EastM));
        }
        else
        {
            cardinal = g.EastM >= 0 ? "EAST" : "WEST";
            dominance = Math.Abs(g.EastM) / Math.Max(1.0, Math.Abs(g.NorthM));
        }

        if (dominance < 1.15)
            return ("", "LOW", "GEOMETRY_AXIS_AMBIGUOUS", g.EastM, g.NorthM, g.NetM, g.Straightness);

        var confidence = expectedAxis.Length > 0 && dominance >= 1.5 && g.Straightness >= 0.20 ? "HIGH" :
                         dominance >= 1.35 ? "MEDIUM" : "LOW";
        return (cardinal, confidence, expectedAxis.Length > 0 ? "WGS84_GEOMETRY+ROUTE_NUMBER_AXIS" : "WGS84_GEOMETRY_DOMINANT_AXIS", g.EastM, g.NorthM, g.NetM, g.Straightness);
    }

    private static (double EastM, double NorthM, double NetM, double Straightness) GeometryDisplacement(RouteCorridor corridor, double startNav, double endNav)
    {
        var start = PositionAtNavStation(corridor, startNav);
        var end = PositionAtNavStation(corridor, endNav);
        var a = AtsMapProjection.ToWgs84(start.X, start.Z);
        var b = AtsMapProjection.ToWgs84(end.X, end.Z);
        var avgLatRad = (a.Latitude + b.Latitude) * 0.5 * Math.PI / 180.0;
        const double metersPerDegree = 111_320.0;
        var north = (b.Latitude - a.Latitude) * metersPerDegree;
        var east = (b.Longitude - a.Longitude) * metersPerDegree * Math.Cos(avgLatRad);
        var net = Math.Sqrt(east * east + north * north);
        var physicalLength = PhysicalLengthBetweenNavStations(corridor, startNav, endNav);
        // ATS map coordinates are ~1:20 in the normal world. Straightness uses map
        // geometry directly, so it remains a dimensionless curvature diagnostic.
        var mapNet = DistanceXZ(start, end);
        var straightness = physicalLength > 1e-6 ? Math.Clamp(mapNet / physicalLength, 0, 1) : 0;
        return (east, north, net, straightness);
    }

    private static double PhysicalLengthBetweenNavStations(RouteCorridor corridor, double startNav, double endNav)
    {
        double total = 0;
        foreach (var edge in corridor.Edges)
        {
            if (edge.EndNavStationM <= startNav || edge.StartNavStationM >= endNav) continue;
            var navLen = edge.EndNavStationM - edge.StartNavStationM;
            var physLen = edge.EndStationM - edge.StartStationM;
            if (navLen <= 1e-9) continue;
            var a = Math.Max(startNav, edge.StartNavStationM);
            var b = Math.Min(endNav, edge.EndNavStationM);
            if (b <= a) continue;
            total += physLen * ((b - a) / navLen);
        }
        return total;
    }

    private static string ExpectedAxis(string identity)
    {
        var parts = identity.Split('-', 2, StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length != 2 || !int.TryParse(new string(parts[1].TakeWhile(char.IsDigit).ToArray()), NumberStyles.Integer, CultureInfo.InvariantCulture, out var number))
            return "";
        var prefix = parts[0].ToUpperInvariant();
        if (prefix == "I" && number < 100) return number % 2 == 0 ? "EW" : "NS";
        if (prefix == "US") return number % 2 == 0 ? "EW" : "NS";
        return "";
    }

    private static double EmissionScore(string state, IReadOnlyDictionary<string, double> direct)
    {
        var strongest = direct.Count > 0 ? direct.Values.Max() : 0.0;
        if (IsUnknown(state)) return strongest >= 60 ? -90.0 : 12.0;
        return direct.TryGetValue(state, out var score) ? score : -18.0;
    }

    private static double TransitionScore(string previous, string current, V09ManeuverEvent? boundary)
    {
        var prevUnknown = IsUnknown(previous);
        var curUnknown = IsUnknown(current);
        if (prevUnknown && curUnknown) return 12.0;
        var same = !prevUnknown && !curUnknown && string.Equals(previous, current, StringComparison.OrdinalIgnoreCase);
        var instruction = boundary?.Instruction ?? "";

        if (instruction.StartsWith("KEEP_", StringComparison.Ordinal) || instruction == "CONTINUE_AT_FORK")
        {
            if (same) return 45;
            if (prevUnknown && !curUnknown) return -5;
            if (!prevUnknown && curUnknown) return -10;
            return -35;
        }
        if (instruction.StartsWith("EXIT_", StringComparison.Ordinal))
        {
            if (same) return -40;
            if (!prevUnknown && curUnknown) return 22;
            if (prevUnknown && !curUnknown) return 0;
            return 5;
        }
        if (instruction.StartsWith("MERGE_", StringComparison.Ordinal))
        {
            if (same) return 8;
            if (!prevUnknown && curUnknown) return 0;
            if (prevUnknown && !curUnknown) return 18;
            return 10;
        }
        if (instruction.StartsWith("TURN_", StringComparison.Ordinal) ||
            instruction.StartsWith("SHARP_", StringComparison.Ordinal) ||
            instruction.StartsWith("U_TURN_", StringComparison.Ordinal))
        {
            if (same) return -20;
            if (!prevUnknown && curUnknown) return 15;
            if (prevUnknown && !curUnknown) return 0;
            return 0;
        }
        if (same) return 20;
        return -10;
    }

    private static bool IsHardIdentityReset(string instruction)
        => instruction.StartsWith("EXIT_", StringComparison.Ordinal) ||
           instruction.StartsWith("MERGE_", StringComparison.Ordinal) ||
           instruction.StartsWith("TURN_", StringComparison.Ordinal) ||
           instruction.StartsWith("SHARP_", StringComparison.Ordinal) ||
           instruction.StartsWith("U_TURN_", StringComparison.Ordinal);

    private static bool IsEntryReset(string instruction)
        => IsHardIdentityReset(instruction);

    private static bool InZone(double navStation, V09Zone zone, bool finalZone)
        => navStation >= zone.StartNavStationM - 1e-6 &&
           (navStation < zone.EndNavStationM - 1e-6 || (finalZone && navStation <= zone.EndNavStationM + 1e-6));

    private static bool IsConfirmationRoadSign(ParsedSign sign)
        => sign.Model.Equals("rot_bd_44", StringComparison.OrdinalIgnoreCase) ||
           sign.DefinitionName.Contains("hw shield single dir", StringComparison.OrdinalIgnoreCase);

    private static bool IsUnknown(string identity)
        => string.IsNullOrWhiteSpace(identity) || identity.Equals("UNKNOWN", StringComparison.OrdinalIgnoreCase);

    private static string SemanticInstruction(ManeuverFeatureResult feature, bool hasExitEvidence)
    {
        var c = feature.PreliminaryClass;
        if (c == "TRANSPARENT") return "CONTINUE";
        if (c.StartsWith("TURN_", StringComparison.Ordinal) ||
            c.StartsWith("SHARP_", StringComparison.Ordinal) ||
            c.StartsWith("U_TURN_", StringComparison.Ordinal) ||
            c.StartsWith("MERGE_", StringComparison.Ordinal))
            return c;

        if (c.StartsWith("BRANCH_", StringComparison.Ordinal))
        {
            var side = c.EndsWith("LEFT", StringComparison.Ordinal) ? "LEFT" : "RIGHT";
            return hasExitEvidence ? "EXIT_" + side : "KEEP_" + side;
        }

        if (c == "FORK_STRAIGHT")
        {
            if (feature.SelectedTouchesDividerSide && !feature.SelectedTouchesOuterSide) return "KEEP_LEFT";
            if (feature.SelectedTouchesOuterSide && !feature.SelectedTouchesDividerSide) return "KEEP_RIGHT";
            return "CONTINUE_AT_FORK";
        }
        return c;
    }

    private static string SemanticConfidence(ManeuverFeatureResult feature, string instruction, bool exitEvidence)
    {
        if (instruction.StartsWith("EXIT_", StringComparison.Ordinal)) return exitEvidence ? "HIGH" : "LOW";
        if (instruction.StartsWith("KEEP_", StringComparison.Ordinal))
            return feature.SelectedLaneCount < feature.InputLaneCount || feature.PreliminaryClass.StartsWith("BRANCH_", StringComparison.Ordinal) ? "MEDIUM" : "LOW";
        return feature.Confidence;
    }

    private static string IdentityChangeNote(string? before, string? after)
    {
        before = IsUnknown(before ?? "") ? "UNKNOWN" : before;
        after = IsUnknown(after ?? "") ? "UNKNOWN" : after;
        return string.Equals(before, after, StringComparison.OrdinalIgnoreCase)
            ? "same_route_identity"
            : $"route_identity:{before}->{after}";
    }

    private static int FindNodeIndex(Prefab prefab, ulong uid)
    {
        for (var i = 0; i < prefab.Nodes.Count; i++)
            if (prefab.Nodes[i].Uid == uid) return i;
        return -1;
    }

    private static Vector3 PositionAtStation(RouteCorridor corridor, double stationM)
    {
        if (corridor.Edges.Count == 0) return Vector3.Zero;
        var edge = corridor.Edges.FirstOrDefault(e => stationM >= e.StartStationM - 1e-6 && stationM <= e.EndStationM + 1e-6)
                   ?? (stationM <= 0 ? corridor.Edges[0] : corridor.Edges[^1]);
        var den = edge.EndStationM - edge.StartStationM;
        var t = den > 1e-9 ? Math.Clamp((stationM - edge.StartStationM) / den, 0, 1) : 0;
        return Vector3.Lerp(edge.Start, edge.End, (float)t);
    }

    private static Vector3 PositionAtNavStation(RouteCorridor corridor, double navStationM)
    {
        if (corridor.Edges.Count == 0) return Vector3.Zero;
        var edge = corridor.Edges.FirstOrDefault(e => navStationM >= e.StartNavStationM - 1e-6 && navStationM <= e.EndNavStationM + 1e-6)
                   ?? (navStationM <= 0 ? corridor.Edges[0] : corridor.Edges[^1]);
        var den = edge.EndNavStationM - edge.StartNavStationM;
        var t = den > 1e-9 ? Math.Clamp((navStationM - edge.StartNavStationM) / den, 0, 1) : 0;
        return Vector3.Lerp(edge.Start, edge.End, (float)t);
    }

    private static double DistanceXZ(Vector3 a, Vector3 b)
    {
        var dx = (double)a.X - b.X;
        var dz = (double)a.Z - b.Z;
        return Math.Sqrt(dx * dx + dz * dz);
    }

    private static void WriteEvidence(
        string path,
        IReadOnlyList<V09Zone> zones,
        IReadOnlyList<V09RouteEvidence> evidence,
        IReadOnlyList<V09SolvedZone> solved)
    {
        using var writer = OpenCsv(path);
        writer.WriteLine("row_type,zone_index,start_nav_station_m,end_nav_station_m,primary_identity,aliases,identity_confidence,identity_source,candidate_scores,evidence_index,evidence_type,evidence_identity,evidence_cardinal,evidence_nav_station_m,evidence_station_m,evidence_segment_index,evidence_lateral_m,evidence_strength,evidence_confidence,source_uid,source_model,notes");
        for (var i = 0; i < zones.Count; i++)
        {
            var s = solved[i];
            writer.WriteLine(string.Join(',',
                "ZONE", i, F(zones[i].StartNavStationM), F(zones[i].EndNavStationM), Csv(s.PrimaryIdentity), Csv(string.Join('|', s.Aliases)), Csv(s.IdentityConfidence), Csv(s.IdentitySource), Csv(s.CandidateScores),
                "", "", "", "", "", "", "", "", "", "", "", "", Csv(s.HasConcurrentEvidence ? "concurrent_direct_evidence" : "")));

            foreach (var e in evidence.Where(e => InZone(e.NavStationM, zones[i], i == zones.Count - 1)))
            {
                writer.WriteLine(string.Join(',',
                    "EVIDENCE", i, F(zones[i].StartNavStationM), F(zones[i].EndNavStationM), Csv(s.PrimaryIdentity), Csv(string.Join('|', s.Aliases)), Csv(s.IdentityConfidence), Csv(s.IdentitySource), Csv(s.CandidateScores),
                    e.EvidenceIndex, Csv(e.EvidenceType), Csv(e.Identity), Csv(e.Cardinal), F(e.NavStationM), F(e.StationM), e.SegmentIndex, F(e.LateralM), F(e.Strength), Csv(e.Confidence), Csv(e.SourceUid), Csv(e.SourceModel), Csv(e.Notes)));
            }
        }
    }

    private static void WriteBlocks(string path, IReadOnlyList<V09SemanticBlock> blocks)
    {
        using var writer = OpenCsv(path);
        writer.WriteLine("block_index,start_zone_index,end_zone_index,start_segment_index,end_segment_index,start_station_m,end_station_m,physical_length_m,start_nav_station_m,end_nav_station_m,nav_length_m,primary_identity,aliases,cardinal,identity_confidence,cardinal_confidence,cardinal_source,east_displacement_m,north_displacement_m,net_displacement_m,corridor_physical_length_m,straightness,evidence_summary");
        foreach (var b in blocks)
        {
            writer.WriteLine(string.Join(',',
                b.BlockIndex, b.StartZoneIndex, b.EndZoneIndex, b.StartSegmentIndex, b.EndSegmentIndex,
                F(b.StartStationM), F(b.EndStationM), F(b.EndStationM - b.StartStationM),
                F(b.StartNavStationM), F(b.EndNavStationM), F(b.EndNavStationM - b.StartNavStationM),
                Csv(b.PrimaryIdentity), Csv(string.Join('|', b.Aliases)), Csv(b.Cardinal), Csv(b.IdentityConfidence), Csv(b.CardinalConfidence), Csv(b.CardinalSource),
                F(b.EastDisplacementM), F(b.NorthDisplacementM), F(b.NetDisplacementM), F(b.CorridorPhysicalLengthM), F(b.Straightness), Csv(b.EvidenceSummary)));
        }
    }

    private static void WriteCruiseLegs(string path, IReadOnlyList<V09CruiseLeg> legs)
    {
        using var writer = OpenCsv(path);
        writer.WriteLine("leg_index,zone_index,block_index,start_nav_station_m,end_nav_station_m,distance_m,distance_mi,road_identity,aliases,cardinal,identity_confidence,cardinal_confidence,next_instruction,next_exit_number,distance_to_next_event_m");
        foreach (var l in legs)
        {
            writer.WriteLine(string.Join(',',
                l.LegIndex, l.ZoneIndex, l.BlockIndex, F(l.StartNavStationM), F(l.EndNavStationM), F(l.DistanceM), F(l.DistanceMi),
                Csv(l.RoadIdentity), Csv(string.Join('|', l.Aliases)), Csv(l.Cardinal), Csv(l.IdentityConfidence), Csv(l.CardinalConfidence), Csv(l.NextInstruction), Csv(l.NextExitNumber),
                l.DistanceToNextEventM.HasValue ? F(l.DistanceToNextEventM.Value) : ""));
        }
    }

    private static void WritePlan(string path, IReadOnlyList<V09PlanEntry> plan)
    {
        using var writer = OpenCsv(path);
        writer.WriteLine("plan_index,nav_station_m,entry_type,instruction,road_identity,cardinal,distance_m,distance_mi,exit_number,next_instruction,next_exit_number,distance_to_next_event_m,confidence,notes");
        foreach (var p in plan)
        {
            writer.WriteLine(string.Join(',',
                p.PlanIndex, F(p.NavStationM), Csv(p.EntryType), Csv(p.Instruction), Csv(p.RoadIdentity), Csv(p.Cardinal),
                p.DistanceM.HasValue ? F(p.DistanceM.Value) : "",
                p.DistanceMi.HasValue ? F(p.DistanceMi.Value) : "",
                Csv(p.ExitNumber), Csv(p.NextInstruction), Csv(p.NextExitNumber),
                p.DistanceToNextEventM.HasValue ? F(p.DistanceToNextEventM.Value) : "",
                Csv(p.Confidence), Csv(p.Notes)));
        }
    }

    private static StreamWriter OpenCsv(string path) => new(path, false, new System.Text.UTF8Encoding(false));
    private static string F(double value) => value.ToString("0.######", CultureInfo.InvariantCulture);
    private static string Hex(ulong value) => value.ToString("X16", CultureInfo.InvariantCulture);
    private static string Csv(string? value)
    {
        value ??= "";
        if (!value.Contains(',') && !value.Contains('"') && !value.Contains('\n') && !value.Contains('\r')) return value;
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}

/// <summary>
/// Independent spherical Lambert Conformal Conic inverse for ATS map coordinates.
/// Parameters are the ATS map projection values used by SCS (lat_1=33, lat_2=45,
/// origin=39/-96 and the ATS map factors). No external projection package is needed.
/// </summary>
internal static class AtsMapProjection
{
    private const double RadiusM = 6_370_997.0;
    private const double MapFactorZ = -0.00017706234;
    private const double MapFactorX = 0.000176689948;
    private static readonly double Phi1 = DegToRad(33.0);
    private static readonly double Phi2 = DegToRad(45.0);
    private static readonly double Phi0 = DegToRad(39.0);
    private static readonly double Lambda0 = DegToRad(-96.0);
    private static readonly double N = Math.Log(Math.Cos(Phi1) / Math.Cos(Phi2)) /
                                       Math.Log(Math.Tan(Math.PI / 4 + Phi2 / 2) / Math.Tan(Math.PI / 4 + Phi1 / 2));
    private static readonly double F = Math.Cos(Phi1) * Math.Pow(Math.Tan(Math.PI / 4 + Phi1 / 2), N) / N;
    private static readonly double Rho0 = RadiusM * F / Math.Pow(Math.Tan(Math.PI / 4 + Phi0 / 2), N);
    private static readonly double LengthOfDegree = RadiusM * Math.PI / 180.0;

    internal readonly record struct LonLat(double Longitude, double Latitude);

    public static LonLat ToWgs84(double mapX, double mapZ)
    {
        var x = mapX * MapFactorX * LengthOfDegree;
        var y = mapZ * MapFactorZ * LengthOfDegree;
        var rho = Math.CopySign(Math.Sqrt(x * x + (Rho0 - y) * (Rho0 - y)), N);
        var theta = Math.Atan2(x, Rho0 - y);
        var phi = 2 * Math.Atan(Math.Pow(RadiusM * F / rho, 1.0 / N)) - Math.PI / 2;
        var lambda = Lambda0 + theta / N;
        return new LonLat(RadToDeg(lambda), RadToDeg(phi));
    }

    private static double DegToRad(double value) => value * Math.PI / 180.0;
    private static double RadToDeg(double value) => value * 180.0 / Math.PI;
}
