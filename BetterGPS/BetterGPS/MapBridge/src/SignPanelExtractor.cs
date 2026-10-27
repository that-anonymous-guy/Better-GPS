using System.Collections;
using System.Globalization;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.RegularExpressions;
using TruckLib;
using TruckLib.Models;
using TruckLib.Sii;

namespace BetterGPSNavigation;

internal sealed record EffectiveSignObject(uint Id, string Kind, bool Visible, bool Reliable,
    IReadOnlyDictionary<string,string> Defaults, IReadOnlyDictionary<string,string> Effective,
    IReadOnlyList<ParsedSignAttribute> Overrides, IReadOnlyList<string> Issues,
    string Text, string Road, string Arrow, string AtlasEntry, string AtlasHash)
{
    public string SchemaStatus { get; init; } = "SUPPORTED";
    public string ArrowStatus => Arrow.Length==0?"NONE":"CANDIDATE_TEXTURE_NOT_VERIFIED";
}
internal sealed record PanelAnchor(string Status, float[]? LocalPosition, float[]? LocalRotation,
    float Scale, float[]? ProvisionalWorldPosition);
internal sealed record ExtractedSignPanel(string SignUid, string AreaName, string BoardToken, string BoardPath,
    string Status, string GroupStatus, PanelAnchor Anchor, IReadOnlyList<EffectiveSignObject> Objects,
    IReadOnlyList<string> Issues)
{
    public bool Reliable => Status == "OK" && Objects.All(o => o.Reliable);
    public bool FollowRoadDir { get; init; }
    public string SupportModel { get; init; } = "";
    public string SupportVariant { get; init; } = "";
    public double[] BoardOffset { get; init; } = [0,0];
    public bool SchemaValidated => Objects.All(o=>o.SchemaStatus=="SUPPORTED");
    public double? WidthM { get; init; }
    public double? HeightM { get; init; }
    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<ExtractedSignPanel,GroupCache> Groups=new();
    private sealed record GroupCache(IReadOnlyList<SignSemanticGroup> Value);
    public IReadOnlyList<SignSemanticGroup> SemanticGroups => Groups.GetValue(this,p=>new(SignGroupResolver.Resolve(p))).Value;
}

/// <summary>Reconstructs board defaults and typed overrides; route ownership is a separate proof.</summary>
internal sealed class SignPanelExtractor(IFileSystem fs, SignTemplateResolver templates, SignBoardResolver boards,Dictionary<ulong,ParsedSignArea[]>? sharedAreas=null)
{
    private sealed record Unit(string Name,string Class,Dictionary<string,string> Attributes,string[] Items);
    private readonly Dictionary<string,List<Unit>> units = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string,Model?> models = new(StringComparer.OrdinalIgnoreCase);
    private readonly BoardModelResolver supportModels = new(fs);
    private readonly Dictionary<string,(string[] Names,string Hash)> atlases = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string,ExtractedSignPanel> panels = new();
    private readonly Dictionary<string,ParsedSignArea> reconstructed = new();
    private readonly Dictionary<string,PanelAnchor> anchors = new();
    private readonly Dictionary<ulong,ParsedSignArea[]> areaSets = sharedAreas??new();
    private readonly HashSet<string> used = [];
    private static readonly string[] ImageFields = ["atlas_path","image_idx","color","opacity","hflip","vflip","image_overridable"];
    // The order of SII properties is not the order of the binary overrides.
    // Indices 3..6 remain explicitly inferred until checked against an editor save/render.
    private static readonly string[] TextFields = ["text","font_face","font_size","font_color","font_opacity","text_halign","text_valign","line_spacing","letter_spacing"];
    private static readonly string[] FrameFields = ["frame_path","color","opacity"];

