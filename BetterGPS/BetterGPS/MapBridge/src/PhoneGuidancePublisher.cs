using System.IO.MemoryMappedFiles;
using System.Text;

namespace BetterGPSNavigation;

// A bounded presentation snapshot, not a second navigation engine. The phone
// consumes exactly the same instructions as the console and live journal.
public sealed class PhoneGuidancePublisher : IDisposable
{
    public const string MappingName = "Local\\BetterGPSOverlayGuidance-v1";
    public const int Capacity = 4096;
    public const uint Magic = 0x48504753;
    private readonly MemoryMappedFile mapping;
    private readonly MemoryMappedViewAccessor view;
    private long sequence;
    private LiveGuidance? presented;
    [System.Runtime.Versioning.SupportedOSPlatform("windows")]
    public PhoneGuidancePublisher()
    {
        mapping = MemoryMappedFile.CreateOrOpen(MappingName, Capacity, MemoryMappedFileAccess.ReadWrite);
        view = mapping.CreateViewAccessor(0, Capacity, MemoryMappedFileAccess.ReadWrite);
        Publish(new(0,"WAITING_FOR_CAPTURE",null,0,null,null,null,null,null,null,null,null,null,null,[],null,null,"Waiting for truck telemetry"));
    }
    public static byte[] Encode(LiveGuidance g, long utcMilliseconds)
    {
        var bytes = new byte[Capacity];
        BitConverter.GetBytes(Magic).CopyTo(bytes,0);
        BitConverter.GetBytes(1U).CopyTo(bytes,4);
        BitConverter.GetBytes(utcMilliseconds).CopyTo(bytes,16);
        BitConverter.GetBytes(g.Instruction is null ? 0U : 1U).CopyTo(bytes,24);
        BitConverter.GetBytes(g.DistanceM ?? double.NaN).CopyTo(bytes,32);
        BitConverter.GetBytes(g.ContinueDistanceM ?? double.NaN).CopyTo(bytes,40);
        BitConverter.GetBytes(g.ThenDistanceM ?? double.NaN).CopyTo(bytes,48);
        Write(bytes,64,256,g.Instruction);
        Write(bytes,320,256,Road(g.CurrentRoad));
        Write(bytes,576,256,Road(g.TargetRoad));
        Write(bytes,832,256,g.Destination);
        Write(bytes,1088,256,g.ThenInstruction is null?null:NativeBridgeCli.SimpleAction(g.ThenInstruction));
        Write(bytes,1344,256,g.Status);
        Write(bytes,1600,256,g.Detail);
        Write(bytes,1856,256,g.Exit);
        Write(bytes,2112,256,g.Cardinal);
        Write(bytes,2368,512,g.Instruction is null ? StatusText(g.Status) : NativeBridgeCli.Describe(g));
        return bytes;
    }
    private static string Road(LiveRoadContext? road) => road?.Status=="RESOLVED"
        ? string.Join(" / ",road.Labels.Where(l=>l.Confidence is "HIGH" or "MEDIUM").Select(l=>
            l.Identity+(l.CardinalConfidence is "HIGH" or "MEDIUM" && l.Cardinal.Length>0?" "+l.Cardinal:"")+
            (l.Confidence=="MEDIUM"?" [provisional]":""))) : "";
    private static string StatusText(string status) => status switch
    {
        "NO_ROUTE" or "WAITING_ROUTE" or "WAITING_FOR_ROUTE" => "Set a route in the game",
        "ARRIVED" or "COMPLETE" => "You have arrived",
        "PAUSED" => "Navigation paused",
        "PREPARING_ROUTE" or "ROUTE_PREPARING" => "Preparing guidance...",
        "STOPPED" => "Guidance offline",
        "STALE" or "DISCONNECTED" => "Waiting for fresh truck telemetry...",
        "ROUTE_REJECTED" or "ROUTE_UNAVAILABLE" or "UNCERTAIN_ACTION" => "Guidance unavailable on this portion",
        _ => "Waiting for live guidance..."
    };
    private static void Write(byte[] destination,int offset,int capacity,string? text)
    {
        // Encoder.Convert never splits a multibyte UTF-8 character at the bound.
        Encoding.UTF8.GetEncoder().Convert((text??"").AsSpan(),destination.AsSpan(offset,capacity-1),true,
            out _,out _,out _);
    }
    public void Publish(LiveGuidance guidance)
    {
        // Freeze only the visible snapshot during pause, never the engine. Any
        // route change, invalidation or disconnection replaces this snapshot.
        if(guidance.Status=="PAUSED" && presented is {Instruction:not null} previous &&
            previous.RouteId==guidance.RouteId && previous.Revision==guidance.Revision)
            guidance=previous with{TimestampMs=guidance.TimestampMs};
        presented=guidance;
        var bytes=Encode(guidance,DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
        view.Write(8,++sequence); // odd => update in progress
        Thread.MemoryBarrier();
        view.WriteArray(0,bytes,0,8);
        view.WriteArray(16,bytes,16,Capacity-16);
        Thread.MemoryBarrier();
        view.Write(8,++sequence); // even => one complete snapshot
    }
    public void Dispose()
    {
        Publish(new(0,"STOPPED",null,0,null,null,null,null,null,null,null,null,null,null,[],null,null,"Guidance host stopped"));
        view.Dispose();mapping.Dispose();
    }
}
