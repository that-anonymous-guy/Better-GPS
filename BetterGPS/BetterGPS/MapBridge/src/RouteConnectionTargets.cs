namespace BetterGPSNavigation;

/// <summary>Destination of a selected connector; never supplies its own road name.</summary>
internal static class RouteConnectionTargets
{
    internal static ActionRoadContext[] Build(DirectedRoadNetworkGraph graph,LiveRoute route,RoadIdentitySpan[] spans)
        =>RoadIdentityResolver.Contexts(route,spans,[]).Select(context=>
        {
            var action=route.Actions.Single(a=>a.Id==context.ActionId);
            var leg=spans.Where(s=>s.SegmentIndex>action.SegmentIndex).OrderBy(s=>s.SegmentIndex).ToArray();
            bool StreetConflict(RoadIdentitySpan s)=>s.Status=="AMBIGUOUS"&&s.Candidates.Length>1&&
                s.Candidates.All(i=>i.Kind=="StreetName"&&i.IdentityConfidence is "HIGH" or "MEDIUM");
            if(leg.Length==0||!(Unproven(leg[0])||StreetConflict(leg[0]))||leg[0].Identities.Length>0)return context;
            bool conflict=StreetConflict(leg[0]);
            string Id(RoadIdentitySpan s)=>DirectedRoadNetworkGraph.Id(s.ItemUid,s.EntryNodeUid,s.ExitNodeUid);
            var start=spans.SingleOrDefault(s=>s.SegmentIndex==action.SegmentIndex);
            if(start is null)return context;
            var previous=start;var visited=new HashSet<string>{start.EntryNodeUid,start.ExitNodeUid};
            var connector=new List<string>();
            foreach(var span in leg)
            {
                // Follow the captured native choice, including forks, but stop before another turn.
                if(span.SegmentIndex!=previous.SegmentIndex+1||span.EntryNodeUid!=previous.ExitNodeUid||
                    !visited.Add(span.ExitNodeUid)||span.WorldEndM-leg[0].WorldStartM>1500||
                    !graph.Next(Id(previous)).Contains(Id(span)))break;
                var portion=graph.Get(Id(span));
                if(portion.Blockers.Any(b=>b!="LANE_WIDTH_4_5M_ASSUMED")||span.Status=="CONFLICT"||
                    span.Status=="AMBIGUOUS"&&(!conflict||span!=leg[0]))break;
                if(conflict&&span.WorldEndM-leg[0].WorldStartM>200)break;
                if(route.Actions.Any(a=>a.SegmentIndex>action.SegmentIndex&&a.SegmentIndex<=span.SegmentIndex&&
                    (a.Instruction.Contains("TURN",StringComparison.Ordinal)||a.Instruction.StartsWith("SHARP_",StringComparison.Ordinal)||
                     a.Instruction.StartsWith("EXIT",StringComparison.Ordinal)||a.Instruction=="ROUNDABOUT")))break;
                if(span.Status=="RESOLVED"&&span.Identities.Length>0&&span.Identities.All(i=>i.IdentityConfidence is "HIGH" or "MEDIUM"))
                {
                    if(conflict&&!span.Identities.Any(i=>leg[0].Candidates.Any(c=>c.Identity==i.Identity&&c.Kind==i.Kind)))break;
                    return context with{JoinedSpanIds=[span.Id],ConnectorSpanIds=connector.ToArray()};
                }
                if(conflict&&span==leg[0]){connector.Add(span.Id);previous=span;continue;}
                if(!Unproven(span))break;
                connector.Add(span.Id);previous=span;
            }
            return context;
        }).ToArray();
    // A low-confidence annotation is neither a road name nor a veto against
    // following the captured branch to its independently resolved destination.
    // Medium/high candidates and explicit conflicts still stop this traversal.
    internal static bool Unproven(RoadIdentitySpan span)=>span.Status=="UNKNOWN"&&span.Identities.Length==0&&
        span.Candidates.All(i=>i.IdentityConfidence=="LOW");
}
