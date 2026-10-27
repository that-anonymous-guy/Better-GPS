namespace BetterGPSNavigation;

// Occurrence-specific continuity. It never changes the independent map graph.
internal static class CapturedRoadContinuity
{
    internal static RoadIdentitySpan[] Fill(DirectedRoadNetworkGraph graph,RoadIdentitySpan[] spans,
        RoadIdentityAnchor[] anchors,LiveRoute route)
    {
        var result=spans.ToArray();var proofs=anchors.ToDictionary(a=>a.Id);
        for(int first=1;first<spans.Length-1;first++)
        {
            if(spans[first].Status!="UNKNOWN")continue;
            int end=first;while(end<spans.Length&&spans[end].Status=="UNKNOWN")end++;
            if(end==spans.Length)break;
            var before=spans[first-1];var after=spans[end];
            first=end;
            if(before.Status!="RESOLVED"||after.Status!="RESOLVED"||after.WorldStartM-before.WorldEndM>5000||
                route.Actions.Any(a=>a.SegmentIndex>=before.SegmentIndex&&a.SegmentIndex<after.SegmentIndex&&
                    RoadIdentityResolver.HardBoundary(a.Instruction)))continue;
            var corridor=spans.Where(s=>s.SegmentIndex>=before.SegmentIndex&&s.SegmentIndex<=after.SegmentIndex).ToArray();
            string Id(RoadIdentitySpan s)=>DirectedRoadNetworkGraph.Id(s.ItemUid,s.EntryNodeUid,s.ExitNodeUid);
            if(corridor.Select(s=>s.EntryNodeUid).Append(after.ExitNodeUid).Distinct().Count()!=corridor.Length+1||
                corridor.Zip(corridor.Skip(1)).Any(p=>!graph.Next(Id(p.First)).Contains(Id(p.Second))))continue;
            var labels=new List<SegmentRoadIdentity>();
            foreach(var key in before.Identities.Select(i=>(i.Identity,i.Kind)).Distinct())
            {
                var left=before.Identities.Where(i=>(i.Identity,i.Kind)==key).ToArray();
                var right=after.Identities.Where(i=>(i.Identity,i.Kind)==key).ToArray();if(right.Length==0)continue;
                var ends=left.Concat(right).ToArray();
                var directions=ends.Select(i=>i.Cardinal).Where(c=>c.Length>0).Distinct().ToArray();if(directions.Length>1)continue;
                var ids=ends.SelectMany(i=>i.ProofIds).Distinct().ToArray();
                if(ids.Select(id=>proofs[id].SourceId.Split(':')[0]).Distinct().Count()<2||
                    ids.Any(id=>proofs[id].AssociationConfidence is not ("HIGH" or "MEDIUM")||
                        !RoadIdentityRules.ReferenceOnly(proofs[id].Raw)))continue;
                if(corridor.Skip(1).SkipLast(1).Any(s=>s.Candidates.Any(i=>i.Kind==key.Kind&&i.Identity!=key.Identity)))continue;
                var cardinal=directions.Length==1&&left.Any(i=>i.Cardinal==directions[0])&&right.Any(i=>i.Cardinal==directions[0])?directions[0]:"";
                labels.Add(new(key.Identity,key.Kind,cardinal,"MEDIUM",cardinal.Length>0?"MEDIUM":"NONE",ids,"CAPTURED_SAME_ROAD_CONTINUITY"));
            }
            if(labels.Count==0)continue;
            foreach(var s in corridor.Skip(1).SkipLast(1))
                result[Array.IndexOf(spans,s)]=s with{Status="RESOLVED",Identities=labels.ToArray(),Candidates=[],Reason="CAPTURED_SAME_ROAD_CONTINUITY"};
        }
        return result;
    }
}
