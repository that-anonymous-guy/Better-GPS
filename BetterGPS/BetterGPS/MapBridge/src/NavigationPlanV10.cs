using System.Globalization;
using System.Numerics;
using System.Text.RegularExpressions;

namespace BetterGPSNavigation;

internal sealed record V10BranchSignEvidence(
    int EvidenceIndex,
    int EventIndex,
    double EventNavStationM,
    string Instruction,
    string ExitNumber,
    ulong SignUid,
    string Template,
    string AreaName,
    string Region,
    double AlongM,
    double LateralM,
    double OrientationErrorDeg,
    string BoardToken,
    double BoardOffsetX,
    double BoardOffsetY,
    double? TemplateLocalX,
    double? TemplateLocalY,
    string Identity,
    string Cardinal,
    string Destinations,
    string AreaExitNumbers,
    string ArrowTokens,
    string MatchReason,
    string Confidence,
    double Strength);

internal sealed record V10ZoneState(
    V09SolvedZone Original,
    string PrimaryIdentity,
    IReadOnlyList<string> Aliases,
    string Confidence,
    string Source);

internal sealed record V10IdentityInterval(
    int IntervalIndex,
    string Identity,
    bool Primary,
    int StartZoneIndex,
    int EndZoneIndex,
    double StartNavStationM,
    double EndNavStationM,
    double DistanceM,
    double DistanceMi,
    string Cardinal,
    string IdentityConfidence,
    string CardinalConfidence,
    string CardinalSource,
    string Sources);

internal sealed record V10CruiseSpan(
    int SpanIndex,
    int PrimaryIntervalIndex,
    int StartZoneIndex,
    int EndZoneIndex,
    double StartNavStationM,
    double EndNavStationM,
    double DistanceM,
    double DistanceMi,
    string SpanType,
    string RoadIdentity,
    string Cardinal,
    string IdentityConfidence,
    string CardinalConfidence,
    int GuidanceEventCount,
    string TerminalInstruction,
    string TerminalExitNumber);

internal sealed record V10GuidanceEvent(
    int GuidanceIndex,
    int EventIndex,
    int ParentSpanIndex,
    double NavStationM,
    double OffsetInSpanM,
    string Instruction,
    string ExitNumber,
    string RoadBefore,
    string RoadAfter,
    bool ChangesIdentity,
    string Destination,
    string BranchEvidenceConfidence,
    string Confidence,
    string NextInstruction,
    double? DistanceToNextEventM);

internal sealed record V10PlanEntry(
    int PlanIndex,
    double NavStationM,
    string EntryType,
    int SpanIndex,
    string Instruction,
    string RoadIdentity,
    string Cardinal,
    double? DistanceM,
    double? DistanceMi,
    string ExitNumber,
    string Destination,
    string RoadBefore,
    string RoadAfter,
    string NextInstruction,
    double? DistanceToNextEventM,
    string Confidence,
    string Notes);

internal sealed record V10Summary(
    int IdentityIntervals,
    int PrimaryIntervals,
    int AliasIntervals,
    int NamedPrimaryIntervals,
    int CruiseSpans,
    int NamedCruiseSpans,
    int ConnectorSpans,
    int GuidanceEvents,
    int PlanEntries,
    int BranchSignEvidence,
    int HighBranchSignEvidence,
    int BranchPromotedZones,
    double NamedNavCoverageM,
    double TotalNavCoverageM)
{
    public double NamedCoverageRatio => TotalNavCoverageM > 0 ? NamedNavCoverageM / TotalNavCoverageM : 0;
}

/// <summary>
/// v0.10 navigation synthesis.
///
/// Key architectural change from v0.9: route continuity and driver guidance are
/// orthogonal. A KEEP event no longer splits a named route into multiple cruise
/// legs. Primary/alias identities get their own intervals; guidance events are
/// attached inside a CruiseSpan. Sign areas are used conservatively to produce
/// branch-specific road-after evidence without flattening multi-board guide signs.
/// </summary>
internal sealed class NavigationPlanV10
{
    private readonly SignRoadIdentityResolver signResolver;
    private readonly SignTemplateResolver templateResolver;

