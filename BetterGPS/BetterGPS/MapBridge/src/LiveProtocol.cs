using System.Buffers.Binary;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace BetterGPSNavigation;

// Coordinates: SCS world metres, Y up; heading atan2(delta Z, delta X), degrees.
// Navigation metres and world metres deliberately use distinct fields.
public sealed record LivePoint(double X, double Y, double Z);
public sealed record LiveEdge(int SegmentIndex, string ItemUid, LivePoint Start, LivePoint End,
    double WorldStartM, double WorldEndM, double NavStartM, double NavEndM);
public sealed record LiveProof(string Field, string Value, string Confidence, string Source,
    string SourceId, string BranchId, string Detail);
public sealed record LiveAction(string Id, int EventIndex, int SegmentIndex, double NavStationM,
    string Instruction, string BranchId, string Confidence, LiveProof[] Proofs)
{
    [JsonIgnore(Condition=JsonIgnoreCondition.WhenWritingNull)]
    public LiveSignTarget[]? SignTargets {get;init;}
    [JsonIgnore(Condition=JsonIgnoreCondition.WhenWritingNull)]
    public int? RoundaboutExitNumber {get;init;}
}
public sealed record LiveSignTarget(string Road,string Cardinal,string Destination,string Confidence,string SourceId)
{
    [JsonIgnore(Condition=JsonIgnoreCondition.WhenWritingNull)]
    public string? Kind {get;init;}
}
public sealed record LiveRoute(string RouteId, LiveEdge[] Edges, LiveAction[] Actions)
{
    [JsonIgnore(Condition=JsonIgnoreCondition.WhenWritingNull)]
    public string? EvidenceFile {get;init;}
    [JsonIgnore(Condition=JsonIgnoreCondition.WhenWritingNull)]
    public string? EvidenceSha256 {get;init;}
    [JsonIgnore(Condition=JsonIgnoreCondition.WhenWritingNull)]
    public LiveServiceArea[]? ServiceAreas {get;init;}
    [JsonIgnore(Condition=JsonIgnoreCondition.WhenWritingDefault)]
    public bool IsPartial {get;init;}
    [JsonIgnore(Condition=JsonIgnoreCondition.WhenWritingNull)]
    public double? NavigationRemainingAtOriginM {get;init;}
    [JsonIgnore(Condition=JsonIgnoreCondition.WhenWritingNull)]
    public RoadIdentitySnapshot? RoadIdentities {get;init;}
}
public sealed record LiveServiceArea(string Id,string Kind,double MinX,double MaxX,double MinY,double MaxY,double MinZ,double MaxZ,bool OnPlannedRoute);
public sealed record LiveVehicle(LivePoint Position, double HeadingDeg, double SpeedMps, bool Paused,
    double? NavigationRemainingM = null)
{
    [JsonIgnore(Condition=JsonIgnoreCondition.WhenWritingDefault)]
    public bool Reversing {get;init;}
}
public sealed record LiveMessage(int Version, string Type, string SessionId, long Sequence,
    long TimestampMs, long Revision, LiveRoute? Route = null, LiveVehicle? Vehicle = null,
    string? Status = null, string? Detail = null);

