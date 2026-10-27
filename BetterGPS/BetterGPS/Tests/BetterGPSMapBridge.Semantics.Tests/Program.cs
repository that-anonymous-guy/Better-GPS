using BetterGPSNavigation;
if(args is ["--extract-asset-list",var assetArchive,var assetList,var assetOutput]){
    using var reader=TruckLib.HashFs.HashFsReader.Open(assetArchive);
    foreach(var path in System.IO.File.ReadLines(assetList).Where(p=>p.StartsWith('/'))){
        try {using var stream=reader.Open(path);var target=System.IO.Path.Combine(assetOutput,path.TrimStart('/'));System.IO.Directory.CreateDirectory(System.IO.Path.GetDirectoryName(target)!);using var output=System.IO.File.Create(target);stream.CopyTo(output);System.Console.WriteLine(path);}
        catch(System.IO.FileNotFoundException){ }
    }
    return;
}
if(args is ["--inspect-display-archive",var displayArchivePath,var displayArchiveOutput]){
    System.IO.Directory.CreateDirectory(displayArchiveOutput);using var displayArchive=TruckLib.HashFs.HashFsReader.Open(displayArchivePath);
    var displayPaths=displayArchive.GetFiles("/ui/dashboard/").Concat(displayArchive.GetFiles("/def/vehicle/car/ford.f150_23/interior/")).Distinct().ToArray();
    foreach(var displayPath in displayPaths.Where(p=>p.EndsWith(".sii")||p.EndsWith(".sui"))){using var displayStream=displayArchive.Open(displayPath);using var displayReader=new System.IO.StreamReader(displayStream);var displayText=displayReader.ReadToEnd();System.IO.File.WriteAllText(System.IO.Path.Combine(displayArchiveOutput,displayPath.Replace('/','_')),displayText);System.Console.WriteLine(displayPath);}
    return;
}
if(args is ["--optimization-benchmark",var optimizationOutput]){OptimizationTests.Run(optimizationOutput);return;}
if(args is ["--road-name-cache-audit",var nameNetwork,var nameRoute,var nameOutput]){RoadNameCacheAudit.Run(nameNetwork,nameRoute,nameOutput);return;}
if(args is ["--road-name-regression",var nameFixtures,var nameResults]){RoadNameCacheAudit.Batch(nameFixtures,nameResults);return;}
if(args is ["--road-name-regression",var allNameFixtures,var allNameResults,var oldNameResults]){RoadNameCacheAudit.Batch(allNameFixtures,allNameResults,oldNameResults);return;}
if(args is ["--road-name-unit-tests"]){RoadNameRegressionTests.Run();return;}
if(args is ["--preparation-scheduling-tests"]){RoutePreparationTests.Scheduling();return;}
if(args is ["--preparation-atlas-tests",var prepAtlasGame,var prepAtlasDocs,var prepAtlasOutput]){RoutePreparationTests.Signs(prepAtlasGame,prepAtlasDocs,prepAtlasOutput,true);return;}
if(args is ["--preparation-sign-tests",var prepSignGame,var prepSignDocs,var prepSignOutput]){RoutePreparationTests.Signs(prepSignGame,prepSignDocs,prepSignOutput);return;}
if(args is ["--preparation-hash-tests"]){RoutePreparationTests.Hashes();return;}
if(args is ["--preparation-network-benchmark",var prepCache,var prepOutput]){RoutePreparationTests.Benchmark(prepCache,prepOutput);return;}
if(args is ["--memory-unit-tests",var memoryUnitOutput]){MemoryOptimizationTests.Units(memoryUnitOutput);return;}
if(args is ["--memory-sii-equivalence",var siiGame,var siiDocs,var siiOutput]){MemoryOptimizationTests.RealSii(siiGame,siiDocs,siiOutput);return;}
if(args is ["--memory-route-benchmark",var routeMemGame,var routeMemDocs,var routeMemCapture,var routeMemOutput]){MemoryOptimizationTests.RouteBenchmark(routeMemGame,routeMemDocs,routeMemCapture,routeMemOutput);return;}
if(args is ["--memory-benchmark",var memGame,var memDocs,var memCapture,var memOutput]){MemoryOptimizationTests.Benchmark(memGame,memDocs,memCapture,memOutput);return;}
if(args is ["--diagnostics-fixture"]){RuntimeDiagnostics.Start();for(int i=0;i<250;i++){using(PerformanceTrace.Measure("route:synthetic"))Thread.Sleep(7);if(i%50==0)GC.Collect();await Task.Delay(15);}return;}
if(args is ["--hardening-cache-keys",var hardeningDocs,var hardeningPrevious,var hardeningResult]){HardeningCacheKeys.Run(hardeningDocs,hardeningPrevious,hardeningResult);return;}
if(args is ["--cache-retention-tests",var retentionOutput]){CacheRetentionTests.Run(retentionOutput);StorageBudgetTests.Run(retentionOutput);return;}
if(args is ["--native-path-fix-tests",var pathGame,var pathDocs,var pathCapture,var pathOutput]){NativePathFixTests.Run(pathGame,pathDocs,pathCapture,pathOutput);return;}
if(args is ["--holistic-audit-tests",var holisticOutput]){await HolisticAuditTests.Run(holisticOutput);return;}
if(args is ["--network-equivalence-tests",var baselineNetwork,var equivalenceOutput]){NetworkEquivalenceTests.Run(baselineNetwork,equivalenceOutput);return;}
if(args is ["--route-rejection-audit",var rejectionAuditOutput]){await RouteRecoveryTests.Run(rejectionAuditOutput);return;}
if(args is ["--route-recovery-tests",var recoveryOutput]){await RouteRecoveryTests.Run(recoveryOutput);return;}
if(args is ["--duplicate-boundary-tests",var duplicateCapture,var duplicateOutput]){DuplicateBoundaryTests.Run(duplicateCapture,duplicateOutput);return;}
if(args is ["--interaction-fix-tests",var interactionFixOutput]){InteractionFixTests.Run(interactionFixOutput);return;}
if(args is ["--roundabout-real-tests",var circleGame,var circleDocs,var circleOutput]){InteractionFixTests.RealRoundabouts(circleGame,circleDocs,circleOutput);return;}
if(args is ["--interaction-probes",var interactionProbeOutput]){InteractionAudit.Run("","",interactionProbeOutput,false);return;}
if(args is ["--phone-integration-tests",var phoneNative,var phoneOutput]){PhoneIntegrationTests.Run(phoneNative,phoneOutput);return;}
if(args is ["--interaction-audit",var interactionGame,var interactionDocs,var interactionOutput])
{InteractionAudit.Run(interactionGame,interactionDocs,interactionOutput);return;}
if(args is ["--complete-world-texture-verification",var cachedTextureGame,var cachedTextureDocs,var cachedTextureManifest,var cachedTextureVerification])
{SystemAuditInspection.CompleteTextureVerification(cachedTextureGame,cachedTextureDocs,cachedTextureManifest,cachedTextureVerification);return;}
if(args is ["--verify-world-cache-assets",var verifyGame,var verifyDocs,var verifyManifest,var verifyOutput])
{SystemAuditInspection.VerifyWorldAssets(verifyGame,verifyDocs,verifyManifest,verifyOutput);return;}
if(args is ["--cache-upgrade-keys",var keyGame,var keyDocs,var keyOldBinary,var keyOutput])
{SystemAuditInspection.CacheKeys(keyGame,keyDocs,keyOldBinary,keyOutput);return;}
if(args is ["--seven-fixes-tests",var sevenDirectory]){SevenFixesTests.Run(sevenDirectory);return;}
if(args is ["--audit-maneuvers",var auditGame,var auditDocs,var auditDirectory,var auditOutput])
{SystemAuditInspection.Run(auditGame,auditDocs,auditDirectory,auditOutput);return;}
if(args is ["--world-cache-tests"]){WorldCacheTests.Run();return;}
if(args is ["--progressive-tests"]){await ProgressiveTests.Run();return;}
if(args is ["--speed-benchmark",var speedDirectory,var speedOutput]){SpeedBenchmark.Run(speedDirectory,speedOutput);return;}
if(args is ["--sign-axis-audit",var signAxisFile]){SignRobustnessTests.Audit(signAxisFile);return;}
if(args is ["--sign-robustness-tests"]){SignRobustnessTests.Run();return;}
if(args is ["--traffic-street-tests"]){TrafficStreetTests.Run();return;}
if(args is ["--inspect-remaining-map",var inspectGame,var inspectDocs,var inspectCapture,var inspectResult,var inspectOutput,var firstPortion,var lastPortion])
{RemainingMapInspection.Run(inspectGame,inspectDocs,inspectCapture,inspectResult,inspectOutput,int.Parse(firstPortion),int.Parse(lastPortion));return;}
if(args is ["--local-route-tests"]){LocalRouteTests.Run();return;}
if(args is ["--connection-target-tests"]){ConnectionTargetTests.Run();return;}
if(args is ["--verify-overlay-census",var overlayCensusFolder])
{OverlayCensus.Verify(overlayCensusFolder);return;}
if(args is ["--overlay-textures",var textureOverlayGame,var textureOverlayDocs,var textureOverlayOutput])
{OverlayCensus.Textures(textureOverlayGame,textureOverlayDocs,textureOverlayOutput);return;}
if(args is ["--overlay-census",var overlayGame,var overlayDocs,var overlayOutput])
{OverlayCensus.Run(overlayGame,overlayDocs,overlayOutput);return;}
if(args is ["--inspect-remaining-map",var remainingGame,var remainingDocs,var remainingCapture,var remainingResult,var remainingOutput])
{RemainingMapInspection.Run(remainingGame,remainingDocs,remainingCapture,remainingResult,remainingOutput);return;}
if(args is ["--latest-route-tests"]){LatestRouteTests.Run();return;}
if(args is ["--performance-tests"]){PerformanceTests.Run();return;}
if(args is ["--remaining-name-tests"]){RemainingRoadNamesTests.Run();return;}
if(args is ["--inspect-corridors",var corridorDirectory,var corridorOut])
{
    var network=System.Text.Json.JsonSerializer.Deserialize<NamedNetworkSnapshot>(File.ReadAllText(Directory.GetFiles(corridorDirectory,"*_named-network.json").Single()),LiveProtocol.Json)!;
    var graph=new DirectedRoadNetworkGraph(network.Region,network.Portions,network.Connections,network.Errors);
    File.WriteAllText(corridorOut,System.Text.Json.JsonSerializer.Serialize(network.Sources.Where(s=>s.Role=="GuideTo"&&s.AssociationStatus=="DIRECTIONAL_BRANCH_TARGET")
        .Select(s=>new{s.Id,s.Identity,s.Cardinal,s.Raw,ids=RoadNameContinuity.SignedCorridor(graph,s)}),LiveProtocol.Json));return;
}
if(args is ["--atlas-census",var atlasGame,var atlasDocs,var atlasOutput])
{
    Directory.CreateDirectory(atlasOutput);
    using var assets=GameAssets.Open(new Options{GameRoot=atlasGame,DocumentsRoot=atlasDocs,OutputRoot=atlasOutput,ProbePath="."});
    var rows=new List<object>();
    foreach(var path in assets.Loader.GetFiles("/def/sign/atlas/").Where(p=>p.EndsWith(".sii",StringComparison.OrdinalIgnoreCase)))
    {
        try
        {
            var file=TruckLib.Sii.SiiFile.Open(path,assets.Loader);
            foreach(var unit in file.Units.Where(u=>u.Class=="image_atlas"))
            if(unit.Attributes.TryGetValue("images_names",out var raw)&&raw is System.Collections.IEnumerable names)
            rows.Add(new{path,entries=names.Cast<object>().Select(SignTemplateResolver.CleanToken).Select(entry=>new{entry,road=SignPanelExtractor.DecodeRoad(entry)}),
                material=unit.Attributes.GetValueOrDefault("material_path")?.ToString()});
        }
        catch(Exception ex){rows.Add(new{path,error=ex.Message});}
    }
    File.WriteAllText(Path.Combine(atlasOutput,"atlases.json"),System.Text.Json.JsonSerializer.Serialize(rows,LiveProtocol.Json));
    assets.WriteManifest(Path.Combine(atlasOutput,"asset_manifest.json"));
    Console.WriteLine($"Inspected {rows.Count} effective mounted atlases.");return;
}
if(args is ["--inspect-sign-variants",var variantGame,var variantDocs,var variantOutput])
{
    using var assets=GameAssets.Open(new Options{GameRoot=variantGame,DocumentsRoot=variantDocs,OutputRoot=variantOutput,ProbePath="."});
    var map=TruckLib.ScsMap.Map.Open("/map/usa.mbd",assets.Loader);
    var resolver=new SignRoadIdentityResolver(map,assets.Loader);
    foreach(var sign in resolver.Signs.Where(s=>s.Uid is 0x570163E593180000 or 0x57015553A0980001))
    {
        var support=new BoardModelResolver(assets.Loader).ResolveSupport(sign);
        var model=TruckLib.Models.Model.Open(sign.ModelDesc,assets.Loader);
        Console.WriteLine(System.Text.Json.JsonSerializer.Serialize(new{sign.Uid,sign.ModelDesc,sign.Variant,support,
            parts=model.Parts.Select(p=>p.Name.ToString()),variants=model.Variants.Select(v=>new{name=v.Name.ToString(),attributes=v.Attributes})},new System.Text.Json.JsonSerializerOptions{IncludeFields=true}));
    }
    return;
}
if(args is ["--inspect-lane-offsets",var offsetGame,var offsetDocs])
{
    using var assets=GameAssets.Open(new Options{GameRoot=offsetGame,DocumentsRoot=offsetDocs,OutputRoot="validation/navigation-complete",ProbePath="."});
    var seen=new HashSet<string>();
    foreach(var file in assets.Loader.GetFiles("/def/world/").Where(f=>f.EndsWith(".sii")))
    foreach(var unit in TruckLib.Sii.SiiFile.Open(file,assets.Loader).Units.Where(u=>u.Class=="road_look"))
    foreach(var attr in unit.Attributes.Where(a=>a.Key.StartsWith("lane_offsets")))
    if(attr.Value is System.Collections.IEnumerable list)
    foreach(var value in list)
    {
        var type=value?.GetType().FullName??"null";
        if(seen.Add(type))Console.WriteLine($"{unit.Name} {attr.Key}: {type} = {value}");
    }
    return;
}
if(args is ["--upgrade-pullthrough-saved",var pullThroughDirectory]){NamedNetworkTests.RefreshSaved(pullThroughDirectory,true);return;}
if(args is ["--export-corridor-fixtures",var corridorFixtures]){NamedNetworkTests.ExportCorridorFixtures(corridorFixtures);return;}
if(args is ["--refresh-named-networks",var refreshDirectory]){NamedNetworkTests.RefreshSaved(refreshDirectory);return;}
if(args is ["--named-network-tests"]){NamedNetworkTests.Run();return;}
if(args is ["--verify-named-networks",var networkDirectory]){NamedNetworkTests.VerifySaved(networkDirectory);return;}
if(args is ["--support-face-real",var faceGame,var faceDocs,var facePanels,var faceOutput]){NamedNetworkTests.RealSupport(faceGame,faceDocs,facePanels,faceOutput);return;}
if(args is ["--road-identity-tests"]){RoadIdentityTests.Run();return;}
if(args is ["--board-geometry-tests"]){BoardGeometryTests.Run();return;}
if(args is ["--board-real-tests",var boardGroups,var boardInspection]){BoardGeometryTests.Real(boardGroups,boardInspection);return;}
if(args is ["--recompile-board-geometry",var boardGame,var boardDocs,var boardCapture,var boardOutput])
{PanelReplayTests.Run(boardGame,boardDocs,boardCapture,boardOutput,false,true);return;}
if(args is ["--inspect-board-surfaces",var surfaceGame,var surfaceDocs,var surfacePaths,var surfaceOutput]){BoardSurfaceInspection.Run(surfaceGame,surfaceDocs,surfacePaths,surfaceOutput);return;}
if(args is ["--group-guidance-tests"]){GroupGuidanceTests.Run();return;}
if(args is ["--texture-proof-test",var textureGame,var textureDocs,var textureOutput]){GroupGuidanceTests.Texture(textureGame,textureDocs,textureOutput);return;}
if(args is ["--verify-group-replay",var previousRecording,var newGroupRoute,var groupResult]){GroupGuidanceTests.Replay(previousRecording,newGroupRoute,groupResult);return;}
if(args is ["--capture-audit-tests"]){CaptureAuditTests.Run();return;}
if(args is ["--inspect-mounts",var mountGame,var mountDocs,var mountOutput]){CaptureAuditTests.InspectResources(mountGame,mountDocs,mountOutput);return;}
if(args is ["--inspect-mounts-full",var fullGame,var fullDocs,var fullOutput]){CaptureAuditTests.InspectResources(fullGame,fullDocs,fullOutput,true);return;}
if(args is ["--inspect-inversions",var invGame,var invDocs,var invOutput]){InversionInspection.Run(invGame,invDocs,invOutput);return;}
if(args is ["--network-tests"]){LaneNetworkTests.Run();return;}
if(args is ["--refresh-groups-audit",var groupAudit,var groupRoute]){LaneAuditExport.RefreshGroups(groupAudit,groupRoute);return;}
if(args is ["--group-tests"]){SignGroupingTests.Run();return;}