    public NavigationPlanV10(SignRoadIdentityResolver signResolver, SignTemplateResolver templateResolver)
    {
        this.signResolver = signResolver;
        this.templateResolver = templateResolver;
    }

    public V10Summary Analyze(
        RouteCorridor corridor,
        V09SemanticRun run,
        string identityIntervalsPath,
        string cruiseSpansPath,
        string guidanceEventsPath,
        string branchSignEvidencePath,
        string betterGpsPlanV2Path)
    {
        var branchEvidence = BuildBranchSignEvidence(corridor, run.Events);
        var states = ApplyBranchEvidence(run, branchEvidence);
        var intervals = BuildIdentityIntervals(corridor, run, states);
        var spans = BuildCruiseSpans(intervals, run.Events);
        var guidance = BuildGuidanceEvents(run.Events, spans, intervals, branchEvidence);
        spans = AttachGuidanceCounts(spans, guidance);
        var plan = BuildPlanV2(corridor, spans, guidance);

        WriteBranchEvidence(branchSignEvidencePath, branchEvidence);
        WriteIdentityIntervals(identityIntervalsPath, intervals);
        WriteCruiseSpans(cruiseSpansPath, spans);
        WriteGuidanceEvents(guidanceEventsPath, guidance);
        WritePlanV2(betterGpsPlanV2Path, plan);

        var namedCoverage = intervals.Where(i => i.Primary && !IsUnknown(i.Identity)).Sum(i => i.DistanceM);
        return new V10Summary(
            intervals.Count,
            intervals.Count(i => i.Primary),
            intervals.Count(i => !i.Primary),
            intervals.Count(i => i.Primary && !IsUnknown(i.Identity)),
            spans.Count,
            spans.Count(s => !IsUnknown(s.RoadIdentity)),
            spans.Count(s => IsUnknown(s.RoadIdentity)),
            guidance.Count,
            plan.Count,
            branchEvidence.Count,
            branchEvidence.Count(e => e.Confidence == "HIGH"),
            states.Count(s => s.Source == "BRANCH_SIGN_HIGH"),
            namedCoverage,
            corridor.TotalNavStationM);
    }

    private IReadOnlyList<V10BranchSignEvidence> BuildBranchSignEvidence(RouteCorridor corridor, IReadOnlyList<V09ManeuverEvent> events)
    {
        var result = new List<V10BranchSignEvidence>();
        foreach (var ev in events)
        {
            var center = PositionAtStation(corridor, ev.DecisionStationM);
            var candidates = signResolver.FindAlongCorridor(corridor, ev.DecisionStationM, center, 900).Take(40).ToList();
            foreach (var c in candidates)
            {
                foreach (var area in c.Sign.Areas.Where(a => a.HasSemanticData))
                {
                    var binding = templateResolver.FindAreaBinding(c.Sign.Template, area.AreaName);
                    var identity = area.RoadShields.Count == 1 ? area.RoadShields[0] : "";
                    var cardinal = area.Cardinals.Count == 1 ? area.Cardinals[0] : "";
                    var destinations = string.Join('|', area.Destinations.Distinct(StringComparer.OrdinalIgnoreCase));
                    var exitMatch = !string.IsNullOrWhiteSpace(ev.ExitNumber) && area.ExitNumbers.Any(x => string.Equals(x, ev.ExitNumber, StringComparison.OrdinalIgnoreCase));
                    var singleShield = area.RoadShields.Count == 1;
                    var laneTextMatch = DirectionTextMatches(area.RawStrings, ev.Instruction);

                    string confidence;
                    string reason;
                    double strength;

                    if (ev.Instruction.StartsWith("EXIT_", StringComparison.Ordinal) && exitMatch && singleShield)
                    {
                        confidence = "HIGH";
                        reason = "SAME_AREA_EXIT+SHIELD";
                        strength = 125;
                    }
                    else if (c.Region == "AFTER" && c.AlongM >= 25 && c.AlongM <= 450 && c.LateralM <= 35 && c.OrientationPerpendicularErrorDeg <= 35 && singleShield)
                    {
                        confidence = "HIGH";
                        reason = "AFTER_SELECTED_CORRIDOR+SINGLE_SHIELD";
                        strength = 112 - Math.Min(25, c.LateralM * 0.5);
                    }
                    else if (exitMatch && !string.IsNullOrWhiteSpace(destinations))
                    {
                        confidence = "MEDIUM";
                        reason = "SAME_AREA_EXIT+DESTINATION";
                        strength = 72;
                    }
                    else if (laneTextMatch && singleShield && c.LateralM <= 55)
                    {
                        confidence = "MEDIUM";
                        reason = "AREA_DIRECTION_TEXT+SINGLE_SHIELD";
                        strength = 68;
                    }
                    else if (singleShield && (area.ArrowTokens.Count > 0 || binding?.LocalX is not null || Math.Abs(area.BoardOffsetX) > 1e-6))
                    {
                        confidence = "LOW";
                        reason = "AREA_STRUCTURED_BUT_BRANCH_UNPROVEN";
                        strength = 28;
                    }
                    else
                    {
                        confidence = "DIAGNOSTIC";
                        reason = "UNASSIGNED_AREA";
                        strength = 5;
                    }

                    result.Add(new V10BranchSignEvidence(
                        result.Count,
                        ev.EventIndex,
                        ev.DecisionNavStationM,
                        ev.Instruction,
                        ev.ExitNumber,
                        c.Sign.Uid,
                        c.Sign.Template,
                        area.AreaName,
                        c.Region,
                        c.AlongM,
                        c.LateralM,
                        c.OrientationPerpendicularErrorDeg,
                        area.BoardToken.Length > 0 ? area.BoardToken : binding?.BoardToken ?? "",
                        area.BoardOffsetX,
                        area.BoardOffsetY,
                        binding?.LocalX,
                        binding?.LocalY,
                        identity,
                        cardinal,
                        destinations,
                        string.Join('|', area.ExitNumbers),
                        string.Join('|', area.ArrowTokens),
                        reason,
                        confidence,
                        strength));
                }
            }
        }
        return result.OrderBy(e => e.EventNavStationM).ThenByDescending(e => e.Strength).ToList();
    }

