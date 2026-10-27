using System.Globalization;
using System.Numerics;
using System.Text.RegularExpressions;

namespace BetterGPSNavigation;

internal sealed record V11BoardSemantic(
    int BoardIndex,
    int EventIndex,
    double EventNavStationM,
    ulong SignUid,
    string Template,
    string AreaName,
    string EffectiveBoardToken,
    string BoardPath,
    string BoardKind,
    string Region,
    double CorridorAlongM,
    double CorridorLateralM,
    double CorridorOrientationErrorDeg,
    Vector3 SignPosition,
    double SignYawDeg,
    Vector3 BoardAnchorPosition,
    double BoardAnchorYawDeg,
    string BoardAnchorSource,
    string BranchIdentity,
    string Cardinal,
    string Toward,
    string ExitNumbers,
    string ArrowTokens,
    string DirectionHint,
    string RawStrings,
    string SourceAttributes,
    string SemanticConfidence)
{
    public string IdentityConfidence { get; init; } = "NONE";
    public string CardinalConfidence { get; init; } = "NONE";
    public string DestinationConfidence { get; init; } = "NONE";
    public string ExitConfidence { get; init; } = "NONE";
    public string ValidatedToward { get; init; } = "";
    public string ValidationNotes { get; init; } = "";
}

internal sealed record V11BranchBoardMatch(
    int MatchIndex,
    int BoardIndex,
    int EventIndex,
    double EventNavStationM,
    ulong SignUid,
    string AreaName,
    string SelectedBranchId,
    string BestAlternativeBranchId,
    int AlternativeBranchCount,
    double SelectedScore,
    double BestAlternativeScore,
    double Margin,
    double SelectedLateralM,
    double SelectedOrientationErrorDeg,
    string BranchIdentity,
    string Cardinal,
    string Toward,
    string ExitNumbers,
    string MatchReason,
    string Confidence)
{
    public string IdentityConfidence { get; init; } = "NONE";
    public string CardinalConfidence { get; init; } = "NONE";
    public string DestinationConfidence { get; init; } = "NONE";
    public string ExitConfidence { get; init; } = "NONE";
    public string ValidatedToward { get; init; } = "";
    public string ValidationNotes { get; init; } = "";
}

internal sealed record V11DriverAction(
    int ActionIndex,
    int EventIndex,
    int SegmentIndex,
    double NavStationM,
    string PreliminaryClass,
    string OriginalInstruction,
    string Instruction,
    bool ActionRequired,
    string SelectedBranchId,
    int InputLaneCount,
    string ValidLaneOrdinals,
    string ActiveLaneOrdinals,
    bool AllInputLanesReachSelectedExit,
    bool ExitValidated,
    string ExitNumber,
    string ActionReason,
    string BranchConfidence,
    string ActionConfidence,
    string ThenInstruction,
    double? DistanceToThenM);

internal sealed record V11ZoneState(
    V09SolvedZone Original,
    string PrimaryIdentity,
    IReadOnlyList<string> Aliases,
    string Confidence,
    string Source);

internal sealed record V11IdentityInterval(
    int IntervalIndex,
    string Identity,
    bool Primary,
    int StartZoneIndex,
    int EndZoneIndex,
    double StartNavStationM,
    double EndNavStationM,
    double DistanceM,
    double DistanceMi,
    string SignedCardinal,
    string IdentityConfidence,
    string CardinalConfidence,
    string CardinalSource,
    double TravelBearingDeg,
    string ExpectedAxisHint,
    string Sources);

internal sealed record V11CruiseSpan(
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
    string SignedCardinal,
    double TravelBearingDeg,
    string IdentityConfidence,
    string CardinalConfidence,
    int GuidanceEventCount,
    string TerminalInstruction,
    string TerminalExitNumber);

internal sealed record V11GuidanceEvent(
    int GuidanceIndex,
    int EventIndex,
    int ParentSpanIndex,
    double NavStationM,
    double OffsetInSpanM,
    string Instruction,
    string ExitNumber,
    string RoadBefore,
    string RoadAfter,
    string CardinalAfter,
    bool ChangesIdentity,
    string DestinationToward,
    string SelectedBranchId,
    string BranchAssociationConfidence,
    string ActionConfidence,
    string RoadIdentityConfidence,
    string CardinalConfidence,
    string DestinationConfidence,
    string NextInstruction,
    double? DistanceToNextEventM);

internal sealed record V11PlanEntry(
    int PlanIndex,
    double NavStationM,
    string EntryType,
    int SpanIndex,
    string Instruction,
    string RoadIdentity,
    string SignedCardinal,
    double TravelBearingDeg,
    double? DistanceM,
    double? DistanceMi,
    string ExitNumber,
    string DestinationToward,
    string RoadBefore,
    string RoadAfter,
    string SelectedBranchId,
    string NextInstruction,
    double? DistanceToNextEventM,
    string ActionConfidence,
    string IdentityConfidence,
    string CardinalConfidence,
    string DestinationConfidence,
    string Notes)
{
    public string ExitConfidence { get; init; } = "NONE";
}

internal sealed record V11Summary(
    int IdentityIntervals,
    int PrimaryIntervals,
    int AliasIntervals,
    int NamedPrimaryIntervals,
    int CruiseSpans,
    int NamedCruiseSpans,
    int ConnectorSpans,
    int GuidanceEvents,
    int SuppressedEvents,
    int BoardSemantics,
    int BranchBoardMatches,
    int HighBranchBoardMatches,
    int AmbiguousBranchBoardMatches,
    int ConflictingBranchBoardMatches,
    int BranchPromotedZones,
    int HighCardinalIntervals,
    int GeometryDerivedSignedCardinals,
    int MultiHighIdentityEvents,
    int DestinationsWithoutHighBranchMatch,
    int UnnecessaryStraightKeeps,
    int HighMatchesBelowRequiredMargin,
    int PlanEntries,
    double NamedNavCoverageM,
    double TotalNavCoverageM)
{
    public double NamedCoverageRatio => TotalNavCoverageM > 0 ? NamedNavCoverageM / TotalNavCoverageM : 0;
    public int SemanticFieldFailures { get; init; }
}

/// <summary>
/// v0.11 evidence-driven navigation synthesis.
///
/// The selected ATS route remains the routing oracle. v0.11 adds four conservative
/// layers on top of v0.10:
///  - exact per-decision alternative PPD branches;
///  - driver-action gating based on exact immediate lane reachability;
///  - AreaName/board semantics matched comparatively against selected + alternatives;
///  - signed cardinal directions only from explicit semantic anchors, never geometry.
/// </summary>
internal sealed class NavigationPlanV11
{
    public LiveRoute? LastLiveRoute { get; private set; }
    public IReadOnlyList<V11BoardSemantic> AuditBoards { get; private set; } = [];
    public IReadOnlyDictionary<int,DecisionBranchSet> AuditBranches { get; private set; } = new Dictionary<int,DecisionBranchSet>();
    public IReadOnlyList<V11BranchBoardMatch> AuditMatches { get; private set; } = [];
    private readonly SignRoadIdentityResolver signResolver;
    private readonly SignBoardResolver boardResolver;
    private readonly DecisionBranchResolver branchResolver;
    private readonly SignPanelExtractor? panelExtractor;

    public NavigationPlanV11(
        SignRoadIdentityResolver signResolver,
        SignBoardResolver boardResolver,
        DecisionBranchResolver branchResolver, SignPanelExtractor? panelExtractor = null)
    {
        this.signResolver = signResolver;
        this.boardResolver = boardResolver;
        this.branchResolver = branchResolver;
        this.panelExtractor = panelExtractor;
    }