if(args is ["--lane-tests"]){LaneAuditExport.Tests();return;}
if(args is ["--audit-lanes",var laneGame,var laneDocs,var laneCapture,var laneOutput])
{ PanelReplayTests.Run(laneGame,laneDocs,laneCapture,laneOutput,true);return; }

if (args is ["--recompile-panels", var replayGame, var replayDocs, var replayCapture, var replayOutput])
{
    PanelReplayTests.Run(replayGame,replayDocs,replayCapture,replayOutput);
    return;
}

if (args is ["--extract-panels", var panelGame, var panelDocs, var panelIndexes, var panelSelected, var panelOutput])
{
    PanelExtractionTests.Run(panelGame,panelDocs,panelIndexes,panelSelected,panelOutput);
    return;
}

if (args is ["--inspect-sign-assets", var signGame, var signDocs, var signPaths, var signOutput])
{
    SignAssetInspection.Run(signGame, signDocs, signPaths, signOutput);
    return;
}

if(args.FirstOrDefault()=="--native")
{
    if(args is ["--native","--verify-recording",var recordingRoot])
    { NativeTests.VerifyMapRecording(recordingRoot); return; }
    if(args is ["--native","--map-live",var fixture,var game,var docs,var output])
    { await NativeTests.MapIntegration(fixture,game,docs,output); return; }
    await NativeTests.Run(args.Skip(1).FirstOrDefault());
    return;
}

