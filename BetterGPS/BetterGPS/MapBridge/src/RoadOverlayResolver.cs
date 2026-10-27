using System.Globalization;
using System.Numerics;
using System.Text.RegularExpressions;
using TruckLib.ScsMap;

namespace BetterGPSNavigation;

internal sealed record ParsedRoadOverlay(
    ulong Uid,
    ulong NodeUid,
    Vector3 Position,
    string RawToken,
    string Identity);

internal sealed record RoadOverlayMatch(
    ParsedRoadOverlay Overlay,
    RouteProjection Projection,
    string Confidence);

/// <summary>
/// v0.8 road identity source. ATS MapOverlay items of type RoadName are the
/// native road-number labels placed on the UI/GPS map. We project those labels
/// onto the selected route corridor instead of treating nearby navigation signs
/// as the primary source of the road number.
/// </summary>
internal sealed class RoadOverlayResolver
{
    private readonly List<ParsedRoadOverlay> overlays = [];
    private readonly Lazy<RoadNetworkIndex> network;

    private static readonly Regex RoadTokenRegex = new(
        @"(?:^|[_\-])(?<prefix>is|interstate|i|us|sr|ca|nv|az|or|wa|ut|id|mt|wy|co|nm|tx|ok|ks|ne|sd|nd|mn|ia|mo|ar|la|wi|il|mi|in|oh|ky|tn|ms|al|ga|fl|sc|nc|va|wv|pa|ny|vt|nh|me|ma|ri|ct|nj|de|md)[_\- ]*(?<num>\d+[a-z]?)(?:$|[_\-])",
        RegexOptions.IgnoreCase | RegexOptions.Compiled);

    private static readonly Regex CompactRoadTokenRegex = new(
        @"^(?<prefix>is|i|us|sr|ca|nv|az|or|wa|ut|id|mt|wy|co|nm|tx|ok|ks|ne|sd|nd|mn|ia|mo|ar|la|wi|il|mi|in|oh|ky|tn|ms|al|ga|fl|sc|nc|va|wv|pa|ny|vt|nh|me|ma|ri|ct|nj|de|md)(?<num>\d+[a-z]?)$",
        RegexOptions.IgnoreCase | RegexOptions.Compiled);

    public RoadOverlayResolver(Map map)
    {
        network=new(()=>new RoadNetworkIndex(map));
        foreach (var overlay in map.MapItems.Values.OfType<MapOverlay>())
        {
            if (overlay.Type != OverlayType.RoadName || overlay.Node is null) continue;
            var raw = overlay.Look.ToString();
            overlays.Add(new ParsedRoadOverlay(
                overlay.Uid,
                overlay.Node.Uid,
                overlay.Node.Position,
                raw,
                NormalizeRoadToken(raw)));
        }
    }

    public IReadOnlyList<ParsedRoadOverlay> Overlays => overlays;
    public int NormalizedCount => overlays.Count(o => !string.IsNullOrWhiteSpace(o.Identity));

    public IReadOnlyList<RoadOverlayMatch> Match(RouteCorridor corridor, double maxLateralM = 180.0)
    {
        if (corridor.Edges.Count == 0) return [];
        const double bboxMargin = 300.0;
        var result = new List<RoadOverlayMatch>();
        foreach (var overlay in overlays)
        {
            if (string.IsNullOrWhiteSpace(overlay.Identity)) continue;
            if (overlay.Position.X < corridor.MinX - bboxMargin || overlay.Position.X > corridor.MaxX + bboxMargin ||
                overlay.Position.Z < corridor.MinZ - bboxMargin || overlay.Position.Z > corridor.MaxZ + bboxMargin)
                continue;
            var projection = corridor.Project(overlay.Position);
            if (projection is null || projection.LateralM > maxLateralM) continue;
            var association=RoadNetworkIndex.Associate(network.Value.Query(overlay.Position));
            var confidence=association.Owner==projection.ItemUid.ToString("X16")?association.Confidence:"LOW";
            result.Add(new RoadOverlayMatch(overlay, projection, confidence));
        }
        return result
            .OrderBy(m => m.Projection.StationM)
            .ThenBy(m => m.Projection.LateralM)
            .ToList();
    }