    private static IReadOnlyList<V10ZoneState> ApplyBranchEvidence(V09SemanticRun run, IReadOnlyList<V10BranchSignEvidence> evidence)
    {
        var states = run.SolvedZones.Select(z => new V10ZoneState(z, z.PrimaryIdentity, z.Aliases, z.IdentityConfidence, z.IdentitySource)).ToList();
        var highByEvent = evidence
            .Where(e => e.Confidence == "HIGH" && !string.IsNullOrWhiteSpace(e.Identity))
            .GroupBy(e => e.EventIndex)
            .ToDictionary(g => g.Key, g => g.OrderByDescending(e => e.Strength).ToList());

        foreach (var (eventIndex, candidates) in highByEvent)
        {
            var identities = candidates.Select(c => c.Identity).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            if (identities.Count != 1) continue;
            var identity = identities[0];
            var afterIndex = states.FindIndex(s => s.Original.Zone.BoundaryBefore?.EventIndex == eventIndex);
            if (afterIndex < 0 || !IsUnknown(states[afterIndex].PrimaryIdentity)) continue;

            states[afterIndex] = states[afterIndex] with
            {
                PrimaryIdentity = identity,
                Confidence = "HIGH",
                Source = "BRANCH_SIGN_HIGH"
            };

            // Continuity may pass through KEEP/FORK guidance, but never through a
            // TURN/EXIT/MERGE without another independent seed.
            for (var i = afterIndex + 1; i < states.Count; i++)
            {
                if (!IsUnknown(states[i].PrimaryIdentity)) break;
                var boundary = states[i].Original.Zone.BoundaryBefore?.Instruction ?? "";
                if (!IsSoftContinuityBoundary(boundary)) break;
                if (states[i].Original.HasStrongDirectEvidence) break;
                states[i] = states[i] with
                {
                    PrimaryIdentity = identity,
                    Confidence = "PROPAGATED",
                    Source = "BRANCH_SIGN_CONTINUITY"
                };
            }
        }
        return states;
    }