if (args is ["--live-captures", var captureDirectory])
{
    var valid = Directory.GetFiles(captureDirectory,"*_v11_regression.txt")
        .Where(p => File.ReadAllLines(p).Contains("structural_snapshot_status=VALID")).ToArray();
    foreach (var file in valid)
    {
        var stem = Path.GetFileName(file)[..^"_v11_regression.txt".Length];
        var compiledCapture = LiveCaptureCompiler.Compile(captureDirectory,stem);
        LiveProtocol.Encode(new(LiveProtocol.VersionFor(compiledCapture),"RouteSnapshot","validation",1,0,1,compiledCapture));
    }
    if (valid.Length == 0) throw new Exception("No validated captures found.");
    Console.WriteLine($"PASS: {valid.Length} real compiled captures satisfy live protocol limits and corridor invariants.");
    return;
}

if (args.FirstOrDefault() == "--live")
{
    await LiveTests.Run(args.Skip(1).FirstOrDefault());
    return;
}

if (args is ["--inspect-city-assets", var gameRoot, var documentsRoot])
{
    using var assets = GameAssets.Open(new Options { GameRoot = gameRoot, DocumentsRoot = documentsRoot, ProbePath = ".", OutputRoot = "." });
    var fs = assets.Loader;
    Console.WriteLine($"city directory exists: {fs.DirectoryExists("/def/city/")}");
    foreach (var file in fs.GetFiles("/def/").Where(f => Path.GetFileName(f).StartsWith("city", StringComparison.OrdinalIgnoreCase) && f.EndsWith(".sii")).Take(3))
    {
        Console.WriteLine(file);
        foreach (var unit in TruckLib.Sii.SiiFile.Open(file, fs).Units.Take(3))
            Console.WriteLine($"  class={unit.Class} name={unit.Name}: " + string.Join(";", unit.Attributes.Select(kv => $"{kv.Key}={kv.Value}")));
    }
    var resolver = new SignRoadIdentityResolver(new TruckLib.ScsMap.Map(), fs);
    if (resolver.KnownDestinationCount == 0 || !resolver.IsKnownDestination("Los Angeles"))
        throw new Exception("Mounted .sui city includes were not loaded correctly.");
    Console.WriteLine($"PASS: {resolver.KnownDestinationCount} mounted city names, including Los Angeles.");
    return;
}

