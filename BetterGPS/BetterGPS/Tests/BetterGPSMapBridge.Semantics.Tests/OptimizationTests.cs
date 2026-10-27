using BetterGPSNavigation;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
internal static class OptimizationTests
{
    internal static void Run(string output)
    {
        Directory.CreateDirectory(output);
        var edges=Enumerable.Range(0,8).Select(i=>new LiveEdge(i,$"{i:X16}",new(i*100,0,0),new((i+1)*100,0,0),i*100,(i+1)*100,i*1000,(i+1)*1000)).ToArray();
        LiveAction A(string id,int segment)=>new(id,segment,segment,segment*1000+500,"TURN_RIGHT",id+"-branch","HIGH",
            new LiveProof[]{new("action","TURN_RIGHT","HIGH","PPD","ppd",id+"-branch","exact"),new("road","I-5","HIGH","SIGN_BOARD","board",id+"-branch","typed"),new("cardinal","NORTH","HIGH","SIGN_BOARD","board",id+"-branch","typed"),new("destination","Sacramento","HIGH","SIGN_BOARD","board",id+"-branch","typed")}
            .Concat(Enumerable.Range(0,60).Select(i=>new LiveProof("lane","[]","HIGH","LANE_NETWORK","lane"+i,id+"-branch","{}"))).ToArray());
        var actions=new[]{A("first",1),A("next",5)};
        var spans=edges.Select(e=>new RoadIdentitySpan("span"+e.SegmentIndex,e.SegmentIndex,e.ItemUid, $"{e.SegmentIndex:X16}",$"{e.SegmentIndex+1:X16}",e.WorldStartM,e.WorldEndM,e.NavStartM,e.NavEndM,"RESOLVED",[new("I-5","RouteReference","NORTH","MEDIUM","MEDIUM",["proof"+e.SegmentIndex],"test")],[],"test")).ToArray();
        var contexts=actions.Select(a=>new ActionRoadContext(a.Id,a.BranchId,["span0"],["span"+(a.SegmentIndex+1)],[],null,"test")).ToArray();
        var route=new LiveRoute("benchmark",edges,actions){RoadIdentities=new(1,"map",edges.Select(e=>new RoadIdentityAnchor("proof"+e.SegmentIndex,"CONFIRMATION_SIGN","source"+e.SegmentIndex,"I-5 NORTH","I-5","RouteReference","NORTH","MEDIUM","VERIFIED_BRANCH","MEDIUM",e.ItemUid,e.SegmentIndex,e.WorldStartM,e.NavStartM,[])).ToArray(),spans,contexts)};
        var engine=new LiveGuidanceEngine();engine.Accept(new(2,"Hello","benchmark",0,0,0));engine.Accept(new(2,"RouteSnapshot","benchmark",1,0,1,route));
        LiveMessage Frame(int n)=>new(2,"VehicleState","benchmark",n+2,n*100,1,Vehicle:new(new(20+n%100*.01,0,0),0,10,false));
        using var hash=IncrementalHash.CreateHash(HashAlgorithmName.SHA256);long allocation=0,ticks=0;
        for(int i=0;i<4000;++i){var frame=Frame(i);long memory=GC.GetAllocatedBytesForCurrentThread(),start=Stopwatch.GetTimestamp();var guidance=engine.Accept(frame);ticks+=Stopwatch.GetTimestamp()-start;allocation+=GC.GetAllocatedBytesForCurrentThread()-memory;hash.AppendData(JsonSerializer.SerializeToUtf8Bytes(guidance,LiveProtocol.Json));}
        // Reuse action IDs across route replacements: neither resolved fields,
        // null conflicts nor branch validity may leak out of the old plan.
        var resetEngine=new LiveGuidanceEngine();resetEngine.Accept(new(2,"Hello","reset",0,0,0));long seq=1;
        LiveGuidance Replace(LiveRoute plan,long revision){resetEngine.Accept(new(2,"RouteSnapshot","reset",seq++,seq*100,revision,plan));return resetEngine.Accept(new(2,"VehicleState","reset",seq++,seq*100,revision,Vehicle:new(new(20,0,0),0,10,false)));}
        var plain=route with{RoadIdentities=null};
        if(Replace(plain,1).Road!="I-5")throw new Exception("Initial field cache fixture failed");
        var renamed=plain with{Actions=plain.Actions.Select(a=>a with{Proofs=a.Proofs.Select(p=>p.Field=="road"?p with{Value="US-101"}:p).ToArray()}).ToArray()};
        if(Replace(renamed,2).Road!="US-101")throw new Exception("Old route name leaked through field cache");
        var conflict=renamed with{Actions=renamed.Actions.Select(a=>a with{Proofs=a.Proofs.Append(new("road","I-80","HIGH","SIGN_BOARD","other",a.BranchId,"conflict")).ToArray()}).ToArray()};
        if(Replace(conflict,3).Road is not null)throw new Exception("Field cache bypassed conflict");
        if(Replace(plain,4).Road!="I-5")throw new Exception("Cached null survived route replacement");
        if(Replace(plain with{Actions=plain.Actions.Select(a=>a with{Confidence="LOW"}).ToArray()},5).Status!="UNCERTAIN_ACTION")throw new Exception("Action validity cache survived route replacement");
        var report=new{frames=4000,allocatedBytes=allocation,milliseconds=ticks*1000.0/Stopwatch.Frequency,sha256=Convert.ToHexString(hash.GetHashAndReset())};File.WriteAllText(Path.Combine(output,"navigation.json"),JsonSerializer.Serialize(report));Console.WriteLine(JsonSerializer.Serialize(report));
    }
}

