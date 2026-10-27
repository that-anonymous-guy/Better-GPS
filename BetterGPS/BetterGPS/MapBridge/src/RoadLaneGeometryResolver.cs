using System.Collections;
using System.Globalization;
using System.Numerics;
using TruckLib;
using TruckLib.ScsMap;
using TruckLib.Sii;

namespace BetterGPSNavigation;

internal sealed record RoadLaneLook(string Token,string Source,float[] Left,float[] Right,string[] Blockers);
internal sealed record RouteLanePiece(string Id,int SegmentIndex,string Kind,string StartPort,string EndPort,
    string BranchId,bool Selected,int Ordinal,string LookToken,string LookSource,IReadOnlyList<Vector3> Points,
    IReadOnlyList<string> Blockers)
{
    public ulong StartNodeUid {get;init;}
    public ulong EndNodeUid {get;init;}
}

/// <summary>Lane offset convention follows ETS2LA RoadUtils and ParsedRoad; native spline from TruckLib.
/// Width is an explicit hypothesis, never a HIGH proof. Cache belongs to one mounted filesystem.</summary>
internal sealed class RoadLaneGeometryResolver(IFileSystem fs)
{
    private readonly Dictionary<string,RoadLaneLook> looks=new(StringComparer.OrdinalIgnoreCase);
    private bool loaded;
    public IReadOnlyList<string> LoadErrors {get{Load();return errors;}}
    private readonly List<string> errors=[];

