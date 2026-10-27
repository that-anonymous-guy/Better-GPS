using System.Globalization;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.RegularExpressions;
using TruckLib;
using TruckLib.Models;

namespace BetterGPSNavigation;

// A versioned payload carried by an existing LiveProof. The outer v1 wire contract stays unchanged.
internal sealed record SignArrowPlacement(uint ObjectId,string Part,string Origin,float[] Position,AuditLink Link)
{
    public BoardPlacementTrace? Placement {get;init;}
}
internal sealed record SignGroupSupport(string GroupId,string SignUid,int EventIndex,string Status,bool Verified);
internal sealed record SignCalibrationEvidence(string ModelPath,string Variant,string Part,string Origin,
    string EvidenceFile,string EvidenceSha256,Dictionary<string,string> AssetSha256)
{
    public float[] LocalFrontNormal {get;init;}=[];
    public Dictionary<string,string> TextureSha256 {get;init;}=[];
    public BoardDrawingTransform? DrawingTransform {get;init;}
}
internal sealed record SignGroupProof(int Version,int EventIndex,string GroupId,string SignUid,string AreaName,
    string Road,string Cardinal,string Destination,string Status,string Confidence,SignFieldValidation Fields,
    string SelectedBranchId,SignArrowPlacement[] Arrows,string[] LaneIds,string[] ReachableExits,
    LaneChain[] Continuations,string[] Blockers)
{
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public string? Exit {get;init;}
    public SignCalibrationEvidence? Calibration {get;init;}
    public BoardModelEvidence? BoardGeometry {get;init;}
    public SupportVariantEvidence? SupportGeometry {get;init;}
    public double? BoardWidthM {get;init;}
    public double? BoardHeightM {get;init;}
    public SignPlacementCalibration? CalibrationDraft {get;init;}
    public SignGroupSupport[] Supports {get;init;}=[];
    public string[] RepeatedCompetingGroupIds {get;init;}=[];
    public string SelectionStatus {get;init;}="CURRENT_ASSOCIATION_ONLY";
    internal static readonly JsonSerializerOptions Json=new(LiveProtocol.Json){IncludeFields=true};
    public static SignGroupProof Read(LiveProof p) => JsonSerializer.Deserialize<SignGroupProof>(p.Value,Json)
        ?? throw new InvalidDataException("Empty sign group evidence.");
    public static void Validate(LiveProof p,string branch)
    {
        var g=Read(p);
        if(g.Version!=1 || g.GroupId!=p.SourceId || g.SelectedBranchId!=branch || p.BranchId!=branch ||
            string.IsNullOrWhiteSpace(g.GroupId) || g.Arrows is null || g.LaneIds is null || g.Blockers is null ||
            g.ReachableExits is null || g.Continuations is null || g.Fields is null || g.Confidence!=p.Confidence ||
            g.Arrows.Any(a=>a.Position.Length!=3 || a.Position.Any(x=>!float.IsFinite(x))) ||
            (g.Exit is not null&&!Regex.IsMatch(g.Exit,@"^\d{1,4}[A-Z]?$")) ||
            (g.Confidence=="HIGH" && (g.Status!="SELECTED_EXIT_CONFIRMED" || g.Blockers.Length!=0 ||
                g.Arrows.Length==0 || g.LaneIds.Length==0 || !g.ReachableExits.SequenceEqual(new[]{branch}))))
            throw new InvalidDataException("Invalid sign group evidence.");
    }
}

// Profiles represent external render/editor measurements, never a choice made by GPS agreement.
internal sealed record SignPlacementCalibration(string ModelPath,string Variant,string AreaName,string BoardPath,
    string Part,string Origin,Dictionary<string,string> AssetSha256,string EvidenceFile,string EvidenceSha256,
    bool FrontFaceVerified,bool LayoutVerified)
{
    public Dictionary<string,string> TextureSha256 {get;init;}=[];
    public float[] LocalFrontNormal {get;init;}=[];
    public BoardDrawingTransform? DrawingTransform {get;init;}
    public float[] ArrowTipUv {get;init;}=[];
    public string ResolvedSupportVariant {get;init;}="";
    public double MeasurementToleranceM {get;init;}=.05;
}