    private static IReadOnlyList<V10IdentityInterval> BuildIdentityIntervals(RouteCorridor corridor, V09SemanticRun run, IReadOnlyList<V10ZoneState> states)
    {
        var intervals = new List<V10IdentityInterval>();

        // Primary identity intervals.
        var i = 0;
        while (i < states.Count)
        {
            var start = i;
            var identity = states[i].PrimaryIdentity;
            while (i + 1 < states.Count && string.Equals(states[i + 1].PrimaryIdentity, identity, StringComparison.OrdinalIgnoreCase)) i++;
            var end = i;
            intervals.Add(MakeInterval(intervals.Count, identity, true, start, end, corridor, run, states));
            i++;
        }

        // Aliases/concurrencies get their own spatial intervals and therefore can no
        // longer leak across an entire primary block.
        var aliases = states.SelectMany(s => s.Aliases).Where(a => !string.IsNullOrWhiteSpace(a)).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        foreach (var alias in aliases)
        {
            i = 0;
            while (i < states.Count)
            {
                while (i < states.Count && !states[i].Aliases.Contains(alias, StringComparer.OrdinalIgnoreCase)) i++;
                if (i >= states.Count) break;
                var start = i;
                while (i + 1 < states.Count && states[i + 1].Aliases.Contains(alias, StringComparer.OrdinalIgnoreCase)) i++;
                var end = i;
                intervals.Add(MakeInterval(intervals.Count, alias, false, start, end, corridor, run, states));
                i++;
            }
        }

        return intervals.OrderBy(x => x.StartNavStationM).ThenByDescending(x => x.Primary).ThenBy(x => x.Identity, StringComparer.OrdinalIgnoreCase)
            .Select((x, idx) => x with { IntervalIndex = idx }).ToList();
    }

    private static V10IdentityInterval MakeInterval(
        int index,
        string identity,
        bool primary,
        int start,
        int end,
        RouteCorridor corridor,
        V09SemanticRun run,
        IReadOnlyList<V10ZoneState> states)
    {
        var first = states[start];
        var last = states[end];
        var startNav = first.Original.Zone.StartNavStationM;
        var endNav = last.Original.Zone.EndNavStationM;
        var confidence = IsUnknown(identity) ? "NONE" : states.Skip(start).Take(end - start + 1).Any(s => s.Confidence == "HIGH") ? "HIGH" :
            states.Skip(start).Take(end - start + 1).Any(s => s.Confidence == "MEDIUM") ? "MEDIUM" : "PROPAGATED";
        var sources = string.Join('|', states.Skip(start).Take(end - start + 1).Select(s => s.Source).Where(x => !string.IsNullOrWhiteSpace(x)).Distinct(StringComparer.OrdinalIgnoreCase));
        var cardinal = ResolveIntervalCardinal(corridor, run, identity, startNav, endNav);
        var d = Math.Max(0, endNav - startNav);
        return new V10IdentityInterval(index, identity, primary, first.Original.Zone.ZoneIndex, last.Original.Zone.ZoneIndex, startNav, endNav, d, d / 1609.344,
            cardinal.Cardinal, confidence, cardinal.Confidence, cardinal.Source, sources);
    }

