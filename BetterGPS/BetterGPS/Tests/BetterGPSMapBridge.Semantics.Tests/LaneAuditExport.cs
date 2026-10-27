using System.Globalization;
using System.Numerics;
using System.Text.Json;
using System.Text.Json.Nodes;
using BetterGPSNavigation;
using TruckLib;
using TruckLib.Models;

internal static class LaneAuditExport
{
    public static void RefreshGroups(string auditPath,string routePath)
    {
        var options=new JsonSerializerOptions{PropertyNamingPolicy=JsonNamingPolicy.CamelCase,PropertyNameCaseInsensitive=true,WriteIndented=true};
        var root=JsonNode.Parse(File.ReadAllText(auditPath))!;
        var route=LiveProtocol.Decode(File.ReadAllBytes(routePath)).Route!;
        Vector3 V(LivePoint p)=>new((float)p.X,(float)p.Y,(float)p.Z);
        var corridor=new RouteCorridor(route.Edges.Select((e,i)=>new RouteCorridorEdge(i,e.SegmentIndex,"RECORDED_ROUTE",0,"",V(e.Start),V(e.End),
            e.WorldStartM,e.WorldEndM,e.NavStartM,e.NavEndM,0)).ToArray(),[]);
        var plates=root["events"]!.AsArray().SelectMany(e=>e!["plates"]!.AsArray()).Where(p=>p?["panel"] is not null).ToArray();
        var evidence=new Dictionary<string,SignContinuityEvidence>();
        foreach(var p in plates)
        {
            var panel=p!["panel"]!.Deserialize<ExtractedSignPanel>(options)!;
            var xyz=p["signPosition"]!.Deserialize<float[]>()!;var position=new Vector3(xyz[0],xyz[1],xyz[2]);
            var projection=corridor.Project(position);if(projection is null)continue;
            evidence[$"{panel.SignUid}:{panel.AreaName}"]=new(panel.SignUid,panel.AreaName,position,projection.StationM,
                projection.NavStationM,projection.LateralM,position.Y-projection.Position.Y,panel.SemanticGroups);
            p["panel"]=JsonSerializer.SerializeToNode(panel,options);
        }
        foreach(var p in plates)
        {
            var key=p!["uid"]!.GetValue<string>()+":"+p["areaName"]!.GetValue<string>();
            if(evidence.TryGetValue(key,out var source))p["continuity"]=JsonSerializer.SerializeToNode(
                source.Groups.Select(g=>SignContinuityResolver.Resolve(source,g,evidence.Values.ToArray())),options);
            if(root["laneGraph"] is not null)RefreshAssignments(p,options);
        }
        root["groupReplaySource"]="Recorded selected-route edges and source object rectangles; no user observation used in calculation.";
        File.WriteAllText(auditPath,root.ToJsonString(options));
        Console.WriteLine($"PASS: refreshed groups and continuity for {evidence.Count} distinct sign areas using recorded route geometry.");
    }
    private static void RefreshAssignments(JsonNode plate,JsonSerializerOptions options)
    {
        var panel=plate["panel"]!.Deserialize<ExtractedSignPanel>(options)!;var arrows=new List<GroupArrowEvidence>();
        foreach(var h in plate["hypotheses"]!.AsArray())
        {
            var link=h!["link"]!;var hits=new List<AuditLaneHit>();
            foreach(var hit in link["hits"]!.AsArray())
            {
                var pos=hit!["position"]!.Deserialize<float[]>()!;
                hits.Add(new(hit["laneOrdinal"]!.GetValue<int>(),hit["lateralM"]!.GetValue<double>(),hit["heightM"]!.GetValue<double>(),
                    new(pos[0],pos[1],pos[2]),hit["geometryStatus"]!.GetValue<string>(),hit["reachableExits"]!.Deserialize<string[]>()!)
                    {LaneId=hit["laneId"]?.GetValue<string>()??"",Chain=hit["chain"]?.Deserialize<LaneChain>(options)});
            }
            var obj=h["objectId"]!.GetValue<uint>();
            arrows.Add(new(obj,$"{h["part"]!.GetValue<string>()}:{obj}:{h["origin"]!.GetValue<string>()}",
                new(link["status"]!.GetValue<string>(),link["confidence"]!.GetValue<string>(),hits,
                    link["reachableExits"]!.Deserialize<string[]>()!,link["blockers"]!.Deserialize<string[]>()!)));
        }
        plate["groupLaneAssignments"]=JsonSerializer.SerializeToNode(panel.SemanticGroups.Select(g=>GroupLaneAssigner.Resolve(g,arrows)),options);
    }
    static float[] P(Vector3 p)=>[p.X,p.Y,p.Z];
    static double? N(string? value)=>double.TryParse(value,NumberStyles.Float,CultureInfo.InvariantCulture,out var n)&&double.IsFinite(n)?n:null;
    public static void Write(MapBridge bridge,IFileSystem fs,LiveRoute plan,string output,string panelsPath)
    {
        var panels=JsonSerializer.Deserialize<ExtractedSignPanel[]>(File.ReadAllText(panelsPath))??[];
        var evidence=new List<SignContinuityEvidence>();
        foreach(var panel in panels)
        {
            var sign=bridge.AuditSigns.FirstOrDefault(s=>$"{s.Uid:X16}"==panel.SignUid);if(sign is null)continue;
            var projection=bridge.AuditCorridor!.Project(sign.Position);if(projection is null)continue;
            evidence.Add(new(panel.SignUid,panel.AreaName,sign.Position,projection.StationM,projection.NavStationM,
                projection.LateralM,sign.Position.Y-projection.Position.Y,panel.SemanticGroups));
        }
        var models=new Dictionary<string,Model>();var events=new List<object>();
        foreach(var action in plan.Actions)
        {
            if(!bridge.NavigationAudit.AuditBranches.TryGetValue(action.EventIndex,out var set)||set.Branches.Count==0)continue;
            var boards=bridge.NavigationAudit.AuditBoards.Where(b=>b.EventIndex==action.EventIndex).ToArray();
            if(boards.Length==0)continue;
            var lanes=bridge.AuditLaneGraph!.LanesFor(set);
            var plates=new List<object>();
            foreach(var board in boards)
            {
                var panel=panels.FirstOrDefault(p=>p.SignUid==$"{board.SignUid:X16}"&&p.AreaName==board.AreaName);
                var sign=bridge.AuditSigns.First(s=>s.Uid==board.SignUid);
                var anchors=new List<(string Part,Vector3 Position,Quaternion Rotation,float Scale)>();
                string? modelError=null;
                try
                {
                    if(!models.TryGetValue(sign.ModelDesc,out var model))models[sign.ModelDesc]=model=Model.Open(sign.ModelDesc,fs);
                    foreach(var part in model.Parts)
                    foreach(var l in part.Locators.Where(l=>l.Name.ToString().Equals(board.AreaName,StringComparison.OrdinalIgnoreCase)))
                        anchors.Add((part.Name.ToString(),Vector3.Transform(l.Position,sign.Rotation)+sign.Position,
                            Quaternion.Normalize(sign.Rotation*l.Rotation),l.Scale));
                }catch(Exception ex){modelError=ex.GetType().Name+": "+ex.Message;}
                var blockers=new List<string>{"BOARD_RENDER_ORIGIN_AND_FRONT_FACE_UNCALIBRATED","ARROW_TEXTURE_NOT_VERIFIED"};
                if(anchors.Count!=1)blockers.Add("ACTIVE_LOCATOR_VARIANT_UNRESOLVED");
                if(sign.FollowRoadDir)blockers.Add("FOLLOW_ROAD_DIR_UNRESOLVED");
                if(panel is null||!panel.Reliable)blockers.Add("PANEL_EXTRACTION_INCOMPLETE");
                if(panel is not null&&!panel.SchemaValidated)blockers.Add("OVERRIDE_SCHEMA_INFERRED");
                if(panel?.GroupStatus=="MULTI_GROUP_UNRESOLVED")blockers.Add("TEXT_ARROW_GROUP_UNRESOLVED");
                if(panel?.BoardOffset.Any(x=>Math.Abs(x)>1e-6)==true)blockers.Add("BOARD_OFFSET_UNCALIBRATED");
                var hypotheses=new List<object>();
                var groupArrows=new List<GroupArrowEvidence>();
                foreach(var anchor in anchors)
                foreach(var obj in panel?.Objects.Where(o=>o.Visible&&o.Arrow=="DOWN_LANE")??[])
                {
                    var l=N(obj.Effective.GetValueOrDefault("coords_l"));var r=N(obj.Effective.GetValueOrDefault("coords_r"));
                    var b=N(obj.Effective.GetValueOrDefault("coords_b"));var t=N(obj.Effective.GetValueOrDefault("coords_t"));
                    if(l is null||r is null||b is null||t is null||r<=l||t<=b)continue;
                    // Board coords are asset coordinates, not assumed normalized fractions.
                    // Retain both origin hypotheses, never choose the one that happens to agree with GPS.
                    foreach(var origin in new[]{"LOCATOR_AS_BOTTOM_LEFT","LOCATOR_AS_BOTTOM_CENTRE"})
                    {
                        if(origin.EndsWith("CENTRE")&&panel!.WidthM is null)continue;
                        var x=(float)((l.Value+r.Value)/2-(origin.EndsWith("CENTRE")?(panel!.WidthM??0)/2:0));
                        var point=anchor.Position+Vector3.Transform(new Vector3(x,(float)b,0)*anchor.Scale,anchor.Rotation);
                        var link=PanelLaneLinker.Link(point,lanes,set.SelectedBranchId,blockers);
                        groupArrows.Add(new(obj.Id,$"{anchor.Part}:{obj.Id}:{origin}",link));
                        hypotheses.Add(new{part=anchor.Part,objectId=obj.Id,origin,point=P(point),link=new{
                            link.Status,link.Confidence,link.Blockers,link.ReachableExits,hits=link.Hits.Select(h=>new{
                                h.LaneOrdinal,h.LaneId,h.Chain,h.LateralM,h.HeightM,position=P(h.Position),h.GeometryStatus,h.ReachableExits})}});
                    }
                }
                if(hypotheses.Count==0)blockers.Add("NO_POSITIONED_DOWN_LANE_ARROW");
                plates.Add(new{board.BoardIndex,uid=$"{board.SignUid:X16}",board.AreaName,board.BoardPath,board.Region,
                    signPosition=P(sign.Position),sign.ModelDesc,sign.Variant,sign.FollowRoadDir,
                    panel,groupLaneAssignments=panel?.SemanticGroups.Select(g=>GroupLaneAssigner.Resolve(g,groupArrows)).ToArray()??[],
                    continuity=evidence.FirstOrDefault(e=>e.SignUid==panel?.SignUid&&e.AreaName==panel.AreaName) is {} source
                        ? source.Groups.Select(g=>SignContinuityResolver.Resolve(source,g,evidence)).ToArray():[],
                    anchors=anchors.Select(a=>new{a.Part,position=P(a.Position),a.Scale}),modelError,
                    hypotheses,blockers=blockers.Distinct(),semantics=new{board.BranchIdentity,board.Cardinal,board.Toward,
                        board.ExitNumbers,board.SemanticConfidence,board.ValidationNotes},
                    oldMatch=bridge.NavigationAudit.AuditMatches.FirstOrDefault(m=>m.BoardIndex==board.BoardIndex)});
            }
            events.Add(new{action.EventIndex,action.Instruction,action.NavStationM,selectedExit=set.SelectedBranchId,
                set.PrefabModel,set.PpdEntryIndex,set.PpdSelectedExitIndex,set.Status,set.Error,
                branches=set.Branches.Select(b=>new{b.BranchId,b.Selected,b.TargetPpdNodeIndex,b.Side,b.LaneMappings,
                    paths=b.ExactLanePaths.Select(p=>new{p.InputLaneOrdinal,p.OutputLaneOrdinal,p.BoundaryLaneId,p.CurveIds,points=p.Points.Select(P)})}),
                lanes=lanes.Select(l=>new{l.Ordinal,l.LaneId,l.Chain,l.GeometryStatus,l.PathBranchId,l.ReachableExits,points=l.Points.Select(P)}),plates});
        }
        var graph=bridge.AuditLaneGraph!;
        var data=new{version=2,plan.RouteId,units="SCS world metres; Y up; navigation metres separate",diagnosticWindow=new{lateralM=2.5,heightM=15},
            laneGraph=new{graph.LoadErrors,graph.Boundaries,pieces=graph.Pieces.Select(p=>new{
                p.Id,p.SegmentIndex,p.Kind,p.StartPort,p.EndPort,p.BranchId,p.Selected,p.Ordinal,p.LookToken,p.LookSource,p.Blockers,points=p.Points.Select(P)})},
            policy="Provisional associations only; no HIGH proof and no change to live instructions.",events};
        File.WriteAllText(Path.Combine(output,plan.RouteId+"_lanes.json"),JsonSerializer.Serialize(data,new JsonSerializerOptions{WriteIndented=true,PropertyNamingPolicy=JsonNamingPolicy.CamelCase}));
        Console.WriteLine($"Lane audit: {events.Count} decisions with plates; {plan.RouteId}_lanes.json");
    }

