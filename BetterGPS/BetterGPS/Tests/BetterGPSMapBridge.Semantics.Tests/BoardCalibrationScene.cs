using System.Numerics;
using System.Text.Json;
using TruckLib.ScsMap;

internal static class BoardCalibrationScene
{
    public static void Write(Map installed,string output)
    {
        // A known recorded example is a fixture, never a navigation/calibration rule.
        var source=installed.MapItems.Values.OfType<Sign>().Single(s=>s.Uid==0x553A567734180000UL);
        var scene=new Map();var cases=new List<object>();
        foreach(var entry in new[]{("reference",Vector3.Zero,0f,""),("rotated-control",new Vector3(100,0,0),MathF.PI/2,""),
            ("reverse-control",new Vector3(200,0,0),MathF.PI,""),("explicit-wide",new Vector3(300,0,0),0f,"wide_sto1"),
            ("explicit-narrow",new Vector3(400,0,0),0f,"narrow")})
        {
            var sign=Sign.Add(scene,entry.Item2,source.Model,source.SignTemplate);
            sign.Look=source.Look;sign.Variant=entry.Item4;sign.FollowRoadDir=false;
            sign.BoardOverrides=source.BoardOverrides?.ToList()??[];
            sign.SignOverrides=source.SignOverrides?.ToList()??[];
            sign.SignBoards=source.SignBoards?.ToArray()??[];
            sign.Node.Rotation=Quaternion.CreateFromAxisAngle(Vector3.UnitY,entry.Item3);
            cases.Add(new{id=entry.Item1,uid=sign.Uid.ToString("X16"),position=new[]{entry.Item2.X,entry.Item2.Y,entry.Item2.Z},
                rotation=new[]{sign.Node.Rotation.X,sign.Node.Rotation.Y,sign.Node.Rotation.Z,sign.Node.Rotation.W},variant=sign.Variant.ToString()});
        }
        var mapDirectory=Path.Combine(output,"calibration-scene","map");Directory.CreateDirectory(mapDirectory);
        scene.Save(mapDirectory,"bettergps_calibration",false);
        var reread=Map.Open(Path.Combine(mapDirectory,"bettergps_calibration.mbd"));
        var signs=reread.MapItems.Values.OfType<Sign>().ToArray();
        if(signs.Length!=5||signs.Any(s=>s.Model!=source.Model||s.SignTemplate!=source.SignTemplate||s.FollowRoadDir||s.BoardOverrides.Count!=(source.BoardOverrides?.Count??0)||s.SignOverrides.Count!=(source.SignOverrides?.Count??0)))
            throw new Exception("Calibration scene roundtrip lost sign data");
        File.WriteAllText(Path.Combine(output,"calibration-scene","scene.json"),JsonSerializer.Serialize(new{version=1,
            sourceUid=source.Uid.ToString("X16"),sourceModel=source.Model.ToString(),source.SignTemplate,cases,
            serializedRoundtrip=true,renderVerified=false,
            note="Isolated editor fixture, not a replacement for usa. Requires the installed game assets. Coordinate measurements must use actual board anchors, not sign-node positions. No rendered result is asserted."},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine("PASS: isolated 5-sign calibration scene saved and read back; rendering not yet verified.");
    }
}