    public void BeginRoute() => used.Clear();
    public IReadOnlyList<ExtractedSignPanel> Panels => used.Order().Select(k=>panels[k]).ToArray();
    public void WriteRoute(string path)
    {
        var extracted=Panels;
        File.WriteAllText(path,JsonSerializer.Serialize(extracted,new JsonSerializerOptions { WriteIndented=true }));
        File.WriteAllText(Path.ChangeExtension(path,"reading-audit.json"),JsonSerializer.Serialize(new{
            panels=extracted.Count,
            extractionStatuses=extracted.GroupBy(p=>p.Status).ToDictionary(g=>g.Key,g=>g.Count()),
            unreadable=extracted.Where(p=>!p.Reliable||!p.SchemaValidated).Select(p=>new{p.SignUid,p.AreaName,p.BoardPath,p.SupportModel,p.Status,p.Issues,
                objects=p.Objects.Where(o=>!o.Reliable||o.SchemaStatus!="SUPPORTED").Select(o=>new{o.Id,o.Kind,o.SchemaStatus,o.Issues})}),
            informationFamilies=extracted.Where(StreetNameLinker.IsInformationPlate).GroupBy(StreetNameLinker.InformationFamily).ToDictionary(g=>g.Key,g=>g.Count()),
            renderVerified=false},LiveProtocol.Json));
    }

    public IReadOnlyList<ParsedSignArea> Areas(ParsedSign sign)
    {
        using var timing=PerformanceTrace.Detail("sign-panels");
        if(!areaSets.TryGetValue(sign.Uid,out var areas))
        {
            // Keep readings in memory for this map. One disk entry per sign
            // costs more than rebuilding it from the shared template indexes.
            areaSets[sign.Uid]=areas=WorldTileCache.For(fs)?.TryGet(sign.Position,sign.Uid,out var cached)==true?cached.Areas:AreasFresh(sign);
        }
        foreach(var area in areas)
        {
            var key=$"{sign.Uid:X16}:{area.AreaName}";used.Add(key);
            if(area.Extraction is {} panel)panels[key]=panel;
            reconstructed[key]=area;
        }
        return areas;
    }

    private ParsedSignArea[] AreasFresh(ParsedSign sign)
    {
        var source = sign.Areas.ToDictionary(a=>a.AreaName,StringComparer.OrdinalIgnoreCase);
        if (templates.TryGet(sign.Template,out var template) && template is { Status:"OK" })
            foreach (var binding in template.Areas.Where(a=>a.BoardToken.Length>0))
                source.TryAdd(binding.AreaName,new(binding.AreaName,[],[],"",0,0,[],[],[],[],[],[]));
        return source.Values.Select(area=>Reconstruct(sign,area)).ToArray();
    }

    public ParsedSignArea Reconstruct(ParsedSign sign,ParsedSignArea area)
    {
        var key=$"{sign.Uid:X16}:{area.AreaName}";
        used.Add(key);
        if(reconstructed.TryGetValue(key,out var known))return known;
        if (!panels.TryGetValue(key,out var panel)) panels[key]=panel=Extract(sign,area);
        if (panel.Status!="OK") return reconstructed[key]=area with { Extraction=panel };
        var visible=panel.Objects.Where(o=>o.Visible && o.Reliable).ToArray();
        var text=visible.Where(o=>o.Kind=="sign_template_text").Select(o=>SignRoadIdentityResolver.NormalizeText(o.Text)).Where(t=>t.Length>0).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        var roads=visible.Select(o=>o.Road).Where(r=>r.Length>0).Concat(text.Where(SignTextClassifier.IsRouteText)
            .SelectMany(SignTextClassifier.RoadReferences)).Distinct().ToArray();
        var cardinals=text.SelectMany(SignTextClassifier.ExplicitCardinals).Distinct().ToArray();
        var destinations=text.Where(t=>!IsShieldFurniture(panel.BoardPath,t)).Where(SignTextClassifier.IsDestinationCandidate).ToArray();
        var exits=text.Select(t=>Regex.Match(t,@"^EXIT\s+(\d{1,4}[A-Z]?)$",RegexOptions.IgnoreCase)).Where(m=>m.Success).Select(m=>m.Groups[1].Value).ToList();
        var board=boards.Find(panel.BoardToken);
        if (board is not null && board.SemanticKind.Contains("EXIT") && !board.SemanticKind.Contains("SHIELD"))
            exits.AddRange(text.Where(SignTextClassifier.IsExitNumber));
        // Reconstructed string attributes are evidence from visible source objects, including defaults.
        var evidence=visible.Where(o=>o.Kind=="sign_template_text").Select(o=>new ParsedSignAttribute(o.Id,0,"System.String",o.Text)).ToArray();
        return reconstructed[key]=area with { BoardToken=panel.BoardToken, RawStrings=text.Concat(visible.Select(o=>o.Effective.GetValueOrDefault("atlas_path","")).Where(p=>p.Length>0)).ToArray(),
            Attributes=evidence, RoadShields=roads, Cardinals=cardinals, Destinations=destinations,
            ExitNumbers=exits.Distinct().ToArray(), ArrowTokens=visible.Select(o=>o.Arrow).Where(a=>a.Length>0).Distinct().ToArray(), Extraction=panel };
    }