internal sealed class SignGroupGuidance(IFileSystem fs,IReadOnlyList<ParsedSign> signs,string? calibrationPath=null,Func<string,string?>? textureHash=null)
{
    private readonly Dictionary<string,Model> models=new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string,string?> hashes=new(StringComparer.OrdinalIgnoreCase);
    private readonly BoardModelResolver boardModels=new(fs);
    private readonly SignTemplateResolver templates=new(fs,signs,true);
    private readonly SignPlacementCalibration[] calibrations=calibrationPath is null?[]:
        JsonSerializer.Deserialize<SignPlacementCalibration[]>(File.ReadAllText(calibrationPath),LiveProtocol.Json)
        ?? throw new InvalidDataException("Empty calibration file.");
    private string? Hash(string path)
    {
        if(hashes.TryGetValue(path,out var h))return h;
        try {return hashes[path]=AssetFileHash.Read(fs,path);}
        catch(IOException){return hashes[path]=null;}
    }
    private bool Verified(SignPlacementCalibration c,ParsedSign sign,ExtractedSignPanel panel,BoardModelEvidence board)
    {
        if(!c.FrontFaceVerified || !c.LayoutVerified || c.ModelPath!=sign.ModelDesc || c.Variant!=sign.Variant ||
            c.AreaName!=panel.AreaName || c.BoardPath!=panel.BoardPath ||
            c.Origin!="MODEL_SURFACE" || c.AssetSha256 is null || board.Surfaces.Length!=1 || !BoardPlacementResolver.Valid(c.DrawingTransform) ||
            !BoardModelResolver.Valid(c.ArrowTipUv,2) || c.ArrowTipUv.Any(x=>x<0||x>1) ||
            c.TextureSha256 is null || c.LocalFrontNormal is null || c.LocalFrontNormal.Length!=3 || c.LocalFrontNormal.Any(x=>!float.IsFinite(x)) ||
            Math.Abs(new Vector3(c.LocalFrontNormal[0],c.LocalFrontNormal[1],c.LocalFrontNormal[2]).Length()-1)>.01)return false;
        if(Vector3.Dot(BoardModelResolver.V(c.LocalFrontNormal),BoardModelResolver.V(board.Surfaces[0].Normal))<.999)return false;
        // Model geometry and board contents must be pinned. Atlas SII alone does not identify an arrow texture.
        var required=new List<string>{sign.ModelDesc,Path.ChangeExtension(sign.ModelDesc,"pmg"),panel.BoardPath};
        required.AddRange(board.AssetSha256.Keys);
        var supportVariant=boardModels.ResolveSupport(sign);
        required.AddRange(supportVariant.AssetSha256.Keys);
        if(supportVariant.Variant!=c.ResolvedSupportVariant)return false;
        if(!templates.TryGet(sign.Template,out var template)||template is not {Status:"OK"})return false;
        required.Add(template.TemplatePath);
        required.AddRange(TruckLib.Sii.SiiFile.Open(template.TemplatePath,fs).Includes??[]);
        if(!models.TryGetValue(sign.ModelDesc,out var support))models[sign.ModelDesc]=support=ModelDocumentCache.Open(sign.ModelDesc,fs);
        try {
            if(!BoardModelResolver.ActiveParts(support,c.ResolvedSupportVariant).Any(p=>p.Name.ToString()==c.Part&&p.Locators.Any(l=>l.Name.ToString()==panel.AreaName)))return false;
            if(sign.Variant.Length>0&&sign.Variant!=c.ResolvedSupportVariant)return false;
        }catch(InvalidDataException){return false;}
        foreach(var arrow in panel.Objects.Where(o=>o.Visible&&o.Arrow.Length>0))
        {
            var atlas=arrow.Effective.GetValueOrDefault("atlas_path","");required.Add(atlas);
            if(atlas!="/def/sign/atlas/down_arrow.sii")return false; // First supported, independently calibratable class.
            const string mat="/material/sign/atlas/down_arrow.mat",tobj="/material/sign/atlas/down_arrow.tobj";
            required.AddRange(new[]{mat,tobj});
            var material=fs.ReadAllText(mat);
            var texture=Regex.Match(material,"texture\\[0\\]\\s*:\\s*\"([^\"]+)\"");
            if(!texture.Success||texture.Groups[1].Value!=tobj||textureHash is null||
                !c.TextureSha256.TryGetValue(tobj,out var expected)||!Regex.IsMatch(expected??"",@"^[0-9A-Fa-f]{64}$")||
                !string.Equals(textureHash(tobj),expected,StringComparison.OrdinalIgnoreCase))return false;
        }
        if(required.Any(p=>!c.AssetSha256.ContainsKey(p)))return false;
        if(c.AssetSha256.Any(a=>!Regex.IsMatch(a.Value??"",@"^[0-9A-Fa-f]{64}$") || !a.Key.StartsWith('/') || a.Key.Split('/').Any(s=>s is "." or "..") ||
            !string.Equals(Hash(a.Key),a.Value,StringComparison.OrdinalIgnoreCase)))return false;
        if(calibrationPath is null || string.IsNullOrWhiteSpace(c.EvidenceFile) || Path.IsPathRooted(c.EvidenceFile))return false;
        var root=Path.GetFullPath(Path.GetDirectoryName(calibrationPath)!)+Path.DirectorySeparatorChar;
        var file=Path.GetFullPath(Path.Combine(root,c.EvidenceFile));
        if(!file.StartsWith(root,StringComparison.OrdinalIgnoreCase) || !File.Exists(file))return false;
        using(var evidence=File.OpenRead(file))
            if(!Convert.ToHexString(SHA256.HashData(evidence)).Equals(c.EvidenceSha256,StringComparison.OrdinalIgnoreCase))return false;
        try {
            var measurements=JsonSerializer.Deserialize<BoardMeasurements>(File.ReadAllText(file),LiveProtocol.Json);
            if(measurements?.SceneSha256 is not {Count:>0})return false;
            foreach(var scene in measurements.SceneSha256)
            {
                if(string.IsNullOrWhiteSpace(scene.Key)||Path.IsPathRooted(scene.Key)||!Regex.IsMatch(scene.Value??"",@"^[0-9A-Fa-f]{64}$"))return false;
                var scenePath=Path.GetFullPath(Path.Combine(root,scene.Key));
                if(!scenePath.StartsWith(root,StringComparison.OrdinalIgnoreCase)||!File.Exists(scenePath))return false;
                using var stream=File.OpenRead(scenePath);
                if(!Convert.ToHexString(SHA256.HashData(stream)).Equals(scene.Value,StringComparison.OrdinalIgnoreCase))return false;
            }
            return measurements is not null && measurements.ModelPath==board.ModelPath && measurements.Variant==board.Variant &&
                measurements.AreaName==panel.AreaName && measurements.ArrowTipUv.SequenceEqual(c.ArrowTipUv) &&
                panel.WidthM is {} w && panel.HeightM is {} h &&
                BoardPlacementResolver.ValidateMeasurements(measurements,c.DrawingTransform!,board,w,h,c.MeasurementToleranceM,out _);
        }catch(JsonException){return false;}
    }

