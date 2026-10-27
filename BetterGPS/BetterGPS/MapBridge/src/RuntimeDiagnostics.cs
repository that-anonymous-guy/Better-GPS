using System.Diagnostics;
using System.IO.MemoryMappedFiles;
namespace BetterGPSNavigation;

// Opt-in, fixed-size IPC. File output belongs to the external collector.
internal static class RuntimeDiagnostics
{
    private static MemoryMappedFile? data,control;
    private static MemoryMappedViewAccessor? view,command;
    private static readonly object gate=new();
    private static Timer? timer;
    private static long write;
    private static int active;
    internal static void Start()
    {
        if(!OperatingSystem.IsWindows())return;
        try{
            data=MemoryMappedFile.CreateOrOpen($"Local\\BetterGPS-Diagnostics-{Environment.ProcessId}-3",65536);
            view=data.CreateViewAccessor();command=null;
            // Engine control uses its own PID; the collector discovers it independently.
            control=MemoryMappedFile.CreateOrOpen($"Local\\BetterGPS-Diagnostics-Control-{Environment.ProcessId}",4096);command=control.CreateViewAccessor();
            view.Write(4,1);view.Write(8,Environment.ProcessId);view.Write(12,3);view.Write(16,Stopwatch.Frequency);view.Write(0,0x42475044);
            timer=new Timer(_=>{try{var now=Stopwatch.GetTimestamp();var h=command.ReadInt64(8);Volatile.Write(ref active,command.ReadInt32(0)==0x42475043&&command.ReadInt32(4)==1&&command.ReadInt32(24)!=0&&command.ReadInt64(16)==Stopwatch.Frequency&&now>=h&&now-h<3*Stopwatch.Frequency?1:0);if(Volatile.Read(ref active)==0)return;
                Value(21,GC.GetTotalMemory(false));Value(22,GC.GetTotalAllocatedBytes(false));for(int i=0;i<3;i++)Value(23+i,GC.CollectionCount(i));Value(26,(long)GC.GetTotalPauseDuration().TotalMicroseconds);
            }catch{Volatile.Write(ref active,0);}},null,0,500);
        }catch{Volatile.Write(ref active,0);}
    }
    private static void Value(int metric,long value){lock(gate)Event(metric,Stopwatch.GetTimestamp(),0,value);}
    private static void Event(int metric,long start,long ticks,long value)
    {
        if(view is null)return;var ticket=++write;long offset=7040+(ticket-1)%1024*40;
        view.Write(offset,-ticket);view.Write(offset+8,start);view.Write(offset+16,ticks);view.Write(offset+24,metric);view.Write(offset+28,Environment.CurrentManagedThreadId);view.Write(offset+32,value);Thread.MemoryBarrier();view.Write(offset,ticket);view.Write(24,ticket);
    }
    internal static long Begin()=>Volatile.Read(ref active)!=0?Stopwatch.GetTimestamp():0;
    internal static void End(string stage,long start)
    {
        if(start==0||Volatile.Read(ref active)==0)return;var ticks=Stopwatch.GetTimestamp()-start;
        int metric=stage.StartsWith("world-tile:")?17:stage.StartsWith("route:")||stage.StartsWith("ready-prefix:")?16:stage=="archives"?18:stage.Contains("index")?19:20;
        lock(gate){if(view is null)return;long at=128+metric*216;view.Write(at,view.ReadInt64(at)+1);view.Write(at+8,view.ReadInt64(at+8)+ticks);view.Write(at+16,Math.Max(view.ReadInt64(at+16),ticks));int bucket=0;double us=ticks*1e6/Stopwatch.Frequency,bound=1;while(bucket<23&&us>bound){bucket++;bound*=2;}long hist=at+24+bucket*8;view.Write(hist,view.ReadInt64(hist)+1);Event(metric,start,ticks,0);}
    }
}