    private ExtractedSignPanel Extract(ParsedSign sign,ParsedSignArea area)
    {
        var issues=new List<string>();
        var token=boards.EffectiveBoardToken(sign,area); var info=boards.Find(token);
        if (info is not { Status:"OK" }) return new($"{sign.Uid:X16}",area.AreaName,token,info?.BoardPath??"",info?.Status??"NO_BOARD", "UNKNOWN",new("NOT_RESOLVED",null,null,1,null),[],[]);
        List<Unit> definition;
        try { definition=ReadUnits(info.BoardPath); } catch(Exception ex) { return new($"{sign.Uid:X16}",area.AreaName,token,info.BoardPath,"PARSE_ERROR","UNKNOWN",new("NOT_RESOLVED",null,null,1,null),[],[ex.Message]); }
        var roots=definition.Where(u=>u.Class=="sign_board_template").ToArray();
        var root=roots.Length==1?roots[0]:null;
        if(root is null) issues.Add("BOARD_ROOT_MISSING");
        var objects=new List<EffectiveSignObject>();
        foreach(var name in root?.Items ?? [])
        {
            var unit=definition.FirstOrDefault(u=>u.Name==name);
            if(unit is null){issues.Add("ITEM_MISSING:"+name);continue;}
            if(!uint.TryParse(unit.Attributes.GetValueOrDefault("id"),out var id)){issues.Add("OBJECT_ID_MISSING:"+name);continue;}
            var effective=new Dictionary<string,string>(unit.Attributes); var objectIssues=new List<string>();
            var overrides=area.Attributes.Where(a=>a.ObjectId==id).ToArray();
            var fields=unit.Class switch { "sign_template_image"=>ImageFields,"sign_template_text"=>TextFields,"sign_template_frame"=>FrameFields,_=>Array.Empty<string>() };
            var schemaStatus="SUPPORTED";
            foreach(var attr in overrides)
            {
                if(attr.AttributeIndex>=fields.Length){objectIssues.Add($"UNKNOWN_ATTRIBUTE:{attr.AttributeIndex}:{attr.ValueType}:{attr.Value}");continue;}
                var field=fields[attr.AttributeIndex];
                if(!AcceptsType(field,attr.ValueType)){objectIssues.Add("ATTRIBUTE_TYPE_MISMATCH:"+field);continue;}
                if(field is "hflip" or "vflip" or "image_overridable" && attr.ValueType!="System.Boolean" && Number(attr.Value) is not (0 or 1))
                {objectIssues.Add("INVALID_BOOLEAN:"+field);continue;}
                if(unit.Class=="sign_template_text" && attr.AttributeIndex is >=3 and <=6) schemaStatus="INFERRED_ATS161";
                effective[field]=attr.Value;
            }
            var opacity=Number(effective.GetValueOrDefault(unit.Class=="sign_template_text"?"font_opacity":"opacity","1"));
            if(!double.IsFinite(opacity) || opacity<0 || opacity>1) objectIssues.Add("INVALID_OPACITY");
            var visible=opacity>0; var road=""; var arrow=""; var entry=""; var hash="";
            if(unit.Class=="sign_template_image" && visible)
            {
                try
                {
                    var atlasPath=effective.GetValueOrDefault("atlas_path","");
                    var atlas=ReadAtlas(atlasPath); hash=atlas.Hash;
                    if(!int.TryParse(effective.GetValueOrDefault("image_idx","0"),out var index) || index<0 || index>=atlas.Names.Length) objectIssues.Add("ATLAS_INDEX_INVALID");
                    else
                    {
                        entry=atlas.Names[index]; road=DecodeRoad(entry);
                        arrow=DecodeArrow(atlasPath,hash,index,Boolean(effective.GetValueOrDefault("hflip","false")),Boolean(effective.GetValueOrDefault("vflip","false")),entry);
                        if(road.Length==0 && arrow.Length==0) objectIssues.Add("UNKNOWN_IMAGE_SEMANTICS:"+entry);
                    }
                } catch(Exception ex){objectIssues.Add("ATLAS_ERROR:"+ex.Message);}
            }
            if(fields.Length==0) objectIssues.Add("UNKNOWN_OBJECT_CLASS:"+unit.Class);
            objects.Add(new(id,unit.Class,visible,objectIssues.Count==0,unit.Attributes,effective,overrides,objectIssues,effective.GetValueOrDefault("text",""),road,arrow,entry,hash){SchemaStatus=schemaStatus});
        }
        // Audited text-based interstate shield. Its labels are shield furniture,
        // not destinations; do not apply this pattern to arbitrary numeric text.
        if(info.BoardPath=="/def/sign/board/shield/ca_fw_single_b.sii")
        {
            var label=objects.SingleOrDefault(o=>o.Id==252837888 && o.Visible && o.Reliable);
            var number=objects.SingleOrDefault(o=>o.Id==3354918910 && o.Visible && o.Reliable);
            if(label is not null && SignRoadIdentityResolver.NormalizeText(label.Text)=="INTERSTATE" &&
                number is not null && Regex.IsMatch(number.Text.Trim(),@"^\d{1,4}[A-Z]?$") )
                objects[objects.IndexOf(number)]=number with {Road="I-"+number.Text.Trim().ToUpperInvariant()};
        }
        DecodeTextShield(info.BoardPath,objects);
        foreach(var id in area.Attributes.Select(a=>a.ObjectId).Distinct().Where(id=>!objects.Any(o=>o.Id==id))) issues.Add("OVERRIDE_OBJECT_MISSING:"+id);
        var active=objects.Where(o=>o.Visible).ToArray();
        var arrowObjects=active.Where(o=>o.Arrow.Length>0).ToArray();
        var groups=(arrowObjects.Length>1 && !arrowObjects.All(o=>o.Arrow=="DOWN_LANE")) || active.Select(o=>o.Road).Where(r=>r.Length>0).Distinct().Count()>1 ||
            active.Where(o=>o.Kind=="sign_template_text").Select(o=>SignRoadIdentityResolver.NormalizeText(o.Text)).Where(t=>!IsShieldFurniture(info.BoardPath,t)).Where(SignTextClassifier.IsDestinationCandidate).Distinct().Count()>1 ||
            active.Where(o=>o.Kind=="sign_template_text").SelectMany(o=>SignTextClassifier.ExplicitCardinals(o.Text)).Distinct().Count()>1
            ? "MULTI_GROUP_UNRESOLVED":"SINGLE_GROUP_CANDIDATE";
        return new($"{sign.Uid:X16}",area.AreaName,token,info.BoardPath,issues.Count==0?"OK":"INCOMPLETE",groups,Anchor(sign,area),objects,issues)
            { FollowRoadDir=sign.FollowRoadDir, SupportModel=sign.ModelDesc, SupportVariant=sign.Variant, BoardOffset=[area.BoardOffsetX,area.BoardOffsetY],
                WidthM=Dimension(root?.Attributes.GetValueOrDefault("width")),
                HeightM=Dimension(root?.Attributes.GetValueOrDefault("height")) };
    }

