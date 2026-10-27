using System.Numerics;
using BetterGPSNavigation;
using TruckLib.Models;

internal static class BoardGeometryTests
{
    public static void Real(string groupsFile,string independentInspection)
    {
        using var groups=System.Text.Json.JsonDocument.Parse(File.ReadAllText(groupsFile));
        using var inspection=System.Text.Json.JsonDocument.Parse(File.ReadAllText(independentInspection));
        var all=groups.RootElement.GetProperty("groups").EnumerateArray().ToArray();
        var portique=all.Where(g=>g.GetProperty("signUid").GetString()=="553A567734180000").ToArray();
        int n=0;void Check(bool ok,string message){n++;if(!ok)throw new Exception(message);}
        Check(portique.Length==9,"Recorded portique groups lost");
        Check(all.All(g=>g.GetProperty("confidence").GetString()!="HIGH"),"Unmeasured geometry became HIGH");
        foreach(var group in portique.DistinctBy(g=>g.GetProperty("areaName").GetString()))
        {
            var board=group.GetProperty("boardGeometry");var path=board.GetProperty("modelPath").GetString();
            var model=inspection.RootElement.GetProperty("models").EnumerateArray().Single(m=>m.GetProperty("path").GetString()==path);
            Check(board.GetProperty("status").GetString()=="SURFACE_CANDIDATE","Board geometry unresolved");
            Check(board.GetProperty("assetSha256").GetProperty(path!).GetString()==model.GetProperty("pmdSha256").GetString(),"Physical board hash differs from inspection");
            var surface=board.GetProperty("surfaces")[0];var piece=model.GetProperty("parts").EnumerateArray().Single(p=>p.GetProperty("name").GetString()==surface.GetProperty("part").GetString()).GetProperty("pieces")[surface.GetProperty("piece").GetInt32()];
            bool Same(string key)=>surface.GetProperty(key).EnumerateArray().Select(v=>v.GetSingle()).SequenceEqual(piece.GetProperty(key).EnumerateArray().Select(v=>v.GetSingle()));
            Check(Same("min"),"Surface minimum differs from raw mesh inspection");
            Check(Same("max"),"Surface maximum differs from raw mesh inspection");
            Check(surface.GetProperty("part").GetString()!="light_main","Lighting geometry used as plaque");
            Check(board.GetProperty("variant").GetString()=="green","Wrong board-model variant namespace");
        }
        var current=portique.Where(g=>g.GetProperty("eventIndex").GetInt32()==5).ToArray();
        Check(current.Single(g=>g.GetProperty("destination").GetString()=="San Diego").GetProperty("status").GetString()=="OTHER_EXIT_CANDIDATE","Recorded South plaque still assigned to selected exit");
        Check(current.Where(g=>g.GetProperty("destination").GetString()!="San Diego").All(g=>g.GetProperty("status").GetString()=="GPS_COMPATIBLE_CANDIDATE"),"Recorded North/East group compatibility lost");
        Console.WriteLine($"PASS: {n} real board/namespace/inspection/exit assertions. Still no render calibration.");
    }
    public static void Run()
    {
        int n=0;void Check(bool ok,string message){n++;if(!ok)throw new Exception(message);}
        void Reject(Action action,string message){bool rejected=false;try{action();}catch(InvalidDataException){rejected=true;}Check(rejected,message);}
        var vertices=new[]{new Vertex(0,0,0),new Vertex(6,0,0),new Vertex(6,3,0),new Vertex(0,3,0)};
        foreach(var v in vertices)v.Normal=-Vector3.UnitZ;
        var surface=BoardModelResolver.Surface("not_a_hardcoded_part",0,vertices,"/test.mat",6,3)!;
        Check(surface is not null,"Planar measured board rejected");
        Check(BoardModelResolver.Surface("x",0,vertices,"x",5,3) is null,"Wrong dimensions accepted");
        vertices[0].Position=new(0,0,.1f);
        Check(BoardModelResolver.Surface("x",0,vertices,"x",6,3) is null,"Nonplanar board accepted");
        vertices[0].Position=Vector3.Zero;vertices[0].Normal=Vector3.UnitZ;
        Check(BoardModelResolver.Surface("x",0,vertices,"x",6,3) is null,"Opposing normals accepted");
        vertices[0].Normal=-Vector3.UnitZ;
        var board=new BoardModelEvidence("SURFACE_CANDIDATE","x","/x.pmd","default","green",[surface!],[1,2,0],[0,0,0,1],2,[],[]);
        var transform=new BoardDrawingTransform([0,0,0],[1,0,0],[0,1,0],"MODEL_ORIGIN");
        var trace=BoardPlacementResolver.Place(new(2,1),transform,board,new(10,20,30),Quaternion.Identity,2,6,3);
        Check(Vector3.Distance(BoardModelResolver.V(trace.WorldPoint),new(14,22,30))<1e-5,"Translation/scale incorrect");
        Check(trace.Contour.Length==4&&trace.Anchor.SequenceEqual(new float[]{10,20,30}),"Incomplete trace");
        var turn90=Quaternion.CreateFromAxisAngle(Vector3.UnitY,MathF.PI/2);
        trace=BoardPlacementResolver.Place(new(2,1),transform,board,Vector3.Zero,turn90,1,6,3);
        Check(Vector3.Distance(BoardModelResolver.V(trace.WorldPoint),new(0,1,-2))<1e-5,"90 degree transform inverted");
        var turn180=Quaternion.CreateFromAxisAngle(Vector3.UnitY,MathF.PI);
        trace=BoardPlacementResolver.Place(new(2,1),transform,board,Vector3.Zero,turn180,1,6,3);
        Check(Vector3.Distance(BoardModelResolver.V(trace.WorldPoint),new(-2,1,0))<1e-5,"180 degree transform incorrect");
        Check(Vector3.Distance(BoardModelResolver.V(trace.WorldNormal),Vector3.UnitZ)<1e-5,"Front normal not transformed");
        trace=BoardPlacementResolver.Place(new(2,1),transform with{Attachment="BOARD1_ORIGIN"},board,Vector3.Zero,Quaternion.Identity,1,6,3);
        Check(Vector3.Distance(BoardModelResolver.V(trace.WorldPoint),new(.5f,-.5f,0))<1e-5,"Locator inverse/scale incorrect");
        Reject(()=>BoardPlacementResolver.Place(Vector2.Zero,transform,board,Vector3.Zero,new(float.NaN,0,0,1),1,6,3),"NaN quaternion accepted");
        Reject(()=>BoardPlacementResolver.Place(Vector2.Zero,transform,board,Vector3.Zero,Quaternion.Identity,0,6,3),"Zero scale accepted");
        Reject(()=>BoardPlacementResolver.Place(Vector2.Zero,transform with{AxisY=[1,0,0]},board,Vector3.Zero,Quaternion.Identity,1,6,3),"Parallel frame accepted");
        Check(BoardPlacementResolver.Hypotheses(board).Length==4,"Shared board hypotheses missing");
        Check(BoardPlacementResolver.Hypotheses(board with{Surfaces=[]}).Length==0,"Surface-free placement invented");
        // Independently specified known coordinates, including a rotated control not used to fit the frame.
        var samples=new[]{new BoardMeasurement([0,0],[0,0,0]),new([6,0],[6,0,0]),new([6,3],[6,3,0]),new([0,3],[0,3,0]),new([2,1],[2,1,0]){Role="ARROW_TIP"}};
        var holdout=new[]{new BoardMeasurement([0,0],[10,0,20]),new([6,0],[10,0,14]),new([6,3],[10,3,14]),new([0,3],[10,3,20]),new([2,1],[10,1,18]){Role="ARROW_TIP"}};
        var measurements=new BoardMeasurements(1,"/x.pmd","green","sb1",[.5f,0],
            [new("fit",false,[0,0,0],[0,0,0,1],1,samples),new("control",true,[10,0,20],[turn90.X,turn90.Y,turn90.Z,turn90.W],1,holdout)]);
        bool Valid(BoardMeasurements m)=>BoardPlacementResolver.ValidateMeasurements(m,transform,board,6,3,.05,out _);
        Check(Valid(measurements),"Independent known controls rejected");
        Check(!Valid(measurements with{Cases=[measurements.Cases[0]]}),"Missing holdout accepted");
        Check(!Valid(measurements with{Cases=[measurements.Cases[0],measurements.Cases[1] with{Holdout=false}]}),"No independent control accepted");
        Check(!Valid(measurements with{Cases=[measurements.Cases[0],measurements.Cases[0] with{Id="duplicate-pose",Holdout=true}]}),"Same orientation used as independent rotated control");
        Check(!Valid(measurements with{Cases=measurements.Cases.Select(c=>c with{Samples=c.Samples.Select(s=>s with{Role="CONTROL"}).ToArray()}).ToArray()}),"Unmeasured arrow tip accepted");
        Check(!Valid(measurements with{Cases=[measurements.Cases[0],measurements.Cases[1] with{Samples=[new([0,0],[10,0,20]),new([1,0],[10,0,19]),new([2,0],[10,0,18]),new([3,0],[10,0,17])]}]}),"Collinear controls accepted");
        holdout[2]=new([6,3],[11,3,14]);
        Check(!Valid(measurements),"One metre error accepted");
        Check(!Valid(measurements with{ArrowTipUv=[float.NaN,0]}),"Invalid atlas tip accepted");
        Console.WriteLine($"PASS: {n} board geometry/measurement assertions (synthetic independent controls).");
    }
}
