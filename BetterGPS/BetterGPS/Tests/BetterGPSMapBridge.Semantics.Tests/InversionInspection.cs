using System.Numerics;
using System.Text.Json;
using BetterGPSNavigation;
using TruckLib;
using TruckLib.Models;

internal static class InversionInspection
{
    // Inspection of installed assets only. No alteration of live decisions or map assets.
    public static void Run(string game,string docs,string output)
    {
        Directory.CreateDirectory(output);
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,ProbePath=".",OutputRoot=output});
        var resolver=new PpdResolver(assets.Loader);
        if(!resolver.TryResolve("2o084",out var path,out var ppd,out var error)||ppd is null)throw new Exception(error);
        float[] V(Vector3 v)=>[v.X,v.Y,v.Z];
        var prefab=new{token="2o084",path,nodes=ppd.Nodes.Select((n,i)=>new{index=i,position=V(n.Position),direction=V(n.Direction),n.InputLines,n.OutputLines}),
            curves=ppd.NavCurves.Select((c,i)=>new{index=i,start=V(c.StartPosition),end=V(c.EndPosition),
                startTangent=V(-Vector3.Transform(Vector3.UnitZ,c.StartRotation)),endTangent=V(-Vector3.Transform(Vector3.UnitZ,c.EndRotation)),
                c.NextLines,c.PreviousLines,blinker=c.Blinker.ToString(),c.Length})};
        var model=Model.Open("/model/sign/navigation/ca_overhead_stand_01_4fwy.pmd",assets.Loader);
        var signModel=new{model.Variants,parts=model.Parts.Select(p=>new{part=p.Name.ToString(),locators=p.Locators.Select(l=>new{
            name=l.Name.ToString(),position=V(l.Position),rotation=new[]{l.Rotation.X,l.Rotation.Y,l.Rotation.Z,l.Rotation.W},l.Scale,
            xAxis=V(Vector3.Transform(Vector3.UnitX,l.Rotation)),yAxis=V(Vector3.Transform(Vector3.UnitY,l.Rotation)),zAxis=V(Vector3.Transform(Vector3.UnitZ,l.Rotation))})})};
        var joining=new[]{ppd.NavCurves[1].EndPosition};var other=new[]{0,3,4}.Select(i=>ppd.NavCurves[i].EndPosition).ToArray();
        var direction=-Vector3.Transform(Vector3.UnitZ,ppd.NavCurves[1].EndRotation);
        var side=ManeuverFeatureExtractor.MergeSideFromEndpoints(joining,other,direction);
        if(side!="LEFT")throw new Exception("Installed 2o084 merge must join toward LEFT");
        var mergeProof=new{selectedCurves=new[]{2,1},otherEntryCurves=new[]{4,0,3},joiningEndpoints=joining.Select(V),otherEndpoints=other.Select(V),exitDirection=V(direction),correctedSide=side};
        var related=new List<object>();
        foreach(var token in new[]{"2o083","2o071"})
        {
            if(!resolver.TryResolve(token,out var file,out var d,out var failure)||d is null)throw new Exception(failure);
            related.Add(new{token,path=file,nodes=d.Nodes.Select((n,i)=>new{index=i,n.InputLines,n.OutputLines}),
                curves=d.NavCurves.Select((c,i)=>new{index=i,start=V(c.StartPosition),end=V(c.EndPosition),c.NextLines,
                    endTangent=V(-Vector3.Transform(Vector3.UnitZ,c.EndRotation))})});
        }
        File.WriteAllText(Path.Combine(output,"installed-assets.json"),JsonSerializer.Serialize(new{prefab,related,signModel,mergeProof},new JsonSerializerOptions{
            PropertyNamingPolicy=JsonNamingPolicy.CamelCase,WriteIndented=true}));
        Console.WriteLine("Extracted installed merge PPD and sign locators/variants: "+Path.GetFullPath(output));
    }
}
