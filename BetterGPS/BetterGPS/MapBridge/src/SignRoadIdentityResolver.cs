using System.Globalization;
using System.Numerics;
using System.Text.RegularExpressions;
using TruckLib;
using TruckLib.ScsMap;
using TruckLib.Sii;

namespace BetterGPSNavigation;

internal sealed record ParsedRoadIdentity(string Shield, string Cardinal)
{
    public string Display => string.Join(" ", new[] { Shield, Cardinal }.Where(s => !string.IsNullOrWhiteSpace(s)));
}

internal sealed record ParsedSignAttribute(
    uint ObjectId,
    uint AttributeIndex,
    string ValueType,
    string Value);

internal sealed record ParsedSignArea(
    string AreaName,
    IReadOnlyList<uint> OverrideIds,
    IReadOnlyList<ParsedSignAttribute> Attributes,
    string BoardToken,
    double BoardOffsetX,
    double BoardOffsetY,
    IReadOnlyList<string> RawStrings,
    IReadOnlyList<string> ExitNumbers,
    IReadOnlyList<string> RoadShields,
    IReadOnlyList<string> Cardinals,
    IReadOnlyList<string> Destinations,
    IReadOnlyList<string> ArrowTokens)
{
    public ExtractedSignPanel? Extraction { get; init; }
    public bool HasSemanticData => ExitNumbers.Count > 0 || RoadShields.Count > 0 || Cardinals.Count > 0 || Destinations.Count > 0 || ArrowTokens.Count > 0;
}

internal sealed record ParsedSign(
    ulong Uid,
    ulong NodeUid,
    Vector3 Position,
    Quaternion Rotation,
    double YawDeg,
    string Model,
    string Look,
    string Variant,
    string Template,
    string DefinitionName,
    string ModelDesc,
    IReadOnlyList<string> RawStrings,
    IReadOnlyList<string> ExitNumbers,
    IReadOnlyList<string> RoadShields,
    IReadOnlyList<string> Cardinals,
    IReadOnlyList<string> Destinations,
    IReadOnlyList<string> LegacyRoads,
    IReadOnlyList<string> LegacyCities,
    IReadOnlyList<ParsedSignArea> Areas,
    bool IsExitNavigationModel,
    bool IsExitSpeedModel,
    bool HasExplicitExitText)
{
    public bool FollowRoadDir { get; init; }
    public bool HasSemanticData => ExitNumbers.Count > 0 || RoadShields.Count > 0 || Cardinals.Count > 0 || Destinations.Count > 0 || LegacyRoads.Count > 0 || LegacyCities.Count > 0;

    public IReadOnlyList<ParsedRoadIdentity> RoadIdentities
    {
        get
        {
            var shields = RoadShields.Where(s => !string.IsNullOrWhiteSpace(s)).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            if (shields.Count == 0) return [];
            if (Cardinals.Count == 0) return shields.Select(s => new ParsedRoadIdentity(s, "")).ToList();
            var result = new List<ParsedRoadIdentity>();
            foreach (var shield in shields)
                foreach (var cardinal in Cardinals)
                    result.Add(new ParsedRoadIdentity(shield, cardinal));
            return result;
        }
    }
}

internal sealed record RouteSignCandidate(
    ParsedSign Sign,
    string Region,
    double DistanceToDecisionM,
    double AlongM,
    double LateralM,
    double OrientationPerpendicularErrorDeg,
    double Score);

/// <summary>
/// Structured sign semantic index. v0.8 adds strict exit-model filtering,
/// cardinal/atlas normalization, and projection against the selected route corridor.
/// </summary>
internal sealed class SignRoadIdentityResolver
{
    private const double CellSize = 500.0;
    private readonly List<ParsedSign> signs = [];
    private readonly Dictionary<(int X, int Z), List<ParsedSign>> grid = [];
    private readonly Dictionary<(int X, int Z), List<ParsedSign>> allGrid = [];
    private readonly Dictionary<string, SignDefinition> definitions = new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> knownDestinations = new(StringComparer.OrdinalIgnoreCase);

    private sealed record SignDefinition(string Name, string ModelDesc);

