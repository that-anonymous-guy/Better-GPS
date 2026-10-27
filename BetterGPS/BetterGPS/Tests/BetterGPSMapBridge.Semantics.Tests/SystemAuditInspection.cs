using System.Text.Json;
using BetterGPSNavigation;
using TruckLib.ScsMap;

internal static class SystemAuditInspection
{
    internal static void CompleteTextureVerification(string game,string docs,string manifest,string verification)
    {
        using var prior=JsonDocument.Parse(File.ReadAllText(verification));
        using var original=JsonDocument.Parse(File.ReadAllText(manifest));
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=".",ProbePath="."});
        var changed=prior.RootElement.GetProperty("changed").EnumerateArray().Select(x=>x.GetString()!).ToList();
        foreach(var entry in changed.ToArray())
        {
            if(!entry.StartsWith("MISSING:")||!entry.EndsWith("#texture"))continue;
            var path=entry[8..];var raw=path[..^8];
            var expected=original.RootElement.GetProperty("resources").EnumerateArray().Single(r=>r.GetProperty("path").GetString()==path);
            var hash=assets.TextureSha256(raw);
            var temporary=verification+".textures.json";assets.WriteManifest(temporary);
            using var uses=JsonDocument.Parse(File.ReadAllText(temporary));
            var actual=uses.RootElement.GetProperty("resources").EnumerateArray().Single(r=>r.GetProperty("path").GetString()==path);
            if(hash==expected.GetProperty("sha256").GetString()&&actual.GetProperty("source").GetString()==expected.GetProperty("source").GetString())changed.Remove(entry);
        }
        File.WriteAllText(verification,JsonSerializer.Serialize(new{sameSources=prior.RootElement.GetProperty("sameSources").GetBoolean(),
            checkedCount=prior.RootElement.GetProperty("checkedCount").GetInt32(),changed,
            catalogue=prior.RootElement.GetProperty("catalogue").GetString()},LiveProtocol.Json));
        Console.WriteLine($"Remaining changed resources after packed texture verification: {changed.Count}.");
    }
    internal static void VerifyWorldAssets(string game,string docs,string manifest,string output)
    {
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=".",ProbePath="."});
        using var data=JsonDocument.Parse(File.ReadAllText(manifest));
        var oldSources=data.RootElement.GetProperty("sources").EnumerateArray().Select(x=>x.GetString()).ToArray();
        var changed=new List<string>();int checkedCount=0;
        foreach(var row in data.RootElement.GetProperty("resources").EnumerateArray())
        {
            var path=row.GetProperty("path").GetString()!;
            try
            {
                using var stream=assets.Loader.Open(path);
                var hash=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(stream));
                if(hash!=row.GetProperty("sha256").GetString()||GameAssets.SourceOf(assets.Loader,path)!=row.GetProperty("source").GetString())changed.Add(path);
            }
            catch(IOException){changed.Add("MISSING:"+path);}
            checkedCount++;
        }
        var sectors=assets.Loader.GetFiles("/map/usa").Where(p=>p.EndsWith(".base",StringComparison.OrdinalIgnoreCase))
            .Select(Sector.SectorCoordsFromSectorFilePath).Distinct().OrderBy(c=>c.X).ThenBy(c=>c.Z).ToArray();
        var catalogue=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(string.Join('|',sectors.Select(c=>$"{c.X}:{c.Z}")))));
        File.WriteAllText(output,JsonSerializer.Serialize(new{sameSources=oldSources.SequenceEqual(assets.Sources),checkedCount,changed,catalogue},LiveProtocol.Json));
        Console.WriteLine($"Compared {checkedCount} effective resource hashes; changed={changed.Count}.");
    }
    internal static void CacheKeys(string game,string docs,string oldBinary,string output)
    {
        var mounts=GameAssets.ReadMounts(Path.Combine(docs,"game.log.txt"));
        string Key(string assembly)
        {
            var rows=new List<string>{"asset-index-v2",Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes(assembly)))};
            foreach(var mount in mounts)
            {
                rows.Add(mount.Pool+":"+Path.GetFullPath(mount.Path));
                IEnumerable<string> paths=Directory.Exists(mount.Path)?Directory.EnumerateFiles(mount.Path,"*",SearchOption.AllDirectories).Order(StringComparer.Ordinal):[mount.Path];
                foreach(var path in paths)rows.Add(AssetIndexCache.ResourceIdentity(path,mount.Pool=="mod"||Directory.Exists(mount.Path)));
            }
            return Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(string.Join('\n',rows))));
        }
        File.WriteAllText(output,JsonSerializer.Serialize(new{oldKey=Key(oldBinary),newKey=Key(typeof(LiveGuidanceEngine).Assembly.Location),
            policy="Only unchanged raw readers and world schema may be upgraded; route/network caches must be rebuilt"},LiveProtocol.Json));
    }
    internal static void Run(string game,string docs,string directory,string output)
    {
        Directory.CreateDirectory(output);
        var routeFile=Directory.GetFiles(directory,"*_route.json").Single(p=>!Path.GetFileName(p).Contains("_head_"));
        var route=JsonSerializer.Deserialize<LiveMessage>(File.ReadAllBytes(routeFile),LiveProtocol.Json)!.Route!;
        var native=File.ReadLines(Path.Combine(directory,"capture.jsonl"))
            .Select(l=>NativeCaptureProtocol.Decode(System.Text.Encoding.UTF8.GetBytes(l)))
            .First(m=>m.Type=="RawRouteSnapshot").Route!;
        var options=new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."};
        using var assets=GameAssets.Open(options);
        var loader=new RouteMapLoader(assets,options,output,null);
        var map=Map.Open(options.MapPath,assets.Loader,loader.Select(native)?.ToArray());
        var extractor=new ManeuverFeatureExtractor(new PpdResolver(assets.Loader));
        var rows=new List<object>();
        foreach(var action in route.Actions)
        {
            var span=route.RoadIdentities!.Spans.Single(s=>s.SegmentIndex==action.SegmentIndex);
            var prefab=(Prefab)map.MapItems[Convert.ToUInt64(span.ItemUid,16)];
            int Index(string uid)=>prefab.Nodes.FindIndex(n=>n.Uid==Convert.ToUInt64(uid,16));
            var features=extractor.Analyze(prefab,Index(span.EntryNodeUid),Index(span.ExitNodeUid));
            rows.Add(new{action.Id,action.SegmentIndex,action.Instruction,action.Confidence,features});
        }
        File.WriteAllText(Path.Combine(output,"maneuver-features.json"),JsonSerializer.Serialize(rows,LiveProtocol.Json));
        assets.WriteManifest(Path.Combine(output,"inspection-asset-manifest.json"));
        Console.WriteLine($"Inspected {rows.Count} recorded maneuver prefabs without recompiling the production route.");
    }
}