    private SignPlacementCalibration? Draft(ParsedSign sign,ExtractedSignPanel panel,BoardModelEvidence board,SupportVariantEvidence support,string part)
    {
        try {
            if(board.Surfaces.Length!=1||part.Length==0||!templates.TryGet(sign.Template,out var template)||template is not {Status:"OK"})return null;
            var paths=board.AssetSha256.Keys.Concat(support.AssetSha256.Keys)
                .Concat(new[]{sign.ModelDesc,Path.ChangeExtension(sign.ModelDesc,"pmg"),template.TemplatePath})
                .Concat(template.Includes)
                .Concat(panel.Objects.Where(o=>o.Visible&&o.Arrow.Length>0).Select(o=>o.Effective.GetValueOrDefault("atlas_path","")))
                .Concat(new[]{"/material/sign/atlas/down_arrow.mat","/material/sign/atlas/down_arrow.tobj"}).Distinct().ToArray();
            var pinned=new Dictionary<string,string>(StringComparer.Ordinal);
            foreach(var path in paths){var hash=Hash(path);if(hash is null)return null;pinned[path]=hash;}
            var tex=textureHash?.Invoke("/material/sign/atlas/down_arrow.tobj");
            return new(sign.ModelDesc,sign.Variant,panel.AreaName,panel.BoardPath,part,"MODEL_SURFACE",pinned,"","",false,false)
            {ResolvedSupportVariant=support.Variant,LocalFrontNormal=board.Surfaces[0].Normal,
                TextureSha256=tex is null?[]:new(){["/material/sign/atlas/down_arrow.tobj"]=tex}};
        }catch(Exception e) when(e is IOException or InvalidDataException or ArgumentException){return null;}
    }
    internal void PrepareAssets(ParsedSign sign,ExtractedSignPanel panel)
    {
        if(!panel.SemanticGroups.Any(g=>IsNavigationGroup(g,panel)))return;
        var board=boardModels.Resolve(panel);var support=boardModels.ResolveSupport(sign);
        // Populate reusable metadata and hashes, never a route-specific placement.
        _=Draft(sign,panel,board,support,"CACHE_PREPARATION");
    }

