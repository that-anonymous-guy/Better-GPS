using System.Numerics;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal sealed record LaneBoundaryMatch(string FromPort,string ToPort,int FromSegment,int ToSegment,
    double LateralM,double HeightM,double AngleDeg,string Status);
internal sealed record LaneChain(string LaneId,string Status,IReadOnlyList<string> CurrentExits,
    IReadOnlyList<string> PieceIds,IReadOnlyList<string> FutureSelectedBranches,IReadOnlyList<string> Blockers);

/// <summary>Directed PPD connectivity plus geometric road boundaries. No lateral lane-change edges are invented.</summary>
internal sealed class LaneRouteGraph
{
    public IReadOnlyList<RouteLanePiece> Pieces {get;}
    public IReadOnlyList<LaneBoundaryMatch> Boundaries {get;}
    public IReadOnlyList<string> LoadErrors {get;}
    private readonly Dictionary<int,DecisionBranchSet> decisions;
    public object Export()=>new{version=1,units="SCS world metres; navigation distance is separate",LoadErrors,
        boundaryWindow=new{lateralM=1.5,heightM=1,angleDeg=30},Boundaries,
        pieces=Pieces.Select(p=>new{p.Id,p.SegmentIndex,p.Kind,p.StartPort,p.EndPort,p.BranchId,p.Selected,p.Ordinal,
            p.LookToken,p.LookSource,p.Blockers,startNodeUid=p.StartNodeUid.ToString("X16"),endNodeUid=p.EndNodeUid.ToString("X16"),points=p.Points.Select(v=>new[]{v.X,v.Y,v.Z})})};
    public LaneRouteGraph(IReadOnlyList<RouteLanePiece> pieces,IReadOnlyList<LaneBoundaryMatch> boundaries,
        Dictionary<int,DecisionBranchSet> decisions,IReadOnlyList<string>? errors=null)
    {Pieces=pieces;Boundaries=boundaries;this.decisions=decisions;LoadErrors=errors??[];}

    public IReadOnlyList<int> DecisionSegments => decisions.Keys.Order().ToArray();

