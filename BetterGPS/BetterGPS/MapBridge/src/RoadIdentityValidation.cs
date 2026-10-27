namespace BetterGPSNavigation;

internal static class RoadIdentityValidation
{
    public static void Validate(LiveRoute route,RoadIdentitySnapshot data)
    {
        void Require([System.Diagnostics.CodeAnalysis.DoesNotReturnIf(false)] bool ok,string reason){if(!ok)throw new InvalidDataException("Road identities: "+reason);}
        Require(data.Version==1&&!string.IsNullOrWhiteSpace(data.MapFingerprint)&&data.Anchors is not null&&data.Spans is not null&&data.Contexts is not null,"schema");
        Require(data.Anchors!.Length<=10000&&data.Spans!.Length<=100000&&data.Contexts!.Length==route.Actions.Length,"size/context coverage");
        var anchors=new Dictionary<string,RoadIdentityAnchor>(StringComparer.Ordinal);
        foreach(var a in data.Anchors)
        {
            Require(a is not null&&!string.IsNullOrWhiteSpace(a.Id)&&anchors.TryAdd(a.Id,a),"duplicate/null anchor");
            Require(RoadIdentityRules.Valid(a!.Kind,a.Identity)&&a.Alternatives is not null&&
                !string.IsNullOrWhiteSpace(a.Source)&&!string.IsNullOrWhiteSpace(a.SourceId)&&a.Raw is not null&&a.OwnerUid is not null&&
                RoadIdentityRules.Confidence(a.ReadingConfidence)&&RoadIdentityRules.Confidence(a.AssociationConfidence)&&
                LiveProtocol.Finite(a.WorldStationM,a.NavStationM)&&a.WorldStationM>=0&&a.NavStationM>=0&&
                a.Cardinal is "" or "NORTH" or "SOUTH" or "EAST" or "WEST","anchor fields");
            Require(a.Alternatives!.All(c=>c is not null&&LiveProtocol.Finite(c.DistanceM,c.HeightM)&&c.DistanceM>=0&&c.HeightM>=0),"alternatives");
            Require(a.SegmentIndex>=-1&&(a.SegmentIndex<0||route.Edges.Any(e=>e.SegmentIndex==a.SegmentIndex&&e.ItemUid==a.OwnerUid)),"anchor owner/occurrence");
            Require(a.AssociationConfidence!="HIGH"||a.AssociationStatus=="VERIFIED_BRANCH","unsupported HIGH association");
            if(a.AssociationConfidence=="HIGH")Require(a.Source=="SIGN_GROUP"&&route.Actions
                .Where(x=>x.SegmentIndex+1==a.SegmentIndex).SelectMany(x=>x.Proofs)
                .Where(p=>p.Source=="SIGN_GROUP"&&p.SourceId==a.SourceId&&p.Confidence=="HIGH").Select(SignGroupProof.Read)
                .Any(g=>g.Road==a.Identity&&g.Fields.IdentityConfidence=="HIGH"),"HIGH ownership lacks verified outgoing group");
        }
        var spans=new Dictionary<string,RoadIdentitySpan>(StringComparer.Ordinal);
        var segments=route.Edges.GroupBy(e=>e.SegmentIndex).ToDictionary(g=>g.Key,g=>g.ToArray());
        Require(data.Spans.Length==segments.Count,"segment coverage");
        var covered=new HashSet<int>();
        foreach(var s in data.Spans)
        {
            Require(s is not null&&!string.IsNullOrWhiteSpace(s.Id)&&spans.TryAdd(s.Id,s)&&covered.Add(s.SegmentIndex)&&segments.ContainsKey(s.SegmentIndex),"duplicate/missing span");
            var edges=segments[s!.SegmentIndex];
            Require(s.ItemUid==edges[0].ItemUid&&LiveProtocol.Finite(s.WorldStartM,s.WorldEndM,s.NavStartM,s.NavEndM)&&
                Math.Abs(s.WorldStartM-edges[0].WorldStartM)<0.01&&Math.Abs(s.WorldEndM-edges[^1].WorldEndM)<0.01&&
                Math.Abs(s.NavStartM-edges[0].NavStartM)<0.01&&Math.Abs(s.NavEndM-edges[^1].NavEndM)<0.01&&
                s.EntryNodeUid is {Length:16}&&s.EntryNodeUid.All(Uri.IsHexDigit)&&s.ExitNodeUid is {Length:16}&&s.ExitNodeUid.All(Uri.IsHexDigit)&&s.Identities is not null&&s.Candidates is not null&&
                s.Status is "RESOLVED" or "UNKNOWN" or "AMBIGUOUS" or "CONFLICT","span fields");
            Require((s.Status=="RESOLVED")== (s.Identities!.Length>0),"resolved state");
            foreach(var label in s.Identities.Concat(s.Candidates!))
            {
                Require(label is not null&&RoadIdentityRules.Valid(label.Kind,label.Identity)&&label.ProofIds is {Length:>0}&&
                    RoadIdentityRules.Confidence(label.IdentityConfidence)&&RoadIdentityRules.Confidence(label.CardinalConfidence)&&
                    label.Cardinal is "" or "NORTH" or "SOUTH" or "EAST" or "WEST","label");
                Require(label!.ProofIds.All(id=>anchors.TryGetValue(id,out var a)&&a.Identity==label.Identity&&a.Kind==label.Kind),"proof reference");
                if(label.Rule=="CAPTURED_SAME_ROAD_CONTINUITY")
                {
                    var ends=label.ProofIds.Select(id=>anchors[id]).ToArray();
                    var start=ends.Min(a=>a.SegmentIndex);var end=ends.Max(a=>a.SegmentIndex);
                    Require(label.IdentityConfidence=="MEDIUM"&&label.CardinalConfidence!="HIGH"&&start<s.SegmentIndex&&end>s.SegmentIndex&&
                        ends.Select(a=>a.SourceId.Split(':')[0]).Distinct().Count()>1&&
                        ends.All(a=>a.AssociationConfidence is "HIGH" or "MEDIUM"&&RoadIdentityRules.ReferenceOnly(a.Raw))&&
                        !route.Actions.Any(a=>a.SegmentIndex>=start&&a.SegmentIndex<end&&RoadIdentityResolver.HardBoundary(a.Instruction)),
                        "captured continuity requires independent bracketing without a road-changing maneuver");
                }
                Require(label.IdentityConfidence!="HIGH"||label.ProofIds.All(id=>anchors[id].AssociationConfidence=="HIGH"&&anchors[id].ReadingConfidence=="HIGH"),"HIGH provenance");
                if(label.IdentityConfidence=="HIGH")
                {
                    var proofs=label.ProofIds.Select(id=>anchors[id]).ToArray();
                    Require(proofs.Any(a=>a.SegmentIndex==s.SegmentIndex)||
                        label.Rule=="BRACKETED_CONNECTED_ANCHORS"&&proofs.Min(a=>a.SegmentIndex)<=s.SegmentIndex&&
                        proofs.Max(a=>a.SegmentIndex)>=s.SegmentIndex&&proofs.Select(a=>a.SourceId.Split(':')[0]).Distinct().Count()>1,
                        "HIGH outside proof scope");
                }
                Require(label.CardinalConfidence!="HIGH"||label.IdentityConfidence=="HIGH"&&label.ProofIds.All(id=>anchors[id].Cardinal==label.Cardinal),"HIGH cardinal");
            }
        }
        var orderedSpans=data.Spans.OrderBy(s=>s.SegmentIndex).ToArray();
        Require(orderedSpans.Zip(orderedSpans.Skip(1)).All(p=>p.First.ExitNodeUid==p.Second.EntryNodeUid),"disconnected directed spans");
        var actionIds=new HashSet<string>();
        foreach(var c in data.Contexts)
        {
            Require(c is not null,"null action context");
            var a=route.Actions.FirstOrDefault(a=>a.Id==c.ActionId);
            Require(a is not null&&actionIds.Add(c.ActionId)&&c.BranchId==a.BranchId&&c.ApproachSpanIds is not null&&c.TargetSpanIds is not null&&c.FutureSpanIds is not null,"action context");
            Require(c.ApproachSpanIds!.All(id=>spans.TryGetValue(id,out var s)&&s.SegmentIndex<a!.SegmentIndex)&&
                c.TargetSpanIds!.All(id=>spans.TryGetValue(id,out var s)&&s.SegmentIndex>a!.SegmentIndex)&&
                c.FutureSpanIds!.All(id=>spans.TryGetValue(id,out var s)&&s.SegmentIndex>a!.SegmentIndex),"context references/direction");
            Require(c.PreferredIdentity is null||c.TargetSpanIds.Any(id=>spans[id].Identities.Concat(spans[id].Candidates).Any(i=>i.Identity==c.PreferredIdentity)),"preferred identity");
            Require(c.JoinedSpanIds is not null&&c.ConnectorSpanIds is not null,"connection context arrays");
            if(c.JoinedSpanIds!.Length>0)
            {
                Require(c.JoinedSpanIds.Length==1&&c.ConnectorSpanIds!.Length>0,"connection destination count");
                Require(c.ConnectorSpanIds.All(id=>spans.TryGetValue(id,out var s)&&s.Identities.Length==0&&
                    (RouteConnectionTargets.Unproven(s)||id==c.ConnectorSpanIds[0]&&s.Status=="AMBIGUOUS"&&
                        s.Candidates.All(i=>i.Kind=="StreetName"&&i.IdentityConfidence is "HIGH" or "MEDIUM")&&
                        c.JoinedSpanIds.Any(joined=>spans[joined].Identities.Any(i=>s.Candidates.Any(candidate=>candidate.Identity==i.Identity))))),"connection cannot rename unknown road");
                Require(c.JoinedSpanIds.All(id=>spans.TryGetValue(id,out var s)&&s.Status=="RESOLVED"&&s.Identities.Length>0&&s.Identities.All(i=>i.IdentityConfidence is "HIGH" or "MEDIUM")),"connection destination confidence");
                var chain=c.ConnectorSpanIds.Concat(c.JoinedSpanIds).Select(id=>spans[id]).ToArray();
                Require(chain[0].SegmentIndex==a!.SegmentIndex+1&&chain.Zip(chain.Skip(1)).All(p=>p.Second.SegmentIndex==p.First.SegmentIndex+1&&p.First.ExitNodeUid==p.Second.EntryNodeUid),"connection chain continuity");
                Require(chain[^1].WorldEndM-chain[0].WorldStartM<=1500,"connection distance bound");
                Require(!route.Actions.Any(other=>other.SegmentIndex>a.SegmentIndex&&other.SegmentIndex<=chain[^1].SegmentIndex&&
                    (other.Instruction.Contains("TURN",StringComparison.Ordinal)||other.Instruction.StartsWith("SHARP_",StringComparison.Ordinal)||other.Instruction.StartsWith("EXIT",StringComparison.Ordinal)||other.Instruction=="ROUNDABOUT")),"connection skipped turn");
            }
        }
    }
}