    internal static IReadOnlyList<AuditLane> BuildLanes(DecisionBranchSet set,RouteCorridor corridor)
    {
        var result=new List<AuditLane>();
        // Stop at the previous prefab. Propagation through another intersection would require an exact lane mapping.
        var incoming=corridor.Edges.Where(e=>e.SegmentIndex<set.SegmentIndex).Reverse().ToList();
        var road=new List<RouteCorridorEdge>();double length=0;
        foreach(var edge in incoming)
        {
            if(edge.ItemType!="Road"||length>900)break;
            road.Add(edge);length+=Vector3.Distance(edge.Start,edge.End);
        }
        road.Reverse();
        foreach(var group in set.Branches.SelectMany(b=>b.ExactLanePaths.Select(p=>(Branch:b.BranchId,Path:p))).GroupBy(x=>x.Path.InputLaneOrdinal))
        {
            var exits=group.Select(x=>x.Branch).Distinct().Order().ToArray();var first=group.First().Path.Points[0];
            if(road.Count>0)
            {
                var last=road[^1];var d=Vector3.Normalize(new(last.End.X-last.Start.X,0,last.End.Z-last.Start.Z));
                if(!float.IsFinite(d.X))continue;
                var right=new Vector3(-d.Z,0,d.X);var offset=Vector3.Dot(first-last.End,right);
                var pts=new List<Vector3>();
                foreach(var e in road)
                {
                    var tangent=new Vector3(e.End.X-e.Start.X,0,e.End.Z-e.Start.Z);if(tangent.LengthSquared()<1e-8)continue;
                    tangent=Vector3.Normalize(tangent);var side=new Vector3(-tangent.Z,0,tangent.X)*offset;
                    if(pts.Count==0)pts.Add(e.Start+side);pts.Add(e.End+side);
                }
                // A separate approximate path avoids hiding a junction gap inside exact PPD geometry.
                if(pts.Count>=2)result.Add(new(group.Key,"BOUNDARY_OFFSET_APPROX",pts,exits));
            }
            foreach(var item in group)result.Add(new(group.Key,"PPD_SAMPLED",item.Path.Points,exits){PathBranchId=item.Branch});
        }
        return result;
    }

