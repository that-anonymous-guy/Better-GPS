using System.Globalization;
using System.Text;
using TruckLib;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal sealed class MapBridge
{
    private readonly Map map;
    private readonly IFileSystem assetFiles;
    private readonly string outputRoot;
    private readonly PpdResolver ppdResolver;
    private readonly PpdManeuverAnalyzer ppdAnalyzer;
    private readonly ExactNavGraphResolver exactNavGraphResolver;
    private readonly ManeuverFeatureExtractor maneuverFeatureExtractor;
    private readonly SignRoadIdentityResolver signResolver;
    private readonly RouteCorridorBuilder corridorBuilder;
    private readonly RoadOverlayResolver roadOverlayResolver;
    private readonly SemanticRoutePlanner semanticRoutePlanner;
    private readonly SignTemplateResolver signTemplateResolver;
    private readonly NavigationPlanV10 navigationPlanV10;
    private readonly SignBoardResolver signBoardResolver;
    private readonly DecisionBranchResolver decisionBranchResolver;
    private readonly NavigationPlanV11 navigationPlanV11;
    private readonly SignPanelExtractor panelExtractor;
    private readonly RoadLaneGeometryResolver roadLanes;
    private readonly SignGroupGuidance groupGuidance;
    private readonly NamedRoadNetwork namedNetwork;
    public IReadOnlyList<SignGroupProof> AuditGroups {get;private set;}=[];
    public bool RichDiagnostics {get;set;}=true;

    public V10Summary? LastV10Summary { get; private set; }
    public V11Summary? LastV11Summary { get; private set; }
    public NavigationPlanV11 NavigationAudit => navigationPlanV11;
    public IReadOnlyList<ParsedSign> AuditSigns => signResolver.Signs;
    public RouteCorridor? AuditCorridor { get; private set; }
    public LaneRouteGraph? AuditLaneGraph { get; private set; }

    public MapBridge(Map map, IFileSystem fileSystem, string outputRoot, bool writeIndexes = true,string? signCalibrationPath=null,Func<string,string?>? textureHash=null,Dictionary<ulong,ParsedSignArea[]>? sharedSignAreas=null)
    {
        this.map = map;
        assetFiles=fileSystem;
        this.outputRoot = outputRoot;
        ppdResolver = new PpdResolver(fileSystem);
        ppdAnalyzer = new PpdManeuverAnalyzer(ppdResolver);
        exactNavGraphResolver = new ExactNavGraphResolver(ppdResolver);
        maneuverFeatureExtractor = new ManeuverFeatureExtractor(ppdResolver);
        using(PerformanceTrace.Measure("sign-index")) signResolver = new SignRoadIdentityResolver(map, fileSystem);
        corridorBuilder = new RouteCorridorBuilder(map, ppdResolver, maneuverFeatureExtractor);
        using(PerformanceTrace.Measure("overlay-index")) roadOverlayResolver = new RoadOverlayResolver(map);
        semanticRoutePlanner = new SemanticRoutePlanner(map, maneuverFeatureExtractor, signResolver);
        signTemplateResolver = new SignTemplateResolver(fileSystem, signResolver.Signs,!writeIndexes);
        navigationPlanV10 = new NavigationPlanV10(signResolver, signTemplateResolver);
        signBoardResolver = new SignBoardResolver(fileSystem, signTemplateResolver, signResolver.Signs,!writeIndexes);
        panelExtractor = new SignPanelExtractor(fileSystem, signTemplateResolver, signBoardResolver,sharedSignAreas);
        roadLanes = new RoadLaneGeometryResolver(fileSystem);
        namedNetwork=new(fileSystem,signResolver,roadOverlayResolver,panelExtractor);
        groupGuidance=new(fileSystem,signResolver.Signs,signCalibrationPath,textureHash);
        decisionBranchResolver = new DecisionBranchResolver(map, ppdResolver, maneuverFeatureExtractor);
        navigationPlanV11 = new NavigationPlanV11(signResolver, signBoardResolver, decisionBranchResolver, panelExtractor);
        if (writeIndexes)
        {
        signResolver.WriteGlobalIndex(Path.Combine(outputRoot, "SIGN_INDEX.csv"));
        signResolver.WriteAreaIndex(Path.Combine(outputRoot, "SIGN_AREA_INDEX.csv"));
        signResolver.WriteAreaAttributeIndex(Path.Combine(outputRoot, "SIGN_AREA_ATTRIBUTES.csv"));
        signTemplateResolver.WriteSchema(Path.Combine(outputRoot, "SIGN_TEMPLATE_SCHEMA.csv"));
        signTemplateResolver.WriteAreaBindings(Path.Combine(outputRoot, "SIGN_TEMPLATE_AREAS.csv"));
        signBoardResolver.WriteIndex(Path.Combine(outputRoot, "SIGN_BOARD_INDEX.csv"));
        signBoardResolver.WriteSchema(Path.Combine(outputRoot, "SIGN_BOARD_SCHEMA.csv"));
        roadOverlayResolver.WriteGlobalIndex(Path.Combine(outputRoot, "ROAD_OVERLAY_INDEX.csv"));
        }
    }

    public LiveRoute CompileLive(ProbeRoute route,bool preview=false,CancellationToken token=default,bool fastStart=false)
    {
        token.ThrowIfCancellationRequested();
        using var routeTiming=PerformanceTrace.Measure("compile-total");
        NativeMapAudit.Check(route,map,outputRoot);
        if (route.Physical.Count < 2 || !route.Logical.Where(l => l.Type == 0).Select(l => l.ItemUid)
            .SequenceEqual(route.Physical.Select(p => p.NodeUid)))
            throw new InvalidDataException("Physical/logical route sequences do not agree.");
        if (route.Physical.Any(p => !map.Nodes.ContainsKey(p.NodeUid)) ||
            route.Logical.Where(l => l.Type == 1).Any(l => !map.MapItems.TryGetValue(l.ItemUid,out var item) || item is not Prefab))
            throw new InvalidDataException("Route UIDs do not match the loaded map/profile.");
        RouteCorridor corridor; using(PerformanceTrace.Measure("corridor")) corridor = corridorBuilder.Build(route,token);
        AuditCorridor=corridor;
        if (corridor.Segments.Count != route.Physical.Count-1)
            throw new InvalidDataException("Incomplete route corridor.");
        foreach (var segment in corridor.Segments.Where(s => s.ItemType == nameof(Prefab)))
        {
            token.ThrowIfCancellationRequested();
            var prefab = (Prefab)map.MapItems[segment.ItemUid];
            var feature = maneuverFeatureExtractor.AnalyzeRoute(prefab,
                FindNodeIndex(prefab,route.Physical[segment.SegmentIndex].NodeUid),
                FindNodeIndex(prefab,route.Physical[segment.SegmentIndex+1].NodeUid),route,segment.SegmentIndex);
            if (!feature.PpdResolved || feature.SelectedCurveIds.Count == 0)
                throw new InvalidDataException($"Unresolved live PPD branch: segment {segment.SegmentIndex}, prefab {segment.ItemUid:X16}, model {prefab.Model}, entry {feature.MapEntryIndex}, exit {feature.MapExitIndex}, PPD {feature.PpdPath}: {feature.Error}");
        }
        using(PerformanceTrace.Measure("semantic-plan")) semanticRoutePlanner.Analyze(route,corridor,roadOverlayResolver.Match(corridor),"","","","",false);
        panelExtractor.BeginRoute();
        using(PerformanceTrace.Measure("route-lanes")) AuditLaneGraph=LaneRouteGraph.Build(map,route,corridor,decisionBranchResolver,roadLanes);
        using(PerformanceTrace.Measure("navigation-plan")) navigationPlanV11.Analyze(route,corridor,semanticRoutePlanner.LastRun!,"","","","","","","","",false);
        token.ThrowIfCancellationRequested();
        if(fastStart)AuditGroups=[];
        else using(PerformanceTrace.Measure("sign-groups")) AuditGroups=groupGuidance.Resolve(navigationPlanV11,panelExtractor,AuditLaneGraph,signResolver.IsKnownDestination);
        RouteDiagnostics.WriteText(Path.Combine(outputRoot,route.Stem+"_lanegraph.json"),()=>System.Text.Json.JsonSerializer.Serialize(
            AuditLaneGraph.Export(),new System.Text.Json.JsonSerializerOptions{PropertyNamingPolicy=System.Text.Json.JsonNamingPolicy.CamelCase}));
        Console.WriteLine($"Voies : {AuditLaneGraph.Pieces.Count(p=>p.Kind=="ROAD_LOOK_SAMPLED")} voies routieres ; "+
            $"{AuditLaneGraph.Boundaries.Count(b=>b.Status=="UNIQUE_GEOMETRIC_CANDIDATE")} raccordements candidats ; panneaux non calibres.");
        var plan=SignGroupGuidance.Attach(navigationPlanV11.LastLiveRoute ?? throw new InvalidDataException("No live plan produced."),AuditGroups.ToArray(),AuditLaneGraph);
        plan=plan with{Actions=plan.Actions.Select(action=>
        {
            var segment=corridor.Segments.Single(s=>s.SegmentIndex==action.SegmentIndex);
            if(!map.MapItems.TryGetValue(segment.ItemUid,out var item)||item is not Prefab prefab)return action;
            var feature=maneuverFeatureExtractor.AnalyzeRoute(prefab,FindNodeIndex(prefab,route.Physical[action.SegmentIndex].NodeUid),
                FindNodeIndex(prefab,route.Physical[action.SegmentIndex+1].NodeUid),route,action.SegmentIndex);
            var annotated=ppdResolver.TryResolve(prefab.Model.ToString(),out _,out var descriptor,out _)&&descriptor is not null?
                RoundaboutGuidance.Annotate(action,feature,descriptor):action;
            return feature.NativePathStatus=="NATIVE_GRAPH_AND_LANE_SEQUENCE_VALIDATED"?
                annotated with{Proofs=annotated.Proofs.Append(new LiveProof("path",string.Join('|',feature.SelectedCurveIds),"HIGH","NATIVE_NAVGRAPH",
                    prefab.Uid.ToString("X16"),annotated.BranchId,"Captured native nodes corroborate this directed forward lane sequence.")).ToArray()}:annotated;
        }).ToArray()};
        plan=AttachIdentities(route,corridor,plan,semanticRoutePlanner.LastRun!,preview?250:2000,token,preview);
        if(RichDiagnostics)
        {
            RouteDiagnostics.Try(()=>panelExtractor.WriteRoute(Path.Combine(outputRoot,route.Stem+"_panels.json")));
            RouteDiagnostics.Try(()=>GroupGuidanceView.Write(outputRoot,route.Stem,plan,AuditGroups,AuditLaneGraph));
        }
        // Standalone diagnostic snapshot remains available even if the session journals cannot be read yet.
        plan=RouteEvidence.Pack(plan,Path.Combine(AssetIndexCache.DirectoryFor(assetFiles)??outputRoot,"live-route-evidence"));
        RouteDiagnostics.WriteBytes(Path.Combine(outputRoot,route.Stem+"_route.json"),()=>LiveProtocol.Encode(
            new(2,"RouteSnapshot","map-bridge-diagnostic",0,0,1,plan)));
        return plan;
    }

    private LiveRoute AttachIdentities(ProbeRoute probe,RouteCorridor corridor,LiveRoute plan,V09SemanticRun run,double halo=2000,CancellationToken token=default,bool preview=false)
    {
        // A complete local region is prepared without GPS selection, then the directed occurrences are joined.
        // Include neighbouring interchanges and confirmations beyond the immediate route corridor.
        // Sector-aligned regions are independent of the moving GPS origin.
        // Nearby routes can reuse the same complete named graph, including all
        // sign evidence, under the mounted-asset/compiler cache fingerprint.
        var cell=preview?250:1000;
        var region=new NetworkRegion(Math.Floor((corridor.MinX-halo)/cell)*cell,Math.Ceiling((corridor.MaxX+halo)/cell)*cell,
            Math.Floor((corridor.MinZ-halo)/cell)*cell,Math.Ceiling((corridor.MaxZ+halo)/cell)*cell);
        var regionKey=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Json.JsonSerializer.SerializeToUtf8Bytes(
            new{region,sourceMargin=preview?200:0,nodes=map.Nodes.Where(p=>region.Contains(p.Value.Position)).Select(p=>p.Key).Order().ToArray()},LiveProtocol.Json)));
        DirectedRoadNetworkGraph? preparedGraph=null;
        NamedNetworkSnapshot BuildRegion()
        {
            DirectedRoadNetworkGraph fresh; using(PerformanceTrace.Measure("network-geometry"))fresh=DirectedRoadNetworkGraph.Build(map,region,roadLanes,decisionBranchResolver,WorldTileCache.For(assetFiles));
            preparedGraph=fresh;
            using(PerformanceTrace.Measure("network-names"))return namedNetwork.Build(fresh,token,preview?200:0);
        }
        NamedNetworkSnapshot local;using(PerformanceTrace.Measure("network-region"))local=AssetIndexCache.Get(assetFiles,"network-region-"+regionKey,BuildRegion)??BuildRegion();
        var graph=preparedGraph??new DirectedRoadNetworkGraph(local.Region,local.Portions,local.Connections,local.Errors);
        bool ValidateRegion(){using(PerformanceTrace.Measure("network-validation"))NamedNetworkValidation.Validate(local,graph);return true;}
        var validationKey=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(local.Fingerprint)));
        if(!AssetIndexCache.Get(assetFiles,"network-valid-"+validationKey,ValidateRegion))ValidateRegion();
        if(preview)local=PartialRoadEvidence.Filter(local);
        if(RichDiagnostics)RouteDiagnostics.Try(()=>NamedNetworkView.Write(outputRoot,probe.Stem,local,probe,corridor,plan));
        var data=NamedRoadNetwork.Join(local,probe,corridor,plan);
        plan=plan with{RoadIdentities=data,NavigationRemainingAtOriginM=probe.Physical[0].TotalDistanceTillEndM};
        plan=plan with{ServiceAreas=ServiceAreaGuidance.Build(map,graph,ppdResolver,plan)};
        plan=NamedRoadNetwork.AttachSignTargets(local,plan);
        LiveProtocol.Validate(new(2,"RouteSnapshot","identity-compile",0,0,1,plan));
        RouteDiagnostics.WriteText(Path.Combine(outputRoot,probe.Stem+"_road-identities.json"),()=>System.Text.Json.JsonSerializer.Serialize(data,LiveProtocol.Json));
        RouteDiagnostics.WriteText(Path.Combine(outputRoot,probe.Stem+"_identity-legacy-evidence.json"),()=>System.Text.Json.JsonSerializer.Serialize(
            new{run.Evidence,run.SolvedZones,run.Blocks},LiveProtocol.Json));
        if(RichDiagnostics)RouteDiagnostics.Try(()=>RoadIdentityView.Write(outputRoot,plan));
        Console.WriteLine($"Identites : {data.Spans.Count(s=>s.Status=="RESOLVED")}/{data.Spans.Length} segments nommes ; "+
            $"{data.Spans.Count(s=>s.Identities.Any(i=>i.IdentityConfidence=="HIGH"))} avec preuves HIGH ; {data.Anchors.Length} lectures auditees.");
        return plan;
    }

    public int IndexedSignCount => signResolver.Signs.Count;
    internal WorldTile PrepareWorldTile(int x,int z,CancellationToken token)
    {
        var region=new NetworkRegion(x*4000d-2000,(x+1)*4000d+2000,z*4000d-2000,(z+1)*4000d+2000);
        var graph=DirectedRoadNetworkGraph.Build(map,region,roadLanes,decisionBranchResolver);
        token.ThrowIfCancellationRequested();
        // Prepare model faces and local anchors, but never persist a pose aligned
        // against this tile's roads: that alignment belongs to the actual graph.
        _=namedNetwork.ReadSources(graph,token,2000);
        var readings=signResolver.Signs.Where(s=>WorldTileCache.Cell(s.Position)==(x,z)).OrderBy(s=>s.Uid)
            .Select(s=>{
                var areas=panelExtractor.Areas(s).ToArray();
                foreach(var area in areas)if(area.Extraction is {} panel)groupGuidance.PrepareAssets(s,panel);
                return new WorldSignReading(s.Uid,areas);
            }).ToArray();
        var portions=graph.Portions.GroupBy(p=>p.ItemUid).ToDictionary(g=>g.Key,g=>g.ToArray());
        var geometry=new List<WorldGeometry>();
        foreach(var item in map.MapItems.Values)
        {
            var position=item switch{Road r when r.Node is not null=>r.Node.Position,Prefab p when p.Nodes.Count>0=>p.Nodes[0].Position,_=>(System.Numerics.Vector3?)null};
            if(position is not {} p0||WorldTileCache.Cell(p0)!=(x,z))continue;
            var uid=item.Uid.ToString("X16");
            geometry.Add(new WorldGeometry(uid,portions.GetValueOrDefault(uid,[]),graph.Errors.Where(e=>e.StartsWith(uid+":",StringComparison.Ordinal)).ToArray()){GeometryVersion=2});
        }
        return new(x,z,readings,geometry.OrderBy(g=>g.ItemUid,StringComparer.Ordinal).ToArray());
    }
    public int SemanticSignCount => signResolver.SemanticSignCount;
    public int ExitSignCount => signResolver.ExitSignCount;
    public int RoadIdentitySignCount => signResolver.RoadIdentitySignCount;
    public int SignDefinitionCount => signResolver.DefinitionCount;
    public int RoadOverlayCount => roadOverlayResolver.Overlays.Count;
    public int NormalizedRoadOverlayCount => roadOverlayResolver.NormalizedCount;
    public int SignTemplateDefinitionCount => signTemplateResolver.TemplateDefinitionCount;
    public int UsedSignTemplateCount => signTemplateResolver.UsedTemplateCount;
    public int ResolvedUsedSignTemplateCount => signTemplateResolver.ResolvedUsedTemplateCount;
    public int UnsupportedUsedSignTemplateCount => signTemplateResolver.UnsupportedUsedTemplateCount;
    public int SignBoardDefinitionCount => signBoardResolver.BoardDefinitionCount;
    public int UsedSignBoardCount => signBoardResolver.UsedBoardCount;
    public int ResolvedUsedSignBoardCount => signBoardResolver.ResolvedUsedBoardCount;
    public int KnownDestinationCount => signResolver.KnownDestinationCount;

    public BridgeSummary Analyze(ProbeRoute route)
    {
        Directory.CreateDirectory(outputRoot);
        var nodesPath = Path.Combine(outputRoot, route.Stem + "_nodes.csv");
        var segmentsPath = Path.Combine(outputRoot, route.Stem + "_segments.csv");
        var logicalPath = Path.Combine(outputRoot, route.Stem + "_logical_items.csv");
        var maneuversPath = Path.Combine(outputRoot, route.Stem + "_maneuvers.csv");
        var routePlanPath = Path.Combine(outputRoot, route.Stem + "_route_plan.csv");
        var navGraphPath = Path.Combine(outputRoot, route.Stem + "_navgraph_route.csv");
        var maneuverFeaturesPath = Path.Combine(outputRoot, route.Stem + "_maneuver_features.csv");
        var laneBranchesPath = Path.Combine(outputRoot, route.Stem + "_lane_branches.csv");
        var signContextPath = Path.Combine(outputRoot, route.Stem + "_sign_context.csv");
        var navigationEventsPath = Path.Combine(outputRoot, route.Stem + "_navigation_events.csv");
        var routeIdentityPath = Path.Combine(outputRoot, route.Stem + "_route_identity.csv");
        var corridorPath = Path.Combine(outputRoot, route.Stem + "_route_corridor.csv");
        var corridorSegmentsPath = Path.Combine(outputRoot, route.Stem + "_route_corridor_segments.csv");
        var roadOverlayMatchesPath = Path.Combine(outputRoot, route.Stem + "_road_overlay_matches.csv");
        var routeIdentityV2Path = Path.Combine(outputRoot, route.Stem + "_route_identity_v2.csv");
        var signContextV2Path = Path.Combine(outputRoot, route.Stem + "_sign_context_v2.csv");
        var navigationEventsV2Path = Path.Combine(outputRoot, route.Stem + "_navigation_events_v2.csv");
        var routeEvidencePath = Path.Combine(outputRoot, route.Stem + "_route_evidence.csv");
        var semanticBlocksPath = Path.Combine(outputRoot, route.Stem + "_semantic_blocks.csv");
        var cruiseLegsPath = Path.Combine(outputRoot, route.Stem + "_cruise_legs.csv");
        var betterGpsPlanPath = Path.Combine(outputRoot, route.Stem + "_bettergps_plan.csv");
        var identityIntervalsV10Path = Path.Combine(outputRoot, route.Stem + "_identity_intervals.csv");
        var cruiseSpansV10Path = Path.Combine(outputRoot, route.Stem + "_cruise_spans.csv");
        var guidanceEventsV10Path = Path.Combine(outputRoot, route.Stem + "_guidance_events.csv");
        var branchSignEvidenceV10Path = Path.Combine(outputRoot, route.Stem + "_branch_sign_evidence.csv");
        var betterGpsPlanV2Path = Path.Combine(outputRoot, route.Stem + "_bettergps_plan_v2.csv");
        var driverActionsV11Path = Path.Combine(outputRoot, route.Stem + "_driver_actions_v11.csv");
        var boardSemanticsV11Path = Path.Combine(outputRoot, route.Stem + "_board_semantics_v11.csv");
        var branchBoardMatchesV11Path = Path.Combine(outputRoot, route.Stem + "_branch_board_matches_v11.csv");
        var identityIntervalsV11Path = Path.Combine(outputRoot, route.Stem + "_identity_intervals_v11.csv");
        var cruiseSpansV11Path = Path.Combine(outputRoot, route.Stem + "_cruise_spans_v11.csv");
        var guidanceEventsV11Path = Path.Combine(outputRoot, route.Stem + "_guidance_events_v11.csv");
        var betterGpsPlanV3Path = Path.Combine(outputRoot, route.Stem + "_bettergps_plan_v3.csv");
        var regressionV11Path = Path.Combine(outputRoot, route.Stem + "_v11_regression.txt");
        var summaryPath = Path.Combine(outputRoot, route.Stem + "_summary.txt");

        var type0 = route.Logical.Where(x => x.Type == 0).ToList();
        var physicalLogicalSame = type0.Count == route.Physical.Count &&
            type0.Select(x => x.ItemUid).SequenceEqual(route.Physical.Select(x => x.NodeUid));

        var nativeBetween = BuildNativeBetween(route);
        var nodeFound = 0;
        var logicalPrefabCount = 0;
        var logicalPrefabFound = 0;
        var logicalPrefabCorrectType = 0;
        var segmentResolved = 0;
        var roads = 0;
        var prefabs = 0;
        var otherItems = 0;
        var nativeHintMatches = 0;
        var nativeHintCompared = 0;
        var ppdPrefabs = 0;
        var ppdResolved = 0;
        var ppdPathFound = 0;
        var ppdDecisionCandidates = 0;

        using (var writer = Csv(nodesPath))
        {
            writer.WriteLine("physical_index,node_uid,found,x,y,z,forward_item_uid,forward_item_type,backward_item_uid,backward_item_type,distance_from_previous_m,total_distance_till_end_m");
            foreach (var row in route.Physical)
            {
                if (!map.Nodes.TryGetValue(row.NodeUid, out var node))
                {
                    writer.WriteLine($"{row.Index},{Hex(row.NodeUid)},0,,,,,,,,{F(row.DistanceFromPreviousM)},{F(row.TotalDistanceTillEndM)}");
                    continue;
                }
                nodeFound++;
                writer.WriteLine(string.Join(',',
                    row.Index, Hex(row.NodeUid), 1,
                    F(node.Position.X), F(node.Position.Y), F(node.Position.Z),
                    HexOrEmpty(node.ForwardItem?.Uid), Escape(ItemTypeName(node.ForwardItem)),
                    HexOrEmpty(node.BackwardItem?.Uid), Escape(ItemTypeName(node.BackwardItem)),
                    F(row.DistanceFromPreviousM), F(row.TotalDistanceTillEndM)));
            }
        }

        using (var writer = Csv(logicalPath))
        {
            writer.WriteLine("logical_index,native_type,node_id,item_uid,map_found,map_type,prefab_model,prefab_origin,prefab_node_count,total_distance_till_end_m");
            foreach (var row in route.Logical)
            {
                if (row.Type == 1) logicalPrefabCount++;
                if (row.Type == 0)
                {
                    var found = map.Nodes.TryGetValue(row.ItemUid, out _);
                    writer.WriteLine($"{row.Index},{row.Type},{row.NodeId},{Hex(row.ItemUid)},{(found ? 1 : 0)},Node,,,,{F(row.TotalDistanceTillEndM)}");
                    continue;
                }

                if (!map.MapItems.TryGetValue(row.ItemUid, out var item))
                {
                    writer.WriteLine($"{row.Index},{row.Type},{row.NodeId},{Hex(row.ItemUid)},0,,,,,{F(row.TotalDistanceTillEndM)}");
                    continue;
                }

                if (row.Type == 1) logicalPrefabFound++;
                if (item is Prefab prefab)
                {
                    if (row.Type == 1) logicalPrefabCorrectType++;
                    writer.WriteLine($"{row.Index},{row.Type},{row.NodeId},{Hex(row.ItemUid)},1,Prefab,{Escape(prefab.Model.ToString())},{prefab.Origin},{prefab.Nodes.Count},{F(row.TotalDistanceTillEndM)}");
                }
                else
                {
                    writer.WriteLine($"{row.Index},{row.Type},{row.NodeId},{Hex(row.ItemUid)},1,{Escape(item.ItemType.ToString())},,,,{F(row.TotalDistanceTillEndM)}");
                }
            }
        }

        using (var writer = Csv(segmentsPath))
        using (var maneuverWriter = Csv(maneuversPath))
        {
            writer.WriteLine("segment_index,start_node_uid,end_node_uid,start_found,end_found,item_resolved,item_uid,item_type,road_type,road_length_m,prefab_model,prefab_origin,prefab_node_count,prefab_entry_index,prefab_exit_index,native_prefab_count,native_prefab_uids,native_matches_item,distance_from_previous_m,end_total_distance_till_end_m");
            maneuverWriter.WriteLine("segment_index,item_uid,prefab_model,map_entry_index,map_exit_index,ppd_resolved,ppd_path,ppd_version,ppd_node_count,nav_curve_count,ppd_entry_index,ppd_exit_index,path_count,best_direction,best_path_length_m,best_curve_ids,best_start_lane,best_end_lane,nav_node_indices,turn_angle_deg,reachable_exit_count,reachable_ppd_exits,is_decision_candidate,geometry_class,native_prefab_count,native_node_ids,error");
            for (var i = 0; i + 1 < route.Physical.Count; i++)
            {
                var aRow = route.Physical[i];
                var bRow = route.Physical[i + 1];
                var aFound = map.Nodes.TryGetValue(aRow.NodeUid, out var a);
                var bFound = map.Nodes.TryGetValue(bRow.NodeUid, out var b);
                MapItem? item = null;
                if (aFound && bFound)
                    item = ResolveCommonMapItem(a!, b!);

                nativeBetween.TryGetValue(i, out var hints);
                hints ??= [];
                var nativeUidText = string.Join('|', hints.Select(h => Hex(h.ItemUid)));
                var nativeMatch = false;
                if (hints.Count > 0 && item != null)
                {
                    nativeHintCompared++;
                    nativeMatch = hints.Any(h => h.ItemUid == item.Uid);
                    if (nativeMatch) nativeHintMatches++;
                }

                var itemType = "";
                var roadType = "";
                var roadLength = "";
                var prefabModel = "";
                var prefabOrigin = "";
                var prefabNodeCount = "";
                var entryIndex = "";
                var exitIndex = "";
                if (item != null)
                {
                    segmentResolved++;
                    itemType = item.ItemType.ToString();
                    switch (item)
                    {
                        case Road road:
                            roads++;
                            roadType = road.RoadType.ToString();
                            roadLength = F(road.Length);
                            break;
                        case Prefab prefab:
                            prefabs++;
                            ppdPrefabs++;
                            prefabModel = prefab.Model.ToString();
                            prefabOrigin = prefab.Origin.ToString(CultureInfo.InvariantCulture);
                            prefabNodeCount = prefab.Nodes.Count.ToString(CultureInfo.InvariantCulture);
                            var entry = FindNodeIndex(prefab, aRow.NodeUid);
                            var exit = FindNodeIndex(prefab, bRow.NodeUid);
                            entryIndex = entry.ToString(CultureInfo.InvariantCulture);
                            exitIndex = exit.ToString(CultureInfo.InvariantCulture);

                            var analysis = ppdAnalyzer.Analyze(prefab, entry, exit);
                            if (analysis.PpdResolved) ppdResolved++;
                            if (analysis.BestPath is not null) ppdPathFound++;
                            if (analysis.IsDecisionCandidate) ppdDecisionCandidates++;

                            var best = analysis.BestPath;
                            var nativeNodeIds = string.Join('|', hints.Select(h => h.NodeId.ToString(CultureInfo.InvariantCulture)));
                            maneuverWriter.WriteLine(string.Join(',',
                                i, Hex(prefab.Uid), Escape(prefabModel), entry, exit,
                                analysis.PpdResolved ? 1 : 0, Escape(analysis.PpdPath),
                                analysis.PpdVersion?.ToString(CultureInfo.InvariantCulture) ?? "",
                                analysis.PpdNodeCount, analysis.NavCurveCount, analysis.PpdEntryIndex, analysis.PpdExitIndex,
                                analysis.PathCount, Escape(best?.Direction.ToString() ?? ""),
                                best is null ? "" : F(best.LengthM),
                                Escape(best is null ? "" : string.Join('|', best.CurveIds)),
                                best?.StartLane.ToString(CultureInfo.InvariantCulture) ?? "",
                                best?.EndLane.ToString(CultureInfo.InvariantCulture) ?? "",
                                Escape(best is null ? "" : string.Join('|', best.NavNodeIndices.Select(n => n == uint.MaxValue ? "-1" : n.ToString(CultureInfo.InvariantCulture)))),
                                best is null ? "" : F(best.TurnAngleDeg),
                                analysis.ReachableExitCount, Escape(string.Join('|', analysis.ReachablePpdExitIndices)),
                                analysis.IsDecisionCandidate ? 1 : 0, Escape(analysis.GeometryClass),
                                hints.Count, Escape(nativeNodeIds), Escape(analysis.Error)));
                            break;
                        default:
                            otherItems++;
                            break;
                    }
                }

                writer.WriteLine(string.Join(',',
                    i, Hex(aRow.NodeUid), Hex(bRow.NodeUid), aFound ? 1 : 0, bFound ? 1 : 0,
                    item != null ? 1 : 0, item != null ? Hex(item.Uid) : "", Escape(itemType),
                    Escape(roadType), roadLength, Escape(prefabModel), prefabOrigin, prefabNodeCount,
                    entryIndex, exitIndex, hints.Count, Escape(nativeUidText), nativeMatch ? 1 : 0,
                    F(bRow.DistanceFromPreviousM), F(bRow.TotalDistanceTillEndM)));
            }
        }

        WriteNativeRoutePlan(route, routePlanPath);
        var navGraphSummary = WriteExactNavGraphRoute(route, navGraphPath);
        var maneuverFeatureSummary = WriteManeuverFeatures(route, maneuverFeaturesPath, laneBranchesPath);
        var signSemanticSummary = WriteSignSemantics(route, signContextPath, navigationEventsPath, routeIdentityPath);
        var corridorSemanticResult = WriteCorridorSemanticsV2(route, corridorPath, corridorSegmentsPath, roadOverlayMatchesPath, routeIdentityV2Path, signContextV2Path, navigationEventsV2Path);
        var corridorSemanticSummary = corridorSemanticResult.Summary;
        var semanticV09 = semanticRoutePlanner.Analyze(
            route,
            corridorSemanticResult.Corridor,
            corridorSemanticResult.OverlayMatches,
            routeEvidencePath,
            semanticBlocksPath,
            cruiseLegsPath,
            betterGpsPlanPath);

        if (semanticRoutePlanner.LastRun is not null)
        {
            LastV10Summary = navigationPlanV10.Analyze(
                corridorSemanticResult.Corridor,
                semanticRoutePlanner.LastRun,
                identityIntervalsV10Path,
                cruiseSpansV10Path,
                guidanceEventsV10Path,
                branchSignEvidenceV10Path,
                betterGpsPlanV2Path);

            panelExtractor.BeginRoute();
            LastV11Summary = navigationPlanV11.Analyze(
                route,
                corridorSemanticResult.Corridor,
                semanticRoutePlanner.LastRun,
                driverActionsV11Path,
                boardSemanticsV11Path,
                branchBoardMatchesV11Path,
                identityIntervalsV11Path,
                cruiseSpansV11Path,
                guidanceEventsV11Path,
                betterGpsPlanV3Path,
                regressionV11Path);
            panelExtractor.WriteRoute(Path.Combine(outputRoot,route.Stem+"_panels.json"));
            // The offline export uses exactly the same validation, grouping and identity compiler as live.
            // An invalid snapshot must not leave a stale transport snapshot available to pack.
            var liveSnapshot=Path.Combine(outputRoot,route.Stem+"_route.json");
            if(File.Exists(liveSnapshot))File.Delete(liveSnapshot);
            try{CompileLive(route);}
            catch(InvalidDataException ex)
            {
                RouteDiagnostics.WriteText(Path.Combine(outputRoot,route.Stem+"_identity-rejection.txt"),ex.Message);
                Console.WriteLine("Identity live export rejected: "+ex.Message);
            }
        }
        else
        {
            LastV10Summary = null;
            LastV11Summary = null;
        }

        var summary = new BridgeSummary(
            route.Stem,
            route.Physical.Count,
            nodeFound,
            route.Physical.Count > 0 ? (double)nodeFound / route.Physical.Count : 0,
            route.Logical.Count,
            logicalPrefabCount,
            logicalPrefabFound,
            logicalPrefabCorrectType,
            Math.Max(0, route.Physical.Count - 1),
            segmentResolved,
            roads,
            prefabs,
            otherItems,
            nativeHintCompared,
            nativeHintMatches,
            ppdPrefabs,
            ppdResolved,
            ppdPathFound,
            ppdDecisionCandidates,
            navGraphSummary.Events,
            navGraphSummary.ExactPaths,
            navGraphSummary.StructuralValidated,
            navGraphSummary.DistanceMatches2m,
            navGraphSummary.StrongValidated,
            maneuverFeatureSummary.PrefabSegments,
            maneuverFeatureSummary.Resolved,
            maneuverFeatureSummary.SelectedPaths,
            maneuverFeatureSummary.Transparent,
            maneuverFeatureSummary.Turns,
            maneuverFeatureSummary.Branches,
            maneuverFeatureSummary.Merges,
            maneuverFeatureSummary.Forks,
            maneuverFeatureSummary.CycleCandidates,
            maneuverFeatureSummary.Other,
            signSemanticSummary.Events,
            signSemanticSummary.EventsWithSignContext,
            signSemanticSummary.ExitCandidates,
            signSemanticSummary.KeepCandidates,
            signSemanticSummary.EventsWithApproachRoadIdentity,
            signSemanticSummary.EventsWithAfterRoadIdentity,
            corridorSemanticSummary.CorridorEdges,
            corridorSemanticSummary.RoadOverlayMatches,
            corridorSemanticSummary.HighRoadOverlayMatches,
            corridorSemanticSummary.IdentityResolvedSegments,
            corridorSemanticSummary.V2Events,
            corridorSemanticSummary.V2ExitCandidates,
            corridorSemanticSummary.V2KeepCandidates,
            corridorSemanticSummary.AmbiguousExitEvents,
            semanticV09.Zones,
            semanticV09.IdentifiedZones,
            semanticV09.UnknownZones,
            semanticV09.SemanticBlocks,
            semanticV09.NamedBlocks,
            semanticV09.CardinalResolvedBlocks,
            semanticV09.CruiseLegs,
            semanticV09.NamedCruiseLegs,
            semanticV09.PlanEntries,
            semanticV09.ConcurrentEvidenceZones,
            semanticV09.NamedCoverageRatio,
            physicalLogicalSame);

        var v10Text = LastV10Summary is null ? "" : $"""

v10_identity_intervals={LastV10Summary.IdentityIntervals}
v10_primary_intervals={LastV10Summary.PrimaryIntervals}
v10_alias_intervals={LastV10Summary.AliasIntervals}
v10_named_primary_intervals={LastV10Summary.NamedPrimaryIntervals}
v10_cruise_spans={LastV10Summary.CruiseSpans}
v10_named_cruise_spans={LastV10Summary.NamedCruiseSpans}
v10_connector_spans={LastV10Summary.ConnectorSpans}
v10_guidance_events={LastV10Summary.GuidanceEvents}
v10_plan_entries={LastV10Summary.PlanEntries}
v10_branch_sign_evidence={LastV10Summary.BranchSignEvidence}
v10_high_branch_sign_evidence={LastV10Summary.HighBranchSignEvidence}
v10_branch_promoted_zones={LastV10Summary.BranchPromotedZones}
v10_named_coverage_ratio={LastV10Summary.NamedCoverageRatio.ToString("0.######", CultureInfo.InvariantCulture)}
""";
        var v11Text = LastV11Summary is null ? "" : $"""

v11_identity_intervals={LastV11Summary.IdentityIntervals}
v11_primary_intervals={LastV11Summary.PrimaryIntervals}
v11_alias_intervals={LastV11Summary.AliasIntervals}
v11_named_primary_intervals={LastV11Summary.NamedPrimaryIntervals}
v11_cruise_spans={LastV11Summary.CruiseSpans}
v11_named_cruise_spans={LastV11Summary.NamedCruiseSpans}
v11_connector_spans={LastV11Summary.ConnectorSpans}
v11_guidance_events={LastV11Summary.GuidanceEvents}
v11_suppressed_events={LastV11Summary.SuppressedEvents}
v11_board_semantics={LastV11Summary.BoardSemantics}
v11_branch_board_matches={LastV11Summary.BranchBoardMatches}
v11_high_branch_board_matches={LastV11Summary.HighBranchBoardMatches}
v11_ambiguous_branch_board_matches={LastV11Summary.AmbiguousBranchBoardMatches}
v11_conflicting_branch_board_matches={LastV11Summary.ConflictingBranchBoardMatches}
v11_branch_promoted_zones={LastV11Summary.BranchPromotedZones}
v11_high_cardinal_intervals={LastV11Summary.HighCardinalIntervals}
v11_geometry_derived_signed_cardinals={LastV11Summary.GeometryDerivedSignedCardinals}
v11_multi_high_identity_events={LastV11Summary.MultiHighIdentityEvents}
v11_destinations_without_high_branch_match={LastV11Summary.DestinationsWithoutHighBranchMatch}
v11_unnecessary_straight_keeps={LastV11Summary.UnnecessaryStraightKeeps}
v11_high_matches_below_required_margin={LastV11Summary.HighMatchesBelowRequiredMargin}
v11_plan_entries={LastV11Summary.PlanEntries}
v11_named_coverage_ratio={LastV11Summary.NamedCoverageRatio.ToString("0.######", CultureInfo.InvariantCulture)}
""";
        RouteDiagnostics.WriteText(summaryPath, summary.ToText() + v10Text + v11Text, Encoding.UTF8);
        return summary;
    }

    private void WriteNativeRoutePlan(ProbeRoute route, string path)
    {
        var nativeRows = route.Logical
            .Where(r => r.Type == 1)
            .OrderBy(r => r.Index)
            .ToList();
        var segmentByLogicalIndex = BuildNativeSegmentIndex(route);
        var routeStartRemaining = route.SnapshotTripDistanceM
            ?? (route.Physical.Count > 0 ? route.Physical[0].TotalDistanceTillEndM : 0.0);

        using var writer = Csv(path);
        writer.WriteLine("event_index,logical_index,segment_index,item_uid,prefab_model,native_node_id,event_total_distance_till_end_m,distance_from_snapshot_to_event_m,distance_to_next_native_event_m,next_native_prefab_model,next_native_node_id,geometry_class,turn_angle_deg,reachable_exit_count,reachable_ppd_exits,best_direction,best_curve_ids,selected_path_nav_node_indices,ppd_nav_node_count,native_node_id_in_range,native_navnode_type,native_navnode_index_value,native_navnode_connection_count,native_navnode_connections,navnode_list_indices_whose_index_equals_native_id,native_id_appears_in_selected_path_navnode_indices,selected_path_curve_overlap_count,selected_path_curve_overlap_ids,error");

        for (var eventIndex = 0; eventIndex < nativeRows.Count; eventIndex++)
        {
            var row = nativeRows[eventIndex];
            var next = eventIndex + 1 < nativeRows.Count ? nativeRows[eventIndex + 1] : null;
            var segmentIndex = segmentByLogicalIndex.TryGetValue(row.Index, out var si) ? si : -1;
            var distanceFromStart = routeStartRemaining - row.TotalDistanceTillEndM;
            var distanceToNext = next is null ? "" : F(row.TotalDistanceTillEndM - next.TotalDistanceTillEndM);
            var nextModel = "";
            var nextNodeId = "";
            if (next is not null && map.MapItems.TryGetValue(next.ItemUid, out var nextItem) && nextItem is Prefab nextPrefab)
            {
                nextModel = nextPrefab.Model.ToString();
                nextNodeId = next.NodeId.ToString(CultureInfo.InvariantCulture);
            }

            string prefabModel = "";
            string geometryClass = "";
            string angle = "";
            string reachableExitCount = "";
            string reachableExits = "";
            string bestDirection = "";
            string bestCurveIds = "";
            string selectedPathNavNodeIndices = "";
            string ppdNavNodeCount = "";
            string nativeNodeIdInRange = "";
            string nativeNavNodeType = "";
            string nativeNavNodeIndexValue = "";
            string nativeNavNodeConnectionCount = "";
            string nativeNavNodeConnections = "";
            string matchingIndexValues = "";
            string appearsInPath = "";
            string overlapCount = "";
            string overlapIds = "";
            string error = "";

            if (segmentIndex < 0 || segmentIndex + 1 >= route.Physical.Count)
            {
                error = "Native logical prefab could not be mapped to a physical route segment.";
            }
            else if (!map.Nodes.TryGetValue(route.Physical[segmentIndex].NodeUid, out var a) ||
                     !map.Nodes.TryGetValue(route.Physical[segmentIndex + 1].NodeUid, out var b))
            {
                error = "Physical route nodes around native prefab were not found in the map.";
            }
            else if (ResolveCommonMapItem(a, b) is not Prefab prefab || prefab.Uid != row.ItemUid)
            {
                error = "Native route prefab did not match the common map prefab for its physical segment.";
            }
            else
            {
                prefabModel = prefab.Model.ToString();
                var entry = FindNodeIndex(prefab, route.Physical[segmentIndex].NodeUid);
                var exit = FindNodeIndex(prefab, route.Physical[segmentIndex + 1].NodeUid);
                var maneuver = ppdAnalyzer.Analyze(prefab, entry, exit);
                var nativeNode = ppdAnalyzer.AnalyzeNativeNode(prefab, row.NodeId, maneuver);
                var best = maneuver.BestPath;

                geometryClass = maneuver.GeometryClass;
                angle = best is null ? "" : F(best.TurnAngleDeg);
                reachableExitCount = maneuver.ReachableExitCount.ToString(CultureInfo.InvariantCulture);
                reachableExits = string.Join('|', maneuver.ReachablePpdExitIndices);
                bestDirection = best?.Direction.ToString() ?? "";
                bestCurveIds = best is null ? "" : string.Join('|', best.CurveIds);
                selectedPathNavNodeIndices = best is null ? "" : string.Join('|', best.NavNodeIndices.Select(n => n == uint.MaxValue ? "-1" : n.ToString(CultureInfo.InvariantCulture)));

                ppdNavNodeCount = nativeNode.PpdNavNodeCount.ToString(CultureInfo.InvariantCulture);
                nativeNodeIdInRange = nativeNode.NativeNodeIdInRange ? "1" : "0";
                nativeNavNodeType = nativeNode.NativeNavNodeType;
                nativeNavNodeIndexValue = nativeNode.NativeNavNodeIndexValue?.ToString(CultureInfo.InvariantCulture) ?? "";
                nativeNavNodeConnectionCount = nativeNode.NativeNavNodeConnectionCount.ToString(CultureInfo.InvariantCulture);
                nativeNavNodeConnections = nativeNode.NativeNavNodeConnections;
                matchingIndexValues = string.Join('|', nativeNode.NavNodeListIndicesWhoseIndexEqualsNativeId);
                appearsInPath = nativeNode.NativeIdAppearsInSelectedPathNavNodeIndices ? "1" : "0";
                overlapCount = nativeNode.SelectedPathCurveOverlapCount.ToString(CultureInfo.InvariantCulture);
                overlapIds = string.Join('|', nativeNode.SelectedPathCurveOverlapIds);
                error = maneuver.Error ?? nativeNode.Error ?? "";
            }

            writer.WriteLine(string.Join(',',
                eventIndex,
                row.Index,
                segmentIndex,
                Hex(row.ItemUid),
                Escape(prefabModel),
                row.NodeId,
                F(row.TotalDistanceTillEndM),
                F(distanceFromStart),
                distanceToNext,
                Escape(nextModel),
                nextNodeId,
                Escape(geometryClass),
                angle,
                reachableExitCount,
                Escape(reachableExits),
                Escape(bestDirection),
                Escape(bestCurveIds),
                Escape(selectedPathNavNodeIndices),
                ppdNavNodeCount,
                nativeNodeIdInRange,
                Escape(nativeNavNodeType),
                nativeNavNodeIndexValue,
                nativeNavNodeConnectionCount,
                Escape(nativeNavNodeConnections),
                Escape(matchingIndexValues),
                appearsInPath,
                overlapCount,
                Escape(overlapIds),
                Escape(error)));
        }
    }

    private NativeNavGraphSummary WriteExactNavGraphRoute(ProbeRoute route, string path)
    {
        var nativeRows = route.Logical
            .Where(r => r.Type == 1)
            .OrderBy(r => r.Index)
            .ToList();
        var segmentByLogicalIndex = BuildNativeSegmentIndex(route);

        var exactPaths = 0;
        var structuralValidated = 0;
        var distanceMatches2m = 0;
        var strongValidated = 0;

        using var writer = Csv(path);
        writer.WriteLine("event_index,logical_index,segment_index,item_uid,prefab_model,native_node_id,map_entry_index,map_exit_index,ppd_entry_index,ppd_exit_index,entry_physical_navnode_ids,exit_physical_navnode_ids,ppd_navnode_count,native_node_id_in_range,native_navnode_type,native_navnode_index_value,candidate_path_count,candidate_orientation_count,graph_orientation,exact_path_found,directed_node_path,directed_curve_ids,vehicle_node_path,vehicle_curve_ids,directed_edges,vehicle_edges,native_vehicle_path_position,vehicle_incoming_source_node_id,vehicle_incoming_curve_ids,vehicle_outgoing_target_node_id,vehicle_outgoing_curve_ids,graph_entry_to_native_m,graph_native_to_exit_m,graph_total_m,route_entry_to_native_m,route_native_to_exit_m,route_total_m,entry_distance_delta_m,exit_distance_delta_m,total_distance_delta_m,distance_match_2m,native_index_curve_on_exact_path,native_on_unconstrained_shortest_path,constrained_equals_unconstrained_shortest_path,unconstrained_node_path,unconstrained_curve_ids,heuristic_best_direction,heuristic_curve_ids,heuristic_curve_overlap_count,heuristic_curve_sequence_match,validation_status,error");

        for (var eventIndex = 0; eventIndex < nativeRows.Count; eventIndex++)
        {
            var row = nativeRows[eventIndex];
            var segmentIndex = segmentByLogicalIndex.TryGetValue(row.Index, out var si) ? si : -1;

            string prefabModel = "";
            int mapEntry = -1;
            int mapExit = -1;
            PpdManeuverResult? heuristic = null;
            ExactNavGraphResult? exact = null;
            string error = "";

            if (segmentIndex < 0 || segmentIndex + 1 >= route.Physical.Count)
            {
                error = "Native logical prefab could not be mapped to a physical route segment.";
            }
            else if (!map.Nodes.TryGetValue(route.Physical[segmentIndex].NodeUid, out var a) ||
                     !map.Nodes.TryGetValue(route.Physical[segmentIndex + 1].NodeUid, out var b))
            {
                error = "Physical route nodes around native prefab were not found in the map.";
            }
            else if (ResolveCommonMapItem(a, b) is not Prefab prefab || prefab.Uid != row.ItemUid)
            {
                error = "Native route prefab did not match the common map prefab for its physical segment.";
            }
            else
            {
                prefabModel = prefab.Model.ToString();
                mapEntry = FindNodeIndex(prefab, route.Physical[segmentIndex].NodeUid);
                mapExit = FindNodeIndex(prefab, route.Physical[segmentIndex + 1].NodeUid);
                heuristic = ppdAnalyzer.Analyze(prefab, mapEntry, mapExit);

                var routeEntryToNative = route.Physical[segmentIndex].TotalDistanceTillEndM - row.TotalDistanceTillEndM;
                var routeNativeToExit = row.TotalDistanceTillEndM - route.Physical[segmentIndex + 1].TotalDistanceTillEndM;

                exact = exactNavGraphResolver.Resolve(
                    prefab,
                    mapEntry,
                    mapExit,
                    row.NodeId,
                    routeEntryToNative,
                    routeNativeToExit,
                    heuristic.BestPath);

                if (exact.ExactPathFound) exactPaths++;
                if (exact.ValidationStatus == "VALIDATED_NODE_ID_AS_NAVNODE_INDEX") structuralValidated++;
                if (exact.DistanceMatch2m) distanceMatches2m++;
                if (exact.ValidationStatus == "VALIDATED_NODE_ID_AS_NAVNODE_INDEX" && exact.DistanceMatch2m) strongValidated++;
                error = exact.Error ?? heuristic.Error ?? "";
            }

            writer.WriteLine(string.Join(',',
                eventIndex,
                row.Index,
                segmentIndex,
                Hex(row.ItemUid),
                Escape(prefabModel),
                row.NodeId,
                mapEntry,
                mapExit,
                exact?.PpdEntryIndex.ToString(CultureInfo.InvariantCulture) ?? "",
                exact?.PpdExitIndex.ToString(CultureInfo.InvariantCulture) ?? "",
                Escape(exact is null ? "" : string.Join('|', exact.EntryPhysicalNavNodeIds)),
                Escape(exact is null ? "" : string.Join('|', exact.ExitPhysicalNavNodeIds)),
                exact?.PpdNavNodeCount.ToString(CultureInfo.InvariantCulture) ?? "",
                exact is null ? "" : (exact.NativeNodeIdInRange ? "1" : "0"),
                Escape(exact?.NativeNavNodeType ?? ""),
                exact?.NativeNavNodeIndexValue?.ToString(CultureInfo.InvariantCulture) ?? "",
                exact?.CandidatePathCount.ToString(CultureInfo.InvariantCulture) ?? "",
                exact?.CandidateOrientationCount.ToString(CultureInfo.InvariantCulture) ?? "",
                Escape(exact?.Orientation.ToString() ?? ""),
                exact is null ? "" : (exact.ExactPathFound ? "1" : "0"),
                Escape(exact is null ? "" : string.Join('|', exact.DirectedNodePath)),
                Escape(exact is null ? "" : string.Join('|', exact.DirectedCurveIds)),
                Escape(exact is null ? "" : string.Join('|', exact.VehicleNodePath)),
                Escape(exact is null ? "" : string.Join('|', exact.VehicleCurveIds)),
                Escape(exact?.DirectedEdges ?? ""),
                Escape(exact?.VehicleEdges ?? ""),
                exact?.NativeVehiclePathPosition.ToString(CultureInfo.InvariantCulture) ?? "",
                exact?.VehicleIncomingSourceNodeId?.ToString(CultureInfo.InvariantCulture) ?? "",
                Escape(exact is null ? "" : string.Join('|', exact.VehicleIncomingCurveIds)),
                exact?.VehicleOutgoingTargetNodeId?.ToString(CultureInfo.InvariantCulture) ?? "",
                Escape(exact is null ? "" : string.Join('|', exact.VehicleOutgoingCurveIds)),
                exact is null ? "" : F(exact.GraphEntryToNativeM),
                exact is null ? "" : F(exact.GraphNativeToExitM),
                exact is null ? "" : F(exact.GraphTotalM),
                exact is null ? "" : F(exact.RouteEntryToNativeM),
                exact is null ? "" : F(exact.RouteNativeToExitM),
                exact is null ? "" : F(exact.RouteTotalM),
                exact is null ? "" : F(exact.EntryDistanceDeltaM),
                exact is null ? "" : F(exact.ExitDistanceDeltaM),
                exact is null ? "" : F(exact.TotalDistanceDeltaM),
                exact is null ? "" : (exact.DistanceMatch2m ? "1" : "0"),
                exact is null ? "" : (exact.NativeIndexCurveOnExactPath ? "1" : "0"),
                exact is null ? "" : (exact.NativeOnUnconstrainedShortestPath ? "1" : "0"),
                exact is null ? "" : (exact.ConstrainedEqualsUnconstrainedShortestPath ? "1" : "0"),
                Escape(exact is null ? "" : string.Join('|', exact.UnconstrainedNodePath)),
                Escape(exact is null ? "" : string.Join('|', exact.UnconstrainedCurveIds)),
                Escape(heuristic?.BestPath?.Direction.ToString() ?? ""),
                Escape(heuristic?.BestPath is null ? "" : string.Join('|', heuristic.BestPath.CurveIds)),
                exact?.HeuristicCurveOverlapCount.ToString(CultureInfo.InvariantCulture) ?? "",
                exact is null ? "" : (exact.HeuristicCurveSequenceMatch ? "1" : "0"),
                Escape(exact?.ValidationStatus ?? ""),
                Escape(error)));
        }

        return new NativeNavGraphSummary(
            nativeRows.Count,
            exactPaths,
            structuralValidated,
            distanceMatches2m,
            strongValidated);
    }

    private ManeuverFeatureSummary WriteManeuverFeatures(ProbeRoute route, string featurePath, string branchPath)
    {
        var prefabSegments = 0;
        var resolved = 0;
        var selectedPaths = 0;
        var transparent = 0;
        var turns = 0;
        var branches = 0;
        var merges = 0;
        var forks = 0;
        var cycleCandidates = 0;
        var other = 0;

        using var writer = Csv(featurePath);
        using var branchWriter = Csv(branchPath);

        writer.WriteLine("segment_index,item_uid,prefab_model,map_entry_index,map_exit_index,ppd_entry_index,ppd_exit_index,ppd_resolved,input_lane_count,selected_lane_count,selected_lane_ordinals,selected_touches_divider_side,selected_touches_outer_side,branch_count,selected_branch_count,reachable_exit_count,reachable_ppd_exits,alternative_exit_count,other_entry_nodes_to_selected_exit,all_input_lanes_reach_selected_exit,representative_angle_deg,selected_physical_length_m,route_segment_distance_m,route_to_physical_scale_ratio,selected_geometry,selected_side,selected_curve_ids,selected_blinkers,selected_has_left_blinker,selected_has_right_blinker,selected_has_forced_no_blinker,selected_traffic_rules,selected_allowed_vehicles,cycle_encountered,map_point_count,map_point_prefab_exit_count,map_point_no_arrow_count,entry_nearest_map_point_distance_m,entry_nearest_map_point_road_size,entry_nearest_map_point_road_offset,entry_nearest_map_point_prefab_exit,entry_nearest_map_point_arrow,exit_nearest_map_point_distance_m,exit_nearest_map_point_road_size,exit_nearest_map_point_road_offset,exit_nearest_map_point_prefab_exit,exit_nearest_map_point_arrow,before_item_type,before_item_uid,before_road_type,before_road_gps_avoid,before_road_show_ui,before_road_ai_vehicles,after_item_type,after_item_uid,after_road_type,after_road_gps_avoid,after_road_show_ui,after_road_ai_vehicles,preliminary_class,confidence,error");
        branchWriter.WriteLine("segment_index,item_uid,prefab_model,map_entry_index,map_exit_index,ppd_entry_index,ppd_exit_index,lane_ordinal,start_curve_id,boundary_lane_id,target_ppd_node_index,selected,curve_ids,length_m,angle_deg,geometry,side,blinkers,traffic_rules,allowed_vehicles,low_probability,limit_displacement,cycle_encountered");

        for (var i = 0; i + 1 < route.Physical.Count; i++)
        {
            if (!map.Nodes.TryGetValue(route.Physical[i].NodeUid, out var a) ||
                !map.Nodes.TryGetValue(route.Physical[i + 1].NodeUid, out var b))
                continue;

            if (ResolveCommonMapItem(a, b) is not Prefab prefab)
                continue;

            prefabSegments++;
            var entry = FindNodeIndex(prefab, route.Physical[i].NodeUid);
            var exit = FindNodeIndex(prefab, route.Physical[i + 1].NodeUid);
            var result = maneuverFeatureExtractor.Analyze(prefab, entry, exit);
            if (result.PpdResolved) resolved++;
            if (result.SelectedBranchCount > 0) selectedPaths++;

            if (result.AnyCycleEncountered) cycleCandidates++;

            switch (result.PreliminaryClass)
            {
                case "TRANSPARENT": transparent++; break;
                case "BRANCH_LEFT":
                case "BRANCH_RIGHT": branches++; break;
                case "MERGE_LEFT":
                case "MERGE_RIGHT": merges++; break;
                case "FORK_STRAIGHT": forks++; break;
                default:
                    if (result.PreliminaryClass.StartsWith("TURN_", StringComparison.Ordinal) ||
                        result.PreliminaryClass.StartsWith("SHARP_", StringComparison.Ordinal) ||
                        result.PreliminaryClass.StartsWith("U_TURN_", StringComparison.Ordinal))
                        turns++;
                    else
                        other++;
                    break;
            }

            var before = i > 0 && map.Nodes.TryGetValue(route.Physical[i - 1].NodeUid, out var beforeA)
                ? ResolveCommonMapItem(beforeA, a)
                : null;
            var after = i + 2 < route.Physical.Count && map.Nodes.TryGetValue(route.Physical[i + 2].NodeUid, out var afterB)
                ? ResolveCommonMapItem(b, afterB)
                : null;
            var beforeCtx = DescribeContext(before);
            var afterCtx = DescribeContext(after);

            var routeDistance = route.Physical[i + 1].DistanceFromPreviousM;
            var ratio = result.SelectedPhysicalLengthM.HasValue && result.SelectedPhysicalLengthM.Value > 1e-6
                ? routeDistance / result.SelectedPhysicalLengthM.Value
                : (double?)null;
            var mp = result.MapPoints;

            writer.WriteLine(string.Join(',',
                i,
                Hex(prefab.Uid),
                Escape(prefab.Model.ToString()),
                entry,
                exit,
                result.PpdEntryIndex,
                result.PpdExitIndex,
                result.PpdResolved ? 1 : 0,
                result.InputLaneCount,
                result.SelectedLaneCount,
                Escape(string.Join('|', result.SelectedLaneOrdinals)),
                result.SelectedTouchesDividerSide ? 1 : 0,
                result.SelectedTouchesOuterSide ? 1 : 0,
                result.BranchCount,
                result.SelectedBranchCount,
                result.ReachableExitCount,
                Escape(string.Join('|', result.ReachableExitIndices)),
                result.AlternativeExitCount,
                result.OtherEntryNodesToSelectedExit,
                result.AllInputLanesReachSelectedExit ? 1 : 0,
                result.RepresentativeAngleDeg.HasValue ? F(result.RepresentativeAngleDeg.Value) : "",
                result.SelectedPhysicalLengthM.HasValue ? F(result.SelectedPhysicalLengthM.Value) : "",
                F(routeDistance),
                ratio.HasValue ? F(ratio.Value) : "",
                Escape(result.SelectedGeometry),
                Escape(result.SelectedSide),
                Escape(string.Join('|', result.SelectedCurveIds)),
                Escape(string.Join('|', result.SelectedBlinkers)),
                result.SelectedHasLeftBlinker ? 1 : 0,
                result.SelectedHasRightBlinker ? 1 : 0,
                result.SelectedHasForcedNoBlinker ? 1 : 0,
                Escape(string.Join('|', result.SelectedTrafficRules)),
                Escape(string.Join('|', result.SelectedAllowedVehicles)),
                result.AnyCycleEncountered ? 1 : 0,
                mp.Count,
                mp.PrefabExitCount,
                mp.NoArrowCount,
                mp.EntryNearestDistanceM.HasValue ? F(mp.EntryNearestDistanceM.Value) : "",
                Escape(mp.EntryNearestRoadSize),
                Escape(mp.EntryNearestRoadOffset),
                BoolOrEmpty(mp.EntryNearestPrefabExit),
                BoolOrEmpty(mp.EntryNearestArrow),
                mp.ExitNearestDistanceM.HasValue ? F(mp.ExitNearestDistanceM.Value) : "",
                Escape(mp.ExitNearestRoadSize),
                Escape(mp.ExitNearestRoadOffset),
                BoolOrEmpty(mp.ExitNearestPrefabExit),
                BoolOrEmpty(mp.ExitNearestArrow),
                Escape(beforeCtx.ItemType),
                beforeCtx.Uid,
                Escape(beforeCtx.RoadType),
                beforeCtx.GpsAvoid,
                beforeCtx.ShowUi,
                beforeCtx.AiVehicles,
                Escape(afterCtx.ItemType),
                afterCtx.Uid,
                Escape(afterCtx.RoadType),
                afterCtx.GpsAvoid,
                afterCtx.ShowUi,
                afterCtx.AiVehicles,
                Escape(result.PreliminaryClass),
                Escape(result.Confidence),
                Escape(result.Error ?? "")));

            foreach (var branch in result.Branches.OrderBy(x => x.LaneOrdinal).ThenBy(x => x.TargetPpdNodeIndex).ThenBy(x => x.LengthM))
            {
                var geometry = GeometryForReport(branch.AngleDeg);
                var side = SideForReport(branch.AngleDeg);
                branchWriter.WriteLine(string.Join(',',
                    i,
                    Hex(prefab.Uid),
                    Escape(prefab.Model.ToString()),
                    entry,
                    exit,
                    result.PpdEntryIndex,
                    result.PpdExitIndex,
                    branch.LaneOrdinal,
                    branch.StartCurveId,
                    branch.BoundaryLaneId,
                    branch.TargetPpdNodeIndex,
                    branch.Selected ? 1 : 0,
                    Escape(string.Join('|', branch.CurveIds)),
                    F(branch.LengthM),
                    F(branch.AngleDeg),
                    Escape(geometry),
                    Escape(side),
                    Escape(string.Join('|', branch.Blinkers)),
                    Escape(string.Join('|', branch.TrafficRules)),
                    Escape(string.Join('|', branch.AllowedVehicles)),
                    branch.LowProbability ? 1 : 0,
                    branch.LimitDisplacement ? 1 : 0,
                    branch.CycleEncountered ? 1 : 0));
            }
        }

        return new ManeuverFeatureSummary(prefabSegments, resolved, selectedPaths, transparent, turns, branches, merges, forks, cycleCandidates, other);
    }

    private SignSemanticSummary WriteSignSemantics(ProbeRoute route, string signContextPath, string navigationEventsPath, string routeIdentityPath)
    {
        var events = 0;
        var withContext = 0;
        var exitCandidates = 0;
        var keepCandidates = 0;
        var withApproachIdentity = 0;
        var withAfterIdentity = 0;

        using var signWriter = Csv(signContextPath);
        using var eventWriter = Csv(navigationEventsPath);
        using var identityWriter = Csv(routeIdentityPath);

        signWriter.WriteLine("segment_index,prefab_uid,prefab_model,preliminary_class,sign_rank,sign_uid,sign_node_uid,region,distance_to_decision_m,along_m,lateral_m,orientation_perpendicular_error_deg,score,model,template,definition_name,model_desc,raw_strings,exit_numbers,road_identities,destinations,legacy_cities");
        eventWriter.WriteLine("event_index,segment_index,prefab_uid,prefab_model,preliminary_class,instruction_candidate,emit_instruction,selected_side,input_lane_count,selected_lane_count,selected_lane_ordinals,selected_touches_divider_side,selected_touches_outer_side,representative_angle_deg,exit_evidence,exit_numbers,approach_road_identities,after_road_identities,all_road_identity_candidates,approach_destinations,after_destinations,all_destination_candidates,sign_candidate_count,best_sign_uid,best_sign_region,best_sign_score,confidence,notes");
        identityWriter.WriteLine("event_index,segment_index,prefab_uid,prefab_model,preliminary_class,instruction_candidate,best_approach_identity,best_after_identity,identity_relation,approach_identity_sign_uid,after_identity_sign_uid,approach_identity_score,after_identity_score");

        var eventIndex = 0;
        for (var i = 0; i + 1 < route.Physical.Count; i++)
        {
            if (!map.Nodes.TryGetValue(route.Physical[i].NodeUid, out var a) ||
                !map.Nodes.TryGetValue(route.Physical[i + 1].NodeUid, out var b))
                continue;
            if (ResolveCommonMapItem(a, b) is not Prefab prefab)
                continue;

            var entry = FindNodeIndex(prefab, route.Physical[i].NodeUid);
            var exit = FindNodeIndex(prefab, route.Physical[i + 1].NodeUid);
            var feature = maneuverFeatureExtractor.Analyze(prefab, entry, exit);
            if (!feature.PpdResolved || feature.SelectedBranchCount == 0) continue;

            var prev = i > 0 && map.Nodes.TryGetValue(route.Physical[i - 1].NodeUid, out var prevNode)
                ? prevNode.Position
                : (System.Numerics.Vector3?)null;
            var next = i + 2 < route.Physical.Count && map.Nodes.TryGetValue(route.Physical[i + 2].NodeUid, out var nextNode)
                ? nextNode.Position
                : (System.Numerics.Vector3?)null;
            var candidates = signResolver.FindForDecision(a.Position, b.Position, prev, next)
                .Take(30)
                .ToList();

            if (candidates.Count > 0) withContext++;

            for (var rank = 0; rank < candidates.Count; rank++)
            {
                var c = candidates[rank];
                signWriter.WriteLine(string.Join(',',
                    i,
                    Hex(prefab.Uid),
                    Escape(prefab.Model.ToString()),
                    Escape(feature.PreliminaryClass),
                    rank + 1,
                    Hex(c.Sign.Uid),
                    Hex(c.Sign.NodeUid),
                    Escape(c.Region),
                    F(c.DistanceToDecisionM),
                    F(c.AlongM),
                    F(c.LateralM),
                    F(c.OrientationPerpendicularErrorDeg),
                    F(c.Score),
                    Escape(c.Sign.Model),
                    Escape(c.Sign.Template),
                    Escape(c.Sign.DefinitionName),
                    Escape(c.Sign.ModelDesc),
                    Escape(string.Join('|', c.Sign.RawStrings)),
                    Escape(string.Join('|', c.Sign.ExitNumbers)),
                    Escape(string.Join('|', c.Sign.RoadIdentities.Select(r => r.Display))),
                    Escape(string.Join('|', c.Sign.Destinations)),
                    Escape(string.Join('|', c.Sign.LegacyCities))));
            }

            var exitEvidenceSigns = candidates
                .Where(c => c.Sign.ExitNumbers.Count > 0)
                .Where(c => c.Region is "NEAR" or "AFTER" || c.DistanceToDecisionM <= 260)
                .OrderByDescending(c => c.Score)
                .ToList();
            var exitNumbers = exitEvidenceSigns.SelectMany(c => c.Sign.ExitNumbers).Distinct(StringComparer.OrdinalIgnoreCase).ToList();

            var approachIdentityCandidates = candidates
                .Where(c => c.Region is "APPROACH" or "NEAR")
                .Where(c => c.Sign.RoadIdentities.Count > 0)
                .OrderByDescending(c => c.Score)
                .ToList();
            var afterIdentityCandidates = candidates
                .Where(c => c.Region is "AFTER" or "NEAR")
                .Where(c => c.Sign.RoadIdentities.Count > 0)
                .OrderByDescending(c => c.Score)
                .ToList();

            var approachIds = approachIdentityCandidates.SelectMany(c => c.Sign.RoadIdentities).Select(r => r.Display).Where(x => x.Length > 0).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            var afterIds = afterIdentityCandidates.SelectMany(c => c.Sign.RoadIdentities).Select(r => r.Display).Where(x => x.Length > 0).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            var allIds = candidates.SelectMany(c => c.Sign.RoadIdentities).Select(r => r.Display).Where(x => x.Length > 0).Distinct(StringComparer.OrdinalIgnoreCase).ToList();

            var approachDestinations = candidates.Where(c => c.Region is "APPROACH" or "NEAR")
                .SelectMany(c => c.Sign.Destinations.Concat(c.Sign.LegacyCities)).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            var afterDestinations = candidates.Where(c => c.Region is "AFTER" or "NEAR")
                .SelectMany(c => c.Sign.Destinations.Concat(c.Sign.LegacyCities)).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            var allDestinations = candidates.SelectMany(c => c.Sign.Destinations.Concat(c.Sign.LegacyCities)).Distinct(StringComparer.OrdinalIgnoreCase).ToList();

            if (approachIds.Count > 0) withApproachIdentity++;
            if (afterIds.Count > 0) withAfterIdentity++;

            var instruction = SemanticInstruction(feature, exitNumbers.Count > 0);
            var emit = feature.PreliminaryClass != "TRANSPARENT";
            if (instruction.StartsWith("EXIT_", StringComparison.Ordinal)) exitCandidates++;
            if (instruction.StartsWith("KEEP_", StringComparison.Ordinal)) keepCandidates++;

            var best = candidates.FirstOrDefault();
            var bestApproach = approachIdentityCandidates.FirstOrDefault();
            var bestAfter = afterIdentityCandidates.FirstOrDefault();
            var bestApproachIdentity = bestApproach?.Sign.RoadIdentities.FirstOrDefault()?.Display ?? "";
            var bestAfterIdentity = bestAfter?.Sign.RoadIdentities.FirstOrDefault()?.Display ?? "";
            var identityRelation = IdentityRelation(bestApproachIdentity, bestAfterIdentity);

            var notes = new List<string>();
            if (feature.PreliminaryClass.StartsWith("BRANCH_", StringComparison.Ordinal) && exitNumbers.Count == 0)
                notes.Add("branch_without_exit_number=>KEEP_candidate");
            if (feature.PreliminaryClass == "FORK_STRAIGHT")
                notes.Add("lane_subset_used_for_keep_side");
            if (afterIds.Count > 1) notes.Add("multiple_after_route_identities_ambiguous");
            if (allDestinations.Count > 0 && afterDestinations.Count == 0) notes.Add("destinations_only_on_approach_not_branch_assigned");

            eventWriter.WriteLine(string.Join(',',
                eventIndex,
                i,
                Hex(prefab.Uid),
                Escape(prefab.Model.ToString()),
                Escape(feature.PreliminaryClass),
                Escape(instruction),
                emit ? 1 : 0,
                Escape(feature.SelectedSide),
                feature.InputLaneCount,
                feature.SelectedLaneCount,
                Escape(string.Join('|', feature.SelectedLaneOrdinals)),
                feature.SelectedTouchesDividerSide ? 1 : 0,
                feature.SelectedTouchesOuterSide ? 1 : 0,
                feature.RepresentativeAngleDeg.HasValue ? F(feature.RepresentativeAngleDeg.Value) : "",
                exitNumbers.Count > 0 ? 1 : 0,
                Escape(string.Join('|', exitNumbers)),
                Escape(string.Join('|', approachIds)),
                Escape(string.Join('|', afterIds)),
                Escape(string.Join('|', allIds)),
                Escape(string.Join('|', approachDestinations)),
                Escape(string.Join('|', afterDestinations)),
                Escape(string.Join('|', allDestinations)),
                candidates.Count,
                best is null ? "" : Hex(best.Sign.Uid),
                Escape(best?.Region ?? ""),
                best is null ? "" : F(best.Score),
                Escape(SemanticConfidence(feature, instruction, exitNumbers.Count > 0, afterIds.Count > 0)),
                Escape(string.Join('|', notes))));

            identityWriter.WriteLine(string.Join(',',
                eventIndex,
                i,
                Hex(prefab.Uid),
                Escape(prefab.Model.ToString()),
                Escape(feature.PreliminaryClass),
                Escape(instruction),
                Escape(bestApproachIdentity),
                Escape(bestAfterIdentity),
                Escape(identityRelation),
                bestApproach is null ? "" : Hex(bestApproach.Sign.Uid),
                bestAfter is null ? "" : Hex(bestAfter.Sign.Uid),
                bestApproach is null ? "" : F(bestApproach.Score),
                bestAfter is null ? "" : F(bestAfter.Score)));

            events++;
            eventIndex++;
        }

        return new SignSemanticSummary(events, withContext, exitCandidates, keepCandidates, withApproachIdentity, withAfterIdentity);
    }

    private CorridorSemanticResult WriteCorridorSemanticsV2(
        ProbeRoute route,
        string corridorPath,
        string corridorSegmentsPath,
        string overlayMatchesPath,
        string routeIdentityV2Path,
        string signContextV2Path,
        string navigationEventsV2Path)
    {
        RouteCorridor corridor; using(PerformanceTrace.Measure("corridor")) corridor = corridorBuilder.Build(route);
        corridor.WriteCsv(corridorPath);
        corridor.WriteSegmentsCsv(corridorSegmentsPath);

        var overlayMatches = roadOverlayResolver.Match(corridor);
        RoadOverlayResolver.WriteMatches(overlayMatchesPath, overlayMatches);
        var highOverlayMatches = overlayMatches.Count(m => m.Confidence == "HIGH");

        var featureBySegment = new Dictionary<int, ManeuverFeatureResult>();
        var centerBySegment = new Dictionary<int, System.Numerics.Vector3>();
        for (var i = 0; i + 1 < route.Physical.Count; i++)
        {
            if (!map.Nodes.TryGetValue(route.Physical[i].NodeUid, out var a) ||
                !map.Nodes.TryGetValue(route.Physical[i + 1].NodeUid, out var b))
                continue;
            if (ResolveCommonMapItem(a, b) is not Prefab prefab) continue;
            var entry = FindNodeIndex(prefab, a.Uid);
            var exit = FindNodeIndex(prefab, b.Uid);
            var feature = maneuverFeatureExtractor.Analyze(prefab, entry, exit);
            if (!feature.PpdResolved || feature.SelectedBranchCount == 0) continue;
            featureBySegment[i] = feature;
            centerBySegment[i] = (a.Position + b.Position) * 0.5f;
        }

        var zoneBySegment = new Dictionary<int, int>();
        var zone = 0;
        foreach (var segment in corridor.Segments.OrderBy(s => s.SegmentIndex))
        {
            zoneBySegment[segment.SegmentIndex] = zone;
            if (featureBySegment.TryGetValue(segment.SegmentIndex, out var f) && IsIdentityBarrier(f.PreliminaryClass))
                zone++;
        }

        var identityRows = BuildIdentityRows(corridor, overlayMatches, zoneBySegment);
        WriteIdentityRows(routeIdentityV2Path, identityRows);
        var identityResolved = identityRows.Count(r => !string.IsNullOrWhiteSpace(r.Identity));

        using var signWriter = Csv(signContextV2Path);
        using var eventWriter = Csv(navigationEventsV2Path);
        signWriter.WriteLine("event_index,segment_index,prefab_uid,prefab_model,preliminary_class,decision_station_m,decision_nav_station_m,sign_rank,sign_uid,region,relative_station_m,lateral_m,orientation_perpendicular_error_deg,score,is_strict_exit_candidate,is_exit_navigation_model,is_exit_speed_model,has_explicit_exit_text,exit_numbers,road_identities,cardinals,destinations,model,template,definition_name,model_desc,raw_strings");
        eventWriter.WriteLine("event_index,segment_index,prefab_uid,prefab_model,preliminary_class,instruction_candidate_v2,emit_instruction,selected_side,decision_station_m,decision_nav_station_m,before_road_identity,after_road_identity,identity_relation,approach_cardinals,after_cardinals,exit_candidate_numbers,selected_exit_number,exit_ambiguous,strict_exit_sign_count,sign_candidate_count,approach_destinations,after_destinations,confidence,notes");

        var v2Events = 0;
        var v2Exit = 0;
        var v2Keep = 0;
        var ambiguousExits = 0;
        var eventIndex = 0;

        foreach (var segment in corridor.Segments.OrderBy(s => s.SegmentIndex))
        {
            if (!featureBySegment.TryGetValue(segment.SegmentIndex, out var feature)) continue;
            if (!map.MapItems.TryGetValue(segment.ItemUid, out var item) || item is not Prefab prefab) continue;

            var decisionStation = (segment.StartStationM + segment.EndStationM) * 0.5;
            var decisionNavStation = (segment.StartNavStationM + segment.EndNavStationM) * 0.5;
            var decisionCenter = centerBySegment.TryGetValue(segment.SegmentIndex, out var c)
                ? c
                : MidPointForSegment(corridor, segment);
            var candidates = signResolver.FindAlongCorridor(corridor, decisionStation, decisionCenter).Take(30).ToList();

            for (var rank = 0; rank < candidates.Count; rank++)
            {
                var candidate = candidates[rank];
                signWriter.WriteLine(string.Join(',',
                    eventIndex,
                    segment.SegmentIndex,
                    Hex(prefab.Uid),
                    Escape(prefab.Model.ToString()),
                    Escape(feature.PreliminaryClass),
                    F(decisionStation),
                    F(decisionNavStation),
                    rank + 1,
                    Hex(candidate.Sign.Uid),
                    Escape(candidate.Region),
                    F(candidate.AlongM),
                    F(candidate.LateralM),
                    F(candidate.OrientationPerpendicularErrorDeg),
                    F(candidate.Score),
                    SignRoadIdentityResolver.IsStrictExitCandidate(candidate) ? 1 : 0,
                    candidate.Sign.IsExitNavigationModel ? 1 : 0,
                    candidate.Sign.IsExitSpeedModel ? 1 : 0,
                    candidate.Sign.HasExplicitExitText ? 1 : 0,
                    Escape(string.Join('|', candidate.Sign.ExitNumbers)),
                    Escape(string.Join('|', candidate.Sign.RoadIdentities.Select(r => r.Display))),
                    Escape(string.Join('|', candidate.Sign.Cardinals)),
                    Escape(string.Join('|', candidate.Sign.Destinations.Concat(candidate.Sign.LegacyCities).Distinct(StringComparer.OrdinalIgnoreCase))),
                    Escape(candidate.Sign.Model),
                    Escape(candidate.Sign.Template),
                    Escape(candidate.Sign.DefinitionName),
                    Escape(candidate.Sign.ModelDesc),
                    Escape(string.Join('|', candidate.Sign.RawStrings))));
            }

            var strictExitCandidates = candidates
                .Where(SignRoadIdentityResolver.IsStrictExitCandidate)
                .OrderByDescending(x => x.Score)
                .ToList();
            var exitScores = new Dictionary<string, double>(StringComparer.OrdinalIgnoreCase);
            foreach (var candidate in strictExitCandidates)
            {
                var bonus = candidate.Sign.HasExplicitExitText ? 20.0 : 0.0;
                foreach (var number in candidate.Sign.ExitNumbers)
                {
                    var score = candidate.Score + bonus;
                    if (!exitScores.TryGetValue(number, out var existing) || score > existing)
                        exitScores[number] = score;
                }
            }
            var orderedExitScores = exitScores.OrderByDescending(kv => kv.Value).ToList();
            var selectedExitNumber = "";
            var exitAmbiguous = false;
            if (orderedExitScores.Count == 1)
            {
                selectedExitNumber = orderedExitScores[0].Key;
            }
            else if (orderedExitScores.Count > 1)
            {
                var margin = orderedExitScores[0].Value - orderedExitScores[1].Value;
                if (margin >= 18.0) selectedExitNumber = orderedExitScores[0].Key;
                else exitAmbiguous = true;
            }

            var beforeZone = zoneBySegment.TryGetValue(segment.SegmentIndex, out var bz) ? bz : -1;
            var nextSegment = corridor.Segments.FirstOrDefault(s => s.SegmentIndex > segment.SegmentIndex);
            var afterZone = nextSegment is not null && zoneBySegment.TryGetValue(nextSegment.SegmentIndex, out var az) ? az : beforeZone;
            var beforeIdentity = NearestIdentity(identityRows, segment.SegmentIndex, beforeZone, -1);
            var afterIdentity = NearestIdentity(identityRows, segment.SegmentIndex, afterZone, +1);

            var approachCardinals = candidates.Where(x => x.Region is "APPROACH" or "NEAR")
                .SelectMany(x => x.Sign.Cardinals).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            var afterCardinals = candidates.Where(x => x.Region is "AFTER" or "NEAR")
                .SelectMany(x => x.Sign.Cardinals).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            var approachDestinations = candidates.Where(x => x.Region is "APPROACH" or "NEAR")
                .SelectMany(x => x.Sign.Destinations.Concat(x.Sign.LegacyCities)).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            var afterDestinations = candidates.Where(x => x.Region is "AFTER" or "NEAR")
                .SelectMany(x => x.Sign.Destinations.Concat(x.Sign.LegacyCities)).Distinct(StringComparer.OrdinalIgnoreCase).ToList();

            var hasStrongExit = !string.IsNullOrWhiteSpace(selectedExitNumber) && feature.PreliminaryClass.StartsWith("BRANCH_", StringComparison.Ordinal);
            var instruction = SemanticInstruction(feature, hasStrongExit);
            if (instruction.StartsWith("EXIT_", StringComparison.Ordinal)) v2Exit++;
            if (instruction.StartsWith("KEEP_", StringComparison.Ordinal)) v2Keep++;
            if (exitAmbiguous) ambiguousExits++;

            var notes = new List<string>();
            if (exitAmbiguous) notes.Add("multiple_strict_exit_numbers_without_score_margin");
            if (strictExitCandidates.Count > 0 && string.IsNullOrWhiteSpace(selectedExitNumber) && !exitAmbiguous)
                notes.Add("strict_exit_sign_without_unique_number");
            if (feature.PreliminaryClass.StartsWith("BRANCH_", StringComparison.Ordinal) && !hasStrongExit)
                notes.Add("branch_without_strong_exit=>KEEP");
            if (beforeIdentity.Identity.Length > 0 && afterIdentity.Identity.Length > 0 && !string.Equals(beforeIdentity.Identity, afterIdentity.Identity, StringComparison.OrdinalIgnoreCase))
                notes.Add("road_identity_changes_across_decision");

            eventWriter.WriteLine(string.Join(',',
                eventIndex,
                segment.SegmentIndex,
                Hex(prefab.Uid),
                Escape(prefab.Model.ToString()),
                Escape(feature.PreliminaryClass),
                Escape(instruction),
                feature.PreliminaryClass == "TRANSPARENT" ? 0 : 1,
                Escape(feature.SelectedSide),
                F(decisionStation),
                F(decisionNavStation),
                Escape(beforeIdentity.Identity),
                Escape(afterIdentity.Identity),
                Escape(IdentityRelation(beforeIdentity.Identity, afterIdentity.Identity)),
                Escape(string.Join('|', approachCardinals)),
                Escape(string.Join('|', afterCardinals)),
                Escape(string.Join('|', orderedExitScores.Select(x => x.Key))),
                Escape(selectedExitNumber),
                exitAmbiguous ? 1 : 0,
                strictExitCandidates.Count,
                candidates.Count,
                Escape(string.Join('|', approachDestinations)),
                Escape(string.Join('|', afterDestinations)),
                Escape(SemanticConfidence(feature, instruction, hasStrongExit, afterIdentity.Identity.Length > 0)),
                Escape(string.Join('|', notes))));

            v2Events++;
            eventIndex++;
        }

        var summary = new CorridorSemanticSummary(
            corridor.Edges.Count,
            overlayMatches.Count,
            highOverlayMatches,
            identityResolved,
            v2Events,
            v2Exit,
            v2Keep,
            ambiguousExits);
        return new CorridorSemanticResult(summary, corridor, overlayMatches);
    }

    private static bool IsIdentityBarrier(string preliminaryClass)
        => preliminaryClass != "TRANSPARENT" && preliminaryClass != "UNRESOLVED";

    private static List<SegmentIdentityV2> BuildIdentityRows(
        RouteCorridor corridor,
        IReadOnlyList<RoadOverlayMatch> matches,
        IReadOnlyDictionary<int, int> zoneBySegment)
    {
        var usable = matches.Where(m => m.Confidence is "HIGH" or "MEDIUM").ToList();
        var result = new List<SegmentIdentityV2>();
        foreach (var segment in corridor.Segments.OrderBy(s => s.SegmentIndex))
        {
            var zone = zoneBySegment.TryGetValue(segment.SegmentIndex, out var z) ? z : -1;
            var sameZone = usable.Where(m => zoneBySegment.TryGetValue(m.Projection.SegmentIndex, out var mz) && mz == zone).ToList();
            var midpoint = (segment.StartStationM + segment.EndStationM) * 0.5;
            var direct = sameZone
                .Where(m => m.Projection.SegmentIndex == segment.SegmentIndex ||
                            (m.Projection.StationM >= segment.StartStationM - 120 && m.Projection.StationM <= segment.EndStationM + 120))
                .OrderBy(m => m.Projection.LateralM)
                .ThenBy(m => Math.Abs(m.Projection.StationM - midpoint))
                .ToList();

            var directIdentities = direct.Select(m => m.Overlay.Identity).Where(x => x.Length > 0).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            string identity = "";
            string source = "";
            string confidence = "NONE";
            RoadOverlayMatch? prev = null;
            RoadOverlayMatch? next = null;

            if (directIdentities.Count == 1)
            {
                identity = directIdentities[0];
                source = "DIRECT_OVERLAY";
                confidence = direct.Any(m => m.Confidence == "HIGH") ? "HIGH" : "MEDIUM";
            }
            else if (directIdentities.Count == 0)
            {
                prev = sameZone.Where(m => m.Projection.StationM <= midpoint)
                    .OrderByDescending(m => m.Projection.StationM).FirstOrDefault();
                next = sameZone.Where(m => m.Projection.StationM >= midpoint)
                    .OrderBy(m => m.Projection.StationM).FirstOrDefault();
                if (prev is not null && next is not null &&
                    string.Equals(prev.Overlay.Identity, next.Overlay.Identity, StringComparison.OrdinalIgnoreCase))
                {
                    identity = prev.Overlay.Identity;
                    source = "BRACKETED_OVERLAYS";
                    confidence = "HIGH";
                }
                else
                {
                    var nearest = new[] { prev, next }
                        .Where(m => m is not null)
                        .Cast<RoadOverlayMatch>()
                        .OrderBy(m => Math.Abs(m.Projection.StationM - midpoint))
                        .FirstOrDefault();
                    if (nearest is not null && Math.Abs(nearest.Projection.StationM - midpoint) <= 5000)
                    {
                        identity = nearest.Overlay.Identity;
                        source = "SINGLE_NEAR_OVERLAY";
                        confidence = "MEDIUM";
                    }
                }
            }
            else
            {
                source = "CONFLICTING_DIRECT_OVERLAYS";
                confidence = "AMBIGUOUS";
            }

            prev ??= sameZone.Where(m => m.Projection.StationM <= midpoint)
                .OrderByDescending(m => m.Projection.StationM).FirstOrDefault();
            next ??= sameZone.Where(m => m.Projection.StationM >= midpoint)
                .OrderBy(m => m.Projection.StationM).FirstOrDefault();

            result.Add(new SegmentIdentityV2(
                segment.SegmentIndex,
                zone,
                identity,
                source,
                confidence,
                string.Join('|', directIdentities),
                prev?.Overlay.Identity ?? "",
                prev?.Overlay.Uid,
                prev is null ? null : midpoint - prev.Projection.StationM,
                next?.Overlay.Identity ?? "",
                next?.Overlay.Uid,
                next is null ? null : next.Projection.StationM - midpoint));
        }
        return result;
    }

    private static void WriteIdentityRows(string path, IReadOnlyList<SegmentIdentityV2> rows)
    {
        using var writer = Csv(path);
        writer.WriteLine("segment_index,identity_zone,identity,source,confidence,direct_overlay_identities,previous_overlay_identity,previous_overlay_uid,previous_overlay_distance_m,next_overlay_identity,next_overlay_uid,next_overlay_distance_m");
        foreach (var row in rows)
        {
            writer.WriteLine(string.Join(',',
                row.SegmentIndex,
                row.Zone,
                Escape(row.Identity),
                Escape(row.Source),
                Escape(row.Confidence),
                Escape(row.DirectOverlayIdentities),
                Escape(row.PreviousOverlayIdentity),
                row.PreviousOverlayUid.HasValue ? Hex(row.PreviousOverlayUid.Value) : "",
                row.PreviousOverlayDistanceM.HasValue ? F(row.PreviousOverlayDistanceM.Value) : "",
                Escape(row.NextOverlayIdentity),
                row.NextOverlayUid.HasValue ? Hex(row.NextOverlayUid.Value) : "",
                row.NextOverlayDistanceM.HasValue ? F(row.NextOverlayDistanceM.Value) : ""));
        }
    }

    private static SegmentIdentityV2 NearestIdentity(IReadOnlyList<SegmentIdentityV2> rows, int eventSegment, int zone, int direction)
    {
        var candidates = rows
            .Where(r => r.Zone == zone && !string.IsNullOrWhiteSpace(r.Identity) && r.Confidence != "AMBIGUOUS")
            .Where(r => direction < 0 ? r.SegmentIndex <= eventSegment : r.SegmentIndex > eventSegment)
            .OrderBy(r => Math.Abs(r.SegmentIndex - eventSegment))
            .ToList();
        if (candidates.Count > 0) return candidates[0];
        return new SegmentIdentityV2(-1, zone, "", "", "NONE", "", "", null, null, "", null, null);
    }

    private static System.Numerics.Vector3 MidPointForSegment(RouteCorridor corridor, RouteCorridorSegment segment)
    {
        var edges = corridor.Edges.Where(e => e.SegmentIndex == segment.SegmentIndex).ToList();
        if (edges.Count == 0) return System.Numerics.Vector3.Zero;
        var first = edges[0].Start;
        var last = edges[^1].End;
        return (first + last) * 0.5f;
    }


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
            // ATS is US-only here: lane ordinal 0 is closest to the divider, i.e. leftmost.
            if (feature.SelectedTouchesDividerSide && !feature.SelectedTouchesOuterSide) return "KEEP_LEFT";
            if (feature.SelectedTouchesOuterSide && !feature.SelectedTouchesDividerSide) return "KEEP_RIGHT";
            return "CONTINUE_AT_FORK";
        }

        return c;
    }

    private static string SemanticConfidence(ManeuverFeatureResult feature, string instruction, bool exitEvidence, bool afterIdentity)
    {
        if (instruction.StartsWith("EXIT_", StringComparison.Ordinal)) return exitEvidence ? "HIGH" : "LOW";
        if (instruction.StartsWith("KEEP_", StringComparison.Ordinal))
            return feature.SelectedLaneCount < feature.InputLaneCount || feature.PreliminaryClass.StartsWith("BRANCH_", StringComparison.Ordinal) ? "MEDIUM" : "LOW";
        if (instruction.StartsWith("TURN_", StringComparison.Ordinal) || instruction.StartsWith("SHARP_", StringComparison.Ordinal) || instruction.StartsWith("U_TURN_", StringComparison.Ordinal))
            return afterIdentity ? "HIGH" : feature.Confidence;
        return feature.Confidence;
    }

    private static string IdentityRelation(string before, string after)
    {
        if (string.IsNullOrWhiteSpace(before) || string.IsNullOrWhiteSpace(after)) return "UNKNOWN";
        return string.Equals(before, after, StringComparison.OrdinalIgnoreCase) ? "SAME" : "CHANGES";
    }

    private sealed record ContextDescription(string ItemType, string Uid, string RoadType, string GpsAvoid, string ShowUi, string AiVehicles);

    private static ContextDescription DescribeContext(MapItem? item)
    {
        return item switch
        {
            Road r => new ContextDescription("Road", Hex(r.Uid), r.RoadType.ToString(), r.GpsAvoid ? "1" : "0", r.ShowInUiMap ? "1" : "0", r.AiVehicles ? "1" : "0"),
            Prefab p => new ContextDescription("Prefab", Hex(p.Uid), p.Model.ToString(), "", p.ShowInUiMap ? "1" : "0", p.AiVehicles ? "1" : "0"),
            null => new ContextDescription("", "", "", "", "", ""),
            _ => new ContextDescription(item.ItemType.ToString(), Hex(item.Uid), "", "", "", "")
        };
    }

    private static string BoolOrEmpty(bool? value) => value.HasValue ? (value.Value ? "1" : "0") : "";

    private static string GeometryForReport(double angleDeg)
    {
        var abs = Math.Abs(angleDeg);
        if (abs <= 2) return "THROUGH";
        if (abs <= 70) return "SLIGHT";
        if (abs <= 110) return "TURN";
        if (abs <= 160) return "SHARP";
        return "U_TURN";
    }

    private static string SideForReport(double angleDeg)
    {
        if (Math.Abs(angleDeg) <= 2) return "STRAIGHT";
        return angleDeg > 0 ? "RIGHT" : "LEFT";
    }

    private static Dictionary<int, int> BuildNativeSegmentIndex(ProbeRoute route)
    {
        var result = new Dictionary<int, int>();
        var physicalIndex = -1;
        foreach (var row in route.Logical)
        {
            if (row.Type == 0)
            {
                physicalIndex++;
                continue;
            }
            if (row.Type == 1 && physicalIndex >= 0 && physicalIndex < route.Physical.Count - 1)
                result[row.Index] = physicalIndex;
        }
        return result;
    }

    private static Dictionary<int, List<LogicalRouteRow>> BuildNativeBetween(ProbeRoute route)
    {
        // Physical node i -> physical node i+1 corresponds to all native type=1 logical
        // items appearing between their type=0 entries.
        var result = new Dictionary<int, List<LogicalRouteRow>>();
        var physicalIndex = -1;
        foreach (var row in route.Logical)
        {
            if (row.Type == 0)
            {
                physicalIndex++;
                continue;
            }
            if (physicalIndex >= 0 && physicalIndex < route.Physical.Count - 1)
            {
                if (!result.TryGetValue(physicalIndex, out var list))
                    result[physicalIndex] = list = [];
                list.Add(row);
            }
        }
        return result;
    }

    private MapItem? ResolveCommonMapItem(INode a, INode b)
    {
        Span<ulong> aIds = stackalloc ulong[2] { a.ForwardItem?.Uid ?? 0, a.BackwardItem?.Uid ?? 0 };
        Span<ulong> bIds = stackalloc ulong[2] { b.ForwardItem?.Uid ?? 0, b.BackwardItem?.Uid ?? 0 };
        foreach (var aId in aIds)
        {
            if (aId == 0) continue;
            foreach (var bId in bIds)
            {
                if (aId == bId && map.MapItems.TryGetValue(aId, out var item))
                    return item;
            }
        }
        return null;
    }

    private static int FindNodeIndex(Prefab prefab, ulong uid)
    {
        for (var i = 0; i < prefab.Nodes.Count; i++)
            if (prefab.Nodes[i].Uid == uid) return i;
        return -1;
    }

    private static StreamWriter Csv(string path) => new(path, false, new UTF8Encoding(false));
    private static string Hex(ulong uid) => uid.ToString("X16", CultureInfo.InvariantCulture);
    private static string HexOrEmpty(ulong? uid) => uid.HasValue ? Hex(uid.Value) : "";
    private static string F(double value) => value.ToString("0.######", CultureInfo.InvariantCulture);
    private static string ItemTypeName(IMapObject? value) => value switch
    {
        MapItem item => item.ItemType.ToString(),
        Node => "Node",
        null => "",
        _ => value.GetType().Name
    };
    private static string Escape(string? value)
    {
        value ??= "";
        if (!value.Contains(',') && !value.Contains('"') && !value.Contains('\n') && !value.Contains('\r')) return value;
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}