    private static (string Cardinal, string Confidence, string Source) ResolveIntervalCardinal(RouteCorridor corridor, V09SemanticRun run, string identity, double startNav, double endNav)
    {
        if (IsUnknown(identity)) return ("", "NONE", "");
        var existing = run.Blocks
            .Where(b => string.Equals(b.PrimaryIdentity, identity, StringComparison.OrdinalIgnoreCase))
            .Where(b => b.EndNavStationM >= startNav - 1e-6 && b.StartNavStationM <= endNav + 1e-6)
            .Where(b => !string.IsNullOrWhiteSpace(b.Cardinal))
            .GroupBy(b => b.Cardinal, StringComparer.OrdinalIgnoreCase)
            .Select(g => new { Cardinal = g.Key, Weight = g.Sum(b => Math.Max(1, Math.Min(endNav, b.EndNavStationM) - Math.Max(startNav, b.StartNavStationM))), Best = g.Select(b => b.CardinalConfidence).OrderByDescending(ConfidenceRank).FirstOrDefault() ?? "LOW" })
            .OrderByDescending(x => x.Weight)
            .ToList();
        if (existing.Count > 0 && (existing.Count == 1 || existing[0].Weight > existing[1].Weight * 1.5))
            return (existing[0].Cardinal, existing[0].Best, "V09_BLOCK_CARDINAL");

        if (endNav - startNav < 3000) return ("", "LOW", "GEOMETRY_TOO_SHORT");
        var a = PositionAtNavStation(corridor, startNav);
        var b = PositionAtNavStation(corridor, endNav);
        var ga = AtsMapProjection.ToWgs84(a.X, a.Z);
        var gb = AtsMapProjection.ToWgs84(b.X, b.Z);
        var latMid = (ga.Latitude + gb.Latitude) * 0.5 * Math.PI / 180.0;
        var north = (gb.Latitude - ga.Latitude) * 111_320.0;
        var east = (gb.Longitude - ga.Longitude) * 111_320.0 * Math.Cos(latMid);
        var axis = ExpectedAxis(identity);
        string cardinal;
        double dominance;
        if (axis == "NS") { cardinal = north >= 0 ? "NORTH" : "SOUTH"; dominance = Math.Abs(north) / Math.Max(1, Math.Abs(east)); }
        else if (axis == "EW") { cardinal = east >= 0 ? "EAST" : "WEST"; dominance = Math.Abs(east) / Math.Max(1, Math.Abs(north)); }
        else if (Math.Abs(north) >= Math.Abs(east)) { cardinal = north >= 0 ? "NORTH" : "SOUTH"; dominance = Math.Abs(north) / Math.Max(1, Math.Abs(east)); }
        else { cardinal = east >= 0 ? "EAST" : "WEST"; dominance = Math.Abs(east) / Math.Max(1, Math.Abs(north)); }
        if (dominance < 1.15) return ("", "LOW", "GEOMETRY_AXIS_AMBIGUOUS");
        return (cardinal, dominance >= 1.5 ? "HIGH" : "MEDIUM", axis.Length > 0 ? "WGS84_GEOMETRY+ROUTE_NUMBER_AXIS" : "WGS84_GEOMETRY");
    }

    private static IReadOnlyList<V10CruiseSpan> BuildCruiseSpans(IReadOnlyList<V10IdentityInterval> intervals, IReadOnlyList<V09ManeuverEvent> events)
    {
        var primary = intervals.Where(i => i.Primary).OrderBy(i => i.StartNavStationM).ToList();
        var result = new List<V10CruiseSpan>();
        foreach (var interval in primary)
        {
            var inside = events.Where(e => e.DecisionNavStationM > interval.StartNavStationM + 1e-6 && e.DecisionNavStationM <= interval.EndNavStationM + 1e-6).OrderBy(e => e.DecisionNavStationM).ToList();
            var terminal = inside.LastOrDefault(e => Math.Abs(e.DecisionNavStationM - interval.EndNavStationM) < 1.0);
            result.Add(new V10CruiseSpan(
                result.Count,
                interval.IntervalIndex,
                interval.StartZoneIndex,
                interval.EndZoneIndex,
                interval.StartNavStationM,
                interval.EndNavStationM,
                interval.DistanceM,
                interval.DistanceMi,
                IsUnknown(interval.Identity) ? "CONNECTOR" : "CRUISE",
                interval.Identity,
                interval.Cardinal,
                interval.IdentityConfidence,
                interval.CardinalConfidence,
                inside.Count,
                terminal?.Instruction ?? "",
                terminal?.ExitNumber ?? ""));
        }
        return result;
    }