    public V11Summary Analyze(
        ProbeRoute route,
        RouteCorridor corridor,
        V09SemanticRun run,
        string driverActionsPath,
        string boardSemanticsPath,
        string branchBoardMatchesPath,
        string identityIntervalsPath,
        string cruiseSpansPath,
        string guidanceEventsPath,
        string betterGpsPlanPath,
        string regressionPath,
        bool writeReports = true)
    {
        var branchSets = run.Events.ToDictionary(e => e.EventIndex, e => branchResolver.Resolve(route, e));
        var boards = BuildBoardSemantics(corridor, run.Events);
        var matches = BuildBranchBoardMatches(run.Events, branchSets, boards);
        AuditBoards=boards;AuditBranches=branchSets;AuditMatches=matches;
        var actions = BuildDriverActions(run.Events, branchSets, matches);
        var states = ApplyBranchEvidence(run, matches);
        var intervals = BuildIdentityIntervals(corridor, run, states, matches);
        var spans = BuildCruiseSpans(intervals, actions);
        var guidance = BuildGuidanceEvents(actions, spans, intervals, matches);
        spans = spans.Select(s => s with { GuidanceEventCount = guidance.Count(g => g.ParentSpanIndex == s.SpanIndex) }).ToList();
        var plan = BuildPlan(corridor, spans, guidance);

        if (writeReports)
        {
            WriteDriverActions(driverActionsPath, actions);
            WriteBoardSemantics(boardSemanticsPath, boards);
            WriteBranchMatches(branchBoardMatchesPath, matches);
            WriteIntervals(identityIntervalsPath, intervals);
            WriteSpans(cruiseSpansPath, spans);
            WriteGuidance(guidanceEventsPath, guidance);
            WritePlan(betterGpsPlanPath, plan);
        }

        var multiHighIdentityEvents = matches
            .Where(m => m.Confidence == "HIGH" && !string.IsNullOrWhiteSpace(m.BranchIdentity))
            .GroupBy(m => m.EventIndex)
            .Count(g => g.Select(x => x.BranchIdentity).Distinct(StringComparer.OrdinalIgnoreCase).Count() > 1);
        var geometryCardinals = intervals.Count(i => i.CardinalSource.Contains("GEOMETRY", StringComparison.OrdinalIgnoreCase) && i.CardinalConfidence == "HIGH");
        var destinationsWithoutHigh = guidance.Count(g => !string.IsNullOrWhiteSpace(g.DestinationToward) &&
            (g.DestinationConfidence != "HIGH" || Split(g.DestinationToward).Any(d =>
                !signResolver.IsKnownDestination(d) || !SignTextClassifier.IsDestinationCandidate(d) ||
                !matches.Any(m => m.EventIndex == g.EventIndex && m.Confidence == "HIGH" && Split(m.ValidatedToward).Contains(d, StringComparer.OrdinalIgnoreCase)))));
        var semanticFailures = destinationsWithoutHigh + intervals.Count(i => i.IdentityConfidence == "HIGH" && !SignTextClassifier.IsRoadIdentity(i.Identity)) + matches.Count(m =>
            (m.IdentityConfidence == "HIGH" && (m.Confidence != "HIGH" || !SignTextClassifier.IsRoadIdentity(m.BranchIdentity))) ||
            (m.CardinalConfidence == "HIGH" && (m.IdentityConfidence != "HIGH" || !new[] { "NORTH", "SOUTH", "EAST", "WEST" }.Contains(m.Cardinal))) ||
            (m.DestinationConfidence == "HIGH" && (m.Confidence != "HIGH" || Split(m.Toward).Length == 0 ||
                Split(m.Toward).Any(d => !signResolver.IsKnownDestination(d) || !SignTextClassifier.IsDestinationCandidate(d)))) ||
            (m.ExitConfidence == "HIGH" && (m.Confidence != "HIGH" || Split(m.ExitNumbers).Length != 1 || !SignTextClassifier.IsExitNumber(m.ExitNumbers))));
        var keepAllLanes = actions.Count(a => a.ActionRequired && a.PreliminaryClass == "FORK_STRAIGHT" && a.Instruction.StartsWith("KEEP_", StringComparison.Ordinal) && a.AllInputLanesReachSelectedExit);
        var highBelowMargin = matches.Count(m => m.Confidence == "HIGH" && m.AlternativeBranchCount > 0 && m.Margin < 18);
        if (writeReports)
            WriteRegression(regressionPath, actions, matches, intervals, multiHighIdentityEvents, geometryCardinals, destinationsWithoutHigh, keepAllLanes, highBelowMargin, semanticFailures);
        LastLiveRoute = null;
        if (!writeReports)
        {
            if (geometryCardinals + destinationsWithoutHigh + keepAllLanes + highBelowMargin + semanticFailures != 0)
                throw new InvalidDataException("Live semantic guards failed.");
            LastLiveRoute = LiveCaptureCompiler.FromMemory(route.Stem,corridor,actions,boards,matches);
        }

        var namedCoverage = intervals.Where(i => i.Primary && !IsUnknown(i.Identity)).Sum(i => i.DistanceM);
        return new V11Summary(
            intervals.Count,
            intervals.Count(i => i.Primary),
            intervals.Count(i => !i.Primary),
            intervals.Count(i => i.Primary && !IsUnknown(i.Identity)),
            spans.Count,
            spans.Count(s => !IsUnknown(s.RoadIdentity)),
            spans.Count(s => IsUnknown(s.RoadIdentity)),
            guidance.Count,
            actions.Count(a => !a.ActionRequired),
            boards.Count,
            matches.Count,
            matches.Count(m => m.Confidence == "HIGH"),
            matches.Count(m => m.Confidence == "AMBIGUOUS"),
            matches.Count(m => m.Confidence == "CONFLICT"),
            states.Count(s => s.Source == "BRANCH_BOARD_HIGH"),
            intervals.Count(i => i.CardinalConfidence == "HIGH"),
            geometryCardinals,
            multiHighIdentityEvents,
            destinationsWithoutHigh,
            keepAllLanes,
            highBelowMargin,
            plan.Count,
            namedCoverage,
            corridor.TotalNavStationM) { SemanticFieldFailures = semanticFailures };
    }

    private IReadOnlyList<V11BoardSemantic> BuildBoardSemantics(RouteCorridor corridor, IReadOnlyList<V09ManeuverEvent> events)
    {
        var result = new List<V11BoardSemantic>();
        foreach (var ev in events)
        {
            var center = PositionAtStation(corridor, ev.DecisionStationM);
            var candidates = signResolver.FindAllAlongCorridor(corridor, ev.DecisionStationM, center, 900).Take(48).ToList();
            foreach (var c in candidates)
            {
                foreach (var area in (panelExtractor?.Areas(c.Sign) ?? c.Sign.Areas).Where(a => a.HasSemanticData || a.RawStrings.Count > 0))
                {
                    var boardToken = boardResolver.EffectiveBoardToken(c.Sign, area);
                    var boardInfo = boardResolver.Find(boardToken);
                    var identity = area.RoadShields.Count == 1 ? area.RoadShields[0] : boardResolver.InferShieldIdentity(c.Sign, area);
                    var cardinal = area.Cardinals.Count == 1 ? area.Cardinals[0] : "";
                    var toward = string.Join('|', area.Destinations.Distinct(StringComparer.OrdinalIgnoreCase));
                    var exitNumbers = string.Join('|', area.ExitNumbers.Distinct(StringComparer.OrdinalIgnoreCase));
                    var arrows = string.Join('|', area.ArrowTokens.Distinct(StringComparer.OrdinalIgnoreCase));
                    // Extracted arrows are candidates until the texture and grouping are verified.
                    // Lane instructions are not turn arrows.
                    var directionHint = area.Extraction is null ? SignTextClassifier.DirectionHint(area.RawStrings.Concat(area.ArrowTokens)) : "";
                    var attrs = string.Join('|', area.Attributes.Select(a => $"{a.ObjectId}:{a.AttributeIndex}:{a.ValueType}:{a.Value}"));
                    var fields = SignSemanticValidator.Validate(area, boardInfo, identity, cardinal, signResolver.IsKnownDestination);
                    // Kept for CSV compatibility; this aggregate must not promote any
                    // individual field. Consumers below use the field-specific values.
                    var semanticConfidence = new[] { fields.IdentityConfidence, fields.CardinalConfidence, fields.DestinationConfidence, fields.ExitConfidence }.Contains("HIGH")
                        ? "HIGH" : area.HasSemanticData ? "MEDIUM" : "LOW";
                    // Map items give one physical anchor for the whole sign. Until a
                    // model-locator parser is validated against TruckLib.Models, multi-board
                    // signs sharing this anchor are deliberately kept AMBIGUOUS instead of
                    // pretending every AreaName has its own proven world position.
                    var anchorPosition = c.Sign.Position;
                    var anchorYawDeg = c.Sign.YawDeg;
                    var anchorSource = "SIGN_NODE";

                    result.Add(new V11BoardSemantic(
                        result.Count,
                        ev.EventIndex,
                        ev.DecisionNavStationM,
                        c.Sign.Uid,
                        c.Sign.Template,
                        area.AreaName,
                        boardToken,
                        boardInfo?.BoardPath ?? "",
                        boardInfo?.SemanticKind ?? "UNKNOWN",
                        c.Region,
                        c.AlongM,
                        c.LateralM,
                        c.OrientationPerpendicularErrorDeg,
                        c.Sign.Position,
                        c.Sign.YawDeg,
                        anchorPosition,
                        anchorYawDeg,
                        anchorSource,
                        identity,
                        cardinal,
                        toward,
                        exitNumbers,
                        arrows,
                        directionHint,
                        string.Join('|', area.RawStrings),
                        attrs,
                        semanticConfidence)
                    {
                        IdentityConfidence = fields.IdentityConfidence,
                        CardinalConfidence = fields.CardinalConfidence,
                        DestinationConfidence = fields.DestinationConfidence,
                        ExitConfidence = fields.ExitConfidence,
                        ValidatedToward = fields.ValidatedToward,
                        ValidationNotes = fields.Notes
                    });
                }
            }
        }
        return result;
    }

