using System.Globalization;
using Microsoft.VisualBasic.FileIO;

namespace BetterGPSNavigation;

public static class LiveCaptureCompiler
{
    internal static LiveRoute FromMemory(string routeId,RouteCorridor corridor,IReadOnlyList<V11DriverAction> actions,
        IReadOnlyList<V11BoardSemantic> boards,IReadOnlyList<V11BranchBoardMatch> matches)
    {
        var edges = corridor.Edges.Select(e => new LiveEdge(e.SegmentIndex,e.ItemUid.ToString("X16"),
            new(e.Start.X,e.Start.Y,e.Start.Z),new(e.End.X,e.End.Y,e.End.Z),
            e.StartStationM,e.EndStationM,e.StartNavStationM,e.EndNavStationM)).ToArray();
        var semantics = boards.ToDictionary(b => b.BoardIndex);
        var result = actions.Where(a => a.ActionRequired).Select(a =>
        {
            var proofs = new List<LiveProof> { new("action",a.Instruction,a.ActionConfidence,
                a.BranchConfidence == "EXACT" ? "PPD" : "UNRESOLVED",
                edges.First(e => e.SegmentIndex == a.SegmentIndex).ItemUid,a.SelectedBranchId,
                $"{a.ActionReason}; active lanes={a.ActiveLaneOrdinals}; input lanes={a.InputLaneCount}") };
            foreach (var b in matches.Where(b => b.EventIndex == a.EventIndex && b.SelectedBranchId == a.SelectedBranchId))
            {
                var semantic = semantics[b.BoardIndex];
                foreach (var (field,value,confidence) in new[] {("road",b.BranchIdentity,b.IdentityConfidence),
                    ("cardinal",b.Cardinal,b.CardinalConfidence),("destination",b.ValidatedToward,b.DestinationConfidence),
                    ("exit",b.ExitNumbers,b.ExitConfidence)})
                    if (!string.IsNullOrEmpty(value)) proofs.Add(new(field,value,b.Confidence == "HIGH" ? confidence : "LOW",
                        "SIGN_BOARD",b.SignUid.ToString("X16")+":"+b.AreaName,a.SelectedBranchId,
                        b.MatchReason+"; "+b.ValidationNotes+"; raw="+semantic.RawStrings+"; attributes="+semantic.SourceAttributes));
            }
            return new LiveAction($"{routeId}:{a.EventIndex}",a.EventIndex,a.SegmentIndex,a.NavStationM,
                a.Instruction,a.SelectedBranchId,a.ActionConfidence,proofs.ToArray());
        }).OrderBy(a => a.NavStationM).ToArray();
        var route = new LiveRoute(routeId,edges,result);
        LiveProtocol.Validate(new(1,"RouteSnapshot","compile",0,0,1,route));
        return route;
    }
    public static LiveRoute Compile(string directory, string stem)
    {
        if (Path.GetFileName(stem) != stem || stem.Contains("..")) throw new InvalidDataException("Invalid capture stem.");
        string PathFor(string suffix) => Path.Combine(directory, stem + suffix);
        // Prefer the complete versioned snapshot. Legacy CSVs remain readable without inventing missing identities.
        var snapshotPath=PathFor("_route.json");
        if(File.Exists(snapshotPath))
        {
            var snapshot=LiveProtocol.Decode(File.ReadAllBytes(snapshotPath));
            if(snapshot.Type!="RouteSnapshot"||snapshot.Route?.RouteId!=stem)throw new InvalidDataException("Snapshot route identity mismatch.");
            return snapshot.Route;
        }
        var regression = File.ReadAllLines(PathFor("_v11_regression.txt"));
        foreach (var guard in new[] { "geometry_derived_high_signed_cardinals", "destinations_without_high_branch_match",
            "straight_keep_events_when_all_input_lanes_reach_selected_branch", "high_matches_below_required_branch_margin", "semantic_field_validation_failures" })
        {
            var entry = regression.FirstOrDefault(l => l.StartsWith(guard + "=", StringComparison.Ordinal));
            if (entry != guard + "=0") throw new InvalidDataException($"Missing or failed guard: {guard}");
        }
        if (!regression.Contains("structural_snapshot_status=VALID") ||
            !regression.Contains("semantic_field_validation_failures=0"))
            throw new InvalidDataException("Capture is not a validated physical snapshot.");
        var edges = Read(PathFor("_route_corridor.csv")).Select(r => new LiveEdge(I(r,"segment_index"), r["item_uid"],
            new(D(r,"start_x"),D(r,"start_y"),D(r,"start_z")), new(D(r,"end_x"),D(r,"end_y"),D(r,"end_z")),
            D(r,"start_station_m"),D(r,"end_station_m"),D(r,"start_nav_station_m"),D(r,"end_nav_station_m"))).ToArray();
        var boards = Read(PathFor("_branch_board_matches_v11.csv"));
        var semantics = Read(PathFor("_board_semantics_v11.csv")).ToDictionary(b => b["board_index"]);
        var actions = Read(PathFor("_driver_actions_v11.csv")).Where(r => r["action_required"] == "1").Select(r =>
        {
            var branch = r["selected_branch_id"];
            var segment = I(r,"segment_index");
            var eventIndex = I(r,"event_index");
            var proofs = new List<LiveProof>();
            var item = edges.FirstOrDefault(e => e.SegmentIndex == segment)?.ItemUid ?? "";
            proofs.Add(new("action",r["instruction"],r["action_confidence"],
                r["branch_confidence"] == "EXACT" ? "PPD" : "UNRESOLVED", item,branch,
                $"{r["action_reason"]}; active lanes={r["active_lane_ordinals"]}; input lanes={r["input_lane_count"]}"));
            foreach (var b in boards.Where(b => I(b,"event_index") == eventIndex && b["selected_branch_id"] == branch))
            {
                foreach (var (field,valueColumn,confidenceColumn) in new[] {
                    ("road","branch_identity","identity_confidence"), ("cardinal","cardinal","cardinal_confidence"),
                    ("destination","validated_toward","destination_confidence"), ("exit","exit_numbers","exit_confidence") })
                {
                    if (b[valueColumn].Length == 0) continue;
                    var confidence = b["confidence"] == "HIGH" ? b[confidenceColumn] : "LOW";
                    proofs.Add(new(field,b[valueColumn],confidence,"SIGN_BOARD",
                        b["sign_uid"] + ":" + b["area_name"],branch,b["match_reason"] + "; " + b["validation_notes"] +
                        "; raw=" + semantics[b["board_index"]]["raw_strings"] +
                        "; attributes=" + semantics[b["board_index"]]["source_attributes"]));
                }
            }
            return new LiveAction($"{stem}:{eventIndex}",eventIndex,segment,D(r,"nav_station_m"),
                r["instruction"],branch,r["action_confidence"],proofs.ToArray());
        }).OrderBy(a => a.NavStationM).ToArray();
        var route = new LiveRoute(stem,edges,actions);
        LiveProtocol.Validate(new(1,"RouteSnapshot","compile",0,0,1,route));
        return route;
    }

    private static List<Dictionary<string,string>> Read(string path)
    {
        using var parser = new TextFieldParser(path);
        parser.TextFieldType = FieldType.Delimited; parser.SetDelimiters(",");
        parser.HasFieldsEnclosedInQuotes = true; parser.TrimWhiteSpace = false;
        var headers = parser.ReadFields() ?? throw new InvalidDataException("Missing CSV header.");
        var result = new List<Dictionary<string,string>>();
        while (!parser.EndOfData)
        {
            var fields = parser.ReadFields()!;
            if (fields.Length != headers.Length) throw new InvalidDataException("CSV column count mismatch.");
            result.Add(headers.Select((h,i) => (h,i)).ToDictionary(x => x.h,x => fields[x.i]));
        }
        return result;
    }
    private static double D(Dictionary<string,string> r,string key) => double.Parse(r[key],CultureInfo.InvariantCulture);
    private static int I(Dictionary<string,string> r,string key) => int.Parse(r[key],CultureInfo.InvariantCulture);
}