    private static IReadOnlyList<V10GuidanceEvent> BuildGuidanceEvents(
        IReadOnlyList<V09ManeuverEvent> events,
        IReadOnlyList<V10CruiseSpan> spans,
        IReadOnlyList<V10IdentityInterval> intervals,
        IReadOnlyList<V10BranchSignEvidence> branchEvidence)
    {
        var result = new List<V10GuidanceEvent>();
        for (var i = 0; i < events.Count; i++)
        {
            var ev = events[i];
            var before = PrimaryAt(intervals, ev.DecisionNavStationM - 0.01);
            var after = PrimaryAt(intervals, ev.DecisionNavStationM + 0.01);
            var parent = spans.FirstOrDefault(s => ev.DecisionNavStationM > s.StartNavStationM + 1e-6 && ev.DecisionNavStationM <= s.EndNavStationM + 1e-6)
                         ?? spans.FirstOrDefault(s => Math.Abs(ev.DecisionNavStationM - s.StartNavStationM) <= 1e-6);
            var best = branchEvidence.Where(e => e.EventIndex == ev.EventIndex && e.Confidence is "HIGH" or "MEDIUM").OrderByDescending(e => e.Strength).FirstOrDefault();
            var next = i + 1 < events.Count ? events[i + 1] : null;
            result.Add(new V10GuidanceEvent(
                result.Count,
                ev.EventIndex,
                parent?.SpanIndex ?? -1,
                ev.DecisionNavStationM,
                parent is null ? 0 : Math.Max(0, ev.DecisionNavStationM - parent.StartNavStationM),
                ev.Instruction,
                ev.ExitNumber,
                before?.Identity ?? "UNKNOWN",
                after?.Identity ?? "UNKNOWN",
                !string.Equals(before?.Identity ?? "UNKNOWN", after?.Identity ?? "UNKNOWN", StringComparison.OrdinalIgnoreCase),
                best?.Destinations ?? "",
                best?.Confidence ?? "",
                ev.Confidence,
                next?.Instruction ?? "END",
                next is null ? null : Math.Max(0, next.DecisionNavStationM - ev.DecisionNavStationM)));
        }
        return result;
    }

    private static IReadOnlyList<V10CruiseSpan> AttachGuidanceCounts(IReadOnlyList<V10CruiseSpan> spans, IReadOnlyList<V10GuidanceEvent> guidance)
        => spans.Select(s => s with { GuidanceEventCount = guidance.Count(g => g.ParentSpanIndex == s.SpanIndex) }).ToList();

    private static IReadOnlyList<V10PlanEntry> BuildPlanV2(RouteCorridor corridor, IReadOnlyList<V10CruiseSpan> spans, IReadOnlyList<V10GuidanceEvent> guidance)
    {
        var result = new List<V10PlanEntry>();
        foreach (var span in spans)
        {
            var first = guidance.Where(g => g.ParentSpanIndex == span.SpanIndex).OrderBy(g => g.NavStationM).FirstOrDefault();
            result.Add(new V10PlanEntry(
                0,
                span.StartNavStationM,
                "ROUTE_SPAN",
                span.SpanIndex,
                span.SpanType == "CRUISE" ? "STAY_ON" : "CONNECTOR",
                span.RoadIdentity,
                span.Cardinal,
                span.DistanceM,
                span.DistanceMi,
                "",
                "",
                "",
                "",
                first?.Instruction ?? "END",
                first is null ? null : Math.Max(0, first.NavStationM - span.StartNavStationM),
                span.IdentityConfidence,
                $"continuous_span;guidance_events={span.GuidanceEventCount}"));
        }
        foreach (var g in guidance)
        {
            result.Add(new V10PlanEntry(
                0, g.NavStationM, "GUIDANCE", g.ParentSpanIndex, g.Instruction, g.RoadAfter, "", null, null, g.ExitNumber, g.Destination,
                g.RoadBefore, g.RoadAfter, g.NextInstruction, g.DistanceToNextEventM, g.Confidence,
                g.ChangesIdentity ? "route_identity_change" : "intermediate_guidance"));
        }
        result.Add(new V10PlanEntry(0, corridor.TotalNavStationM, "ARRIVAL", spans.LastOrDefault()?.SpanIndex ?? -1, "ARRIVE", spans.LastOrDefault()?.RoadIdentity ?? "UNKNOWN", spans.LastOrDefault()?.Cardinal ?? "", null, null, "", "", "", "", "", null, "", "route_end"));
        return result.OrderBy(e => e.NavStationM).ThenBy(e => e.EntryType == "GUIDANCE" ? 0 : e.EntryType == "ROUTE_SPAN" ? 1 : 2).Select((e, idx) => e with { PlanIndex = idx }).ToList();
    }

    private static V10IdentityInterval? PrimaryAt(IReadOnlyList<V10IdentityInterval> intervals, double nav)
        => intervals.Where(i => i.Primary && nav >= i.StartNavStationM - 1e-6 && nav <= i.EndNavStationM + 1e-6).OrderBy(i => Math.Abs(nav - (i.StartNavStationM + i.EndNavStationM) * 0.5)).FirstOrDefault();

