using System.Diagnostics;
using System.Globalization;

// A single bounded telemetry process, never one nvidia-smi launch per frame/sample.
// NVIDIA values describe the whole adapter, not just ATS or an individual mod.
sealed class GpuMonitor : IDisposable
{
    Process? process;Task? reader;Task? errors;readonly Stopwatch clock=new();
    public List<GpuReading> Samples {get;}=[];
    public string? Warning {get;private set;}
    public void Start()
    {
        var exe=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"nvidia-smi.exe");
        if(!File.Exists(exe)){Warning="Télémétrie NVIDIA indisponible : nvidia-smi absent. Aucune charge GPU n'est inventée.";return;}
        try
        {
            var start=new ProcessStartInfo(exe){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
            start.ArgumentList.Add("--query-gpu=index,name,utilization.gpu,memory.used,memory.total,temperature.gpu,power.draw,clocks.current.graphics");
            start.ArgumentList.Add("--format=csv,noheader,nounits");start.ArgumentList.Add("--loop-ms=1000");
            clock.Start();process=Process.Start(start)??throw new IOException("nvidia-smi non démarré");
            try{process.PriorityClass=ProcessPriorityClass.BelowNormal;}catch(System.ComponentModel.Win32Exception){}
            errors=Task.Run(async()=>{while(await process.StandardError.ReadLineAsync() is {} line)if(Warning is null)Warning="Télémétrie NVIDIA : "+line[..Math.Min(line.Length,250)];});
            reader=Task.Run(async()=>{while(await process.StandardOutput.ReadLineAsync() is {} line)
            {
                var sample=Parse(line,clock.Elapsed.TotalSeconds);if(sample is not null&&Samples.Count<4800)Samples.Add(sample);
            }});
        }
        catch(Exception e) when(e is IOException or System.ComponentModel.Win32Exception){Warning="Télémétrie NVIDIA indisponible : "+e.Message;}
    }
    internal static GpuReading? Parse(string line,double seconds)
    {
        var fields=line.Split(',').Select(v=>v.Trim()).ToArray();if(fields.Length!=8||!int.TryParse(fields[0],out int index))return null;
        double? Number(int i)=>double.TryParse(fields[i],NumberStyles.Float,CultureInfo.InvariantCulture,out double n)&&double.IsFinite(n)&&n>=0?n:null;
        return new(seconds,index,fields[1],Number(2),Number(3),Number(4),Number(5),Number(6),Number(7));
    }
    public async Task Stop()
    {
        try{if(process is not null&&!process.HasExited)process.Kill();}catch(InvalidOperationException){}catch(System.ComponentModel.Win32Exception){}
        if(reader is not null)await reader;if(errors is not null)await errors;
        if(Samples.Count==0&&Warning is null)Warning="Aucun échantillon GPU exploitable reçu.";
    }
    public void Dispose(){try{if(process is not null&&!process.HasExited)process.Kill();}catch{}process?.Dispose();}
}
