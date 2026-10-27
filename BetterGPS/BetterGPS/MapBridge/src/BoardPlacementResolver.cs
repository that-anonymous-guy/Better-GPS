using System.Numerics;

namespace BetterGPSNavigation;

internal sealed record BoardDrawingTransform(float[] Origin,float[] AxisX,float[] AxisY,string Attachment);
internal sealed record BoardPlacementTrace(float[] DrawingPoint,float[] ModelPoint,float[] WorldPoint,
    float[][] Contour,float[] WorldNormal,string Hypothesis)
{
    public float[] Anchor {get;init;}=[];
    public float[] Rotation {get;init;}=[];
    public float Scale {get;init;}
}
internal sealed record BoardMeasurement(float[] DrawingPoint,float[] MeasuredWorldPoint)
{
    public string Role {get;init;}="CONTROL";
}
internal sealed record BoardMeasurementCase(string Id,bool Holdout,float[] Position,float[] Rotation,float Scale,BoardMeasurement[] Samples);
internal sealed record BoardMeasurements(int Version,string ModelPath,string Variant,string AreaName,float[] ArrowTipUv,BoardMeasurementCase[] Cases)
{
    public Dictionary<string,string> SceneSha256 {get;init;}=[];
}

internal static class BoardPlacementResolver
{
    public static bool Valid(BoardDrawingTransform? t)=>t is not null&&BoardModelResolver.Valid(t.Origin,3)&&
        BoardModelResolver.Valid(t.AxisX,3)&&BoardModelResolver.Valid(t.AxisY,3)&&
        Math.Abs(BoardModelResolver.V(t.AxisX).Length()-1)<.01&&Math.Abs(BoardModelResolver.V(t.AxisY).Length()-1)<.01&&
        Math.Abs(Vector3.Dot(BoardModelResolver.V(t.AxisX),BoardModelResolver.V(t.AxisY)))<.01&&
        t.Attachment is "MODEL_ORIGIN" or "BOARD1_ORIGIN";
    public static BoardDrawingTransform[] Hypotheses(BoardModelEvidence evidence)
    {
        if(evidence.Surfaces.Length!=1)return [];
        var s=evidence.Surfaces[0];var a=BoardModelResolver.V(s.Min);var b=BoardModelResolver.V(s.Max);
        return (evidence.LocatorPosition is null?new[]{"MODEL_ORIGIN"}:new[]{"MODEL_ORIGIN","BOARD1_ORIGIN"})
            .SelectMany(attachment=>new[]{new BoardDrawingTransform([a.X,a.Y,(a.Z+b.Z)/2],[1,0,0],[0,1,0],attachment),
                new([b.X,a.Y,(a.Z+b.Z)/2],[-1,0,0],[0,1,0],attachment)}).ToArray();
    }
    public static string Name(BoardDrawingTransform t)=>t.Attachment+"/"+(t.AxisX[0]<0?"X_REVERSE":"X_FORWARD");
    private static Vector3 Attached(Vector3 p,BoardDrawingTransform t,BoardModelEvidence board)
    {
        if(t.Attachment=="MODEL_ORIGIN")return p;
        if(!BoardModelResolver.Valid(board.LocatorPosition,3)||!BoardModelResolver.Valid(board.LocatorRotation,4)||!float.IsFinite(board.LocatorScale)||board.LocatorScale<=0)
            throw new InvalidDataException("BOARD_LOCATOR_UNRESOLVED");
        var q=board.LocatorRotation!;var r=new Quaternion(q[0],q[1],q[2],q[3]);
        if(Math.Abs(r.Length()-1)>.01)throw new InvalidDataException("BOARD_LOCATOR_ROTATION_INVALID");
        return Vector3.Transform((p-BoardModelResolver.V(board.LocatorPosition!))/board.LocatorScale,Quaternion.Inverse(r));
    }
    public static BoardPlacementTrace Place(Vector2 drawing,BoardDrawingTransform t,BoardModelEvidence board,
        Vector3 anchor,Quaternion rotation,float scale,double width,double height)
    {
        if(!Valid(t)||!RoadLaneGeometryResolver.Finite(anchor)||!BoardModelResolver.Valid([rotation.X,rotation.Y,rotation.Z,rotation.W],4)||Math.Abs(rotation.Length()-1)>.01||!float.IsFinite(scale)||scale<=0||
            !float.IsFinite(drawing.X)||!float.IsFinite(drawing.Y)||!double.IsFinite(width+height)||width<=0||height<=0)throw new InvalidDataException("INVALID_PLACEMENT_TRANSFORM");
        Vector3 Model(Vector2 p)=>BoardModelResolver.V(t.Origin)+BoardModelResolver.V(t.AxisX)*p.X+BoardModelResolver.V(t.AxisY)*p.Y;
        Vector3 World(Vector2 p){var v=anchor+Vector3.Transform(Attached(Model(p),t,board)*scale,rotation);if(!RoadLaneGeometryResolver.Finite(v))throw new InvalidDataException("PLACEMENT_OVERFLOW");return v;}
        var normal=board.Surfaces.Length==1?BoardModelResolver.V(board.Surfaces[0].Normal):Vector3.Normalize(Vector3.Cross(BoardModelResolver.V(t.AxisX),BoardModelResolver.V(t.AxisY)));
        if(t.Attachment=="BOARD1_ORIGIN")
        {
            var q=board.LocatorRotation!;normal=Vector3.Transform(normal,Quaternion.Inverse(new(q[0],q[1],q[2],q[3])));
        }
        return new([drawing.X,drawing.Y],BoardModelResolver.V(Model(drawing)),BoardModelResolver.V(World(drawing)),
            new[]{Vector2.Zero,new((float)width,0),new((float)width,(float)height),new(0,(float)height)}.Select(p=>BoardModelResolver.V(World(p))).ToArray(),
            BoardModelResolver.V(Vector3.Transform(normal,rotation)),Name(t))
            {Anchor=BoardModelResolver.V(anchor),Rotation=[rotation.X,rotation.Y,rotation.Z,rotation.W],Scale=scale};
    }
    public static bool ValidateMeasurements(BoardMeasurements m,BoardDrawingTransform t,BoardModelEvidence board,double width,double height,double tolerance,out double maxError)
    {
        maxError=0;
        if(m.Version!=1||!Valid(t)||!double.IsFinite(tolerance)||tolerance<=0||tolerance>.25||m.Cases is null||m.Cases.Length<2||m.Cases.Any(c=>c is null)||
            !m.Cases.Any(c=>c.Holdout)||!m.Cases.Any(c=>!c.Holdout)||m.Cases.Select(c=>c.Id).Distinct().Count()!=m.Cases.Length||
            !BoardModelResolver.Valid(m.ArrowTipUv,2)||m.ArrowTipUv.Any(v=>v<0||v>1))return false;
        try
        {
            foreach(var c in m.Cases)
            {
                if(string.IsNullOrWhiteSpace(c.Id)||!BoardModelResolver.Valid(c.Position,3)||!BoardModelResolver.Valid(c.Rotation,4)||c.Samples is null||c.Samples.Length<5||c.Samples.Any(s=>s is null)||!c.Samples.Any(s=>s.Role=="ARROW_TIP"))return false;
                if(c.Samples.Any(s=>!BoardModelResolver.Valid(s.DrawingPoint,2)||!BoardModelResolver.Valid(s.MeasuredWorldPoint,3)))return false;
                // Distributed controls, not four copies of a single point or collinear points.
                var p=c.Samples.Select(s=>new Vector2(s.DrawingPoint[0],s.DrawingPoint[1])).ToArray();
                if(!p.Any(a=>p.Any(b=>p.Any(d=>Math.Abs((b.X-a.X)*(d.Y-a.Y)-(b.Y-a.Y)*(d.X-a.X))>width*height*.1))))return false;
                var q=c.Rotation;var r=new Quaternion(q[0],q[1],q[2],q[3]);
                foreach(var s in c.Samples)
                {
                    var predicted=Place(new(s.DrawingPoint[0],s.DrawingPoint[1]),t,board,BoardModelResolver.V(c.Position),r,c.Scale,width,height);
                    var error=Vector3.Distance(BoardModelResolver.V(predicted.WorldPoint),BoardModelResolver.V(s.MeasuredWorldPoint));
                    maxError=Math.Max(maxError,error);if(error>tolerance)return false;
                }
            }
            Quaternion Rotation(BoardMeasurementCase c)=>new(c.Rotation[0],c.Rotation[1],c.Rotation[2],c.Rotation[3]);
            if(!m.Cases.Where(c=>!c.Holdout).Any(a=>m.Cases.Where(c=>c.Holdout).Any(b=>Math.Abs(Quaternion.Dot(Rotation(a),Rotation(b)))<.924)))return false;
            return true;
        }
        catch(InvalidDataException){return false;}
    }
}
