using System.Numerics;
using System.IO.Compression;
using BetterGPSNavigation;
using TruckLib.HashFs;
internal static class CaptureAuditTests
{
    public static void Run()
    {
        int checks=0;void Check(bool value,string why){checks++;if(!value)throw new Exception(why);}
        var n=new NativeNode(256,-512,768,0,1,0,0,0,"0000000000000001","0000000000000002",true,true,"READ_OK");
        NodeComparison Compare(NativeNode value,Vector3? p=null,Quaternion? q=null,ulong bw=1,ulong fw=2)=>
            NativeMapAudit.Compare(0,"0000000000000003",value,p??new Vector3(1,-2,3),q??Quaternion.Identity,bw,fw);
        Check(Compare(n).Status=="MATCH","Fixed point position and connections match");
        Check(Compare(n with {Qw=-1}).Status=="MATCH","q and -q are equivalent");
        Check(Compare(n with {RawX=65536}).Issues.Contains("POSITION_MISMATCH"),"Wrong fixed point scale rejected");
        Check(Compare(n,q:Quaternion.CreateFromAxisAngle(Vector3.UnitY,MathF.PI)).Issues.Contains("ROTATION_MISMATCH"),"Opposite rotation detected");
        Check(Compare(n,fw:1).Issues.Contains("FORWARD_MISMATCH"),"Swapped connections detected");
        Check(Compare(n,p:new Vector3(float.NaN,0,0)).Issues.Contains("POSITION_MISMATCH"),"Nonfinite map position cannot validate");
        Check(Compare(n with {ForwardValid=false,Status="CONNECTION_UNAVAILABLE"}).Status=="MISMATCH","Incomplete evidence cannot validate");
        var physical=new[]{new NativePhysical("0000000000000003",0,100,0,10),new NativePhysical("0000000000000004",100,0,10,0)};
        var logical=physical.Select(p=>new NativeLogical(0,0,p.NodeUid,p.DistanceFromPreviousM,p.TotalDistanceTillEndM,p.TimeFromPreviousS,p.TotalTimeTillEndS)).ToArray();
        var raw=new NativeRoute("0000000000000001",physical,logical){Nodes=[n,n]};
        var msg=new NativeMessage(2,"RawRouteSnapshot","test",1,0,1,raw);
        Check(NativeCaptureProtocol.Decode(NativeCaptureProtocol.Encode(msg)).Route!.Nodes![0]==n,"V2 evidence roundtrip");
        bool rejected=false;try{NativeCaptureProtocol.Validate(msg with {Route=raw with {Nodes=null}});}catch(InvalidDataException){rejected=true;}
        Check(rejected,"V2 missing evidence rejected");
        rejected=false;try{NativeCaptureProtocol.Validate(msg with {Route=raw with {Physical=[physical[0],physical[1] with {TotalDistanceTillEndM=200}]}});}catch(InvalidDataException){rejected=true;}
        Check(rejected,"V2 increasing remaining distances cannot bypass suffix validation");
        Check(!(raw with {Nodes=[n,n with {RawX=500}]}).IsSuffixOf(raw),"Changed geometry cannot reuse a validated corridor");
        var mounts=GameAssets.ParseMounts([
            "[fs] device C:/game/base.scs mounted to scs pool.",
            "[fs] device C:/workshop/270880/1/latest mounted to mod pool.",
            "[fs] device C:/workshop/270880/2/universal.zip mounted to mod pool.",
            "[fs] device C:/docs mounted to user pool."]);
        Check(mounts.Count==3&&mounts[1].Path.EndsWith("latest")&&mounts[2].Path.EndsWith(".zip"),"Workshop directory and ZIP mounts included");
        string root=Path.Combine(Path.GetTempPath(),"BetterGPSAuditTests-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(root,"base","def"));Directory.CreateDirectory(Path.Combine(root,"mod","def"));
        File.WriteAllText(Path.Combine(root,"base","def","same.sii"),"base");File.WriteAllText(Path.Combine(root,"mod","def","same.sii"),"override");
        var fs=new MountedDirectoryFileSystem(Path.Combine(root,"mod"));
        Check(fs.ReadAllText("/def/same.sii")=="override","Mounted directory reads virtual path");
        bool escapes=false;try{fs.Open("/../../outside.txt");}catch(InvalidDataException){escapes=true;}
        Check(escapes,"Virtual path cannot escape mount");
        string zipPath=Path.Combine(root,"override.zip");
        using(var zip=ZipFile.Open(zipPath,ZipArchiveMode.Create)){var entry=zip.CreateEntry("def/same.sii");using var w=new StreamWriter(entry.Open());w.Write("zip override");}
        using var zipped=ZipScsFileSystem.OpenArchive(zipPath);
        var uses=new System.Collections.Concurrent.ConcurrentDictionary<string,GameAssets.ResourceUse>();
        var loader=new AssetLoader([new TrackedFileSystem(zipped,"zip",uses),new MountedDirectoryFileSystem(Path.Combine(root,"base"))]);
        Check(loader.ReadAllText("/def/same.sii")=="zip override","Highest priority ZIP overrides base");
        Check(uses["/def/same.sii"].Source=="zip"&&uses["/def/same.sii"].Sha256.Length==64,"Winning resource has provenance hash");
        var viewRoot=Path.GetFullPath("validation/capture-v2-checks");Directory.CreateDirectory(viewRoot);
        var view=Path.Combine(viewRoot,"EXEMPLE_SYNTHETIQUE.html");
        NativeMapAudit.WriteView(view,"EXEMPLE SYNTHETIQUE — pas une capture du jeu",[Compare(n),Compare(n with {RawX=512}) with {Index=1}],["Exemple de correspondance puis d'écart volontaire"]);
        var html=File.ReadAllText(view);
        Check(html.Contains("MISMATCH")&&html.Contains("POSITION_MISMATCH")&&html.Contains("<svg")&&html.Contains("row0"),"Audit view exposes mismatch and links to node rows");
        Console.WriteLine($"PASS: {checks} native evidence and mounted resource assertions.");
    }
    public static void InspectResources(string game,string docs,string output,bool loadMap=false)
    {
        Directory.CreateDirectory(output);
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath=output});
        assets.WriteManifest(Path.Combine(output,"asset_manifest.json"));
        if(assets.LoadErrors.Count>0)throw new Exception("Unreadable resources: "+string.Join(",",assets.LoadErrors));
        var bytes=assets.Loader.ReadAllBytes("/map/usa.mbd");
        if(loadMap){var map=TruckLib.ScsMap.Map.Open("/map/usa.mbd",assets.Loader);Console.WriteLine($"PASS: loaded installed map: {map.Nodes.Count} nodes, {map.MapItems.Count} items.");}
        assets.WriteManifest(Path.Combine(output,"asset_manifest.json"));
        Console.WriteLine($"PASS: {assets.Sources.Count} real mounted sources; /map/usa.mbd {bytes.Length} bytes; provenance saved.");
    }
}
