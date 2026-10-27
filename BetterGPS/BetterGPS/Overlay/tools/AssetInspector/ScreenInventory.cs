using System.Reflection;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Text.Json;

// Offline, read-only stock/DLC audit. No game hooks or runtime configuration writes.
static class ScreenInventory
{
 public static void Run(object fs,Type owner,IDisposable assets,string output)
 {
  var ft=fs.GetType();var get=ft.GetMethod("GetFiles",new[]{typeof(string)})!;
  var read=ft.GetMethod("ReadAllBytes",new[]{typeof(string)})!;
  var texture=owner.GetMethod("ReadTexture",BindingFlags.Instance|BindingFlags.NonPublic)!;
  var cache=new Dictionary<string,byte[]>();var errors=new List<string>();
  byte[] Read(string path){if(cache.TryGetValue(path,out var b))return b;b=(byte[])read.Invoke(fs,new object[]{path})!;cache[path]=b;return b;}
  string Text(string path)=>System.Text.Encoding.UTF8.GetString(Read(path));
  string[] Files(string path){try{return ((IEnumerable<string>)get.Invoke(fs,new object[]{path})!).ToArray();}catch{return [];}}
  void Export(string path,byte[] bytes){var dst=Path.Combine(output,"extracted",path.TrimStart('/'));Directory.CreateDirectory(Path.GetDirectoryName(dst)!);File.WriteAllBytes(dst,bytes);}
  var trucks=new SortedSet<string>();
  foreach(var path in Files("/def/vehicle/truck/").Where(p=>Path.GetFileName(p).StartsWith("fallback")))
   foreach(Match m in Regex.Matches(Text(path),@"/def/vehicle/truck/([^/]+)/"))trucks.Add(m.Groups[1].Value);
  // Non-fallback equipment variants are audited too; absent directories are empty.
  foreach(var id in new[]{"freightliner.cascadia2019","freightliner.cascadia2024","kenworth.t680_2022","volvo.vnr_e"})trucks.Add(id);
  var records=new List<object>();
  foreach(var id in trucks)
  {
   var definitions=new Dictionary<string,string>();var models=new SortedSet<string>();
   void Expand(string path,int depth=0){if(depth>12||definitions.ContainsKey(path))return;try{var text=Text(path);definitions[path]=text;Export(path,Read(path));foreach(Match m in Regex.Matches(text,"@include\\s+\"([^\"]+)\"")){var inc=m.Groups[1].Value;Expand(new Uri(new Uri("https://ats.invalid"+path),inc).AbsolutePath,depth+1);}foreach(Match m in Regex.Matches(text,"(?:^|\\s)(?:model|model_uk)\\s*:\\s*\"([^\"]+\\.pmd)\""))models.Add(m.Groups[1].Value);}catch(Exception e){errors.Add(path+": "+e.GetBaseException().Message);}}
   foreach(var file in Files($"/def/vehicle/truck/{id}/interior/").Where(p=>p.EndsWith(".sii")||p.EndsWith(".sui")))Expand(file);
   if(definitions.Count==0)continue;
   var materialRecords=new List<object>();
   foreach(var model in models)
   {
    try{
     var pmd=Read(model);var pmgPath=Path.ChangeExtension(model,".pmg").Replace('\\','/');var pmg=Read(pmgPath);Export(model,pmd);Export(pmgPath,pmg);
     if(BitConverter.ToUInt32(pmd)!=4)throw new InvalidDataException("Unsupported PMD version");
     var count=BitConverter.ToUInt32(pmd,4);var offset=BitConverter.ToUInt32(pmd,56);
     for(int index=0;index<count;++index){var at=(int)BitConverter.ToUInt32(pmd,(int)offset+index*4);var end=Array.IndexOf(pmd,(byte)0,at);var mat=System.Text.Encoding.UTF8.GetString(pmd,at,end-at);
      string text;try{text=Text(mat);}catch{continue;}
      foreach(Match m in Regex.Matches(text,"\"([^\"]+\\.tobj)\"")){
       var path=m.Groups[1].Value;
       if(!Regex.IsMatch(path,@"/(?:gps|dashboard\d*|cam_mirrors|camera_mirror[^/]*)\.tobj$",RegexOptions.IgnoreCase))continue;
       Export(mat,Read(mat));Export(path,Read(path));int width=0,height=0;string sha="";
       try{var bytes=(byte[])texture.Invoke(assets,new object[]{path})!;width=BitConverter.ToInt32(bytes,16);height=BitConverter.ToInt32(bytes,12);sha=Convert.ToHexString(SHA256.HashData(bytes));Export(path+".dds",bytes);}catch(Exception e){errors.Add(path+": "+e.GetBaseException().Message);}
       materialRecords.Add(new{model,pmg=pmgPath,material=index,path=mat,texture=path,width,height,textureSha256=sha,pmgSha256=Convert.ToHexString(SHA256.HashData(pmg))});
      }
     }
    }catch(Exception e){errors.Add(model+": "+e.GetBaseException().Message);}
   }
   var uiPaths=new SortedSet<string>();foreach(var text in definitions.Values)foreach(Match m in Regex.Matches(text,"\"(/ui/[^\"]+\\.sii)\""))uiPaths.Add(m.Groups[1].Value);
   foreach(var path in uiPaths){try{Export(path,Read(path));}catch(Exception e){errors.Add(path+": "+e.GetBaseException().Message);}}
   records.Add(new{id,definitions=definitions.Keys,models,uiPaths,materials=materialRecords});
   Console.WriteLine($"{id}: {definitions.Count} definitions, {models.Count} models, {materialRecords.Count} dynamic screen materials");
  }
  var json=new JsonSerializerOptions{WriteIndented=true};
  File.WriteAllText(Path.Combine(output,"inventory.json"),JsonSerializer.Serialize(new{scope="installed stock and DLC, IgnoreMods=true",trucks=records,errors},json));
  owner.GetMethod("WriteManifest")!.Invoke(assets,new object[]{Path.Combine(output,"sources.json")});
  Console.WriteLine($"Inventory: {records.Count} trucks, {errors.Count} errors; {output}");
 }
}