    public static LaneRouteGraph Build(Map map,ProbeRoute route,RouteCorridor corridor,
        DecisionBranchResolver resolver,RoadLaneGeometryResolver roads)
    {
        var pieces=new List<RouteLanePiece>();var sets=new Dictionary<int,DecisionBranchSet>();
        foreach(var seg in corridor.Segments)
        {
            if(!map.MapItems.TryGetValue(seg.ItemUid,out var item))continue;
            if(item is Road road)pieces.AddRange(roads.Resolve(road,seg.SegmentIndex,
                route.Physical[seg.SegmentIndex].NodeUid,route.Physical[seg.SegmentIndex+1].NodeUid)
                .Select(p=>p with {StartNodeUid=route.Physical[seg.SegmentIndex].NodeUid,EndNodeUid=route.Physical[seg.SegmentIndex+1].NodeUid}));
            else if(item is Prefab)
            {
                var set=resolver.Resolve(route,new(-1,seg.SegmentIndex,"","",seg.StartStationM,seg.StartNavStationM,"",false,"LOW"));
                sets[seg.SegmentIndex]=set;
                foreach(var branch in set.Branches)
                foreach(var path in branch.ExactLanePaths)
                {
                    if(path.Points.Count<2||path.OutputLaneOrdinal<0)continue;
                    var id=$"P:{seg.SegmentIndex}:{branch.BranchId}:{path.InputLaneOrdinal}:{path.OutputLaneOrdinal}:{string.Join('-',path.CurveIds)}";
                    pieces.Add(new(id,seg.SegmentIndex,"PPD_SAMPLED",
                        $"P:{seg.SegmentIndex}:in:{path.InputLaneOrdinal}",
                        $"P:{seg.SegmentIndex}:out:{branch.TargetPpdNodeIndex}:{path.OutputLaneOrdinal}",
                        branch.BranchId,branch.Selected,path.InputLaneOrdinal,"","",path.Points,
                        branch.TransformMaxErrorM>1?["PPD_TRANSFORM_RESIDUAL_OVER_1M"]:[])
                        {StartNodeUid=route.Physical[seg.SegmentIndex].NodeUid,EndNodeUid=branch.TargetMapNodeUid});
                }
            }
        }
        var matches=new List<LaneBoundaryMatch>();
        for(int i=0;i<corridor.Segments.Count-1;i++)
        {
            var outgoing=pieces.Where(p=>p.SegmentIndex==i&&p.Selected).GroupBy(p=>p.EndPort).Select(g=>g.First()).ToArray();
            var incoming=pieces.Where(p=>p.SegmentIndex==i+1).GroupBy(p=>p.StartPort).Select(g=>g.First()).ToArray();
            matches.AddRange(Match(outgoing,incoming));
        }
        return new(pieces,matches,sets,roads.LoadErrors);
    }
    internal static IReadOnlyList<LaneBoundaryMatch> Match(IReadOnlyList<RouteLanePiece> from,IReadOnlyList<RouteLanePiece> to)
    {
        var candidates=new List<LaneBoundaryMatch>();var rejected=new List<LaneBoundaryMatch>();
        foreach(var a in from)foreach(var b in to)
        {
            if(a.EndNodeUid!=0 && b.StartNodeUid!=0 && a.EndNodeUid!=b.StartNodeUid)continue;
            if(a.Points.Count<2||b.Points.Count<2)continue;
            var x=a.Points[^1];var y=b.Points[0];
            if(!RoadLaneGeometryResolver.Finite(x)||!RoadLaneGeometryResolver.Finite(y))continue;
            var u=a.Points[^1]-a.Points[^2];var v=b.Points[1]-b.Points[0];
            u.Y=v.Y=0;if(u.LengthSquared()<1e-8||v.LengthSquared()<1e-8)continue;
            var angle=Math.Acos(Math.Clamp(Vector3.Dot(Vector3.Normalize(u),Vector3.Normalize(v)),-1,1))*180/Math.PI;
            var lateral=Vector2.Distance(new(x.X,x.Z),new(y.X,y.Z));var height=Math.Abs(x.Y-y.Y);
            var match=new LaneBoundaryMatch(a.EndPort,b.StartPort,a.SegmentIndex,b.SegmentIndex,lateral,height,angle,
                height>1?"REJECTED_HEIGHT":angle>30?"REJECTED_DIRECTION":"REJECTED_POSITION");
            if(lateral<=1.5&&height<=1&&angle<=30)candidates.Add(match with{Status="CANDIDATE"});
            else rejected.Add(match);
        }
        return candidates.Select(c=>c with{Status=candidates.Count(x=>x.FromPort==c.FromPort)==1&&candidates.Count(x=>x.ToPort==c.ToPort)==1
            ?"UNIQUE_GEOMETRIC_CANDIDATE":"AMBIGUOUS_BOUNDARY"})
            .Concat(rejected.Where(r=>!candidates.Any(c=>c.FromPort==r.FromPort)).GroupBy(r=>r.FromPort)
                .Select(g=>g.OrderBy(r=>r.LateralM+r.HeightM).First())).ToArray();
    }
    private IEnumerable<RouteLanePiece> Next(RouteLanePiece p)=>Boundaries.Where(b=>b.FromPort==p.EndPort&&b.Status=="UNIQUE_GEOMETRIC_CANDIDATE")
        .SelectMany(b=>Pieces.Where(n=>n.StartPort==b.ToPort));

