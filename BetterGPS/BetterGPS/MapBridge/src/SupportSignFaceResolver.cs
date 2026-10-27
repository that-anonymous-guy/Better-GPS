using System.Numerics;
using System.Text.RegularExpressions;
using TruckLib;
using TruckLib.Models;

namespace BetterGPSNavigation;

internal sealed record SupportSignFace(Vector3 Position,Vector3 Normal,string Status);

/// <summary>Some board templates draw directly on the support mesh (model_name empty).
/// Their face belongs to support space, not to the rotated text locator. No render calibration is implied.</summary>
internal sealed class SupportSignFaceResolver(IFileSystem fs,BoardModelResolver boards)
{
    private readonly Dictionary<string,Model> models=[];
    private readonly Dictionary<string,string> materials=[];
    private sealed record MeshPlanes((Vector3 Min,Vector3 Max,Vector3 Normal)[] Value);
    private readonly System.Runtime.CompilerServices.ConditionalWeakTable<IReadOnlyList<Vertex>,MeshPlanes> planes=new();
    private sealed record LocalFace(float[] Position,float[] Normal,string Status);
    private readonly Dictionary<string,LocalFace?> localFaces=[];
    public SupportSignFace? Resolve(ParsedSign sign,ExtractedSignPanel panel)
    {
        if(sign.FollowRoadDir||panel.BoardOffset.Any(x=>Math.Abs(x)>1e-6)||!BoardModelResolver.Valid(panel.Anchor.LocalPosition,3))return null;
        var key=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Json.JsonSerializer.SerializeToUtf8Bytes(
            new{sign.Model,sign.ModelDesc,sign.Variant,sign.Look,panel.BoardPath,panel.Anchor.LocalPosition},LiveProtocol.Json)));
        if(!localFaces.TryGetValue(key,out var local))localFaces[key]=local=AssetIndexCache.GetShared(fs,"support-face-"+key,()=>
        {
            var face=ResolveFresh(sign with{Position=Vector3.Zero,Rotation=Quaternion.Identity},panel);
            return face is null?null:new LocalFace(BoardModelResolver.V(face.Position),BoardModelResolver.V(face.Normal),face.Status);
        });
        return local is null?null:new(sign.Position+Vector3.Transform(BoardModelResolver.V(local.Position),sign.Rotation),
            Vector3.Normalize(Vector3.Transform(BoardModelResolver.V(local.Normal),sign.Rotation)),local.Status);
    }
    private SupportSignFace? ResolveFresh(ParsedSign sign,ExtractedSignPanel panel)
    {
        try
        {
            var support=boards.ResolveSupport(sign);
            if(!models.TryGetValue(sign.ModelDesc,out var model))models[sign.ModelDesc]=model=ModelDocumentCache.Open(sign.ModelDesc,fs);
            var variant=support.Variant.Length>0?support.Variant:sign.Variant.Length>0?sign.Variant:
                model.Variants.Count==1?model.Variants[0].Name.ToString():"";
            if(variant.Length==0)return null;
            // An empty instance look with one possible model look is unambiguous; several looks stay unresolved.
            var look=sign.Look.Length>0?model.Looks.SingleOrDefault(l=>l.Name.ToString()==sign.Look):model.Looks.Count==1?model.Looks[0]:null;
            if(look is null)return null;
            var point=BoardModelResolver.V(panel.Anchor.LocalPosition!);
            var candidates=new List<(Vector3 Normal,Vector3 Center,double Distance)>();
            foreach(var part in BoardModelResolver.ActiveParts(model,variant))
            foreach(var piece in part.Pieces)
            {
                if(piece.Material>=look.Materials.Count)continue;
                var material=look.Materials[(int)piece.Material];
                if(!materials.TryGetValue(material,out var text))materials[material]=text=fs.ReadAllText(material);
                bool street=panel.BoardPath.StartsWith("/def/sign/board/info/us_streetname_",StringComparison.OrdinalIgnoreCase);
                if(!Regex.IsMatch(text,"effect\\s*:\\s*\"eut2\\.retroreflective(?:\\.[^\"]+)?\"")&&
                    !(street&&Regex.IsMatch(text,"effect\\s*:\\s*\"eut2\\.dif\\.rfx\"")))continue;
                // Illuminated plates are closed meshes: front, back and edges share one piece.
                // Resolve each planar face independently before comparing it with the text locator.
                foreach(var plane in planes.GetValue(piece.Vertices,v=>new(Planes(v).ToArray())).Value)
                {
                var (min,max,normal)=plane;
                var distance=Vector3.Distance(point,Vector3.Clamp(point,min,max));
                candidates.Add((normal,(min+max)/2,distance));
                }
            }
            var sorted=candidates.OrderBy(c=>c.Distance).ToArray();
            if(sorted.Length==0||sorted[0].Distance>.25||sorted.Skip(1).Any(c=>c.Distance<sorted[0].Distance+.05))return null;
            return new(sign.Position+Vector3.Transform(sorted[0].Center,sign.Rotation),
                Vector3.Normalize(Vector3.Transform(sorted[0].Normal,sign.Rotation)),"SUPPORT_MESH_FACE_CANDIDATE_NOT_RENDER_VERIFIED");
        }
        catch(Exception e) when(e is IOException or InvalidDataException or ArgumentException){return null;}
    }
    internal static IEnumerable<(Vector3 Min,Vector3 Max,Vector3 Normal)> Planes(IReadOnlyList<Vertex> vertices)
    {
        var remaining=vertices.ToList();
        while(remaining.Count>0)
        {
            var first=remaining[0];
            var face=remaining.Where(v=>Vector3.Dot(first.Normal,v.Normal)>.999&&
                Math.Abs(Vector3.Dot(v.Position-first.Position,first.Normal))<.005).ToArray();
            remaining.Remove(first);
            var members=face.ToHashSet();
            remaining.RemoveAll(members.Contains);
            if(Plane(face) is {} plane)yield return plane;
        }
    }
    internal static (Vector3 Min,Vector3 Max,Vector3 Normal)? Plane(IReadOnlyList<Vertex> vertices)
    {
        if(vertices.Count<3||vertices.Any(v=>!RoadLaneGeometryResolver.Finite(v.Position)||!RoadLaneGeometryResolver.Finite(v.Normal)))return null;
        var n=vertices[0].Normal;if(Math.Abs(n.Length()-1)>.01||vertices.Any(v=>Math.Abs(v.Normal.Length()-1)>.01||Vector3.Dot(n,v.Normal)<.99))return null;
        if(vertices.Any(v=>Math.Abs(Vector3.Dot(v.Position-vertices[0].Position,n))>.005))return null;
        var min=vertices.Select(v=>v.Position).Aggregate(Vector3.Min);var max=vertices.Select(v=>v.Position).Aggregate(Vector3.Max);
        if((max-min).LengthSquared()<.01)return null;
        return(min,max,Vector3.Normalize(n));
    }
}
