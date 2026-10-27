using BetterGPSNavigation;
using TruckLib.ScsMap;
if(args is ["--refresh-board-view",var boardView]){GroupGuidanceView.Refresh(boardView);return 0;}
if(args is ["--check-board-measurements",var boardCandidates,var boardMeasurements,var boardTransform,var boardResult])
    return BoardCalibrationTools.Check(boardCandidates,boardMeasurements,boardTransform,boardResult);

if (args.FirstOrDefault() == "--capture")
    return await NativeBridgeCli.Run(args.Skip(1).ToArray());

if (args.FirstOrDefault() == "--live")
    return await LiveGuidanceCli.Run(args.Skip(1).ToArray());

if (args.Length == 0 || args.Any(a => a is "--help" or "-h" or "/?"))
{
    Options.PrintUsage();
    return 0;
}

try
{
    var options = Options.Parse(args);
    Directory.CreateDirectory(options.OutputRoot);

    Console.WriteLine("BetterGPSNavigation 0.11 - driver-action gating + branch-unique boards + signed-cardinal evidence + BetterGPS plan v3");
    Console.WriteLine($"Game root : {options.GameRoot}");
    Console.WriteLine($"Documents : {options.DocumentsRoot}");
    Console.WriteLine($"Probe     : {options.ProbePath}");
    Console.WriteLine($"Output    : {options.OutputRoot}");
    Console.WriteLine();

    var routes = ProbeRoute.LoadAll(options.ProbePath);
    Console.WriteLine($"Found {routes.Count} probe route snapshot(s).");

    using var assets = GameAssets.Open(options);
    Console.WriteLine($"Loaded {assets.Sources.Count} asset source(s) in priority order:");
    foreach (var source in assets.Sources.Take(20)) Console.WriteLine("  " + source);
    if (assets.Sources.Count > 20) Console.WriteLine($"  ... {assets.Sources.Count - 20} more");

    var mapPath = options.MapPath;
    if (!assets.Loader.FileExists(mapPath))
    {
        var candidates = assets.Loader.DirectoryExists("/map/")
            ? assets.Loader.GetFiles("/map/").Where(x => x.EndsWith(".mbd", StringComparison.OrdinalIgnoreCase)).ToList()
            : [];
        throw new FileNotFoundException($"Map '{mapPath}' not found. MBD candidates: {string.Join(", ", candidates)}");
    }

    Console.WriteLine();
    Console.WriteLine($"Loading TruckLib map {mapPath} ...");
    var map = Map.Open(mapPath, assets.Loader);
    Console.WriteLine($"Map loaded: {map.Nodes.Count:N0} nodes, {map.MapItems.Count:N0} items, {map.Sectors.Count:N0} sectors.");
    File.WriteAllLines(Path.Combine(options.OutputRoot, "asset_sources.txt"), assets.Sources);

    var bridge = new MapBridge(map, assets.Loader, options.OutputRoot);
    Console.WriteLine($"Sign index    : {bridge.IndexedSignCount:N0} map signs, {bridge.SemanticSignCount:N0} semantic, {bridge.ExitSignCount:N0} exit-number, {bridge.RoadIdentitySignCount:N0} road-identity.");
    Console.WriteLine($"Road overlays : {bridge.RoadOverlayCount:N0} RoadName overlays, {bridge.NormalizedRoadOverlayCount:N0} normalized route identities.");
    Console.WriteLine($"Sign templates: {bridge.ResolvedUsedSignTemplateCount:N0}/{bridge.UsedSignTemplateCount:N0} used resolved ({bridge.SignTemplateDefinitionCount:N0} definitions, unsupported={bridge.UnsupportedUsedSignTemplateCount:N0}).");
    Console.WriteLine($"Sign boards   : {bridge.ResolvedUsedSignBoardCount:N0}/{bridge.UsedSignBoardCount:N0} used resolved ({bridge.SignBoardDefinitionCount:N0} definitions).");
    Console.WriteLine($"Destinations  : {bridge.KnownDestinationCount:N0} known city names; other place candidates remain below HIGH.");

    var summaries = new List<BridgeSummary>();
    var v10Summaries = new List<V10Summary>();
    var v11Summaries = new List<V11Summary>();

    foreach (var route in routes)
    {
        Console.WriteLine();
        Console.WriteLine($"Analyzing {route.Stem} ...");
        var summary = bridge.Analyze(route);
        summaries.Add(summary);
        Console.WriteLine($"  Nodes   : {summary.NodesFound}/{summary.PhysicalNodes}");
        Console.WriteLine($"  Segments: {summary.SegmentsResolved}/{summary.SegmentCount} (roads={summary.RoadSegments}, prefabs={summary.PrefabSegments}, other={summary.OtherSegments})");
        Console.WriteLine($"  PPD     : {summary.PpdResolved}/{summary.PpdPrefabs} loaded, {summary.PpdPathFound}/{summary.PpdPrefabs} paths");
        Console.WriteLine($"  Features: {summary.ManeuverFeatureSelectedPaths}/{summary.ManeuverFeaturePrefabSegments} selected lane paths");
        Console.WriteLine($"  Semantic: zones={summary.V09IdentifiedZones}/{summary.V09Zones} named ({summary.V09NamedCoverageRatio * 100:0.0}% nav coverage)");

        if (bridge.LastV10Summary is { } v10)
        {
            v10Summaries.Add(v10);
            Console.WriteLine($"  Plan v2 : intervals={v10.IdentityIntervals}, spans={v10.NamedCruiseSpans} named + {v10.ConnectorSpans} connector, guidance={v10.GuidanceEvents}, branch-high={v10.HighBranchSignEvidence}");
        }
        if (bridge.LastV11Summary is { } v11)
        {
            v11Summaries.Add(v11);
            Console.WriteLine($"  Plan v3 : spans={v11.NamedCruiseSpans} named + {v11.ConnectorSpans} connector, guidance={v11.GuidanceEvents}, suppressed={v11.SuppressedEvents}, boards={v11.BoardSemantics}, branch-high={v11.HighBranchBoardMatches}, ambiguous={v11.AmbiguousBranchBoardMatches}, conflicts={v11.ConflictingBranchBoardMatches}, cardinal-high={v11.HighCardinalIntervals}, named={v11.NamedCoverageRatio * 100:0.0}%");
        }

        var structuralStatus = summary.PhysicalNodes > 0
            ? (summary.PerfectNodeCoverage && summary.PerfectSegments && summary.PerfectNativePrefabs && summary.PerfectPpdResolution && summary.PerfectManeuverFeatures && summary.PhysicalMatchesLogicalNodeSequence
                ? "VALID"
                : "FAILED")
            : summary.LogicalItems > 0
                ? "TRANSIENT_LOGICAL_ONLY"
                : "EMPTY";

        var regressionPath = Path.Combine(options.OutputRoot, route.Stem + "_v11_regression.txt");
        File.AppendAllText(regressionPath, $"""

Structural snapshot classification
structural_snapshot_status={structuralStatus}
physical_nodes={summary.PhysicalNodes}
logical_items={summary.LogicalItems}
physical_matches_logical_node_sequence={summary.PhysicalMatchesLogicalNodeSequence}

""");
        Console.WriteLine($"  Snapshot: {structuralStatus}");
    }

    using (var writer = new StreamWriter(Path.Combine(options.OutputRoot, "SUMMARY.txt"), false))
    {
        foreach (var s0 in summaries)
        {
            writer.WriteLine(s0.ToText());
            writer.WriteLine("---");
        }
    }

    File.WriteAllText(Path.Combine(options.OutputRoot, "V11_SUMMARY.txt"), $"""
BetterGPSNavigation 0.11 - evidence-driven navigation summary

used_sign_templates={bridge.UsedSignTemplateCount}
resolved_used_sign_templates={bridge.ResolvedUsedSignTemplateCount}
unsupported_used_sign_templates={bridge.UnsupportedUsedSignTemplateCount}
sign_template_definitions={bridge.SignTemplateDefinitionCount}
used_sign_boards={bridge.UsedSignBoardCount}
resolved_used_sign_boards={bridge.ResolvedUsedSignBoardCount}
sign_board_definitions={bridge.SignBoardDefinitionCount}
known_destination_names={bridge.KnownDestinationCount}

v11_identity_intervals={v11Summaries.Sum(v => v.IdentityIntervals)}
v11_primary_intervals={v11Summaries.Sum(v => v.PrimaryIntervals)}
v11_alias_intervals={v11Summaries.Sum(v => v.AliasIntervals)}
v11_named_primary_intervals={v11Summaries.Sum(v => v.NamedPrimaryIntervals)}
v11_cruise_spans={v11Summaries.Sum(v => v.CruiseSpans)}
v11_named_cruise_spans={v11Summaries.Sum(v => v.NamedCruiseSpans)}
v11_connector_spans={v11Summaries.Sum(v => v.ConnectorSpans)}
v11_guidance_events={v11Summaries.Sum(v => v.GuidanceEvents)}
v11_suppressed_events={v11Summaries.Sum(v => v.SuppressedEvents)}
v11_board_semantics={v11Summaries.Sum(v => v.BoardSemantics)}
v11_branch_board_matches={v11Summaries.Sum(v => v.BranchBoardMatches)}
v11_high_branch_board_matches={v11Summaries.Sum(v => v.HighBranchBoardMatches)}
v11_ambiguous_branch_board_matches={v11Summaries.Sum(v => v.AmbiguousBranchBoardMatches)}
v11_conflicting_branch_board_matches={v11Summaries.Sum(v => v.ConflictingBranchBoardMatches)}
v11_branch_promoted_zones={v11Summaries.Sum(v => v.BranchPromotedZones)}
v11_high_cardinal_intervals={v11Summaries.Sum(v => v.HighCardinalIntervals)}
v11_geometry_derived_high_signed_cardinals={v11Summaries.Sum(v => v.GeometryDerivedSignedCardinals)}
v11_multi_high_identity_events={v11Summaries.Sum(v => v.MultiHighIdentityEvents)}
v11_destinations_without_high_branch_match={v11Summaries.Sum(v => v.DestinationsWithoutHighBranchMatch)}
v11_unnecessary_straight_keeps={v11Summaries.Sum(v => v.UnnecessaryStraightKeeps)}
v11_high_matches_below_required_margin={v11Summaries.Sum(v => v.HighMatchesBelowRequiredMargin)}
v11_semantic_field_validation_failures={v11Summaries.Sum(v => v.SemanticFieldFailures)}
v11_plan_entries={v11Summaries.Sum(v => v.PlanEntries)}

Global outputs:
- SIGN_AREA_ATTRIBUTES.csv: typed SignOverride attributes, preserving AreaName/object ID/attribute index.
- SIGN_BOARD_INDEX.csv and SIGN_BOARD_SCHEMA.csv: effective sign-board definitions from installed ATS assets.

Per-route outputs:
- route_*_driver_actions_v11.csv
- route_*_board_semantics_v11.csv
- route_*_branch_board_matches_v11.csv
- route_*_identity_intervals_v11.csv
- route_*_cruise_spans_v11.csv
- route_*_guidance_events_v11.csv
- route_*_bettergps_plan_v3.csv
- route_*_v11_regression.txt

Confidence policy:
- EXACT = selected ATS/PPD structural fact.
- HIGH = explicit semantic evidence uniquely associated with selected branch.
- MEDIUM = conservative propagation/support.
- LOW = weak inference.
- AMBIGUOUS = selected and alternative explanations are too close.
- CONFLICT = evidence disagrees.

Critical rules:
- local geometry never creates signed NORTH/SOUTH/EAST/WEST; it only creates travel_bearing_deg.
- boards are matched by AreaName, not by assigning an entire physical Sign to one branch.
- HIGH requires selected-branch uniqueness versus alternatives.
- EXIT requires branch-unique exit evidence; otherwise guidance remains/downgrades to KEEP.
- destinations enter plan v3 only from HIGH branch-associated board semantics.
- displayed distances remain route_task navigation distances.
""");

    File.WriteAllText(Path.Combine(options.OutputRoot, "V10_SUMMARY.txt"), $"""
BetterGPSNavigation 0.10 compatibility summary (A/B reference)

v10_identity_intervals={v10Summaries.Sum(v => v.IdentityIntervals)}
v10_cruise_spans={v10Summaries.Sum(v => v.CruiseSpans)}
v10_guidance_events={v10Summaries.Sum(v => v.GuidanceEvents)}
v10_high_branch_sign_evidence={v10Summaries.Sum(v => v.HighBranchSignEvidence)}
v10_plan_entries={v10Summaries.Sum(v => v.PlanEntries)}
""");

    // route_task can expose a logical route for a short recalculation window while
    // physical_route_items is empty. Those snapshots are valuable diagnostics, but
    // there is no physical corridor to validate and they must not fail the run.
    var actionableSummaries = summaries.Where(x => x.PhysicalNodes > 0).ToList();
    var transientLogicalOnlySnapshots = summaries.Count(x => x.PhysicalNodes == 0 && x.LogicalItems > 0);
    var emptySnapshots = summaries.Count(x => x.PhysicalNodes == 0 && x.LogicalItems == 0);
    var structuralFailures = actionableSummaries.Where(x =>
        !(x.PerfectNodeCoverage &&
          x.PerfectSegments &&
          x.PerfectNativePrefabs &&
          x.PerfectPpdResolution &&
          x.PerfectManeuverFeatures &&
          x.PhysicalMatchesLogicalNodeSequence)).ToList();
    var structuralPerfect = structuralFailures.Count == 0;

    var v11Safe = v11Summaries.All(v =>
        v.GeometryDerivedSignedCardinals == 0 &&
        v.DestinationsWithoutHighBranchMatch == 0 &&
        v.UnnecessaryStraightKeeps == 0 &&
        v.HighMatchesBelowRequiredMargin == 0 &&
        v.SemanticFieldFailures == 0);
    var perfect = structuralPerfect && v11Safe;

    File.AppendAllText(Path.Combine(options.OutputRoot, "V11_SUMMARY.txt"), $"""

Snapshot validation:
structurally_actionable_snapshots={actionableSummaries.Count}
transient_logical_only_snapshots={transientLogicalOnlySnapshots}
empty_snapshots={emptySnapshots}
structural_failures_on_actionable_snapshots={structuralFailures.Count}
v11_hard_guards_pass={v11Safe}
""");

    Console.WriteLine();
    Console.WriteLine($"Snapshot validation: actionable={actionableSummaries.Count}, transient-logical-only={transientLogicalOnlySnapshots}, empty={emptySnapshots}, structural-failures={structuralFailures.Count}");
    Console.WriteLine(perfect
        ? "PASS: all structurally actionable snapshots + v0.11 evidence guards resolved cleanly."
        : "PARTIAL: an actionable structural mapping or v0.11 evidence guard failed. Inspect route_*_v11_regression.txt.");
    Console.WriteLine($"Reports: {options.OutputRoot}");
    return perfect ? 0 : 2;
}
catch (Exception ex)
{
    Console.Error.WriteLine();
    Console.Error.WriteLine("FATAL: " + ex.Message);
    Console.Error.WriteLine(ex);
    return 1;
}
