using System.Numerics;
using System.Text.Json;
using System.Text.RegularExpressions;
using TruckLib.Models;
using TruckLib.Sii;

var stock=Path.GetFullPath(args[0]);
var output=Path.GetFullPath(args[1]);
var package=Path.Combine(output,"mod");
int GlassMaterial(string path)
{
    var refs=Regex.Matches(System.Text.Encoding.ASCII.GetString(File.ReadAllBytes(path+".pmd")),@"/automat/[^\x00]+\.mat").Select(m=>m.Value).ToArray();
    return Array.FindIndex(refs,p=>File.ReadAllText(Path.Combine(package,p.TrimStart('/'))).Contains("\"eut2.glass.rfx\""));
}
int PupilFronts(byte[] data, Vector3 expected, int glassMaterial)
{
    int I(int offset)=>BitConverter.ToInt32(data,offset);
    float F(int offset)=>BitConverter.ToSingle(data,offset);
    var found=0;
    var glassFront=float.NegativeInfinity;var pupilFront=float.NegativeInfinity;
    for(var p=0;p<I(4);p++)
    {
        var piece=I(84)+100*p;var stride=I(piece+60);var uv=I(piece+72);var normal=I(piece+68);
        for(var i=0;i<I(piece+4);i++)
        {
            var position=I(piece+64)+i*stride;
            var p3=new Vector3(F(position),F(position+4),F(position+8));
            if(I(piece+16)==glassMaterial)glassFront=Math.Max(glassFront,Vector3.Dot(p3,expected));
            if(Math.Abs(F(uv+i*stride)-.5625f)>.0001f)continue;
            pupilFront=Math.Max(pupilFront,Vector3.Dot(p3,expected));
            var n=new Vector3(F(normal+i*stride),F(normal+i*stride+4),F(normal+i*stride+8));
            if(Vector3.Dot(n,expected)>.9999f)found++;
        }
    }
    if(!float.IsFinite(glassFront)||glassFront-pupilFront<.00001f)
        throw new InvalidDataException("Optical pupil must sit behind a separate protective glass piece");
    return found;
}
{
    var entries=Directory.GetFiles(package,"*",SearchOption.AllDirectories).Select(p=>Path.GetRelativePath(package,p).Replace('\\','/')).ToArray();
    if(entries.Any(e=>e.StartsWith("unit/hookup/")||e.StartsWith("def/vehicle/truck/kenworth.t680_2022/cabin/")))
        throw new InvalidDataException("Camera must not ship permanent hookups or cabin defaults");
    var definition=File.ReadAllText(Path.Combine(package,"def/vehicle/truck/kenworth.t680_2022/accessory/bgps_cam/pp804395.sii"));
    if(!definition.Contains("price: 150")||!definition.Contains("icon: \"bettergps/pp804395\""))
        throw new InvalidDataException("Rear camera definition changed unexpectedly");
    foreach(var side in new[]{(396,"bgps_right"),(397,"bgps_left")})
    {
        var text=File.ReadAllText(Path.Combine(package,$"def/vehicle/truck/kenworth.t680_2022/accessory/{side.Item2}/pp804{side.Item1}.sii"));
        if(!text.Contains($"pp804{side.Item1}.kenworth.t680_2022.{side.Item2}")||!text.Contains("price: 250"))
            throw new InvalidDataException("Side camera must be independently purchasable for $250");
    }
    foreach(var file in entries.Where(e=>e.EndsWith(".sii")))
    {
        var text=File.ReadAllText(Path.Combine(package,file));
        if(text.Contains("defaults[")||text.Contains("overrides[")||text.Contains("require["))
            throw new InvalidDataException("Cameras must be independent optional accessories");
    }
    foreach(var name in new[]{"pp804395","pp804396","pp807032"})
    {
        var icon=File.ReadAllBytes(Path.Combine(package,$"material/ui/accessory/bettergps/{name}.dds"));
        if(BitConverter.ToInt32(icon,12)!=64||BitConverter.ToInt32(icon,16)!=256)
            throw new InvalidDataException("Icon must use stock 256x64 layout");
    }
}
/* Inspect the unpacked package source; the user may keep it in a ZIP. */
using var sidePoses=JsonDocument.Parse(File.ReadAllText(Path.Combine(output,"../../pp804396/placement/mounts.json")));
using var sideModelInfo=JsonDocument.Parse(File.ReadAllText(Path.Combine(output,"../../pp804396/pp804396_dimensions.json")));
var ballAngle=sideModelInfo.RootElement.GetProperty("user_adjustment").GetProperty("ball_up_degrees").GetDouble()*Math.PI/180;
using var poses=JsonDocument.Parse(File.ReadAllText(Path.Combine(output,"placement","mounts.json")));
var checkedModels=new List<object>();
var observedCameraTypes=new HashSet<string>();
foreach(var pose in poses.RootElement.EnumerateArray())
{
    var cabin=pose.GetProperty("cabin").GetString()!;
    var original=Model.Open(Path.Combine(stock,"cabin",cabin+".pmd"));
    var pmd=File.ReadAllBytes(Path.Combine(stock,"cabin",cabin+".pmd"));
    var pmg=File.ReadAllBytes(Path.Combine(package,"vehicle","truck","kenworth_t680_2022","cabin",cabin+".pmg"));
    var sourcePmg=File.ReadAllBytes(Path.Combine(stock,"cabin",cabin+".pmg"));
    int U(int offset)=>BitConverter.ToInt32(pmg,offset);
    if(!(U(76)<U(80)&&U(80)+44*U(20)==U(84)&&U(84)+100*U(4)<=U(88)&&
        U(88)+U(92)<=U(96)&&U(96)+U(100)<=U(104)&&U(104)+U(108)==pmg.Length))
        throw new InvalidDataException("Noncanonical native PMG table/pool layout");
    foreach(var pair in new[]{(96,100),(104,108)})
    {
        var oldOffset=BitConverter.ToInt32(sourcePmg,pair.Item1);
        var oldSize=BitConverter.ToInt32(sourcePmg,pair.Item2);
        if(U(pair.Item2)!=oldSize||!sourcePmg.AsSpan(oldOffset,oldSize).SequenceEqual(pmg.AsSpan(U(pair.Item1),oldSize)))
            throw new InvalidDataException("Raw vertex/index pool changed");
    }
    var modified=Model.Load(pmd,pmg);
    foreach(var name in modified.Parts.SelectMany(p=>p.Locators).Select(l=>l.Name.ToString()).Where(n=>n.StartsWith("bgps_")))observedCameraTypes.Add(name);
    if(modified.Parts.Sum(p=>p.Locators.Count)!=original.Parts.Sum(p=>p.Locators.Count)+3)
        throw new InvalidDataException("Expected one rear and two independent side camera sockets");
    if(original.Parts.Count!=modified.Parts.Count)throw new InvalidDataException("Parts changed");
    for(int i=0;i<original.Parts.Count;i++)
    {
        var a=original.Parts[i];var b=modified.Parts[i];
        if(a.Name!=b.Name||a.Pieces.Count!=b.Pieces.Count)throw new InvalidDataException("Pieces changed");
        for(int j=0;j<a.Pieces.Count;j++)
        {
            var av=a.Pieces[j].Vertices;var bv=b.Pieces[j].Vertices;
            if(av.Count!=bv.Count)throw new InvalidDataException("Vertex count changed");
            for(int k=0;k<av.Count;k++)
                if(av[k].Position!=bv[k].Position||av[k].Normal!=bv[k].Normal)
                    throw new InvalidDataException("Vertex position/normal changed");
        }
        for(int j=0;j<a.Locators.Count;j++)
        {
            var x=a.Locators[j];var y=b.Locators[j];
            if(x.Name!=y.Name||x.Position!=y.Position||x.Rotation!=y.Rotation||x.HookupOffset!=y.HookupOffset)
                throw new InvalidDataException("Existing locator changed");
        }
    }
    var locator=modified.Parts.SelectMany(p=>p.Locators).Single(l=>l.Name.ToString()=="bgps_cam");
    if(locator.HookupOffset!=0)throw new InvalidDataException("Tuning socket must have an empty hookup");
    var stringOffset=BitConverter.ToInt32(pmg,88);
    if(pmg[stringOffset]!=0)throw new InvalidDataException("Empty hookup must resolve to NUL");
    var xyz=pose.GetProperty("position_scs").EnumerateArray().Select(p=>p.GetSingle()).ToArray();
    if(Vector3.Distance(locator.Position,new(xyz[0],xyz[1],xyz[2]))>1e-6)
        throw new InvalidDataException("Mount position mismatch");
    if(Vector3.Dot(Vector3.Transform(-Vector3.UnitZ,locator.Rotation),Vector3.UnitZ)<0.8f)
        throw new InvalidDataException("Tuning socket must face the rear of the cabin");
    if(pose.GetProperty("mount_contact_gap_min_m").GetDouble()>0.0001||
        pose.GetProperty("mount_contact_gap_max_m").GetDouble()>0.001||
        pose.GetProperty("penetrating_camera_vertices").GetInt32()!=0)
        throw new InvalidDataException("Camera mounting plate must fit the wall within 1 mm without penetration");
    var pivotHeight=locator.Position.Y+Vector3.Transform(new Vector3(0,.045f,0),locator.Rotation).Y;
    if(Math.Abs(pivotHeight-(2.619778537750244+(cabin=="dc"?0:.10)))>0.00001)
        throw new InvalidDataException("Sleeper rear camera must be raised 10 cm; Day Cab stays unchanged");
    foreach(var sidePose in sidePoses.RootElement.EnumerateArray().Where(p=>p.GetProperty("cabin").GetString()==cabin))
    {
        var name=sidePose.GetProperty("locator").GetString();
        var loc=modified.Parts.SelectMany(p=>p.Locators).Single(l=>l.Name.ToString()==name);
        var expected=sidePose.GetProperty("position_scs").EnumerateArray().Select(p=>p.GetSingle()).ToArray();
        var sign=name=="bgps_right"?1:-1;
        if(loc.HookupOffset!=0||Vector3.Distance(loc.Position,new(expected[0],expected[1],expected[2]))>.000002f||
            Vector3.Transform(-Vector3.UnitZ,loc.Rotation).X*sign<.8f)
            throw new InvalidDataException("Side camera socket position or side visibility is wrong");
        if(sidePose.GetProperty("mount_contact_gap_max_m").GetDouble()>.001||
            sidePose.GetProperty("mirror_intersections").GetInt32()!=0||
            sidePose.GetProperty("mirror_symmetry_max_error_m").GetDouble()>.000002)
            throw new InvalidDataException("Side camera fit, mirror clearance or symmetry failed");
    }
    if(pose.GetProperty("rear_equipment_intersecting_triangle_pairs").GetInt32()!=0)
        throw new InvalidDataException("Camera intersects rear equipment");
    checkedModels.Add(new{cabin,parts=modified.Parts.Count,geometryAndExistingLocatorsPreserved=true,
        canonicalNativeLayout=true,byteIdenticalVertexAndIndexPools=true,socketFacesRear=true});
}
foreach(var part in new[]{396,397})
{
    var path=Path.Combine(package,$"vehicle/truck/upgrade/bettergps/pp804{part}/pp804{part}_truck");
    var model=Model.Open(path+".pmd");
    if(model.Looks.Single().Materials.Count!=4||model.Parts.Sum(p=>p.Pieces.Count)!=4)
        throw new InvalidDataException("Side camera must have satin, optics, glass and metal pieces");
    var data=File.ReadAllBytes(path+".pmg");
    var expected=new Vector3((float)Math.Cos(ballAngle)*(part==396?1:-1),0,-(float)Math.Sin(ballAngle));
    var found=PupilFronts(data,expected,GlassMaterial(path));
    if(found<12)throw new InvalidDataException("Side camera pupil does not match the user's edited angle");
    checkedModels.Add(new{part,materials=4,userOpticalAnglePreserved=true});
}
foreach(var lod in new[]{0})
{
    var model=Model.Open(Path.Combine(package,"vehicle","truck","upgrade","bettergps","pp804395",$"pp804395_lod{lod}.pmd"));
    if(model.Looks.Single().Materials.Count!=5||model.Parts.Sum(p=>p.Pieces.Count)!=5)
        throw new InvalidDataException("Expected satin, optics, glass, metal and dark cavity pieces");
    var vertices=model.Parts.SelectMany(p=>p.Pieces).SelectMany(p=>p.Vertices).Select(v=>v.Position).ToArray();
    var minimum=vertices.Aggregate(new Vector3(float.PositiveInfinity),Vector3.Min);
    var maximum=vertices.Aggregate(new Vector3(float.NegativeInfinity),Vector3.Max);
    var size=maximum-minimum;
    if(Math.Abs(size.X-.075f)>0.000002f)
        throw new InvalidDataException("Camera width must remain 75 mm");
    // Read the pupil's palette UV and normals from the converted PMG, so a
    // rotated locator or a tilted chamfer cannot masquerade as head tilt.
    var data=File.ReadAllBytes(Path.Combine(package,"vehicle/truck/upgrade/bettergps/pp804395/pp804395_lod0.pmg"));
    var pupilFronts=PupilFronts(data,Vector3.Normalize(new Vector3(0,-1,-1)),GlassMaterial(Path.Combine(package,"vehicle/truck/upgrade/bettergps/pp804395/pp804395_lod0")));
    if(pupilFronts<8)throw new InvalidDataException("Optical pupil must face 45 degrees downward");
    foreach(var x in new[]{-.060f,.060f})foreach(var y in new[]{.044f,.048f})foreach(var z in new[]{.0485f,.0935f})
    {
        var expected=new Vector3(-x*.625f,z*.625f,y*.625f);
        if(!vertices.Any(v=>Vector3.Distance(v,expected)<.000001f))
            throw new InvalidDataException("Fixed wall plate was moved with the camera head");
    }
    foreach(var side in new[]{-1f,1f})foreach(var dx in new[]{-.003f,.003f})foreach(var y in new[]{0f,.046f})foreach(var z in new[]{.062f,.080f})
    {
        var expected=new Vector3(-(side*.052f+dx)*.625f,z*.625f,y*.625f);
        if(!vertices.Any(v=>Vector3.Distance(v,expected)<.000001f))
            throw new InvalidDataException("Fixed straight tab was moved with the camera head");
    }
    checkedModels.Add(new{lod,parts=model.Parts.Count,materials=model.Looks.Single().Materials.Count,
        pupilDownTiltDegrees=45,fixedPlateAndTabsPreserved=true,
        sizeMillimetres=new[]{size.X*1000,size.Y*1000,size.Z*1000}});
}
using var frontPoses=JsonDocument.Parse(File.ReadAllText(Path.Combine(output,"../../pp807032/placement/mounts.json")));
var storagePath=Path.Combine(package,"def/vehicle/addon_hookup_storage.bettergps.sii");
var storageText=File.ReadAllText(storagePath);
var frontDefinition=File.ReadAllText(Path.Combine(package,"def/vehicle/addon_hookups/bettergps_pp807032.sui"));
if(!storageText.Contains("@include \"addon_hookups/bettergps_pp807032.sui\"")||
   !frontDefinition.Contains("accessory_hookup_data : pp807032.bgps.addon_hookup")||
   !frontDefinition.Contains("price: 250")||!frontDefinition.Contains("suitable_for[]: \"bumper\"")||
   !frontDefinition.Contains("model: \"/vehicle/truck/upgrade/bettergps/pp807032/pp807032_lod0.pmd\"")||
   frontDefinition.Contains("exterior_model"))
    throw new InvalidDataException("Front camera must use the working bumper-type hookup filter");
