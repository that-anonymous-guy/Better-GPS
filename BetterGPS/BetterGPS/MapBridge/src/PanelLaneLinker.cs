using System.Numerics;

namespace BetterGPSNavigation;

internal sealed record AuditLane(int Ordinal,string GeometryStatus,IReadOnlyList<Vector3> Points,
    IReadOnlyList<string> ReachableExits)
{
    public string PathBranchId {get;init;}="";
    public string LaneId {get;init;}="";
    public LaneChain? Chain {get;init;}
}
internal sealed record AuditLaneHit(int LaneOrdinal,double LateralM,double HeightM,Vector3 Position,
    string GeometryStatus,IReadOnlyList<string> ReachableExits)
{
    public string LaneId {get;init;}="";
    public LaneChain? Chain {get;init;}
}
internal sealed record AuditLink(string Status,string Confidence,IReadOnlyList<AuditLaneHit> Hits,
    IReadOnlyList<string> ReachableExits,IReadOnlyList<string> Blockers);

/// <summary>Diagnostic association only. No result from an uncalibrated plate can enter live HIGH proofs.</summary>
internal static class PanelLaneLinker
{
    public static AuditLink Link(Vector3 arrow,IReadOnlyList<AuditLane> lanes,string selectedExit,
        IReadOnlyList<string> sourceBlockers,double maxLateralM=2.5,double maxHeightM=15)
    {
        if(!float.IsFinite(arrow.X)||!float.IsFinite(arrow.Y)||!float.IsFinite(arrow.Z) ||
            !double.IsFinite(maxLateralM)||maxLateralM<=0 || !double.IsFinite(maxHeightM)||maxHeightM<=0)
            return new("INVALID_GEOMETRY","NONE",[],[],["NON_FINITE_OR_INVALID_GEOMETRY"]);
        var hits=new List<AuditLaneHit>();
        foreach(var lane in lanes)
        {
            AuditLaneHit? best=null;
            for(var i=0;i+1<lane.Points.Count;i++)
            {
                var a=lane.Points[i];var b=lane.Points[i+1];
                if(!float.IsFinite(a.X)||!float.IsFinite(a.Y)||!float.IsFinite(a.Z)||
                    !float.IsFinite(b.X)||!float.IsFinite(b.Y)||!float.IsFinite(b.Z))continue;
                var d=new Vector2(b.X-a.X,b.Z-a.Z);if(d.LengthSquared()<1e-8)continue;
                var t=Math.Clamp(Vector2.Dot(new(arrow.X-a.X,arrow.Z-a.Z),d)/d.LengthSquared(),0,1);
                // Do not attach a distant upstream sign to the clamped endpoint of a prefab curve.
                var raw=Vector2.Dot(new(arrow.X-a.X,arrow.Z-a.Z),d)/d.LengthSquared();
                if((i==0 && raw<0)||(i==lane.Points.Count-2 && raw>1))continue;
                var q=Vector3.Lerp(a,b,t);var height=arrow.Y-q.Y;
                var lateral=Vector2.Distance(new(arrow.X,arrow.Z),new(q.X,q.Z));
                if(height<0 || height>maxHeightM || lateral>maxLateralM)continue;
                if(best is null || lateral<best.LateralM)best=new(lane.Ordinal,lateral,height,q,lane.GeometryStatus,lane.ReachableExits){LaneId=lane.LaneId,Chain=lane.Chain};
            }
            if(best is not null)hits.Add(best);
        }
        hits=hits.GroupBy(h=>(h.LaneOrdinal,h.GeometryStatus,h.LaneId)).Select(g=>g.OrderBy(h=>h.LateralM).First()).ToList();
        var exits=hits.SelectMany(h=>h.ReachableExits).Distinct().Order().ToArray();
        var blockers=sourceBlockers.Concat(hits.Any(h=>h.GeometryStatus=="BOUNDARY_OFFSET_APPROX")?["UPSTREAM_LANE_GEOMETRY_APPROXIMATE"]:Array.Empty<string>())
            .Concat(hits.SelectMany(h=>h.Chain?.Blockers??[]))
            .Concat(hits.Count==0?["NO_LANE_WITHIN_DIAGNOSTIC_WINDOW"]:Array.Empty<string>())
            .Concat(exits.Length>1?["LANE_REACHES_MULTIPLE_EXITS"]:Array.Empty<string>()).Distinct().ToArray();
        var status=exits.Length==0?(hits.Count>0?"CHAIN_BLOCKED":"NO_LANE_LINK"):exits.Length>1?"AMBIGUOUS_EXITS":
            exits[0]==selectedExit?"SELECTED_EXIT_CANDIDATE":"OTHER_EXIT_CANDIDATE";
        // This API intentionally cannot issue HIGH: renderer/variant/front-face calibration is still required.
        return new(status,hits.Count==0?"NONE":"LOW",hits.OrderBy(h=>h.LateralM).ToArray(),exits,blockers);
    }
}