    public static SignFieldValidation ValidateFields(SignSemanticGroup group,ExtractedSignPanel panel,Func<string,bool> known)
    {
        var objects=panel.Objects.Where(o=>group.ObjectIds.Contains(o.Id)&&o.Visible).ToArray();
        var road=RoadIdentityRules.Valid(group.Kind,group.Road)&&objects.Any(o=>o.Reliable&&o.SchemaStatus=="SUPPORTED"&&
            (group.Kind=="StreetName"?SignRoadIdentityResolver.NormalizeText(o.Text).Equals(group.Road,StringComparison.OrdinalIgnoreCase):
            o.Road==group.Road||SignTextClassifier.SupportsIdentity(SignRoadIdentityResolver.NormalizeText(o.Text),group.Road)))?"HIGH":group.Road.Length>0?"MEDIUM":"NONE";
        var cardinal=group.Cardinal.Length==0?"NONE":road=="HIGH"&&objects.Any(o=>o.Reliable&&o.SchemaStatus=="SUPPORTED"&&
            SignTextClassifier.ExplicitCardinals(SignRoadIdentityResolver.NormalizeText(o.Text)).Contains(group.Cardinal))?"HIGH":"MEDIUM";
        var destination=group.Destination.Length==0?"NONE":known(group.Destination)&&SignTextClassifier.IsDestinationCandidate(group.Destination)&&
            objects.Any(o=>o.Reliable&&o.SchemaStatus=="SUPPORTED"&&SignRoadIdentityResolver.NormalizeText(o.Text).Equals(group.Destination,StringComparison.OrdinalIgnoreCase))?"HIGH":"MEDIUM";
        return new(road,cardinal,destination,ExplicitExit(group,panel) is null?"NONE":"HIGH",destination=="HIGH"?group.Destination:"","GROUP_SCOPED_OBJECT_EVIDENCE");
    }
    internal static bool ReadableGroupRoad(SignSemanticGroup group,ExtractedSignPanel panel)=>RoadIdentityRules.Valid(group.Kind,group.Road)&&
        (group.Kind!="StreetName"||group.Destination.Length==0)&&
        panel.Objects.Any(o=>group.ObjectIds.Contains(o.Id)&&o.Visible&&o.Reliable&&
            (group.Kind=="StreetName"?SignRoadIdentityResolver.NormalizeText(o.Text).Equals(group.Road,StringComparison.OrdinalIgnoreCase):
            o.Road==group.Road||SignTextClassifier.SupportsIdentity(SignRoadIdentityResolver.NormalizeText(o.Text),group.Road)));
    internal static string? ExplicitExit(SignSemanticGroup group,ExtractedSignPanel panel)
    {
        var exits=panel.Objects.Where(o=>group.ObjectIds.Contains(o.Id)&&o.Visible&&o.Reliable&&o.SchemaStatus=="SUPPORTED"&&o.Kind=="sign_template_text")
            .Select(o=>Regex.Match(SignRoadIdentityResolver.NormalizeText(o.Text),@"^EXIT\s+(\d{1,4}[A-Z]?)$",RegexOptions.IgnoreCase))
            .Where(m=>m.Success).Select(m=>m.Groups[1].Value.ToUpperInvariant()).Distinct().ToArray();
        return exits.Length==1?exits[0]:null;
    }

