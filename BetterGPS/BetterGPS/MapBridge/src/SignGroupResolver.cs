using System.Globalization;
using System.Text.RegularExpressions;

namespace BetterGPSNavigation;

internal sealed record SignSemanticGroup(string Id,string Status,string Method,string Road,string Cardinal,
    string Destination,IReadOnlyList<uint> ObjectIds,IReadOnlyList<uint> ArrowObjectIds,
    IReadOnlyList<uint> SeparatorObjectIds,IReadOnlyList<string> Blockers)
{
    public string Kind {get;init;}="RouteReference";
}

/// <summary>Partitions source rectangles at explicit vertical dividers, never by GPS agreement.</summary>
internal static class SignGroupResolver
{
    private sealed record Rect(EffectiveSignObject Object,double Left,double Right,double Bottom,double Top);
    private static double N(string? s)=>double.TryParse(s,NumberStyles.Float,CultureInfo.InvariantCulture,out var n)?n:double.NaN;
    public static IReadOnlyList<SignSemanticGroup> Resolve(ExtractedSignPanel panel)
    {
        var visible=panel.Objects.Where(o=>o.Visible).ToArray();
        var rects=visible.Select(o=>new Rect(o,N(o.Effective.GetValueOrDefault("coords_l")),N(o.Effective.GetValueOrDefault("coords_r")),
            N(o.Effective.GetValueOrDefault("coords_b")),N(o.Effective.GetValueOrDefault("coords_t"))))
            .Where(r=>double.IsFinite(r.Left)&&double.IsFinite(r.Right)&&double.IsFinite(r.Bottom)&&double.IsFinite(r.Top)&&r.Right>r.Left&&r.Top>r.Bottom).ToArray();
        var separators=rects.Where(r=>r.Object.Kind=="sign_template_frame"&&r.Object.Reliable&&
            panel.WidthM is >0 && panel.HeightM is >0 && r.Right-r.Left<=panel.WidthM*.05&&
            r.Top-r.Bottom>=panel.HeightM*.8 && r.Left>0 && r.Right<panel.WidthM).OrderBy(r=>r.Left).ToArray();
        // A single shared shield between two vertically separated cardinal/arrow
        // rows is an explicit two-direction junction plate, not two guessed roads.
        if(separators.Length==0 && SharedShieldRows(panel,rects) is {} rows)return rows;
        if(separators.Length==0 && HorizontalRegions(panel,rects) is {} horizontal)return horizontal;
        var regions=new List<(double Left,double Right)>();double left=double.NegativeInfinity;
        foreach(var separator in separators){var centre=(separator.Left+separator.Right)/2;regions.Add((left,centre));left=centre;}
        regions.Add((left,double.PositiveInfinity));
        var result=new List<SignSemanticGroup>();
        for(var i=0;i<regions.Count;i++)
        {
            var region=regions[i];var selected=rects.Where(r=>r.Object.Kind!="sign_template_frame"&&r.Left>=region.Left-.001&&r.Right<=region.Right+.001).Select(r=>r.Object).ToArray();
            if(separators.Length==0)selected=visible.Where(o=>o.Kind!="sign_template_frame").ToArray();
            if(selected.Length==0)continue;
            var text=selected.Where(o=>o.Kind=="sign_template_text").Select(o=>SignRoadIdentityResolver.NormalizeText(o.Text)).ToArray();
            var roads=selected.Select(o=>o.Road).Where(x=>x.Length>0).Concat(text.SelectMany(SignTextClassifier.RoadReferences)).Distinct().ToArray();
            var kind="RouteReference";
            if(roads.Length==0&&selected.Any(o=>o.Arrow.Length>0))
            {
                roads=selected.Where(o=>o.Visible&&o.Reliable&&o.Kind=="sign_template_text")
                    .Select(o=>SignRoadIdentityResolver.NormalizeText(o.Text)).Where(IsSignedStreetName).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
                if(roads.Length>0)kind="StreetName";
            }
            var cardinals=text.SelectMany(SignTextClassifier.ExplicitCardinals).Distinct().ToArray();
            var destinations=text.Where(t=>!roads.Contains(t,StringComparer.OrdinalIgnoreCase)).Where(SignTextClassifier.IsDestinationCandidate).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
            // Another place-like line may be the prefix of a wrapped street name.
            // Do not announce the suffix line as a complete road identity.
            if(kind=="StreetName"&&destinations.Length>0)roads=[];
            var arrows=selected.Where(o=>o.Arrow.Length>0).ToArray();
            var blockers=new List<string>{"GROUP_LAYOUT_CANDIDATE_NOT_RENDER_VERIFIED"};
            if(selected.Any(o=>!o.Reliable||o.SchemaStatus!="SUPPORTED"))blockers.Add("GROUP_OBJECT_OR_SCHEMA_UNVERIFIED");
            if(selected.Any(o=>!rects.Any(r=>r.Object.Id==o.Id)))blockers.Add("OBJECT_RECT_MISSING");
            if(separators.Length>0 && rects.Any(r=>r.Object.Kind!="sign_template_frame"&&separators.Any(s=>r.Left<(s.Left+s.Right)/2-.001&&r.Right>(s.Left+s.Right)/2+.001)))blockers.Add("OBJECT_CROSSES_DIVIDER");
            if(roads.Length!=1)blockers.Add("ROAD_MISSING_OR_MULTIPLE");
            if(kind=="RouteReference"?cardinals.Length!=1:cardinals.Length>1)blockers.Add("CARDINAL_MISSING_OR_MULTIPLE");
            if(kind=="RouteReference"?destinations.Length!=1:destinations.Length>1)blockers.Add("DESTINATION_MISSING_OR_MULTIPLE");
            if(arrows.Length==0)blockers.Add("ARROW_MISSING");
            if(arrows.Select(a=>a.Arrow).Distinct().Count()>1)blockers.Add("ARROW_ROLES_CONFLICT");
            var complete=roads.Length==1&&(kind=="StreetName"?cardinals.Length<=1&&destinations.Length<=1:cardinals.Length==1&&destinations.Length==1)&&arrows.Length>0 &&
                !blockers.Contains("OBJECT_CROSSES_DIVIDER")&&!blockers.Contains("OBJECT_RECT_MISSING")&&arrows.Select(a=>a.Arrow).Distinct().Count()==1;
            result.Add(new($"{panel.SignUid}:{panel.AreaName}:group{i}",complete?"COMPLETE_CANDIDATE":"UNRESOLVED",
                separators.Length>0?"EXPLICIT_VERTICAL_DIVIDER":"SINGLE_REGION",roads.Length==1?roads[0]:"",cardinals.Length==1?cardinals[0]:"",
                destinations.Length==1?destinations[0]:"",selected.Select(o=>o.Id).ToArray(),arrows.Select(a=>a.Id).ToArray(),separators.Select(s=>s.Object.Id).ToArray(),blockers){Kind=kind});
        }
        return result;
    }
    private static SignSemanticGroup[]? SharedShieldRows(ExtractedSignPanel panel,Rect[] rects)
    {
        var objects=panel.Objects.Where(o=>o.Visible&&o.Kind!="sign_template_frame").ToArray();
        if(objects.Any(o=>!o.Reliable||o.SchemaStatus!="SUPPORTED"||!rects.Any(r=>r.Object.Id==o.Id)))return null;
        var shields=rects.Where(r=>r.Object.Road.Length>0).ToArray();
        var arrows=rects.Where(r=>r.Object.Arrow is "LEFT" or "RIGHT").ToArray();
        var cards=rects.Where(r=>SignTextClassifier.ExplicitCardinals(SignRoadIdentityResolver.NormalizeText(r.Object.Text)).Count()==1).ToArray();
        if(shields.Length!=1||arrows.Length!=2||cards.Length!=2||arrows.Select(r=>r.Object.Arrow).Distinct().Count()!=2||
            objects.Any(o=>o.Text.Length>0&&!cards.Any(r=>r.Object.Id==o.Id)))return null;
        double Y(Rect r)=>(r.Bottom+r.Top)/2;
        var sorted=cards.OrderBy(Y).ToArray();
        var names=sorted.Select(r=>SignTextClassifier.ExplicitCardinals(SignRoadIdentityResolver.NormalizeText(r.Object.Text)).Single()).ToArray();
        if(!((names[0]=="NORTH"&&names[1]=="SOUTH")||(names[0]=="SOUTH"&&names[1]=="NORTH")||
            (names[0]=="EAST"&&names[1]=="WEST")||(names[0]=="WEST"&&names[1]=="EAST")))return null;
        if(sorted[0].Top>=shields[0].Bottom||sorted[1].Bottom<=shields[0].Top)return null;
        var pairs=sorted.Select(c=>arrows.OrderBy(a=>Math.Abs(Y(a)-Y(c))).ToArray()).ToArray();
        if(pairs[0][0].Object.Id==pairs[1][0].Object.Id||pairs.Where((p,i)=>Math.Abs(Y(p[0])-Y(sorted[i]))>
            Math.Max(sorted[i].Top-sorted[i].Bottom,p[0].Top-p[0].Bottom)*.5).Any())return null;
        return sorted.Select((c,i)=>new SignSemanticGroup($"{panel.SignUid}:{panel.AreaName}:group{i}","COMPLETE_CANDIDATE",
            "SHARED_SHIELD_DIRECTION_ROWS",shields[0].Object.Road,names[i],"",
            [shields[0].Object.Id,c.Object.Id,pairs[i][0].Object.Id],[pairs[i][0].Object.Id],[],
            ["GROUP_LAYOUT_CANDIDATE_NOT_RENDER_VERIFIED"])).ToArray();
    }
    private static SignSemanticGroup[]? HorizontalRegions(ExtractedSignPanel panel,Rect[] rects)
    {
        var dividers=rects.Where(r=>r.Object.Kind=="sign_template_frame"&&r.Object.Reliable&&
            panel.WidthM is >0&&panel.HeightM is >0&&r.Top-r.Bottom<=panel.HeightM*.05&&
            r.Right-r.Left>=panel.WidthM*.8&&r.Bottom>0&&r.Top<panel.HeightM).OrderBy(r=>r.Bottom).ToArray();
        if(dividers.Length==0)return null;
        var objects=rects.Where(r=>r.Object.Kind!="sign_template_frame").ToArray();
        if(panel.Objects.Any(o=>o.Visible&&o.Kind!="sign_template_frame"&&!objects.Any(r=>r.Object.Id==o.Id))||
            objects.Any(r=>dividers.Any(d=>r.Bottom<(d.Bottom+d.Top)/2-.001&&r.Top>(d.Bottom+d.Top)/2+.001)))return null;
        var boundaries=new[]{double.NegativeInfinity}.Concat(dividers.Select(d=>(d.Bottom+d.Top)/2)).Append(double.PositiveInfinity).ToArray();
        var result=new List<SignSemanticGroup>();
        for(int i=0;i<boundaries.Length-1;i++)
        {
            var selected=objects.Where(r=>r.Bottom>=boundaries[i]-.001&&r.Top<=boundaries[i+1]+.001).Select(r=>r.Object).ToArray();
            if(selected.Length==0)return null;
            foreach(var group in Resolve(panel with{Objects=selected}))
                result.Add(group with{Id=$"{panel.SignUid}:{panel.AreaName}:row{i}",Method="EXPLICIT_HORIZONTAL_DIVIDER",SeparatorObjectIds=dividers.Select(d=>d.Object.Id).ToArray()});
        }
        return result.ToArray();
    }
    internal static bool IsSignedStreetName(string text)=>RoadIdentityRules.Street(text)&&Regex.IsMatch(text,
        @"(?:\S+\s+(?:STREET|ST|AVENUE|AVE|BOULEVARD|BLVD|ROAD|RD|DRIVE|DR|LANE|LN|WAY|COURT|CT|HIGHWAY|HWY|PARKWAY|PKWY|CIRCLE|CIR|PLACE|PL|TERRACE|TER|TRAIL|TRL)\.?$|^(?:AVENUE|AVE|ROAD|RD|STREET|ST|ROUTE)\s+\d+[A-Z]?$)",RegexOptions.IgnoreCase);
}