    private void Load()
    {
        if(loaded)return;loaded=true;
        var index=AssetIndexCache.GetShared(fs,"road-looks",()=>{LoadDefinitions();return new LookIndex(looks.Values.ToArray(),errors.ToArray());});
        foreach(var look in index.Looks)looks[look.Token]=look;
        errors.Clear();errors.AddRange(index.Errors);
    }
    private sealed record LookIndex(RoadLaneLook[] Looks,string[] Errors);
    private void LoadDefinitions()
    {
        var index=new SiiClassIndex(fs,"road_look");
        foreach(var file in fs.GetFiles("/def/world/").Where(p=>p.EndsWith(".sii",StringComparison.OrdinalIgnoreCase)).Order())
        {
            if(!index.MayContain(file))continue;
            try
            {
                foreach(var unit in SiiDocumentCache.Open(file,fs).Units.Where(u=>u.Class=="road_look"))
                {
                    var attrs=unit.Attributes.ToDictionary(k=>k.Key,k=>(object?)k.Value);
                    looks[unit.Name]=Parse(unit.Name,file,attrs);
                }
            }
            catch(Exception ex){errors.Add(file+":"+ex.GetType().Name);}
        }
    }
    internal static RoadLaneLook Parse(string token,string source,IReadOnlyDictionary<string,object?> attrs)
    {
        object[] ArrayOf(string name)=>attrs.TryGetValue(name,out var raw)&&raw is IEnumerable seq&&raw is not string
            ?seq.Cast<object>().ToArray():[];
        var left=ArrayOf("lanes_left");var right=ArrayOf("lanes_right");
        var blockers=new List<string>{"LANE_WIDTH_4_5M_ASSUMED"};
        float Number(object? v)=>Convert.ToSingle(v,CultureInfo.InvariantCulture);
        var l=new float[left.Length];var r=new float[right.Length];
        const float width=4.5f,half=2.25f;
        var center=l.Length>0&&r.Length>0&&(l.Length+r.Length)%2==1?(l.Length-r.Length)*width:0;
        if(r.Length>0)
        {
            r[0]=l.Length>0||r.Length==1?center+half:r.Length%2==1?center-half:center-MathF.Ceiling(r.Length/2f)*width+half;
            for(int i=1;i<r.Length;i++)r[i]=r[i-1]+width;
        }
        if(l.Length>0)
        {
            l[0]=r.Length>0||l.Length==1?center-half:l.Length%2==1?center+half:center+MathF.Ceiling(l.Length/2f)*width-half;
            for(int i=1;i<l.Length;i++)l[i]=l[i-1]-width;
        }
        try
        {
            var offset=attrs.TryGetValue("road_offset",out var v)?Number(v):0;
            for(int i=0;i<l.Length;i++)l[i]-=offset;
            for(int i=0;i<r.Length;i++)r[i]+=offset;
            Apply("lane_offsets_left",l,-1);Apply("lane_offsets_right",r,1);
            void Apply(string name,float[] centers,int sign)
            {
                var values=ArrayOf(name);if(!attrs.ContainsKey(name))return;
                if(values.Length!=centers.Length){blockers.Add("LANE_OFFSET_COUNT_MISMATCH");return;}
                for(int i=0;i<values.Length;i++)
                {
                    var x=values[i] switch{Vector2 vec=>vec.X,ValueTuple<int,int> pair=>pair.Item1,
                        ValueTuple<float,float> pair=>pair.Item1,
                        ValueTuple<object,object> pair when pair.Item1 is int or float or double && pair.Item2 is int or float or double=>Number(pair.Item1),
                        _=>float.NaN};
                    if(!float.IsFinite(x)){blockers.Add("LANE_OFFSET_TYPE_UNSUPPORTED");continue;}
                    centers[i]+=sign*x;
                }
            }
            if(l.Concat(r).Any(x=>!float.IsFinite(x)))blockers.Add("NON_FINITE_LANE_OFFSET");
        }
        catch(Exception ex) when(ex is FormatException or InvalidCastException or OverflowException)
        {blockers.Add("ROAD_OFFSET_INVALID");}
        if(l.Length+r.Length==0)blockers.Add("NO_ROAD_LANES");
        return new(token,source,l,r,blockers.Distinct().ToArray());
    }
    public IReadOnlyList<RouteLanePiece> Resolve(Road road,int segment,ulong startNode,ulong endNode)
    {
        Load();var token=road.RoadType.ToString();
        if(!looks.TryGetValue("road."+token,out var look)&&!looks.TryGetValue(token,out look))
        {var error="ROAD_LOOK_MISSING:"+token;if(!errors.Contains(error))errors.Add(error);return [];}
        bool forward=road.Node.Uid==startNode&&road.ForwardNode.Uid==endNode;
        bool reverse=road.ForwardNode.Uid==startNode&&road.Node.Uid==endNode;
        if(!forward&&!reverse)return [];
        bool rightSide=forward!=road.LeftHandTraffic;
        var offsets=rightSide?look.Right:look.Left;var side=rightSide?"RIGHT":"LEFT";
        if(offsets.Length==0)return [];
        var result=new List<RouteLanePiece>();
        // No index interpolation through a changed road look: retain a visible transition blocker.
        var previous=road.Node.BackwardItem as Road;
        RoadLaneLook? prior=null;
        if(previous is not null){var key=previous.RoadType.ToString();looks.TryGetValue("road."+key,out prior);if(prior is null)looks.TryGetValue(key,out prior);}
        var priorOffsets=rightSide?prior?.Right:prior?.Left;
        bool changed=prior is not null&&prior.Token!=look.Token;
        var steps=Math.Clamp((int)Math.Ceiling(road.Length/5f),2,4000);
        // Native spline samples and normals are independent of the lane offset.
        var positions=new Vector3[steps+1];var normals=new Vector3[steps+1];
        for(int i=0;i<=steps;i++)
        {
            var sample=road.InterpolateCurve(i/(float)steps);
            positions[i]=sample.Position;normals[i]=Vector3.Normalize(Vector3.Transform(Vector3.UnitX,sample.Rotation));
        }
        for(int lane=0;lane<offsets.Length;lane++)
        {
            var blockers=look.Blockers.ToList();
            if(previous is not null&&previous.LeftHandTraffic!=road.LeftHandTraffic)blockers.Add("TRAFFIC_SIDE_TRANSITION_UNRESOLVED");
            if(priorOffsets is not null&&priorOffsets.Length!=offsets.Length)blockers.Add("LANE_COUNT_TRANSITION_UNRESOLVED");
            if(changed&&priorOffsets?.Length==offsets.Length)blockers.Add("OFFSET_TRANSITION_INDEX_ASSUMED");
            if(prior is not null)blockers.AddRange(prior.Blockers.Where(b=>b!="LANE_WIDTH_4_5M_ASSUMED"));
            var points=new Vector3[steps+1];
            for(int i=0;i<=steps;i++)
            {
                float t=i/(float)steps;
                var offset=offsets[lane];
                if(changed&&priorOffsets?.Length==offsets.Length)offset=priorOffsets[lane]+(offset-priorOffsets[lane])*t;
                points[i]=positions[i]-normals[i]*offset;
            }
            if(reverse)Array.Reverse(points);
            if(points.Any(p=>!Finite(p)))blockers.Add("NON_FINITE_LANE_GEOMETRY");
            var id=$"R:{segment}:{road.Uid:X16}:{side}:{lane}";
            result.Add(new(id,segment,"ROAD_LOOK_SAMPLED",id+":in",id+":out","",true,lane,look.Token,look.Source,points,blockers.Distinct().ToArray()));
        }
        return result;
    }
    internal static bool Finite(Vector3 p)=>float.IsFinite(p.X)&&float.IsFinite(p.Y)&&float.IsFinite(p.Z);
}

