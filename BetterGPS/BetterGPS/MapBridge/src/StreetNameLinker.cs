using System.Numerics;
using System.Text.RegularExpressions;

namespace BetterGPSNavigation;

// Lateral street-name blades name the parallel axis; generic overhead information
// boards name the axis facing their inscription. Associate native junction arms,
// not whichever lane happens to pass closest underneath a plate.
internal static class StreetNameLinker
{
    internal static string Canonical(string name)=>Regex.Replace(SignRoadIdentityResolver.NormalizeText(name),
        @"\b(?:STREET|ST|ROAD|RD|AVENUE|AVE|BOULEVARD|BLVD|BL|DRIVE|DR|HIGHWAY|HWY|LANE|LN|COURT|CT|PARKWAY|PKWY|CIRCLE|CIR|PLACE|PL|TERRACE|TER|TRAIL|TRL)\.?$",m=>m.Value.TrimEnd('.').ToUpperInvariant() switch{
            "STREET" or "ST"=>"St","ROAD" or "RD"=>"Rd","AVENUE" or "AVE"=>"Ave","BOULEVARD" or "BLVD" or "BL"=>"Blvd","DRIVE" or "DR"=>"Dr",
            "LANE" or "LN"=>"Ln","COURT" or "CT"=>"Ct","PARKWAY" or "PKWY"=>"Pkwy","CIRCLE" or "CIR"=>"Cir","PLACE" or "PL"=>"Pl",
            "TERRACE" or "TER"=>"Ter","TRAIL" or "TRL"=>"Trl",_=>"Hwy"},RegexOptions.IgnoreCase);
    internal static string[] IntersectionHeaders(ExtractedSignPanel panel)
    {
        // Local intersection boards can put the cross-street above a divider
        // and landmark directions below it. A landmark arrow does not belong
        // to that header, nor does its missing arrow erase the street name.
        if(!panel.BoardPath.Contains("_int_",StringComparison.OrdinalIgnoreCase))return [];
        var groups=SignGroupResolver.Resolve(panel);
        if(groups.Count<2||groups.Any(g=>g.Method!="EXPLICIT_HORIZONTAL_DIVIDER"))return [];
        var arrows=panel.Objects.Where(o=>o.Visible&&o.Reliable&&o.Arrow.Length>0).ToArray();
        if(arrows.Length==0)return [];
        double Coordinate(EffectiveSignObject o,string key)=>double.TryParse(o.Effective.GetValueOrDefault(key),
            System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out var v)?v:double.NaN;
        var result=new List<string>();
        foreach(var group in groups.Where(g=>g.ArrowObjectIds.Count==0))
        {
            var objects=panel.Objects.Where(o=>o.Visible&&group.ObjectIds.Contains(o.Id)).ToArray();
            if(objects.Length!=1||objects[0] is not {Kind:"sign_template_text",Reliable:true,SchemaStatus:"SUPPORTED"})continue;
            var name=SignRoadIdentityResolver.NormalizeText(objects[0].Text);
            if(SignGroupResolver.IsSignedStreetName(name)&&arrows.All(a=>Coordinate(objects[0],"coords_b")>Coordinate(a,"coords_t")))result.Add(Canonical(name));
        }
        return result.Distinct().ToArray();
    }
    internal static bool IsStreetPlate(ExtractedSignPanel panel,string definition)=>
        panel.BoardPath.StartsWith("/def/sign/board/info/us_streetname_",StringComparison.OrdinalIgnoreCase)||
        Regex.IsMatch(definition,@"\bstreet\s+name\b",RegexOptions.IgnoreCase)||
        Regex.IsMatch(panel.BoardPath,@"^/def/sign/board/info/hwy_info_1(?:a|_b)?\.sii$",RegexOptions.IgnoreCase)&&
        Names(panel) is {Length:1};
    internal static bool IsInformationPlate(ExtractedSignPanel panel)=>Regex.IsMatch(panel.BoardPath,
        @"^/def/sign/board/info/hwy_info_1(?:a|_b)?\.sii$",RegexOptions.IgnoreCase);
    internal static bool OverheadInformationPlate(DirectedRoadNetworkGraph graph,ExtractedSignPanel panel,Vector3 position)=>
        panel is {Status:"OK",SchemaValidated:true,FollowRoadDir:false}&&panel.BoardOffset.All(v=>Math.Abs(v)<1e-6)&&
        InformationFamily(panel)=="SUSPENDED_INFORMATION_PLATE"&&NamedRoadNetwork.Associate(graph,position,null,"CrossStreet","")
            .Where(a=>a.Status=="CANDIDATE"&&a.DistanceM<=25&&graph.Get(a.PortionId).Kind=="ROAD")
            .OrderBy(a=>a.DistanceM).FirstOrDefault() is {HeightM:>=4 and <=12};
    // The board template is reused on several supports. Height alone cannot
    // establish which street a plate names (bridges and mod supports included).
    // This family is backed by the inspected mounted mesh and the reported
    // L/9th junction. New families remain explicit candidates until calibrated.
    internal static string InformationFamily(ExtractedSignPanel panel)=>!IsInformationPlate(panel)?"LATERAL_STREET_PLATE":
        Regex.IsMatch(panel.SupportModel,@"^/model/sign/navigation/tx_side_board_(?:green|brown)_\d+_\d+_\d+x\d+\.pmd$",RegexOptions.IgnoreCase)?
            "SUSPENDED_INFORMATION_PLATE":"UNCLASSIFIED_INFORMATION_SUPPORT";
    internal static NetworkNameSource Confirmation(DirectedRoadNetworkGraph graph,NetworkNameSource source)
    {
        return Associate(graph,source with{Role="CrossStreet",Owners=[],AssociationStatus="ROLE_NEEDS_BRANCH_OR_CROSS_STREET_LINK"});
    }
    internal static NetworkNameSource InterpretedAxis(DirectedRoadNetworkGraph graph,NetworkNameSource source)
    {
        var result=Associate(graph,source with{Source=source.StreetAxisMode=="FACING"?"STREET_CONFIRMATION_SIGN":"STREET_NAME_SIGN",
            Role="CrossStreet",Owners=[],AssociationStatus="ROLE_NEEDS_BRANCH_OR_CROSS_STREET_LINK"});
        return result with{Source=source.Source};
    }
    // A generic information board can name either axis. Resolve this only from
    // independent inscriptions on the native graph, never the selected GPS arm.
    // Freeze first-pass names so two uncertain panels cannot justify each other.
    internal static NetworkNameSource InterpretWithIndependentNames(DirectedRoadNetworkGraph graph,NetworkNameSource source,
        NetworkNameSource[] sources,NamedNetworkPortion[] names)
        =>InterpretWithIndependentNames(graph,source,sources.ToDictionary(s=>s.Id),names.ToDictionary(n=>n.PortionId));
    internal static NetworkNameSource InterpretWithIndependentNames(DirectedRoadNetworkGraph graph,NetworkNameSource source,
        IReadOnlyDictionary<string,NetworkNameSource> proofs,IReadOnlyDictionary<string,NamedNetworkPortion> lookup)
    {
        if(source.Normal is null||!SignGroupResolver.IsSignedStreetName(source.Identity))return source;
        var options=new[]{"LATERAL","FACING"}.Select(mode=>
        {
            var axis=InterpretedAxis(graph,source with{StreetAxisMode=mode});
            var evidence=axis.Owners.SelectMany(id=>lookup[id].Identities).Where(l=>l.Kind=="StreetName"&&
                l.SourceIds.All(id=>proofs[id].ObjectId!=source.ObjectId&&proofs[id].Source!="CORROBORATED_STREET_INTERPRETATION")).ToArray();
            return new{Mode=mode,Axis=axis,Blocked=evidence.Any(l=>l.Identity!=source.Identity),
                Matched=evidence.Any(l=>l.Identity==source.Identity),Evidence=evidence};
        }).ToArray();
        var possible=options.Where(o=>o.Axis.Owners.Length>0&&!o.Blocked).ToArray();
        if(possible.Length!=1||!possible[0].Matched&&!options.Any(o=>o.Blocked))return source;
        var chosen=possible[0];
        return chosen.Axis with{Source="CORROBORATED_STREET_INTERPRETATION",StreetAxisMode=chosen.Mode,
            InterpretationStatus=chosen.Matched?"INDEPENDENT_SAME_STREET_AXIS":"INDEPENDENT_OTHER_STREET_AXIS_EXCLUSION",
            InterpretationProofIds=options.SelectMany(o=>o.Evidence.SelectMany(l=>l.SourceIds)).Distinct().Order().ToArray()};
    }
    internal static string[] Names(ExtractedSignPanel panel)
    {
        if(panel.Objects.Any(o=>o.Visible&&o.Arrow.Length>0))return [];
        var text=panel.Objects.Where(o=>o.Visible&&o.Reliable&&o.Kind=="sign_template_text").Select(o=>SignRoadIdentityResolver.NormalizeText(o.Text))
            .Where(s=>RoadIdentityRules.Street(s)&&!Regex.IsMatch(s,@"^(?:X+|CITY OF\b.*|\d+\s+(?:EAST|WEST|NORTH|SOUTH)|NORTH|SOUTH|EAST|WEST)$",RegexOptions.IgnoreCase))
            .Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        bool Suffix(string s)=>Regex.IsMatch(s,@"^(?:ST|STREET|RD|ROAD|AVE|AVENUE|BL|BLVD|BOULEVARD|DR|DRIVE|LN|LANE|HWY|HIGHWAY|WAY|CT|COURT|PKWY|PARKWAY|CIRCLE|CIR|PLACE|PL|TERRACE|TER|TRAIL|TRL)\.?$",RegexOptions.IgnoreCase);
        var names=text.Where(s=>!Suffix(s)).ToArray();var suffixes=text.Where(Suffix).ToArray();
        if(names.Length!=1||suffixes.Length>1)return [];
        var name=names[0]+(suffixes.Length==1?" "+suffixes[0]:"");
        name=Regex.Replace(name,@"\b(\d+)\s+(ST|ND|RD|TH)\b","$1$2",RegexOptions.IgnoreCase);
        if(name==name.ToUpperInvariant())name=System.Globalization.CultureInfo.InvariantCulture.TextInfo.ToTitleCase(name.ToLowerInvariant());
        name=Regex.Replace(name,@"\b(\d+)(ST|ND|RD|TH)\b",m=>m.Groups[1].Value+m.Groups[2].Value.ToLowerInvariant(),RegexOptions.IgnoreCase);
        return [Canonical(name)];
    }
    internal static NetworkNameSource Associate(DirectedRoadNetworkGraph graph,NetworkNameSource source)
    {
        if(source.Normal is null)return source;
        var normal=NetworkPortion.V(source.Normal);normal.Y=0;
        if(normal.LengthSquared()<.8)return source;
        normal=Vector3.Normalize(normal);
        bool Axis(float[][] path,bool entry)=>Parallel(path,entry,normal,source.Source=="STREET_CONFIRMATION_SIGN");
        var nearby=source.Alternatives.Where(a=>a.DistanceM<=55&&a.HeightM<=12&&a.Status=="CANDIDATE")
            .Where(a=>graph.Get(a.PortionId).Kind=="PREFAB")
            .GroupBy(a=>graph.Get(a.PortionId).ItemUid)
            .Where(g=>g.SelectMany(a=>new[]{graph.Get(a.PortionId).EntryNodeUid,graph.Get(a.PortionId).ExitNodeUid}).Distinct().Count()>=3)
            .OrderBy(g=>g.Min(a=>a.DistanceM)).ToArray();
        if(nearby.Length==0)return source;
        // A city intersection can consist of several directly connected prefabs.
        // Treat only native shared-port neighbours as one junction; proximity
        // alone must not merge two separate intersections.
        var cluster=new HashSet<string>{nearby[0].Key};
        var candidates=source.Alternatives.Where(a=>a.DistanceM<=55&&a.HeightM<=12&&a.Status=="CANDIDATE")
            .Select(a=>graph.Get(a.PortionId)).Where(p=>p.Kind=="PREFAB").Select(p=>p.ItemUid).Distinct().ToArray();
        var junctionPorts=candidates.ToDictionary(id=>id,id=>graph.Portions.Where(p=>p.ItemUid==id)
            .SelectMany(p=>new[]{p.EntryNodeUid,p.ExitNodeUid}).ToHashSet());
        bool changed;
        do
        {
            changed=false;
            foreach(var id in candidates.Where(id=>!cluster.Contains(id)))
                if(cluster.Any(other=>junctionPorts[id].Overlaps(junctionPorts[other]))){cluster.Add(id);changed=true;}
        }while(changed);
        if(nearby.Skip(1).Any(g=>!cluster.Contains(g.Key)&&g.Min(a=>a.DistanceM)<nearby[0].Min(a=>a.DistanceM)+8))return source;
        var junction=graph.Portions.Where(p=>cluster.Contains(p.ItemUid)).ToArray();
        var ports=junction.SelectMany(p=>new[]{p.EntryNodeUid,p.ExitNodeUid}).ToHashSet();
        var arms=graph.Portions.Where(p=>p.Kind=="ROAD"&&p.Blockers.All(b=>b=="LANE_WIDTH_4_5M_ASSUMED")&&
            (ports.Contains(p.EntryNodeUid)||ports.Contains(p.ExitNodeUid)))
            .Where(p=>p.Paths.All(path=>Axis(path,ports.Contains(p.EntryNodeUid)))).ToArray();
        // One physical arm on either side is permitted. Three arms or competing parallel
        // carriageways require a more specific plate and stay unresolved.
        var armPorts=arms.SelectMany(p=>new[]{p.EntryNodeUid,p.ExitNodeUid}).Where(ports.Contains).Distinct().ToArray();
        if(armPorts.Length is <1 or >2)return source;
        if(armPorts.Any(port=>arms.Where(p=>p.EntryNodeUid==port||p.ExitNodeUid==port).Select(p=>p.ItemUid).Distinct().Count()!=1))return source;
        var owners=arms.Select(p=>p.Id).ToHashSet();
        foreach(var p in junction.Where(p=>armPorts.Contains(p.EntryNodeUid)&&armPorts.Contains(p.ExitNodeUid)&&
            p.Paths.All(path=>Axis(path,true)&&Axis(path,false))))owners.Add(p.Id);
        return source with{Role="StreetAxis",AssociationStatus="JUNCTION_STREET_AXIS",Owners=owners.Order(StringComparer.Ordinal).ToArray()};
    }
    // Opposed arrows on separate street-name rows identify different arms,
    // including a name change across a straight intersection. They do not name
    // a remote turn found by extending the board's facing direction.
    internal static NetworkNameSource DirectionalArm(DirectedRoadNetworkGraph graph,NetworkNameSource source)
    {
        var unresolved=source with{Role="GuideTo",Owners=[],AssociationStatus="ROLE_NEEDS_BRANCH_OR_CROSS_STREET_LINK"};
        if(source.Normal is null||source.Arrow is not ("LEFT" or "RIGHT"))return unresolved;
        var face=NetworkPortion.V(source.Normal);face.Y=0;
        if(face.LengthSquared()<.8)return unresolved;
        var forward=-Vector3.Normalize(face);
        var outward=new Vector3(-forward.Z,0,forward.X)*(source.Arrow=="RIGHT"?1:-1);
        var choices=source.Alternatives.Where(a=>a.Status=="CANDIDATE"&&a.DistanceM<=110&&a.HeightM<=12)
            .Where(a=>graph.Get(a.PortionId).Kind=="PREFAB").GroupBy(a=>graph.Get(a.PortionId).ItemUid)
            .Where(g=>g.SelectMany(a=>new[]{graph.Get(a.PortionId).EntryNodeUid,graph.Get(a.PortionId).ExitNodeUid}).Distinct().Count()>=3)
            .OrderBy(g=>g.Min(a=>a.DistanceM)).ToArray();
        if(choices.Length==0)return unresolved;
        // Divided local junctions are assembled from several directly joined
        // prefabs. Stopping at the first piece mistakes a central link for an
        // outgoing street and misses the opposite carriageway.
        var nearbyItems=source.Alternatives.Where(a=>a.Status=="CANDIDATE"&&a.DistanceM<=110&&a.HeightM<=12)
            .Select(a=>graph.Get(a.PortionId)).Where(p=>p.Kind=="PREFAB").Select(p=>p.ItemUid).ToHashSet();
        var pieces=graph.Portions.Where(p=>nearbyItems.Contains(p.ItemUid)).GroupBy(p=>p.ItemUid)
            .Where(g=>g.SelectMany(p=>new[]{p.EntryNodeUid,p.ExitNodeUid}).Distinct().Count()>2||g.All(p=>p.LengthM<=25))
            .ToDictionary(g=>g.Key,g=>g.SelectMany(p=>new[]{p.EntryNodeUid,p.ExitNodeUid}).ToHashSet());
        var cluster=new HashSet<string>{choices[0].Key};bool changed;
        do
        {
            changed=false;
            foreach(var item in pieces.Keys.Where(id=>!cluster.Contains(id)))
                if(cluster.Any(id=>pieces[id].Overlaps(pieces[item]))){cluster.Add(item);changed=true;}
        }while(changed);
        if(choices.Skip(1).Any(g=>!cluster.Contains(g.Key)&&g.Min(a=>a.DistanceM)<choices[0].Min(a=>a.DistanceM)+8))return unresolved;
        var junction=graph.Portions.Where(p=>cluster.Contains(p.ItemUid)).ToArray();
        var ports=junction.SelectMany(p=>new[]{p.EntryNodeUid,p.ExitNodeUid}).ToHashSet();
        var twoPort=graph.Portions.Where(p=>p.Kind=="PREFAB").GroupBy(p=>p.ItemUid)
            .Where(g=>g.SelectMany(p=>new[]{p.EntryNodeUid,p.ExitNodeUid}).Distinct().Count()==2).Select(g=>g.Key).ToHashSet();
        var portPoints=junction.SelectMany(p=>p.Paths.SelectMany(path=>new[]{(Node:p.EntryNodeUid,Point:NetworkPortion.V(path[0])),
            (Node:p.ExitNodeUid,Point:NetworkPortion.V(path[^1]))})).GroupBy(x=>x.Node)
            .Select(g=>g.Select(x=>x.Point).Aggregate(Vector3.Zero,(a,b)=>a+b)/g.Count()).ToArray();
        var centre=portPoints.Aggregate(Vector3.Zero,(a,b)=>a+b)/portPoints.Length;
        // An arm may start with a two-port prefab (crosswalk, curve or entrance),
        // rather than a Road item. Its native ports still define the same arm.
        var arms=graph.Portions.Where(p=>!cluster.Contains(p.ItemUid)&&p.Blockers.All(b=>b=="LANE_WIDTH_4_5M_ASSUMED")&&
            (p.Kind=="ROAD"||twoPort.Contains(p.ItemUid))&&
            (ports.Contains(p.EntryNodeUid)^ports.Contains(p.ExitNodeUid)))
            .Where(p=>p.Paths.All(path=>
            {
                bool starts=ports.Contains(p.EntryNodeUid);
                var port=NetworkPortion.V(starts?path[0]:path[^1]);
                if(Vector3.Dot(port-centre,outward)<0)return false;
                var d=starts?NetworkPortion.V(path[Math.Min(2,path.Length-1)])-NetworkPortion.V(path[0]):
                    NetworkPortion.V(path[Math.Max(0,path.Length-3)])-NetworkPortion.V(path[^1]);
                d.Y=0;return d.LengthSquared()>.001&&Vector3.Dot(Vector3.Normalize(d),outward)>.6;
            })).ToArray();
        if(arms.Length==0||arms.Select(p=>p.ItemUid).Distinct().Count()!=1)return unresolved;
        var armPorts=arms.SelectMany(p=>new[]{p.EntryNodeUid,p.ExitNodeUid}).Where(ports.Contains).ToHashSet();
        // Only traversals leaving through the named arm inherit its name inside
        // the intersection; the reverse traversal leaves through another arm.
        var owners=arms.Select(p=>p.Id).Concat(junction.Where(p=>armPorts.Contains(p.ExitNodeUid)).Select(p=>p.Id));
        return source with{Role="StreetAxis",AssociationStatus="JUNCTION_STREET_AXIS",Owners=owners.Distinct().Order(StringComparer.Ordinal).ToArray()};
    }
    private static bool Parallel(float[][] path,bool entry,Vector3 normal,bool alongNormal=false)
    {
        if(path.Length<2)return false;
        var delta=entry?NetworkPortion.V(path[Math.Min(2,path.Length-1)])-NetworkPortion.V(path[0]):
            NetworkPortion.V(path[^1])-NetworkPortion.V(path[Math.Max(0,path.Length-3)]);
        delta.Y=0;
        return delta.LengthSquared()>.001&&(alongNormal?Math.Abs(Vector3.Dot(Vector3.Normalize(delta),normal))>.9:
            Math.Abs(Vector3.Dot(Vector3.Normalize(delta),normal))<.35);
    }
}