    internal static SignGroupProof Evaluate(int eventIndex,SignSemanticGroup group,ExtractedSignPanel panel,
        string selected,SignArrowPlacement[] arrows,bool calibrated,SignFieldValidation fields,LaneChain[] continuations,
        IEnumerable<string> placementBlockers)
    {
        // A missing optional cardinal/destination does not invalidate an otherwise identifiable group.
        var source=panel.Objects.Where(o=>o.Visible&&group.ObjectIds.Contains(o.Id)).ToArray();
        if(IsIdentityObservation(group,panel))
        {
            // Reading an identifier does not require a lane arrow. Assigning it to a road is a separate operation.
            var readBlocks=group.Blockers.Where(b=>b is "GROUP_OBJECT_OR_SCHEMA_UNVERIFIED" or "OBJECT_CROSSES_DIVIDER" or "OBJECT_RECT_MISSING" or "ROAD_MISSING_OR_MULTIPLE")
                .Append("ROAD_SEGMENT_NOT_ASSOCIATED").Distinct().Order().ToArray();
            return new(1,eventIndex,group.Id,panel.SignUid,panel.AreaName,group.Road,group.Cardinal,group.Destination,
                "ROAD_IDENTITY_UNASSIGNED","LOW",fields,selected,[],[],[],[],readBlocks){Exit=ExplicitExit(group,panel)};
        }
        var structural=group.Blockers.Where(b=>b!="GROUP_LAYOUT_CANDIDATE_NOT_RENDER_VERIFIED" &&
            !(b=="CARDINAL_MISSING_OR_MULTIPLE"&&source.SelectMany(o=>SignTextClassifier.ExplicitCardinals(SignRoadIdentityResolver.NormalizeText(o.Text))).Distinct().Count()==0) &&
            !(b=="DESTINATION_MISSING_OR_MULTIPLE"&&source.All(o=>!SignTextClassifier.IsDestinationCandidate(SignRoadIdentityResolver.NormalizeText(o.Text))))).ToArray();
        var blocks=structural.Concat(placementBlockers).ToHashSet(StringComparer.Ordinal);
        if(!calibrated)blocks.Add("PLACEMENT_NOT_RENDER_VERIFIED");
        foreach(var b in arrows.SelectMany(a=>a.Link.Blockers))blocks.Add(b);
        if(group.ArrowObjectIds.Any(id=>!arrows.Any(a=>a.ObjectId==id)))blocks.Add("ARROW_PLACEMENT_MISSING");
        var exits=arrows.SelectMany(a=>a.Link.ReachableExits).Distinct().Order().ToArray();
        var outcomes=arrows.Select(a=>a.Link.Status).Distinct().ToArray();
        var usable=group.Road.Length>0&&!structural.Any(b=>b is "ROAD_MISSING_OR_MULTIPLE" or "OBJECT_CROSSES_DIVIDER" or "OBJECT_RECT_MISSING" or "ARROW_ROLES_CONFLICT");
        var status=!usable?"GROUP_UNRESOLVED":arrows.Length==0?"NO_ARROW_GEOMETRY":
            outcomes.Contains("SELECTED_EXIT_CANDIDATE")&&outcomes.Contains("OTHER_EXIT_CANDIDATE")?"AMBIGUOUS_PLACEMENT":
            outcomes.All(s=>s=="SELECTED_EXIT_CANDIDATE")?"GPS_COMPATIBLE_CANDIDATE":
            outcomes.All(s=>s=="OTHER_EXIT_CANDIDATE")?"OTHER_EXIT_CANDIDATE":
            outcomes.Contains("AMBIGUOUS_EXITS")?"AMBIGUOUS_EXITS":"CHAIN_BLOCKED";
        bool proven=status=="GPS_COMPATIBLE_CANDIDATE"&&calibrated&&blocks.Count==0&&exits.SequenceEqual(new[]{selected});
        if(proven)status="SELECTED_EXIT_CONFIRMED";
        return new(1,eventIndex,group.Id,panel.SignUid,panel.AreaName,group.Road,group.Cardinal,group.Destination,status,
            proven?"HIGH":arrows.Length>0?"LOW":"NONE",fields,selected,arrows,
            arrows.SelectMany(a=>a.Link.Hits).Select(h=>h.LaneId).Where(s=>s.Length>0).Distinct().Order().ToArray(),
            exits,continuations,blocks.Order().ToArray()){Exit=ExplicitExit(group,panel)};
    }

