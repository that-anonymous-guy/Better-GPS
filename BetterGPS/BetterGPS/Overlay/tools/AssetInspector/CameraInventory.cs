using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
// Read-only inventory. Camera view definitions are not automatically physical cameras.
static class CameraInventory {
 public static void Run(object fs,Type owner,IDisposable assets,string output){
  var ft=fs.GetType();var rawSources=((System.Collections.IEnumerable)owner.GetField("resourceFileSystems",BindingFlags.Instance|BindingFlags.NonPublic)!.GetValue(assets)!).Cast<object>().ToArray();var get=ft.GetMethod("GetFiles",new[]{typeof(string)})!;var reader=ft.GetMethod("ReadAllBytes",new[]{typeof(string)})!;
  var errors=new List<string>();var readCache=new Dictionary<string,byte[]>();
  byte[] Read(string p){if(readCache.TryGetValue(p,out var b))return b;b=(byte[])reader.Invoke(fs,new object[]{p})!;readCache[p]=b;return b;}
  string Text(string p)=>Encoding.UTF8.GetString(Read(p));
  string[] Files(string root){try{return ((IEnumerable<string>)get.Invoke(fs,new object[]{root})!).ToArray();}catch{return [];}}
  IEnumerable<string> Tree(string root){var seen=new HashSet<string>();var pending=new Queue<string>();pending.Enqueue(root);while(pending.Count>0&&seen.Count<5000){var path=pending.Dequeue();if(!seen.Add(path))continue;foreach(var p in Files(path))yield return p;var children=new SortedSet<string>();foreach(var raw in rawSources){try{var rt=raw.GetType();var exists=rt.GetMethod("EntryExists",new[]{typeof(string)});if(exists!=null&&!exists.Invoke(raw,new object[]{path})!.ToString()!.Contains("Directory",StringComparison.OrdinalIgnoreCase))continue;var method=rt.GetMethod("GetDirectoryListing",new[]{typeof(string),typeof(bool),typeof(bool)});if(method==null)continue;var listing=method.Invoke(raw,new object[]{path,false,true});if(listing==null)continue;foreach(var child in (IEnumerable<string>)listing.GetType().GetProperty("Subdirectories")!.GetValue(listing)!)children.Add(child);}catch{}}foreach(var d in children)pending.Enqueue(d.EndsWith('/')?d:d+'/');}}
  void Export(string p){var dst=Path.Combine(output,"extracted",p.TrimStart('/'));Directory.CreateDirectory(Path.GetDirectoryName(dst)!);File.WriteAllBytes(dst,Read(p));}
  var physical=new Regex(@"/accessory/(?:s_)?mirror/(?:camera|cam_)|cam_mirrors|camera_mirror|mirror_camera(?!_presets)|digital.?mirror|mirrorcam|mirror.?eye|\bcms\b|\badas\b|mpc2|backup.?cam|rear.?cam|safety.?cam|front.?cam",RegexOptions.IgnoreCase);
  var weak=new Regex(@"camera|mirror|sensor|windshield",RegexOptions.IgnoreCase);
  var ids=new SortedSet<string>();foreach(var f in Files("/def/vehicle/truck/").Where(p=>Path.GetFileName(p).StartsWith("fallback")))foreach(Match m in Regex.Matches(Text(f),@"/def/vehicle/truck/([^/]+)/"))ids.Add(m.Groups[1].Value);
  foreach(var id in new[]{"freightliner.cascadia2019","freightliner.cascadia2024","kenworth.t680_2022","volvo.vnr_e"})ids.Add(id);
  var records=new List<object>();
  foreach(var id in ids){var definitions=Tree($"/def/vehicle/truck/{id}/").Where(p=>p.EndsWith(".sii")||p.EndsWith(".sui")).Distinct().ToArray();if(definitions.Length==0)continue;
   var evidence=new List<object>();var models=new SortedSet<string>();var cameraDefs=new SortedSet<string>();int defCount=0;
   foreach(var f in definitions){try{var text=Text(f);++defCount;foreach(Match m in Regex.Matches(text,"\"(/[^\"]+\\.pmd)\""))models.Add(m.Groups[1].Value);bool hit=physical.IsMatch(f+"\n"+text),mirror=f.Contains("/mirror/")||f.Contains("/f_mirror/");if(hit||mirror){Export(f);if(hit)foreach(Match linked in Regex.Matches(text,"\"(/[^\"]+(?:\\.sii|\\.tobj))\"")){var link=linked.Groups[1].Value;if(link.StartsWith("/ui/")||link.EndsWith(".tobj"))try{Export(link);}catch(Exception e){errors.Add(link+": "+e.GetBaseException().Message);}}evidence.Add(new{path=f,physicalCandidate=hit,text,sha256=Convert.ToHexString(SHA256.HashData(Read(f)))});foreach(Match m in Regex.Matches(text,"\"(/[^\"]+\\.pmd)\""))models.Add(m.Groups[1].Value);}foreach(Match m in Regex.Matches(text,"\"(/def/camera/[^\"]+)\""))cameraDefs.Add(m.Groups[1].Value);}catch(Exception e){errors.Add(f+": "+e.GetBaseException().Message);}}
   var modelRoot="/vehicle/truck/"+id.Replace("intnational","international").Replace('.','_')+"/";var modelFiles=Tree(modelRoot).Distinct().ToArray();foreach(var f in modelFiles.Where(p=>p.EndsWith(".pmd")))models.Add(f);
   var modelEvidence=new List<object>();foreach(var f in models){try{var b=Read(f);var strings=Regex.Matches(Encoding.Latin1.GetString(b),@"[ -~]{4,}").Select(m=>m.Value).ToArray();var hits=strings.Where(t=>physical.IsMatch(t)).ToArray();if(hits.Length>0||physical.IsMatch(f)){Export(f);try{Export(Path.ChangeExtension(f,".pmg").Replace('\\','/'));}catch(Exception e){errors.Add(f+": mesh "+e.GetBaseException().Message);}modelEvidence.Add(new{path=f,cameraStrings=hits,sha256=Convert.ToHexString(SHA256.HashData(b))});}}catch(Exception e){errors.Add(f+": "+e.GetBaseException().Message);}}
   var names=modelFiles.Where(p=>weak.IsMatch(p)).ToArray();
   records.Add(new{id,definitionsRead=defCount,modelRoot,modelFiles=modelFiles.Length,modelsRead=models.Count,modelPaths=models,definitions=evidence,modelEvidence,candidateFiles=names,cameraDefinitionReferences=cameraDefs});Console.WriteLine($"{id}: defs={defCount}, models={models.Count}, evidence={evidence.Count}/{modelEvidence.Count}, named={names.Length}");
  }
  var cameraUnits=new List<object>();foreach(var f in Tree("/def/camera/").Where(p=>p.EndsWith(".sii")||p.EndsWith(".sui")).Distinct()){try{var text=Text(f);if(physical.IsMatch(f)&&!f.Contains("cutscene")){Export(f);cameraUnits.Add(new{path=f,text});}}catch(Exception e){errors.Add(f+": "+e.GetBaseException().Message);}}
  File.WriteAllText(Path.Combine(output,"camera-inventory.json"),JsonSerializer.Serialize(new{scope="installed stock/DLC, IgnoreMods=true; metadata candidates do not prove absence of baked camera geometry",trucks=records,cameraUnits,errors},new JsonSerializerOptions{WriteIndented=true}));
  owner.GetMethod("WriteManifest")!.Invoke(assets,new object[]{Path.Combine(output,"sources.json")});Console.WriteLine($"Camera inventory: {records.Count} trucks; {cameraUnits.Count} camera unit candidates; errors={errors.Count}");
 }
}
