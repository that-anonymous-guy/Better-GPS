using System.Text.Json;
using BetterGPSNavigation;
using TruckLib.ScsMap;

internal static class InteractionFixTests
{
    internal static void Run(string output)
    {
        Directory.CreateDirectory(output);int count=0;
        void Check(bool ok,string why){count++;if(!ok)throw new Exception(why);}
        InteractionAudit.Run("","",output,false);
        using var probes=JsonDocument.Parse(File.ReadAllText(Path.Combine(output,"synthetic-guidance-probes.json")));
        foreach(var probe in probes.RootElement.EnumerateArray())Check(!probe.GetProperty("failure").GetBoolean(),probe.GetProperty("scenario").GetString()!);
        var selected=new[]{0,1,2,3,10,11};
        Check(RoundaboutGuidance.ExitOrdinal(selected,2,[(1,[0,1,7]),(1,[0,1,8]),(3,[0,1,2,3,12])],4)==2,"Passed arms and future arms confused; duplicate lanes counted");
        Check(RoundaboutGuidance.ExitOrdinal(selected,2,[(3,[0,1,2,3,12])],4)==1,"First exit ordinal");
        Check(RoundaboutGuidance.ExitOrdinal(selected,2,[(1,[0,1,7]),(1,[0,1,2,3,12])],4) is null,"Ambiguous multi-path arm was assigned a number");
        Check(RoundaboutGuidance.ExitOrdinal(selected,2,[(1,[20,21])],4) is null,"Unrelated input lane supplied an ordinal");
        var plain=new LiveRoute("straight",[new(0,"0000000000000001",new(0,0,0),new(100,0,0),0,100,0,1000)],[]);
        foreach(var kind in ServiceAreaGuidance.Kinds)
        {
            var r=plain with{ServiceAreas=[new("service",kind,350,400,-3,6,-20,20,false)]};
            var e=new LiveGuidanceEngine();e.Accept(new(2,"Hello","service",0,0,0));e.Accept(new(2,"RouteSnapshot","service",1,0,1,r));
            var g=e.Accept(new(2,"VehicleState","service",2,100,1,Vehicle:new(new(370,0,0),0,0,false)));
            Check(g.Status=="SERVICE_AREA"&&g.ServiceAreaKind==kind&&NativeBridgeCli.Describe(g).Length>20,"Service access incorrectly rejected: "+kind);
            var resume=e.Accept(new(2,"VehicleState","service",3,200,1,Vehicle:new(new(50,0,0),0,10,false,500)));
            Check(resume.Instruction=="CONTINUE"&&resume.ServiceAreaKind is null,"Service context persisted after rejoining route: "+kind);
        }
        var far=new LiveGuidanceEngine();far.Accept(new(2,"Hello","far",0,0,0));far.Accept(new(2,"RouteSnapshot","far",1,0,1,plain));
        Check(far.Accept(new(2,"VehicleState","far",2,100,1,Vehicle:new(new(1000,0,0),0,10,false,0))).Status=="OFF_ROUTE","Zero remaining caused arrival far from destination");
        var partial=new LiveGuidanceEngine();partial.Accept(new(2,"Hello","partial",0,0,0));partial.Accept(new(2,"RouteSnapshot","partial",1,0,1,plain with{IsPartial=true}));
        Check(partial.Accept(new(2,"VehicleState","partial",2,100,1,Vehicle:new(new(130,0,0),0,10,false,0))).Status!="COMPLETE","Partial route announced arrival outside its last node");
        var raw=new NativeRoute("0000000000000001",[new("0000000000000001",0,1000,0,10),new("0000000000000002",1000,0,10,0)],
            [new(0,0,"0000000000000001",0,1000,0,10),new(0,0,"0000000000000002",1000,0,10,0)]);
        Check(raw.IsSuffixOf(raw),"Identical captured route no longer reusable");
        Check(!(raw with{Physical=raw.Physical.Select(p=>p with{TotalDistanceTillEndM=p.TotalDistanceTillEndM+100}).ToArray()}).IsSuffixOf(raw),"Same IDs with changed distance axis reused");
        var action=new LiveAction("huge:0",0,0,500,"TURN_RIGHT","branch","HIGH",[
            new("action","TURN_RIGHT","HIGH","PPD","0000000000000001","branch","exact"),
            new("audit",new string('x',9*1024*1024),"LOW","DIAGNOSTIC","fixture","branch","large retained evidence")]);
        var huge=plain with{RouteId="huge",Actions=[action]};
        var compact=RouteEvidence.Pack(huge,Path.Combine(output,"large-route-evidence"));
        var bytes=LiveProtocol.Encode(new(2,"RouteSnapshot","large",0,0,1,compact));
        Check(bytes.Length<4096,"Large route still exceeds frame budget");
        var restored=RouteEvidence.Unpack(LiveProtocol.Decode(bytes).Route!);
        Check(restored.Actions[0].Proofs[1].Value.Length==9*1024*1024,"Full proof evidence was lost");
        var rebound=RouteEvidence.Unpack(compact with{RouteId="reused",IsPartial=true});
        Check(rebound.RouteId=="reused"&&rebound.Actions[0].Id=="reused:0"&&rebound.IsPartial,"Cached evidence could not rebind to a partial route");
        var eLarge=new LiveGuidanceEngine();eLarge.Accept(new(2,"Hello","large",0,0,0));eLarge.Accept(new(2,"RouteSnapshot","large",1,0,1,compact));
        Check(eLarge.Accept(new(2,"VehicleState","large",2,100,1,Vehicle:new(new(20,0,0),0,10,false))).Instruction=="TURN_RIGHT","Packed route cannot guide");
        bool rejected=false;try{RouteEvidence.Unpack(compact with{EvidenceSha256=new string('0',64)});}catch(InvalidDataException){rejected=true;}
        Check(rejected,"Mismatched route evidence accepted");
        foreach(var size in new[]{4095,600})
        {
            var descriptor=new TruckLib.Models.Ppd.PrefabDescriptor();
            var curves=(System.Collections.IList)descriptor.NavCurves;
            var curveType=descriptor.NavCurves.GetType().GetGenericArguments()[0];
            for(int i=0;i<size;i++)
            {
                var curve=Activator.CreateInstance(curveType)!;
                curveType.GetProperty("Length")!.SetValue(curve,1f);curves.Add(curve);
            }
            void Connect(int from,int to)=>((ICollection<int>)curveType.GetProperty("NextLines")!.GetValue(curves[from])!).Add(to);
            var ends=new Dictionary<int,int>();
            if(size==4095){for(int i=0;i<size;i++){if(i*2+1<size){Connect(i,i*2+1);Connect(i,i*2+2);}else ends[i]=i;}}
            else {for(int i=0;i<size-1;i++)Connect(i,i+1);ends[size-1]=1;}
            object?[] arguments=[descriptor,0,ends,false];
            var paths=(System.Collections.IEnumerable)typeof(ManeuverFeatureExtractor).GetMethod("EnumeratePaths",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Static)!.Invoke(null,arguments)!;
            var outputs=paths.Cast<object>().Select(p=>(int)p.GetType().GetProperty("TargetNodeIndex")!.GetValue(p)!).ToHashSet();
            Check(outputs.SetEquals(ends.Values),"Bounded graph search lost a late exit or deep lane path");
        }
        File.WriteAllText(Path.Combine(output,"acceptance.json"),JsonSerializer.Serialize(new{count,status="PASS",coreScenarios=6,serviceKinds=6,largeRouteWireBytes=bytes.Length},LiveProtocol.Json));
        Console.WriteLine($"PASS: {count} interaction, replay, service, roundabout and large-route acceptance assertions.");
    }
    internal static void RealRoundabouts(string game,string docs,string output)
    {
        Directory.CreateDirectory(output);using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."});
        var map=Map.Open("/map/usa.mbd",assets.Loader);var resolver=new PpdResolver(assets.Loader);var features=new ManeuverFeatureExtractor(resolver);
        IEnumerable<MapItem> Items(IEnumerable<MapItem> items){foreach(var item in items){yield return item;if(item is Compound c)foreach(var child in Items(c.MapItems.Values))yield return child;}}
        var prefabs=Items(map.MapItems.Values).OfType<Prefab>().Where(p=>resolver.TryGetPath(p.Model.ToString(),out var path)&&path.Contains("roundabout",StringComparison.OrdinalIgnoreCase))
            .GroupBy(p=>p.Model.ToString()).Select(g=>g.First()).ToArray();var rows=new List<object>();int annotated=0,numbered=0;
        foreach(var prefab in prefabs)
        for(int entry=0;entry<prefab.Nodes.Count;entry++)for(int exit=0;exit<prefab.Nodes.Count;exit++)
        {
            var f=features.Analyze(prefab,entry,exit);if(f.SelectedBranchCount==0)continue;
            if(!resolver.TryResolve(prefab.Model.ToString(),out _,out var descriptor,out _)||descriptor is null)continue;
            var id=prefab.Uid.ToString("X16");var branch=id+":"+entry+"->"+exit;
            var action=new LiveAction("circle",0,0,0,"TURN_RIGHT",branch,"HIGH",[new("action","TURN_RIGHT","HIGH","PPD",id,branch,"selected native ports")]);
            var converted=RoundaboutGuidance.Annotate(action,f,descriptor);
            if(converted.Instruction=="ROUNDABOUT")annotated++;
            if(converted.RoundaboutExitNumber is {} n){if(n<1||n>prefab.Nodes.Count)throw new Exception("Invalid real roundabout ordinal");numbered++;}
            if(RoundaboutGuidance.Annotate(action,f with{PpdPath="/prefab/cross/ordinary.ppd"},descriptor).Instruction=="ROUNDABOUT")throw new Exception("Ordinary AI loop became a roundabout");
            rows.Add(new{model=prefab.Model.ToString(),entry,exit,f.PpdPath,converted.Instruction,converted.RoundaboutExitNumber,f.AnyCycleEncountered});
        }
        if(annotated==0)throw new Exception("No mounted roundabout recognized");
        File.WriteAllText(Path.Combine(output,"real-roundabouts.json"),JsonSerializer.Serialize(new{models=prefabs.Length,paths=rows.Count,annotated,numbered,rows},LiveProtocol.Json));
        Console.WriteLine($"PASS: {prefabs.Length} mounted roundabout models; {annotated} cyclic paths recognized, {numbered} numbered conservatively.");
    }
}