    public static void Tests()
    {
        AuditLane Lane(int i,float x,string[] exits,string status="PPD_SAMPLED")=>new(i,status,[new(x,0,0),new(x,0,50)],exits);
        void Check(bool ok){if(!ok)throw new Exception("Lane association regression failed");}
        var lanes=new[]{Lane(0,0,["gps"]),Lane(1,4,["other"])};
        var selected=PanelLaneLinker.Link(new(0,7,20),lanes,"gps",["UNCALIBRATED"]);
        Check(selected.Status=="SELECTED_EXIT_CANDIDATE"&&selected.Confidence=="LOW"&&selected.Blockers.Contains("UNCALIBRATED"));
        Check(PanelLaneLinker.Link(new(4,7,20),lanes,"gps",[]).Status=="OTHER_EXIT_CANDIDATE");
        Check(PanelLaneLinker.Link(new(0,7,20),[Lane(0,0,["gps","other"])],"gps",[]).Status=="AMBIGUOUS_EXITS");
        Check(PanelLaneLinker.Link(new(0,-1,20),lanes,"gps",[]).Hits.Count==0);
        Check(PanelLaneLinker.Link(new(0,16,20),lanes,"gps",[]).Hits.Count==0);
        Check(PanelLaneLinker.Link(new(0,7,-100),lanes,"gps",[]).Hits.Count==0);
        Check(PanelLaneLinker.Link(new(float.NaN,7,20),lanes,"gps",[]).Status=="INVALID_GEOMETRY");
        Check(PanelLaneLinker.Link(new(0,7,20),[Lane(0,0,["gps"],"BOUNDARY_OFFSET_APPROX")],"gps",[]).Blockers.Contains("UPSTREAM_LANE_GEOMETRY_APPROXIMATE"));
        Console.WriteLine("PASS: 8 lane association regressions (alternatives, ambiguity, height, endpoints, invalid data, provisional policy).");
    }
}