    public SignGroupProof[] Resolve(NavigationPlanV11 audit,SignPanelExtractor extractor,LaneRouteGraph graph,Func<string,bool> known)
    {
        var result=new List<SignGroupProof>();
        foreach(var ev in audit.AuditBranches.OrderBy(k=>k.Key))
        {
            var set=ev.Value;var lanes=graph.CurrentLanesFor(set);
            foreach(var board in audit.AuditBoards.Where(b=>b.EventIndex==ev.Key))
            {
                var sign=signs.First(s=>s.Uid==board.SignUid);
                var panel=extractor.Panels.Single(p=>p.SignUid==$"{sign.Uid:X16}"&&p.AreaName==board.AreaName);
                foreach(var identity in panel.SemanticGroups.Where(g=>IsNavigationGroup(g,panel)&&IsIdentityObservation(g,panel)))
                    result.Add(Evaluate(ev.Key,identity,panel,set.SelectedBranchId,[],false,ValidateFields(identity,panel,known),[],[])
                        with{BoardWidthM=panel.WidthM,BoardHeightM=panel.HeightM});
                var anchors=new List<(string Part,Vector3 Position,Quaternion Rotation,float Scale)>();
                var placementBlocks=new List<string>();
                var geometry=boardModels.Resolve(panel);
                var supportGeometry=boardModels.ResolveSupport(sign);
                try {
                    if(!models.TryGetValue(sign.ModelDesc,out var model))models[sign.ModelDesc]=model=ModelDocumentCache.Open(sign.ModelDesc,fs);
                    var profiles=calibrations.Where(c=>Verified(c,sign,panel,geometry)).ToArray();
                    if(profiles.Length>1)placementBlocks.Add("MULTIPLE_CALIBRATIONS");
                    var profile=profiles.Length==1?profiles[0]:null;
                    var active=model.Parts.ToArray();
                    if(profile is not null)active=BoardModelResolver.ActiveParts(model,profile.ResolvedSupportVariant);
                    else if(supportGeometry.Variant.Length>0)
                    {
                        try{active=BoardModelResolver.ActiveParts(model,supportGeometry.Variant);}
                        catch(InvalidDataException){active=[];placementBlocks.Add("SUPPORT_VARIANT_UNSUPPORTED");}
                        if(supportGeometry.Status=="DEFINITION_DEFAULT")placementBlocks.Add("SUPPORT_DEFAULT_NOT_RENDER_VERIFIED");
                    }
                    else placementBlocks.Add("ACTIVE_LOCATOR_VARIANT_UNRESOLVED");
                    foreach(var part in active.Where(p=>profile is null || p.Name.ToString()==profile.Part))
                    foreach(var l in part.Locators.Where(l=>l.Name.ToString().Equals(board.AreaName,StringComparison.OrdinalIgnoreCase)))
                        anchors.Add((part.Name.ToString(),Vector3.Transform(l.Position,sign.Rotation)+sign.Position,
                            Quaternion.Normalize(sign.Rotation*l.Rotation),l.Scale));
                    if(sign.FollowRoadDir)placementBlocks.Add("FOLLOW_ROAD_DIR_UNRESOLVED");
                    if(panel.BoardOffset.Any(x=>Math.Abs(x)>1e-6))placementBlocks.Add("BOARD_OFFSET_UNCALIBRATED");
                    if(anchors.Count!=1)placementBlocks.Add("ACTIVE_LOCATOR_VARIANT_UNRESOLVED");
                    if(profile is null)placementBlocks.AddRange(geometry.Blockers.Concat(new[]{"ARROW_TIP_NOT_MEASURED"}));
                    if(anchors.Any(a=>!RoadLaneGeometryResolver.Finite(a.Position)||!float.IsFinite(a.Scale)||a.Scale<=0))
                        placementBlocks.Add("INVALID_LOCATOR_TRANSFORM");
                    var draft=anchors.Count==1?Draft(sign,panel,geometry,supportGeometry,anchors[0].Part):null;
                    foreach(var group in panel.SemanticGroups.Where(g=>IsNavigationGroup(g,panel)&&!IsIdentityObservation(g,panel)))
                    {
                        var arrows=new List<SignArrowPlacement>();
                        foreach(var anchor in anchors)
                        foreach(var obj in panel.Objects.Where(o=>o.Visible&&group.ArrowObjectIds.Contains(o.Id)&&o.Arrow=="DOWN_LANE"))
                        foreach(var transform in profile is null?BoardPlacementResolver.Hypotheses(geometry):new[]{profile.DrawingTransform!})
                        {
                            double N(string key)=>double.TryParse(obj.Effective.GetValueOrDefault(key),NumberStyles.Float,CultureInfo.InvariantCulture,out var n)?n:double.NaN;
                            var l=N("coords_l");var r=N("coords_r");var b=N("coords_b");var t=N("coords_t");
                            if(!double.IsFinite(l+r+b+t)||r<=l||t<=b || panel.WidthM is null || panel.HeightM is null)continue;
                            var tip=profile?.ArrowTipUv??new float[]{.5f,0};
                            bool Flip(string key)=>(bool.TryParse(obj.Effective.GetValueOrDefault(key),out var flag)&&flag) || N(key)==1;
                            var u=Flip("hflip")?1-tip[0]:tip[0];var v=Flip("vflip")?1-tip[1]:tip[1];
                            var trace=BoardPlacementResolver.Place(new((float)(l+(r-l)*u),(float)(b+(t-b)*v)),transform,geometry,
                                anchor.Position,anchor.Rotation,anchor.Scale,panel.WidthM.Value,panel.HeightM.Value);
                            var position=BoardModelResolver.V(trace.WorldPoint);
                            if(!RoadLaneGeometryResolver.Finite(position))continue;
                            var link=PanelLaneLinker.Link(position,lanes,set.SelectedBranchId,[]);
                            if(profile is not null)
                            {
                                var face=BoardModelResolver.V(trace.WorldNormal);
                                bool Facing(AuditLaneHit hit)
                                {
                                    var lane=lanes.First(l=>l.LaneId==hit.LaneId);
                                    var segment=lane.Points.Zip(lane.Points.Skip(1),(a,b)=>(A:a,B:b))
                                        .OrderBy(s=>Vector3.Distance((s.A+s.B)/2,hit.Position)).First();
                                    return Vector3.Dot(Vector3.Normalize(segment.B-segment.A),face)<-.5f;
                                }
                                if(link.Hits.Any(h=>!Facing(h)))link=link with{Blockers=link.Blockers.Concat(new[]{"FRONT_FACE_NOT_APPROACH"}).ToArray()};
                            }
                            arrows.Add(new(obj.Id,anchor.Part,trace.Hypothesis,[position.X,position.Y,position.Z],link){Placement=trace});
                        }
                        var ahead=arrows.SelectMany(a=>a.Link.Hits).Select(h=>h.LaneId).Distinct().SelectMany(id=>graph.TraceAhead(id,set.SegmentIndex)).ToArray();
                        result.Add(Evaluate(ev.Key,group,panel,set.SelectedBranchId,arrows.ToArray(),profile is not null,
                            ValidateFields(group,panel,known),ahead,placementBlocks) with {CalibrationDraft=draft,BoardWidthM=panel.WidthM,BoardHeightM=panel.HeightM,BoardGeometry=geometry,SupportGeometry=supportGeometry,Calibration=profile is null?null:
                                new(profile.ModelPath,profile.Variant,profile.Part,profile.Origin,profile.EvidenceFile,profile.EvidenceSha256,profile.AssetSha256)
                                    {LocalFrontNormal=profile.LocalFrontNormal,TextureSha256=profile.TextureSha256,DrawingTransform=profile.DrawingTransform}});
                    }
                } catch(Exception e) when(e is IOException or InvalidDataException or ArgumentException) {
                    foreach(var group in panel.SemanticGroups.Where(g=>IsNavigationGroup(g,panel)&&!IsIdentityObservation(g,panel)))result.Add(Evaluate(ev.Key,group,panel,set.SelectedBranchId,[],false,
                        ValidateFields(group,panel,known),[],["MODEL_ERROR:"+e.GetType().Name]) with{BoardWidthM=panel.WidthM,BoardHeightM=panel.HeightM,BoardGeometry=geometry,SupportGeometry=supportGeometry});
                }
            }
        }
        return Correlate(result.OrderBy(g=>g.EventIndex).ThenBy(g=>g.GroupId,StringComparer.Ordinal).ToArray());
    }

