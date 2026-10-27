using System.Numerics;
using System.Text.Json;
using BetterGPSNavigation;

internal static class GroupGuidanceTests
{
    public static void Texture(string game,string documents,string output)
    {
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=documents,ProbePath=".",OutputRoot=output});
        var hash=assets.TextureSha256("/material/sign/atlas/down_arrow.tobj");
        var expected=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes("validation/sign-reconstruction-research/assets/material/sign/atlas/down_arrow.dds")));
        if(hash!=expected)throw new Exception("Effective mounted texture differs from independently extracted DDS");
        Directory.CreateDirectory(output);assets.WriteManifest(Path.Combine(output,"texture-manifest.json"));
        File.WriteAllText(Path.Combine(output,"texture-proof.json"),JsonSerializer.Serialize(new{hash,independentDdsSha256=expected,match=true},LiveProtocol.Json));
        Console.WriteLine("PASS: effective mounted arrow texture hash matches independently extracted DDS.");
    }
    public static void Replay(string oldRecording,string newRoute,string output)
    {
        var next=LiveProtocol.Decode(File.ReadAllBytes(newRoute)).Route!;
        var oldEngine=new LiveGuidanceEngine();var newEngine=new LiveGuidanceEngine();
        int frames=0,snapshots=0,calibratedFrames=0;double maximumDistanceCorrectionM=0;var laneStates=new Dictionary<string,int>();
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        using var compiled=new StreamWriter(Path.ChangeExtension(output,"compiled.jsonl"));
        using var samples=new StreamWriter(Path.ChangeExtension(output,"console.txt"));
        string? previousSample=null;
        foreach(var line in File.ReadLines(oldRecording).Where(l=>!string.IsNullOrWhiteSpace(l)))
        {
            var m=LiveProtocol.Decode(System.Text.Encoding.UTF8.GetBytes(line));
            var changed=m;
            if(m.Route is {} r)
            {
                if(r.RouteId!=next.RouteId)throw new Exception("Replay route differs from target capture");
                if(r.Actions.Length!=next.Actions.Length || r.Actions.Zip(next.Actions).Any(pair=>
                    pair.First.Id!=pair.Second.Id || pair.First.Instruction!=pair.Second.Instruction ||
                    pair.First.NavStationM!=pair.Second.NavStationM || pair.First.BranchId!=pair.Second.BranchId))
                    throw new Exception("Action, branch or station changed");
                changed=m with{Route=next,Version=LiveProtocol.VersionFor(next)};snapshots++;
            }
            var before=oldEngine.Accept(m);var after=newEngine.Accept(changed);
            compiled.WriteLine(System.Text.Encoding.UTF8.GetString(LiveProtocol.Encode(changed with{Version=LiveProtocol.VersionFor(next)})));
            var sampleKey=JsonSerializer.Serialize(new{after.Status,after.ActionId,after.CurrentRoad,after.TargetRoad,after.Destination,after.DestinationCandidates},LiveProtocol.Json);
            if(sampleKey!=previousSample){samples.WriteLine(NativeBridgeCli.Describe(after));previousSample=sampleKey;}
            bool distanceValid=before.DistanceM==after.DistanceM;
            if(after.Detail.Contains("ATS_REMAINING_CALIBRATED")&&before.DistanceM is double oldDistance&&after.DistanceM is double newDistance)
            {
                var edge=next.Edges.First(e=>e.WorldStartM<=after.WorldStationM&&e.WorldEndM>=after.WorldStationM);
                var tolerance=Math.Max(5,(edge.NavEndM-edge.NavStartM)/Math.Max(1,edge.WorldEndM-edge.WorldStartM)*3);
                var correction=Math.Abs(oldDistance-newDistance);distanceValid=correction<=tolerance+.01;
                calibratedFrames++;maximumDistanceCorrectionM=Math.Max(maximumDistanceCorrectionM,correction);
            }
            if(before.Status!=after.Status || before.ActionId!=after.ActionId || before.Instruction!=after.Instruction ||
                !distanceValid || before.ThenInstruction!=after.ThenInstruction || before.ThenDistanceM!=after.ThenDistanceM)
                throw new Exception($"Guidance changed at sequence {m.Sequence}");
            if(after.Detail.Contains("Lane="))
            {
                var state=after.Detail.Split("Lane=")[1].Split(':')[0];laneStates[state]=laneStates.GetValueOrDefault(state)+1;
            }
            frames++;
        }
        if(snapshots==0||frames<10)throw new Exception("Recording does not contain a route and vehicle frames");
        File.WriteAllText(output,JsonSerializer.Serialize(new{frames,snapshots,actionCount=next.Actions.Length,actionsAndThenUnchanged=true,
            distanceCorrectionsBoundedByThreeWorldMetres=true,calibratedFrames,maximumDistanceCorrectionM,laneStates,
            note="Recorded real vehicle frames; lane locations are geometric candidates, not ingame ground truth."},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine($"PASS: {frames} recorded messages; actions/status/then unchanged; {calibratedFrames} calibrated distances (max correction {maximumDistanceCorrectionM:0.00} GPS metres).");
    }
    public static void Run()
    {
        int n=0;void Check(bool b,string why){n++;if(!b)throw new Exception(why);}
        var panels=JsonSerializer.Deserialize<ExtractedSignPanel[]>(File.ReadAllText("validation/ingame-capture-v2/20261010_152355_4f394069/live_000007_FEB0FDDD39647C99_panels.json"))!;
        var panel=panels.Single(p=>p.SignUid=="553A567734180000"&&p.AreaName=="sb3_1");
        var north=panel.SemanticGroups.Single(g=>g.Road=="I-5");var east=panel.SemanticGroups.Single(g=>g.Road=="I-10");
        bool Known(string s)=>s is "Sacramento" or "San Bernardino";
        var fields=SignGroupGuidance.ValidateFields(north,panel,Known);
        Check(fields.IdentityConfidence=="HIGH"&&fields.CardinalConfidence=="HIGH"&&fields.ValidatedToward=="Sacramento","Complete group rejected because its plaque is multiple");
        var ef=SignGroupGuidance.ValidateFields(east,panel,Known);
        Check(ef.IdentityConfidence=="HIGH"&&ef.CardinalConfidence=="HIGH"&&ef.ValidatedToward=="San Bernardino","Other column inherited north semantics");
        var fake=SignGroupGuidance.ValidateFields(north with{Road="I-10",Cardinal="EAST",Destination="San Bernardino"},panel,Known);
        Check(fake.IdentityConfidence!="HIGH"&&fake.CardinalConfidence!="HIGH"&&fake.DestinationConfidence!="HIGH","Fields leaked across source object IDs");
        var chain=new LaneChain("lane","CURRENT_EXIT_ONLY",["gps"],["lane","ppd"],[],[]);
        SignArrowPlacement Arrow(string exit="gps",string origin="BOTTOM_CENTRE",string[]? blocks=null)=>new(north.ArrowObjectIds.Single(),"part",origin,[0,5,5],
            new(exit=="gps"?"SELECTED_EXIT_CANDIDATE":"OTHER_EXIT_CANDIDATE","LOW",
                [new(0,0,5,new(0,0,5),"PPD_SAMPLED",[exit]){LaneId="lane",Chain=chain with{CurrentExits=[exit]}}],[exit],blocks??[]));
        SignGroupProof Eval(SignArrowPlacement[] arrows,bool verified=false)=>SignGroupGuidance.Evaluate(0,north,panel,"gps",arrows,verified,fields,[],[]);
        var provisional=Eval([Arrow()]);Check(provisional.Status=="GPS_COMPATIBLE_CANDIDATE"&&provisional.Confidence!="HIGH","Uncalibrated placement promoted");
        var confirmed=Eval([Arrow()],true);Check(confirmed.Status=="SELECTED_EXIT_CONFIRMED"&&confirmed.Confidence=="HIGH","Current exit depends on missing future decision");
        Check(Eval([Arrow("other")]).Status=="OTHER_EXIT_CANDIDATE","Wrong exit not classified");
        Check(Eval([Arrow(),Arrow("other","BOTTOM_LEFT")]).Status=="AMBIGUOUS_PLACEMENT","Chose origin by GPS agreement");
        Check(Eval([Arrow(),Arrow(origin:"BOTTOM_LEFT")]).Confidence=="LOW","Consensus of guesses treated as calibrated");
        Check(Eval([Arrow(blocks:["LANE_WIDTH_4_5M_ASSUMED"])],true).Confidence!="HIGH","Width assumption promoted to calibrated ownership");
        Check(Eval([],true).Status=="NO_ARROW_GEOMETRY","Missing arrow accepted");
        var identifierPanel=panel with{Objects=panel.Objects.Select(o=>o with{Arrow=""}).ToArray()};
        var identifier=SignGroupGuidance.Evaluate(0,north with{ArrowObjectIds=[]},identifierPanel,"gps",[],false,fields,[],["BOARD_MODEL_NOT_FOUND"]);
        Check(identifier.Status=="ROAD_IDENTITY_UNASSIGNED"&&identifier.Road=="I-5","Road reading wrongly requires arrow placement");
        Check(!identifier.Blockers.Contains("ARROW_MISSING")&&!identifier.Blockers.Contains("BOARD_MODEL_NOT_FOUND"),"Road reading inherits unrelated placement failures");
        Check(identifier.Confidence!="HIGH"&&identifier.ReachableExits.Length==0,"Arrowless identifier silently assigned to GPS exit");
        var optional=north with{Cardinal="",Destination="",Blockers=north.Blockers.Concat(new[]{"CARDINAL_MISSING_OR_MULTIPLE","DESTINATION_MISSING_OR_MULTIPLE"}).ToArray()};
        var optionalPanel=panel with{Objects=panel.Objects.Select(o=>o.Text is "NORTH" or "Sacramento"?o with{Text=""}:o).ToArray()};
        // HTML-marked cardinal spelling is normalized before removing the optional fixture text.
        optionalPanel=optionalPanel with{Objects=optionalPanel.Objects.Select(o=>SignRoadIdentityResolver.NormalizeText(o.Text)=="NORTH"?o with{Text=""}:o).ToArray()};
        Check(SignGroupGuidance.Evaluate(0,optional,optionalPanel,"gps",[Arrow()],true,fields,[],[]).Confidence=="HIGH","Optional text blocks an otherwise proven route");
        LiveProof Encode(SignGroupProof g)=>new("group",JsonSerializer.Serialize(g,SignGroupProof.Json),g.Confidence,"SIGN_GROUP",g.GroupId,"gps","");
        var edge=new LiveEdge(0,"0000000000000001",new(0,0,0),new(100,0,0),0,100,0,1000);
        var action=new LiveAction("a",0,0,500,"TURN_LEFT","gps","HIGH",[new("action","TURN_LEFT","HIGH","PPD","prefab","gps","")]);
        LiveGuidance Guide(params SignGroupProof[] gs)
        {
            var engine=new LiveGuidanceEngine();engine.Accept(new(1,"Hello","s",0,0,0));
            engine.Accept(new(1,"RouteSnapshot","s",1,0,1,new("r",[edge],[action with{Proofs=action.Proofs.Concat(gs.Select(Encode)).ToArray()}])));
            return engine.Accept(new(1,"VehicleState","s",2,100,1,Vehicle:new(new(0,0,0),0,10,false)));
        }
        var guided=Guide(confirmed);Check(guided.Road=="I-5"&&guided.Cardinal=="NORTH"&&guided.Destination=="Sacramento","Live engine does not consume a complete group");
        var cruise=guided with{Status="CRUISE",Instruction="KEEP_RIGHT",DistanceM=77*1609.344,DistanceMi=77,
            Road=null,Cardinal=null,Destination=null,CurrentRoad=new("RESOLVED",
                [new("CA-99","RouteReference","NORTH","MEDIUM","MEDIUM",["local-signs"])],["segment:103"],"VEHICLE_SEGMENT"),ContinueDistanceM=50*1609.344};
        Check(NativeBridgeCli.Describe(cruise).StartsWith("Continue for 50.0 mi on CA-99 NORTH")&&
            NativeBridgeCli.Describe(cruise).Contains("keep right in 77.0 mi"),"Distant maneuver must not replace current road in cruise");
        Check(NativeBridgeCli.Describe(cruise with{ContinueDistanceM=null}).StartsWith("Continue on CA-99 NORTH"),
            "Known current road survives an unresolved continuation distance");
        Check(!NativeBridgeCli.Describe(cruise with{Status="PREPARE"}).StartsWith("Continue"),"Approaching maneuver retains priority");
        var access=cruise with{CurrentRoad=new("UNKNOWN",[],["ramp"],"UNNAMED_CONNECTOR"),TargetRoad=null,
            SignTargets=[new("CA-180","WEST","","MEDIUM","captured-access")]};
        Check(NativeBridgeCli.Describe(access).Contains("toward CA-180 WEST")&&
            !NativeBridgeCli.Describe(access).StartsWith("Continue"),"An unnamed access retains its signed destination without inventing a present-road continuation");
        var exitObject=panel.Objects.First(o=>o.Kind=="sign_template_text") with{Id=987,Text="EXIT 57A",Visible=true,Reliable=true,SchemaStatus="SUPPORTED"};
        var exitPanel=panel with{Objects=panel.Objects.Append(exitObject).ToArray()};
        var exitGroup=north with{ObjectIds=north.ObjectIds.Append(987u).ToArray()};
        Check(SignGroupGuidance.ExplicitExit(exitGroup,exitPanel)=="57A"&&SignGroupGuidance.ValidateFields(exitGroup,exitPanel,Known).ExitConfidence=="HIGH",
            "Explicit exit text retains field-specific evidence within its group");
        Check(SignGroupGuidance.ExplicitExit(north,exitPanel) is null,
            "An exit in another semantic group cannot label the selected group");
        Check(SignGroupGuidance.ExplicitExit(exitGroup,exitPanel with{Objects=exitPanel.Objects.Select(o=>o.Id==987?o with{Text="57"}:o).ToArray()}) is null,
            "Bare shield numbers cannot become exit numbers");
        Check(Guide(confirmed with{Exit="57A",Fields=fields with{ExitConfidence="HIGH"}}).Exit=="57A",
            "Live group selection preserves an independently validated exit number");
        Check(Guide(provisional).Road is null,"Provisional route displayed as confirmed");
        var competing=confirmed with{GroupId=east.Id,Road=east.Road,Cardinal=east.Cardinal,Destination=east.Destination,Fields=ef};
        Check(Guide(confirmed,competing).Road is null&&Guide(confirmed,competing).Destination is null,"Two groups collapsed into one arbitrary label");
        Check(Guide(confirmed,confirmed with{GroupId="same-on-second-sign"}).Road=="I-5","Repeated identical proven group became ambiguous");
        Check(Guide(confirmed with{Fields=fields with{IdentityConfidence="MEDIUM"}}).Cardinal is null,"Cardinal confirmed without a validated road");
        bool reject=false;try{SignGroupProof.Validate(Encode(confirmed with{Blockers=["UNCALIBRATED"]}),"gps");}catch(InvalidDataException){reject=true;}
        Check(reject,"Malformed HIGH evidence accepted");
        Check(SignGroupProof.Read(Encode(provisional)).Arrows[0].Link.Hits[0].Position.Z==5,"Hit coordinates lost in evidence serialization");
        var following=provisional with{EventIndex=1,SignUid="another",GroupId="following",LaneIds=["ppd"]};
        var repeated=SignGroupGuidance.Correlate([provisional with{Continuations=[chain]},following]);
        Check(repeated[0].Supports.Length==1&&!repeated[0].Supports[0].Verified&&repeated[0].Confidence!="HIGH","Provisional following panel promoted origin");
        Check(SignGroupGuidance.Correlate([provisional with{Continuations=[chain]},following with{LaneIds=["unrelated"]}])[0].Supports.Length==0,"Nearby unrelated lane supplied support");
        var upstream=confirmed with{Continuations=[chain with{PieceIds=["futureGps","futureOther"]}]};
        var support=confirmed with{EventIndex=1,SignUid="future",GroupId="futureNorth",LaneIds=["futureGps"]};
        var rejected=competing with{EventIndex=1,SignUid="future",GroupId="futureEast",Status="OTHER_EXIT_CANDIDATE",Confidence="LOW",LaneIds=["futureOther"],
            Calibration=new("model","variant","part","BOTTOM_CENTRE","synthetic-render-evidence","synthetic",new())};
        Check(SignGroupGuidance.Correlate([upstream,competing,support,rejected])[0].SelectionStatus=="FUTURE_DISCRIMINATES","A connected calibrated opposing group did not refine the name");
        Check(SignGroupGuidance.Correlate([upstream,competing,support,rejected with{Calibration=null}])[0].SelectionStatus!="FUTURE_DISCRIMINATES","Unverified competing exit refined the name");
        Check(SignGroupGuidance.Correlate([upstream,competing,support,rejected with{Status="GPS_COMPATIBLE_CANDIDATE"}])[0].SelectionStatus!="FUTURE_DISCRIMINATES","Two continuing names arbitrarily separated");
        RouteLanePiece Piece(string id,int seg,float z,string branch="gps")=>new(id,seg,"PPD_SAMPLED",id+":in",id+":out",branch,true,0,"","",[new(0,0,z),new(0,0,z+10)],[]){StartNodeUid=(ulong)seg+1,EndNodeUid=(ulong)seg+2};
        DecisionBranchSet Set(int seg,string branch)=>new(seg,seg,0,"",0,1,1,[0],true,[new(branch,1,true,[0],[],[],0,"THROUGH",[],0,0,"")],branch,"OK",null);
        var pieces=new[]{Piece("p0",0,0),Piece("p1",1,10,"next"),Piece("p2",2,20,"later")};
        var boundaries=LaneRouteGraph.Match([pieces[0]],[pieces[1]]).Concat(LaneRouteGraph.Match([pieces[1]],[pieces[2]])).ToArray();
        var graph=new LaneRouteGraph(pieces,boundaries,new(){{0,Set(0,"gps")},{1,Set(1,"next")},{2,Set(2,"later")}});
        Check(graph.TraceAhead("p0",0).Count==3,"Lookahead stops at the first future prefab");
        Check(graph.TraceAhead("p0",0,maxDecisions:2).Count==2,"Decision bound ignored");
        Check(graph.TraceAhead("p0",0,maxWorldM:5).Count==1,"World distance bound ignored");
        Check(graph.Trace("p0",0,lookahead:false).Status=="CURRENT_EXIT_ONLY","Current decision not independently traced");
        Check(LaneRouteGraph.Match([pieces[0]],[pieces[1] with{StartNodeUid=999}]).Count==0,"Geometries at different map nodes connected");
        var lane=new VehicleLaneGeometry("l",0,[new(0,0,0),new(100,0,0)]);
        var v=new LiveVehicle(new(50,0,0),0,10,false);
        Check(VehicleLaneLocator.Locate(v,0,[]).Status=="UNKNOWN","Empty lane set failed");
        Check(VehicleLaneLocator.Locate(v,0,[lane]).LaneId=="l","Truck centre not located");
        Check(VehicleLaneLocator.Locate(v,0,[lane,lane with{Id="r",Points=[new(0,0,4.5),new(100,0,4.5)]}]).LaneId=="l","Nearest lane not selected");
        Check(VehicleLaneLocator.Locate(v with{Position=new(50,0,2.25)},0,[lane,lane with{Id="r",Points=[new(0,0,4.5),new(100,0,4.5)]}]).Status=="BETWEEN_LANES","Truck between lanes assigned arbitrarily");
        Check(VehicleLaneLocator.Locate(v with{Position=new(50,4,0)},0,[lane]).Status=="UNKNOWN","Stacked carriageway matched");
        Check(VehicleLaneLocator.Locate(v with{HeadingDeg=180},0,[lane]).Status=="UNKNOWN","Wrong traffic direction matched");
        Check(VehicleLaneLocator.Locate(v,1,[lane]).Status=="UNKNOWN","Wrong GPS segment matched");
        Check(VehicleLaneLocator.Locate(v,0,[lane with{PhysicalLaneId="input0"},lane with{Id="otherBranch",PhysicalLaneId="input0"}]).LaneId=="input0","Two paths from one physical input counted as two lanes");
        var attached=SignGroupGuidance.Attach(new("r",[edge],[action]),[provisional],graph);
        var legacy=new LiveProof("road","I-10","HIGH","SIGN_BOARD","old-board","gps","older parser");
        var legacyAttached=SignGroupGuidance.Attach(new("r",[edge],[action with{Proofs=[legacy]}]),[provisional],graph);
        Check(legacyAttached.Actions[0].Proofs.Any(p=>p.Source=="SIGN_LEGACY_OBSERVATION"&&p.Value=="I-10"&&p.Confidence=="LOW"),"Legacy text dropped or old association trusted without validation");
        Check(attached.Actions[0].Proofs.Any(p=>p.Source=="SIGN_GROUP")&&attached.Actions[0].Instruction==action.Instruction&&attached.Actions[0].NavStationM==500,"Attachment modifies action or distance");
        LiveProtocol.Decode(LiveProtocol.Encode(new(1,"RouteSnapshot","roundtrip",0,0,1,attached)));Check(true,"Outer v1 contract roundtrip");
        Console.WriteLine($"PASS: {n} group-to-live, field isolation, topology, lookahead, lane location and wire assertions.");
    }
}