var assertions = 0;
void Check(bool condition, string message)
{
    assertions++;
    if (!condition) throw new Exception(message);
}

foreach (var text in new[] { "LEFT LANE", "RIGHT LANE", "RIGHT 2 LANES", "SECOND RIGHT", "RIGHT TURN", "LEFT TURN", "TO", "PREPASS", "ALL TRUCKS", "KEEP LEFT", "EXIT ONLY", "IN 2 LANES", "NEXT EXIT", "35 MPH", "EXIT 57", "I-5 NORTH", "/def/sign/atlas/is5.mat", "@@city_name@@" })
    Check(!SignTextClassifier.IsDestinationCandidate(text), $"Instruction/technical text accepted as destination: {text}");
foreach (var text in new[] { "Los Angeles", "Bakersfield", "North Las Vegas", "West Sacramento", "Death Valley", "Central Ave", "Turners Falls", "Right Hand Canyon", "St. George" })
    Check(SignTextClassifier.IsDestinationCandidate(text), $"Genuine place candidate rejected: {text}");
Check(SignTextClassifier.ExplicitCardinals("North Las Vegas").Count == 0, "Place name manufactured NORTH");
Check(SignTextClassifier.ExplicitCardinals("West Sacramento").Count == 0, "Place name manufactured WEST");
Check(SignTextClassifier.ExplicitCardinals("I-5 NORTH").SequenceEqual(new[] { "NORTH" }), "Explicit route direction lost");
Check(SignTextClassifier.DirectionHint(new[] { "Right Hand Canyon" }) == "", "Place name manufactured RIGHT branch hint");
Check(SignTextClassifier.DirectionHint(new[] { "RIGHT LANE" }) == "RIGHT", "Lane guidance lost");
Check(SignTextClassifier.DirectionHint(new[] { "LEFT LANE", "RIGHT LANE" }) == "", "Conflicting arrows chose a branch");
Check(!SignTextClassifier.IsRouteText("IN 2 LANES"), "Lane instruction became state route");
foreach(var (text,direction) in new[]{("I-10 E","EAST"),("I-10 W","WEST"),("I-5 N","NORTH"),("I-5 S","SOUTH")})
{
    Check(SignTextClassifier.IsRouteText(text)&&!SignTextClassifier.IsDestinationCandidate(text),"Abbreviated signed route became destination");
    Check(SignTextClassifier.ExplicitCardinals(text).SequenceEqual(new[]{direction}),"Abbreviated signed cardinal lost");
}
Check(SignTextClassifier.ExplicitCardinals("E Main St").Count==0&&SignTextClassifier.IsDestinationCandidate("E Main St"),"Street name prefix became route direction");
Check(!SignTextClassifier.IsDestinationCandidate("EXIT"),"Exit label became destination");