internal sealed record CorridorSemanticResult(
    CorridorSemanticSummary Summary,
    RouteCorridor Corridor,
    IReadOnlyList<RoadOverlayMatch> OverlayMatches);

internal sealed record CorridorSemanticSummary(
    int CorridorEdges,
    int RoadOverlayMatches,
    int HighRoadOverlayMatches,
    int IdentityResolvedSegments,
    int V2Events,
    int V2ExitCandidates,
    int V2KeepCandidates,
    int AmbiguousExitEvents);

internal sealed record SegmentIdentityV2(
    int SegmentIndex,
    int Zone,
    string Identity,
    string Source,
    string Confidence,
    string DirectOverlayIdentities,
    string PreviousOverlayIdentity,
    ulong? PreviousOverlayUid,
    double? PreviousOverlayDistanceM,
    string NextOverlayIdentity,
    ulong? NextOverlayUid,
    double? NextOverlayDistanceM);

internal sealed record SignSemanticSummary(
    int Events,
    int EventsWithSignContext,
    int ExitCandidates,
    int KeepCandidates,
    int EventsWithApproachRoadIdentity,
    int EventsWithAfterRoadIdentity);

internal sealed record ManeuverFeatureSummary(
    int PrefabSegments,
    int Resolved,
    int SelectedPaths,
    int Transparent,
    int Turns,
    int Branches,
    int Merges,
    int Forks,
    int CycleCandidates,
    int Other);