    public LaneChain Trace(string laneId,int decisionSegment,int maxSegments=64,bool lookahead=true)
    {
        var start=Pieces.FirstOrDefault(p=>p.Id==laneId);
        if(start is null)return new(laneId,"NO_LANE",[],[],[],["LANE_ID_MISSING"]);
        var future=decisions.Where(k=>lookahead&&k.Key>decisionSegment&&k.Value.Branches.Count>1).OrderBy(k=>k.Key).FirstOrDefault();
        int stop=future.Value is null?decisionSegment:future.Key;
        if(stop-start.SegmentIndex>maxSegments)return new(laneId,"LOOKAHEAD_LIMIT",[],[laneId],[],["LOOKAHEAD_SEGMENT_LIMIT"]);
        var visited=new HashSet<string>();var exits=new HashSet<string>();var compatible=new HashSet<string>();var blocks=new HashSet<string>();
        var futureBranches=new HashSet<string>();var pending=new Queue<(RouteLanePiece Piece,string Current)>();pending.Enqueue((start,""));
        var states=new HashSet<(string,string)>();bool reached=false;
        while(pending.Count>0)
        {
            var (p,current)=pending.Dequeue();if(!states.Add((p.Id,current)))continue;visited.Add(p.Id);
            foreach(var b in p.Blockers)blocks.Add(b);
            // Unsafe/missing offset schemas cannot produce a chain, even if endpoints happen to match.
            if(p.Blockers.Any(b=>b!="LANE_WIDTH_4_5M_ASSUMED")){blocks.Add("LANE_GEOMETRY_UNRESOLVED");continue;}
            if(p.SegmentIndex==decisionSegment&&p.Kind=="PPD_SAMPLED")
            {
                exits.Add(p.BranchId);current=p.BranchId;
                if(!p.Selected)continue;
            }
            else if(p.Kind=="PPD_SAMPLED"&&!p.Selected)continue;
            if(p.SegmentIndex==stop&&p.Kind=="PPD_SAMPLED"&&p.Selected)
            {
                reached=true;compatible.Add(current);if(stop>decisionSegment)futureBranches.Add(p.BranchId);continue;
            }
            if(p.SegmentIndex>=stop)continue;
            var next=Next(p).ToArray();
            if(next.Length==0)blocks.Add(Boundaries.Any(b=>b.FromPort==p.EndPort&&b.Status=="AMBIGUOUS_BOUNDARY")?"AMBIGUOUS_BOUNDARY":"BOUNDARY_NOT_MATCHED");
            foreach(var n in next)pending.Enqueue((n,current));
        }
        var status=reached?(stop>decisionSegment?"GPS_CONTINUATION_CANDIDATE":"CURRENT_EXIT_ONLY"):
            exits.Count>0&&exits.All(e=>!decisions[decisionSegment].Branches.Any(b=>b.Selected&&b.BranchId==e))?"OTHER_EXIT_ONLY":"CHAIN_BLOCKED";
        // Dead alternatives remain visible. A successful geometric candidate still needs calibrated sign ownership.
        return new(laneId,status,exits.Order().ToArray(),visited.Order().ToArray(),futureBranches.Order().ToArray(),blocks.Order().ToArray());
    }
    // Bounded, independent stages: a missing future boundary never invalidates the current exit.
    public IReadOnlyList<LaneChain> TraceAhead(string laneId,int decisionSegment,int maxSegments=128,int maxDecisions=8,double maxWorldM=5000)
    {
        var start=Pieces.FirstOrDefault(p=>p.Id==laneId);
        if(start is null)return [Trace(laneId,decisionSegment,lookahead:false)];
        var result=new List<LaneChain>();
        foreach(var segment in DecisionSegments.Where(s=>s>=decisionSegment).Take(maxDecisions))
        {
            var length=Pieces.Where(p=>p.Selected&&p.SegmentIndex>=start.SegmentIndex&&p.SegmentIndex<segment)
                .GroupBy(p=>p.SegmentIndex).Sum(g=>g.Min(p=>p.Points.Zip(p.Points.Skip(1),(a,b)=>(double)Vector3.Distance(a,b)).Sum()));
            if(segment-start.SegmentIndex>maxSegments || length>maxWorldM)break;
            var chain=Trace(laneId,segment,maxSegments,false);result.Add(chain);
            if(chain.Status!="CURRENT_EXIT_ONLY")break;
        }
        return result;
    }

    public IReadOnlyList<AuditLane> CurrentLanesFor(DecisionBranchSet set) => LanesFor(set).Select(l=>
    {
        var chain=Trace(l.LaneId,set.SegmentIndex,lookahead:false);
        return l with {Chain=chain,ReachableExits=chain.CurrentExits};
    }).ToArray();

    public IReadOnlyList<AuditLane> LanesFor(DecisionBranchSet set)
    {
        int first=set.SegmentIndex;
        while(first>0&&Pieces.Any(p=>p.SegmentIndex==first-1&&p.Kind=="ROAD_LOOK_SAMPLED"))first--;
        return Pieces.Where(p=>p.SegmentIndex>=first&&p.SegmentIndex<=set.SegmentIndex).Select(p=>
        {
            var chain=Trace(p.Id,set.SegmentIndex);
            return new AuditLane(p.Ordinal,p.Kind,p.Points,chain.CurrentExits){PathBranchId=p.BranchId,LaneId=p.Id,Chain=chain};
        }).ToArray();
    }
}