// Parse the actual multifile storage entry and its include, not just regexes.
var parsedHookups=SiiFile.Open(storagePath);
if(parsedHookups.Units.Count!=2||!frontDefinition.Contains("accessory_hookup_data : pp807032low.bgps.addon_hookup"))
    throw new InvalidDataException("Expected one visible camera and its hidden legacy save alias");
var blocks=Regex.Matches(frontDefinition,@"accessory_hookup_data\s*:\s*([^\s]+)\s*\{([^}]+)\}")
    .ToDictionary(m=>m.Groups[1].Value,m=>Regex.Matches(m.Groups[2].Value,"suitable_for\\[\\]:\\s*\"([^\"]+)\"").Select(v=>v.Groups[1].Value).ToHashSet());
if(blocks.Any(b=>!b.Value.SetEquals(new[]{"bumper"})))
    throw new InvalidDataException("Full unit-ID filters must not replace the working hookup parent-type filter");
var definitions=Regex.Matches(frontDefinition,@"accessory_hookup_data\s*:\s*([^\s]+)\s*\{([^}]+)\}");
foreach(Match definition in definitions)
{
    var body=definition.Groups[2].Value;
    var legacy=definition.Groups[1].Value.StartsWith("pp807032low.");
    if(!Regex.IsMatch(body,@"price:\s*"+(legacy?"0":"250")+@"\b")||body.Contains("/pp807032_low/")||
       !body.Contains("conflict_with[]: \"sign.kenworth.t680_2022.f_banner\"")||
       !body.Contains("conflict_with[]: \"sign_low.kenworth.t680_2022.f_banner\"")||body.Contains("bullbar"))
        throw new InvalidDataException("Front camera price/model/banner conflicts or bullbar compatibility changed");
}
foreach(var banner in new[]{"sign","sign_low"})
{
    var bannerFile=Path.Combine(package,$"def/vehicle/truck/kenworth.t680_2022/accessory/f_banner/{banner}.sii");
    var bannerText=File.ReadAllText(bannerFile);
    if(SiiFile.Open(bannerFile).Units.Count!=1||
       !bannerText.Contains("conflict_with[]: \"small.kenworth.t680_2022.bullbar\"")||
       !bannerText.Contains("conflict_with[]: \"pp807032.bgps.addon_hookup\"")||
       !bannerText.Contains("conflict_with[]: \"pp807032low.bgps.addon_hookup\""))
        throw new InvalidDataException("Banner must retain the stock bullbar conflict and both camera IDs");
}
var wheelReference=Model.Open(Path.Combine(stock,"accessory/steering_w/standard.pmd"));
if(!wheelReference.Parts.SelectMany(p=>p.Locators).Any(l=>l.Name.ToString()=="slot_0"&&l.HookupOffset==0))
    throw new InvalidDataException("Stock steering-wheel reference must have the same empty slot format");
