using System.Globalization;
using System.Numerics;
using Microsoft.VisualBasic.FileIO;
using BetterGPSNavigation;

internal static class PanelExtractionTests
{
    private static IEnumerable<Dictionary<string,string>> Csv(string path)
    {
        using var parser=new TextFieldParser(path);
        parser.TextFieldType=FieldType.Delimited;parser.SetDelimiters(",");parser.HasFieldsEnclosedInQuotes=true;parser.TrimWhiteSpace=false;
        var header=parser.ReadFields()!;
        while(!parser.EndOfData){var fields=parser.ReadFields()!;yield return header.Select((h,i)=>(h,i)).ToDictionary(x=>x.h,x=>fields[x.i]);}
    }
    public static void Run(string game,string docs,string indexes,string selection,string output)
    {
        Directory.CreateDirectory(output);
        var selected=Csv(selection).ToDictionary(r=>r["sign_uid"]);
        var attributes=Csv(Path.Combine(indexes,"SIGN_AREA_ATTRIBUTES.csv")).Where(r=>selected.ContainsKey(r["sign_uid"]))
            .GroupBy(r=>r["sign_uid"]+":"+r["area_name"]).ToDictionary(g=>g.Key,g=>g.Select(r=>new ParsedSignAttribute(
                uint.Parse(r["object_id"]),uint.Parse(r["attribute_index"]),r["value_type"],r["value"])).ToArray());
        var areas=Csv(Path.Combine(indexes,"SIGN_AREA_INDEX.csv")).Where(r=>selected.ContainsKey(r["sign_uid"]))
            .GroupBy(r=>r["sign_uid"]).ToDictionary(g=>g.Key,g=>g.Select(r=>new ParsedSignArea(r["area_name"],[],
                attributes.GetValueOrDefault(g.Key+":"+r["area_name"],[]),r["board_token"],double.Parse(r["board_offset_x"],CultureInfo.InvariantCulture),double.Parse(r["board_offset_y"],CultureInfo.InvariantCulture),[],[],[],[],[],[])).ToArray());
        var signs=selected.Values.Select(r=>new ParsedSign(ulong.Parse(r["sign_uid"],NumberStyles.HexNumber),0,
            new(float.Parse(r["x"],CultureInfo.InvariantCulture),float.Parse(r["y"],CultureInfo.InvariantCulture),float.Parse(r["z"],CultureInfo.InvariantCulture)),
            Quaternion.Identity, double.Parse(r["yaw_deg"],CultureInfo.InvariantCulture),r["model"],r["look"],r["variant"],r["template"],r["definition_name"],r["model_desc"],[],[],[],[],[],[],[],areas.GetValueOrDefault(r["sign_uid"],[]),false,false,false)).ToArray();
        using var assets=GameAssets.Open(new Options{GameRoot=game,DocumentsRoot=docs,ProbePath=".",OutputRoot=output});
        var templates=new SignTemplateResolver(assets.Loader,signs);var boards=new SignBoardResolver(assets.Loader,templates,signs);
        var extractor=new SignPanelExtractor(assets.Loader,templates,boards);
        extractor.BeginRoute();var result=signs.SelectMany(extractor.Areas).ToArray();
        extractor.WriteRoute(Path.Combine(output,"panels.json"));
        File.WriteAllLines(Path.Combine(output,"asset_sources.txt"),assets.Sources);
        var count=0;
        void Check(bool condition,string message){count++;if(!condition)throw new Exception(message);}
        var primary=signs.Single(s=>s.Uid==0x55CA962B55980000UL);
        var reconstructed=extractor.Areas(primary).Single(a=>a.AreaName=="board1");
        Check(reconstructed.RoadShields.SequenceEqual(new[]{"I-10"}),"Atlas index 10 was not decoded as I-10");
        Check(reconstructed.Cardinals.SequenceEqual(new[]{"EAST"}),"Text override EAST was lost");
        Check(reconstructed.ArrowTokens.SequenceEqual(new[]{"LEFT"}),"Static mirrored arrow was lost");
        Check(reconstructed.Extraction!.Objects.Single(o=>o.Id==2379350015).Visible==false,"Hidden US-101 supplied visible evidence");
        Check(reconstructed.Extraction.Objects.Single(o=>o.Id==428212224).Overrides.Count==0,"Static arrow unexpectedly relied on an override");
        Check(reconstructed.Extraction.Anchor.Status=="ACTIVE_MODEL_LOCATOR_UNCALIBRATED","Expected unique active-variant model anchor");
        var validation=SignSemanticValidator.Validate(reconstructed,boards.Find(reconstructed.BoardToken),"I-10","EAST",_=>false);
        Check(validation.IdentityConfidence=="HIGH" && validation.CardinalConfidence=="HIGH","Effective atlas/text field evidence not recognized");
        var baseArea=primary.Areas.Single(a=>a.AreaName=="board1");
        ParsedSign Mutate(uint index,string type,string value,ulong uid)=>primary with {Uid=uid,Areas=[baseArea with{
            Attributes=baseArea.Attributes.Where(a=>a.ObjectId!=3158704126 || a.AttributeIndex!=index)
                .Append(new(3158704126,index,type,value)).ToArray()}]};
        var badIndex=extractor.Areas(Mutate(1,"System.UInt32","99999",primary.Uid+1)).Single(a=>a.AreaName=="board1");
        Check(badIndex.Extraction!.Objects.Any(o=>o.Issues.Contains("ATLAS_INDEX_INVALID")),"Invalid image index did not fail explicitly");
        Check(!badIndex.RoadShields.Contains("I-10"),"Invalid atlas supplied road evidence");
        var unknown=extractor.Areas(Mutate(99,"System.Single","1",primary.Uid+2)).Single(a=>a.AreaName=="board1");
        var unknownValidation=SignSemanticValidator.Validate(unknown,boards.Find(unknown.BoardToken),"I-10","EAST",_=>true);
        Check(unknownValidation.IdentityConfidence!="HIGH","Unknown attribute was promoted HIGH");
        var wrongType=extractor.Areas(Mutate(1,"System.String","10",primary.Uid+3)).Single(a=>a.AreaName=="board1");
        Check(wrongType.Extraction!.Objects.Any(o=>o.Issues.Any(i=>i.StartsWith("ATTRIBUTE_TYPE_MISMATCH"))),"Wrong typed attribute accepted");
        Check(SignPanelExtractor.DecodeRoad("interstate 10")=="I-10","Route family normalization failed");
        Check(SignPanelExtractor.DecodeRoad("us 101 ca")=="US-101","US route normalized to state route");
        Check(SignPanelExtractor.DecodeRoad("us 101 ca guide 2")=="US-101","Guide atlas suffix lost US-101");
        Check(SignPanelExtractor.DecodeRoad("ca 58")=="CA-58","CA route normalization failed");
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/type_b_90.sii","unreviewed",0,false,false)=="","Unknown atlas hash supplied arrow");
        // Candidate decoding must not be upgraded into a route ownership proof.
        Check(reconstructed.Extraction.Anchor.Status!="MODEL_LOCATOR","Uncalibrated anchor exposed as branch proof");
        var defaults=extractor.Areas(primary with {Uid=primary.Uid+4,Areas=[]}).Single(a=>a.AreaName=="board1");
        Check(defaults.Cardinals.SequenceEqual(new[]{"NORTH"}),"Entirely static board was not reconstructed");
        Check(defaults.ArrowTokens.SequenceEqual(new[]{"LEFT"}),"Static board arrow was not reconstructed");
        var twoAreas=extractor.Areas(primary with {Uid=primary.Uid+5,Areas=[baseArea,baseArea with {
            AreaName="copy",BoardToken=reconstructed.BoardToken,Attributes=baseArea.Attributes
                .Where(a=>a.ObjectId!=3956408318 || a.AttributeIndex!=0).Append(new(3956408318,0,"System.String","WEST")).ToArray()}]});
        Check(twoAreas.Single(a=>a.AreaName=="board1").Cardinals.SequenceEqual(new[]{"EAST"}) &&
            twoAreas.Single(a=>a.AreaName=="copy").Cardinals.SequenceEqual(new[]{"WEST"}),"Reused ObjectId leaked between two areas");
        var rightHash="42FFD62D45D53D161963AEBAB973460637B38B340256B950E0217F733F938332";
        var upHash="881BB391AEE0C7A1B045E3A3224BE668B0BA4694E2ABECF6BD847FC14A24D49D";
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/type_b_90.sii",rightHash,0,false,false)=="RIGHT" &&
            SignPanelExtractor.DecodeArrow("/def/sign/atlas/type_b_90.sii",rightHash,0,true,false)=="LEFT","Horizontal mirror did not invert arrow");
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/type_b.sii",upHash,0,true,false)=="UP" &&
            SignPanelExtractor.DecodeArrow("/def/sign/atlas/type_b.sii",upHash,0,false,true)=="DOWN","Vertical/horizontal arrow transformations failed");
        var overhead=signs.Single(s=>s.Uid==0x552196EFE8180001UL);
        var overheadArea=extractor.Areas(overhead).Single(a=>a.AreaName=="sb1_1");
        Check(overheadArea.Cardinals.SequenceEqual(new[]{"SOUTH"}),"Overhead visibility reconstruction retained WEST or lost SOUTH");
        Check(!overheadArea.Extraction!.Objects.Single(o=>o.Id==1474428927).Visible &&
            overheadArea.Extraction.Objects.Single(o=>o.Id==1300234240).Visible,"Opacity overrides did not switch text objects");
        Check(overheadArea.ArrowTokens.SequenceEqual(new[]{"DOWN_LANE"}),"Lane arrows were lost or interpreted as turns");
        Check(overheadArea.Extraction.GroupStatus=="SINGLE_GROUP_CANDIDATE","Two down arrows invented separate branches");
        Check(!overheadArea.Extraction.SchemaValidated,"Inferred text override schema presented as validated");
        var overheadValidation=SignSemanticValidator.Validate(overheadArea,boards.Find(overheadArea.BoardToken),"I-5","SOUTH",_=>true);
        Check(overheadValidation.IdentityConfidence!="HIGH" && overheadValidation.CardinalConfidence!="HIGH","Inferred visibility supplied HIGH evidence");
        Check(overheadValidation.Notes.Contains("destination:San Diego=KNOWN_CITY"),"Known city was mislabeled after a reliability downgrade");
        Check(overheadArea.Extraction.Objects.Where(o=>o.Arrow.Length>0).All(o=>o.ArrowStatus=="CANDIDATE_TEXTURE_NOT_VERIFIED"),"Arrow atlas identification became a texture validation claim");
        var shield=extractor.Areas(signs.Single(s=>s.Uid==0x55CA985DB3980001UL)).Single(a=>a.AreaName=="ml6");
        Check(shield.RoadShields.SequenceEqual(new[]{"I-10"}),"Text interstate shield number not reconstructed");
        Check(shield.Destinations.Count==0 && shield.Extraction!.GroupStatus=="SINGLE_GROUP_CANDIDATE","Shield furniture became destinations or invented groups");
        var downHash="956CB587FFDE6FB0A23CE578736339F19E947B47379DC06EB177580C5CED0686";
        var diagonalHash="2819D4362A0BD871B8BE747D1624A0BB1F34046572A1A9F42597A5EF5E9381F5";
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/down_arrow.sii",downHash,0,true,false)=="DOWN_LANE" &&
            SignPanelExtractor.DecodeArrow("/def/sign/atlas/down_arrow.sii",downHash,0,false,true)=="UP_LANE","Lane arrow flips incorrect");
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/type_b_45.sii",diagonalHash,0,false,false)=="DIAGONAL_UP_RIGHT" &&
            SignPanelExtractor.DecodeArrow("/def/sign/atlas/type_b_45.sii",diagonalHash,0,true,true)=="DIAGONAL_DOWN_LEFT","Diagonal arrow flips incorrect");
        Check(SignPanelExtractor.DecodeArrow("/def/sign/atlas/down_arrow.sii","modified",0,false,false)=="","Modified atlas supplied a lane arrow");
        var invalidFlip=extractor.Areas(Mutate(4,"System.Single","2",primary.Uid+6)).Single(a=>a.AreaName=="board1");
        Check(invalidFlip.Extraction!.Objects.Any(o=>o.Issues.Contains("INVALID_BOOLEAN:hflip")),"Non-boolean flip accepted");
        var byteFlip=extractor.Areas(Mutate(4,"System.SByte","1",primary.Uid+7)).Single(a=>a.AreaName=="board1");
        Check(byteFlip.Extraction!.Objects.Single(o=>o.Id==3158704126).Reliable,"Recorded signed-byte boolean was rejected");
        File.WriteAllText(Path.Combine(output,"validation.txt"),$"PASS: {count} extraction assertions; {signs.Length} selected signs; {result.Length} reconstructed areas.\n");
        Console.WriteLine(File.ReadAllText(Path.Combine(output,"validation.txt")));
    }
}
