using System.Diagnostics;
using System.Text.Json;
namespace BetterGPSNavigation;
internal static class PerformanceTrace
{
    private static readonly AsyncLocal<string?> Destination=new();
    public static string? Output {get=>Destination.Value;set=>Destination.Value=value;}
    internal static Action<string,long>? AllocationObserver;
    internal static Action<string,double,long>? DetailObserver;
    internal static IDisposable? Detail(string stage)=>DetailObserver is null?null:new Detailed(stage);
    private sealed class Detailed(string stage):IDisposable
    {
        private readonly long start=Stopwatch.GetTimestamp(),bytes=GC.GetAllocatedBytesForCurrentThread();
        public void Dispose()=>DetailObserver?.Invoke(stage,Stopwatch.GetElapsedTime(start).TotalSeconds,GC.GetAllocatedBytesForCurrentThread()-bytes);
    }
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string,object> Gates=new();
    public static IDisposable Measure(string stage)=>new Timing(stage);
    private sealed class Timing(string stage):IDisposable
    {
        private readonly Stopwatch watch=Stopwatch.StartNew();
        private readonly long diagnosticStart=RuntimeDiagnostics.Begin();
        private readonly long allocationStart=AllocationObserver is null?0:GC.GetAllocatedBytesForCurrentThread();
        public void Dispose()
        {
            watch.Stop();
            RuntimeDiagnostics.End(stage,diagnosticStart);
            if(allocationStart!=0)AllocationObserver?.Invoke(stage,GC.GetAllocatedBytesForCurrentThread()-allocationStart);
            Console.WriteLine($"[Timing] {stage}: {watch.Elapsed.TotalSeconds:F3} s");
            if(Output is {} path)lock(Gates.GetOrAdd(path,_=>new()))RouteDiagnostics.Try(()=>File.AppendAllText(path,JsonSerializer.Serialize(new{stage,seconds=watch.Elapsed.TotalSeconds,utc=DateTime.UtcNow})+Environment.NewLine));
        }
    }
}
