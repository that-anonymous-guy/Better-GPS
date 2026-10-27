using System.Diagnostics;
using System.Security.Principal;
using System.Text.Json;
using System.IO.Compression;

return await App.Run(args);
static class App
{
    internal static readonly JsonSerializerOptions Json=new(){WriteIndented=true,PropertyNameCaseInsensitive=true};
    internal static string Value(string[] args,string key,string fallback){var i=Array.IndexOf(args,key);return i>=0&&i+1<args.Length?args[i+1]:fallback;}
    internal static async Task<int> Run(string[] args)
    {
        try
        {
            if(args.Contains("--self-test")){Tests.Run();return 0;}
            if(args.Contains("--gpu-smoke")){using var gpu=new GpuMonitor();gpu.Start();await Task.Delay(3500);await gpu.Stop();Console.WriteLine(JsonSerializer.Serialize(new{gpu.Samples,gpu.Warning},Json));return gpu.Samples.Count>0?0:1;}
            if(args is ["--integration-test",var fixture,var testOutput]){await Tests.Integration(fixture,testOutput);return 0;}
            if(args.FirstOrDefault()=="compare")
            {
                Analysis.Compare(Value(args,"--baseline",""),Value(args,"--candidate",""),Value(args,"--output","comparaison.html"));return 0;
            }
            if(args.FirstOrDefault()=="analyze")
            {
                var path=Value(args,"--capture","");var raw=ReadCapture(path);
                Analysis.Save(raw,Path.GetFullPath(Value(args,"--output",Path.GetDirectoryName(Path.GetFullPath(path))!)));return 0;
            }
            if(args.Length>0&&args[0]!="capture")throw new ArgumentException("Commandes : capture, analyze, compare, --self-test");
            if(!new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator))
            {
                Console.Error.WriteLine("Le traçage Windows exige les droits administrateur. Lance DIAGNOSTIC_MODS_ATS.cmd : seule cette application sera élevée.");return 2;
            }
            int seconds=int.Parse(Value(args,"--seconds","180"));if(seconds<1||seconds>600)throw new ArgumentException("Durée : 1 à 600 secondes");
            Capture.CleanupStaleSessions();
            using var self=Process.GetCurrentProcess();self.PriorityClass=ProcessPriorityClass.BelowNormal;
            Console.WriteLine("ATS — diagnostic indépendant des mods. Attente du jeu (5 minutes)...");
            Process? game=null;var wait=Stopwatch.StartNew();
            while(game is null&&wait.Elapsed.TotalMinutes<5)
            {
                if(int.TryParse(Value(args,"--pid",""),out int pid))game=Process.GetProcessById(pid);
                else game=Process.GetProcessesByName("amtrucks").FirstOrDefault();
                if(game is null)await Task.Delay(500);
            }
            if(game is null)throw new InvalidOperationException("ATS introuvable");
            using(game)
            {
                var output=Path.GetFullPath(Value(args,"--output",Path.Combine(AppContext.BaseDirectory,"captures",DateTime.Now.ToString("yyyyMMdd_HHmmss_fff"))));Directory.CreateDirectory(output);
                File.WriteAllText(Path.Combine(output,"capture.owner"),"ATSModDiagnostics:1");
                var docs=Value(args,"--documents",Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),"American Truck Simulator"));
                var data=await Capture.Run(game,seconds,docs,!args.Contains("--light"),args.Contains("--include-unfocused"));
                data.Label=Value(args,"--label","ATS");
                SaveCapture(data,Path.Combine(output,"capture.json.gz"));
                Analysis.Save(data,output);Console.WriteLine("Rapport : "+Path.Combine(output,"rapport.html"));
                CaptureRetention.Apply(Path.Combine(AppContext.BaseDirectory,"captures"),output);
                if(data.Presents.Count==0)Console.WriteLine("Aucun Present DXGI reçu : le rapport indique explicitement que les FPS ne sont pas disponibles.");
            }
            return 0;
        }
        catch(Exception error){Console.Error.WriteLine("Diagnostic : "+error.Message);if(args is ["--integration-test",_,var testFailureOutput]){Directory.CreateDirectory(testFailureOutput);File.WriteAllText(Path.Combine(testFailureOutput,"failure.txt"),error.ToString());}return 1;}
    }
    internal static CaptureData ReadCapture(string path)
    {
        using var file=File.OpenRead(path);using Stream stream=path.EndsWith(".gz",StringComparison.OrdinalIgnoreCase)?new GZipStream(file,CompressionMode.Decompress,true):file;
        var data=JsonSerializer.Deserialize<CaptureData>(stream,Json)??throw new InvalidDataException("Capture vide");
        if(data.Version!=1)throw new InvalidDataException("Version de capture non prise en charge");return data;
    }
    internal static void SaveCapture(CaptureData data,string path)
    {
        using var file=File.Create(path);using var gzip=new GZipStream(file,CompressionLevel.Fastest);JsonSerializer.Serialize(gzip,data);
    }
}
record ReadEvent(double Seconds,string Path,long Offset,int Bytes,int ThreadId=0);
record PresentEvent(double Seconds,ulong Chain);
record FocusEvent(double Seconds,bool Active);
record ResourceEvent(double Seconds,string Name,double CpuCores,long PrivateBytes,long WorkingBytes);
record CpuModule(string Path,long Base,long Size);
record ModuleSample(double Seconds,ulong Address,int ThreadId=0);
record ThreadReading(double Seconds,int ThreadId,double CpuCores);
record GpuReading(double Seconds,int Index,string Name,double? Utilization,double? UsedMiB,double? TotalMiB,double? Temperature,double? Watts,double? GraphicsMHz);
record StackBucket(int Second,int ThreadId,string Frames,int Samples);
sealed class CaptureData
{
    public int Version {get;set;}=1;
    public int Pid {get;set;}
    public DateTime StartedUtc {get;set;}
    public double Seconds {get;set;}
    public string Label {get;set;}="ATS";
    public List<Mount> Mounts {get;set;}=[];
    public List<ReadEvent> Reads {get;set;}=[];
    public List<PresentEvent> Presents {get;set;}=[];
    public List<FocusEvent> Focus {get;set;}=[];
    public List<double> Markers {get;set;}=[];
    public List<ResourceEvent> Resources {get;set;}=[];
    public List<ModuleSample> CpuSamples {get;set;}=[];
    public List<CpuModule> Modules {get;set;}=[];
    public List<ThreadReading> Threads {get;set;}=[];
    public List<GpuReading> Gpu {get;set;}=[];
    public List<StackBucket> Stacks {get;set;}=[];
    public bool DeepCapture {get;set;}
    public List<string> LogIssues {get;set;}=[];
    public List<string> Warnings {get;set;}=[];
    public int LostEvents {get;set;}
    public int DroppedReads {get;set;}
    public int DroppedPresents {get;set;}
    public int DroppedCpuSamples {get;set;}
    public int UnnamedReads {get;set;}
    public long UnnamedReadBytes {get;set;}
    public int TotalReadEvents {get;set;}
    public int RecoveredReads {get;set;}
    public bool IncludeUnfocused {get;set;}
}
