using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using BetterGPSNavigation;
using TruckLib.Models;

// Read-only research export. No inferred surface is installed as a calibration.
internal static class BoardSurfaceInspection
{
    public static void Run(string game,string docs,string pathsFile,string output)
    {
        Directory.CreateDirectory(output);
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,ProbePath=".",OutputRoot=output});
        var paths=JsonSerializer.Deserialize<string[]>(File.ReadAllText(pathsFile))
            ?? throw new InvalidDataException("Missing model paths");
        float[] V(Vector3 v)=>[v.X,v.Y,v.Z];
        string Hash(string path){using var stream=assets.Loader.Open(path);return Convert.ToHexString(SHA256.HashData(stream));}
        var models=paths.Select(path=>
        {
            if(!path.StartsWith('/')||path.Split('/').Any(p=>p is "." or "..")||!path.EndsWith(".pmd",StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Expected a virtual PMD path");
            var model=Model.Open(path,assets.Loader);
            return new{path,pmdSha256=Hash(path),pmgSha256=Hash(Path.ChangeExtension(path,"pmg")),
                looks=model.Looks.Select(l=>new{name=l.Name.ToString(),materials=l.Materials}).ToArray(),
                variants=model.Variants.Select(v=>new{name=v.Name.ToString(),attributes=v.Attributes.Select(a=>new{tag=a.Tag.ToString(),a.Type,a.Value}).ToArray()}).ToArray(),
                parts=model.Parts.Select(p=>new{name=p.Name.ToString(),
                    locators=p.Locators.Select(l=>new{name=l.Name.ToString(),position=V(l.Position),rotation=new[]{l.Rotation.X,l.Rotation.Y,l.Rotation.Z,l.Rotation.W},l.Scale}).ToArray(),
                    pieces=p.Pieces.Select((piece,index)=>new{index,piece.Material,vertexCount=piece.Vertices.Count,triangleCount=piece.Triangles.Count,
                        min=piece.Vertices.Count==0?null:V(piece.Vertices.Select(v=>v.Position).Aggregate(Vector3.Min)),
                        max=piece.Vertices.Count==0?null:V(piece.Vertices.Select(v=>v.Position).Aggregate(Vector3.Max)),
                        vertices=piece.Vertices.Select(v=>new{position=V(v.Position),normal=V(v.Normal),
                            uv=v.TextureCoordinates?.Select(uv=>new[]{uv.X,uv.Y}).ToArray()}).ToArray()}).ToArray()}).ToArray()};
        }).ToArray();
        File.WriteAllText(Path.Combine(output,"surfaces.json"),JsonSerializer.Serialize(new{version=1,calibrated=false,models},new JsonSerializerOptions{WriteIndented=true}));
        assets.WriteManifest(Path.Combine(output,"asset-manifest.json"));
        Console.WriteLine($"Exported geometry of {models.Length} models; no calibration applied.");
    }
}
