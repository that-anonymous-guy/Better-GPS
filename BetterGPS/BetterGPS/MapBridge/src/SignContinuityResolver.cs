using System.Numerics;

namespace BetterGPSNavigation;

internal sealed record SignContinuityEvidence(string SignUid,string AreaName,Vector3 Position,double WorldStationM,
    double NavStationM,double LateralM,double HeightM,IReadOnlyList<SignSemanticGroup> Groups);
internal sealed record SignContinuation(string SignUid,string AreaName,string GroupId,double WorldAheadM,double NavAheadM,
    double LateralM,string Status);
internal sealed record SignContinuity(string GroupId,string Status,string Confidence,int IndependentSupportCount,
    IReadOnlyList<SignContinuation> Supports,IReadOnlyList<string> Blockers)
{
    public IReadOnlyList<string> RepeatedCompetingGroupIds {get;init;}=[];
}

internal static class SignContinuityResolver
{
    static bool Finite(Vector3 p)=>float.IsFinite(p.X)&&float.IsFinite(p.Y)&&float.IsFinite(p.Z);
    public static SignContinuity Resolve(SignContinuityEvidence origin,SignSemanticGroup group,
        IReadOnlyList<SignContinuityEvidence> evidence,double maxWorldAheadM=2500)
    {
        var blockers=new List<string>{"SELECTED_ROUTE_PROXIMITY_NOT_LANE_OWNERSHIP","SIGN_FACE_AND_PLACEMENT_UNCALIBRATED"};
        if(group.Status!="COMPLETE_CANDIDATE")return new(group.Id,"GROUP_UNRESOLVED","NONE",0,[],blockers);
        if(!Finite(origin.Position)||!double.IsFinite(origin.WorldStationM)||!double.IsFinite(origin.NavStationM)||!double.IsFinite(maxWorldAheadM)||maxWorldAheadM<=0)
            return new(group.Id,"INVALID_STATION","NONE",0,[],["NON_FINITE_STATION"]);
        bool Same(SignSemanticGroup g)=>g.Status=="COMPLETE_CANDIDATE"&&g.Road.Equals(group.Road,StringComparison.OrdinalIgnoreCase)&&
            g.Cardinal.Equals(group.Cardinal,StringComparison.OrdinalIgnoreCase)&&g.Destination.Equals(group.Destination,StringComparison.OrdinalIgnoreCase);
        var supports=new List<SignContinuation>();var accepted=new List<Vector3>{origin.Position};
        foreach(var candidate in evidence.Where(e=>e.SignUid!=origin.SignUid&&Finite(e.Position)&&double.IsFinite(e.WorldStationM)&&double.IsFinite(e.NavStationM)&&
            double.IsFinite(e.LateralM)&&double.IsFinite(e.HeightM)&& e.WorldStationM>origin.WorldStationM+25&&
            e.WorldStationM<=origin.WorldStationM+maxWorldAheadM&&e.NavStationM>origin.NavStationM&&
            e.LateralM>=0&&e.LateralM<=20&&Math.Abs(e.HeightM)<=15).OrderBy(e=>e.WorldStationM).ThenBy(e=>e.SignUid))
        {
            var match=candidate.Groups.FirstOrDefault(Same);if(match is null)continue;
            if(supports.Any(s=>s.SignUid==candidate.SignUid)||accepted.Any(p=>Vector3.Distance(p,candidate.Position)<25))continue;
            accepted.Add(candidate.Position);
            supports.Add(new(candidate.SignUid,candidate.AreaName,match.Id,candidate.WorldStationM-origin.WorldStationM,
                candidate.NavStationM-origin.NavStationM,candidate.LateralM,"FOLLOWING_SELECTED_CORRIDOR_CANDIDATE"));
        }
        // Two inscriptions repeated on the same downstream portiques do not distinguish destinations.
        var competing=origin.Groups.Where(g=>g.Id!=group.Id&&g.Status=="COMPLETE_CANDIDATE"&&
                (g.Road!=group.Road||g.Cardinal!=group.Cardinal||g.Destination!=group.Destination))
            .Where(g=>supports.Count(s=>evidence.Any(e=>e.SignUid==s.SignUid&&e.Groups.Any(x=>x.Status=="COMPLETE_CANDIDATE"&&
                x.Road.Equals(g.Road,StringComparison.OrdinalIgnoreCase)&&x.Cardinal.Equals(g.Cardinal,StringComparison.OrdinalIgnoreCase)&&
                x.Destination.Equals(g.Destination,StringComparison.OrdinalIgnoreCase))))>=2).Select(g=>g.Id).ToArray();
        if(competing.Length>0)blockers.Add("COMPETING_GROUP_ALSO_REPEATS_ON_SAME_SUPPORTS");
        return new(group.Id,competing.Length>0?"REPEATED_BUT_NOT_DISCRIMINATING":supports.Count>=2?"REPEATED_AHEAD_CANDIDATE":supports.Count==1?"ONE_SUPPORT_AHEAD":"NO_SUPPORT_AHEAD",
            supports.Count>0?"LOW":"NONE",supports.Count,supports,blockers){RepeatedCompetingGroupIds=competing};
    }
}