    private IReadOnlyList<V11BranchBoardMatch> BuildBranchBoardMatches(
        IReadOnlyList<V09ManeuverEvent> events,
        IReadOnlyDictionary<int, DecisionBranchSet> branchSets,
        IReadOnlyList<V11BoardSemantic> boards)
    {
        var eventById = events.ToDictionary(e => e.EventIndex);
        var result = new List<V11BranchBoardMatch>();
        foreach (var board in boards)
        {
            var hasEvent = eventById.TryGetValue(board.EventIndex, out var ev);
            var hasBranchSet = branchSets.TryGetValue(board.EventIndex, out var set);
            if (!hasEvent || !hasBranchSet || set is null || set.Status != "OK")
            {
                result.Add(new V11BranchBoardMatch(result.Count, board.BoardIndex, board.EventIndex, board.EventNavStationM, board.SignUid, board.AreaName,
                    set?.SelectedBranchId ?? "", "", set?.Branches.Count(b => !b.Selected) ?? 0, 0, 0, 0, double.NaN, double.NaN,
                    board.BranchIdentity, board.Cardinal, board.Toward, board.ExitNumbers, "BRANCH_GEOMETRY_UNAVAILABLE", "LOW"));
                continue;
            }
            var selected = set.Branches.SingleOrDefault(b => b.Selected);
            if (selected is null) continue;

            var selectedProjection = DecisionBranchResolver.Project(board.BoardAnchorPosition, board.BoardAnchorYawDeg, selected);
            var selectedScore = ScoreBoardForBranch(board, selected, selectedProjection);
            var alternatives = set.Branches.Where(b => !b.Selected)
                .Select(b => new { Branch = b, Projection = DecisionBranchResolver.Project(board.BoardAnchorPosition, board.BoardAnchorYawDeg, b) })
                .Select(x => new { x.Branch, x.Projection, Score = ScoreBoardForBranch(board, x.Branch, x.Projection) })
                .OrderByDescending(x => x.Score)
                .ToList();
            var bestAlt = alternatives.FirstOrDefault();
            var altScore = bestAlt?.Score ?? 0;
            var margin = alternatives.Count == 0 ? selectedScore : selectedScore - altScore;
            var semanticStrong = !string.IsNullOrWhiteSpace(board.BranchIdentity) || !string.IsNullOrWhiteSpace(board.Toward) || !string.IsNullOrWhiteSpace(board.ExitNumbers);
            var geometryGood = board.Region != "APPROACH" && selectedProjection is not null && selectedProjection.LateralM <= 55 && selectedProjection.OrientationPerpendicularErrorDeg <= 40;
            var approachDirectionGood = board.Region == "APPROACH" && !string.IsNullOrWhiteSpace(board.DirectionHint) && board.DirectionHint == selected.Side;
            var branchDiscriminatorGood = geometryGood || approachDirectionGood;
            var semanticAreasOnSamePhysicalSign = boards.Count(b => b.EventIndex == board.EventIndex && b.SignUid == board.SignUid &&
                (!string.IsNullOrWhiteSpace(b.BranchIdentity) || !string.IsNullOrWhiteSpace(b.Toward) || !string.IsNullOrWhiteSpace(b.ExitNumbers)));
            var areaHasPhysicalAnchor = board.BoardAnchorSource == "MODEL_LOCATOR" || semanticAreasOnSamePhysicalSign <= 1;

            string confidence;
            string reason;
            if (bestAlt is not null && bestAlt.Score > selectedScore + 8 && bestAlt.Score >= 58)
            {
                confidence = "CONFLICT";
                reason = "ALTERNATIVE_BRANCH_SCORES_BETTER";
            }
            else if (bestAlt is not null && Math.Abs(margin) < 12 && Math.Max(selectedScore, altScore) >= 55)
            {
                confidence = "AMBIGUOUS";
                reason = "SELECTED_NOT_UNIQUE_VS_ALTERNATIVE";
            }
            else if (!areaHasPhysicalAnchor && semanticStrong && semanticAreasOnSamePhysicalSign > 1 && selectedScore >= 55)
            {
                confidence = "AMBIGUOUS";
                reason = "MULTI_BOARD_SIGN_WITHOUT_MODEL_LOCATOR";
            }
            else if (semanticStrong && branchDiscriminatorGood && areaHasPhysicalAnchor && selectedScore >= 72 && (alternatives.Count == 0 || margin >= 18))
            {
                confidence = "HIGH";
                reason = alternatives.Count == 0 ? "ONLY_BRANCH+SEMANTIC_DISCRIMINATOR" : "SELECTED_BRANCH_UNIQUE_MARGIN";
            }
            else if (semanticStrong && selectedScore >= 58 && (alternatives.Count == 0 || margin >= 8))
            {
                confidence = "MEDIUM";
                reason = "SELECTED_BRANCH_PROBABLE";
            }
            else
            {
                confidence = "LOW";
                reason = "BRANCH_NOT_PROVEN";
            }

            // A better spline must not promote a sign-node proximity score into calibrated ownership.
            // Current extraction does not establish renderer origin/front face or a physical lane assignment.
            if(confidence=="HIGH"&&board.BoardAnchorSource!="CALIBRATED_LANE_CHAIN")
            {
                confidence="MEDIUM";
                reason="SIGN_LANE_OWNERSHIP_NOT_CALIBRATED";
            }

            result.Add(new V11BranchBoardMatch(
                result.Count,
                board.BoardIndex,
                board.EventIndex,
                board.EventNavStationM,
                board.SignUid,
                board.AreaName,
                selected.BranchId,
                bestAlt?.Branch.BranchId ?? "",
                alternatives.Count,
                selectedScore,
                altScore,
                margin,
                selectedProjection?.LateralM ?? double.NaN,
                selectedProjection?.OrientationPerpendicularErrorDeg ?? double.NaN,
                board.BranchIdentity,
                board.Cardinal,
                board.Toward,
                board.ExitNumbers,
                reason,
                confidence)
            {
                IdentityConfidence = SignSemanticValidator.Combine(confidence, board.IdentityConfidence),
                CardinalConfidence = SignSemanticValidator.Combine(confidence, board.CardinalConfidence),
                DestinationConfidence = SignSemanticValidator.Combine(confidence, board.DestinationConfidence),
                ExitConfidence = SignSemanticValidator.Combine(confidence, board.ExitConfidence),
                ValidatedToward = confidence == "HIGH" ? board.ValidatedToward : "",
                ValidationNotes = board.ValidationNotes
            });
        }
        return result.OrderBy(x => x.EventNavStationM).ThenByDescending(x => x.SelectedScore).ToList();
    }

