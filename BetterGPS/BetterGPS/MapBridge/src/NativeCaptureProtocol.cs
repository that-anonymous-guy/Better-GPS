using System.Buffers.Binary;
using System.Globalization;
using System.Text.Json;

namespace BetterGPSNavigation;

public sealed record NativePhysical(string NodeUid,double DistanceFromPreviousM,double TotalDistanceTillEndM,
    double TimeFromPreviousS,double TotalTimeTillEndS);
public sealed record NativeLogical(uint Type,uint NodeId,string ItemUid,double DistanceFromPreviousM,
    double TotalDistanceTillEndM,double TimeFromPreviousS,double TotalTimeTillEndS);
public sealed record NativeNode(int RawX,int RawY,int RawZ,uint Flags,double Qw,double Qx,double Qy,double Qz,
    string BackwardUid,string ForwardUid,bool BackwardValid,bool ForwardValid,string Status);
public sealed record NativeSource(string ProbeVersion,string GameBuild,uint GpsManagerOffset);
public sealed record NativeRoute(string Fingerprint,NativePhysical[] Physical,NativeLogical[] Logical)
{
    public NativeNode[]? Nodes { get; init; }
    public long CapturedAtTickMs { get; init; }
    internal NativeRoute WithoutZeroDistanceDuplicates()
    {
        var logicalNodes=Logical.Select((row,index)=>(row,index)).Where(p=>p.row.Type==0).ToArray();
        if(logicalNodes.Length!=Physical.Length)return this;
        var removed=new HashSet<int>();var removedLogical=new HashSet<int>();
        for(int i=1;i<Physical.Length;i++)
        {
            var current=Physical[i];var previous=Physical[i-1];
            var logical=logicalNodes[i];var before=logicalNodes[i-1];
            // Only redundant boundary markers. A lane path between two equal
            // nodes, or any nonzero journey, remains a real loop to resolve.
            if(current.NodeUid!=previous.NodeUid||current.DistanceFromPreviousM!=0||current.TimeFromPreviousS!=0||
                Math.Abs(current.TotalDistanceTillEndM-previous.TotalDistanceTillEndM)>0.01||
                Math.Abs(current.TotalTimeTillEndS-previous.TotalTimeTillEndS)>0.01||
                logical.index!=before.index+1||logical.row.ItemUid!=before.row.ItemUid||
                logical.row.DistanceFromPreviousM!=0||logical.row.TimeFromPreviousS!=0||
                Math.Abs(logical.row.TotalDistanceTillEndM-before.row.TotalDistanceTillEndM)>0.01||
                Math.Abs(logical.row.TotalTimeTillEndS-before.row.TotalTimeTillEndS)>0.01||
                Nodes is not null&&(Nodes.Length!=Physical.Length||Nodes[i]!=Nodes[i-1]))continue;
            removed.Add(i);removedLogical.Add(logical.index);
        }
        return removed.Count==0?this:this with
        {
            Physical=Physical.Where((_,i)=>!removed.Contains(i)).ToArray(),
            Logical=Logical.Where((_,i)=>!removedLogical.Contains(i)).ToArray(),
            Nodes=Nodes?.Where((_,i)=>!removed.Contains(i)).ToArray()
        };
    }
    internal ProbeRoute ToProbe(string id) => new()
    {
        Stem=id,PhysicalPath="",LogicalPath="",NativeEvidence=this,
        Physical=Physical.Select((p,i)=>new PhysicalRouteRow(i,NativeCaptureProtocol.Uid(p.NodeUid),p.DistanceFromPreviousM,
            p.TimeFromPreviousS,p.TotalDistanceTillEndM,p.TotalTimeTillEndS)).ToArray(),
        Logical=Logical.Select((p,i)=>new LogicalRouteRow(i,p.Type,p.NodeId,NativeCaptureProtocol.Uid(p.ItemUid),
            p.DistanceFromPreviousM,p.TimeFromPreviousS,0,0,p.TotalDistanceTillEndM,p.TotalTimeTillEndS)).ToArray()
    };
    public bool IsSuffixOf(NativeRoute older)
    {
        if(Physical.Length>older.Physical.Length || Logical.Length>older.Logical.Length) return false;
        if(Nodes is not null && (older.Nodes is null || !Nodes.SequenceEqual(older.Nodes.TakeLast(Nodes.Length)))) return false;
        if(Nodes is null && older.Nodes is not null) return false;
        // A new endpoint/checkpoint can preserve the same node IDs while
        // changing the navigation station axis. Such a plan must be rebuilt.
        if(Physical.Zip(older.Physical.TakeLast(Physical.Length)).Any(p=>
            Math.Abs(p.First.TotalDistanceTillEndM-p.Second.TotalDistanceTillEndM)>5))return false;
        return Physical.Select(p=>p.NodeUid).SequenceEqual(older.Physical.TakeLast(Physical.Length).Select(p=>p.NodeUid)) &&
            Logical.Select(p=>(p.Type,p.NodeId,p.ItemUid)).SequenceEqual(older.Logical.TakeLast(Logical.Length).Select(p=>(p.Type,p.NodeId,p.ItemUid)));
    }
}
public sealed record NativeVehicle(LivePoint Position,double ScsHeading,double SignedSpeedMps,bool Paused,
    bool PoseValid,bool SpeedValid,double? NavigationRemainingM=null)
{
    public LiveVehicle ToLive() => new(Position,(((-90-360*ScsHeading)%360)+360)%360,
        Math.Abs(SignedSpeedMps),Paused,NavigationRemainingM){Reversing=SignedSpeedMps < -0.1};
}
public sealed record NativeMessage(int Version,string Type,string SessionId,long Sequence,long TimestampMs,long Generation,
    NativeRoute? Route=null,NativeVehicle? Vehicle=null,string? Status=null)
{
    public NativeSource? Source { get; init; }
}

