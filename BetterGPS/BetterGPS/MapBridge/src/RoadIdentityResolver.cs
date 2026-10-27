using System.Numerics;
using System.Text.Json;
using System.Text.RegularExpressions;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal sealed class RoadIdentityResolver(RoadNetworkIndex network,SignRoadIdentityResolver signs,
    RoadOverlayResolver overlays,SignPanelExtractor panels,string fingerprint)
{
    public RoadIdentitySnapshot Build(ProbeRoute probe,RouteCorridor corridor,LiveRoute route,V09SemanticRun run)
    {
        var anchors=new List<RoadIdentityAnchor>();
        foreach(var o in overlays.Overlays.Where(o=>o.Identity.Length>0&&
            o.Position.X>=corridor.MinX-200&&o.Position.X<=corridor.MaxX+200&&o.Position.Z>=corridor.MinZ-200&&o.Position.Z<=corridor.MaxZ+200))
            Add(o.Uid.ToString("X16"),"ROAD_OVERLAY",o.RawToken,o.Identity,"RouteReference","",o.Position,"MEDIUM");
        var seen=new HashSet<ulong>();
        foreach(var segment in corridor.Segments)
        foreach(var edge in corridor.Edges.Where(e=>e.SegmentIndex==segment.SegmentIndex))
        foreach(var sign in signs.QueryAll(edge.Start,190))
        {
            if(!seen.Add(sign.Uid))continue;
            var raw=string.Join(" | ",sign.RawStrings);
            // Default template shields may carry no per-item override text and are absent from the old semantic-only index.
            foreach(var area in panels.Areas(sign).Where(a=>a.Extraction is {Status:"OK"} p&&
                p.BoardPath.StartsWith("/def/sign/board/shield/",StringComparison.OrdinalIgnoreCase)&&
                a.RoadShields.Count==1&&a.ArrowTokens.Count==0&&a.Destinations.Count==0))
            {
                var field=SignSemanticValidator.Validate(area,null,area.RoadShields[0],area.Cardinals.Count==1?area.Cardinals[0]:"",signs.IsKnownDestination);
                var text=string.Join(" | ",area.RawStrings);
                if(RoadIdentityRules.ReferenceOnly(text))Add(sign.Uid.ToString("X16")+":"+area.AreaName,"CONFIRMATION_SHIELD",text,
                    area.RoadShields[0],"RouteReference",field.CardinalConfidence=="HIGH"&&area.Cardinals.Count==1?area.Cardinals[0]:"",
                    sign.Position,field.IdentityConfidence=="HIGH"?"HIGH":"MEDIUM");
            }
            bool confirmation=Confirmation(sign);
            if(confirmation&&sign.RoadShields.Count==1&&RoadIdentityRules.ReferenceOnly(raw))
            {
                var areas=sign.Areas.Where(a=>a.RoadShields.Count==1&&a.RoadShields[0]==sign.RoadShields[0]).ToArray();
                var cardinals=areas.SelectMany(a=>a.Cardinals).Distinct().ToArray();
                Add(sign.Uid.ToString("X16"),"CONFIRMATION_SIGN",raw,sign.RoadShields[0],"RouteReference",
                    cardinals.Length==1?cardinals[0]:"",sign.Position,"MEDIUM");
            }
            // Only a model definition explicitly identifying a street-name sign can supply this role.
            if(Regex.IsMatch(sign.DefinitionName,@"\bstreet\s+name\b",RegexOptions.IgnoreCase)&&
                sign.RoadShields.Count==0&&!sign.Areas.Any(a=>a.ArrowTokens.Count>0))
                foreach(var name in sign.RawStrings.Where(RoadIdentityRules.Street).Distinct())
                    Add(sign.Uid.ToString("X16")+":"+name,"STREET_NAME_SIGN",name,name,"StreetName","",sign.Position,"MEDIUM");
        }
        // Calibrated selected groups are independently associated anchors, never a reason to upgrade other observations.
        foreach(var a in route.Actions)
        foreach(var g in a.Proofs.Where(p=>p.Source=="SIGN_GROUP").Select(SignGroupProof.Read)
            .Where(g=>g.Confidence=="HIGH"&&g.Fields.IdentityConfidence=="HIGH"&&RoadIdentityRules.Reference(g.Road)))
        {
            var outgoing=corridor.Segments.FirstOrDefault(s=>s.SegmentIndex==a.SegmentIndex+1);
            if(outgoing is null)continue;
            anchors.Add(new(g.GroupId,"SIGN_GROUP",g.GroupId,g.Road,g.Road,"RouteReference",
                g.Fields.CardinalConfidence=="HIGH"?g.Cardinal:"","HIGH","VERIFIED_BRANCH","HIGH",outgoing.ItemUid.ToString("X16"),
                outgoing.SegmentIndex,outgoing.StartStationM,outgoing.StartNavStationM,[]));
        }
        var spans=Solve(corridor,probe,anchors.ToArray(),run.Events);
        return new(1,fingerprint,anchors.OrderBy(a=>a.WorldStationM).ThenBy(a=>a.Id,StringComparer.Ordinal).ToArray(),spans,
            Contexts(route,spans,run.Events));

        void Add(string uid,string source,string raw,string identity,string kind,string cardinal,Vector3 position,string reading)
        {
            var alternatives=network.Query(position);var association=RoadNetworkIndex.Associate(alternatives);
            var projection=corridor.Project(position);
            if(projection is null||projection.LateralM>180)return;
            if(association.Status=="AMBIGUOUS_ROADS")
            {
                // Retain every plausible owner; none is chosen because it occurs in the GPS route.
                var possible=alternatives.Where(c=>c.DistanceM<=alternatives[0].DistanceM+15).Select(c=>c.ItemUid).ToHashSet();
                foreach(var candidate in corridor.Segments.Where(s=>possible.Contains(s.ItemUid.ToString("X16"))))
                {
                    var part=new RouteCorridor(corridor.Edges.Where(e=>e.SegmentIndex==candidate.SegmentIndex).ToArray(),[candidate]).Project(position);
                    if(part is not null)anchors.Add(new(uid+":"+source+":"+candidate.SegmentIndex,source,uid,raw,identity,kind,cardinal,reading,
                        "AMBIGUOUS_ROADS","LOW",candidate.ItemUid.ToString("X16"),candidate.SegmentIndex,part.StationM,part.NavStationM,alternatives));
                }
                if(anchors.Any(a=>a.SourceId==uid&&a.Source==source))return;
            }
            var owners=corridor.Segments.Where(s=>s.ItemUid.ToString("X16")==association.Owner).ToArray();
            if(owners.Length==0||association.Owner.Length==0)
            {
                anchors.Add(new(uid+":"+source,source,uid,raw,identity,kind,cardinal,reading,
                    association.Owner.Length>0?"OTHER_ROAD":association.Status,association.Confidence,association.Owner,
                    -1,projection.StationM,projection.NavStationM,alternatives));return;
            }
            foreach(var s in owners)
            {
                var part=new RouteCorridor(corridor.Edges.Where(e=>e.SegmentIndex==s.SegmentIndex).ToArray(),[s]).Project(position);
                if(part is null)continue;
                anchors.Add(new(uid+":"+source+":"+s.SegmentIndex,source,uid,raw,identity,kind,cardinal,reading,
                    association.Status,association.Confidence,association.Owner,s.SegmentIndex,part.StationM,part.NavStationM,alternatives));
            }
        }
    }
    internal static bool Confirmation(ParsedSign sign)
    {
        var description=sign.DefinitionName+" "+sign.ModelDesc;
        return (sign.Model=="rot_bd_44"||Regex.IsMatch(description,@"shield.*(?:single|confirmation|reassurance)|(?:confirmation|reassurance).*shield",RegexOptions.IgnoreCase))&&
            !Regex.IsMatch(description,@"exit|junction|guide|ovh|overhead",RegexOptions.IgnoreCase)&&
            !sign.Areas.Any(a=>a.ArrowTokens.Count>0);
    }
    internal static RoadIdentitySpan[] Solve(RouteCorridor corridor,ProbeRoute probe,RoadIdentityAnchor[] anchors,
        IReadOnlyList<V09ManeuverEvent> events)
    {
        var spans=new List<RoadIdentitySpan>();
        var possibleAnchors=anchors.Where(a=>a.SegmentIndex>=0&&a.AssociationConfidence is "HIGH" or "MEDIUM" or "LOW").ToArray();
        var usable=possibleAnchors.Where(a=>a.AssociationConfidence is "HIGH" or "MEDIUM").ToArray();
        foreach(var s in corridor.Segments)
        {
            var direct=usable.Where(a=>a.SegmentIndex==s.SegmentIndex).ToArray();
            var identities=new List<SegmentRoadIdentity>();var candidates=new List<SegmentRoadIdentity>();
            foreach(var group in direct.GroupBy(a=>(a.Identity,a.Kind)))
                candidates.Add(Label(group.ToArray(),"DIRECT_ANCHOR",false));
            // Multiple labels on a road are candidates until each has an independent association proof.
            if(candidates.Count==1)identities.Add(candidates[0]);
            else if(candidates.Count>1&&direct.All(a=>a.AssociationConfidence=="HIGH"))identities.AddRange(candidates);
            if(direct.Length==0)
            {
                var middle=(s.StartStationM+s.EndStationM)/2;
                var prev=usable.Where(a=>a.WorldStationM<=middle).OrderByDescending(a=>a.WorldStationM).FirstOrDefault();
                var next=usable.Where(a=>a.WorldStationM>=middle).OrderBy(a=>a.WorldStationM).FirstOrDefault();
                if(prev is not null&&next is not null&&prev.SourceId.Split(':')[0]!=next.SourceId.Split(':')[0]&&prev.Identity==next.Identity&&prev.Kind==next.Kind&&
                    next.WorldStationM-prev.WorldStationM<=5000&&Connected(probe,prev.SegmentIndex,next.SegmentIndex)&&
                    !events.Any(e=>e.SegmentIndex>=prev.SegmentIndex&&e.SegmentIndex<next.SegmentIndex&&HardBoundary(e.Instruction)))
                    identities.Add(Label([prev,next],"BRACKETED_CONNECTED_ANCHORS",true));
                else
                {
                    var nearest=new[]{prev,next}.Where(a=>a is not null).Cast<RoadIdentityAnchor>()
                        .OrderBy(a=>Math.Abs(a.WorldStationM-middle)).FirstOrDefault();
                    if(nearest is not null&&Math.Abs(nearest.WorldStationM-middle)<=1000&&Connected(probe,Math.Min(s.SegmentIndex,nearest.SegmentIndex),Math.Max(s.SegmentIndex,nearest.SegmentIndex))&&
                        !events.Any(e=>e.SegmentIndex>=Math.Min(s.SegmentIndex,nearest.SegmentIndex)&&e.SegmentIndex<Math.Max(s.SegmentIndex,nearest.SegmentIndex)&&
                            (HardBoundary(e.Instruction)||e.Instruction.StartsWith("KEEP",StringComparison.Ordinal)||e.Instruction.Contains("FORK",StringComparison.Ordinal))))
                        candidates.Add(Label([nearest],"SINGLE_ANCHOR_LIMITED_INFERENCE",false) with{IdentityConfidence="LOW",Cardinal="",CardinalConfidence="NONE"});
                }
            }
            // Ambiguous annotations remain hypotheses over their plausible owners, even after corroboration.
            if(identities.Count==0)
            foreach(var group in possibleAnchors.Where(a=>a.AssociationConfidence=="LOW").GroupBy(a=>(a.Identity,a.Kind)))
            {
                var ordered=group.OrderBy(a=>a.WorldStationM).ToArray();
                var local=ordered.Where(a=>a.SegmentIndex==s.SegmentIndex).ToArray();
                if(local.Length>0)candidates.Add(Label(local,"AMBIGUOUS_OWNER",false));
                else
                {
                    var prev=ordered.LastOrDefault(a=>a.WorldStationM<=s.StartStationM);
                    var next=ordered.FirstOrDefault(a=>a.WorldStationM>=s.EndStationM);
                    if(prev is not null&&next is not null&&prev.SourceId.Split(':')[0]!=next.SourceId.Split(':')[0]&&
                        next.WorldStationM-prev.WorldStationM<=5000&&Connected(probe,prev.SegmentIndex,next.SegmentIndex)&&
                        !events.Any(e=>e.SegmentIndex>=prev.SegmentIndex&&e.SegmentIndex<next.SegmentIndex&&HardBoundary(e.Instruction)))
                        candidates.Add(Label([prev,next],"AMBIGUOUS_BRACKET_HYPOTHESIS",true));
                }
            }
            candidates=candidates.GroupBy(c=>(c.Identity,c.Kind,c.Cardinal))
                .Select(g=>g.OrderByDescending(c=>c.IdentityConfidence=="HIGH"?3:c.IdentityConfidence=="MEDIUM"?2:1).First()).ToList();
            bool cardinalConflict=direct.GroupBy(a=>a.Identity).Any(g=>g.Select(a=>a.Cardinal).Where(x=>x.Length>0).Distinct().Count()>1);
            var state=cardinalConflict?"CONFLICT":identities.Count>0?"RESOLVED":candidates.Count>1?"AMBIGUOUS":"UNKNOWN";
            if(cardinalConflict){candidates=identities.Concat(candidates).Distinct().ToList();identities.Clear();}
            spans.Add(new($"segment:{s.SegmentIndex}",s.SegmentIndex,s.ItemUid.ToString("X16"),
                probe.Physical[s.SegmentIndex].NodeUid.ToString("X16"),probe.Physical[s.SegmentIndex+1].NodeUid.ToString("X16"),
                s.StartStationM,s.EndStationM,s.StartNavStationM,s.EndNavStationM,state,identities.ToArray(),candidates.ToArray(),
                state=="RESOLVED"?string.Join(';',identities.Select(i=>i.Rule)):cardinalConflict?"CARDINAL_CONFLICT":direct.Length>1?"OWNERSHIP_OR_CONCURRENCY_UNPROVEN":"NO_BRACKETED_IDENTITY"));
        }
        return spans.ToArray();
    }
    private static bool Connected(ProbeRoute probe,int start,int end)=>start>=0&&end+1<probe.Physical.Count&&
        Enumerable.Range(start,end-start+2).Select(i=>probe.Physical[i].NodeUid).Distinct().Count()==end-start+2;
    internal static bool HardBoundary(string instruction)=>instruction.Contains("TURN",StringComparison.Ordinal)||
        instruction.StartsWith("SHARP_",StringComparison.Ordinal)||instruction=="ROUNDABOUT"||
        instruction.StartsWith("EXIT",StringComparison.Ordinal)||instruction.StartsWith("MERGE",StringComparison.Ordinal);
    private static SegmentRoadIdentity Label(RoadIdentityAnchor[] a,string rule,bool bracket)
    {
        var card=a.Select(x=>x.Cardinal).Where(x=>x.Length>0).Distinct().ToArray();
        bool high=a.All(x=>x.ReadingConfidence=="HIGH"&&x.AssociationConfidence=="HIGH");
        return new(a[0].Identity,a[0].Kind,card.Length==1?card[0]:"",high?"HIGH":a.Any(x=>x.AssociationConfidence=="LOW")?"LOW":"MEDIUM",
            card.Length==1?(high?"HIGH":a.Any(x=>x.AssociationConfidence=="LOW")?"LOW":"MEDIUM"):"NONE",a.Select(x=>x.Id).Distinct().Order(StringComparer.Ordinal).ToArray(),rule);
    }
    internal static ActionRoadContext[] Contexts(LiveRoute route,RoadIdentitySpan[] spans,IReadOnlyList<V09ManeuverEvent> events)
        =>route.Actions.Select(a=>
        {
            var before=spans.LastOrDefault(s=>s.SegmentIndex<a.SegmentIndex);
            var after=spans.FirstOrDefault(s=>s.SegmentIndex>a.SegmentIndex);
            var future=new List<RoadIdentitySpan>();
            if(after is not null)
            foreach(var s in spans.Where(s=>s.SegmentIndex>=after.SegmentIndex))
            {
                if(s.WorldEndM-after.WorldStartM>5000)break;
                if(s.SegmentIndex>after.SegmentIndex&&events.Any(e=>e.SegmentIndex==s.SegmentIndex-1&&HardBoundary(e.Instruction)))break;
                if(s.Status is "CONFLICT" or "AMBIGUOUS")break;
                future.Add(s);
            }
            var named=future.Where(s=>s.Identities.Length>0||s.Status=="UNKNOWN"&&s.Candidates.Length==1).ToArray();
            // An unknown outgoing portion is not proven to be a connector. Never skip it for targetRoad.
            var target=after;string? preferred=null;
            if(target is not null&&target.Identities.Length>0)
            {
                var common=target.Identities.Select(i=>i.Identity).ToHashSet(StringComparer.Ordinal);
                foreach(var s in named.Where(s=>s.SegmentIndex>target.SegmentIndex))common.IntersectWith((s.Identities.Length>0?s.Identities:s.Candidates).Select(i=>i.Identity));
                if(common.Count==1)preferred=common.Single();
            }
            return new ActionRoadContext(a.Id,a.BranchId,before is null?[]:[before.Id],target is null?[]:[target.Id],
                future.Select(s=>s.Id).ToArray(),preferred,after is null?"ROUTE_END":after.Identities.Length==0?"TARGET_UNKNOWN":"OUTGOING_SEGMENT");
        }).ToArray();
}
