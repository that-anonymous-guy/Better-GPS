using System.Text.Json;
using BetterGPSNavigation;
using TruckLib.ScsMap;

internal static class InteractionAudit
{
    internal static void Run(string game,string docs,string output,bool inspectMap=true)
    {
        Directory.CreateDirectory(output);
        var observations=new List<object>();
        LiveAction Action(string id,int segment,double nav)=>new(id,segment,segment,nav,"TURN_RIGHT",id,"HIGH",
            [new("action","TURN_RIGHT","HIGH","PPD","0000000000000001",id,"selected native branch")]);
        var route=new LiveRoute("audit",Enumerable.Range(0,5).Select(i=>new LiveEdge(i,$"{i+1:X16}",new(i*100,0,0),new((i+1)*100,0,0),i*100,(i+1)*100,i*1000,(i+1)*1000)).ToArray(),
            [Action("first",1,1500),Action("second",3,3500)]);
        LiveGuidanceEngine Engine(LiveRoute r)
        {
            var e=new LiveGuidanceEngine();e.Accept(new(2,"Hello","audit",0,0,0));e.Accept(new(2,"RouteSnapshot","audit",1,0,1,r));return e;
        }
        LiveGuidance Frame(LiveGuidanceEngine e,long seq,double x,double y=0,double? remaining=null)=>
            e.Accept(new(2,"VehicleState","audit",seq,seq*100,1,Vehicle:new(new(x,y,0),0,10,false,remaining)));
        var e=Engine(route);Frame(e,2,50);var passed=Frame(e,3,220);var relocated=Frame(e,4,10);var resumed=Frame(e,5,10);
        observations.Add(new{scenario="teleport_back_on_same_native_route",expectedAction="first",observedAction=resumed.ActionId,
            passedAction=passed.ActionId,relocalization=relocated.Status,failure=resumed.ActionId!="first"});
        e=Engine(route);Frame(e,2,50);var near=Frame(e,3,145);Frame(e,4,130);var away=Frame(e,5,115);
        observations.Add(new{scenario="urgent_phase_after_short_backtrack",expectedPhase="PREPARE",observedPhase=away.Status,
            before=near.Status,distanceM=away.DistanceM,failure=away.Status=="EXECUTE"});
        var straight=new LiveRoute("audit",[new(0,"0000000000000001",new(0,0,0),new(100,0,0),0,100,0,1000)],[]);
        var beyond=Frame(Engine(straight),2,130,remaining:0);
        observations.Add(new{scenario="destination_30m_beyond_last_map_node",expectedStatus="COMPLETE_OR_FINAL_APPROACH",observedStatus=beyond.Status,
            failure=beyond.Status=="OFF_ROUTE"});
        var depotStart=Frame(Engine(straight),2,-30,remaining:1300);
        observations.Add(new{scenario="start_30m_before_first_map_node",expectedStatus="DEPOT_EXIT_OR_APPROACH",observedStatus=depotStart.Status,
            failure=depotStart.Status=="OFF_ROUTE"});
        var repeated=new LiveRoute("audit",[
            new(0,"0000000000000001",new(0,0,0),new(100,0,0),0,100,0,1000),
            new(1,"0000000000000002",new(100,0,0),new(0,0,0),100,200,1000,2000),
            new(2,"0000000000000001",new(0,0,0),new(100,0,0),200,300,2000,3000)],
            [Action("first_visit",0,800),Action("second_visit",2,2800)]){NavigationRemainingAtOriginM=3000};
        var repeat=Frame(Engine(repeated),2,20,remaining:800);
        observations.Add(new{scenario="same_road_revisited_after_waypoint",expectedAction="second_visit",observedAction=repeat.ActionId,
            repeat.NavStationM,failure=repeat.ActionId!="second_visit"});
        var stacked=new LiveRoute("audit",[
            new(0,"0000000000000001",new(0,0,0),new(100,0,0),0,100,0,1000),
            new(1,"0000000000000002",new(100,0,0),new(100,4,0),100,104,1000,1040),
            new(2,"0000000000000003",new(100,4,0),new(0,4,0),104,204,1040,2040),
            new(3,"0000000000000004",new(0,4,0),new(100,4,0),204,304,2040,3040)],
            [Action("lower",0,800),Action("upper",3,2800)]){NavigationRemainingAtOriginM=3040};
        var overpass=Frame(Engine(stacked),2,20,2.5,2840);
        observations.Add(new{scenario="close_stacked_roads_pose_height_offset",expectedAction="lower",observedAction=overpass.ActionId,
            overpass.NavStationM,failure=overpass.ActionId!="lower"});
        File.WriteAllText(Path.Combine(output,"synthetic-guidance-probes.json"),JsonSerializer.Serialize(observations,LiveProtocol.Json));
        if(!inspectMap){Console.WriteLine($"Probed {observations.Count} guidance state scenarios.");return;}
        Console.WriteLine($"Probed {observations.Count} guidance state scenarios; inspecting mounted map objects...");
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,OutputRoot=output,ProbePath="."});
        var map=Map.Open("/map/usa.mbd",assets.Loader);
        var counts=new Dictionary<string,int>();var prefabTokens=new Dictionary<string,int>();var examples=new Dictionary<string,List<object>>();
        IEnumerable<MapItem> Items(IEnumerable<MapItem> input)
        {
            foreach(var item in input){yield return item;if(item is Compound c)foreach(var child in Items(c.MapItems.Values))yield return child;}
        }
        var seen=new HashSet<ulong>();long duplicates=0;
        foreach(var item in Items(map.MapItems.Values))
        {
            if(!seen.Add(item.Uid)){duplicates++;continue;}
            var kind=item.GetType().Name;counts[kind]=counts.GetValueOrDefault(kind)+1;
            if(item is Prefab p){var token=p.Model.ToString();prefabTokens[token]=prefabTokens.GetValueOrDefault(token)+1;}
            if(kind is "Road" or "Prefab" or "Sign" or "Model" or "Terrain" or "Building" or "Curve" or "Vegetation" or "Fence")continue;
            if(!examples.TryGetValue(kind,out var list))examples[kind]=list=[];
            if(list.Count>=4)continue;
            var props=new Dictionary<string,string>();
            foreach(var property in item.GetType().GetProperties().Where(p=>p.Name is "Name" or "Type" or "Action" or "TriggerType" or "Model" or "FerryName"))
                try{props[property.Name]=property.GetValue(item)?.ToString()??"";}catch{}
            list.Add(new{uid=item.Uid.ToString("X16"),props});
        }
        File.WriteAllText(Path.Combine(output,"mounted-map-census.json"),JsonSerializer.Serialize(new{sectors=map.Sectors.Count,nodes=map.Nodes.Count,uniqueItemUids=seen.Count,duplicateVisitsSkipped=duplicates,counts,prefabTokens,examples,sources=assets.Sources},LiveProtocol.Json));
        Console.WriteLine($"Inspected {map.Sectors.Count} sectors, {counts.Count} map object types and {prefabTokens.Count} placed prefab models.");
    }
}
