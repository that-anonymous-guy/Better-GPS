using System.Numerics;
using System.Security.Cryptography;
using TruckLib;
using TruckLib.Models;
using TruckLib.Sii;

namespace BetterGPSNavigation;

internal sealed record BoardSurfaceFrame(string Part,int Piece,float[] Min,float[] Max,float[] Normal,string Material);
internal sealed record BoardModelEvidence(string Status,string Token,string ModelPath,string Look,string Variant,
    BoardSurfaceFrame[] Surfaces,float[]? LocatorPosition,float[]? LocatorRotation,float LocatorScale,
    Dictionary<string,string> AssetSha256,string[] Blockers);
internal sealed record SupportVariantEvidence(string Variant,string Status,Dictionary<string,string> AssetSha256);

internal sealed class BoardModelResolver(IFileSystem fs)
{
    private readonly Dictionary<string,BoardModelEvidence> cache=new(StringComparer.Ordinal);
    private sealed record ModelDefinition(string Name,string Class,string Model,string Look,string Variant,string[] Paths);
    private readonly Dictionary<string,List<ModelDefinition>> definitions=new(StringComparer.Ordinal);
    private readonly Dictionary<string,List<ModelDefinition>> supports=new(StringComparer.Ordinal);
    private bool loaded;
    private readonly Dictionary<string,string> hashesByPath=new(StringComparer.Ordinal);
    private string Hash(string path)
    {
        if(hashesByPath.TryGetValue(path,out var cached))return cached;
        return hashesByPath[path]=AssetFileHash.Read(fs,path);
    }
    internal static float[] V(Vector3 v)=>[v.X,v.Y,v.Z];
    internal static Vector3 V(float[] a)=>new(a[0],a[1],a[2]);
    internal static bool Valid(float[]? a,int size)=>a is not null&&a.Length==size&&a.All(float.IsFinite);
    private static string Text(Unit u,string key)=>u.Attributes.TryGetValue(key,out var v)?SignTemplateResolver.CleanToken((object?)v):"";
    private static string[] Paths(string root,SiiFile file)=>new[]{root}.Concat(file.Includes??[]).Distinct(StringComparer.Ordinal).ToArray();