    private static double ScoreBoardForBranch(
        V11BoardSemantic board,
        DecisionBranchGeometry branch,
        BranchProjection? projection)
    {
        // IMPORTANT: this function is deliberately symmetric. It must not know which
        // branch ATS selected, otherwise the selected-vs-alternative margin would be
        // circular evidence rather than a real discriminator.
        double score;
        if (board.Region == "APPROACH")
        {
            // Before the split, the physical sign/gantry belongs to the decision rather
            // than to any outgoing road. Outgoing-branch proximity is therefore not
            // evidence. A board can become branch-specific here only when its own
            // AreaName semantics expose a direction that distinguishes the branch.
            score = 24.0;
            if (!string.IsNullOrWhiteSpace(board.DirectionHint))
            {
                if (board.DirectionHint == branch.Side) score += 72;
                else if (branch.Side == "STRAIGHT") score -= 24;
                else score -= 42;
            }
            // A very near board can receive a small non-decisive geometry bonus, but
            // geometry alone can never satisfy the HIGH gate for APPROACH.
            if (projection is not null && projection.LateralM <= 80) score += 6;
            return score;
        }

        if (projection is null) return -100;
        score = 100.0;
        score -= Math.Min(120, projection.LateralM * 1.55);
        score -= Math.Min(55, projection.OrientationPerpendicularErrorDeg * 0.55);

        if (!string.IsNullOrWhiteSpace(board.DirectionHint))
        {
            if (board.DirectionHint == branch.Side) score += 18;
            else if (branch.Side != "STRAIGHT") score -= 16;
        }
        return score;
    }

    private IReadOnlyList<V11DriverAction> BuildDriverActions(
        IReadOnlyList<V09ManeuverEvent> events,
        IReadOnlyDictionary<int, DecisionBranchSet> branchSets,
        IReadOnlyList<V11BranchBoardMatch> matches)
    {
        var draft = new List<V11DriverAction>();
        foreach (var ev in events)
        {
            branchSets.TryGetValue(ev.EventIndex, out var set);
            var inputCount = set?.InputLaneCount ?? 0;
            var valid = set?.SelectedLaneOrdinals.OrderBy(x => x).ToList() ?? [];
            var active = valid.ToList();
            var all = set?.AllInputLanesReachSelectedExit == true;
            var original = ev.Instruction;
            var instruction = original;
            var actionRequired = true;
            var exitValidated = false;
            var reason = "PPD_MANEUVER";

            var high = matches.Where(m => m.EventIndex == ev.EventIndex && m.Confidence == "HIGH").ToList();
            var highExitNumbers = high
                .Where(m => m.ExitConfidence == "HIGH")
                .SelectMany(m => Split(m.ExitNumbers))
                .Where(x => !string.IsNullOrWhiteSpace(x))
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .ToList();
            var validatedExitNumber = highExitNumbers.Count == 1 ? highExitNumbers[0] : "";

            if (original.StartsWith("EXIT_", StringComparison.Ordinal))
            {
                var oldNumberAgrees = string.IsNullOrWhiteSpace(ev.ExitNumber) ||
                                      string.Equals(ev.ExitNumber, validatedExitNumber, StringComparison.OrdinalIgnoreCase);
                exitValidated = validatedExitNumber.Length > 0 && oldNumberAgrees;
                if (!exitValidated)
                {
                    // Exit topology does not depend on an optional exit number.
                    instruction = original;
                    reason = highExitNumbers.Count > 1
                        ? "EXIT_WITHOUT_NUMBER_MULTIPLE_CANDIDATES"
                        : (!string.IsNullOrWhiteSpace(ev.ExitNumber) && validatedExitNumber.Length > 0 && !oldNumberAgrees)
                            ? "EXIT_WITHOUT_NUMBER_CONFLICT"
                            : "EXIT_WITHOUT_VALIDATED_NUMBER";
                }
                else reason = "EXPLICIT_EXIT_BOARD_BRANCH_UNIQUE";
            }
            else if (ev.PreliminaryClass.StartsWith("BRANCH_", StringComparison.Ordinal) &&
                     instruction.StartsWith("KEEP_", StringComparison.Ordinal) &&
                     validatedExitNumber.Length > 0)
            {
                // v0.10 may have conservatively emitted KEEP because the old whole-sign
                // matcher could not prove an exit. v0.11 is allowed to upgrade only after
                // the board itself is HIGH and branch-unique.
                instruction = instruction.EndsWith("LEFT", StringComparison.Ordinal) ? "EXIT_LEFT" : "EXIT_RIGHT";
                exitValidated = true;
                reason = "KEEP_UPGRADED_BY_BRANCH_UNIQUE_EXIT_BOARD";
            }

            if (all && ev.PreliminaryClass == "FORK_STRAIGHT" &&
                (instruction.StartsWith("KEEP_", StringComparison.Ordinal) || instruction is "CONTINUE_AT_FORK" or "FORK_STRAIGHT"))
            {
                // All lanes reaching a THROUGH target means no lane-position action.
                // Do NOT apply this rule to BRANCH_LEFT/RIGHT: even if every lane can
                // reach that target, the driver still has to choose the diverging fork.
                instruction = "SUPPRESS";
                actionRequired = false;
                reason = "ALL_INPUT_LANES_REACH_STRAIGHT_SELECTED_BRANCH";
            }
            else if (instruction is "CONTINUE_AT_FORK" or "FORK_STRAIGHT")
            {
                var laneInstruction = LaneInstruction(inputCount, valid);
                instruction = laneInstruction;
                actionRequired = laneInstruction != "SUPPRESS";
                reason = laneInstruction == "USE_SELECTED_LANES" ? "BRANCH_CHOICE_NON_EDGE_LANES" : "BRANCH_CHOICE_LANE_POSITION";
            }

            var branchConfidence = set?.Status == "OK" ? "EXACT" : "LOW";
            var actionConfidence = branchConfidence != "EXACT" ? "LOW" : !actionRequired ? "HIGH" :
                instruction.StartsWith("TURN_", StringComparison.Ordinal) || instruction.StartsWith("SHARP_", StringComparison.Ordinal) || instruction.StartsWith("U_TURN_", StringComparison.Ordinal) || instruction.StartsWith("MERGE_", StringComparison.Ordinal) || instruction == "MERGE" ? "HIGH" :
                instruction.StartsWith("EXIT_", StringComparison.Ordinal) ? "HIGH" :
                instruction.StartsWith("KEEP_", StringComparison.Ordinal) || instruction == "USE_SELECTED_LANES" ? "HIGH" : "MEDIUM";

            draft.Add(new V11DriverAction(
                draft.Count, ev.EventIndex, ev.SegmentIndex, ev.DecisionNavStationM, ev.PreliminaryClass, original, instruction, actionRequired,
                set?.SelectedBranchId ?? "", inputCount, JoinInts(valid), JoinInts(active), all, exitValidated, exitValidated ? validatedExitNumber : "", reason, branchConfidence, actionConfidence, "", null));
        }

        // v0.11 deliberately keeps ActiveLaneOrdinals equal to the immediately
        // valid exact-lane set. Cross-item lane continuation is not promoted to
        // EXACT until we can geometrically match lane endpoints across every road /
        // transparent prefab between two decisions. This avoids inventing early lane
        // positioning from ordinal equality alone.

        for (var i = 0; i < draft.Count; i++)
        {
            if (!string.IsNullOrWhiteSpace(draft[i].ThenInstruction)) continue;
            var next = draft.Skip(i + 1).FirstOrDefault(a => a.ActionRequired);
            if (next is not null)
                draft[i] = draft[i] with { ThenInstruction = next.Instruction, DistanceToThenM = Math.Max(0, next.NavStationM - draft[i].NavStationM) };
            draft[i] = draft[i] with { ActionIndex = i };
        }
        return draft;
    }

