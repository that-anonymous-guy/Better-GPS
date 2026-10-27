using System.Numerics;
using System.Text.Json;
using BetterGPSNavigation;

internal static class SignGroupingTests
{
    public static void Run()
    {
        int count=0;void Check(bool b,string m){count++;if(!b)throw new Exception(m);}
        var panels=JsonSerializer.Deserialize<ExtractedSignPanel[]>(File.ReadAllText("validation/reconstruction-v2/recompiled/live_000003_877056C700BF6AB2_panels.json"))!;
        var source=panels.Single(p=>p.SignUid=="552196EFE8180001"&&p.AreaName=="sb3_1") with{WidthM=9.6,HeightM=3};
        var groups=source.SemanticGroups;
        Check(groups.Count==2,"Recorded divided panel did not produce two groups");
        var north=groups.Single(g=>g.Road=="I-5");var east=groups.Single(g=>g.Road=="I-10");
        Check(north.Cardinal=="NORTH"&&north.Destination=="Sacramento"&&north.Status=="COMPLETE_CANDIDATE","Mixed I5 group");
        Check(east.Cardinal=="EAST"&&east.Destination=="San Bernardino"&&east.Status=="COMPLETE_CANDIDATE","Mixed I10 group");
        Check(north.ArrowObjectIds.SequenceEqual(new uint[]{3561619454})&&east.ArrowObjectIds.SequenceEqual(new uint[]{3469344766}),"Arrow crossed groups");
        Check(north.SeparatorObjectIds.SequenceEqual(new uint[]{4135845886}),"Divider evidence lost");
        var unsplit=source with{Objects=source.Objects.Where(o=>o.Kind!="sign_template_frame").ToArray()};
        Check(unsplit.SemanticGroups.Count==1&&unsplit.SemanticGroups[0].Status=="UNRESOLVED","Guessed split without divider");
        var crossing=source with {Objects=source.Objects.Select(o=>o.Id==3196846078?o with{Effective=new Dictionary<string,string>(o.Effective){["coords_r"]="8"}}:o).ToArray()};
        Check(crossing.SemanticGroups.All(g=>g.Status=="UNRESOLVED"&&g.Blockers.Contains("OBJECT_CROSSES_DIVIDER")),"Crossing text accepted");
        var missing=source with{Objects=source.Objects.Select(o=>o.Id==3766222846?o with{Visible=false}:o).ToArray()};
        Check(missing.SemanticGroups.Single(g=>g.Road=="I-5").Status=="UNRESOLVED","Hidden direction used");
        SignContinuityEvidence E(string id,double station,double lateral=0,double height=0,string cardinal="NORTH",float? px=null)=>
            new(id,"board",new(px??(float)station,0,0),station,station*20,lateral,height,[north with{Cardinal=cardinal}]);
        var origin=E("origin",0);var next=E("next",100);var later=E("later",300);
        var result=SignContinuityResolver.Resolve(origin,north,[next,later]);
        Check(result.Status=="REPEATED_AHEAD_CANDIDATE"&&result.IndependentSupportCount==2&&result.Confidence=="LOW","Two distinct following supports lost");
        Check(SignContinuityResolver.Resolve(origin,north,[next,next with{AreaName="another"},later]).IndependentSupportCount==2,"Same UID counted twice");
        Check(SignContinuityResolver.Resolve(origin,north,[next,later with{Position=next.Position}]).IndependentSupportCount==1,"Same physical location counted twice");
        Check(SignContinuityResolver.Resolve(origin,north,[E("back",-100),E("south",100,cardinal:"SOUTH"),E("far",3000),E("side",100,lateral:30),E("height",100,height:30)]).IndependentSupportCount==0,"Wrong direction/behind/off route supports accepted");
        Check(SignContinuityResolver.Resolve(origin,north,[E("origin",100)]).IndependentSupportCount==0,"Origin counted as its own support");
        Check(SignContinuityResolver.Resolve(origin,north with{Status="UNRESOLVED"},[next,later]).Status=="GROUP_UNRESOLVED","Unresolved group received continuity");
        Check(SignContinuityResolver.Resolve(origin with{WorldStationM=double.NaN},north,[next]).Status=="INVALID_STATION","Invalid station accepted");
        Check(result.Supports[0].WorldAheadM==100&&result.Supports[0].NavAheadM==2000,"World/nav units conflated");
        var commonOrigin=origin with{Groups=[north,east]};
        var commonResult=SignContinuityResolver.Resolve(commonOrigin,north,[next with{Groups=[north,east]},later with{Groups=[north,east]}]);
        Check(commonResult.Status=="REPEATED_BUT_NOT_DISCRIMINATING"&&commonResult.RepeatedCompetingGroupIds.SequenceEqual(new[]{east.Id}),"Repeated competitor falsely resolved destination");
        Check(SignContinuityResolver.Resolve(origin,north,[next with{Position=new(float.NaN,0,0)}]).IndependentSupportCount==0,"Non-finite sign position accepted");
        Console.WriteLine($"PASS: {count} grouping and lookahead assertions, including the recorded divided panel and independent evidence guards.");
    }
}