    internal static void DecodeTextShield(string path,List<EffectiveSignObject> objects)
    {
        uint id=path switch{
            "/def/sign/board/shield/ca_hw_shield_1x.sii"=>2605187071,
            "/def/sign/board/shield/ca_hw_shield_1x_wide.sii"=>3408396286,
            "/def/sign/board/shield/rte_ca_1cahw_fwyent_arrow.sii"=>3205365758,
            _=>0};
        if(id==0)return;
        var numbers=objects.Where(o=>o.Visible&&o.Reliable&&o.Kind=="sign_template_text"&&
            Regex.IsMatch(o.Text.Trim(),@"^\d{1,4}[A-Z]?$",RegexOptions.IgnoreCase)).ToArray();
        if(numbers.Length!=1||numbers[0].Id!=id)return;
        var number=numbers[0];objects[objects.IndexOf(number)]=number with{Road="CA-"+number.Text.Trim().ToUpperInvariant()};
    }

    private static double? Dimension(string? value)
    { var n=Number(value);return double.IsFinite(n)&&n>0?n:null; }

    private PanelAnchor Anchor(ParsedSign sign,ParsedSignArea area)
    {
        var key=AnchorKey(sign,area.AreaName);
        if(!anchors.TryGetValue(key,out var local))anchors[key]=local=AssetIndexCache.GetShared(fs,"support-anchor-"+key,()=>AnchorFresh(sign,area) with{ProvisionalWorldPosition=null});
        if(local.LocalPosition is not {} position)return local;
        var world=Vector3.Transform(BoardModelResolver.V(position),sign.Rotation)+sign.Position;
        return local with{ProvisionalWorldPosition=[world.X,world.Y,world.Z]};
    }
    internal static string AnchorKey(ParsedSign sign,string area)=>Convert.ToHexString(SHA256.HashData(System.Text.Json.JsonSerializer.SerializeToUtf8Bytes(
        new{sign.Model,sign.ModelDesc,sign.Look,sign.Variant,area},LiveProtocol.Json)));
    private PanelAnchor AnchorFresh(ParsedSign sign,ParsedSignArea area)
    {
        try
        {
            if(!models.TryGetValue(sign.ModelDesc,out var model)) models[sign.ModelDesc]=model=ModelDocumentCache.Open(sign.ModelDesc,fs);
            var support=supportModels.ResolveSupport(sign);
            var variant=support.Variant.Length>0?support.Variant:model!.Variants.Count==1?model.Variants[0].Name.ToString():"";
            var locators=BoardModelResolver.ActiveParts(model!,variant).SelectMany(p=>p.Locators)
                .Where(l=>l.Name.ToString().Equals(area.AreaName,StringComparison.OrdinalIgnoreCase)).ToArray();
            if(locators.Length!=1) return new("LOCATOR_NOT_UNIQUE",null,null,1,null);
            var l=locators[0]; var p=Vector3.Transform(l.Position,sign.Rotation)+sign.Position;
            // The active variant supplies one locator; offsets and FollowRoadDir still
            // need their own transform. This model observation does not establish HIGH ownership.
            return new("ACTIVE_MODEL_LOCATOR_UNCALIBRATED",[l.Position.X,l.Position.Y,l.Position.Z],[l.Rotation.X,l.Rotation.Y,l.Rotation.Z,l.Rotation.W],l.Scale,[p.X,p.Y,p.Z]);
        }catch(Exception ex){return new("MODEL_ERROR:"+ex.GetType().Name,null,null,1,null);}
    }

