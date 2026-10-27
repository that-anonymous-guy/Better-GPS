namespace BetterGPSNavigation;

internal static class PartialRoadEvidence
{
    // Distant pairs can lose competing evidence outside the first window.
    // Keep direct and local continuation proofs; wait for the complete region
    // before displaying a name inferred between two distant anchors.
    internal static NamedNetworkSnapshot Filter(NamedNetworkSnapshot data)=>data with{
        Names=data.Names.Select(n=>{
            var identities=n.Identities.Where(i=>!i.Rule.StartsWith("BRACKETED_",StringComparison.Ordinal)).ToArray();
            var candidates=n.Candidates.Where(i=>!i.Rule.StartsWith("BRACKETED_",StringComparison.Ordinal)).ToArray();
            return n with{Identities=identities,Candidates=candidates,Status=identities.Length>0?"RESOLVED":candidates.Length>0?"AMBIGUOUS":"UNKNOWN"};
        }).ToArray()};
}