if(frontPoses.RootElement.GetArrayLength()!=8)throw new InvalidDataException("All eight stock bumpers required");
if(Directory.Exists(Path.Combine(package,"vehicle/truck/kenworth_t680_2022/chassis"))&&
   Directory.GetFiles(Path.Combine(package,"vehicle/truck/kenworth_t680_2022/chassis"),"*.pmg").Length!=0)
    throw new InvalidDataException("Bumper child must not ship chassis overrides");
foreach(var obsolete in new[]{"bgps_front","bgps_cam_f"})
    if(Directory.Exists(Path.Combine(package,$"def/vehicle/truck/kenworth.t680_2022/accessory/{obsolete}"))&&
       Directory.GetFiles(Path.Combine(package,$"def/vehicle/truck/kenworth.t680_2022/accessory/{obsolete}"),"*.sii").Length>0)
        throw new InvalidDataException("Obsolete direct front accessory definitions remain");
Vector3 Position(JsonElement pose){var p=pose.GetProperty("position_scs").EnumerateArray().Select(x=>x.GetSingle()).ToArray();return new(p[0],p[1],p[2]);}
var chassisModels=Directory.GetFiles(Path.Combine(stock,"chassis"),"*.pmd");
if(chassisModels.Length!=8)throw new InvalidDataException("All eight stock chassis required");
foreach(var pose in frontPoses.RootElement.EnumerateArray())
{
    var bumper=pose.GetProperty("bumper").GetString()!;
    var source=Path.Combine(stock,"accessory/bumper",bumper);
    var target=Path.Combine(package,"vehicle/truck/kenworth_t680_2022/accessory/bumper",bumper+".pmg");
    var oldData=File.ReadAllBytes(source+".pmg");var data=File.ReadAllBytes(target);
    int U(int offset)=>BitConverter.ToInt32(data,offset);
    if(U(20)!=1||BitConverter.ToInt32(oldData,20)!=0||U(92)!=4||data[U(88)]!=0||data.Length!=oldData.Length+48||U(96)%4!=0||
       U(80)+44*U(20)!=U(84)||U(84)+100*U(4)>U(88)||U(88)+U(92)>U(96)||U(104)+U(108)!=data.Length)
        throw new InvalidDataException("Bumper slot has invalid native metadata layout");
    foreach(var pair in new[]{(96,100),(104,108)})
    {
        var at=BitConverter.ToInt32(oldData,pair.Item1);var count=BitConverter.ToInt32(oldData,pair.Item2);
        if(count!=U(pair.Item2)||!oldData.AsSpan(at,count).SequenceEqual(data.AsSpan(U(pair.Item1),count)))
            throw new InvalidDataException("Bumper raw vertex/index pool changed");
    }
    var original=Model.Open(source+".pmd");var modified=Model.Load(File.ReadAllBytes(source+".pmd"),data);
    if(original.Parts.Count!=modified.Parts.Count||original.Parts.Sum(p=>p.Pieces.Count)!=modified.Parts.Sum(p=>p.Pieces.Count))
        throw new InvalidDataException("Bumper parts/pieces changed");
    var before=original.Parts.SelectMany(p=>p.Pieces).SelectMany(p=>p.Vertices).ToArray();
    var after=modified.Parts.SelectMany(p=>p.Pieces).SelectMany(p=>p.Vertices).ToArray();
    if(before.Length!=after.Length||before.Where((v,i)=>v.Position!=after[i].Position||v.Normal!=after[i].Normal).Any())
        throw new InvalidDataException("Bumper geometry changed");
    var oldLocators=original.Parts.SelectMany(p=>p.Locators).ToArray();
    var locators=modified.Parts.SelectMany(p=>p.Locators).ToArray();
    if(locators.Length!=oldLocators.Length+1||oldLocators.Where((l,i)=>l.Name!=locators[i].Name||l.Position!=locators[i].Position||l.Rotation!=locators[i].Rotation||l.HookupOffset!=locators[i].HookupOffset).Any())
        throw new InvalidDataException("Existing bumper locators changed");
    var locator=locators.Single();
    if(locator.Name.ToString()!="slot_0"||locator.HookupOffset!=0||
       Vector3.Distance(locator.Position,Position(pose))>.000001f||locator.Position.X>=0||
       Vector3.Transform(-Vector3.UnitZ,locator.Rotation).Z>-.99)
        throw new InvalidDataException("Bumper child must use empty slot_0 on the left, facing forward");
    var acceptsLowClipping=pose.TryGetProperty("accepted_grille_flat_clipping",out var clipping)&&clipping.GetBoolean()&&bumper.StartsWith("low_");
    if(pose.GetProperty("body_grille_intersections").GetInt32()!=0||(!acceptsLowClipping&&pose.GetProperty("strap_grille_intersections").GetInt32()!=0)||
       pose.GetProperty("body_to_grille_vertex_gap_m").GetDouble()>.003||pose.GetProperty("strap_depth_past_grille_m").GetDouble()<.035)
        throw new InvalidDataException("Front camera mounting fit changed");
    foreach(var chassisPath in chassisModels)
    {
        var chassis=Model.Open(chassisPath);
        var anchor=chassis.Parts.SelectMany(p=>p.Locators).Single(l=>l.Name.ToString()=="bumper");
        var actual=anchor.Position+Vector3.Transform(locator.Position,anchor.Rotation);
        var expected=anchor.Position+Vector3.Transform(Position(pose),anchor.Rotation);
        if(Vector3.Distance(actual,expected)>.000001f)throw new InvalidDataException("Bumper child does not follow its parent transform");
    }
    checkedModels.Add(new{bumper,geometryAndExistingLocatorsPreserved=true,emptyOptionalSlot=true,parentChassisCombinations=8,canonicalNativeLayout=true});
}
foreach(var name in new[]{"pp807032"})
{
    var path=Path.Combine(package,$"vehicle/truck/upgrade/bettergps/{name}/{name}_lod0");
    var model=Model.Open(path+".pmd");
    if(model.Looks.Single().Materials.Count!=5||model.Parts.Sum(p=>p.Pieces.Count)!=5)
        throw new InvalidDataException("Front camera must include a separate dark optical cavity");
    var data=File.ReadAllBytes(path+".pmg");
    using var prefs=JsonDocument.Parse(File.ReadAllText(Path.Combine(output,"../../pp807032/user-adjustments.json")));
    var angle=prefs.RootElement.GetProperty("head_down_degrees").GetDouble()*Math.PI/180;
    var expected=new Vector3(0,-(float)Math.Sin(angle),-(float)Math.Cos(angle));
    var found=PupilFronts(data,expected,GlassMaterial(path));
    if(found<8)throw new InvalidDataException("Front optical angle differs from the user sketch");
    checkedModels.Add(new{part=name,materials=5,userOpticalAnglePreserved=true});
}
// Exact-name comparisons alone missed the actual engine rule: upgrades bind
// to locator names by PREFIX. Reject a rear type consuming a front/side point.
var declaredTypes=Directory.GetFiles(Path.Combine(package,"def"),"*.sii",SearchOption.AllDirectories)
    .SelectMany(p=>Regex.Matches(File.ReadAllText(p),@"accessory_addon_data\s*:\s*[^\s]+\.(bgps_\w+)").Select(m=>m.Groups[1].Value)).ToHashSet();
foreach(var locator in observedCameraTypes)
{
    var matches=declaredTypes.Where(t=>locator.StartsWith(t,StringComparison.Ordinal)).ToArray();
    if(matches.Length!=1||matches[0]!=locator)throw new InvalidDataException("Ambiguous ATS prefix binding: "+locator+" -> "+string.Join(",",matches));
}
if(!observedCameraTypes.SetEquals(new[]{"bgps_cam","bgps_right","bgps_left"}))
    throw new InvalidDataException("Unexpected camera locator types");
File.WriteAllText(Path.Combine(output,"independent-model-check.json"),JsonSerializer.Serialize(
    new{status="PASS",reader="TruckLib.Models + TruckLib.Sii",atsPrefixBindingsUnambiguous=true,
        frontAttachment="registered accessory_hookup_data on bumper slot_0, matching stock steering-wheel/light-bar system",models=checkedModels},new JsonSerializerOptions{WriteIndented=true}));
Console.WriteLine("PASS: registered bumper hookup/include, 8 empty slot_0 mounts, 64 parent transforms, unchanged rear/side sockets, geometry and optical angles.");