    private static IReadOnlyList<V11ZoneState> ApplyBranchEvidence(V09SemanticRun run, IReadOnlyList<V11BranchBoardMatch> matches)
    {
        var states = run.SolvedZones.Select(z => new V11ZoneState(z, z.PrimaryIdentity, z.Aliases, z.IdentityConfidence, z.IdentitySource)).ToList();
        var highByEvent = matches
            .Where(m => m.IdentityConfidence == "HIGH" && !string.IsNullOrWhiteSpace(m.BranchIdentity))
            .GroupBy(m => m.EventIndex)
            .ToDictionary(g => g.Key, g => g.OrderByDescending(x => x.SelectedScore).ToList());

        foreach (var (eventIndex, candidates) in highByEvent)
        {
            var identities = candidates.Select(c => c.BranchIdentity).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            if (identities.Count == 0) continue;
            var afterIndex = states.FindIndex(s => s.Original.Zone.BoundaryBefore?.EventIndex == eventIndex);
            if (afterIndex < 0) continue;

            var after = states[afterIndex];
            string? primary = null;
            if (!IsUnknown(after.PrimaryIdentity) && identities.Contains(after.PrimaryIdentity, StringComparer.OrdinalIgnoreCase))
                primary = after.PrimaryIdentity;
            else if (afterIndex > 0 && !IsUnknown(states[afterIndex - 1].PrimaryIdentity) && identities.Contains(states[afterIndex - 1].PrimaryIdentity, StringComparer.OrdinalIgnoreCase) && IsSoftContinuityBoundary(after.Original.Zone.BoundaryBefore?.Instruction ?? ""))
                primary = states[afterIndex - 1].PrimaryIdentity;
            else if (identities.Count == 1)
                primary = identities[0];

            if (primary is null)
            {
                // Multiple branch-unique route shields can be a legitimate concurrency.
                // Preserve them as aliases, but do not invent which one is primary.
                states[afterIndex] = after with
                {
                    Aliases = after.Aliases.Concat(identities).Distinct(StringComparer.OrdinalIgnoreCase).ToList(),
                    Source = "BRANCH_BOARD_MULTI_IDENTITY"
                };
                continue;
            }

            var aliases = after.Aliases.Concat(identities.Where(x => !x.Equals(primary, StringComparison.OrdinalIgnoreCase))).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            states[afterIndex] = after with { PrimaryIdentity = primary, Aliases = aliases, Confidence = "HIGH", Source = identities.Count > 1 ? "BRANCH_BOARD_CONCURRENCY" : "BRANCH_BOARD_HIGH" };
            for (var i = afterIndex + 1; i < states.Count; i++)
            {
                if (!IsUnknown(states[i].PrimaryIdentity)) break;
                var boundary = states[i].Original.Zone.BoundaryBefore?.Instruction ?? "";
                if (!IsSoftContinuityBoundary(boundary)) break;
                if (states[i].Original.HasStrongDirectEvidence) break;
                states[i] = states[i] with { PrimaryIdentity = primary, Confidence = "MEDIUM", Source = "BRANCH_BOARD_CONTINUITY" };
            }
        }
        return states;
    }