    public void WriteGlobalIndex(string path)
    {
        using var writer = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        writer.WriteLine("overlay_uid,node_uid,x,y,z,raw_token,normalized_identity");
        foreach (var o in overlays.OrderBy(o => o.Uid))
        {
            writer.WriteLine(string.Join(',',
                Hex(o.Uid), Hex(o.NodeUid),
                F(o.Position.X), F(o.Position.Y), F(o.Position.Z),
                Csv(o.RawToken), Csv(o.Identity)));
        }
    }

    public static string NormalizeRoadToken(string raw)
    {
        if (string.IsNullOrWhiteSpace(raw) || raw == "0") return "";
        var token = raw.Trim().ToLowerInvariant().Replace(' ', '_');
        token = token.Replace("road_name_", "road_").Replace("roadname_", "road_");
        // Native overlay families, not a list of highway numbers or GPS routes.
        var business=Regex.Match(token,@"^bus(\d{1,4}[a-z]?)$");
        if(business.Success)return "I-"+business.Groups[1].Value.ToUpperInvariant()+"-BUS";
        var loop=Regex.Match(token,@"^txl(\d{1,4})$");
        if(loop.Success)return "TX-LOOP-"+loop.Groups[1].Value;
        var alternate=Regex.Match(token,@"^us(\d{1,4}[a-z]?)alt$");
        if(alternate.Success)return "US-"+alternate.Groups[1].Value.ToUpperInvariant()+"-ALT";
        var stateReference=Regex.Match(token,@"^([a-z]{2})_r(\d{1,4}[a-z]?)$");
        if(stateReference.Success)
        {
            var reference=stateReference.Groups[1].Value.ToUpperInvariant()+"-"+stateReference.Groups[2].Value.ToUpperInvariant();
            if(SignTextClassifier.IsRoadIdentity(reference))return reference;
        }

        var match = RoadTokenRegex.Match("_" + token + "_");
        if (!match.Success)
        {
            var compact = CompactRoadTokenRegex.Match(token);
            if (!compact.Success) return "";
            match = compact;
        }

        var prefix = match.Groups["prefix"].Value.ToLowerInvariant();
        var number = match.Groups["num"].Value.ToUpperInvariant();
        var displayPrefix = prefix switch
        {
            "is" or "interstate" or "i" => "I",
            "us" => "US",
            "sr" => "SR",
            _ => prefix.ToUpperInvariant()
        };
        return $"{displayPrefix}-{number}";
    }

    public static void WriteMatches(string path, IReadOnlyList<RoadOverlayMatch> matches)
    {
        using var writer = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        writer.WriteLine("overlay_uid,node_uid,raw_token,normalized_identity,x,y,z,route_station_m,route_nav_station_m,lateral_m,route_segment_index,route_item_type,route_item_uid,route_heading_deg,confidence");
        foreach (var m in matches)
        {
            writer.WriteLine(string.Join(',',
                Hex(m.Overlay.Uid), Hex(m.Overlay.NodeUid), Csv(m.Overlay.RawToken), Csv(m.Overlay.Identity),
                F(m.Overlay.Position.X), F(m.Overlay.Position.Y), F(m.Overlay.Position.Z),
                F(m.Projection.StationM), F(m.Projection.NavStationM), F(m.Projection.LateralM),
                m.Projection.SegmentIndex, Csv(m.Projection.ItemType), Hex(m.Projection.ItemUid), F(m.Projection.HeadingDeg), Csv(m.Confidence)));
        }
    }

    private static string F(double value) => value.ToString("0.######", CultureInfo.InvariantCulture);
    private static string Hex(ulong value) => value.ToString("X16", CultureInfo.InvariantCulture);
    private static string Csv(string? value)
    {
        value ??= "";
        if (!value.Contains(',') && !value.Contains('"') && !value.Contains('\n') && !value.Contains('\r')) return value;
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}
