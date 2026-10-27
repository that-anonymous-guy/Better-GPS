using System.Text.Json;

namespace BetterGPSNavigation;

internal sealed record VehicleLaneGeometry(string Id,int SegmentIndex,LivePoint[] Points)
{
    public string PhysicalLaneId {get;init;}="";
}
internal sealed record VehicleLaneCandidate(string LaneId,double LateralM,double HeightM,double HeadingErrorDeg);
internal sealed record VehicleLaneLocation(string Status,string? LaneId,VehicleLaneCandidate[] Candidates);

internal static class VehicleLaneLocator
{
    // Keep alternatives near a lane boundary. The GPS segment restricts the search; it does not select a lane.
    public static VehicleLaneLocation Locate(LiveVehicle vehicle,int segment,IReadOnlyList<VehicleLaneGeometry> lanes)
    {
        var hits=new List<VehicleLaneCandidate>();
        foreach(var lane in lanes.Where(l=>l.SegmentIndex==segment))
        {
            VehicleLaneCandidate? best=null;
            foreach(var (a,b) in lane.Points.Zip(lane.Points.Skip(1)))
            {
                var dx=b.X-a.X;var dz=b.Z-a.Z;var len=dx*dx+dz*dz;if(len<1e-8)continue;
                var t=Math.Clamp(((vehicle.Position.X-a.X)*dx+(vehicle.Position.Z-a.Z)*dz)/len,0,1);
                var lateral=Math.Sqrt(Math.Pow(vehicle.Position.X-a.X-t*dx,2)+Math.Pow(vehicle.Position.Z-a.Z-t*dz,2));
                var height=Math.Abs(vehicle.Position.Y-a.Y-t*(b.Y-a.Y));
                var angle=Math.Abs(Math.IEEERemainder(vehicle.HeadingDeg-Math.Atan2(dz,dx)*180/Math.PI,360));
                if(!LiveProtocol.Finite(lateral,height,angle)||lateral>3.5||height>1.5||angle>45)continue;
                if(best is null||lateral<best.LateralM)best=new(lane.PhysicalLaneId.Length>0?lane.PhysicalLaneId:lane.Id,lateral,height,angle);
            }
            if(best is not null)hits.Add(best);
        }
        var ordered=hits.GroupBy(h=>h.LaneId).Select(g=>g.MinBy(h=>h.LateralM)!).OrderBy(h=>h.LateralM).ThenBy(h=>h.LaneId,StringComparer.Ordinal).ToArray();
        var candidates=ordered.Where(h=>h.LateralM<=ordered[0].LateralM+1).ToArray();
        return new(candidates.Length==0?"UNKNOWN":candidates.Length>1?"BETWEEN_LANES":"GEOMETRIC_CANDIDATE",
            candidates.Length==1?candidates[0].LaneId:null,candidates);
    }
    public static VehicleLaneGeometry[] Read(LiveAction action)=>action.Proofs.Where(p=>p.Source=="LANE_NETWORK")
        .SelectMany(p=>JsonSerializer.Deserialize<VehicleLaneGeometry[]>(p.Value,LiveProtocol.Json)??[]).ToArray();
}