    private IReadOnlyList<V11IdentityInterval> BuildIdentityIntervals(
        RouteCorridor corridor,
        V09SemanticRun run,
        IReadOnlyList<V11ZoneState> states,
        IReadOnlyList<V11BranchBoardMatch> matches)
    {
        var intervals = new List<V11IdentityInterval>();
        var i = 0;
        while (i < states.Count)
        {
            var start = i;
            var identity = states[i].PrimaryIdentity;
            while (i + 1 < states.Count && string.Equals(states[i + 1].PrimaryIdentity, identity, StringComparison.OrdinalIgnoreCase)) i++;
            intervals.Add(MakeInterval(intervals.Count, identity, true, start, i, corridor, run, states, matches));
            i++;
        }
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
                intervals.Add(MakeInterval(intervals.Count, alias, false, start, i, corridor, run, states, matches));
                i++;
            }
        }
        return intervals.OrderBy(x => x.StartNavStationM).ThenByDescending(x => x.Primary).ThenBy(x => x.Identity, StringComparer.OrdinalIgnoreCase)
            .Select((x, idx) => x with { IntervalIndex = idx }).ToList();
    }

    private V11IdentityInterval MakeInterval(
        int index,
        string identity,
        bool primary,
        int start,
        int end,
        RouteCorridor corridor,
        V09SemanticRun run,
        IReadOnlyList<V11ZoneState> states,
        IReadOnlyList<V11BranchBoardMatch> matches)
    {
        var first = states[start]; var last = states[end];
        var startNav = first.Original.Zone.StartNavStationM; var endNav = last.Original.Zone.EndNavStationM;
        // An alias cannot borrow the primary route's HIGH confidence. Require a
        // direct, identity-specific anchor inside this interval for either role.
        var hasIdentityAnchor = SignTextClassifier.IsRoadIdentity(identity) &&
            (run.Evidence.Any(e => e.Confidence == "HIGH" && e.Identity.Equals(identity, StringComparison.OrdinalIgnoreCase) &&
                e.NavStationM >= startNav - 1e-6 && e.NavStationM <= endNav + 1e-6) ||
             matches.Any(m => m.IdentityConfidence == "HIGH" && m.BranchIdentity.Equals(identity, StringComparison.OrdinalIgnoreCase) &&
                m.EventNavStationM >= startNav - 2 && m.EventNavStationM <= endNav + 2));
        var allZonesProven=states.Skip(start).Take(end-start+1).All(s=>s.Confidence=="HIGH"&&
            run.Evidence.Any(e=>e.Confidence=="HIGH"&&e.Identity.Equals(identity,StringComparison.OrdinalIgnoreCase)&&
                e.SegmentIndex>=s.Original.Zone.StartSegmentIndex&&e.SegmentIndex<=s.Original.Zone.EndSegmentIndex));
        var confidence = IsUnknown(identity) ? "NONE" : hasIdentityAnchor && allZonesProven ? "HIGH" :
            states.Skip(start).Take(end - start + 1).Any(s => s.Confidence is "HIGH" or "MEDIUM" or "CONCURRENT" or "PROPAGATED") ? "MEDIUM" : "LOW";
        var sources = string.Join('|', states.Skip(start).Take(end - start + 1).Select(s => s.Source).Where(x => !string.IsNullOrWhiteSpace(x)).Distinct(StringComparer.OrdinalIgnoreCase));
        var cardinal = ResolveSignedCardinal(corridor, run, matches, identity, startNav, endNav);
        var d = Math.Max(0, endNav - startNav);
        return new V11IdentityInterval(index, identity, primary, first.Original.Zone.ZoneIndex, last.Original.Zone.ZoneIndex, startNav, endNav, d, d / 1609.344,
            cardinal.Cardinal, confidence, cardinal.Confidence, cardinal.Source, cardinal.BearingDeg, ExpectedAxis(identity), sources);
    }

    private static (string Cardinal, string Confidence, string Source, double BearingDeg) ResolveSignedCardinal(
        RouteCorridor corridor,
        V09SemanticRun run,
        IReadOnlyList<V11BranchBoardMatch> matches,
        string identity,
        double startNav,
        double endNav)
    {
        var bearing = TravelBearing(corridor, startNav, endNav);
        if (IsUnknown(identity)) return ("", "NONE", "", bearing);

        var anchors = new List<(string Cardinal, double Weight, string Source)>();
        foreach (var m in matches.Where(m => m.CardinalConfidence == "HIGH" && string.Equals(m.BranchIdentity, identity, StringComparison.OrdinalIgnoreCase) && !string.IsNullOrWhiteSpace(m.Cardinal)))
        {
            if (m.EventNavStationM >= startNav - 2 && m.EventNavStationM <= endNav + 2)
                anchors.Add((m.Cardinal.ToUpperInvariant(), 140, "BRANCH_BOARD"));
        }
        foreach (var e in run.Evidence.Where(e => e.EvidenceType == "CONFIRMATION_SIGN" && e.Confidence == "HIGH" && string.Equals(e.Identity, identity, StringComparison.OrdinalIgnoreCase) && !string.IsNullOrWhiteSpace(e.Cardinal)))
        {
            if (e.NavStationM >= startNav - 1e-6 && e.NavStationM <= endNav + 1e-6)
                anchors.Add((e.Cardinal.ToUpperInvariant(), Math.Max(100, e.Strength), "CONFIRMATION_SIGN"));
        }

        var groups = anchors.GroupBy(a => a.Cardinal, StringComparer.OrdinalIgnoreCase)
            .Select(g => new { Cardinal = g.Key, Weight = g.Sum(x => x.Weight), Sources = string.Join('+', g.Select(x => x.Source).Distinct()) })
            .OrderByDescending(x => x.Weight).ToList();
        if (groups.Count == 0)
            return ("", "NONE", "NO_EXPLICIT_CARDINAL_ANCHOR", bearing);
        if (groups.Count > 1)
            return ("", "CONFLICT", "EXPLICIT_CARDINAL_CONFLICT", bearing);
        return (groups[0].Cardinal, "HIGH", groups[0].Sources, bearing);
    }

    private static IReadOnlyList<V11CruiseSpan> BuildCruiseSpans(IReadOnlyList<V11IdentityInterval> intervals, IReadOnlyList<V11DriverAction> actions)
    {
        var primary = intervals.Where(i => i.Primary).OrderBy(i => i.StartNavStationM).ToList();
        var result = new List<V11CruiseSpan>();
        foreach (var interval in primary)
        {
            var inside = actions.Where(a => a.ActionRequired && a.NavStationM > interval.StartNavStationM + 1e-6 && a.NavStationM <= interval.EndNavStationM + 1e-6).OrderBy(a => a.NavStationM).ToList();
            var terminal = inside.LastOrDefault(a => Math.Abs(a.NavStationM - interval.EndNavStationM) < 1.0);
            result.Add(new V11CruiseSpan(result.Count, interval.IntervalIndex, interval.StartZoneIndex, interval.EndZoneIndex,
                interval.StartNavStationM, interval.EndNavStationM, interval.DistanceM, interval.DistanceMi,
                IsUnknown(interval.Identity) ? "CONNECTOR" : "CRUISE", interval.Identity, interval.SignedCardinal, interval.TravelBearingDeg,
                interval.IdentityConfidence, interval.CardinalConfidence, inside.Count, terminal?.Instruction ?? "", terminal?.ExitNumber ?? ""));
        }
        return result;
    }

    private static IReadOnlyList<V11GuidanceEvent> BuildGuidanceEvents(
        IReadOnlyList<V11DriverAction> actions,
        IReadOnlyList<V11CruiseSpan> spans,
        IReadOnlyList<V11IdentityInterval> intervals,
        IReadOnlyList<V11BranchBoardMatch> matches)
    {
        var active = actions.Where(a => a.ActionRequired && a.Instruction != "SUPPRESS").OrderBy(a => a.NavStationM).ToList();
        var result = new List<V11GuidanceEvent>();
        for (var i = 0; i < active.Count; i++)
        {
            var action = active[i];
            var before = PrimaryAt(intervals, action.NavStationM - 0.01);
            var after = PrimaryAt(intervals, action.NavStationM + 0.01);
            var parent = spans.FirstOrDefault(s => action.NavStationM > s.StartNavStationM + 1e-6 && action.NavStationM <= s.EndNavStationM + 1e-6)
                         ?? spans.FirstOrDefault(s => Math.Abs(action.NavStationM - s.StartNavStationM) <= 1e-6);

            var high = matches.Where(m => m.EventIndex == action.EventIndex && m.Confidence == "HIGH").OrderByDescending(m => m.SelectedScore).ToList();
            var branchConfidence = high.Count > 0 ? "HIGH" : matches.Any(m => m.EventIndex == action.EventIndex && m.Confidence == "AMBIGUOUS") ? "AMBIGUOUS" : matches.Any(m => m.EventIndex == action.EventIndex && m.Confidence == "CONFLICT") ? "CONFLICT" : "LOW";
            var destination = string.Join('|', high.SelectMany(m => Split(m.ValidatedToward)).Where(x => !string.IsNullOrWhiteSpace(x)).Distinct(StringComparer.OrdinalIgnoreCase));
            var destConfidence = string.IsNullOrWhiteSpace(destination) ? "NONE" : "HIGH";
            var exitNumber = action.ExitValidated ? action.ExitNumber : "";
            if (action.ExitValidated && !high.Any(m => m.ExitConfidence == "HIGH" && Split(m.ExitNumbers).Contains(action.ExitNumber, StringComparer.OrdinalIgnoreCase)))
                exitNumber = "";
            var next = i + 1 < active.Count ? active[i + 1] : null;

            result.Add(new V11GuidanceEvent(
                result.Count, action.EventIndex, parent?.SpanIndex ?? -1, action.NavStationM,
                parent is null ? 0 : Math.Max(0, action.NavStationM - parent.StartNavStationM), action.Instruction, exitNumber,
                before?.Identity ?? "UNKNOWN", after?.Identity ?? "UNKNOWN", after?.SignedCardinal ?? "",
                !string.Equals(before?.Identity ?? "UNKNOWN", after?.Identity ?? "UNKNOWN", StringComparison.OrdinalIgnoreCase),
                destination, action.SelectedBranchId, branchConfidence, action.ActionConfidence,
                after?.IdentityConfidence ?? "NONE", after?.CardinalConfidence ?? "NONE", destConfidence,
                next?.Instruction ?? "END", next is null ? null : Math.Max(0, next.NavStationM - action.NavStationM)));
        }
        return result;
    }

    private static IReadOnlyList<V11PlanEntry> BuildPlan(RouteCorridor corridor, IReadOnlyList<V11CruiseSpan> spans, IReadOnlyList<V11GuidanceEvent> guidance)
    {
        var result = new List<V11PlanEntry>();
        foreach (var span in spans)
        {
            var first = guidance.Where(g => g.ParentSpanIndex == span.SpanIndex).OrderBy(g => g.NavStationM).FirstOrDefault();
            result.Add(new V11PlanEntry(0, span.StartNavStationM, "ROUTE_SPAN", span.SpanIndex,
                span.SpanType == "CRUISE" ? "STAY_ON" : "CONNECTOR", span.RoadIdentity, span.SignedCardinal, span.TravelBearingDeg,
                span.DistanceM, span.DistanceMi, "", "", "", "", "", first?.Instruction ?? "END",
                first is null ? null : Math.Max(0, first.NavStationM - span.StartNavStationM), "NONE", span.IdentityConfidence,
                span.CardinalConfidence, "NONE", $"continuous_span;guidance_events={span.GuidanceEventCount}"));
        }
        foreach (var g in guidance)
        {
            var afterSpan = spans.FirstOrDefault(s => s.SpanIndex == g.ParentSpanIndex);
            result.Add(new V11PlanEntry(0, g.NavStationM, "GUIDANCE", g.ParentSpanIndex, g.Instruction, g.RoadAfter, g.CardinalAfter,
                afterSpan?.TravelBearingDeg ?? double.NaN, null, null, g.ExitNumber, g.DestinationToward, g.RoadBefore, g.RoadAfter,
                g.SelectedBranchId, g.NextInstruction, g.DistanceToNextEventM, g.ActionConfidence, g.RoadIdentityConfidence,
                g.CardinalConfidence, g.DestinationConfidence, g.ChangesIdentity ? "route_identity_change" : "intermediate_guidance")
            { ExitConfidence = string.IsNullOrWhiteSpace(g.ExitNumber) ? "NONE" : "HIGH" });
        }
        var last = spans.LastOrDefault();
        result.Add(new V11PlanEntry(0, corridor.TotalNavStationM, "ARRIVAL", last?.SpanIndex ?? -1, "ARRIVE", last?.RoadIdentity ?? "UNKNOWN",
            last?.SignedCardinal ?? "", last?.TravelBearingDeg ?? double.NaN, null, null, "", "", "", "", "", "", null, "HIGH",
            last?.IdentityConfidence ?? "NONE", last?.CardinalConfidence ?? "NONE", "NONE", "route_end"));
        return result.OrderBy(e => e.NavStationM).ThenBy(e => e.EntryType == "GUIDANCE" ? 0 : e.EntryType == "ROUTE_SPAN" ? 1 : 2)
            .Select((e, idx) => e with { PlanIndex = idx }).ToList();
    }

    private static string LaneInstruction(int laneCount, IReadOnlyList<int> lanes)
    {
        if (laneCount <= 0 || lanes.Count == 0) return "USE_SELECTED_LANES";
        var ordered = lanes.Distinct().OrderBy(x => x).ToList();
        if (ordered.Count >= laneCount) return "SUPPRESS";
        var contiguous = ordered[^1] - ordered[0] + 1 == ordered.Count;
        if (contiguous && ordered[0] == 0) return "KEEP_LEFT";
        if (contiguous && ordered[^1] == laneCount - 1) return "KEEP_RIGHT";
        return "USE_SELECTED_LANES";
    }

    private static string[] Split(string value) => string.IsNullOrWhiteSpace(value) ? [] : value.Split('|', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
    private static string JoinInts(IEnumerable<int> values) => string.Join('|', values);
    private static bool IsSoftContinuityBoundary(string instruction) => instruction is "KEEP_LEFT" or "KEEP_RIGHT" or "CONTINUE_AT_FORK" or "FORK_STRAIGHT";
    private static bool IsUnknown(string s) => string.IsNullOrWhiteSpace(s) || s.Equals("UNKNOWN", StringComparison.OrdinalIgnoreCase);

    private static string ExpectedAxis(string identity)
    {
        var m = Regex.Match(identity ?? "", @"^(?<kind>I|US)-(?<num>\d+)$", RegexOptions.IgnoreCase);
        if (!m.Success || !int.TryParse(m.Groups["num"].Value, out var n)) return "";
        if (m.Groups["kind"].Value.Equals("I", StringComparison.OrdinalIgnoreCase) && n >= 100) return "";
        return n % 2 == 0 ? "EW" : "NS";
    }

    private static double TravelBearing(RouteCorridor corridor, double startNav, double endNav)
    {
        if (endNav <= startNav + 1e-6) return double.NaN;
        var a = PositionAtNavStation(corridor, startNav);
        var b = PositionAtNavStation(corridor, endNav);
        var ga = AtsMapProjection.ToWgs84(a.X, a.Z);
        var gb = AtsMapProjection.ToWgs84(b.X, b.Z);
        var lat1 = ga.Latitude * Math.PI / 180.0;
        var lat2 = gb.Latitude * Math.PI / 180.0;
        var dLon = (gb.Longitude - ga.Longitude) * Math.PI / 180.0;
        var y = Math.Sin(dLon) * Math.Cos(lat2);
        var x = Math.Cos(lat1) * Math.Sin(lat2) - Math.Sin(lat1) * Math.Cos(lat2) * Math.Cos(dLon);
        var deg = Math.Atan2(y, x) * 180.0 / Math.PI;
        return (deg + 360.0) % 360.0;
    }

    private static V11IdentityInterval? PrimaryAt(IReadOnlyList<V11IdentityInterval> intervals, double nav)
        => intervals.Where(i => i.Primary && nav >= i.StartNavStationM - 1e-6 && nav <= i.EndNavStationM + 1e-6)
            .OrderBy(i => Math.Abs(nav - (i.StartNavStationM + i.EndNavStationM) * 0.5)).FirstOrDefault();

    private static Vector3 PositionAtStation(RouteCorridor corridor, double station)
    {
        if (corridor.Edges.Count == 0) return Vector3.Zero;
        var e = corridor.Edges.FirstOrDefault(x => station >= x.StartStationM - 1e-6 && station <= x.EndStationM + 1e-6)
                ?? corridor.Edges.OrderBy(x => Math.Min(Math.Abs(station - x.StartStationM), Math.Abs(station - x.EndStationM))).First();
        var denom = e.EndStationM - e.StartStationM;
        var t = denom > 1e-9 ? Math.Clamp((station - e.StartStationM) / denom, 0, 1) : 0;
        return Vector3.Lerp(e.Start, e.End, (float)t);
    }

    private static Vector3 PositionAtNavStation(RouteCorridor corridor, double nav)
    {
        if (corridor.Edges.Count == 0) return Vector3.Zero;
        var e = corridor.Edges.FirstOrDefault(x => nav >= x.StartNavStationM - 1e-6 && nav <= x.EndNavStationM + 1e-6)
                ?? corridor.Edges.OrderBy(x => Math.Min(Math.Abs(nav - x.StartNavStationM), Math.Abs(nav - x.EndNavStationM))).First();
        var denom = e.EndNavStationM - e.StartNavStationM;
        var t = denom > 1e-9 ? Math.Clamp((nav - e.StartNavStationM) / denom, 0, 1) : 0;
        return Vector3.Lerp(e.Start, e.End, (float)t);
    }

    private static void WriteDriverActions(string path, IReadOnlyList<V11DriverAction> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("action_index,event_index,segment_index,nav_station_m,preliminary_class,original_instruction,instruction,action_required,selected_branch_id,input_lane_count,valid_lane_ordinals,active_lane_ordinals,all_input_lanes_reach_selected_exit,exit_validated,exit_number,action_reason,branch_confidence,action_confidence,then_instruction,distance_to_then_m");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.ActionIndex, r.EventIndex, r.SegmentIndex, F(r.NavStationM), Csv(r.PreliminaryClass), Csv(r.OriginalInstruction), Csv(r.Instruction), r.ActionRequired ? 1 : 0, Csv(r.SelectedBranchId), r.InputLaneCount, Csv(r.ValidLaneOrdinals), Csv(r.ActiveLaneOrdinals), r.AllInputLanesReachSelectedExit ? 1 : 0, r.ExitValidated ? 1 : 0, Csv(r.ExitNumber), Csv(r.ActionReason), Csv(r.BranchConfidence), Csv(r.ActionConfidence), Csv(r.ThenInstruction), r.DistanceToThenM.HasValue ? F(r.DistanceToThenM.Value) : ""));
    }

    private static void WriteBoardSemantics(string path, IReadOnlyList<V11BoardSemantic> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("board_index,event_index,event_nav_station_m,sign_uid,template,area_name,effective_board_token,board_path,board_kind,region,corridor_along_m,corridor_lateral_m,corridor_orientation_error_deg,board_anchor_x,board_anchor_y,board_anchor_z,board_anchor_yaw_deg,board_anchor_source,branch_identity,cardinal,toward,exit_numbers,arrow_tokens,direction_hint,raw_strings,source_attributes,semantic_confidence,identity_confidence,cardinal_confidence,destination_confidence,exit_confidence,validated_toward,validation_notes");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.BoardIndex, r.EventIndex, F(r.EventNavStationM), Hex(r.SignUid), Csv(r.Template), Csv(r.AreaName), Csv(r.EffectiveBoardToken), Csv(r.BoardPath), Csv(r.BoardKind), Csv(r.Region), F(r.CorridorAlongM), F(r.CorridorLateralM), F(r.CorridorOrientationErrorDeg), F(r.BoardAnchorPosition.X), F(r.BoardAnchorPosition.Y), F(r.BoardAnchorPosition.Z), F(r.BoardAnchorYawDeg), Csv(r.BoardAnchorSource), Csv(r.BranchIdentity), Csv(r.Cardinal), Csv(r.Toward), Csv(r.ExitNumbers), Csv(r.ArrowTokens), Csv(r.DirectionHint), Csv(r.RawStrings), Csv(r.SourceAttributes), Csv(r.SemanticConfidence), Csv(r.IdentityConfidence), Csv(r.CardinalConfidence), Csv(r.DestinationConfidence), Csv(r.ExitConfidence), Csv(r.ValidatedToward), Csv(r.ValidationNotes)));
    }

    private static void WriteBranchMatches(string path, IReadOnlyList<V11BranchBoardMatch> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("match_index,board_index,event_index,event_nav_station_m,sign_uid,area_name,selected_branch_id,best_alternative_branch_id,alternative_branch_count,selected_score,best_alternative_score,margin,selected_lateral_m,selected_orientation_error_deg,branch_identity,cardinal,toward,exit_numbers,match_reason,confidence,identity_confidence,cardinal_confidence,destination_confidence,exit_confidence,validated_toward,validation_notes");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.MatchIndex, r.BoardIndex, r.EventIndex, F(r.EventNavStationM), Hex(r.SignUid), Csv(r.AreaName), Csv(r.SelectedBranchId), Csv(r.BestAlternativeBranchId), r.AlternativeBranchCount, F(r.SelectedScore), F(r.BestAlternativeScore), F(r.Margin), FD(r.SelectedLateralM), FD(r.SelectedOrientationErrorDeg), Csv(r.BranchIdentity), Csv(r.Cardinal), Csv(r.Toward), Csv(r.ExitNumbers), Csv(r.MatchReason), Csv(r.Confidence), Csv(r.IdentityConfidence), Csv(r.CardinalConfidence), Csv(r.DestinationConfidence), Csv(r.ExitConfidence), Csv(r.ValidatedToward), Csv(r.ValidationNotes)));
    }

    private static void WriteIntervals(string path, IReadOnlyList<V11IdentityInterval> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("interval_index,identity,primary,start_zone_index,end_zone_index,start_nav_station_m,end_nav_station_m,distance_m,distance_mi,signed_cardinal,identity_confidence,cardinal_confidence,cardinal_source,travel_bearing_deg,expected_axis_hint,sources");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.IntervalIndex, Csv(r.Identity), r.Primary ? 1 : 0, r.StartZoneIndex, r.EndZoneIndex, F(r.StartNavStationM), F(r.EndNavStationM), F(r.DistanceM), F(r.DistanceMi), Csv(r.SignedCardinal), Csv(r.IdentityConfidence), Csv(r.CardinalConfidence), Csv(r.CardinalSource), FD(r.TravelBearingDeg), Csv(r.ExpectedAxisHint), Csv(r.Sources)));
    }

    private static void WriteSpans(string path, IReadOnlyList<V11CruiseSpan> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("span_index,primary_interval_index,start_zone_index,end_zone_index,start_nav_station_m,end_nav_station_m,distance_m,distance_mi,span_type,road_identity,signed_cardinal,travel_bearing_deg,identity_confidence,cardinal_confidence,guidance_event_count,terminal_instruction,terminal_exit_number");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.SpanIndex, r.PrimaryIntervalIndex, r.StartZoneIndex, r.EndZoneIndex, F(r.StartNavStationM), F(r.EndNavStationM), F(r.DistanceM), F(r.DistanceMi), Csv(r.SpanType), Csv(r.RoadIdentity), Csv(r.SignedCardinal), FD(r.TravelBearingDeg), Csv(r.IdentityConfidence), Csv(r.CardinalConfidence), r.GuidanceEventCount, Csv(r.TerminalInstruction), Csv(r.TerminalExitNumber)));
    }

    private static void WriteGuidance(string path, IReadOnlyList<V11GuidanceEvent> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("guidance_index,event_index,parent_span_index,nav_station_m,offset_in_span_m,instruction,exit_number,road_before,road_after,cardinal_after,changes_identity,destination_toward,selected_branch_id,branch_association_confidence,action_confidence,road_identity_confidence,cardinal_confidence,destination_confidence,next_instruction,distance_to_next_event_m");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.GuidanceIndex, r.EventIndex, r.ParentSpanIndex, F(r.NavStationM), F(r.OffsetInSpanM), Csv(r.Instruction), Csv(r.ExitNumber), Csv(r.RoadBefore), Csv(r.RoadAfter), Csv(r.CardinalAfter), r.ChangesIdentity ? 1 : 0, Csv(r.DestinationToward), Csv(r.SelectedBranchId), Csv(r.BranchAssociationConfidence), Csv(r.ActionConfidence), Csv(r.RoadIdentityConfidence), Csv(r.CardinalConfidence), Csv(r.DestinationConfidence), Csv(r.NextInstruction), r.DistanceToNextEventM.HasValue ? F(r.DistanceToNextEventM.Value) : ""));
    }

    private static void WritePlan(string path, IReadOnlyList<V11PlanEntry> rows)
    {
        using var w = CsvWriter(path);
        w.WriteLine("plan_index,nav_station_m,entry_type,span_index,instruction,road_identity,signed_cardinal,travel_bearing_deg,distance_m,distance_mi,exit_number,destination_toward,road_before,road_after,selected_branch_id,next_instruction,distance_to_next_event_m,action_confidence,identity_confidence,cardinal_confidence,destination_confidence,notes,exit_confidence");
        foreach (var r in rows) w.WriteLine(string.Join(',', r.PlanIndex, F(r.NavStationM), Csv(r.EntryType), r.SpanIndex, Csv(r.Instruction), Csv(r.RoadIdentity), Csv(r.SignedCardinal), FD(r.TravelBearingDeg), r.DistanceM.HasValue ? F(r.DistanceM.Value) : "", r.DistanceMi.HasValue ? F(r.DistanceMi.Value) : "", Csv(r.ExitNumber), Csv(r.DestinationToward), Csv(r.RoadBefore), Csv(r.RoadAfter), Csv(r.SelectedBranchId), Csv(r.NextInstruction), r.DistanceToNextEventM.HasValue ? F(r.DistanceToNextEventM.Value) : "", Csv(r.ActionConfidence), Csv(r.IdentityConfidence), Csv(r.CardinalConfidence), Csv(r.DestinationConfidence), Csv(r.Notes), Csv(r.ExitConfidence)));
    }

    private static void WriteRegression(
        string path,
        IReadOnlyList<V11DriverAction> actions,
        IReadOnlyList<V11BranchBoardMatch> matches,
        IReadOnlyList<V11IdentityInterval> intervals,
        int multiHighIdentityEvents,
        int geometryHighCardinals,
        int destinationsWithoutHigh,
        int keepAllLanes,
        int highBelowMargin,
        int semanticFailures)
    {
        File.WriteAllText(path, $"""
BetterGPSNavigation v0.11 regression guards

geometry_derived_high_signed_cardinals={geometryHighCardinals}
destinations_without_high_branch_match={destinationsWithoutHigh}
straight_keep_events_when_all_input_lanes_reach_selected_branch={keepAllLanes}
high_matches_below_required_branch_margin={highBelowMargin}
semantic_field_validation_failures={semanticFailures}
multi_high_identity_events={multiHighIdentityEvents}
cardinal_conflicts={intervals.Count(i => i.CardinalConfidence == "CONFLICT")}
ambiguous_branch_boards={matches.Count(m => m.Confidence == "AMBIGUOUS")}
conflicting_branch_boards={matches.Count(m => m.Confidence == "CONFLICT")}
suppressed_events={actions.Count(a => !a.ActionRequired)}

PASS requires the five hard-guard counters above multi_high_identity_events to be zero. multi_high_identity_events is diagnostic because signed route concurrency can be legitimate.
""");
    }

    private static StreamWriter CsvWriter(string path) => new(path, false, new System.Text.UTF8Encoding(false));
    private static string F(double v) => v.ToString("0.######", CultureInfo.InvariantCulture);
    private static string FD(double v) => double.IsFinite(v) ? F(v) : "";
    private static string Hex(ulong v) => v.ToString("X16", CultureInfo.InvariantCulture);
    private static string Csv(string? value)
    {
        value ??= "";
        if (!value.Contains(',') && !value.Contains('"') && !value.Contains('\n') && !value.Contains('\r')) return value;
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}
