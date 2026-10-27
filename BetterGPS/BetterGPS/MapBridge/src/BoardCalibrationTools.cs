using System.Text.Json;

namespace BetterGPSNavigation;

internal static class BoardCalibrationTools
{
    public static void WritePacket(string directory,string stem,IReadOnlyList<SignGroupProof> groups)
    {
        var output=Path.Combine(directory,stem+"_calibration");Directory.CreateDirectory(output);
        foreach(var g in groups.Where(g=>g.BoardGeometry is {Surfaces.Length:1}&&g.BoardWidthM is >0&&g.BoardHeightM is >0)
            .DistinctBy(g=>(g.SignUid,g.AreaName)))
        {
            var key=g.SignUid+"_"+g.AreaName;
            if(key.IndexOfAny(Path.GetInvalidFileNameChars())>=0)continue;
            var board=g.BoardGeometry!;var w=(float)g.BoardWidthM!;var h=(float)g.BoardHeightM!;
            var controls=new[]{new[]{0f,0f},new[]{w,0f},new[]{w,h},new[]{0f,h}};
            var hypotheses=BoardPlacementResolver.Hypotheses(board);
            var blankSamples=controls.Select(p=>new{role="CONTROL",drawingPoint=(float[]?)p,measuredWorldPoint=(float[]?)null})
                .Append(new{role="ARROW_TIP",drawingPoint=(float[]?)null,measuredWorldPoint=(float[]?)null}).ToArray();
            var draft=new{version=1,calibrated=false,g.SignUid,g.AreaName,g.BoardWidthM,g.BoardHeightM,
                boardGeometry=board,supportGeometry=g.SupportGeometry,hypotheses,
                groups=groups.Where(x=>x.SignUid==g.SignUid&&x.AreaName==g.AreaName).Select(x=>new{x.GroupId,x.Road,x.Cardinal,x.Destination,x.Arrows}),
                note="Predictions are hypotheses, never measurements. Fill measuredWorldPoint from a metrically referenced SCS editor/render test. Independent rotated case required."};
            File.WriteAllText(Path.Combine(output,key+"_candidates.json"),JsonSerializer.Serialize(draft,SignGroupProof.Json));
            if(g.CalibrationDraft is {} profile)
            {
                var profilePath=Path.Combine(output,key+"_profile.template.json");
                if(!File.Exists(profilePath))File.WriteAllText(profilePath,JsonSerializer.Serialize(new[]{profile},SignGroupProof.Json));
            }
            var measurements=new{version=1,modelPath=board.ModelPath,variant=board.Variant,g.AreaName,
                arrowTipUv=(float[]?)null,sceneSha256=new Dictionary<string,string>(),cases=new[]{
                    new{id="reference",holdout=false,position=new[]{0f,0f,0f},rotation=new[]{0f,0f,0f,1f},scale=1f,
                        samples=blankSamples},
                    new{id="rotated-control",holdout=true,position=new[]{10f,0f,20f},rotation=new[]{0f,.70710677f,0f,.70710677f},scale=1f,
                        samples=blankSamples}}};
            // Regenerating a route must not overwrite the user's measurements.
            var file=Path.Combine(output,key+"_measurements.template.json");
            if(!File.Exists(file))File.WriteAllText(file,JsonSerializer.Serialize(measurements,SignGroupProof.Json));
        }
    }
    public static int Check(string candidates,string measurements,string transform,string output)
    {
        try {
            using var candidate=JsonDocument.Parse(File.ReadAllText(candidates));
            var root=candidate.RootElement;
            var board=root.GetProperty("boardGeometry").Deserialize<BoardModelEvidence>(LiveProtocol.Json)!;
            var m=JsonSerializer.Deserialize<BoardMeasurements>(File.ReadAllText(measurements),LiveProtocol.Json)!;
            var t=JsonSerializer.Deserialize<BoardDrawingTransform>(File.ReadAllText(transform),LiveProtocol.Json)!;
            var metadata=m is not null&&m.ModelPath==board.ModelPath&&m.Variant==board.Variant&&m.AreaName==root.GetProperty("areaName").GetString();
            double error=0;
            var pass=metadata&&BoardPlacementResolver.ValidateMeasurements(m!,t,board,root.GetProperty("boardWidthM").GetDouble(),root.GetProperty("boardHeightM").GetDouble(),.05,out error);
            File.WriteAllText(output,JsonSerializer.Serialize(new{measurementsPass=pass,maxErrorM=error,toleranceM=.05,
                productionCalibrated=false,note="This checks submitted coordinates. It does not prove they were measured in SCS or unlock HIGH; pinned production profile and independent lane proofs are still required."},SignGroupProof.Json));
            Console.WriteLine(pass?"PASS: geometric controls within 0.05 m. Production profile remains separate.":"FAIL: missing, incompatible or inaccurate measurements.");return pass?0:2;
        }catch(Exception e) when(e is IOException or JsonException or InvalidOperationException or ArgumentException)
        {Console.Error.WriteLine("Calibration check failed: "+e.Message);return 2;}
    }
}
