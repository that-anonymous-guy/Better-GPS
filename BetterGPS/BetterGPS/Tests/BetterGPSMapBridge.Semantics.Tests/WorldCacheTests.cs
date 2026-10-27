using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using BetterGPSNavigation;
internal static class WorldCacheTests
{
    internal static void Run()
    {
        var root=Path.GetFullPath(Path.Combine("validation/world-cache/tests",Guid.NewGuid().ToString("N")));
        int count=0;void Check(bool ok,string message){count++;if(!ok)throw new Exception(message);}
        var key=Convert.ToHexString(SHA256.HashData("assets-a"u8));
        var cache=new WorldTileCache(root,key);
        var portion=new NetworkPortion("road:a>b","road","ROAD","a","b",[[[1,2,3],[7,8,9]]],["LANE_WIDTH_4_5M_ASSUMED"]);
        var source=new NetworkNameSource("s","42","SIGN_GROUP","GuideTo","L Street","L Street","StreetName","","",
            [10,11,12],[0,0,1],"MODEL_FACE","MEDIUM","",[],[]){Arrow="RIGHT",LanePoints=[[1,2,3]],GroupMethod="EXPLICIT_HORIZONTAL_DIVIDER"};
        var tile=new WorldTile(-1,0,[new(42,[new("main",[2],[],"board",0,0,["L Street"],[],[],[],[],[])])],
            [new("0000000000000017",[portion],["0000000000000017:test-error"])]);
        cache.Write(tile);
        var large=tile with{X=3,Geometry=[new("0000000000000017",[],[new string('X',9*1024*1024)])]};
        cache.Write(large);var bounded=new WorldTileCache(root,key);
        var largeFirst=bounded.Read(3,0);var largeSecond=bounded.Read(3,0);
        Check(largeFirst is not null&&largeSecond is not null&&!ReferenceEquals(largeFirst,largeSecond),"Highly compressed oversized pages stayed resident despite expanded memory budget");
        Check(bounded.TryGet(new(12001,0,10),42,out var largeReading)&&largeReading.Areas[0].RawStrings[0]=="L Street","Uncached oversized page lost its road names");
        var restarted=new WorldTileCache(root,key);
        Check(JsonSerializer.Serialize(restarted.Read(-1,0),LiveProtocol.Json)==JsonSerializer.Serialize(tile,LiveProtocol.Json),"Restart preserves panel overrides, world geometry, source fields and diagnostics");
        Check(restarted.TryGet(new(-1,0,10),42,out var reading)&&reading.Areas[0].RawStrings[0]=="L Street","Negative coordinates select the correct page and sign");
        Check(restarted.TryGeometry(new(-1,0,10),23,out var geometry)&&geometry.Portions[0].Paths[0][1][2]==9,"Cached lane coordinates retain their values");
        Check(!restarted.TryGet(new(1,0,10),42,out _),"Nearby pages cannot supply a name for a different sector");
        Check(!new WorldTileCache(root,Convert.ToHexString(SHA256.HashData("assets-b"u8))).TryGet(new(-1,0,10),42,out _),"Changed mounted resources reject old pages");
        var fresh=new WorldTileCache(root,key);Check(fresh.Read(0,0) is null,"Missing data uses the original reader");
        fresh.Write(tile with{X=0});Check(fresh.Read(0,0) is not null,"A missing page becomes visible when background preparation publishes it");
        var bytes=File.ReadAllBytes(cache.PathFor(-1,0));bytes[^1]^=1;File.WriteAllBytes(cache.PathFor(-1,0),bytes);
        Check(new WorldTileCache(root,key).Read(-1,0) is null,"Damaged compressed pages are rejected before interpreting road names");
        cache.Write(tile);Check(new WorldTileCache(root,key).Read(-1,0) is not null,"Interrupted or corrupt cache pages can be repaired atomically");
        cache.MarkComplete([(-1,0),(0,0)]);
        Check(cache.IsComplete([(0,0),(-1,0)])&&!cache.IsComplete([(-1,0),(0,0),(1,0)]),"Completion index accepts reordered sectors but invalidates a changed map catalogue");
        Check(WorldTileCache.Cell(new(-4001,0,-1))==(-2,-1),"Sector arithmetic agrees on both negative axes");
        var graph=new DirectedRoadNetworkGraph(new(-100,100,-100,100),[portion],[],[]);
        var reassociated=NamedRoadNetwork.Reassociate(graph,source with{Owners=["old-route-owner"],AssociationStatus="DIRECTIONAL_BRANCH_TARGET"});
        Check(!reassociated.Owners.Contains("old-route-owner"),"Cached sign readings never retain an ownership from another route graph");
        var sign=new ParsedSign(42,0,Vector3.Zero,Quaternion.Identity,0,"model-a","","","","","/model/test.pmd",[],[],[],[],[],[],[],[],false,false,false);
        Check(SignPanelExtractor.AnchorKey(sign,"main")!=SignPanelExtractor.AnchorKey(sign with{Model="model-b"},"main"),"Two definitions sharing a mesh cannot share the anchor of their different default variants");
        Check(SignPanelExtractor.AnchorKey(sign,"main")==SignPanelExtractor.AnchorKey(sign with{Uid=43,Position=new(500,0,500)},"main"),"The same local attachment can be reused on different world instances");
        Console.WriteLine($"PASS: {count} world-cache persistence, corruption, invalidation and source-isolation assertions.");
    }
}