    private static bool DirectionTextMatches(IReadOnlyList<string> raw, string instruction)
    {
        var joined = string.Join(' ', raw).ToUpperInvariant();
        if (instruction.Contains("LEFT", StringComparison.Ordinal) && Regex.IsMatch(joined, @"\bLEFT\b|LEFT\s+\d+\s+LANES?")) return true;
        if (instruction.Contains("RIGHT", StringComparison.Ordinal) && Regex.IsMatch(joined, @"\bRIGHT\b|SECOND\s+RIGHT|RIGHT\s+\d+\s+LANES?")) return true;
        return false;
    }

    private static bool IsSoftContinuityBoundary(string instruction)
        => instruction is "KEEP_LEFT" or "KEEP_RIGHT" or "CONTINUE_AT_FORK" or "FORK_STRAIGHT";

    private static string ExpectedAxis(string identity)
    {
        var m = Regex.Match(identity ?? "", @"^(?<kind>I|US)-(?<num>\d+)$", RegexOptions.IgnoreCase);
        if (!m.Success || !int.TryParse(m.Groups["num"].Value, out var n)) return "";
        if (m.Groups["kind"].Value.Equals("I", StringComparison.OrdinalIgnoreCase) && n >= 100) return "";
        return n % 2 == 0 ? "EW" : "NS";
    }

    private static int ConfidenceRank(string c) => c switch { "HIGH" => 4, "MEDIUM" => 3, "CONCURRENT" => 3, "PROPAGATED" => 2, "LOW" => 1, _ => 0 };
    private static bool IsUnknown(string s) => string.IsNullOrWhiteSpace(s) || s.Equals("UNKNOWN", StringComparison.OrdinalIgnoreCase);

    private static Vector3 PositionAtStation(RouteCorridor corridor, double station)
    {
        if (corridor.Edges.Count == 0) return Vector3.Zero;
        var e = corridor.Edges.FirstOrDefault(x => station >= x.StartStationM - 1e-6 && station <= x.EndStationM + 1e-6) ?? corridor.Edges.OrderBy(x => Math.Min(Math.Abs(station - x.StartStationM), Math.Abs(station - x.EndStationM))).First();
        var denom = e.EndStationM - e.StartStationM;
        var t = denom > 1e-9 ? Math.Clamp((station - e.StartStationM) / denom, 0, 1) : 0;
        return Vector3.Lerp(e.Start, e.End, (float)t);
    }

    private static Vector3 PositionAtNavStation(RouteCorridor corridor, double nav)
    {
        if (corridor.Edges.Count == 0) return Vector3.Zero;
        var e = corridor.Edges.FirstOrDefault(x => nav >= x.StartNavStationM - 1e-6 && nav <= x.EndNavStationM + 1e-6) ?? corridor.Edges.OrderBy(x => Math.Min(Math.Abs(nav - x.StartNavStationM), Math.Abs(nav - x.EndNavStationM))).First();
        var denom = e.EndNavStationM - e.StartNavStationM;
        var t = denom > 1e-9 ? Math.Clamp((nav - e.StartNavStationM) / denom, 0, 1) : 0;
        return Vector3.Lerp(e.Start, e.End, (float)t);
    }