    private static bool IsNavigationGroup(SignSemanticGroup group,ExtractedSignPanel panel)=>group.Road.Length>0 ||
        panel.Objects.Any(o=>group.ObjectIds.Contains(o.Id)&&o.Visible&&(o.Road.Length>0||o.Arrow.Length>0));
    private static bool IsIdentityObservation(SignSemanticGroup group,ExtractedSignPanel panel)=>group.Road.Length>0&&group.ArrowObjectIds.Count==0&&
        panel.Objects.Where(o=>o.Visible&&group.ObjectIds.Contains(o.Id)).All(o=>o.Arrow.Length==0);

    internal static SignGroupProof[] Correlate(SignGroupProof[] groups)
    {
        static bool Same(SignGroupProof a,SignGroupProof b)=>a.Road==b.Road&&a.Cardinal==b.Cardinal&&a.Destination==b.Destination;
        return groups.Select(g=>
        {
            var pieces=g.Continuations.SelectMany(c=>c.PieceIds).ToHashSet(StringComparer.Ordinal);
            var later=groups.Where(n=>n.EventIndex>g.EventIndex&&n.SignUid!=g.SignUid&&n.LaneIds.Any(pieces.Contains)).ToArray();
            var supports=later.Where(n=>Same(g,n)&&n.Status is "SELECTED_EXIT_CONFIRMED" or "GPS_COMPATIBLE_CANDIDATE")
                .GroupBy(n=>n.SignUid).Select(s=>s.OrderBy(n=>n.EventIndex).First()).Select(n=>new SignGroupSupport(n.GroupId,n.SignUid,n.EventIndex,n.Status,n.Confidence=="HIGH")).ToArray();
            var rivals=groups.Where(n=>n.EventIndex==g.EventIndex&&n.SignUid==g.SignUid&&n.AreaName==g.AreaName&&!Same(n,g)).ToArray();
            var repeated=rivals.Where(r=>supports.Count(s=>later.Any(n=>n.SignUid==s.SignUid&&Same(r,n)))>=2).Select(r=>r.GroupId).ToArray();
            // Repetition itself never selects a name. A later calibrated opposing group is required for every rival.
            bool discriminates=g.Confidence=="HIGH"&&rivals.Length>0&&supports.Any(s=>s.Verified&&rivals.All(r=>later.Any(n=>
                n.EventIndex==s.EventIndex&&Same(r,n)&&n.Status=="OTHER_EXIT_CANDIDATE"&&n.Calibration is not null&&n.Blockers.Length==0)));
            return g with {Supports=supports,RepeatedCompetingGroupIds=repeated,SelectionStatus=discriminates?"FUTURE_DISCRIMINATES":
                repeated.Length>0?"REPEATED_BUT_NOT_DISCRIMINATING":supports.Length>0?"SUPPORTED_AHEAD":"CURRENT_ASSOCIATION_ONLY"};
        }).ToArray();
    }

