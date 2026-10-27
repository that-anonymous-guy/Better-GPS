using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
// Offline export only: selected representative geometry, not assembled variant visibility.
static class CameraMeshAudit {
 public static void Run(object fs,Type owner,IDisposable assets,string output,string allTruck=null){
  var reader=fs.GetType().GetMethod("ReadAllBytes",new[]{typeof(string)})!;var texture=owner.GetMethod("ReadTexture",BindingFlags.Instance|BindingFlags.NonPublic)!;
  var done=new HashSet<string>();var materials=new Dictionary<string,object>();var models=new List<object>();var errors=new List<string>();
  byte[] Read(string p)=>(byte[])reader.Invoke(fs,new object[]{p})!;
  void Save(string p,byte[] b){var target=Path.Combine(output,"extracted",p.TrimStart('/'));Directory.CreateDirectory(Path.GetDirectoryName(target)!);File.WriteAllBytes(target,b);}
  void Material(string p){if(!done.Add(p))return;try{var b=Read(p);Save(p,b);var text=Encoding.UTF8.GetString(b);var textures=new List<object>();foreach(Match m in Regex.Matches(text,"\"([^\"]+\\.tobj)\"")){var path=m.Groups[1].Value;if(!path.StartsWith('/'))path=new Uri(new Uri("https://ats.invalid"+p),path).AbsolutePath;try{if(done.Add(path)){Save(path,Read(path));var image=(byte[])texture.Invoke(assets,new object[]{path})!;Save(path+".dds",image);}textures.Add(new{path});}catch(Exception e){errors.Add(path+": "+e.GetBaseException().Message);}}materials[p]=new{path=p,text,textures};}catch(Exception e){errors.Add(p+": "+e.GetBaseException().Message);}}
  using var doc=JsonDocument.Parse(File.ReadAllText(Path.GetFullPath(Path.Combine(output,"../stock/camera-inventory.json"))));
  foreach(var truck in doc.RootElement.GetProperty("trucks").EnumerateArray()){
   var id=truck.GetProperty("id").GetString()!;var all=truck.GetProperty("modelPaths").EnumerateArray().Select(x=>x.GetString()!).ToArray();
   if(allTruck!=null&&id!=allTruck)continue;
   var selected=new SortedSet<string>();
   foreach(var external in new[]{false,true}){var candidates=all.Where(p=>p.Contains("/interior/")&&p.Contains("/external/")==external&&!p.Contains("/accessory/")&&!p.Contains("/upgrade/")&&!p.Contains("anim")&&!p.Contains("variant")).OrderBy(p=>p.Length).ToArray();var day=candidates.Where(p=>Regex.IsMatch(Path.GetFileName(p),@"day|^dc\.")).ToArray();if(day.Length>0)selected.Add(day.OrderByDescending(p=>p.Contains("_d.")).ThenByDescending(p=>p.Contains("exclusive")||p.Contains("prem")).ThenBy(p=>p.Length).First());else if(candidates.Length>0)selected.Add(candidates[0]);}
   var cabs=all.Where(p=>p.Contains("/cabin/")&&!p.Contains("/interior/")&&!p.Contains("anim")&&!p.Contains("wiper")&&!p.Contains("win_open")).OrderBy(p=>p.Length).ToArray();
   if(cabs.Length>0)selected.Add(cabs[0]);
   else {var bodies=all.Where(p=>!p.Contains("/interior/")&&!p.Contains("/accessory/")&&!p.Contains("/upgrade/")&&!p.Contains("anim")&&!p.Contains("chassis")&&!p.Contains("wheel")).OrderBy(p=>p.Length).ToArray();if(bodies.Length>0)selected.Add(bodies[0]);}
   if(id=="kenworth.t680_2022")foreach(var mirror in all.Where(p=>p.EndsWith("/accessory/mirror/standard_chrome.pmd")))selected.Add(mirror);
   foreach(var e in truck.GetProperty("modelEvidence").EnumerateArray())if(e.GetProperty("path").GetString() is string p&&p.Contains("/accessory/")&&!p.Contains("int_display")&&!p.Contains("_int."))selected.Add(p);
   if(allTruck!=null)selected.UnionWith(all);
   foreach(var p in selected)try{var b=Read(p);var geometry=Path.ChangeExtension(p,".pmg").Replace('\\','/');var mesh=Read(geometry);Save(p,b);Save(geometry,mesh);if(BitConverter.ToUInt32(b)!=4)throw new InvalidDataException("PMD version");uint count=BitConverter.ToUInt32(b,4),offset=BitConverter.ToUInt32(b,56);var mats=new List<string>();for(int i=0;i<count;i++){int at=(int)BitConverter.ToUInt32(b,(int)offset+i*4),end=Array.IndexOf(b,(byte)0,at);var material=Encoding.UTF8.GetString(b,at,end-at);mats.Add(material);Material(material);}models.Add(new{truck=id,path=p,geometry,materials=mats,meshSha256=Convert.ToHexString(SHA256.HashData(mesh))});}catch(Exception e){errors.Add(p+": "+e.GetBaseException().Message);}
   Console.WriteLine(id+": "+selected.Count+" geometry candidates exported");
  }
  if(allTruck!=null&&models.Count==0)throw new InvalidDataException("No models exported for truck "+allTruck);
  owner.GetMethod("WriteManifest")!.Invoke(assets,new object[]{Path.Combine(output,"sources.json")});
  File.WriteAllText(Path.Combine(output,"mesh-audit.json"),JsonSerializer.Serialize(new{scope=allTruck==null?"stock geometry and texture dependencies; variants require visibility validation":"all referenced PMD/PMG paths for stock truck "+allTruck+"; variants require visibility validation",models,materials,errors},new JsonSerializerOptions{WriteIndented=true}));Console.WriteLine($"Mesh extraction: models={models.Count}, materials={materials.Count}, warnings={errors.Count}");
 }
}