public static class LiveProtocol
{
    public const int Version = 2;
    public static int VersionFor(LiveRoute route)=>route.RoadIdentities is null&&route.EvidenceFile is null?1:2;
    public const int MaxPayloadBytes = 8 * 1024 * 1024;
    public static readonly JsonSerializerOptions Json = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        RespectRequiredConstructorParameters = true,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow
    };

    public static byte[] Encode(LiveMessage message)
    {
        Validate(message);
        var bytes = JsonSerializer.SerializeToUtf8Bytes(message, Json);
        if (bytes.Length > MaxPayloadBytes) throw new InvalidDataException("Message exceeds 8 MiB.");
        return bytes;
    }

    public static LiveMessage Decode(ReadOnlySpan<byte> bytes)
    {
        if (bytes.Length is 0 or > MaxPayloadBytes) throw new InvalidDataException("Invalid message size.");
        var message = JsonSerializer.Deserialize<LiveMessage>(bytes, Json)
            ?? throw new InvalidDataException("Null message.");
        Validate(message);
        return message;
    }

    public static async Task WriteAsync(Stream stream, LiveMessage message, CancellationToken token = default)
    {
        var payload = Encode(message);
        var header = new byte[4];
        BinaryPrimitives.WriteInt32LittleEndian(header, payload.Length);
        await stream.WriteAsync(header, token);
        await stream.WriteAsync(payload, token);
        await stream.FlushAsync(token);
    }

    public static async Task<LiveMessage?> ReadAsync(Stream stream, CancellationToken token = default)
    {
        var header = new byte[4];
        if (await stream.ReadAsync(header.AsMemory(0, 1), token) == 0) return null;
        await stream.ReadExactlyAsync(header.AsMemory(1), token);
        var size = BinaryPrimitives.ReadInt32LittleEndian(header);
        if (size is <= 0 or > MaxPayloadBytes) throw new InvalidDataException("Invalid frame length.");
        var payload = new byte[size];
        await stream.ReadExactlyAsync(payload, token);
        return Decode(payload);
    }

    public static void Validate(LiveMessage m)
    {
        if (m.Version is not (1 or 2) || string.IsNullOrWhiteSpace(m.SessionId) || m.SessionId.Length > 128 ||
            m.Sequence < 0 || m.TimestampMs < 0 || m.Revision < 0)
            throw new InvalidDataException("Invalid protocol envelope.");
        if (m.Type is not ("Hello" or "RouteSnapshot" or "VehicleState" or "RouteCleared" or "Heartbeat" or "SourceState"))
            throw new InvalidDataException("Unknown message type.");
        if(m.Type=="SourceState" && (m.Status is not ("WAITING_DLL" or "WAITING_ROUTE" or "WAITING_POSITION" or
            "PREPARING_ROUTE" or "RECALCULATING" or "GPS_UNAVAILABLE" or "INVALID_ROUTE" or "TRANSIENT_ROUTE" or
            "CLEAR_PENDING" or "ROUTE_TOO_LARGE" or "PAUSED" or "NO_TELEMETRY" or "REVERSING" or "STALE" or
            "ROUTE_REJECTED" or "ROUTE_UNAVAILABLE" or "DISCONNECTED" or "NO_ROUTE") || string.IsNullOrWhiteSpace(m.Detail)))
            throw new InvalidDataException("Invalid source state.");
        if(m.Type!="SourceState" && (m.Status is not null || m.Detail is not null))
            throw new InvalidDataException("Source state fields on another message type.");
        if ((m.Type == "RouteSnapshot") != (m.Route is not null) ||
            (m.Type == "VehicleState") != (m.Vehicle is not null))
            throw new InvalidDataException("Payload does not match message type.");
        if (m.Route is { } r)
        {
            if(r.EvidenceFile is not null)
            {
                if(m.Version!=2)throw new InvalidDataException("Local route evidence requires protocol v2.");
                Validate(m with{Route=RouteEvidence.Unpack(r)});return;
            }
            if(r.IsPartial&&r.Actions?.Any(a=>a?.Instruction=="ARRIVE")==true)throw new InvalidDataException("A partial route cannot announce arrival.");
            if(m.Version==1 && r.RoadIdentities is not null)throw new InvalidDataException("Identity snapshot requires protocol v2.");
            if (string.IsNullOrWhiteSpace(r.RouteId) || r.Edges is null || r.Actions is null ||
                r.Edges.Length is 0 or > 100000 || r.Actions.Length > 10000)
                throw new InvalidDataException("Invalid compiled route.");
            double world = -1, nav = -1;
            if(r.ServiceAreas is {} areas&&(areas.Length>2048||areas.Any(a=>a is null||string.IsNullOrWhiteSpace(a.Id)||
                !ServiceAreaGuidance.Kinds.Contains(a.Kind)||!Finite(a.MinX,a.MaxX,a.MinY,a.MaxY,a.MinZ,a.MaxZ)||
                a.MinX>a.MaxX||a.MinY>a.MaxY||a.MinZ>a.MaxZ||a.MaxX-a.MinX>520||a.MaxZ-a.MinZ>520)))
                throw new InvalidDataException("Invalid service area context.");
            int segment = -1;
            foreach (var e in r.Edges)
            {
                if (e is null || !PointValid(e.Start) || !PointValid(e.End) ||
                    !Finite(e.WorldStartM, e.WorldEndM, e.NavStartM, e.NavEndM) ||
                    e.SegmentIndex < 0 || e.SegmentIndex < segment ||
                    !System.Text.RegularExpressions.Regex.IsMatch(e.ItemUid ?? "",@"^[0-9A-Fa-f]{16}$") ||
                    e.WorldStartM < world || e.NavStartM < nav || e.WorldStartM < 0 || e.NavStartM < 0 ||
                    e.WorldEndM <= e.WorldStartM || e.NavEndM < e.NavStartM ||
                    Distance(e.Start, e.End) < 0.000001)
                    throw new InvalidDataException("Invalid or unordered corridor edge.");
                world = e.WorldEndM; nav = e.NavEndM; segment = e.SegmentIndex;
            }
            if(r.NavigationRemainingAtOriginM is double origin&&(!double.IsFinite(origin)||origin<0||origin<nav-.1))
                throw new InvalidDataException("Invalid ATS navigation origin.");
            var ids = new HashSet<string>(StringComparer.Ordinal);
            double previous = -1;
            foreach (var a in r.Actions)
            {
                if (a is null || string.IsNullOrWhiteSpace(a.Id) || !ids.Add(a.Id) ||
                    !Finite(a.NavStationM) || a.NavStationM < previous || a.NavStationM < 0 || a.NavStationM > nav ||
                    !r.Edges.Any(e => e.SegmentIndex == a.SegmentIndex &&
                        e.NavStartM <= a.NavStationM + 0.01 && e.NavEndM >= a.NavStationM - 0.01) ||
                    !AllowedInstructions.Contains(a.Instruction) || a.Proofs is null || a.Proofs.Length > 1000 ||
                    a.Proofs.Any(p => p is null || string.IsNullOrWhiteSpace(p.Field) || p.Value is null))
                    throw new InvalidDataException($"Invalid route action {a?.Id}: segment={a?.SegmentIndex}, station={a?.NavStationM}, instruction={a?.Instruction}.");
                previous = a.NavStationM;
                if(a.RoundaboutExitNumber is {} ordinal&&(a.Instruction!="ROUNDABOUT"||ordinal is <1 or >32))
                    throw new InvalidDataException("Invalid roundabout exit ordinal.");
                if(a.SignTargets is {} targets&&(targets.Length>1000||targets.Any(t=>t is null||
                    !RoadIdentityRules.Valid(t.Kind??"RouteReference",t.Road)||t.Cardinal is not ("" or "NORTH" or "SOUTH" or "EAST" or "WEST")||
                    t.Confidence!="MEDIUM"||string.IsNullOrWhiteSpace(t.SourceId)||t.Destination is null)))
                    throw new InvalidDataException("Invalid directional sign target.");
                foreach(var p in a.Proofs.Where(p=>p.Source=="SIGN_GROUP"))SignGroupProof.Validate(p,a.BranchId);
            }
            if(r.RoadIdentities is {} identities)RoadIdentityValidation.Validate(r,identities);
        }
        if (m.Vehicle is { } v && (!PointValid(v.Position) || !Finite(v.HeadingDeg, v.SpeedMps) ||
            v.SpeedMps < 0 || (v.NavigationRemainingM is { } d && (!Finite(d) || d < 0))))
            throw new InvalidDataException("Invalid vehicle state.");
    }

    public static readonly HashSet<string> AllowedInstructions = new(StringComparer.Ordinal)
    { "TURN_LEFT", "TURN_RIGHT", "SHARP_LEFT", "SHARP_RIGHT", "KEEP_LEFT", "KEEP_RIGHT", "CONTINUE", "STAY", "MERGE", "MERGE_LEFT", "MERGE_RIGHT", "EXIT_LEFT", "EXIT_RIGHT", "U_TURN", "U_TURN_LEFT", "U_TURN_RIGHT", "ROUNDABOUT", "SLIGHT_LEFT", "SLIGHT_RIGHT", "CONTINUE_AT_FORK", "FORK_STRAIGHT", "USE_SELECTED_LANES" };
    internal static bool Finite(params double[] values) => values.All(double.IsFinite);
    internal static bool PointValid(LivePoint? p) => p is not null && Finite(p.X, p.Y, p.Z);
    internal static double Distance(LivePoint a, LivePoint b) => Math.Sqrt(Math.Pow(a.X-b.X, 2) + Math.Pow(a.Y-b.Y, 2) + Math.Pow(a.Z-b.Z, 2));
}
