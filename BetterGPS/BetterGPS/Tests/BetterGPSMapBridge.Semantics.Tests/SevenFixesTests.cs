using System.Text.Json;
using BetterGPSNavigation;

internal static class SevenFixesTests
{
    internal static void Run(string directory)
    {
        int count=0;void Check(bool ok,string why){count++;if(!ok)throw new Exception(why);}
        var route=JsonSerializer.Deserialize<LiveMessage>(File.ReadAllBytes(Directory.GetFiles(directory,"*_route.json").Single(p=>!p.Contains("_head_"))),LiveProtocol.Json)!.Route!;
        route=RouteEvidence.Unpack(route);
        var spans=route.RoadIdentities!.Spans;
        Check(spans.Where(s=>s.SegmentIndex is >=14 and <=28).All(s=>s.Status=="RESOLVED"&&s.Identities.Any(i=>i.Identity=="CA-99")&&s.Identities.All(i=>i.Identity!="CA-120")),"CA-120 mention still hides CA-99");
        Check(spans.Where(s=>s.SegmentIndex<14).All(s=>s.Identities.All(i=>i.Identity!="CA-99")),"Highway reference leaked onto city approach");
        Check(spans.Where(s=>s.SegmentIndex is 59 or 60).All(s=>s.Identities.All(i=>i.Identity!="I-5")),"I-5 still names the exit ramp");
        Check(route.Actions.Where(a=>a.SegmentIndex is 59 or 68).All(a=>a.Instruction=="EXIT_RIGHT"&&a.Confidence=="HIGH"),"Numberless exits lost their instruction");
        Check(route.Actions.Where(a=>a.SegmentIndex is 6 or 100).All(a=>a.Instruction=="TURN_RIGHT"),"Right street turns changed");
        Check(route.Actions.Where(a=>a.SegmentIndex is 65 or 91).All(a=>a.Instruction=="KEEP_RIGHT"),"Lane positioning became a turn or exit");
        var guides=File.ReadLines(Path.Combine(directory,"guidance.jsonl")).Select(l=>JsonSerializer.Deserialize<LiveGuidance>(l,LiveProtocol.Json)!).ToArray();
        Check(guides.Any(g=>g.Instruction=="MERGE_LEFT"&&g.TargetRoad?.Labels.Any(l=>l.Identity=="CA-99")==true),"First merge has no CA-99 target");
        Check(guides.Any(g=>g.ContinueToward?.Labels.Any(l=>l.Identity=="CA-120")==true&&g.JoinedRoad?.Labels.Any(l=>l.Identity=="I-5")==true),"Current CA-120 connection and future I-5 were conflated");
        Check(guides.Any(g=>g.ActionId?.EndsWith(":11")==true&&NativeBridgeCli.Describe(g).Contains("toward Navy Dr")),"Last turn still has no Navy Dr destination");
        Check(guides.All(g=>!(g.TimestampMs is >=292703 and <=293703&&g.Status=="COMPLETE")),"Arrival still announced 1.36 km early");
        Check(guides.Any(g=>g.TimestampMs>=292703&&g.Instruction=="CONTINUE"&&
            (g.CurrentRoad?.Labels.Any(l=>l.Identity=="Navy Dr")==true||g.ContinueToward?.Labels.Any(l=>l.Identity=="Navy Dr")==true)),"Last street destination lost after final turn");
        long bytes=new FileInfo(Path.Combine(directory,"guidance.jsonl")).Length+new FileInfo(Path.Combine(directory,"guidance-proofs.jsonl")).Length;
        Check(bytes<5_000_000,"Guidance and retained proof logs exceed the 5 MB recorded-drive budget");
        var packets=File.ReadLines(Path.Combine(directory,"guidance-proofs.jsonl")).Select(l=>JsonDocument.Parse(l).RootElement.GetProperty("id").GetString()).ToArray();
        Check(packets.Length==packets.Distinct().Count()&&guides.All(g=>g.ProofReference is null||packets.Contains(g.ProofReference)),"Proof references are not recoverable");
        RoadIdentityValidation.Validate(route,route.RoadIdentities);
        Check(true,"Corrected route proofs and destination chains validate");
        // Independent arrival criterion on a straight route with no maneuver at all.
        var straight=new LiveRoute("straight",[new(0,"0000000000000001",new(0,0,0),new(100,0,0),0,100,0,2000)],[]);
        LiveGuidance At(double x,double remaining)
        {
            var e=new LiveGuidanceEngine();e.Accept(new(2,"Hello","test",0,0,0));e.Accept(new(2,"RouteSnapshot","test",1,0,1,straight));
            return e.Accept(new(2,"VehicleState","test",2,100,1,Vehicle:new(new(x,0,0),0,10,false,remaining)));
        }
        Check(At(5,1900).Instruction=="CONTINUE","No-action route reports arrival at its beginning");
        Check(At(100,500).Status!="COMPLETE","Native final approach ignored");
        Check(At(100,0).Status=="COMPLETE","Actual destination never completes");
        Console.WriteLine($"PASS: {count} independent recorded-drive and arrival acceptance assertions; logs={bytes} bytes.");
    }
}