ParsedSignArea Area(string[] raw, string[]? shields = null, string[]? cardinals = null,
    string[]? destinations = null, string[]? exits = null, string type = "System.String") =>
    new("board1", [1], raw.Select((s, i) => new ParsedSignAttribute(1, (uint)i, type, s)).ToList(),
        "", 0, 0, raw, exits ?? [], shields ?? [], cardinals ?? [], destinations ?? [], []);
bool Known(string value) => new[] { "Los Angeles", "Bakersfield", "North Las Vegas" }.Contains(value, StringComparer.OrdinalIgnoreCase);
SignFieldValidation Validate(ParsedSignArea area, string identity = "", string cardinal = "", SignBoardInfo? board = null) =>
    SignSemanticValidator.Validate(area, board, identity, cardinal, Known);

var known = Validate(Area(["Los Angeles"], destinations: ["Los Angeles"]));
Check(known.DestinationConfidence == "HIGH" && known.ValidatedToward == "Los Angeles", "Known destination not validated");
var mixed = Validate(Area(["Los Angeles", "Central Ave", "RIGHT LANE"], destinations: ["Los Angeles", "Central Ave", "RIGHT LANE"]));
Check(mixed.DestinationConfidence == "MEDIUM" && mixed.ValidatedToward == "Los Angeles", "Unverified text inherited HIGH");
Check(Validate(Area(["Central Ave"], destinations: ["Central Ave"])).ValidatedToward == "", "Unknown street promoted to HIGH");
Check(Validate(Area(["Los Angeles"], destinations: ["Los Angeles"], type: "System.Int32")).ValidatedToward == "", "Non-text attribute accepted as destination proof");
var route = Validate(Area(["I-5", "NORTH"], shields: ["I-5"], cardinals: ["NORTH"]), "I-5", "NORTH");
Check(route.IdentityConfidence == "HIGH" && route.CardinalConfidence == "HIGH", "Explicit shield/cardinal pair lost");
Check(Validate(Area(["US-99"], shields: ["I-5"]), "I-5").IdentityConfidence != "HIGH", "Wrong route text validated another shield");
Check(Validate(Area(["/def/sign/atlas/is5.mat"], shields: ["I-5"]), "I-5").IdentityConfidence == "HIGH", "Atlas shield evidence lost");
Check(Validate(Area(["North Las Vegas"], cardinals: ["NORTH"], destinations: ["North Las Vegas"]), "", "NORTH").CardinalConfidence != "HIGH", "Destination supplied signed cardinal");
Check(Validate(Area(["I-5", "US-99", "NORTH"], shields: ["I-5", "US-99"], cardinals: ["NORTH"]), "I-5", "NORTH").CardinalConfidence != "HIGH", "Multiple shields assigned a single cardinal");
Check(Validate(Area(["I-5", "NORTH", "SOUTH"], shields: ["I-5"], cardinals: ["NORTH", "SOUTH"]), "I-5", "NORTH").CardinalConfidence == "CONFLICT", "Cardinal conflict hidden");
var exitBoard = new SignBoardInfo("exit", "/def/sign/board/exit.sii", "OK", "EXIT", "", 1);
var shieldBoard = new SignBoardInfo("shield", "/def/sign/board/interstate.sii", "OK", "SHIELD", "I", 1);
Check(Validate(Area(["EXIT 57 A"], exits: ["57A"])).ExitConfidence == "HIGH", "Explicit exit not validated");
Check(Validate(Area(["57"], exits: ["57"]), board: exitBoard).ExitConfidence == "HIGH", "Exit board numeric evidence lost");
Check(Validate(Area(["57"], exits: ["57"]), board: shieldBoard).ExitConfidence != "HIGH", "Shield number became exit");
Check(Validate(Area(["57"], exits: ["57"])).ExitConfidence != "HIGH", "Bare number without board context became exit");
Check(Validate(Area(["5"]), "I-5", board: shieldBoard).IdentityConfidence == "HIGH", "Resolved shield board lost numeric identity");
Check(Validate(Area(["5"]), "US-5", board: shieldBoard).IdentityConfidence != "HIGH", "Shield family mismatch inherited HIGH");
Check(SignSemanticValidator.Combine("HIGH", "MEDIUM") == "MEDIUM", "Association promoted weak field");
Check(SignSemanticValidator.Combine("MEDIUM", "HIGH") == "MEDIUM", "Field promoted weak association");
Check(SignSemanticValidator.Combine("AMBIGUOUS", "HIGH") == "AMBIGUOUS", "Ambiguous association lost");
Check(SignSemanticValidator.Combine("HIGH", "NONE") == "NONE", "Missing field inherited HIGH");
Check(SignSemanticValidator.Combine("HIGH", "CONFLICT") == "CONFLICT", "Field conflict inherited HIGH");
Console.WriteLine($"PASS: {assertions} semantic regression assertions.");