    private List<Unit> ReadUnits(string path)
    {
        if(units.TryGetValue(path,out var result))return result;
        var key=Convert.ToHexString(SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(path)));
        return units[path]=AssetIndexCache.GetShared(fs,"panel-units-"+key,()=>{
            var file=SiiDocumentCache.Open(path,fs);
            return file.Units.Select(u=>new Unit(u.Name,u.Class,u.Attributes.ToDictionary(k=>k.Key,k=>SignTemplateResolver.CleanToken((object?)k.Value)),
                u.Attributes.TryGetValue("items",out var raw) && (object?)raw is IEnumerable seq && (object?)raw is not string
                    ? seq.Cast<object>().Select(SignTemplateResolver.CleanToken).ToArray():[])).ToList();
        });
    }
    private (string[] Names,string Hash) ReadAtlas(string path)
    {
        if(atlases.TryGetValue(path,out var result))return result;
        var key=Convert.ToHexString(SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(path)));
        var atlas=AssetIndexCache.GetShared(fs,"panel-atlas-"+key,()=>{
            using var stream=fs.Open(path);var hash=Convert.ToHexString(SHA256.HashData(stream));
            var file=SiiDocumentCache.Open(path,fs);var unit=file.Units.Single(u=>u.Class=="image_atlas");
            if(!unit.Attributes.TryGetValue("images_names",out var raw)||(object?)raw is not IEnumerable sequence||(object?)raw is string)throw new InvalidDataException("ATLAS_NAMES_MISSING");
            return new AtlasContent(sequence.Cast<object>().Select(SignTemplateResolver.CleanToken).ToArray(),hash);
        });
        result=(atlas.Names,atlas.Hash);atlases[path]=result;return result;
    }
    private sealed record AtlasContent(string[] Names,string Hash);

    internal static string DecodeRoad(string entry)
    {
        entry=Regex.Replace(entry.Trim().ToUpperInvariant(),@"[_-]+"," ");
        entry=Regex.Replace(entry,@"\s+(?:GUIDE|SHIELD|WHITE|BLACK|WIDE|LOGO)(?:\s+[1-9])?$","");
        entry=Regex.Replace(entry,@"\s+(?:GUIDE|SHIELD|WHITE|BLACK|WIDE|LOGO)(?:\s+[1-9])?$","");
        var local=Regex.Match(entry,@"^("+SignTextClassifier.Prefix+@") COUNTY ([A-Z]?\d{1,4})$");
        if(local.Success)return local.Groups[1].Value+"-CR-"+local.Groups[2].Value;
        var rural=Regex.Match(entry,@"^TX (FARM|RANCH|PARK) ROAD (\d{1,4})$");
        if(rural.Success)return "TX-"+(rural.Groups[1].Value switch{"FARM"=>"FM","RANCH"=>"RM",_=>"PR"})+"-"+rural.Groups[2].Value;
        if(entry=="KS TURNPIKE")return "KS-TURNPIKE";
        var letter=Regex.Match(entry,@"^MO ([A-Z]{1,2})$");if(letter.Success)return "MO-"+letter.Groups[1].Value;
        var business=Regex.Match(entry,@"^BUSINESS (?:(US) )?(\d{1,4}[A-Z]?)$");
        if(business.Success)return (business.Groups[1].Success?"US":"I")+"-"+business.Groups[2].Value+"-BUS";
        var stateBusiness=Regex.Match(entry,@"^("+SignTextClassifier.Prefix+@") (\d{1,4}[A-Z]?) BUSINESS$");
        if(stateBusiness.Success)return stateBusiness.Groups[1].Value+"-"+stateBusiness.Groups[2].Value+"-BUS";
        entry=Regex.Replace(entry,@"^(?:"+SignTextClassifier.Prefix+@") INTERSTATE ","INTERSTATE ");
        var match=Regex.Match(entry,@"^(INTERSTATE|IS|"+SignTextClassifier.Prefix+@")[ ]+(\d{1,4}[A-Z]?)(?:[ ]+(?:"+SignTextClassifier.Prefix+@"|[1-9]))*$",RegexOptions.IgnoreCase);
        if(!match.Success)return "";
        var prefix=match.Groups[1].Value.ToUpperInvariant();if(prefix is "INTERSTATE" or "IS")prefix="I";
        return prefix+"-"+match.Groups[2].Value.ToUpperInvariant();
    }
    internal static string DecodeArrow(string path,string hash,int index,bool hflip,bool vflip,string entry="")
    {
        // Reviewed atlas definitions. Candidate only: texture overrides have not been fingerprinted here.
        if(index==0)
        {
        if(path=="/def/sign/atlas/small_arrow_12.sii"&&hash=="9145A494FD7A1DBCCB5B552F157496B9E6CEA132B81F533C308947130F4FED3D"||
            path=="/def/sign/atlas/small_arrow_12_ok.sii"&&hash=="355E91C3FD381C647445CECA69E6B690CE2D04D326649AB87A71610F9295C794")return hflip?"RIGHT":"LEFT";
        if(path=="/def/sign/atlas/small_arrow_01.sii"&&hash=="B7A06D2C4B6938E49235591BC6611AF0A1951D76596749B3554FB75605546316")return vflip?"DOWN":"UP";
        if(path=="/def/sign/atlas/small_arrow_03.sii"&&hash=="39E4E2C1F1A55D761106E3AD90A873FC5B502B92FA4ACC4E001B651C0FDCAD3B")return (vflip?"DIAGONAL_DOWN_":"DIAGONAL_UP_")+(hflip?"LEFT":"RIGHT");
        if(path=="/def/sign/atlas/down_arrow.sii" && hash=="956CB587FFDE6FB0A23CE578736339F19E947B47379DC06EB177580C5CED0686") return vflip?"UP_LANE":"DOWN_LANE";
        if(path=="/def/sign/atlas/type_b_45.sii" && hash=="2819D4362A0BD871B8BE747D1624A0BB1F34046572A1A9F42597A5EF5E9381F5") return (vflip?"DIAGONAL_DOWN_":"DIAGONAL_UP_")+(hflip?"LEFT":"RIGHT");
        if(path=="/def/sign/atlas/type_b_90.sii" && hash=="42FFD62D45D53D161963AEBAB973460637B38B340256B950E0217F733F938332") return hflip?"LEFT":"RIGHT";
        if(path=="/def/sign/atlas/type_b.sii" && hash=="881BB391AEE0C7A1B045E3A3224BE668B0BA4694E2ABECF6BD847FC14A24D49D")return vflip?"DOWN":"UP";
        }
        // Read the effective atlas entry, including a mod's path/index and flips.
        // Only declared directional symbols and inspected standard families qualify.
        var label=SignTextClassifier.Normalize(entry).ToUpperInvariant();
        var horizontal=label switch{"RIGHT" or "ARROW RIGHT" or "TYPE A 90" or "TYPE B 90" or "TYPE C" or "TYPE C2"=>"RIGHT",
            "LEFT" or "ARROW LEFT"=>"LEFT",_=>""};
        if(horizontal.Length>0)return hflip?(horizontal=="RIGHT"?"LEFT":"RIGHT"):horizontal;
        if(label is "UP" or "ARROW UP" or "STRAIGHT" or "TYPE B")return vflip?"DOWN":"UP";
        if(label is "DOWN" or "ARROW DOWN")return vflip?"UP":"DOWN";
        return "";
    }
    private static bool IsShieldFurniture(string path,string text)=>path=="/def/sign/board/shield/ca_fw_single_b.sii" && text is "INTERSTATE" or "CALIFORNIA";
    private static bool AcceptsType(string field,string type)=>field switch
    {
        "atlas_path" or "text" or "font_face" or "frame_path"=>type=="System.String",
        "image_idx" or "color" or "font_color" or "text_halign" or "text_valign"=>type is "System.Int32" or "System.UInt32",
        "hflip" or "vflip" or "image_overridable"=>type is "System.Boolean" or "System.Byte" or "System.SByte" or "System.Int32" or "System.UInt32" or "System.Single",
        _=>type is "System.Single" or "System.Double"
    };
    private static bool Boolean(string value)=>value.Equals("true",StringComparison.OrdinalIgnoreCase) || double.TryParse(value,NumberStyles.Float,CultureInfo.InvariantCulture,out var v)&&v!=0;
    private static double Number(string? value)=>double.TryParse(value,NumberStyles.Float,CultureInfo.InvariantCulture,out var n)?n:double.NaN;
}