internal sealed record NativeNavGraphSummary(
    int Events,
    int ExactPaths,
    int StructuralValidated,
    int DistanceMatches2m,
    int StrongValidated);

internal sealed record BridgeSummary(
    string Route,
    int PhysicalNodes,
    int NodesFound,
    double NodeCoverage,
    int LogicalItems,
    int LogicalPrefabs,
    int LogicalPrefabsFound,
    int LogicalPrefabsCorrectType,
    int SegmentCount,
    int SegmentsResolved,
    int RoadSegments,
    int PrefabSegments,
    int OtherSegments,
    int NativeHintCompared,
    int NativeHintMatches,
    int PpdPrefabs,
    int PpdResolved,
    int PpdPathFound,
    int PpdDecisionCandidates,
    int NativeNavGraphEvents,
    int NativeNavGraphExactPaths,
    int NativeNodeIdStructuralValidated,
    int NativeNavGraphDistanceMatches2m,
    int NativeNodeIdStrongValidated,
    int ManeuverFeaturePrefabSegments,
    int ManeuverFeatureResolved,
    int ManeuverFeatureSelectedPaths,
    int ManeuverFeatureTransparent,
    int ManeuverFeatureTurns,
    int ManeuverFeatureBranches,
    int ManeuverFeatureMerges,
    int ManeuverFeatureForks,
    int ManeuverFeatureCycleCandidates,
    int ManeuverFeatureOther,
    int SignSemanticEvents,
    int SignSemanticEventsWithContext,
    int SignSemanticExitCandidates,
    int SignSemanticKeepCandidates,
    int SignSemanticEventsWithApproachRoadIdentity,
    int SignSemanticEventsWithAfterRoadIdentity,
    int CorridorEdges,
    int RoadOverlayMatches,
    int HighRoadOverlayMatches,
    int RouteIdentityResolvedSegments,
    int V2NavigationEvents,
    int V2ExitCandidates,
    int V2KeepCandidates,
    int V2AmbiguousExitEvents,
    int V09Zones,
    int V09IdentifiedZones,
    int V09UnknownZones,
    int V09SemanticBlocks,
    int V09NamedBlocks,
    int V09CardinalResolvedBlocks,
    int V09CruiseLegs,
    int V09NamedCruiseLegs,
    int V09PlanEntries,
    int V09ConcurrentEvidenceZones,
    double V09NamedCoverageRatio,
    bool PhysicalMatchesLogicalNodeSequence)
{
    public bool PerfectNodeCoverage => NodesFound == PhysicalNodes;
    public bool PerfectSegments => SegmentsResolved == SegmentCount;
    public bool PerfectNativePrefabs => LogicalPrefabs == LogicalPrefabsFound && LogicalPrefabs == LogicalPrefabsCorrectType;
    public bool PerfectPpdResolution => PpdResolved == PpdPrefabs && PpdPathFound == PpdPrefabs;
    public bool PerfectNativeNavGraph => NativeNavGraphExactPaths == NativeNavGraphEvents; // node_id meaning remains experimental
    public bool PerfectManeuverFeatures => ManeuverFeatureResolved == ManeuverFeaturePrefabSegments && ManeuverFeatureSelectedPaths == ManeuverFeaturePrefabSegments;

    public string ToText() => $"""
route={Route}
physical_nodes={PhysicalNodes}
nodes_found={NodesFound}
node_coverage_percent={NodeCoverage * 100:0.000}
logical_items={LogicalItems}
logical_prefabs={LogicalPrefabs}
logical_prefabs_found={LogicalPrefabsFound}
logical_prefabs_correct_type={LogicalPrefabsCorrectType}
segments={SegmentCount}
segments_resolved={SegmentsResolved}
road_segments={RoadSegments}
prefab_segments={PrefabSegments}
other_segments={OtherSegments}
native_hint_compared={NativeHintCompared}
native_hint_matches={NativeHintMatches}
ppd_prefabs={PpdPrefabs}
ppd_resolved={PpdResolved}
ppd_paths_found={PpdPathFound}
ppd_decision_candidates={PpdDecisionCandidates}
native_navgraph_events={NativeNavGraphEvents}
native_navgraph_exact_paths={NativeNavGraphExactPaths}
native_node_id_structural_validated={NativeNodeIdStructuralValidated}
native_navgraph_distance_matches_2m={NativeNavGraphDistanceMatches2m}
native_node_id_strong_validated={NativeNodeIdStrongValidated}
perfect_native_navgraph={PerfectNativeNavGraph}
maneuver_feature_prefab_segments={ManeuverFeaturePrefabSegments}
maneuver_feature_resolved={ManeuverFeatureResolved}
maneuver_feature_selected_paths={ManeuverFeatureSelectedPaths}
maneuver_feature_transparent={ManeuverFeatureTransparent}
maneuver_feature_turns={ManeuverFeatureTurns}
maneuver_feature_branches={ManeuverFeatureBranches}
maneuver_feature_merges={ManeuverFeatureMerges}
maneuver_feature_forks={ManeuverFeatureForks}
maneuver_feature_cycle_candidates={ManeuverFeatureCycleCandidates}
maneuver_feature_other={ManeuverFeatureOther}
sign_semantic_events={SignSemanticEvents}
sign_semantic_events_with_context={SignSemanticEventsWithContext}
sign_semantic_exit_candidates={SignSemanticExitCandidates}
sign_semantic_keep_candidates={SignSemanticKeepCandidates}
sign_semantic_events_with_approach_road_identity={SignSemanticEventsWithApproachRoadIdentity}
sign_semantic_events_with_after_road_identity={SignSemanticEventsWithAfterRoadIdentity}
corridor_edges={CorridorEdges}
road_overlay_matches={RoadOverlayMatches}
high_road_overlay_matches={HighRoadOverlayMatches}
route_identity_v2_resolved_segments={RouteIdentityResolvedSegments}
v2_navigation_events={V2NavigationEvents}
v2_exit_candidates={V2ExitCandidates}
v2_keep_candidates={V2KeepCandidates}
v2_ambiguous_exit_events={V2AmbiguousExitEvents}
v09_zones={V09Zones}
v09_identified_zones={V09IdentifiedZones}
v09_unknown_zones={V09UnknownZones}
v09_semantic_blocks={V09SemanticBlocks}
v09_named_blocks={V09NamedBlocks}
v09_cardinal_resolved_blocks={V09CardinalResolvedBlocks}
v09_cruise_legs={V09CruiseLegs}
v09_named_cruise_legs={V09NamedCruiseLegs}
v09_plan_entries={V09PlanEntries}
v09_concurrent_evidence_zones={V09ConcurrentEvidenceZones}
v09_named_coverage_percent={V09NamedCoverageRatio * 100:0.000}
perfect_maneuver_features={PerfectManeuverFeatures}
perfect_ppd_resolution={PerfectPpdResolution}
physical_matches_logical_node_sequence={PhysicalMatchesLogicalNodeSequence}
perfect_node_coverage={PerfectNodeCoverage}
perfect_segments={PerfectSegments}
perfect_native_prefabs={PerfectNativePrefabs}
""";
}