    public static LiveRoute Attach(LiveRoute route,SignGroupProof[] groups,LaneRouteGraph graph)
    {
        return route with {Actions=route.Actions.Select(a=>
        {
            // Preserve older parsed evidence as observations; its route association has not been revalidated by this pipeline.
            var proofs=a.Proofs.Select(p=>p.Source=="SIGN_BOARD"?
                p with{Source="SIGN_LEGACY_OBSERVATION",Confidence="LOW",Detail="Original association not revalidated; "+p.Detail}:p).ToList();
            foreach(var g in groups.Where(g=>g.EventIndex==a.EventIndex))
            {
                proofs.Add(new("group",JsonSerializer.Serialize(g,SignGroupProof.Json),g.Confidence,"SIGN_GROUP",g.GroupId,a.BranchId,string.Join(';',g.Blockers)));
                foreach(var (field,value,confidence) in new[]{("road",g.Road,g.Fields.IdentityConfidence),
                    ("cardinal",g.Cardinal,g.Fields.CardinalConfidence),("destination",g.Destination,g.Fields.DestinationConfidence),
                    ("exit",g.Exit??"",g.Fields.ExitConfidence)})
                    if(value.Length>0)proofs.Add(new(field,value,SignSemanticValidator.Combine(g.Confidence,confidence),
                        "SIGN_BOARD",g.GroupId,a.BranchId,g.Status+"; "+string.Join(';',g.Blockers)));
            }
            var lanes=graph.Pieces.Where(p=>p.Selected&&p.SegmentIndex<=a.SegmentIndex&&
                p.SegmentIndex>=route.Actions.Where(prev=>prev.SegmentIndex<a.SegmentIndex).Select(prev=>prev.SegmentIndex).DefaultIfEmpty(0).Max())
                .Select(p=>new VehicleLaneGeometry(p.Id,p.SegmentIndex,p.Points.Select(v=>new LivePoint(v.X,v.Y,v.Z)).ToArray())
                    {PhysicalLaneId=p.Kind=="PPD_SAMPLED"?p.StartPort:p.Id}).ToArray();
            proofs.Add(new("geometry",JsonSerializer.Serialize(lanes,LiveProtocol.Json),"LOW","LANE_NETWORK",route.RouteId,a.BranchId,"Lane centres; geometry remains subject to road width assumptions."));
            return a with {Proofs=proofs.ToArray()};
        }).ToArray()};
    }
}