    private static readonly Regex AtlasRoadRegex = new(@"/def/sign/atlas/(?<name>[^/]+?)\.(?:mat|sii)$", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex AtlasNameRoadRegex = new(@"^(?<prefix>is|i|us|sr|ca|nv|az|or|wa|ut|id|mt|wy|co|nm|tx|ok|ks|ne|sd|nd|mn|ia|mo|ar|la|wi|il|mi|in|oh|ky|tn|ms|al|ga|fl|sc|nc|va|wv|pa|ny|vt|nh|me|ma|ri|ct|nj|de|md)[_-]?(?<num>\d+[a-z]?)", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex DirectRoadRegex = new(@"\b(?<prefix>I|US|SR|CA|NV|AZ|OR|WA|UT|ID|MT|WY|CO|NM|TX|OK|KS|NE|SD|ND|MN|IA|MO|AR|LA|WI|IL|MI|IN|OH|KY|TN|MS|AL|GA|FL|SC|NC|VA|WV|PA|NY|VT|NH|ME|MA|RI|CT|NJ|DE|MD)[-\s]?(?<num>\d+[A-Z]?)\b", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex StateRouteRegex = new(@"\bSTATE\s+(?:ROUTE|HWY|HIGHWAY)\s+(?<num>\d+[A-Z]?)\b", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex ExitRegex = new(@"\bEXIT(?:S)?\s+(?<num>\d+\s*[A-Z]?)\b", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex PureExitNumberRegex = new(@"^(?<num>\d+\s*[A-Z]?)$", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex CardinalRegex = new(@"^(NORTH|SOUTH|EAST|WEST)$", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex CardinalWordRegex = new(@"\b(NORTH|SOUTH|EAST|WEST)\b", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex HtmlTagRegex = new(@"<[^>]*>", RegexOptions.Compiled);
    private static readonly Regex WhitespaceRegex = new(@"\s+", RegexOptions.Compiled);

    public SignRoadIdentityResolver(Map map, IFileSystem fs)
    {
        var index=AssetIndexCache.GetShared(fs,"sign-definitions",()=>{
            LoadKnownDestinations(fs);LoadDefinitions(fs);
            return new DefinitionIndex(definitions,knownDestinations.ToArray());
        });
        foreach(var pair in index.Definitions)definitions[pair.Key]=pair.Value;
        knownDestinations.UnionWith(index.Destinations);
        foreach (var sign in EnumerateSigns(map.MapItems.Values).DistinctBy(s=>s.Uid))
        {
            if (sign.Node is null) continue;
            var parsed = Parse(sign);
            signs.Add(parsed);
            var key = Cell(parsed.Position.X, parsed.Position.Z);
            if (!allGrid.TryGetValue(key, out var allList)) allGrid[key] = allList = [];
            allList.Add(parsed);
            if (!parsed.HasSemanticData) continue;
            if (!grid.TryGetValue(key, out var list)) grid[key] = list = [];
            list.Add(parsed);
        }
    }

    public IReadOnlyList<ParsedSign> Signs => signs;
    private static IEnumerable<Sign> EnumerateSigns(IEnumerable<MapItem> items)
    {
        foreach(var item in items)
        {
            if(item is Sign sign)yield return sign;
            if(item is Compound compound)
                foreach(var child in EnumerateSigns(compound.MapItems.Values))yield return child;
        }
    }
    private sealed record DefinitionIndex(Dictionary<string,SignDefinition> Definitions,string[] Destinations);
    public int DefinitionCount => definitions.Count;
    public int SemanticSignCount => signs.Count(s => s.HasSemanticData);
    public int ExitSignCount => signs.Count(s => s.ExitNumbers.Count > 0);
    public int RoadIdentitySignCount => signs.Count(s => s.RoadIdentities.Count > 0);

    public void WriteGlobalIndex(string path)
    {
        using var writer = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        writer.WriteLine("sign_uid,node_uid,x,y,z,yaw_deg,model,look,variant,template,definition_name,model_desc,raw_strings,exit_numbers,road_shields,cardinals,destinations,legacy_roads,legacy_cities,road_identities,is_exit_navigation_model,is_exit_speed_model,has_explicit_exit_text");
        foreach (var s in signs.Where(s => s.HasSemanticData).OrderBy(s => s.Uid))
        {
            writer.WriteLine(string.Join(',',
                Hex(s.Uid), Hex(s.NodeUid), F(s.Position.X), F(s.Position.Y), F(s.Position.Z), F(s.YawDeg),
                Csv(s.Model), Csv(s.Look), Csv(s.Variant), Csv(s.Template), Csv(s.DefinitionName), Csv(s.ModelDesc),
                Csv(string.Join('|', s.RawStrings)), Csv(string.Join('|', s.ExitNumbers)),
                Csv(string.Join('|', s.RoadShields)), Csv(string.Join('|', s.Cardinals)),
                Csv(string.Join('|', s.Destinations)), Csv(string.Join('|', s.LegacyRoads)), Csv(string.Join('|', s.LegacyCities)),
                Csv(string.Join('|', s.RoadIdentities.Select(r => r.Display))),
                s.IsExitNavigationModel ? 1 : 0,
                s.IsExitSpeedModel ? 1 : 0,
                s.HasExplicitExitText ? 1 : 0));
        }
    }

    public void WriteAreaIndex(string path)
    {
        using var writer = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        writer.WriteLine("sign_uid,template,model,area_name,override_ids,attribute_count,board_token,board_offset_x,board_offset_y,raw_strings,exit_numbers,road_shields,cardinals,destinations,arrow_tokens");
        foreach (var s in signs.Where(s => s.Areas.Count > 0).OrderBy(s => s.Uid))
        {
            foreach (var a in s.Areas.OrderBy(a => a.AreaName, StringComparer.OrdinalIgnoreCase))
            {
                writer.WriteLine(string.Join(',',
                    Hex(s.Uid), Csv(s.Template), Csv(s.Model), Csv(a.AreaName), Csv(string.Join('|', a.OverrideIds)), a.Attributes.Count, Csv(a.BoardToken), F(a.BoardOffsetX), F(a.BoardOffsetY),
                    Csv(string.Join('|', a.RawStrings)), Csv(string.Join('|', a.ExitNumbers)), Csv(string.Join('|', a.RoadShields)), Csv(string.Join('|', a.Cardinals)),
                    Csv(string.Join('|', a.Destinations)), Csv(string.Join('|', a.ArrowTokens))));
            }
        }
    }


    public void WriteAreaAttributeIndex(string path)
    {
        using var writer = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        writer.WriteLine("sign_uid,template,model,area_name,object_id,attribute_index,value_type,value");
        foreach (var s in signs.Where(s => s.Areas.Count > 0).OrderBy(s => s.Uid))
        {
            foreach (var a in s.Areas.OrderBy(a => a.AreaName, StringComparer.OrdinalIgnoreCase))
            {
                foreach (var attr in a.Attributes.OrderBy(x => x.ObjectId).ThenBy(x => x.AttributeIndex))
                    writer.WriteLine(string.Join(',', Hex(s.Uid), Csv(s.Template), Csv(s.Model), Csv(a.AreaName), attr.ObjectId, attr.AttributeIndex, Csv(attr.ValueType), Csv(attr.Value)));
            }
        }
    }

    /// <summary>Legacy v0.7 spatial matcher retained for comparison reports.</summary>
    public IReadOnlyList<RouteSignCandidate> FindForDecision(
        Vector3 entry,
        Vector3 exit,
        Vector3? previous,
        Vector3? next,
        double searchRadiusM = 450.0)
    {
        var incoming = previous.HasValue ? NormalizeXZ(entry - previous.Value) : NormalizeXZ(exit - entry);
        if (incoming.LengthSquared() < 1e-8f) incoming = NormalizeXZ(exit - entry);
        var outgoing = next.HasValue ? NormalizeXZ(next.Value - exit) : NormalizeXZ(exit - entry);
        if (outgoing.LengthSquared() < 1e-8f) outgoing = NormalizeXZ(exit - entry);
        var midpoint = (entry + exit) * 0.5f;

        var found = new Dictionary<ulong, RouteSignCandidate>();
        foreach (var sign in Query(midpoint, searchRadiusM + Vector2.Distance(XZ(entry), XZ(exit)) * 0.5))
        {
            if (!sign.HasSemanticData) continue;
            var best = CandidateFor(sign, entry, exit, midpoint, incoming, outgoing);
            if (best is null) continue;
            if (!found.TryGetValue(sign.Uid, out var existing) || best.Score > existing.Score)
                found[sign.Uid] = best;
        }
        return found.Values.OrderByDescending(c => c.Score).ThenBy(c => c.DistanceToDecisionM).ToList();
    }

    /// <summary>
    /// v0.8 matcher: a sign must project onto the selected route corridor near the
    /// decision station. This rejects a large class of signs belonging to parallel
    /// ramps/interchanges that happened to be close to the prefab center.
    /// </summary>
    public IReadOnlyList<RouteSignCandidate> FindAlongCorridor(
        RouteCorridor corridor,
        double decisionStationM,
        Vector3 decisionCenter,
        double queryRadiusM = 900.0)
    {
        var result = new List<RouteSignCandidate>();
        foreach (var sign in Query(decisionCenter, queryRadiusM))
        {
            var projection = corridor.Project(sign.Position);
            if (projection is null) continue;
            var relative = projection.StationM - decisionStationM;
            if (relative < -650 || relative > 500 || projection.LateralM > 120) continue;

            var routeDir = HeadingVector(projection.HeadingDeg);
            var orientErr = PerpendicularOrientationError(sign.YawDeg, routeDir);
            var region = relative < -30 ? "APPROACH" : relative > 30 ? "AFTER" : "NEAR";
            var dist = Math.Sqrt(relative * relative + projection.LateralM * projection.LateralM);
            var score = SemanticBaseScore(sign)
                        + (region == "APPROACH" ? 70 : region == "AFTER" ? 60 : 65)
                        - Math.Abs(relative) * 0.09
                        - projection.LateralM * 0.32
                        - Math.Min(orientErr, 90) * 0.08;
            if (region == "AFTER" && sign.RoadIdentities.Count > 0) score += 15;
            if (sign.IsExitNavigationModel && sign.ExitNumbers.Count > 0) score += 15;
            if (sign.IsExitSpeedModel) score -= 80;
            result.Add(new RouteSignCandidate(sign, region, dist, relative, projection.LateralM, orientErr, score));
        }
        return result.OrderByDescending(c => c.Score).ThenBy(c => c.DistanceToDecisionM).ToList();
    }

    /// <summary>v0.11 variant that also considers signs whose strings are not yet semantically classified.
    /// Board definitions/AreaName can supply the missing context later in the pipeline.</summary>
    public IReadOnlyList<RouteSignCandidate> FindAllAlongCorridor(
        RouteCorridor corridor,
        double decisionStationM,
        Vector3 decisionCenter,
        double queryRadiusM = 900.0)
    {
        var result = new List<RouteSignCandidate>();
        foreach (var sign in QueryGrid(allGrid, decisionCenter, queryRadiusM))
        {
            if (sign.Areas.Count == 0 && (string.IsNullOrWhiteSpace(sign.Template) || sign.Template == "0")) continue;
            var projection = corridor.Project(sign.Position);
            if (projection is null) continue;
            var relative = projection.StationM - decisionStationM;
            if (relative < -650 || relative > 500 || projection.LateralM > 120) continue;

            var routeDir = HeadingVector(projection.HeadingDeg);
            var orientErr = PerpendicularOrientationError(sign.YawDeg, routeDir);
            var region = relative < -30 ? "APPROACH" : relative > 30 ? "AFTER" : "NEAR";
            var dist = Math.Sqrt(relative * relative + projection.LateralM * projection.LateralM);
            // Keep ranking mostly geometric here. Board semantics are resolved later, so
            // an unclassified-but-structured AreaName must not be starved by old text scoring.
            var score = 100
                        + (region == "APPROACH" ? 10 : region == "AFTER" ? 8 : 9)
                        - Math.Abs(relative) * 0.04
                        - projection.LateralM * 0.45
                        - Math.Min(orientErr, 90) * 0.10;
            result.Add(new RouteSignCandidate(sign, region, dist, relative, projection.LateralM, orientErr, score));
        }
        return result.OrderByDescending(c => c.Score).ThenBy(c => c.DistanceToDecisionM).ToList();
    }

    public IReadOnlyList<ParsedSign> Query(Vector3 center, double radiusM)
        => QueryGrid(grid, center, radiusM);
    public IReadOnlyList<ParsedSign> QueryAll(Vector3 center,double radiusM)=>QueryGrid(allGrid,center,radiusM);

    private static IReadOnlyList<ParsedSign> QueryGrid(
        IReadOnlyDictionary<(int X, int Z), List<ParsedSign>> source,
        Vector3 center,
        double radiusM)
    {
        var minX = (int)Math.Floor((center.X - radiusM) / CellSize);
        var maxX = (int)Math.Floor((center.X + radiusM) / CellSize);
        var minZ = (int)Math.Floor((center.Z - radiusM) / CellSize);
        var maxZ = (int)Math.Floor((center.Z + radiusM) / CellSize);
        var r2 = radiusM * radiusM;
        var result = new List<ParsedSign>();
        for (var x = minX; x <= maxX; x++)
        for (var z = minZ; z <= maxZ; z++)
        {
            if (!source.TryGetValue((x, z), out var cellSigns)) continue;
            foreach (var sign in cellSigns)
            {
                var dx = sign.Position.X - center.X;
                var dz = sign.Position.Z - center.Z;
                if (dx * dx + dz * dz <= r2) result.Add(sign);
            }
        }
        return result;
    }

    public static bool IsStrictExitCandidate(RouteSignCandidate candidate)
    {
        var s = candidate.Sign;
        if (s.IsExitSpeedModel || !s.IsExitNavigationModel || s.ExitNumbers.Count == 0) return false;
        if (candidate.AlongM < -500 || candidate.AlongM > 90) return false;
        if (candidate.LateralM > 80) return false;
        return true;
    }

    private static RouteSignCandidate? CandidateFor(
        ParsedSign sign,
        Vector3 entry,
        Vector3 exit,
        Vector3 midpoint,
        Vector2 incoming,
        Vector2 outgoing)
    {
        var p = XZ(sign.Position);
        var e0 = XZ(entry);
        var e1 = XZ(exit);
        var mid = XZ(midpoint);
        var signFromEntry = p - e0;
        var signFromExit = p - e1;
        RouteSignCandidate? best = null;

        if (incoming.LengthSquared() > 1e-8f)
        {
            var along = Vector2.Dot(signFromEntry, incoming);
            var upstream = -along;
            var lateral = Math.Abs(Cross(incoming, signFromEntry));
            if (upstream >= -35 && upstream <= 450 && lateral <= 160)
            {
                var dist = Math.Sqrt(upstream * upstream + lateral * lateral);
                var orientErr = PerpendicularOrientationError(sign.YawDeg, incoming);
                var score = SemanticBaseScore(sign) + 70 - dist * 0.12 - Math.Min(orientErr, 90) * 0.08;
                best = new RouteSignCandidate(sign, "APPROACH", dist, -upstream, lateral, orientErr, score);
            }
        }

        var nearDistance = Vector2.Distance(p, mid);
        if (nearDistance <= 200)
        {
            var dir = NormalizeXZ(exit - entry);
            var lateral = dir.LengthSquared() > 1e-8f ? Math.Abs(Cross(dir, p - mid)) : nearDistance;
            var orientErr = dir.LengthSquared() > 1e-8f ? PerpendicularOrientationError(sign.YawDeg, dir) : 90;
            var score = SemanticBaseScore(sign) + 45 - nearDistance * 0.10 - Math.Min(orientErr, 90) * 0.05;
            var c = new RouteSignCandidate(sign, "NEAR", nearDistance, 0, lateral, orientErr, score);
            if (best is null || c.Score > best.Score) best = c;
        }

        if (outgoing.LengthSquared() > 1e-8f)
        {
            var along = Vector2.Dot(signFromExit, outgoing);
            var lateral = Math.Abs(Cross(outgoing, signFromExit));
            if (along >= -35 && along <= 320 && lateral <= 140)
            {
                var dist = Math.Sqrt(along * along + lateral * lateral);
                var orientErr = PerpendicularOrientationError(sign.YawDeg, outgoing);
                var score = SemanticBaseScore(sign) + 55 - dist * 0.12 - Math.Min(orientErr, 90) * 0.08;
                if (sign.RoadIdentities.Count > 0) score += 20;
                var c = new RouteSignCandidate(sign, "AFTER", dist, along, lateral, orientErr, score);
                if (best is null || c.Score > best.Score) best = c;
            }
        }
        return best;
    }

    private static double SemanticBaseScore(ParsedSign sign)
    {
        var score = 0.0;
        if (sign.ExitNumbers.Count > 0) score += 55;
        if (sign.RoadIdentities.Count > 0) score += 45;
        if (sign.Destinations.Count > 0 || sign.LegacyCities.Count > 0) score += 25;
        return score;
    }

    private ParsedSign Parse(Sign sign)
    {
        var raw = new List<string>();
        var areaRaw = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
        var areaIds = new Dictionary<string, List<uint>>(StringComparer.OrdinalIgnoreCase);
        var areaAttributes = new Dictionary<string, List<ParsedSignAttribute>>(StringComparer.OrdinalIgnoreCase);

        foreach (var ov in sign.SignOverrides ?? [])
        {
            var areaName = ov.AreaName.ToString();
            if (string.IsNullOrWhiteSpace(areaName) || areaName == "0") areaName = "<unnamed>";
            if (!areaRaw.TryGetValue(areaName, out var list)) areaRaw[areaName] = list = [];
            if (!areaIds.TryGetValue(areaName, out var ids)) areaIds[areaName] = ids = [];
            if (!areaAttributes.TryGetValue(areaName, out var attrs)) areaAttributes[areaName] = attrs = [];
            ids.Add(ov.Id);

            foreach (var attr in ov.Attributes ?? [])
            {
                var value = attr.GetValue();
                var valueText = Convert.ToString(value, CultureInfo.InvariantCulture) ?? value?.ToString() ?? "";
                attrs.Add(new ParsedSignAttribute(ov.Id, attr.Index, value?.GetType().FullName ?? "null", valueText));
                if (value is string text && !string.IsNullOrWhiteSpace(text))
                {
                    var normalized = Normalize(text);
                    raw.Add(normalized);
                    list.Add(normalized);
                }
            }
        }

        var boardOverrides = new Dictionary<string, SignBoardOverride>(StringComparer.OrdinalIgnoreCase);
        foreach (var board in sign.BoardOverrides ?? [])
        {
            var areaName = board.AreaName.ToString();
            if (string.IsNullOrWhiteSpace(areaName) || areaName == "0") areaName = "<unnamed>";
            boardOverrides[areaName] = board;
        }

        var legacyRoads = new List<string>();
        var legacyCities = new List<string>();
        foreach (var board in sign.SignBoards ?? [])
        {
            AddToken(board.Road, legacyRoads);
            AddToken(board.City1, legacyCities);
            AddToken(board.City2, legacyCities);
        }

        raw = raw.Where(s => !string.IsNullOrWhiteSpace(s)).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        legacyRoads = legacyRoads.Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        legacyCities = legacyCities.Distinct(StringComparer.OrdinalIgnoreCase).ToList();

        definitions.TryGetValue(sign.Model.ToString(), out var definition);
        var definitionName = definition?.Name ?? "";
        var modelDesc = definition?.ModelDesc ?? "";
        var signNameHint = string.Join(" ", sign.SignTemplate ?? "", sign.Model.ToString(), sign.Variant.ToString(), definitionName, modelDesc).ToLowerInvariant();
        var isExitSpeedModel = signNameHint.Contains("exit mph", StringComparison.Ordinal) || signNameHint.Contains("exit_mph", StringComparison.Ordinal);
        var isNavigationModel = modelDesc.StartsWith("/model/sign/navigation", StringComparison.OrdinalIgnoreCase) || signNameHint.Contains("navigation", StringComparison.Ordinal);
        var isExitNavigationModel = !isExitSpeedModel && isNavigationModel &&
            (signNameHint.Contains("exit", StringComparison.Ordinal) || signNameHint.Contains("side board green", StringComparison.Ordinal));

        var whole = ClassifyStrings(raw, isExitNavigationModel, isExitSpeedModel);
        whole.Shields.AddRange(legacyRoads.Where(LooksLikeRoadToken).Select(NormalizeRoadToken));

        var allAreaNames = areaRaw.Keys.Concat(boardOverrides.Keys).Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
        var areas = new List<ParsedSignArea>();
        foreach (var areaName in allAreaNames)
        {
            var strings = areaRaw.TryGetValue(areaName, out var ar) ? ar.Where(x => !string.IsNullOrWhiteSpace(x)).Distinct(StringComparer.OrdinalIgnoreCase).ToList() : [];
            var sem = ClassifyStrings(strings, isExitNavigationModel, isExitSpeedModel);
            boardOverrides.TryGetValue(areaName, out var bo);
            var arrows = strings.Where(x => x.Contains("arrow", StringComparison.OrdinalIgnoreCase)).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            areas.Add(new ParsedSignArea(
                areaName,
                areaIds.TryGetValue(areaName, out var ids) ? ids.Distinct().ToList() : [],
                areaAttributes.TryGetValue(areaName, out var attrs) ? attrs.ToList() : [],
                bo?.Board.ToString() ?? "",
                bo?.Offset.X ?? 0,
                bo?.Offset.Y ?? 0,
                strings,
                sem.Exits.Distinct(StringComparer.OrdinalIgnoreCase).ToList(),
                sem.Shields.Distinct(StringComparer.OrdinalIgnoreCase).ToList(),
                sem.Cardinals.Distinct(StringComparer.OrdinalIgnoreCase).ToList(),
                sem.Destinations.Distinct(StringComparer.OrdinalIgnoreCase).ToList(),
                arrows));
        }

        return new ParsedSign(
            sign.Uid,
            sign.Node.Uid,
            sign.Node.Position,
            sign.Node.Rotation,
            QuaternionYawDeg(sign.Node.Rotation),
            sign.Model.ToString(),
            sign.Look.ToString(),
            sign.Variant.ToString(),
            sign.SignTemplate ?? "",
            definitionName,
            modelDesc,
            raw,
            whole.Exits.Distinct(StringComparer.OrdinalIgnoreCase).ToList(),
            whole.Shields.Distinct(StringComparer.OrdinalIgnoreCase).ToList(),
            whole.Cardinals.Distinct(StringComparer.OrdinalIgnoreCase).ToList(),
            whole.Destinations.Distinct(StringComparer.OrdinalIgnoreCase).ToList(),
            legacyRoads,
            legacyCities,
            areas,
            isExitNavigationModel,
            isExitSpeedModel,
            whole.HasExplicitExitText) { FollowRoadDir = sign.FollowRoadDir };
    }

    private sealed record ParsedTextSemantics(
        List<string> Exits,
        List<string> Shields,
        List<string> Cardinals,
        List<string> Destinations,
        bool HasExplicitExitText);

    private static ParsedTextSemantics ClassifyStrings(
        IEnumerable<string> source,
        bool isExitNavigationModel,
        bool isExitSpeedModel)
    {
        var exits = new List<string>();
        var shields = new List<string>();
        var cardinals = new List<string>();
        var destinations = new List<string>();
        var hasExplicitExitText = false;

        foreach (var text in source)
        {
            // Instruction phrases must be rejected before parsing state abbreviations:
            // "IN 2 LANES" is not Indiana route 2. Compass words in place names are
            // not signed route directions (e.g. North Las Vegas).
            if (SignTextClassifier.IsInstruction(text)) continue;
            cardinals.AddRange(SignTextClassifier.ExplicitCardinals(text));

            var atlas = ParseAtlasRoad(text);
            if (!string.IsNullOrWhiteSpace(atlas))
            {
                shields.Add(atlas);
                continue;
            }

            var cardinal = CardinalRegex.Match(text);
            if (cardinal.Success)
            {
                cardinals.Add(cardinal.Groups[1].Value.ToUpperInvariant());
                continue;
            }

            if (!isExitSpeedModel && !LooksLikeExitDistanceText(text))
            {
                var exit = ExitRegex.Match(text);
                if (exit.Success)
                {
                    exits.Add(NormalizeExit(exit.Groups["num"].Value));
                    hasExplicitExitText = true;
                    continue;
                }
            }

            var stateRoute = SignTextClassifier.IsRouteText(text) ? StateRouteRegex.Match(text) : Match.Empty;
            if (stateRoute.Success)
            {
                shields.Add("SR-" + stateRoute.Groups["num"].Value.ToUpperInvariant());
                continue;
            }

            var roadMatches = DirectRoadRegex.Matches(SignTextClassifier.IsRouteText(text) ? text : "");
            if (roadMatches.Count > 0)
            {
                foreach (Match rm in roadMatches) shields.Add(FormatRoad(rm.Groups["prefix"].Value, rm.Groups["num"].Value));
                continue;
            }

            var pureNumber = PureExitNumberRegex.Match(text);
            if (!isExitSpeedModel && pureNumber.Success && isExitNavigationModel)
            {
                exits.Add(NormalizeExit(pureNumber.Groups["num"].Value));
                continue;
            }

            if (LooksLikeDestination(text)) destinations.Add(text);
        }

        return new ParsedTextSemantics(exits, shields, cardinals, destinations, hasExplicitExitText);
    }

    private void LoadDefinitions(IFileSystem fs)
    {
        if (!fs.DirectoryExists("/def/world/")) return;
        foreach (var file in fs.GetFiles("/def/world/")
                     .Where(f => f.EndsWith(".sii", StringComparison.OrdinalIgnoreCase))
                     .Where(f => Path.GetFileName(f).StartsWith("sign", StringComparison.OrdinalIgnoreCase))
                     .OrderBy(f => f, StringComparer.OrdinalIgnoreCase))
        {
            SiiFile sii;
            try { sii = SiiDocumentCache.Open(file, fs); }
            catch { continue; }
            foreach (var unit in sii.Units)
            {
                if (!unit.Name.StartsWith("sign.", StringComparison.OrdinalIgnoreCase)) continue;
                var token = unit.Name.Split('.').LastOrDefault();
                if (string.IsNullOrWhiteSpace(token)) continue;
                unit.Attributes.TryGetValue("sign_name", out var rawName);
                unit.Attributes.TryGetValue("model_desc", out var rawModelDesc);
                var name = Convert.ToString(rawName, CultureInfo.InvariantCulture)?.Trim().Trim('"') ?? "";
                var modelDesc = Convert.ToString(rawModelDesc, CultureInfo.InvariantCulture)?.Trim().Trim('"') ?? "";
                definitions[token] = new SignDefinition(name, modelDesc);
            }
        }
    }

    internal static string NormalizeText(string text) => Normalize(text);

    public bool IsKnownDestination(string text) => knownDestinations.Contains(SignTextClassifier.Normalize(text));
    public int KnownDestinationCount => knownDestinations.Count;

    private void LoadKnownDestinations(IFileSystem fs)
    {
        if (!fs.DirectoryExists("/def/")) return;
        // City definitions are .sui fragments without a SiiNunit header. Parse the
        // city*.sii entry points so TruckLib resolves their includes, including DLCs.
        foreach (var file in fs.GetFiles("/def/").Where(f => f.EndsWith(".sii", StringComparison.OrdinalIgnoreCase) &&
                     (Path.GetFileName(f).Equals("city.sii", StringComparison.OrdinalIgnoreCase) ||
                      Path.GetFileName(f).StartsWith("city.", StringComparison.OrdinalIgnoreCase))))
        {
            try
            {
                foreach (var unit in SiiDocumentCache.Open(file, fs).Units.Where(u => u.Class == "city_data"))
                {
                    foreach (var key in new[] { "city_name", "city_name_localized" })
                    {
                        if (!unit.Attributes.TryGetValue(key, out var rawName)) continue;
                        var name = Normalize(Convert.ToString(rawName, CultureInfo.InvariantCulture)?.Trim('"') ?? "");
                        if (SignTextClassifier.IsDestinationCandidate(name)) knownDestinations.Add(name);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[WARN] Destination definition '{file}' could not be read: {ex.Message}");
            }
        }
    }

    private static string Normalize(string text)
    {
        text = text.Replace("<br>", " ", StringComparison.OrdinalIgnoreCase);
        text = HtmlTagRegex.Replace(text, " ");
        text = WhitespaceRegex.Replace(text, " ").Trim();
        text = Regex.Replace(text, @"\bN\s+ORTH\b", "NORTH", RegexOptions.IgnoreCase);
        text = Regex.Replace(text, @"\bS\s+OUTH\b", "SOUTH", RegexOptions.IgnoreCase);
        text = Regex.Replace(text, @"\bE\s+AST\b", "EAST", RegexOptions.IgnoreCase);
        text = Regex.Replace(text, @"\bW\s+EST\b", "WEST", RegexOptions.IgnoreCase);
        return WhitespaceRegex.Replace(text, " ").Trim();
    }

    private static string ParseAtlasRoad(string text)
    {
        var match = AtlasRoadRegex.Match(text);
        if (!match.Success) return "";
        var name = match.Groups["name"].Value;
        var road = AtlasNameRoadRegex.Match(name);
        if (!road.Success) return "";
        return FormatRoad(road.Groups["prefix"].Value, road.Groups["num"].Value);
    }

    private static string FormatRoad(string prefix, string number)
    {
        prefix = prefix.Trim().ToUpperInvariant();
        if (prefix == "IS") prefix = "I";
        number = number.Trim().ToUpperInvariant().Replace(" ", "");
        return $"{prefix}-{number}";
    }

    private static string NormalizeExit(string value) => WhitespaceRegex.Replace(value.ToUpperInvariant(), "").Trim();

    private static bool LooksLikeExitDistanceText(string text)
        => text.Contains("MILE", StringComparison.OrdinalIgnoreCase) ||
           text.Contains("MPH", StringComparison.OrdinalIgnoreCase) ||
           Regex.IsMatch(text, @"\b\d+\s*FT\b", RegexOptions.IgnoreCase);

    private static bool LooksLikeDestination(string text)
    {
        return SignTextClassifier.IsDestinationCandidate(text);
    }

    private static void AddToken(object? token, List<string> list)
    {
        if (token is null) return;
        var s = token.ToString() ?? "";
        if (!string.IsNullOrWhiteSpace(s) && s != "0") list.Add(s);
    }

    private static bool LooksLikeRoadToken(string text) => DirectRoadRegex.IsMatch(text) || Regex.IsMatch(text, @"^[a-zA-Z]{1,3}_?\d+[a-zA-Z]?$", RegexOptions.CultureInvariant);

    private static string NormalizeRoadToken(string text)
    {
        var direct = DirectRoadRegex.Match(text.Replace('_', '-'));
        if (direct.Success) return FormatRoad(direct.Groups["prefix"].Value, direct.Groups["num"].Value);
        var parts = text.Split(new[] { '_', '-' }, StringSplitOptions.RemoveEmptyEntries);
        return parts.Length >= 2 ? FormatRoad(parts[0], string.Join('-', parts.Skip(1))) : text.ToUpperInvariant();
    }

    private static (int X, int Z) Cell(float x, float z) => ((int)Math.Floor(x / CellSize), (int)Math.Floor(z / CellSize));
    private static Vector2 XZ(Vector3 p) => new(p.X, p.Z);
    private static Vector2 NormalizeXZ(Vector3 p)
    {
        var v = new Vector2(p.X, p.Z);
        return v.LengthSquared() < 1e-8f ? Vector2.Zero : Vector2.Normalize(v);
    }
    private static Vector2 HeadingVector(double headingDeg)
    {
        var rad = headingDeg * Math.PI / 180.0;
        return new Vector2((float)Math.Sin(rad), (float)Math.Cos(rad));
    }
    private static double Cross(Vector2 a, Vector2 b) => a.X * b.Y - a.Y * b.X;

    private static double QuaternionYawDeg(Quaternion q)
    {
        var forward = Vector3.Transform(Vector3.UnitZ, q);
        if (Math.Abs(forward.X) < 1e-8f && Math.Abs(forward.Z) < 1e-8f) return 0;
        return Math.Atan2(forward.X, forward.Z) * 180.0 / Math.PI;
    }

    private static double PerpendicularOrientationError(double signYawDeg, Vector2 routeDir)
    {
        if (routeDir.LengthSquared() < 1e-8f) return 90;
        var routeDeg = Math.Atan2(routeDir.X, routeDir.Y) * 180.0 / Math.PI;
        var delta = Math.Abs(NormalizeDeg(signYawDeg - routeDeg));
        delta = Math.Min(delta, 180 - delta);
        return Math.Abs(90 - delta);
    }

    private static double NormalizeDeg(double deg)
    {
        deg %= 360;
        if (deg <= -180) deg += 360;
        if (deg > 180) deg -= 360;
        return deg;
    }

    private static string Hex(ulong value) => value.ToString("X16", CultureInfo.InvariantCulture);
    private static string F(double value) => value.ToString("0.######", CultureInfo.InvariantCulture);
    private static string Csv(string? value)
    {
        value ??= "";
        if (!value.Contains(',') && !value.Contains('"') && !value.Contains('\n') && !value.Contains('\r')) return value;
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}