    internal static Part[] ActiveParts(Model model,string variant)
    {
        // Definition strings can spell "Default" while the packed model token is "default".
        var matches=model.Variants.Where(v=>v.Name.ToString().Equals(variant,StringComparison.OrdinalIgnoreCase)).ToArray();
        if(variant.Length==0||matches.Length!=1)throw new InvalidDataException("VARIANT_UNRESOLVED");
        var attrs=matches[0].Attributes;
        if(attrs.Count!=model.Parts.Count||attrs.Any(a=>a.Tag.ToString()!="visible"||a.Type!=0||a.Value>1))
            throw new InvalidDataException("VARIANT_SCHEMA_UNSUPPORTED");
        return model.Parts.Where((p,i)=>attrs[i].Value==1).ToArray();
    }
    private void Load()
    {
        if(loaded)return;
        var rows=AssetIndexCache.GetShared(fs,"board-support-definitions",()=>LoadDefinitions());
        foreach(var row in rows)
        {
            var target=row.Class=="sign_model"?supports:definitions;
            if(!target.TryGetValue(row.Name,out var values))target[row.Name]=values=[];
            values.Add(row);
        }
        loaded=true;
    }
    private ModelDefinition[] LoadDefinitions()
    {
        var rows=new List<ModelDefinition>();
        foreach(var path in fs.GetFiles("/def/world/").Where(p=>Path.GetFileName(p).StartsWith("sign",StringComparison.Ordinal)&&p.EndsWith(".sii",StringComparison.Ordinal)).Order(StringComparer.Ordinal))
        {
            var file=SiiDocumentCache.Open(path,fs);
            foreach(var unit in file.Units.Where(u=>u.Class is "sign_board_model" or "sign_model"))
            {
                rows.Add(new(unit.Name,unit.Class,Text(unit,"model_desc"),Text(unit,"look_name"),Text(unit,"variant_name"),Paths(path,file)));
            }
        }
        return rows.ToArray();
    }
    public SupportVariantEvidence ResolveSupport(ParsedSign sign)
    {
        try {
        Load();var hashes=new Dictionary<string,string>(StringComparer.Ordinal);
        if(!supports.TryGetValue("sign."+sign.Model,out var defs)||defs.Count!=1||defs[0].Model!=sign.ModelDesc)
            return new("","UNRESOLVED",hashes);
        foreach(var p in defs[0].Paths)hashes[p]=Hash(p);
        var variant=sign.Variant.Length>0?sign.Variant:defs[0].Variant;
        return new(variant,variant.Length==0?"UNRESOLVED":sign.Variant.Length>0?"INSTANCE":"DEFINITION_DEFAULT",hashes);
        }catch(Exception e) when(e is IOException or ArgumentException or InvalidDataException or UnsupportedVersionException)
        {return new("","READ_ERROR",new(StringComparer.Ordinal));}
    }
    public BoardModelEvidence Resolve(ExtractedSignPanel panel)
    {
        if(cache.TryGetValue(panel.BoardPath,out var cached))return cached;
        var key=Convert.ToHexString(SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(panel.BoardPath)));
        return cache[panel.BoardPath]=AssetIndexCache.GetShared(fs,"board-frame-"+key,()=>ResolveFresh(panel));
    }
    private BoardModelEvidence ResolveFresh(ExtractedSignPanel panel)
    {
        var hashes=new Dictionary<string,string>(StringComparer.Ordinal);var token="";var path="";var look="";var variant="";
        BoardModelEvidence Fail(string status)=>cache[panel.BoardPath]=new(status,token,path,look,variant,[],null,null,1,hashes,["BOARD_MODEL_"+status]);
        try
        {
            var board=SiiDocumentCache.Open(panel.BoardPath,fs);var roots=board.Units.Where(u=>u.Class=="sign_board_template").ToArray();
            if(roots.Length!=1)return Fail("UNSUPPORTED_SCHEMA");
            foreach(var p in Paths(panel.BoardPath,board))hashes[p]=Hash(p);
            token=Text(roots[0],"model_name");if(token.Length==0||token=="0")return Fail("NOT_FOUND");
            Load();
            if(!definitions.TryGetValue("board_model."+token,out var defs))return Fail("NOT_FOUND");
            if(defs.Count!=1)return Fail("MULTIPLE_MATCHES");
            var def=defs[0];foreach(var p in def.Paths)hashes[p]=Hash(p);
            path=def.Model;look=def.Look;variant=def.Variant;
            if(!path.StartsWith('/')||path.Split('/').Any(s=>s is "." or "..")||!path.EndsWith(".pmd",StringComparison.Ordinal))return Fail("UNSUPPORTED_SCHEMA");
            hashes[path]=Hash(path);hashes[Path.ChangeExtension(path,"pmg")]=Hash(Path.ChangeExtension(path,"pmg"));
            var model=ModelDocumentCache.Open(path,fs);var parts=ActiveParts(model,variant);
            var looks=model.Looks.Where(l=>l.Name.ToString()==look).ToArray();if(looks.Length!=1)return Fail("LOOK_UNRESOLVED");
            var surfaces=new List<BoardSurfaceFrame>();
            foreach(var part in parts)
            foreach(var piece in part.Pieces)
            {
                if(piece.Material>=looks[0].Materials.Count)return Fail("UNSUPPORTED_SCHEMA");
                var material=looks[0].Materials[(int)piece.Material];hashes[material]=Hash(material);
                var text=fs.ReadAllText(material);
                // Only the reviewed reflective board class, never lighting/support geometry.
                if(!System.Text.RegularExpressions.Regex.IsMatch(text,@"\beffect\s*:\s*""eut2\.retroreflective(?:\.[^""]+)?"""))continue;
                var frame=Surface(part.Name.ToString(),part.Pieces.IndexOf(piece),piece.Vertices,material,panel.WidthM,panel.HeightM);
                if(frame is not null)surfaces.Add(frame);
            }
            if(surfaces.Count!=1)return Fail(surfaces.Count==0?"SURFACE_NOT_FOUND":"SURFACE_AMBIGUOUS");
            var locators=parts.SelectMany(p=>p.Locators).Where(l=>l.Name.ToString()=="board1").ToArray();
            var locator=locators.Length==1?locators[0]:null;
            return cache[panel.BoardPath]=new("SURFACE_CANDIDATE",token,path,look,variant,surfaces.ToArray(),
                locator is null?null:V(locator.Position),locator is null?null:[locator.Rotation.X,locator.Rotation.Y,locator.Rotation.Z,locator.Rotation.W],
                locator?.Scale??1,hashes,["BOARD_DRAWING_FRAME_UNCALIBRATED","BOARD_ATTACHMENT_UNCALIBRATED"]);
        }
        catch(Exception e) when(e is IOException or ArgumentException or InvalidDataException or UnsupportedVersionException)
        {return Fail(e is InvalidDataException&&e.Message.StartsWith("VARIANT_")?e.Message:"READ_ERROR:"+e.GetType().Name);}
    }
    internal static BoardSurfaceFrame? Surface(string part,int index,IReadOnlyList<Vertex> vertices,string material,double? width,double? height)
    {
        if(vertices.Count<4||width is not >0||height is not >0||vertices.Any(v=>!RoadLaneGeometryResolver.Finite(v.Position)||!RoadLaneGeometryResolver.Finite(v.Normal)))return null;
        var min=vertices.Select(v=>v.Position).Aggregate(Vector3.Min);var max=vertices.Select(v=>v.Position).Aggregate(Vector3.Max);
        // Deliberately bounded to locally XY planar boards; other models stay unsupported.
        if(Math.Abs((max.X-min.X)-width.Value)>.025||Math.Abs((max.Y-min.Y)-height.Value)>.025||max.Z-min.Z>.005)return null;
        var normal=vertices[0].Normal;
        if(Math.Abs(normal.Length()-1)>.01||Math.Abs(normal.Z)<.99||vertices.Any(v=>Math.Abs(v.Normal.Length()-1)>.01||Vector3.Dot(normal,v.Normal)<.99))return null;
        return new(part,index,V(min),V(max),V(Vector3.Normalize(normal)),material);
    }
}

