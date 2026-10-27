using System.Text;
using System.Security.Cryptography;
using System.Text.Json;

namespace BetterGPSNavigation;

// Immutable proofs are stored once; the moving journal contains references.
internal sealed class GuidanceJournal : IDisposable
{
    private readonly StreamWriter events, proofs;
    private readonly HashSet<string> written=[];
    private sealed record ProofDigest(string Value);
    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<LiveProof,ProofDigest> digests=new();
    private static string ProofHash(LiveProof[] packet)
    {
        var parts=packet.Select(p=>digests.GetValue(p,proof=>new(Convert.ToHexString(
            SHA256.HashData(JsonSerializer.SerializeToUtf8Bytes(proof,LiveProtocol.Json))))).Value);
        return Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(string.Join(':',parts))));
    }
    private string? lastKey;
    private long lastSecond=-1;
    private readonly bool compact;private long compactBytes;
    public GuidanceJournal(string directory,bool compact=false)
    {
        this.compact=compact;
        events=Open(Path.Combine(directory,"guidance.jsonl"));
        proofs=compact?new StreamWriter(Stream.Null):Open(Path.Combine(directory,"guidance-proofs.jsonl"));
    }
    private static StreamWriter Open(string path)
    {
        try{return new(path,false,new UTF8Encoding(false));}
        catch(Exception e) when(e is IOException or UnauthorizedAccessException)
        {RouteDiagnostics.Try(()=>throw new IOException("Guidance recording unavailable: "+path,e));return new(Stream.Null);}
    }
    public void Write(LiveGuidance g)
    {
        if(compact){
            if(compactBytes>=8L*1024*1024)return;
            var compactKey=$"{g.Status}:{g.ActionId}:{g.Road}:{g.PreparationStatus}";
            if(compactKey==lastKey&&g.TimestampMs/1000==lastSecond)return;
            var line=JsonSerializer.Serialize(new{g.TimestampMs,g.Status,g.ActionId,g.Instruction,g.Road,g.Cardinal,g.DistanceM,g.PreparationStatus},LiveProtocol.Json);
            var bytes=Encoding.UTF8.GetByteCount(line)+Encoding.UTF8.GetByteCount(Environment.NewLine);if(compactBytes+bytes>8L*1024*1024){compactBytes=8L*1024*1024;return;}
            events.WriteLine(line);events.Flush();compactBytes+=bytes;lastKey=compactKey;lastSecond=g.TimestampMs/1000;return;
        }
        string Context(LiveRoadContext? c)=>c is null?"":c.Status+":"+string.Join('|',c.Labels.Select(l=>l.Identity+":"+l.Cardinal));
        var proofHash=ProofHash(g.Proofs);
        var key=$"{g.RouteId}:{g.Revision}:{g.Status}:{g.ActionId}:{g.Instruction}:{Context(g.CurrentRoad)}:{Context(g.TargetRoad)}:{Context(g.JoinedRoad)}:{Context(g.ContinueToward)}:{g.Detail}:{g.PreparationStatus}:{g.PreparationDetail}:{g.Road}:{proofHash}";
        bool moving=g.Status is "CRUISE" or "PREPARE" or "EXECUTE";
        if(key==lastKey&&(!moving||g.TimestampMs/1000==lastSecond))return;
        string? reference=null;
        if(g.Proofs.Length>0)
        {
            reference=$"{g.RouteId}:{g.Revision}:{g.ActionId ?? g.Status}:{proofHash}";
            if(written.Add(reference))proofs.WriteLine(JsonSerializer.Serialize(new{id=reference,proofs=g.Proofs},LiveProtocol.Json));
        }
        events.WriteLine(JsonSerializer.Serialize(g with{Proofs=[],ProofReference=reference},LiveProtocol.Json));
        // Flush changes and at most once per moving second; never every telemetry tick.
        events.Flush();proofs.Flush();lastKey=key;lastSecond=g.TimestampMs/1000;
    }
    public void Dispose(){RouteDiagnostics.Try(()=>events.Dispose());RouteDiagnostics.Try(()=>proofs.Dispose());}
}
