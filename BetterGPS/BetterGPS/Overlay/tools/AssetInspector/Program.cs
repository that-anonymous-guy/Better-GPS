using System.Reflection;
using System.Runtime.Loader;
var bin=Path.GetFullPath(args[0]);
AssemblyLoadContext.Default.Resolving+=(context,name)=>{var path=Path.Combine(bin,name.Name+".dll");return File.Exists(path)?context.LoadFromAssemblyPath(path):null;};
var assembly=Assembly.LoadFrom(Path.Combine(bin,"BetterGPSNavigation.dll"));
var type=assembly.GetType("BetterGPSNavigation.Options")!;var options=Activator.CreateInstance(type)!;
foreach(var pair in new[]{("GameRoot",args[1]),("DocumentsRoot",args[2]),("ProbePath","."),("OutputRoot",args[3])})type.GetProperty(pair.Item1)!.SetValue(options,pair.Item2);
if(args.Skip(4).Any(a=>a is "--screen-inventory" or "--camera-inventory" or "--mesh-audit" or "--stock"))type.GetProperty("IgnoreMods")!.SetValue(options,true);
var owner=assembly.GetType("BetterGPSNavigation.GameAssets")!;using var assets=(IDisposable)owner.GetMethod("Open")!.Invoke(null,new[]{options})!;
var fs=owner.GetProperty("Loader")!.GetValue(assets)!;var fsType=fs.GetType();
Console.WriteLine("FS methods: "+string.Join("; ",fsType.GetMethods().Where(m=>m.Name is "GetFiles" or "GetDirectories" or "ReadAllBytes" or "ReadAllText" or "OpenRead").Select(m=>m.ToString())));
Directory.CreateDirectory(args[3]);
var get=fsType.GetMethods().First(m=>m.Name=="GetFiles"&&m.GetParameters().Length==1);
if(args.Skip(4).FirstOrDefault(a=>a.StartsWith("--tree=")) is string treeArg){
 var roots=new Queue<string>();roots.Enqueue(treeArg[7..]);var seen=new HashSet<string>();
 var sources=((System.Collections.IEnumerable)owner.GetField("resourceFileSystems",BindingFlags.Instance|BindingFlags.NonPublic)!.GetValue(assets)!).Cast<object>().ToArray();
 var found=new List<string>();var read=fsType.GetMethod("ReadAllBytes",new[]{typeof(string)})!;
 while(roots.Count>0&&seen.Count<20000){var root=roots.Dequeue();if(!seen.Add(root))continue;try{foreach(var path in (IEnumerable<string>)get.Invoke(fs,new object[]{root})!){found.Add(path);if(args.Contains("--mirror-units")&&(path.EndsWith(".sii")||path.EndsWith(".sui"))){var bytes=(byte[])read.Invoke(fs,new object[]{path})!;var text=System.Text.Encoding.UTF8.GetString(bytes);if(text.Contains("mirror_camera")||text.Contains("parking_camera")){var target=Path.Combine(args[3],path.TrimStart('/'));Directory.CreateDirectory(Path.GetDirectoryName(target)!);File.WriteAllBytes(target,bytes);Console.WriteLine("MATCH "+path+" "+text[..Math.Min(text.Length,700)].Replace("\n"," "));}}}}catch{}
 foreach(var src in sources)try{var rt=src.GetType();var exists=rt.GetMethod("EntryExists",new[]{typeof(string)});if(exists!=null&&!exists.Invoke(src,new object[]{root})!.ToString()!.Contains("Directory"))continue;var listing=rt.GetMethod("GetDirectoryListing",new[]{typeof(string),typeof(bool),typeof(bool)})!.Invoke(src,new object[]{root,false,true});foreach(var path in (IEnumerable<string>)listing!.GetType().GetProperty("Subdirectories")!.GetValue(listing)!)roots.Enqueue(path.EndsWith('/')?path:path+'/');}catch{}
 }
 File.WriteAllLines(Path.Combine(args[3],"tree-files.txt"),found.Distinct().Order());Console.WriteLine("Tree: "+seen.Count+" directories, "+found.Distinct().Count()+" files");return;
}
if(args.Skip(4).Contains("--mesh-audit")){CameraMeshAudit.Run(fs,owner,assets,args[3],args.Skip(4).FirstOrDefault(a=>a.StartsWith("--all-truck="))?[12..]);return;}
if(args.Skip(4).Contains("--camera-inventory")){CameraInventory.Run(fs,owner,assets,args[3]);return;}
if(args.Skip(4).Contains("--screen-inventory")){ScreenInventory.Run(fs,owner,assets,args[3]);return;}
if(args.Skip(4).Contains("--export-shields")){
 var readTexture=owner.GetMethod("ReadTexture",BindingFlags.Instance|BindingFlags.NonPublic)!;
 var normalize=assembly.GetType("BetterGPSNavigation.RoadOverlayResolver")!.GetMethod("NormalizeRoadToken",BindingFlags.Static|BindingFlags.Public|BindingFlags.NonPublic)!;
 var paths=((IEnumerable<string>)get.Invoke(fs,new object[]{"/material/ui/map/road/"})!).Where(p=>p.EndsWith(".tobj",StringComparison.OrdinalIgnoreCase)).ToArray();
 var atlasSizes=Directory.EnumerateFiles(args[3],"*.dds").ToDictionary(Path.GetFullPath,p=>new FileInfo(p).Length,StringComparer.OrdinalIgnoreCase);
 long atlasBytes=atlasSizes.Values.Sum();
 var exported=new List<object>();
 foreach(var path in paths){try{var token=Path.GetFileNameWithoutExtension(path)[5..];var identity=(string)normalize.Invoke(null,new object[]{token})!;if(string.IsNullOrWhiteSpace(identity)||!System.Text.RegularExpressions.Regex.IsMatch(identity,@"^[A-Z]{1,3}-[0-9]+[A-Z]?$"))continue;
 var bytes=(byte[])readTexture.Invoke(assets,new object[]{path})!;var target=Path.GetFullPath(Path.Combine(args[3],identity+".dds"));
 if(bytes.Length>4*1024*1024||atlasBytes+bytes.Length>60L*1024*1024){Console.WriteLine("Shield atlas budget reached; route text remains available: "+identity);continue;}
 File.WriteAllBytes(target+".tmp",bytes);File.Move(target+".tmp",target,true);atlasBytes-=atlasSizes.GetValueOrDefault(target);atlasSizes[target]=bytes.Length;atlasBytes+=bytes.Length;exported.Add(new{path,identity,bytes=bytes.Length});}catch(Exception e){Console.WriteLine("FAILED "+path+": "+e.GetBaseException().Message);}}
 // Share the active mounted palette and environment settings with the renderer.
 // All preparation happens in this background process, never on the render thread.
 try{
  var textReader=fsType.GetMethod("ReadAllText",new[]{typeof(string)})!;
  string Read(string path)=>(string)textReader.Invoke(fs,new object[]{path})!;
  string Expand(string path,int depth=0){if(depth>12)throw new InvalidDataException("Recursive include");var text=Read(path);return System.Text.RegularExpressions.Regex.Replace(text,"@include\\s+\"([^\"]+)\"",m=>Expand(m.Groups[1].Value.StartsWith('/')?m.Groups[1].Value:path[..(path.LastIndexOf('/')+1)]+m.Groups[1].Value,depth+1));}
  var map=Expand("/def/map_data.sii");var fields=System.Text.RegularExpressions.Regex.Matches(map,@"(?m)^\s*(\w+):\s*0x([0-9a-fA-F]{8})").Cast<System.Text.RegularExpressions.Match>().GroupBy(m=>m.Groups[1].Value).ToDictionary(g=>g.Key,g=>Convert.ToUInt32(g.Last().Groups[2].Value,16));
  foreach(System.Text.RegularExpressions.Match m in System.Text.RegularExpressions.Regex.Matches(map,"map_area_color_name\\[\\]:\\s*\"([^\"]+)\"\\s*map_area_color\\[\\]:\\s*0x([0-9a-fA-F]+)[^\\n]*\\s*map_area_discovered_color\\[\\]:\\s*0x([0-9a-fA-F]+)")){fields["area_"+m.Groups[1].Value]=Convert.ToUInt32(m.Groups[2].Value,16);fields["area_discovered_"+m.Groups[1].Value]=Convert.ToUInt32(m.Groups[3].Value,16);}
  var keys=System.Text.Json.JsonSerializer.Deserialize<string[]>(File.ReadAllText(Path.Combine(AppContext.BaseDirectory,"palette_keys.json")))!;
  var colors=new float[32*4];for(int i=0;i<Math.Min(32,keys.Length);i++)if(fields.TryGetValue(keys[i],out var c))for(int j=0;j<4;j++)colors[i*4+j]=((c>>(j*8))&255)/255f;
  var binary=new byte[colors.Length*4];Buffer.BlockCopy(colors,0,binary,0,binary.Length);File.WriteAllBytes(Path.Combine(args[3],"map-source.bin.tmp"),binary);File.Move(Path.Combine(args[3],"map-source.bin.tmp"),Path.Combine(args[3],"map-source.bin"),true);
  var env=Read("/def/env_data.sii");var climate=Read("/def/climate.sii");var profile=System.Text.RegularExpressions.Regex.Match(climate,@"climate_profile\s*:\s*climate\.default\s*\{([\s\S]*?)\}").Groups[1].Value;
  double Number(string text,string key,double fallback){var m=System.Text.RegularExpressions.Regex.Match(text,@"\b"+key+@":\s*([-+\d.eE]+)");return m.Success&&double.TryParse(m.Groups[1].Value,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out var n)?n:fallback;}
  double Pair(string key,int which,double fallback){var m=System.Text.RegularExpressions.Regex.Match(profile,@"\b"+key+@":\s*\(([-+\d.eE]+)\s*,\s*([-+\d.eE]+)\)");return m.Success&&double.TryParse(m.Groups[which+1].Value,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out var n)?n:fallback;}
  double[] solar={Pair("map_origin",0,39),Pair("map_factor",0,-1.7706234e-4),Pair("map_factor",1,1.76689948e-4),Number(profile,"standard_paralel_1",33),Number(profile,"standard_paralel_2",45),Number(env,"day_in_year",172),Number(env,"summer_time",1),Number(env,"lamps_on_elevation",5),profile.Contains("lambert_conic")?1:0};
  binary=new byte[solar.Length*8];Buffer.BlockCopy(solar,0,binary,0,binary.Length);File.WriteAllBytes(Path.Combine(args[3],"solar.bin.tmp"),binary);File.Move(Path.Combine(args[3],"solar.bin.tmp"),Path.Combine(args[3],"solar.bin"),true);
 }catch(Exception e){Console.WriteLine("Theme metadata fallback: "+e.GetBaseException().Message);}
 File.WriteAllText(Path.Combine(args[3],"manifest.json.tmp"),System.Text.Json.JsonSerializer.Serialize(exported));File.Move(Path.Combine(args[3],"manifest.json.tmp"),Path.Combine(args[3],"manifest.json"),true);
 Console.WriteLine("Extracted "+exported.Count+" ATS map textures.");return;
}
foreach(var root in args.Skip(4).Where(a=>a!="--stock")){
 if(root.StartsWith("--texture=")){var path=root[10..];var read=owner.GetMethod("ReadTexture",BindingFlags.Instance|BindingFlags.NonPublic)!;var bytes=(byte[])read.Invoke(assets,new object[]{path})!;var target=Path.Combine(args[3],Path.GetFileNameWithoutExtension(path)+".dds");File.WriteAllBytes(target,bytes);Console.WriteLine(path+": DDS "+bytes.Length+" bytes");continue;}
 if(root.EndsWith("/")){var files=((IEnumerable<string>)get.Invoke(fs,new object[]{root})!).ToArray();File.WriteAllLines(Path.Combine(args[3],root.Trim('/').Replace('/','_')+".txt"),files);Console.WriteLine(root+": "+files.Length+" files; "+string.Join(", ",files.Take(8)));}
 else {var read=fsType.GetMethod("ReadAllBytes",new[]{typeof(string)})!;var bytes=(byte[])read.Invoke(fs,new object[]{root})!;var destination=Path.Combine(args[3],root.TrimStart('/').Replace('/',Path.DirectorySeparatorChar));Directory.CreateDirectory(Path.GetDirectoryName(destination)!);File.WriteAllBytes(destination,bytes);Console.WriteLine(root+": extracted "+bytes.Length+" bytes");}
}
