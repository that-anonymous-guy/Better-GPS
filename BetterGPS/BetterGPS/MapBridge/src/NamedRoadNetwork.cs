using System.Numerics;
using System.Text.Json;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using TruckLib;

namespace BetterGPSNavigation;

internal sealed record NetworkOwnerCandidate(string PortionId,double DistanceM,double HeightM,double? FaceDot,string Status);
internal sealed record NetworkNameSource(string Id,string ObjectId,string Source,string Role,string Raw,string Identity,
    string Kind,string Cardinal,string Destination,float[] Position,float[]? Normal,string PoseStatus,string ReadingConfidence,
    string AssociationStatus,string[] Owners,NetworkOwnerCandidate[] Alternatives)
{
    public string Arrow {get;init;}="";
    public float[][] LanePoints {get;init;}=[];
    public string AdvanceGuideId {get;init;}="";
    public string GroupMethod {get;init;}="";
    public string BoardPath {get;init;}="";
    public string SupportModel {get;init;}="";
    public string InterpretationStatus {get;init;}="";
    public string StreetAxisMode {get;init;}="";
    public string[] InterpretationProofIds {get;init;}=[];
}
internal sealed record NetworkIdentity(string Identity,string Kind,string Cardinal,string Confidence,string CardinalConfidence,
    string[] SourceIds,string Rule);
internal sealed record NamedNetworkPortion(string PortionId,string Status,NetworkIdentity[] Identities,NetworkIdentity[] Candidates);
internal sealed record NamedNetworkSnapshot(int Version,string Fingerprint,string ReusePolicy,NetworkRegion Region,
    NetworkPortion[] Portions,NetworkConnection[] Connections,string[] Errors,NetworkNameSource[] Sources,NamedNetworkPortion[] Names);