    private static void WriteIdentityIntervals(string path, IReadOnlyList<V10IdentityInterval> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("interval_index,identity,primary,start_zone_index,end_zone_index,start_nav_station_m,end_nav_station_m,distance_m,distance_mi,cardinal,identity_confidence,cardinal_confidence,cardinal_source,sources");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.IntervalIndex, Csv(r.Identity), r.Primary ? 1 : 0, r.StartZoneIndex, r.EndZoneIndex, F(r.StartNavStationM), F(r.EndNavStationM), F(r.DistanceM), F(r.DistanceMi), Csv(r.Cardinal), Csv(r.IdentityConfidence), Csv(r.CardinalConfidence), Csv(r.CardinalSource), Csv(r.Sources)));
    }

    private static void WriteCruiseSpans(string path, IReadOnlyList<V10CruiseSpan> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("span_index,primary_interval_index,start_zone_index,end_zone_index,start_nav_station_m,end_nav_station_m,distance_m,distance_mi,span_type,road_identity,cardinal,identity_confidence,cardinal_confidence,guidance_event_count,terminal_instruction,terminal_exit_number");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.SpanIndex, r.PrimaryIntervalIndex, r.StartZoneIndex, r.EndZoneIndex, F(r.StartNavStationM), F(r.EndNavStationM), F(r.DistanceM), F(r.DistanceMi), Csv(r.SpanType), Csv(r.RoadIdentity), Csv(r.Cardinal), Csv(r.IdentityConfidence), Csv(r.CardinalConfidence), r.GuidanceEventCount, Csv(r.TerminalInstruction), Csv(r.TerminalExitNumber)));
    }

    private static void WriteGuidanceEvents(string path, IReadOnlyList<V10GuidanceEvent> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("guidance_index,event_index,parent_span_index,nav_station_m,offset_in_span_m,instruction,exit_number,road_before,road_after,changes_identity,destination,branch_evidence_confidence,confidence,next_instruction,distance_to_next_event_m");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.GuidanceIndex, r.EventIndex, r.ParentSpanIndex, F(r.NavStationM), F(r.OffsetInSpanM), Csv(r.Instruction), Csv(r.ExitNumber), Csv(r.RoadBefore), Csv(r.RoadAfter), r.ChangesIdentity ? 1 : 0, Csv(r.Destination), Csv(r.BranchEvidenceConfidence), Csv(r.Confidence), Csv(r.NextInstruction), r.DistanceToNextEventM.HasValue ? F(r.DistanceToNextEventM.Value) : ""));
    }

    private static void WriteBranchEvidence(string path, IReadOnlyList<V10BranchSignEvidence> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("evidence_index,event_index,event_nav_station_m,instruction,event_exit_number,sign_uid,template,area_name,region,along_m,lateral_m,orientation_error_deg,board_token,board_offset_x,board_offset_y,template_local_x,template_local_y,identity,cardinal,destinations,area_exit_numbers,arrow_tokens,match_reason,confidence,strength");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.EvidenceIndex, r.EventIndex, F(r.EventNavStationM), Csv(r.Instruction), Csv(r.ExitNumber), Hex(r.SignUid), Csv(r.Template), Csv(r.AreaName), Csv(r.Region), F(r.AlongM), F(r.LateralM), F(r.OrientationErrorDeg), Csv(r.BoardToken), F(r.BoardOffsetX), F(r.BoardOffsetY), r.TemplateLocalX.HasValue ? F(r.TemplateLocalX.Value) : "", r.TemplateLocalY.HasValue ? F(r.TemplateLocalY.Value) : "", Csv(r.Identity), Csv(r.Cardinal), Csv(r.Destinations), Csv(r.AreaExitNumbers), Csv(r.ArrowTokens), Csv(r.MatchReason), Csv(r.Confidence), F(r.Strength)));
    }

    private static void WritePlanV2(string path, IReadOnlyList<V10PlanEntry> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("plan_index,nav_station_m,entry_type,span_index,instruction,road_identity,cardinal,distance_m,distance_mi,exit_number,destination,road_before,road_after,next_instruction,distance_to_next_event_m,confidence,notes");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.PlanIndex, F(r.NavStationM), Csv(r.EntryType), r.SpanIndex, Csv(r.Instruction), Csv(r.RoadIdentity), Csv(r.Cardinal), r.DistanceM.HasValue ? F(r.DistanceM.Value) : "", r.DistanceMi.HasValue ? F(r.DistanceMi.Value) : "", Csv(r.ExitNumber), Csv(r.Destination), Csv(r.RoadBefore), Csv(r.RoadAfter), Csv(r.NextInstruction), r.DistanceToNextEventM.HasValue ? F(r.DistanceToNextEventM.Value) : "", Csv(r.Confidence), Csv(r.Notes)));
    }

    private static StreamWriter CsvWriter(string path) => new(path, false, new System.Text.UTF8Encoding(false));
    private static string F(double v) => v.ToString("0.######", CultureInfo.InvariantCulture);
    private static string Hex(ulong v) => v.ToString("X16", CultureInfo.InvariantCulture);
    private static string Csv(string? value)
    {
        value ??= "";
        if (!value.Contains(',') && !value.Contains('"') && !value.Contains('\n') && !value.Contains('\r')) return value;
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}