public static class NativeCaptureProtocol
{
    public static ulong Uid(string value) => ulong.Parse(value,NumberStyles.HexNumber,CultureInfo.InvariantCulture);
    private static bool ValidUid(string? value) => value is not null && value.Length==16 &&
        ulong.TryParse(value,NumberStyles.HexNumber,CultureInfo.InvariantCulture,out var uid) && uid!=0;
    public static void Validate(NativeMessage m)
    {
        if(m.Version is not (1 or 2) || string.IsNullOrWhiteSpace(m.SessionId) || m.SessionId.Length>128 ||
            m.Sequence<0 || m.TimestampMs<0 || m.Generation<0 ||
            m.Type is not ("Hello" or "RawRouteSnapshot" or "TelemetryFrame" or "RouteCleared" or "CaptureStatus"))
            throw new InvalidDataException("Invalid native capture envelope.");
        if((m.Type=="RawRouteSnapshot")!=(m.Route is not null) || (m.Type=="TelemetryFrame")!=(m.Vehicle is not null))
            throw new InvalidDataException("Native payload does not match its type.");
        if(m.Type=="CaptureStatus" && string.IsNullOrWhiteSpace(m.Status)) throw new InvalidDataException("Missing capture status.");
        if(m.Vehicle is { } v && (!LiveProtocol.PointValid(v.Position) || !LiveProtocol.Finite(v.ScsHeading,v.SignedSpeedMps) ||
            v.ScsHeading is <0 or >=1 || (v.NavigationRemainingM is { } d && (!double.IsFinite(d)||d<0))))
            throw new InvalidDataException("Invalid captured vehicle.");
        if(m.Route is { } r)
        {
            if((m.Version==2 && r.Nodes is null) || (r.Nodes is not null && r.Nodes.Length!=r.Physical?.Length) || r.CapturedAtTickMs<0)
                throw new InvalidDataException("V2 route requires native node evidence.");
            if(r.Nodes is not null) foreach(var n in r.Nodes)
                if(n is null || !LiveProtocol.Finite(n.Qw,n.Qx,n.Qy,n.Qz) ||
                    n.Qw*n.Qw+n.Qx*n.Qx+n.Qy*n.Qy+n.Qz*n.Qz is <0.9 or >1.1 ||
                    !HexReference(n.BackwardUid) || !HexReference(n.ForwardUid) ||
                    n.Status is not ("READ_OK" or "CONNECTION_UNAVAILABLE"))
                    throw new InvalidDataException("Invalid native node evidence.");
            if(m.Version==2 && r.Physical is { } items)
                for(int i=1;i<items.Length;i++)
                    if(items[i] is { } current && items[i-1] is { } previous && current.TotalDistanceTillEndM>previous.TotalDistanceTillEndM+1)
                        throw new InvalidDataException($"V2 remaining distances increase at node {i}; capture requires review.");
            if(r.Physical is null || r.Logical is null || r.Physical.Length is <2 or >20000 || r.Logical.Length is <2 or >20000 ||
                r.Fingerprint is null || r.Fingerprint.Length!=16 ||
                !ulong.TryParse(r.Fingerprint,NumberStyles.HexNumber,CultureInfo.InvariantCulture,out _))
                throw new InvalidDataException("Incomplete or oversized native route.");
            foreach(var p in r.Physical)
                if(p is null || !ValidUid(p.NodeUid) || !Metrics(p.DistanceFromPreviousM,p.TotalDistanceTillEndM,p.TimeFromPreviousS,p.TotalTimeTillEndS))
                    throw new InvalidDataException("Invalid physical item.");
            foreach(var p in r.Logical)
                if(p is null || p.Type>1 || !ValidUid(p.ItemUid) || !Metrics(p.DistanceFromPreviousM,p.TotalDistanceTillEndM,p.TimeFromPreviousS,p.TotalTimeTillEndS))
                    throw new InvalidDataException("Invalid logical item.");
            if(!r.Physical.Select(p=>p.NodeUid).SequenceEqual(r.Logical.Where(p=>p.Type==0).Select(p=>p.ItemUid)))
                throw new InvalidDataException("Physical and logical node sequences differ.");
        }
    }
    private static bool Metrics(params double[] values)=>values.All(v=>double.IsFinite(v)&&v>=0&&v<1e9);
    private static bool HexReference(string? value)=>value is not null && value.Length==16 &&
        ulong.TryParse(value,NumberStyles.HexNumber,CultureInfo.InvariantCulture,out _);
    public static NativeMessage Decode(ReadOnlySpan<byte> bytes)
    {
        if(bytes.Length is <=0 or >LiveProtocol.MaxPayloadBytes) throw new InvalidDataException("Invalid native frame length.");
        var m=JsonSerializer.Deserialize<NativeMessage>(bytes,LiveProtocol.Json)??throw new InvalidDataException("Null native message.");
        Validate(m); return m;
    }
    public static byte[] Encode(NativeMessage m)
    {
        Validate(m); var bytes=JsonSerializer.SerializeToUtf8Bytes(m,LiveProtocol.Json);
        if(bytes.Length>LiveProtocol.MaxPayloadBytes) throw new InvalidDataException("Native frame exceeds 8 MiB.");
        return bytes;
    }
    public static async Task<NativeMessage?> ReadAsync(Stream stream,CancellationToken token=default)
    {
        var header=new byte[4]; if(await stream.ReadAsync(header.AsMemory(0,1),token)==0) return null;
        await stream.ReadExactlyAsync(header.AsMemory(1),token);
        int length=BinaryPrimitives.ReadInt32LittleEndian(header);
        if(length is <=0 or >LiveProtocol.MaxPayloadBytes) throw new InvalidDataException("Invalid native frame length.");
        var bytes=new byte[length]; await stream.ReadExactlyAsync(bytes,token); return Decode(bytes);
    }
    public static async Task WriteAsync(Stream stream,NativeMessage message,CancellationToken token=default)
    {
        var payload=Encode(message); var header=new byte[4]; BinaryPrimitives.WriteInt32LittleEndian(header,payload.Length);
        await stream.WriteAsync(header,token); await stream.WriteAsync(payload,token); await stream.FlushAsync(token);
    }
}
