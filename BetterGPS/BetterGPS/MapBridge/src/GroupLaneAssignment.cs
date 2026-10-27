namespace BetterGPSNavigation;

internal sealed record GroupArrowEvidence(uint ObjectId,string HypothesisId,AuditLink Link);
internal sealed record GroupLaneAssignment(string GroupId,string Status,string Confidence,
    IReadOnlyList<uint> ArrowObjectIds,IReadOnlyList<string> HypothesisIds,IReadOnlyList<string> LaneIds,
    IReadOnlyList<string> FutureSelectedBranches,IReadOnlyList<string> Blockers);

internal static class GroupLaneAssigner
{
    public static GroupLaneAssignment Resolve(SignSemanticGroup group,IReadOnlyList<GroupArrowEvidence> arrows)
    {
        var candidates=arrows.Where(a=>group.ArrowObjectIds.Contains(a.ObjectId)).ToArray();
        var hits=candidates.SelectMany(a=>a.Link.Hits).ToArray();
        var blocks=group.Blockers.Concat(candidates.SelectMany(a=>a.Link.Blockers)).ToHashSet();
        blocks.Add("SIGN_OWNERSHIP_NOT_CALIBRATED");
        var viable=hits.Where(h=>h.Chain?.Status is "GPS_CONTINUATION_CANDIDATE" or "CURRENT_EXIT_ONLY").ToArray();
        bool conflictingPlacement=viable.Length>0&&hits.Any(h=>h.Chain?.Status=="OTHER_EXIT_ONLY");
        var status=group.Status!="COMPLETE_CANDIDATE"?"GROUP_UNRESOLVED":candidates.Length==0?"NO_ARROW_GEOMETRY":
            conflictingPlacement?"AMBIGUOUS_PLACEMENT":viable.Length>0?"GPS_CONTINUATION_CANDIDATE":hits.Any(h=>h.Chain?.Status=="OTHER_EXIT_ONLY")?"OTHER_EXIT_CANDIDATE":"CHAIN_BLOCKED";
        if(conflictingPlacement)blocks.Add("PLACEMENT_HYPOTHESES_DISAGREE_ON_GPS_EXIT");
        if(candidates.Select(a=>a.HypothesisId).Distinct().Count()>1)blocks.Add("MULTIPLE_PLACEMENT_HYPOTHESES");
        return new(group.Id,status,"LOW",group.ArrowObjectIds,candidates.Select(a=>a.HypothesisId).Distinct().ToArray(),
            hits.Select(h=>h.LaneId).Where(id=>id!="").Distinct().ToArray(),
            viable.SelectMany(h=>h.Chain!.FutureSelectedBranches).Distinct().ToArray(),blocks.Order().ToArray());
    }
}