/// <summary>Names are solved on the entire local graph before joining any GPS occurrence.</summary>
internal sealed class NamedRoadNetwork(IFileSystem fs,SignRoadIdentityResolver signs,RoadOverlayResolver overlays,SignPanelExtractor panels)
{
    private readonly BoardModelResolver boards=new(fs);
    private SupportSignFaceResolver? supportFaces;
    public NamedNetworkSnapshot Build(DirectedRoadNetworkGraph graph,CancellationToken token=default,double sourceMargin=0)
    {
        var ordered=ReadSources(graph,token,sourceMargin);
        ordered=AdvanceExitLinker.Link(graph,ordered);
        if(ordered.Any(s=>s.Source=="UNCLASSIFIED_STREET_SIGN"))
        {
            var independentlyNamed=Solve(graph,ordered,token);
            var streetProofs=ordered.ToDictionary(s=>s.Id);var streetNames=independentlyNamed.ToDictionary(n=>n.PortionId);
            ordered=ordered.Select(s=>s.Source=="UNCLASSIFIED_STREET_SIGN"?
                StreetNameLinker.InterpretWithIndependentNames(graph,s,streetProofs,streetNames):s).ToArray();
        }
        return Prepare(graph,ordered,token);
    }
    internal NetworkNameSource[] ReadSources(DirectedRoadNetworkGraph graph,CancellationToken token=default,double sourceMargin=0)
    {
        // A short startup graph must still contain the neighbouring roads
        // used to disambiguate each sign. Defer readings near its boundary.
        bool HasContext(Vector3 p,double margin)=>p.X>=graph.Region.MinX+margin&&p.X<=graph.Region.MaxX-margin&&
            p.Z>=graph.Region.MinZ+margin&&p.Z<=graph.Region.MaxZ-margin;
        var sources=new List<NetworkNameSource>();
        foreach(var o in overlays.Overlays.Where(o=>o.Identity.Length>0&&HasContext(o.Position,sourceMargin)).OrderBy(o=>o.Uid))
            Add(o.Uid.ToString("X16"),o.Uid.ToString("X16"),"ROAD_OVERLAY","MapAnnotation",o.RawToken,o.Identity,"RouteReference","","",o.Position,null,"UI_ANNOTATION","MEDIUM");
        foreach(var sign in signs.Signs.Where(s=>HasContext(s.Position,sourceMargin)).OrderBy(s=>s.Uid))
        {
            token.ThrowIfCancellationRequested();
            var areas=panels.Areas(sign);
            foreach(var originalArea in areas)
            {
                var area=originalArea;
                var panel=area.Extraction;if(panel is not {Status:"OK"})continue;
                bool pullThrough=IsPullThrough(panel,area);
                bool distanceBoard=IsRouteDistanceBoard(panel,area);
                bool own=distanceBoard||pullThrough||(panel.BoardPath.StartsWith("/def/sign/board/shield/",StringComparison.OrdinalIgnoreCase)||RoadIdentityResolver.Confirmation(sign))&&
                    area.RoadShields.Count==1&&area.ArrowTokens.Count==0&&area.Destinations.Count==0&&RoadIdentityRules.ReferenceOnly(string.Join(" | ",area.RawStrings));
                if(area.RoadShields.Count==1&&panel.BoardPath.StartsWith("/def/sign/board/shield/",StringComparison.OrdinalIgnoreCase))
                {
                    // A number, direction and route arrow can be separate boards
                    // on one support. An arrow assembly is a guide, not confirmation.
                    var directions=ShieldAssemblyLinker.Companions(areas,area,"/def/sign/board/shield/card_dir_2.sii");
                        var cards=directions.SelectMany(a=>a.Cardinals).Distinct().ToArray();
                        if(cards.Length==1)area=area with{Cardinals=cards,RawStrings=area.RawStrings.Concat(directions.SelectMany(a=>a.RawStrings)).ToArray(),
                            Attributes=area.Attributes.Concat(directions.SelectMany(a=>a.Attributes)).ToArray()};
                    var arrows=ShieldAssemblyLinker.Companions(areas,area,"/def/sign/board/shield/arrow_dir.sii");
                    // Even an unassigned arrow makes a multi-shield assembly
                    // unsuitable as an own-road confirmation.
                    if(arrows.Length==0&&areas.Any(a=>a.Extraction?.BoardPath=="/def/sign/board/shield/arrow_dir.sii"))own=false;
                    if(arrows.Length>0)
                    {
                        own=false;
                        var tokens=arrows.SelectMany(a=>a.ArrowTokens).Distinct().ToArray();
                        if(arrows.All(a=>a.Extraction is{Status:"OK",Reliable:true})&&tokens.Length==1)
                        {
                            var assemblyPose=Pose(graph,sign,panel);
                            Add($"{sign.Uid:X16}:{area.AreaName}:assembly",$"{sign.Uid:X16}","SHIELD_ASSEMBLY","GuideTo",
                                string.Join(" | ",area.RawStrings.Concat(arrows.SelectMany(a=>a.RawStrings))),area.RoadShields[0],"RouteReference",
                                area.Cardinals.Count==1?area.Cardinals[0]:"","",assemblyPose.Position,assemblyPose.Normal,assemblyPose.Status,"MEDIUM",tokens[0]);
                        }
                    }
                }
                var groups=panel.SemanticGroups.Where(g=>RoadIdentityRules.Valid(g.Kind,g.Road)).ToArray();
                bool street=StreetNameLinker.IsStreetPlate(panel,sign.DefinitionName);
                var headers=StreetNameLinker.IntersectionHeaders(panel);
                if(!own&&groups.Length==0&&!street&&headers.Length==0)continue;
                var pose=Pose(graph,sign,panel);
                bool overheadStreet=street&&StreetNameLinker.OverheadInformationPlate(graph,panel,pose.Position);
                if(own)
                {
                    var fields=SignSemanticValidator.Validate(area,null,area.RoadShields[0],area.Cardinals.Count==1?area.Cardinals[0]:"",signs.IsKnownDestination);
                    Add($"{sign.Uid:X16}:{area.AreaName}:own",$"{sign.Uid:X16}",distanceBoard?"DISTANCE_BOARD_REFERENCE":pullThrough?"PULL_THROUGH_REFERENCE":"CONFIRMATION_SHIELD","OwnRoute",string.Join(" | ",area.RawStrings),area.RoadShields[0],"RouteReference",
                        fields.CardinalConfidence=="HIGH"&&area.Cardinals.Count==1?area.Cardinals[0]:"",pullThrough&&area.Destinations.Count==1?area.Destinations[0]:"",pose.Position,pose.Normal,pose.Status,fields.IdentityConfidence);
                }
                else
                {
                    foreach(var group in groups)
                    {
                        var arrows=panel.Objects.Where(o=>o.Visible&&o.Reliable&&group.ArrowObjectIds.Contains(o.Id)).Select(o=>o.Arrow).Distinct().ToArray();
                        bool streetRows=group.Kind=="StreetName"&&group.Method=="EXPLICIT_HORIZONTAL_DIVIDER"&&
                            groups.Length==2&&groups.All(g=>g.Kind=="StreetName"&&SignGroupGuidance.ReadableGroupRoad(g,panel))&&
                            groups.SelectMany(g=>panel.Objects.Where(o=>o.Visible&&o.Reliable&&g.ArrowObjectIds.Contains(o.Id)).Select(o=>o.Arrow)).Order().SequenceEqual(new[]{"LEFT","RIGHT"});
                        Add(group.Id,$"{sign.Uid:X16}",streetRows?"DIRECTIONAL_STREET_ROWS":"SIGN_GROUP","GuideTo",string.Join(" | ",area.RawStrings),group.Road,group.Kind,group.Cardinal,group.Destination,
                            pose.Position,pose.Normal,pose.Status,SignGroupGuidance.ReadableGroupRoad(group,panel)?"MEDIUM":"LOW",arrows.Length==1?arrows[0]:"",
                            arrows.Length==1&&arrows[0]=="DOWN_LANE"?LanePoints(sign,panel,group):[],group.Method);
                    }
                    if(street)
                        foreach(var name in StreetNameLinker.Names(panel))
                        {
                            bool unresolved=StreetNameLinker.IsInformationPlate(panel)&&!overheadStreet;
                            Add($"{sign.Uid:X16}:{area.AreaName}:{name}",$"{sign.Uid:X16}",unresolved?"UNCLASSIFIED_STREET_SIGN":overheadStreet?"STREET_CONFIRMATION_SIGN":"STREET_NAME_SIGN",overheadStreet?"OwnRoute":"CrossStreet",name,name,"StreetName","","",
                                pose.Position,pose.Normal,pose.Status,"MEDIUM",boardPath:panel.BoardPath,supportModel:panel.SupportModel,
                                interpretation:unresolved?"SUPPORT_FAMILY_OR_PLACEMENT_UNRESOLVED":StreetNameLinker.InformationFamily(panel));
                        }
                    foreach(var name in headers)
                        Add($"{sign.Uid:X16}:{area.AreaName}:header:{name}",$"{sign.Uid:X16}","STREET_NAME_SIGN","CrossStreet",name,name,"StreetName","","",
                            pose.Position,pose.Normal,pose.Status,"MEDIUM",groupMethod:"EXPLICIT_INTERSECTION_STREET_HEADER");
                }
            }
        }
        var ordered=sources.GroupBy(s=>s.Id).Select(g=>g.First()).OrderBy(s=>s.Id,StringComparer.Ordinal).ToArray();
        return ordered;

        void Add(string id,string objectId,string source,string role,string raw,string identity,string kind,string cardinal,string destination,
            Vector3 position,Vector3? normal,string pose,string reading,string arrow="",float[][]? lanePoints=null,string groupMethod="",string boardPath="",string supportModel="",string interpretation="")=>sources.Add(Reassociate(graph,new(id,objectId,source,role,raw,identity,kind,cardinal,destination,
                BoardModelResolver.V(position),normal is null?null:BoardModelResolver.V(normal.Value),pose,reading,"",[],[]){Arrow=arrow,LanePoints=lanePoints??[],GroupMethod=groupMethod,BoardPath=boardPath,SupportModel=supportModel,InterpretationStatus=interpretation}));
    }
    private float[][] LanePoints(ParsedSign sign,ExtractedSignPanel panel,SignSemanticGroup group)
    {
        if(sign.FollowRoadDir||panel.BoardOffset.Any(x=>Math.Abs(x)>1e-6)||panel.WidthM is not {} width||panel.HeightM is not {} height||
            !BoardModelResolver.Valid(panel.Anchor.LocalPosition,3)||!BoardModelResolver.Valid(panel.Anchor.LocalRotation,4))return [];
        try
        {
            var board=boards.Resolve(panel);var transforms=BoardPlacementResolver.Hypotheses(board);
            if(transforms.Length==0)return [];
            var q=panel.Anchor.LocalRotation!;var rotation=Quaternion.Normalize(sign.Rotation*new Quaternion(q[0],q[1],q[2],q[3]));
            var anchor=sign.Position+Vector3.Transform(BoardModelResolver.V(panel.Anchor.LocalPosition!),sign.Rotation);
            var points=new List<float[]>();
            foreach(var arrow in panel.Objects.Where(o=>o.Visible&&o.Reliable&&group.ArrowObjectIds.Contains(o.Id)))
            {
                double N(string key)=>double.TryParse(arrow.Effective.GetValueOrDefault(key),System.Globalization.NumberStyles.Float,
                    System.Globalization.CultureInfo.InvariantCulture,out var n)?n:double.NaN;
                var l=N("coords_l");var r=N("coords_r");var b=N("coords_b");var t=N("coords_t");
                if(!double.IsFinite(l+r+b+t)||r<=l||t<=b)return [];
                // All attachment/mirroring hypotheses must agree on the owner.
                // Both ends cover an unknown texture tip and vertical flip.
                foreach(var transform in transforms)foreach(var y in new[]{b,t})
                    points.Add(BoardPlacementResolver.Place(new((float)((l+r)/2),(float)y),transform,board,anchor,rotation,
                        panel.Anchor.Scale,width,height).WorldPoint);
            }
            return points.ToArray();
        }
        catch(Exception e) when(e is IOException or InvalidDataException or ArgumentException){return [];}
    }
    internal static bool IsPullThrough(ExtractedSignPanel panel,ParsedSignArea area)=>
        panel.Status=="OK"&&panel.SchemaValidated&&
        panel.BoardPath.StartsWith("/def/sign/board/overhead/",StringComparison.OrdinalIgnoreCase)&&
        Regex.IsMatch(Path.GetFileNameWithoutExtension(panel.BoardPath),@"(?:^|_)pt_\d+$",RegexOptions.IgnoreCase)&&
        area.RoadShields.Count==1&&area.Cardinals.Count<=1&&area.Destinations.Count<=1&&
        area.ArrowTokens.Count==0&&area.ExitNumbers.Count==0&&
        RoadIdentityRules.ReferenceOnly(string.Join(" | ",area.RawStrings));
    internal static bool IsRouteDistanceBoard(ExtractedSignPanel panel,ParsedSignArea area)=>panel.Status=="OK"&&
        panel.Objects.Any(o=>o.Visible&&o.Kind=="sign_template_mileage")&&area.RoadShields.Count==1&&
        area.Cardinals.Count<=1&&area.ArrowTokens.Count==0&&area.ExitNumbers.Count==0&&
        !panel.Objects.Any(o=>o.Visible&&o.Kind=="sign_template_image"&&o.Road.Length==0)&&
        RoadIdentityRules.ReferenceOnly(string.Join(" | ",area.RawStrings.Concat(panel.Objects.Where(o=>o.Visible).Select(o=>o.Text))));
    internal static NamedNetworkSnapshot Prepare(DirectedRoadNetworkGraph graph,NetworkNameSource[] ordered,CancellationToken token=default)
    {
        NamedNetworkPortion[] names;
        using(PerformanceTrace.Measure("network-solve"))names=Solve(graph,ordered,token);
        var fingerprint=JsonContentHash.Compute(new{solver="native-access-and-captured-continuity-v9",graph.Region,graph.Portions,graph.Connections,graph.Errors,sources=ordered});
        // Durable result, rebuilt from effective mounted assets; no stale persistent cache is loaded.
        return new(1,"network-content-sha256:"+fingerprint,"REBUILD_FROM_MOUNTED_ASSETS_NO_CACHE_REUSE",graph.Region,graph.Portions,graph.Connections,graph.Errors,ordered,names);
    }
    internal static NetworkNameSource Reassociate(DirectedRoadNetworkGraph graph,NetworkNameSource s)
    {
            if(s.Source=="CORROBORATED_STREET_INTERPRETATION")return StreetNameLinker.InterpretedAxis(graph,s);
            if(s.Source=="UNCLASSIFIED_STREET_SIGN")return s with{Owners=[],AssociationStatus="STREET_INTERPRETATION_UNRESOLVED",
                Alternatives=Associate(graph,BoardModelResolver.V(s.Position),s.Normal is null?null:BoardModelResolver.V(s.Normal),"CrossStreet",s.PoseStatus)};
            if(s.Kind=="StreetName")s=s with{Identity=StreetNameLinker.Canonical(s.Identity)};
            if(s.Source=="STREET_CONFIRMATION_SIGN")s=s with{Role="OwnRoute"};
            var normal=s.Normal is null?(Vector3?)null:BoardModelResolver.V(s.Normal);
            var alternatives=Associate(graph,BoardModelResolver.V(s.Position),normal,s.Role,s.PoseStatus);
            var plausible=alternatives.Where(a=>a.Status=="CANDIDATE").ToArray();
            string association;string[] owners;
            if(s.Role is "GuideTo" or "CrossStreet")
            {association="ROLE_NEEDS_BRANCH_OR_CROSS_STREET_LINK";owners=[];}
            else if(plausible.Length==0){association="NO_PLAUSIBLE_OWNER";owners=[];}
            else
            {
                var best=plausible.Min(a=>a.DistanceM);
                owners=plausible.Where(a=>a.DistanceM<=best+8).Select(a=>a.PortionId).Order().ToArray();
                var items=owners.Select(o=>graph.Get(o).ItemUid).Distinct().ToArray();
                association=best>50?"DISTANT":s.Role=="OwnRoute"&&normal is null?"POSE_UNRESOLVED":items.Length==1?s.Role=="MapAnnotation"?"UNIQUE_PHYSICAL_ROAD_BOTH_SENSES":"ORIENTED_GEOMETRIC_CANDIDATE":"AMBIGUOUS_OWNERS";
            }
            // No face means no association of an official cardinal with either traffic direction.
            var result=s with{Cardinal=s.Role=="OwnRoute"&&normal is null?"":s.Cardinal,AssociationStatus=association,Owners=owners,Alternatives=alternatives};
            if(s.Source=="DIRECTIONAL_STREET_ROWS")return StreetNameLinker.DirectionalArm(graph,result);
            if(s.Source=="STREET_CONFIRMATION_SIGN")return StreetNameLinker.Confirmation(graph,result);
            if(s.Role=="GuideTo")return GuideSignLinker.Associate(graph,result);
            return s.Role is "CrossStreet" or "StreetAxis"?StreetNameLinker.Associate(graph,result with{Role="CrossStreet",Owners=[],AssociationStatus="ROLE_NEEDS_BRANCH_OR_CROSS_STREET_LINK"}):result;
    }
    private (Vector3 Position,Vector3? Normal,string Status) Pose(DirectedRoadNetworkGraph graph,ParsedSign sign,ExtractedSignPanel panel)
    {
        using var timing=PerformanceTrace.Detail("sign-pose");
        // Extracted normal is a model hypothesis, never independently verified or HIGH.
        try
        {
            var board=boards.Resolve(panel);
            if(board.Status=="NOT_FOUND"&&board.ModelPath.Length==0)
            {
                var support=(supportFaces??=new(fs,boards)).Resolve(sign with{FollowRoadDir=false},panel);
                return support is null?(sign.Position,null,"SUPPORT_FACE_UNRESOLVED"):Aligned(support.Position,support.Normal,support.Status);
            }
            if(board.Surfaces.Length!=1||!BoardModelResolver.Valid(panel.Anchor.LocalPosition,3)||!BoardModelResolver.Valid(panel.Anchor.LocalRotation,4)||
                panel.BoardOffset.Any(x=>Math.Abs(x)>1e-6))return(sign.Position,null,"POSE_UNRESOLVED:"+board.Status);
            var q=panel.Anchor.LocalRotation!;
            var rotation=Quaternion.Normalize(sign.Rotation*new Quaternion(q[0],q[1],q[2],q[3]));
            var normal=Vector3.Transform(BoardModelResolver.V(board.Surfaces[0].Normal),rotation);
            var position=sign.Position+Vector3.Transform(BoardModelResolver.V(panel.Anchor.LocalPosition!),sign.Rotation);
            if(!RoadLaneGeometryResolver.Finite(normal)||normal.LengthSquared()<.99)return(sign.Position,null,"POSE_UNRESOLVED");
            return Aligned(position,Vector3.Normalize(normal),"MODEL_FACE_CANDIDATE_NOT_RENDER_VERIFIED");
            (Vector3 Position,Vector3? Normal,string Status) Aligned(Vector3 p,Vector3 n,string status)
            {
                if(!sign.FollowRoadDir)return(p,n,status);
                var aligned=RoadAlignedSignPose.Align(graph,sign.Position,p,n);
                return aligned is {} a?(a.Position,a.Normal,"ROAD_ALIGNED_MODEL_FACE_CANDIDATE"):(sign.Position,null,"FOLLOW_ROAD_DIR_AMBIGUOUS");
            }
        }
        catch(Exception e) when(e is IOException or InvalidDataException or ArgumentException){return(sign.Position,null,"POSE_READ_ERROR");}
    }
    internal static NetworkOwnerCandidate[] Associate(DirectedRoadNetworkGraph graph,Vector3 position,Vector3? normal,string role,string pose)
    {
        using var timing=PerformanceTrace.Detail("sign-association");
        var result=new List<NetworkOwnerCandidate>();
        foreach(var p in graph.Query(position))
        {
            double best=double.PositiveInfinity,height=0,crossBest=double.PositiveInfinity,crossHeight=0;Vector3 tangent=default,crossTangent=default;
            foreach(var path in p.Paths)
            for(int i=1;i<path.Length;i++)
            {
                var a=NetworkPortion.V(path[i-1]);var b=NetworkPortion.V(path[i]);var v=new Vector2(b.X-a.X,b.Z-a.Z);
                if(v.LengthSquared()<1e-8)continue;
                var t=Math.Clamp(Vector2.Dot(new(position.X-a.X,position.Z-a.Z),v)/v.LengthSquared(),0,1);var q=Vector3.Lerp(a,b,t);
                var distance=Vector2.Distance(new(position.X,position.Z),new(q.X,q.Z));
                if(distance<best){best=distance;height=Math.Abs(position.Y-q.Y);tangent=Vector3.Normalize(b-a);}
                // A nearby endpoint beyond the sign is not the carriageway at the sign.
                // Intersect its horizontal cross-section; a suspended plaque does not share road altitude.
                if(role=="OwnRoute"&&normal is Vector3 face)
                {
                    var horizontal=new Vector2(face.X,face.Z);var denominator=Vector2.Dot(horizontal,v);
                    if(Math.Abs(denominator)>1e-6)
                    {
                        var crossT=Vector2.Dot(horizontal,new(position.X-a.X,position.Z-a.Z))/denominator;
                        if(crossT>=0&&crossT<=1)
                        {
                            var crossing=Vector3.Lerp(a,b,crossT);
                            var lateral=Vector2.Distance(new(position.X,position.Z),new(crossing.X,crossing.Z));
                            if(lateral<crossBest){crossBest=lateral;crossHeight=Math.Abs(position.Y-crossing.Y);crossTangent=Vector3.Normalize(b-a);}
                        }
                    }
                }
            }
            if(best>180||!double.IsFinite(best))continue;
            bool crosses=double.IsFinite(crossBest);
            if(role=="OwnRoute"&&normal is not null&&crosses){best=crossBest;height=crossHeight;tangent=crossTangent;}
            double? dot=normal is null?null:Vector3.Dot(normal.Value,tangent);
            var status=p.Blockers.Any(b=>b!="LANE_WIDTH_4_5M_ASSUMED")?"UNRESOLVED_GEOMETRY":
                role=="OwnRoute"&&dot is not null&&dot>-.5?"FACE_NOT_APPROACH_CANDIDATE":
                role=="OwnRoute"&&normal is not null&&!crosses?"OUTSIDE_SIGN_CROSS_SECTION":best>180?"DISTANT":"CANDIDATE";
            result.Add(new(p.Id,best,height,dot,status));
        }
        return result.OrderBy(c=>c.DistanceM).ThenBy(c=>c.PortionId,StringComparer.Ordinal).ToArray();
    }
    internal static NamedNetworkPortion[] Solve(DirectedRoadNetworkGraph graph,NetworkNameSource[] sources,CancellationToken token=default)
    {
        token.ThrowIfCancellationRequested();
        var unbranched=new Dictionary<string,HashSet<string>>();
        HashSet<string> Unbranched(string owner)
        {
            token.ThrowIfCancellationRequested();
            if(!unbranched.TryGetValue(owner,out var path))unbranched[owner]=path=UnbranchedContinuity(graph,owner).ToHashSet();
            return path;
        }
        var signed=new Dictionary<string,HashSet<string>>();
        HashSet<string> Signed(NetworkNameSource source)
        {
            token.ThrowIfCancellationRequested();
            if(!signed.TryGetValue(source.Id,out var path))signed[source.Id]=path=RoadNameContinuity.SignedCorridor(graph,source).ToHashSet();
            return path;
        }
        var labels=graph.Portions.ToDictionary(p=>p.Id,p=>new List<NetworkIdentity>());
        var candidates=graph.Portions.ToDictionary(p=>p.Id,p=>new List<NetworkIdentity>());
        var accesses=NativeAccessRoads.Find(graph);
        // Mileage boards can mention a future route, rather than the pavement.
        bool MileageConfirmed(NetworkNameSource s)=>s.Source!="DISTANCE_BOARD_REFERENCE"||
            sources.Any(other=>other.ObjectId!=s.ObjectId&&other.Source=="CONFIRMATION_SHIELD"&&
                other.Identity==s.Identity&&other.ReadingConfidence is "HIGH" or "MEDIUM"&&
                s.Owners.Any(owner=>Unbranched(owner).Overlaps(other.Owners)));
        var usable=sources.Where(s=>(s.Role is "OwnRoute" or "MapAnnotation" or "StreetAxis")&&s.ReadingConfidence is "HIGH" or "MEDIUM"&&MileageConfirmed(s)&&
            (s.AssociationStatus is "UNIQUE_PHYSICAL_ROAD_BOTH_SENSES" or "ORIENTED_GEOMETRIC_CANDIDATE" or "JUNCTION_STREET_AXIS"||RepeatedMapOverlay(graph,s,sources))&&
            StreetPlateConfirmed(graph,s,sources)).ToArray();
        // Proof records are immutable and identical along every propagated portion.
        var proofLabels=new Dictionary<(string,string?,string,string),NetworkIdentity>();
        NetworkIdentity Label(NetworkNameSource first,NetworkNameSource? second,string rule,string confidence)
        {
            var key=(first.Id,second?.Id,rule,confidence);
            if(proofLabels.TryGetValue(key,out var known))return known;
            string cardinal=first.Cardinal;
            if(second is not null&&second.Cardinal.Length>0){if(cardinal.Length==0)cardinal=second.Cardinal;else if(cardinal!=second.Cardinal)cardinal="";}
            var sourceIds=second is null||second.Id==first.Id?new[]{first.Id}:new[]{first.Id,second.Id}.Order().ToArray();
            return proofLabels[key]=new(first.Identity,first.Kind,cardinal,confidence,
                first.Cardinal.Length>0||second is {Cardinal.Length:>0}?confidence:"NONE",sourceIds,rule);
        }
        var usableSet=usable.Select(s=>s.Id).ToHashSet(StringComparer.Ordinal);
        foreach(var s in sources.Where(s=>s.Role is "OwnRoute" or "MapAnnotation" or "StreetAxis"))
        foreach(var owner in s.Owners)
            (usableSet.Contains(s.Id)?labels:candidates)[owner].Add(Label(s,null,"DIRECT_NETWORK_ANCHOR",usableSet.Contains(s.Id)?"MEDIUM":"LOW"));
        var initialLabels=labels.ToDictionary(k=>k.Key,k=>k.Value.ToArray());
        foreach(var s in usable)
        foreach(var owner in s.Owners)
        foreach(var id in RoadNameContinuity.Corridor(graph,owner,canEnter:id=>
            Unbranched(owner).Contains(id)&&!(s.Kind=="RouteReference"&&accesses.Contains(id))&&
            !initialLabels[id].Any(l=>l.Kind==s.Kind&&l.Identity!=s.Identity),followShallowExits:false)
            .Where(id=>!s.Owners.Contains(id)))
            labels[id].Add(Label(s,null,"UNBRANCHED_NETWORK_CONTINUITY","MEDIUM"));
        // Explicit turn shields and street arrows name the immediate outgoing
        // road. Keep this local: no continuation through another junction or ramp.
        foreach(var s in sources.Where(s=>LocalTurnSource(s)))
        foreach(var id in LocalTurnScope(graph,s).Where(id=>!accesses.Contains(id)))
            labels[id].Add(Label(s,null,"SIGNED_OUTGOING_ROAD","MEDIUM"));
        foreach(var s in sources.Where(s=>s.GroupMethod=="SHARED_SHIELD_DIRECTION_ROWS"))
        foreach(var id in SharedRouteArmScope(graph,s).Where(id=>!accesses.Contains(id)))
            labels[id].Add(Label(s,null,"JUNCTION_ROUTE_ARM","MEDIUM") with{Cardinal="",CardinalConfidence="NONE"});
        var directLabels=labels.ToDictionary(k=>k.Key,k=>k.Value.ToArray());
        var constrainedSigned=new Dictionary<string,HashSet<string>>();
        HashSet<string> ConstrainedSigned(NetworkNameSource source)
        {
            token.ThrowIfCancellationRequested();
            if(!constrainedSigned.TryGetValue(source.Id,out var path))constrainedSigned[source.Id]=path=
                RoadNameContinuity.SignedCorridor(graph,source,id=>!directLabels[id].Any(l=>l.Kind==source.Kind&&(l.Identity!=source.Identity||
                    l.Cardinal.Length>0&&source.Cardinal.Length>0&&l.Cardinal!=source.Cardinal))).ToHashSet();
            return path;
        }
        // Branch signs from independent approaches can corroborate the common
        // road downstream. Their individual approach ramps never inherit it.
        foreach(var group in sources.Where(s=>s.Role=="GuideTo"&&s.AssociationStatus=="DIRECTIONAL_BRANCH_TARGET"&&
            s.Arrow!="DOWN_LANE"&&
            s.ReadingConfidence is "MEDIUM" or "HIGH"&&RoadIdentityRules.ReferenceOnly(s.Raw)).GroupBy(s=>(s.Identity,s.Kind,s.Cardinal)))
        {
            var observations=group.ToArray();
            for(int i=0;i<observations.Length;i++)for(int j=i+1;j<observations.Length;j++)
            {
                var a=observations[i];var b=observations[j];
                if(a.ObjectId==b.ObjectId)continue;
                var common=ConstrainedSigned(a).Intersect(ConstrainedSigned(b)).ToArray();
                if(common.Any(id=>directLabels[id].Any(l=>l.Kind==a.Kind&&(l.Identity!=a.Identity||l.Cardinal.Length>0&&a.Cardinal.Length>0&&l.Cardinal!=a.Cardinal))))continue;
                foreach(var id in common.Where(id=>!accesses.Contains(id)))labels[id].Add(Label(a,b,"CORROBORATED_SIGNED_BRANCH_CONTINUITY","MEDIUM"));
            }
        }
        // Local independent inscriptions constrain single-anchor geometric
        // propagation as well as long-distance bracketing. Otherwise a remote
        // shield can spill through shallow exits onto an independently signed road.
        var localLabels=labels.ToDictionary(k=>k.Key,k=>k.Value.ToArray());
        var sourceById=sources.ToDictionary(s=>s.Id);
        // A directional destination row must not rename pavement independently
        // identified by a facing street inscription. Resolve the inscription's
        // reciprocal corridor first, stopping at other street inscriptions.
        bool FacingStreet(NetworkNameSource s)=>s.Source=="STREET_CONFIRMATION_SIGN"||s.Source=="CORROBORATED_STREET_INTERPRETATION"&&s.StreetAxisMode=="FACING";
        foreach(var s in usable.Where(FacingStreet))
        foreach(var owner in s.Owners)
        foreach(var id in RoadNameContinuity.Corridor(graph,owner,canEnter:id=>!localLabels[id].Any(l=>l.Kind==s.Kind&&
            l.Identity!=s.Identity&&l.SourceIds.Any(proof=>FacingStreet(sourceById[proof]))),followShallowExits:false))
        {
            labels[id].RemoveAll(l=>l.Kind==s.Kind&&l.Identity!=s.Identity&&
                l.SourceIds.All(proof=>sourceById[proof].Source=="DIRECTIONAL_STREET_ROWS"));
            if(!s.Owners.Contains(id))labels[id].Add(Label(s,null,"RECIPROCAL_GEOMETRIC_CONTINUITY","MEDIUM"));
        }
        localLabels=labels.ToDictionary(k=>k.Key,k=>k.Value.ToArray());
        foreach(var s in usable)
        foreach(var owner in s.Owners)
        {
            var path=RoadNameContinuity.Corridor(graph,owner,canEnter:id=>
                !(s.Kind=="RouteReference"&&accesses.Contains(id)&&!s.Owners.Contains(id))&&
                !localLabels[id].Any(l=>l.Kind==s.Kind&&(l.Identity!=s.Identity||
                l.Cardinal.Length>0&&s.Cardinal.Length>0&&l.Cardinal!=s.Cardinal)),followShallowExits:false);
            foreach(var id in path.Where(id=>!s.Owners.Contains(id)))
                labels[id].Add(Label(s,null,"RECIPROCAL_GEOMETRIC_CONTINUITY","MEDIUM"));
        }
        // Independent local branch confirmations also constrain long-distance
        // bracketing. A remote same-reference pair must not overwrite a signed road.
        directLabels=labels.ToDictionary(k=>k.Key,k=>k.Value.ToArray());
        // Explicit branch inscriptions also bracket the road between independent
        // junctions. The directed mandatory path excludes alternatives and cycles;
        // TO/JCT/EXIT indications cannot act as naming endpoints.
        var bracketSources=usable.Concat(sources.Where(s=>s.Role=="GuideTo"&&s.AssociationStatus=="DIRECTIONAL_BRANCH_TARGET"&&
            s.Arrow!="DOWN_LANE"&&
            s.ReadingConfidence is "MEDIUM" or "HIGH"&&RoadIdentityRules.ReferenceOnly(s.Raw))).ToArray();
        foreach(var signedPass in new[]{false,true})
        {
        // Finish current-road proofs first. A guide indication cannot contradict
        // a corridor already bracketed by independent road confirmations.
        if(signedPass)directLabels=labels.ToDictionary(k=>k.Key,k=>k.Value.ToArray());
        foreach(var group in bracketSources.GroupBy(s=>(s.Identity,s.Kind)))
        {
            var observations=group.ToArray();
            foreach(var a in observations)
            foreach(var b in observations.Where(b=>b.ObjectId!=a.ObjectId))
            foreach(var start in a.Owners)foreach(var end in b.Owners)
            {
                token.ThrowIfCancellationRequested();
                if((a.Role=="GuideTo"||b.Role=="GuideTo")!=signedPass)continue;
                // A directional guide names what follows its branch. It cannot
                // confirm the road approaching that branch as a downstream end.
                if(b.Role=="GuideTo")continue;
                if(a.Cardinal.Length>0&&b.Cardinal.Length>0&&a.Cardinal!=b.Cardinal)continue;
                if(a.Kind=="RouteReference"&&!BracketDirectionCompatible(graph,start,end))continue;
                var path=MandatoryCorridor(graph,start,end,double.PositiveInfinity,true,token);
                if(path is null||path.Length<3)continue;
                if(signedPass&&(path.Any(id=>!Signed(a).Contains(id))||
                    path.Any(id=>directLabels[id].Any(l=>l.Kind=="StreetName")&&a.Kind=="RouteReference")))continue;
                if(path.Any(id=>directLabels[id].Any(l=>l.Kind==a.Kind&&(l.Identity!=a.Identity||
                    a.Cardinal.Length>0&&b.Cardinal.Length>0&&l.Cardinal.Length>0&&l.Cardinal!=a.Cardinal))))continue;
                var label=Label(a,b,a.Role=="GuideTo"||b.Role=="GuideTo"?"BRACKETED_SIGNED_NETWORK_CORRIDOR":"BRACKETED_MANDATORY_NETWORK_CORRIDOR","MEDIUM");
                if(a.Cardinal.Length==0||b.Cardinal.Length==0)label=label with{Cardinal="",CardinalConfidence="NONE"};
                foreach(var id in path.Skip(1).SkipLast(1).Where(id=>!signedPass||!accesses.Contains(id)))labels[id].Add(label);
            }
        }
        }
        // Resolve established confirmations and bracketed corridors first. A
        // single median icon may fill their gaps, never displace those proofs
        // or become an endpoint for remote bracketing across other highways.
        var established=labels.ToDictionary(k=>k.Key,k=>k.Value.ToArray());
        foreach(var s in sources.Where(s=>!usableSet.Contains(s.Id)&&DividedRoadOverlay.CoversPair(graph,s)))
        {
            bool Compatible(string id)=>!established[id].Any(l=>l.Kind==s.Kind&&l.Identity!=s.Identity);
            if(s.Owners.Any(id=>!Compatible(id)))continue;
            foreach(var owner in s.Owners)
            foreach(var id in RoadNameContinuity.Corridor(graph,owner,canEnter:Compatible,followShallowExits:true))
            {
                if(accesses.Contains(id))continue;
                labels[id].Add(Label(s,null,id==owner?"DIRECT_NETWORK_ANCHOR":"RECIPROCAL_GEOMETRIC_CONTINUITY","MEDIUM"));
            }
        }
        return graph.Portions.Select(p=>
        {
            var direct=labels[p.Id].GroupBy(l=>(l.Identity,l.Kind,l.Cardinal)).Select(g=>g.OrderByDescending(l=>l.SourceIds.Length).First()).ToArray();
            bool conflict=direct.GroupBy(l=>l.Identity).Any(g=>g.Select(l=>l.Cardinal).Where(c=>c.Length>0).Distinct().Count()>1);
            // A street name and a numbered highway can name the same pavement.
            bool multiple=direct.GroupBy(l=>l.Kind).Any(g=>g.Select(l=>l.Identity).Distinct().Count()>1);
            var resolved=conflict||multiple?Array.Empty<NetworkIdentity>():direct;
            var possible=candidates[p.Id].Concat(conflict||multiple?direct:[]).GroupBy(l=>(l.Identity,l.Kind,l.Cardinal)).Select(g=>g.First()).ToArray();
            return new NamedNetworkPortion(p.Id,conflict?"CONFLICT":multiple?"AMBIGUOUS":resolved.Length>0?"RESOLVED":possible.Length>1?"AMBIGUOUS":"UNKNOWN",resolved,possible);
        }).ToArray();
    }
    internal static bool StreetPlateConfirmed(DirectedRoadNetworkGraph graph,NetworkNameSource source,IEnumerable<NetworkNameSource> sources)
    {
        if(source.Source is not ("STREET_NAME_SIGN" or "STREET_CONFIRMATION_SIGN")||SignGroupResolver.IsSignedStreetName(source.Identity))return true;
        // Bare names on generic information plates need two independent plates
        // on the same native street axis; a lone city/place label is insufficient.
        if(source.Role!="StreetAxis"||source.Normal is null)return false;
        var items=source.Owners.Select(id=>graph.Get(id).ItemUid).ToHashSet();
        return sources.Any(other=>other.Source==source.Source&&other.ObjectId!=source.ObjectId&&other.Identity==source.Identity&&
            other.Role=="StreetAxis"&&other.ReadingConfidence is "HIGH" or "MEDIUM"&&other.Normal is not null&&
            Vector3.Distance(NetworkPortion.V(source.Position),NetworkPortion.V(other.Position))<=200&&
            Math.Abs(Vector3.Dot(NetworkPortion.V(source.Normal),NetworkPortion.V(other.Normal)))>.9&&
            other.Owners.Any(id=>items.Contains(graph.Get(id).ItemUid)));
    }
    internal static bool CorroboratedMapOverlay(DirectedRoadNetworkGraph graph,NetworkNameSource source,IEnumerable<NetworkNameSource> sources)
    {
        return DividedRoadOverlay.CoversPair(graph,source)||RepeatedMapOverlay(graph,source,sources);
    }
    private static bool RepeatedMapOverlay(DirectedRoadNetworkGraph graph,NetworkNameSource source,IEnumerable<NetworkNameSource> sources)
    {
        bool Eligible(NetworkNameSource s)=>s.Role=="MapAnnotation"&&s.AssociationStatus=="AMBIGUOUS_OWNERS"&&
            s.ReadingConfidence is "HIGH" or "MEDIUM"&&s.Owners.Length is >=2 and <=4&&s.Owners.All(id=>
                graph.Get(id).Kind=="ROAD"&&NativeAccessRoads.Lanes(graph.Get(id),true)>=2&&
                graph.Get(id).Blockers.All(b=>b=="LANE_WIDTH_4_5M_ASSUMED"));
        if(!Eligible(source))return false;
        // Repetition must cover every ambiguous carriageway through reciprocal
        // native continuity. A nearby parallel road with no matching second
        // annotation remains ambiguous. No travel cardinal is assigned.
        return sources.Any(other=>other.ObjectId!=source.ObjectId&&other.Identity==source.Identity&&Eligible(other)&&
            source.Owners.All(a=>other.Owners.Any(b=>RoadNameContinuity.Corridor(graph,a).Contains(b)))&&
            other.Owners.All(b=>source.Owners.Any(a=>RoadNameContinuity.Corridor(graph,b).Contains(a))));
    }
    internal static bool LocalTurnSource(NetworkNameSource s)=>s.Role=="GuideTo"&&s.Kind=="StreetName"&&s.AssociationStatus=="DIRECTIONAL_BRANCH_TARGET"&&
        s.ReadingConfidence is "MEDIUM" or "HIGH"&&s.Arrow is "LEFT" or "RIGHT"&&RoadIdentityRules.ReferenceOnly(s.Raw);
    internal static string[] SharedRouteArmScope(DirectedRoadNetworkGraph graph,NetworkNameSource source)
    {
        if(source.GroupMethod!="SHARED_SHIELD_DIRECTION_ROWS"||source.Role!="GuideTo"||source.Kind!="RouteReference"||
            source.AssociationStatus!="DIRECTIONAL_BRANCH_TARGET"||source.ReadingConfidence is not ("HIGH" or "MEDIUM")||
            !RoadIdentityRules.ReferenceOnly(source.Raw))return [];
        var result=new HashSet<string>();var origin=NetworkPortion.V(source.Position);
        foreach(var branch in source.Owners)
        {
            var next=graph.Next(branch);
            if(next.Length!=1||graph.Get(next[0]).Kind!="ROAD")continue;
            var road=graph.Get(next[0]);
            // The numbered road at a two-direction junction plate is also named
            // in the opposite traffic sense. Its official cardinal is not inverted.
            foreach(var sense in graph.Portions.Where(p=>p.ItemUid==road.ItemUid&&p.Kind=="ROAD"))
            foreach(var id in RoadNameContinuity.Corridor(graph,sense.Id,followShallowExits:false))
            {
                var p=graph.Get(id);
                if(p.Paths.SelectMany(path=>path).All(point=>System.Numerics.Vector3.Distance(origin,NetworkPortion.V(point))<=1000))result.Add(id);
            }
        }
        return result.Order().ToArray();
    }
    internal static string[] LocalTurnScope(DirectedRoadNetworkGraph graph,NetworkNameSource source)
    {
        if(!LocalTurnSource(source))return [];
        var result=new HashSet<string>();
        foreach(var owner in source.Owners)
        {
            if(graph.Get(owner).Kind!="PREFAB")continue;
            result.Add(owner);var current=owner;double distance=0;var seen=new HashSet<string>{owner};
            while(distance<1500)
            {
                var next=graph.Next(current);
                if(next.Length!=1)break;
                var p=graph.Get(next[0]);
                if(p.Kind!="ROAD"||!seen.Add(p.Id)||p.Blockers.Any(b=>b!="LANE_WIDTH_4_5M_ASSUMED"))break;
                result.Add(p.Id);distance+=p.LengthM;current=p.Id;
            }
            var exit=graph.Next(owner);
            if(exit.Length==1&&graph.Get(exit[0]).Kind=="ROAD")
            {
                var physical=graph.Get(exit[0]).ItemUid;var origin=NetworkPortion.V(source.Position);
                foreach(var sense in graph.Portions.Where(p=>p.ItemUid==physical&&p.Kind=="ROAD"))
                foreach(var id in RoadNameContinuity.Corridor(graph,sense.Id,followShallowExits:false))
                    if(graph.Get(id).Paths.SelectMany(path=>path).All(point=>Vector3.Distance(origin,NetworkPortion.V(point))<=500))result.Add(id);
            }
        }
        return result.Order().ToArray();
    }
    internal static string[] UnbranchedContinuity(DirectedRoadNetworkGraph graph,string start)
    {
        if(graph.UnbranchedCorridors.TryGetValue(start,out var cached))return cached;
        var result=new HashSet<string>{start};
        foreach(bool forward in new[]{true,false})
        {
            var current=start;var items=new HashSet<string>{graph.Get(start).ItemUid};
            while(true)
            {
                var next=forward?graph.Next(current):graph.Previous(current);
                if(next.Length!=1)break;
                var p=graph.Get(next[0]);
                var opposite=forward?graph.Previous(p.Id):graph.Next(p.Id);
                var from=forward?graph.Get(current):p;var to=forward?p:graph.Get(current);
                bool aligned=from.Paths.All(a=>to.Paths.All(b=>
                    Vector3.Dot(Vector3.Normalize(NetworkPortion.V(a[^1])-NetworkPortion.V(a[^2])),
                        Vector3.Normalize(NetworkPortion.V(b[1])-NetworkPortion.V(b[0])))>.9));
                if(opposite.Length!=1||!aligned||p.Blockers.Any(b=>b!="LANE_WIDTH_4_5M_ASSUMED")||!items.Add(p.ItemUid))break;
                result.Add(p.Id);current=p.Id;
            }
        }
        return graph.UnbranchedCorridors[start]=result.Order(StringComparer.Ordinal).ToArray();
    }
    internal static bool BracketDirectionCompatible(DirectedRoadNetworkGraph graph,string start,string end)
    {
        var a=graph.Get(start).Paths[0];var b=graph.Get(end).Paths[0];
        Vector3 Direction(float[][] path)
        {
            var v=NetworkPortion.V(path[^1])-NetworkPortion.V(path[Math.Max(0,path.Length-4)]);v.Y=0;
            return v.LengthSquared()>.001?Vector3.Normalize(v):Vector3.Zero;
        }
        var from=Direction(a);var to=Direction(b);
        var delta=NetworkPortion.V(b[^1])-NetworkPortion.V(a[^1]);delta.Y=0;
        // Adjacent opposite carriageways and an aligned endpoint behind the
        // starting carriageway require a return loop, not forward confirmation.
        if(Vector3.Dot(from,to)<-.5)return false;
        return !(Vector3.Dot(from,to)>.85&&Vector3.Dot(delta,from)<-50||
            Vector3.Dot(delta,from)<-50&&Vector3.Dot(delta,to)<-50);
    }
    internal static string[]? UniquePath(DirectedRoadNetworkGraph graph,string start,string end,double limit)
    {
        if(start==end)return [start];
        var paths=new List<string[]>();int states=0;
        var pending=new Stack<(string[] Path,double Length)>();pending.Push(([start],graph.Get(start).LengthM));
        while(pending.Count>0)
        {
            if(++states>2000)return null; // Exhaustion is unresolved, never evidence of uniqueness.
            var (path,length)=pending.Pop();
            foreach(var n in graph.Next(path[^1]))
            {
                if(path.Contains(n))continue;
                double distance=length+graph.Get(n).LengthM;if(distance>limit)continue;
                if(path.Length>=128)return null; // A truncated alternative could conceal a second path.
                var extended=path.Append(n).ToArray();
                if(n==end){paths.Add(extended);if(paths.Count>1)return null;}else pending.Push((extended,distance));
            }
        }
        return paths.Count==1?paths[0]:null;
    }
    // A portion is mandatory only when removing it leaves no directed route (production uses the entire local graph).
    // Branches that diverge and reconverge therefore do not acquire a name just because both ends agree.
    internal static string[]? MandatoryCorridor(DirectedRoadNetworkGraph graph,string start,string end,double limit,bool singleRoadPass=false,CancellationToken token=default)
    {
        var key=(start,end,limit,singleRoadPass);
        if(graph.MandatoryCorridors.TryGetValue(key,out var cached))return cached;
        if(double.IsPositiveInfinity(limit))
        {
            if(!graph.Reachability.TryGetValue(start,out var search))graph.Reachability[start]=search=new(graph,start,token);
            if(search.Complete)return graph.MandatoryCorridors[key]=search.Corridor(graph,end,singleRoadPass);
        }
        return graph.MandatoryCorridors[key]=ComputeMandatoryCorridor(graph,start,end,limit,singleRoadPass,token);
    }
    private static string[]? ComputeMandatoryCorridor(DirectedRoadNetworkGraph graph,string start,string end,double limit,bool singleRoadPass,CancellationToken token)
    {
        var original=Shortest(null);if(original.Exhausted||original.Path is null)return null;
        // Driving out and back over the same physical Road is not a reference-continuity observation.
        // Reject this pair conservatively; do not select a different route to make it fit.
        if(singleRoadPass&&original.Path.Select(graph.Get).Where(p=>p.Kind=="ROAD").GroupBy(p=>p.ItemUid).Any(g=>g.Count()>1))return null;
        var mandatory=new List<string>{start};
        foreach(var candidate in original.Path.Skip(1).SkipLast(1))
        {
            if(double.IsPositiveInfinity(limit))
            {
                // An alternative only needs to exist; computing its shortest
                // distance for every excluded vertex needlessly re-sorts the map.
                var seen=new HashSet<string>{start};var pending=new Stack<string>();pending.Push(start);
                bool reachable=false;
                while(pending.TryPop(out var current))
                {
                    token.ThrowIfCancellationRequested();
                    if(current==end){reachable=true;break;}
                    if(seen.Count>10000)return null;
                    foreach(var next in graph.Next(current))if(next!=candidate&&seen.Add(next))pending.Push(next);
                }
                if(!reachable)mandatory.Add(candidate);
            }
            else
            {
                var alternative=Shortest(candidate);if(alternative.Exhausted)return null;
                if(alternative.Path is null)mandatory.Add(candidate);
            }
        }
        if(end!=start)mandatory.Add(end);
        return mandatory.ToArray();

        (string[]? Path,bool Exhausted) Shortest(string? excluded)
        {
            var distances=new Dictionary<string,double>{{start,graph.Length(start)}};
            var previous=new Dictionary<string,string>();var queue=new PriorityQueue<string,double>();queue.Enqueue(start,distances[start]);
            int states=0;
            while(queue.TryDequeue(out var current,out var distance))
            {
                token.ThrowIfCancellationRequested();
                if(distance>distances[current])continue;
                if(distance>limit)return(null,false);
                if(++states>10000)return(null,true);
                if(current==end)
                {
                    var path=new List<string>{end};while(previous.TryGetValue(path[^1],out var prior))path.Add(prior);
                    path.Reverse();return(path.ToArray(),false);
                }
                foreach(var next in graph.Next(current))
                {
                    if(next==excluded)continue;
                    var length=distance+graph.Length(next);
                    if(length>limit||distances.TryGetValue(next,out var best)&&best<=length)continue;
                    distances[next]=length;previous[next]=current;queue.Enqueue(next,length);
                }
            }
            return(null,false);
        }
    }
    public static RoadIdentitySnapshot Join(NamedNetworkSnapshot data,ProbeRoute probe,RouteCorridor corridor,LiveRoute route)
    {
        var spans=corridor.Segments.Select(s=>new RoadIdentitySpan($"segment:{s.SegmentIndex}",s.SegmentIndex,s.ItemUid.ToString("X16"),
            probe.Physical[s.SegmentIndex].NodeUid.ToString("X16"),probe.Physical[s.SegmentIndex+1].NodeUid.ToString("X16"),
            s.StartStationM,s.EndStationM,s.StartNavStationM,s.EndNavStationM,"UNKNOWN",[],[],"")).ToArray();
        return Join(data,spans,route);
    }
    internal static LiveRoute AttachSignTargets(NamedNetworkSnapshot data,LiveRoute route)
    {
        if(route.RoadIdentities is null)return route;
        var graph=new DirectedRoadNetworkGraph(data.Region,data.Portions,data.Connections,data.Errors);
        var names=data.Names.ToDictionary(n=>n.PortionId);
        return route with{Actions=route.Actions.Select(a=>
        {
            var span=route.RoadIdentities.Spans.Single(s=>s.SegmentIndex==a.SegmentIndex);
            var portion=DirectedRoadNetworkGraph.Id(span.ItemUid,span.EntryNodeUid,span.ExitNodeUid);
            var sources=data.Sources.Where(s=>s.Role=="GuideTo"&&s.ReadingConfidence is "MEDIUM" or "HIGH"&&
                s.AssociationStatus is "DIRECTIONAL_BRANCH_TARGET" or "ADVANCE_EXIT_BRANCH_TARGET"&&s.Owners.Contains(portion)).ToArray();
            var targets=sources.Select(s=>new LiveSignTarget(s.Identity,s.Cardinal,s.Destination,"MEDIUM",s.Id){Kind=s.Kind=="StreetName"?s.Kind:null}).ToList();
            // Every placement hypothesis must reach this exit; repetition must come from
            // different physical signs. This remains a provisional guide target, never a road name.
            var allowed=new HashSet<string>{"ARROW_TIP_NOT_MEASURED","BOARD_ATTACHMENT_UNCALIBRATED","BOARD_DRAWING_FRAME_UNCALIBRATED",
                "PLACEMENT_NOT_RENDER_VERIFIED","SUPPORT_DEFAULT_NOT_RENDER_VERIFIED","LANE_WIDTH_4_5M_ASSUMED"};
            foreach(var group in a.Proofs.Where(p=>p.Source=="SIGN_GROUP").Select(SignGroupProof.Read)
                .Where(g=>RoadIdentityRules.Reference(g.Road)&&g.Status=="GPS_COMPATIBLE_CANDIDATE"&&g.Fields.IdentityConfidence=="HIGH"&&
                    (g.Cardinal.Length==0||g.Fields.CardinalConfidence=="HIGH")&&g.Blockers.All(allowed.Contains)&&
                    g.ReachableExits.SequenceEqual(new[]{a.BranchId}))
                .GroupBy(g=>(g.Road,g.Cardinal,g.Destination)).Where(g=>g.Select(p=>p.SignUid).Distinct().Count()>=2))
                targets.Add(new(group.Key.Road,group.Key.Cardinal,group.Key.Destination,"MEDIUM",string.Join('|',group.Select(g=>g.GroupId).Order())){Kind=!RoadIdentityRules.Reference(group.Key.Road)?"StreetName":null});
            // A connector may have no own name. Resolve its destination through
            // native ports, stopping at the first choice, not by searching ahead
            // along the selected GPS route for a convenient matching inscription.
            var confirmed=ForwardTarget(graph,names,portion);
            var targetMethod="network-forward:";
            if(confirmed.Length==0)
            {
                var nextAction=route.Actions.Where(next=>next.SegmentIndex>a.SegmentIndex).Select(next=>next.SegmentIndex).DefaultIfEmpty(int.MaxValue).Min();
                confirmed=LegTarget(graph,names,route.RoadIdentities.Spans,a.SegmentIndex,nextAction);
                targetMethod="route-leg-forward:";
            }
            if(confirmed.Length==0)
            {
                confirmed=AccessTarget(graph,names,route,a.SegmentIndex);
                targetMethod="captured-access-forward:";
            }
            if(confirmed.Length>0)
            {
                targets=targets.Where(t=>!confirmed.Any(l=>l.Kind==(t.Kind=="StreetName"?"StreetName":"RouteReference"))||confirmed.Any(l=>l.Identity==t.Road&&
                    (l.Cardinal.Length==0||t.Cardinal.Length==0||l.Cardinal==t.Cardinal))).ToList();
                foreach(var label in confirmed.Where(l=>!targets.Any(t=>t.Road==l.Identity)))
                    targets.Add(new(label.Identity,label.Cardinal,"","MEDIUM",targetMethod+portion+"|"+string.Join('|',label.SourceIds)){Kind=label.Kind=="StreetName"?label.Kind:null});
            }
            return a with{SignTargets=targets.Count>0?targets.ToArray():null};
        }).ToArray()};
    }
    internal static NetworkIdentity[] ForwardTarget(DirectedRoadNetworkGraph graph,IReadOnlyDictionary<string,NamedNetworkPortion> names,string branch)
    {
        var current=branch;var items=new HashSet<string>{graph.Get(branch).ItemUid};double distance=0;
        for(int i=0;i<100;i++)
        {
            var next=graph.Next(current);if(next.Length!=1)return [];
            var p=graph.Get(next[0]);
            if(!items.Add(p.ItemUid)||p.Blockers.Any(b=>b!="LANE_WIDTH_4_5M_ASSUMED"))return [];
            if(names.TryGetValue(p.Id,out var named))
            {
                if(named.Status=="RESOLVED")return named.Identities;
                if(named.Status is "CONFLICT" or "AMBIGUOUS")return [];
            }
            distance+=p.LengthM;if(distance>2500)return [];
            current=p.Id;
        }
        return [];
    }
    internal static NetworkIdentity[] AccessTarget(DirectedRoadNetworkGraph graph,IReadOnlyDictionary<string,NamedNetworkPortion> names,
        LiveRoute route,int segment)
    {
        var spans=route.RoadIdentities!.Spans;var start=spans.Single(s=>s.SegmentIndex==segment);
        string Id(RoadIdentitySpan s)=>DirectedRoadNetworkGraph.Id(s.ItemUid,s.EntryNodeUid,s.ExitNodeUid);
        var current=Id(start);var seen=new HashSet<string>{start.EntryNodeUid,start.ExitNodeUid};
        foreach(var s in spans.Where(s=>s.SegmentIndex>segment).OrderBy(s=>s.SegmentIndex))
        {
            if(s.WorldEndM-start.WorldStartM>1500||!seen.Add(s.ExitNodeUid)||!graph.Next(current).Contains(Id(s)))return [];
            if(route.Actions.Any(a=>a.SegmentIndex>=segment+1&&a.SegmentIndex<=s.SegmentIndex&&
                (a.Instruction.Contains("TURN",StringComparison.Ordinal)||a.Instruction.StartsWith("SHARP_",StringComparison.Ordinal)||
                    a.Instruction=="ROUNDABOUT"||a.Instruction.StartsWith("EXIT",StringComparison.Ordinal))))return [];
            var named=names.GetValueOrDefault(Id(s));
            if(named?.Status is "CONFLICT" or "AMBIGUOUS")return [];
            if(named?.Status=="RESOLVED")return named.Identities;
            current=Id(s);
        }
        return [];
    }
    internal static NetworkIdentity[] LegTarget(DirectedRoadNetworkGraph graph,IReadOnlyDictionary<string,NamedNetworkPortion> names,
        RoadIdentitySpan[] spans,int segment,int nextActionSegment)
    {
        var start=spans.Single(s=>s.SegmentIndex==segment);
        var current=DirectedRoadNetworkGraph.Id(start.ItemUid,start.EntryNodeUid,start.ExitNodeUid);
        foreach(var s in spans.Where(s=>s.SegmentIndex>segment&&s.SegmentIndex<nextActionSegment).OrderBy(s=>s.SegmentIndex))
        {
            if(s.WorldStartM-start.WorldEndM>1500)return [];
            var id=DirectedRoadNetworkGraph.Id(s.ItemUid,s.EntryNodeUid,s.ExitNodeUid);
            if(!graph.Next(current).Contains(id))return [];
            if(names.TryGetValue(id,out var named))
            {
                if(named.Status=="RESOLVED")return named.Identities;
                if(named.Status is "CONFLICT" or "AMBIGUOUS")return [];
            }
            current=id;
        }
        // This supplies only a maneuver destination along the captured leg.
        // No portion is renamed, and no later maneuver is skipped.
        return [];
    }
    internal static RoadIdentitySnapshot Join(NamedNetworkSnapshot data,RoadIdentitySpan[] input,LiveRoute route)
    {
        var anchors=new List<RoadIdentityAnchor>();
        var sources=data.Sources.ToDictionary(s=>s.Id);var names=data.Names.ToDictionary(n=>n.PortionId);
        var spans=new List<RoadIdentitySpan>();
        foreach(var s in input)
        {
            var entry=s.EntryNodeUid;var exit=s.ExitNodeUid;var uid=s.ItemUid;
            var id=DirectedRoadNetworkGraph.Id(uid,entry,exit);var named=names.GetValueOrDefault(id);
            SegmentRoadIdentity ConvertLabel(NetworkIdentity l)
            {
                var ids=new List<string>();
                foreach(var sourceId in l.SourceIds)
                {
                    var source=sources[sourceId];var proofId=$"network:{source.Id}:{s.SegmentIndex}";ids.Add(proofId);
                    if(anchors.Any(a=>a.Id==proofId))continue;
                    // The proof is attached to this exact directed occurrence, its original owners remain in the saved network.
                    anchors.Add(new(proofId,source.Source,source.Id,source.Raw,source.Identity,source.Kind,source.Cardinal,source.ReadingConfidence,
                        l.Rule,l.Confidence,uid,s.SegmentIndex,(s.WorldStartM+s.WorldEndM)/2,(s.NavStartM+s.NavEndM)/2,
                        source.Alternatives.Select(a=>new RoadOwnershipCandidate(data.Portions.First(p=>p.Id==a.PortionId).ItemUid,a.DistanceM,a.HeightM,a.Status+":"+a.PortionId)).ToArray()));
                }
                return new(l.Identity,l.Kind,l.Cardinal,l.Confidence,l.CardinalConfidence,ids.ToArray(),l.Rule);
            }
            spans.Add(new($"segment:{s.SegmentIndex}",s.SegmentIndex,uid,entry,exit,s.WorldStartM,s.WorldEndM,s.NavStartM,s.NavEndM,
                named?.Status??"UNKNOWN",named?.Identities.Select(ConvertLabel).ToArray()??[],named?.Candidates.Select(ConvertLabel).ToArray()??[],
                named is null?"DIRECTED_PORTION_NOT_RESOLVED":id));
        }
        var proofAnchors=anchors.OrderBy(a=>a.Id).ToArray();
        var occurrences=CapturedRoadContinuity.Fill(new(data.Region,data.Portions,data.Connections,data.Errors),spans.ToArray(),proofAnchors,route);
        return new(1,data.Fingerprint,proofAnchors,occurrences,RouteConnectionTargets.Build(new(data.Region,data.Portions,data.Connections,data.Errors),route,occurrences));
    }
}

