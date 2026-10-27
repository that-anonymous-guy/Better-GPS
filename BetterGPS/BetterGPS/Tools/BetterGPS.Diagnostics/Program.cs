using System.Diagnostics;
using System.Globalization;
using System.IO.MemoryMappedFiles;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

[assembly:System.Runtime.Versioning.SupportedOSPlatform("windows")]
return await DiagnosticTool.Run(args);

static class DiagnosticTool
{
    internal static readonly JsonSerializerOptions Json=new(){WriteIndented=true};
    static readonly string[] Names=["Frame interval","Native Present wait (includes pacing)","Overlay Present CPU","Texture hook incl. native binding (1/64 sampled)","Phone upload total","Upload Map / driver wait","Menu upload","Phone CPU composition","Menu CPU composition","Host poll / theme","Settings save","Probe worker tick","Full route read","SDK frame callback","Native packet serialization","Capture pipe write / backpressure","Route compilation / prefix","World cache sector","Archive opening","Index preparation","Other engine stage","Managed heap bytes","Managed allocated bytes","GC generation 0","GC generation 1","GC generation 2","GC pause cumulative microseconds","Private native GPS map preparation"];
    static string Value(string[] args,string name,string fallback){var i=Array.IndexOf(args,name);return i>=0&&i+1<args.Length?args[i+1]:fallback;}
    internal static async Task<int> Run(string[] args)
    {
        try{
            if(args.Contains("--self-test")){SelfTest();return 0;}
            using(var self=Process.GetCurrentProcess())self.PriorityClass=ProcessPriorityClass.BelowNormal;
            if(args.FirstOrDefault()=="compare"){
                var baseline=JsonSerializer.Deserialize<Report>(File.ReadAllText(Value(args,"--baseline","")),Json)!;
                var candidate=JsonSerializer.Deserialize<Report>(File.ReadAllText(Value(args,"--candidate","")),Json)!;
                var output=Path.GetFullPath(Value(args,"--output","comparison.html"));Directory.CreateDirectory(Path.GetDirectoryName(output)!);
                File.WriteAllText(output,HtmlCompare(baseline,candidate));Console.WriteLine(output);return 0;
            }
            if(args.FirstOrDefault()=="import-presentmon"){
                var r=new Report{Label=Value(args,"--label","PresentMon"),StartedUtc=DateTime.UtcNow,FrameSource="PresentMon CSV (CPU Present cadence)",Warnings=["Import CSV : les compteurs internes et ressources des processus ne sont pas disponibles."]};
                var frames=ReadPresentMon(Value(args,"--csv",""));r.Frames=SummarizeFrames(frames.Select(x=>x.Ms).ToList());r.FrameTimes=frames;Save(r,Path.GetFullPath(Value(args,"--output","presentmon-report")));return 0;
            }
            if(args.Length>0&&args[0]!="capture")throw new ArgumentException("Commandes : capture, import-presentmon, compare, --self-test");
            var seconds=int.Parse(Value(args,"--seconds","180"),CultureInfo.InvariantCulture);if(seconds<1||seconds>600)throw new ArgumentException("Durée : 1 à 600 secondes.");
            var label=Value(args,"--label","BetterGPS");var directory=Path.GetFullPath(Value(args,"--output",Path.Combine(AppContext.BaseDirectory,"captures",DateTime.Now.ToString("yyyyMMdd_HHmmss_fff"))));Directory.CreateDirectory(directory);
            Console.WriteLine("BetterGPS — diagnostic. Attente d'ATS (5 min maximum)...");
            int? requestedPid=int.TryParse(Value(args,"--pid",""),out var parsed)?parsed:null;
            Process? game=null;var wait=Stopwatch.StartNew();while(game is null&&wait.Elapsed.TotalMinutes<5){game=requestedPid is {} id?Process.GetProcessById(id):Process.GetProcessesByName("amtrucks").FirstOrDefault();if(game is null)await Task.Delay(500);}
            if(game is null)throw new InvalidOperationException("ATS introuvable.");
            using(game)using(var ownership=new Semaphore(1,1,$"Local\\BetterGPS-DiagnosticCapture-{game.Id}")){
                if(!ownership.WaitOne(0))throw new InvalidOperationException("Une capture est déjà en cours pour ce jeu.");
                try{return await Capture(args,game,seconds,label,directory);}finally{ownership.Release();}
            }
        }catch(Exception e){Console.Error.WriteLine("Diagnostic : "+e.Message);return 1;}
    }
    static async Task<int> Capture(string[] args,Process game,int seconds,string label,string directory)
    {
        var report=new Report{Label=label,StartedUtc=DateTime.UtcNow,Warnings=["Les temps internes sont inclusifs : ne pas additionner parents et enfants.","Une corrélation temporelle ne prouve pas qu'un coût en arrière-plan cause une saccade.","Sans PresentMon, les frames correspondent aux appels Present, pas aux images effectivement affichées ; GPU et latence d'affichage non mesurés."]};
        report.BinaryHashes=Hashes(game);
        using var raw=new StreamWriter(Path.Combine(directory,"events.jsonl"));using var resources=new StreamWriter(Path.Combine(directory,"resources.jsonl"));
        var peers=new Dictionary<(int,int),Peer>();var controls=new Dictionary<int,Command>();var monitors=new Dictionary<int,ProcessMonitor>();var events=new List<NativeEvent>();
        var known=new Dictionary<int,string>{{game.Id,game.ProcessName},{Environment.ProcessId,"BetterGPSDiagnostics"}};long nextDiscovery=0;
        if(int.TryParse(Value(args,"--engine-pid",""),out var enginePid)){using var engine=Process.GetProcessById(enginePid);known[enginePid]=engine.ProcessName;}
        var token=DateTime.UtcNow.Ticks;bool cancelled=false;ConsoleCancelEventHandler handler=(_,e)=>{e.Cancel=true;cancelled=true;};Console.CancelKeyPress+=handler;
        var timer=Stopwatch.StartNew();long started=Stopwatch.GetTimestamp(),nextResource=0,logBytes=0;bool keyWasDown=false;Process? pm=null;
        var pmPath=Value(args,"--presentmon","");
        if(pmPath.Length>0){var si=new ProcessStartInfo(Path.GetFullPath(pmPath)){UseShellExecute=false,CreateNoWindow=true};foreach(var s in new[]{"--process_id",game.Id.ToString(),"--output_file",Path.Combine(directory,"presentmon.csv"),"--timed",seconds.ToString(),"--terminate_after_timed","--no_console_stats","--qpc_time","--session_name","BetterGPSDiagnostics-"+game.Id+"-"+token})si.ArgumentList.Add(s);pm=Process.Start(si);}
        Console.WriteLine($"Capture de {seconds}s. Retourne dans ATS. Ctrl+Maj+F8 marque une saccade ; Ctrl+C termine la capture.\nRésultats : {directory}");
        try{
            while(!cancelled&&timer.Elapsed.TotalSeconds<seconds&&!game.HasExited){
                if(timer.ElapsedMilliseconds>=nextDiscovery){nextDiscovery=timer.ElapsedMilliseconds+1000;foreach(var process in Process.GetProcessesByName("BetterGPSNavigation")){using(process)known[process.Id]=process.ProcessName;}
                foreach(var entry in known){
                    try{
                        var pid=entry.Key;
                        if(pid!=Environment.ProcessId&&!controls.ContainsKey(pid))controls[pid]=new Command(pid,token);
                        foreach(var component in pid==game.Id?new[]{1,2}:pid==Environment.ProcessId?Array.Empty<int>():new[]{3})if(!peers.ContainsKey((pid,component))){var peer=Peer.TryOpen(pid,component);if(peer is not null)peers[(pid,component)]=peer;}
                    }catch(Exception e) when(e is InvalidOperationException or System.ComponentModel.Win32Exception or IOException or UnauthorizedAccessException){ }
                }}
                foreach(var command in controls.Values)command.Heartbeat();
                if(timer.ElapsedMilliseconds>=nextResource){nextResource=timer.ElapsedMilliseconds+250;foreach(var entry in known){try{if(!monitors.TryGetValue(entry.Key,out var monitor))monitors[entry.Key]=monitor=new ProcessMonitor(entry.Key,entry.Value);var sample=monitor.Take(timer.Elapsed.TotalSeconds);if(sample is not null){report.Resources.Add(sample);resources.WriteLine(JsonSerializer.Serialize(sample));}}catch(Exception e) when(e is ArgumentException or InvalidOperationException or System.ComponentModel.Win32Exception){}}}
                foreach(var peer in peers.Values){foreach(var ev in peer.Read(started)){
                    if(events.Count<300000)events.Add(ev);else report.TruncatedEvents++;
                    if(logBytes<32L*1024*1024){var line=JsonSerializer.Serialize(ev);raw.WriteLine(line);logBytes+=line.Length+2;}else report.RawLogTruncated=true;
                }}
                var key=Native.Down(0x11)&&Native.Down(0x10)&&Native.Down(0x77);if(key&&!keyWasDown)report.Markers.Add(timer.Elapsed.TotalSeconds);keyWasDown=key;
                await Task.Delay(50);
            }
        }finally{
            report.DurationSeconds=timer.Elapsed.TotalSeconds;
            foreach(var command in controls.Values){command.Dispose();}Console.CancelKeyPress-=handler;
            if(pm is not null){if(!pm.HasExited&&cancelled){var stopInfo=new ProcessStartInfo(Path.GetFullPath(pmPath)){UseShellExecute=false,CreateNoWindow=true};foreach(var arg in new[]{"--session_name","BetterGPSDiagnostics-"+game.Id+"-"+token,"--terminate_existing_session"})stopInfo.ArgumentList.Add(arg);using var stopper=Process.Start(stopInfo);if(stopper is not null&&!await Task.Run(()=>stopper.WaitForExit(3000)))stopper.Kill(true);}if(!pm.HasExited){if(!await Task.Run(()=>pm.WaitForExit(3000)))pm.Kill(true);}report.PresentMonExitCode=pm.HasExited?pm.ExitCode:null;pm.Dispose();}
        }
        if(report.PresentMonExitCode is {} code&&code!=0)report.Warnings.Add("PresentMon a échoué (code "+code+"). Vérifier les droits ETW et sa version CLI.");
        foreach(var peer in peers.Values){report.Timings.AddRange(peer.Summaries());report.LostEvents+=peer.Lost;report.Components.Add($"PID {peer.Pid}, composant {peer.Component}");peer.Dispose();}
        var groups=events.Where(e=>e.Component==1&&e.Metric==0).GroupBy(e=>e.Value&~3UL).OrderByDescending(g=>g.Count()).ToList();
        report.SwapChainCount=groups.Count;
        var frames=groups.FirstOrDefault()?.Where(e=>(e.Value&1)!=0).Select(e=>new FramePoint(e.Seconds,e.Milliseconds,(e.Value&2)!=0)).ToList()??[];
        if(groups.Count>1)report.Warnings.Add("Plusieurs swapchains : le rapport utilise celle ayant produit le plus de frames.");
        report.ExcludedUnfocusedFrames=(groups.FirstOrDefault()?.Count()??0)-frames.Count;
        var correlationFrames=frames;
        var pmCsv=Path.Combine(directory,"presentmon.csv");if(File.Exists(pmCsv)){try{var imported=ReadPresentMon(pmCsv,started);if(imported.Count>0){frames=imported;report.FrameSource="PresentMon CSV (non filtré selon le focus)";}}catch(InvalidDataException e){report.Warnings.Add(e.Message);}}
        report.FrameTimes=frames;report.Frames=SummarizeFrames(frames.Select(f=>f.Ms).ToList());
        if(frames.Count==0)report.Warnings.Add("Aucune frame mesurée. Sans BetterGPS actif, fournir un CSV PresentMon pour comparer les FPS.");
        if(report.LostEvents>0||report.TruncatedEvents>0)report.Warnings.Add("Des événements manquent : les corrélations et percentiles de frames sont incomplets. Les compteurs agrégés internes restent disponibles.");
        var spanEvents=events.Where(e=>e.Metric!=0&&e.Metric!=1&&(e.Metric<21||e.Metric==27)).ToList();
        foreach(var frame in correlationFrames.OrderByDescending(f=>f.Ms).Take(40)){
            var overlap=spanEvents.Where(e=>e.Seconds<=frame.Seconds&&e.Seconds+e.Milliseconds/1000>=frame.Seconds-frame.Ms/1000).OrderByDescending(e=>e.Milliseconds).Take(8).Select(e=>$"{Name(e.Metric)} : {e.Milliseconds:F3} ms (PID {e.Pid}, thread {e.Thread})").ToList();
            report.Spikes.Add(new Spike(frame.Seconds,frame.Ms,overlap));
        }
        report.ManagedEvents=events.Where(e=>e.Component==3&&e.Metric>=21&&e.Metric<=26).ToList();
        Save(report,directory);Console.WriteLine("Terminé : "+Path.Combine(directory,"rapport.html"));return 0;
    }
    static Dictionary<string,string> Hashes(Process game){var result=new Dictionary<string,string>();try{var folder=Path.Combine(Path.GetDirectoryName(game.MainModule!.FileName)!,"plugins","BetterGPS");foreach(var relative in new[]{"BetterGPSOverlay.dll","BetterGPSProbe.dll","runtime/engine/BetterGPSNavigation.dll"}){var path=Path.Combine(folder,relative);if(File.Exists(path)){using var s=File.OpenRead(path);result[relative]=Convert.ToHexString(SHA256.HashData(s));}}}catch{}return result;}
    internal static string Name(int metric)=>metric>=0&&metric<Names.Length?Names[metric]:"Metric "+metric;
    internal static FrameStats SummarizeFrames(List<double> times){times=times.Where(t=>double.IsFinite(t)&&t>0).Order().ToList();if(times.Count==0)return new();double mean=times.Average();return new(){Count=times.Count,AverageFps=1000/mean,MeanMs=mean,P50Ms=Percentile(times,.5),P95Ms=Percentile(times,.95),P99Ms=Percentile(times,.99),P999Ms=Percentile(times,.999),MaxMs=times[^1],Low1PercentFps=1000/times.TakeLast(Math.Max(1,(int)Math.Ceiling(times.Count*.01))).Average(),Over33Ms=times.Count(t=>t>33.333),Over50Ms=times.Count(t=>t>50),Over100Ms=times.Count(t=>t>100)};}
    internal static double Percentile(List<double> sorted,double q)=>sorted.Count==0?0:sorted[Math.Clamp((int)Math.Ceiling(sorted.Count*q)-1,0,sorted.Count-1)];
    static List<string> Csv(string text){var cells=new List<string>();var b=new StringBuilder();bool quote=false;for(int i=0;i<text.Length;i++){char c=text[i];if(c=='"'){if(quote&&i+1<text.Length&&text[i+1]=='"'){b.Append('"');i++;}else quote=!quote;}else if(c==','&&!quote){cells.Add(b.ToString());b.Clear();}else b.Append(c);}cells.Add(b.ToString());return cells;}
    static List<FramePoint> ReadPresentMon(string file,long? captureOrigin=null)
    {
        using var reader=File.OpenText(file);var header=Csv(reader.ReadLine()??"");
        int index=header.IndexOf("MsBetweenPresents");if(index<0)index=header.IndexOf("FrameTime");
        int cpu=header.IndexOf("MsCPUBusy"),wait=header.IndexOf("MsCPUWait");
        if(index<0&&(cpu<0||wait<0))throw new InvalidDataException("CSV PresentMon : FrameTime / MsBetweenPresents / MsCPUBusy + MsCPUWait absents.");
        int qpc=header.IndexOf("CPUStartQPC"),time=header.IndexOf("CPUStartTime"),seconds=header.IndexOf("TimeInSeconds");
        int gpu=header.IndexOf("MsGPUBusy"),display=header.IndexOf("DisplayLatency"),app=header.IndexOf("Application"),chainIndex=header.IndexOf("SwapChainAddress");
        var rows=new Dictionary<string,List<FramePoint>>();double elapsed=0,origin=double.NaN;
        while(reader.ReadLine() is {} line){
            var cells=Csv(line);
            double? Number(int i)=>i>=0&&i<cells.Count&&double.TryParse(cells[i],NumberStyles.Float,CultureInfo.InvariantCulture,out var n)&&double.IsFinite(n)?n:null;
            if(app>=0&&app<cells.Count&&!cells[app].Equals("amtrucks.exe",StringComparison.OrdinalIgnoreCase))continue;
            double ms=Number(index)??((Number(cpu)??0)+(Number(wait)??0));if(ms<=0)continue;
            double t=elapsed+=ms/1000;
            if(Number(qpc) is {} raw){if(double.IsNaN(origin))origin=captureOrigin??raw;t=(raw-origin)/Stopwatch.Frequency;}
            else if(Number(time) is {} rawMs){if(double.IsNaN(origin))origin=rawMs;t=(rawMs-origin)/1000;}
            else if(Number(seconds) is {} rawS){if(double.IsNaN(origin))origin=rawS;t=rawS-origin;}
            string chain=chainIndex>=0&&chainIndex<cells.Count?cells[chainIndex]:"default";
            if(!rows.TryGetValue(chain,out var list))rows[chain]=list=[];
            if(list.Count<300000)list.Add(new(t,ms,false,Number(gpu),Number(display)));
        }
        return rows.Values.OrderByDescending(v=>v.Count).FirstOrDefault()??[];
    }
    static void Save(Report report,string directory){Directory.CreateDirectory(directory);File.WriteAllText(Path.Combine(directory,"rapport.json"),JsonSerializer.Serialize(report,Json));File.WriteAllText(Path.Combine(directory,"rapport.html"),Html(report));File.WriteAllText(Path.Combine(directory,"capture.owner"),"BetterGPSDiagnostics-v1");try{CaptureRetention.Trim(directory);}catch(IOException){}catch(UnauthorizedAccessException){}}
    static string E(string s)=>WebUtility.HtmlEncode(s);
    static string Shell(string title,string body)=>"<!doctype html><html lang='fr'><meta charset='utf-8'><title>"+E(title)+"</title><style>body{background:#171a20;color:#e7eaf0;font:16px system-ui;max-width:1250px;margin:32px auto;padding:0 20px}h1,h2{color:#eea64d}table{border-collapse:collapse;width:100%;margin:20px 0}td,th{padding:9px;border-bottom:1px solid #444;text-align:left}aside{background:#29313d;padding:16px}svg{width:100%;background:#232832}summary{cursor:pointer}small{color:#bcc5d5}</style><h1>"+E(title)+"</h1>"+body+"</html>";
    static string Html(Report r){var f=r.Frames;var body=new StringBuilder($"<p>{E(r.Label)} — {r.StartedUtc:O} — {r.DurationSeconds:F1} s</p><aside>{string.Join("<br>",r.Warnings.Select(E))}</aside><p>Source frames : {E(r.FrameSource)}. Événements perdus : {r.LostEvents}; exclus hors focus : {r.ExcludedUnfocusedFrames}.</p><h2>Fluidité</h2><p>FPS moyens : {f.AverageFps:F1} · 1% low : {f.Low1PercentFps:F1} · P99 : {f.P99Ms:F2} ms · Maximum : {f.MaxMs:F2} ms · &gt;50 ms : {f.Over50Ms} · &gt;100 ms : {f.Over100Ms}</p><small>1% low = 1000 / moyenne du 1% des intervalles les plus lents. Les chargements et pauses au premier plan restent inclus.</small>");
        if(r.FrameTimes.Count>0){double xmax=Math.Max(.001,r.FrameTimes.Max(x=>x.Seconds)),ymax=Math.Max(50,Math.Min(500,f.MaxMs));var bins=r.FrameTimes.GroupBy(x=>(int)(x.Seconds/xmax*1000)).Select(g=>g.MaxBy(x=>x.Ms)!);body.Append("<svg viewBox='0 0 1000 220' role='img' aria-label='Temps d’image, pics conservés'><polyline fill='none' stroke='#7ab5ff' stroke-width='1' points='");foreach(var x in bins)body.Append(CultureInfo.InvariantCulture,$"{x.Seconds/xmax*1000:F1},{210-Math.Min(ymax,x.Ms)/ymax*200:F1} ");body.Append("'/></svg>");}
        var gpuFrames=r.FrameTimes.Where(f=>f.GpuBusyMs.HasValue).ToList();
        if(gpuFrames.Count>0)body.Append($"<p>PresentMon GPU busy : moyenne {gpuFrames.Average(f=>f.GpuBusyMs!.Value):F2} ms / maximum {gpuFrames.Max(f=>f.GpuBusyMs!.Value):F2} ms. Données GPU du processus entier, pas uniquement BetterGPS.</p>");
        body.Append("<h2>Coûts internes</h2><small>Hooks de texture : 1 appel sur 64 mesuré, cumul limité aux échantillons. Temps muraux inclusifs ; la durée de Map inclut l’attente pilote. Present natif inclut VSync/limiteur : ce n’est pas du CPU BetterGPS. Histogrammes P99 : bornes supérieures, pas percentiles exacts.</small><table><tr><th>Étape</th><th>PID</th><th>Appels mesurés</th><th>Moyenne ms</th><th>P99 ≤ ms</th><th>Temps total ms</th></tr>");foreach(var t in r.Timings.OrderByDescending(t=>t.TotalMs))body.Append($"<tr><td>{E(t.Name)}</td><td>{t.Pid}</td><td>{t.Count}</td><td>{t.MeanMs:F4}</td><td>{(t.P99UpperMs is {} bound?bound.ToString("F3"):"&gt;4194") }</td><td>{t.TotalMs:F1}</td></tr>");body.Append("</table><h2>Ressources des processus</h2><small>CPU en équivalents cœurs, pas en % du PC. I/O = octets logiques Windows, pas lectures physiques du disque. Page faults = défauts totaux, pas uniquement défauts matériels.</small><table><tr><th>Processus / PID</th><th>CPU moyen / pic (cœurs)</th><th>RAM privée max MiB</th><th>I/O lecture / écriture MiB</th><th>Défauts de page</th></tr>");foreach(var g in r.Resources.GroupBy(s=>(s.Name,s.Pid)))body.Append($"<tr><td>{E(g.Key.Name)} / {g.Key.Pid}</td><td>{g.Average(s=>s.CpuCores):F2} / {g.Max(s=>s.CpuCores):F2}</td><td>{g.Max(s=>s.PrivateBytes)/1048576.0:F1}</td><td>{g.Sum(s=>(double)s.ReadDelta)/1048576.0:F1} / {g.Sum(s=>(double)s.WriteDelta)/1048576.0:F1}</td><td>{g.Sum(s=>s.FaultDelta)}</td></tr>");body.Append("</table><h2>Moteur .NET / GC</h2>");
        foreach(var process in r.ManagedEvents.GroupBy(e=>e.Pid)){
            ulong Delta(int metric){var points=process.Where(e=>e.Metric==metric).OrderBy(e=>e.Seconds).ToList();return points.Count>1&&points[^1].Value>=points[0].Value?points[^1].Value-points[0].Value:0;}
            var heap=process.Where(e=>e.Metric==21).Select(e=>e.Value).DefaultIfEmpty().Max();
            body.Append($"<p>PID {process.Key} : heap estimé max {heap/1048576.0:F1} MiB ; allocations entre échantillons {Delta(22)/1048576.0:F1} MiB ; GC 0/1/2 : {Delta(23)}/{Delta(24)}/{Delta(25)} ; pauses GC cumulées {Delta(26)/1000.0:F2} ms.</p>");
        }
        body.Append("<h2>Pics et corrélations</h2>");foreach(var s in r.Spikes.Take(20))body.Append($"<details><summary>{s.Seconds:F2}s — {s.Ms:F2} ms</summary><p>{(s.Overlapping.Count==0?"Aucune étape BetterGPS instrumentée observée dans cet intervalle. Cela n’exclut pas une pression mémoire ou disque indirecte.":string.Join("<br>",s.Overlapping.Select(E)))}</p></details>");body.Append($"<p>Marqueurs manuels : {E(string.Join(", ",r.Markers.Select(m=>m.ToString("F2")+"s")))}</p><p>Compteurs GC et mémoire gérée : {r.ManagedEvents.Count} points dans le JSON. Capture locale, aucun envoi réseau.</p>");return Shell("BetterGPS — rapport de performances",body.ToString());}
    static string HtmlCompare(Report a,Report b){if(a.Frames.Count==0||b.Frames.Count==0)return Shell("Comparaison incomplète","<aside>Une capture ne contient aucune frame. Importer un CSV PresentMon pour une session sans BetterGPS ; aucune différence de FPS ne peut être calculée.</aside>");string Row(string name,double x,double y)=>$"<tr><td>{name}</td><td>{x:F3}</td><td>{y:F3}</td><td>{y-x:+0.000;-0.000;0}</td></tr>";return Shell("BetterGPS — comparaison",$"<aside>Comparer la même sauvegarde, le même trajet, les mêmes réglages et une durée similaire. Répéter les essais. Une différence seule ne prouve pas la causalité. Sources : {E(a.FrameSource)} / {E(b.FrameSource)}.</aside><table><tr><th>Mesure</th><th>{E(a.Label)}</th><th>{E(b.Label)}</th><th>Différence</th></tr>"+Row("FPS moyens",a.Frames.AverageFps,b.Frames.AverageFps)+Row("1% low FPS",a.Frames.Low1PercentFps,b.Frames.Low1PercentFps)+Row("P99 ms",a.Frames.P99Ms,b.Frames.P99Ms)+Row("Pics >50 ms",a.Frames.Over50Ms,b.Frames.Over50Ms)+"</table>");}
    static void ExtraTests()
    {
        var root=Path.Combine(Path.GetTempPath(),"bettergps-diagnostics-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        var csv=Path.Combine(root,"v2.csv");File.WriteAllText(csv,"Application,SwapChainAddress,CPUStartTime,MsCPUBusy,MsCPUWait,MsGPUBusy\namtrucks.exe,0x1,1000,6,4,7\namtrucks.exe,0x1,2000,6,4,8\n");
        var frames=ReadPresentMon(csv);if(frames.Count!=2||frames[1].Seconds!=1||frames[0].Ms!=10||frames[1].GpuBusyMs!=8)throw new Exception("PresentMon v2 units / fallback / GPU invalid");
        var captures=Path.Combine(root,"captures");Directory.CreateDirectory(captures);string current="";
        for(int i=0;i<9;i++){current=Path.Combine(captures,$"20260101_00000{i}_000");Directory.CreateDirectory(current);File.WriteAllText(Path.Combine(current,"capture.owner"),"BetterGPSDiagnostics-v1");File.WriteAllText(Path.Combine(current,"rapport.html"),"fixture");}
        var foreign=Path.Combine(captures,"20260101_999999_000");Directory.CreateDirectory(foreign);File.WriteAllText(Path.Combine(foreign,"capture.owner"),"BetterGPSDiagnostics-v1");File.WriteAllText(Path.Combine(foreign,"personal.txt"),"preserve");
        CaptureRetention.Trim(current);if(Directory.GetDirectories(captures).Length!=9||!File.Exists(Path.Combine(foreign,"personal.txt")))throw new Exception("Capture retention deleted user data or kept too many captures");
        foreach(var file in Directory.EnumerateFiles(root,"*",SearchOption.AllDirectories))File.Delete(file);foreach(var dir in Directory.EnumerateDirectories(captures))Directory.Delete(dir);Directory.Delete(captures);Directory.Delete(root);
        Console.WriteLine("PASS: PresentMon v2 timestamp units, CPU/GPU fields, bounded capture retention and foreign file protection.");
    }
    static void SelfTest(){ExtraTests();var stats=SummarizeFrames(Enumerable.Repeat(10d,99).Append(100d).ToList());if(stats.Count!=100||stats.P99Ms!=10||stats.MaxMs!=100||stats.Low1PercentFps!=10||stats.Over50Ms!=1)throw new Exception("Percentiles / 1% low invalides");if(Csv("\"a,b\",\"a\"\"b\",3").Count!=3)throw new Exception("CSV quotes");var empty=SummarizeFrames([]);if(empty.Count!=0||!double.IsFinite(empty.AverageFps))throw new Exception("Empty capture");Console.WriteLine("PASS: nearest-rank percentiles, mean-slowest-1% low, spikes, empty captures, quoted CSV.");}
}

sealed class Command:IDisposable
{
    readonly MemoryMappedFile file;readonly MemoryMappedViewAccessor view;
    internal Command(int pid,long token){file=MemoryMappedFile.CreateOrOpen($"Local\\BetterGPS-Diagnostics-Control-{pid}",4096);view=file.CreateViewAccessor();view.Write(4,1);view.Write(16,Stopwatch.Frequency);view.Write(32,token);view.Write(0,0x42475043);Heartbeat();}
    internal void Heartbeat(){view.Write(8,Stopwatch.GetTimestamp());view.Write(24,1);}
    public void Dispose(){view.Write(24,0);view.Dispose();file.Dispose();}
}
sealed class Peer:IDisposable
{
    readonly MemoryMappedFile file;readonly MemoryMappedViewAccessor view;readonly long[] counts=new long[32],ticks=new long[32];readonly long[,] hist=new long[32,24];long sequence;readonly long frequency;
    internal int Pid,Component;internal long Lost;
    Peer(int pid,int component,MemoryMappedFile file){Pid=pid;Component=component;this.file=file;view=file.CreateViewAccessor(0,65536,MemoryMappedFileAccess.Read);if(view.ReadInt32(0)!=0x42475044||view.ReadInt32(4)!=1)throw new InvalidDataException("Version IPC inconnue");frequency=view.ReadInt64(16);if(frequency!=Stopwatch.Frequency)throw new InvalidDataException("Fréquence QPC incompatible");sequence=view.ReadInt64(24);for(int m=0;m<32;m++){counts[m]=view.ReadInt64(128+m*216);ticks[m]=view.ReadInt64(136+m*216);for(int b=0;b<24;b++)hist[m,b]=view.ReadInt64(152+m*216+b*8);}}
    internal static Peer? TryOpen(int pid,int component){try{return new(pid,component,MemoryMappedFile.OpenExisting($"Local\\BetterGPS-Diagnostics-{pid}-{component}",MemoryMappedFileRights.Read));}catch(Exception e) when(e is FileNotFoundException or IOException or UnauthorizedAccessException){return null;}}
    internal List<NativeEvent> Read(long start){var result=new List<NativeEvent>();var end=view.ReadInt64(24);if(end-sequence>1024){Lost+=end-sequence-1024;sequence=end-1024;}while(sequence<end){long ticket=sequence+1,at=7040+(ticket-1)%1024*40;long before=view.ReadInt64(at);if(before!=ticket)break;long stamp=view.ReadInt64(at+8),duration=view.ReadInt64(at+16);int metric=view.ReadInt32(at+24),thread=view.ReadInt32(at+28);ulong value=view.ReadUInt64(at+32);Thread.MemoryBarrier();if(view.ReadInt64(at)!=ticket)break;sequence=ticket;if(stamp>=start)result.Add(new(Pid,Component,metric,thread,(stamp-start)/(double)frequency,duration*1000d/frequency,value));}return result;}
    internal List<Timing> Summaries(){var result=new List<Timing>();for(int m=1;m<21;m++){long count=view.ReadInt64(128+m*216)-counts[m],total=view.ReadInt64(136+m*216)-ticks[m];if(count<=0)continue;long cumulative=0,target=(long)Math.Ceiling(count*.99);double? upper=0;for(int b=0;b<24;b++){cumulative+=Math.Max(0,view.ReadInt64(152+m*216+b*8)-hist[m,b]);if(cumulative>=target){upper=b==23?null:Math.Pow(2,b)/1000;break;}}result.Add(new(DiagnosticTool.Name(m),Pid,Component,count,total*1000d/frequency/count,total*1000d/frequency,upper,m==3));}return result;}
    public void Dispose(){view.Dispose();file.Dispose();}
}
sealed class ProcessMonitor(int pid,string name)
{
    double lastTime,lastCpu;ulong lastRead,lastWrite;uint lastFault;bool primed;
    internal ResourceSample? Take(double seconds){using var p=Process.GetProcessById(pid);p.Refresh();double cpu=p.TotalProcessorTime.TotalSeconds;Native.GetProcessIoCounters(p.Handle,out var io);var mem=new Native.Memory{Size=(uint)Marshal.SizeOf<Native.Memory>()};Native.GetProcessMemoryInfo(p.Handle,ref mem,mem.Size);var result=primed?new ResourceSample(seconds,pid,name,Math.Max(0,(cpu-lastCpu)/Math.Max(.001,seconds-lastTime)),p.PrivateMemorySize64,p.WorkingSet64,io.ReadBytes>=lastRead?io.ReadBytes-lastRead:0,io.WriteBytes>=lastWrite?io.WriteBytes-lastWrite:0,mem.Faults>=lastFault?mem.Faults-lastFault:0):null;lastTime=seconds;lastCpu=cpu;lastRead=io.ReadBytes;lastWrite=io.WriteBytes;lastFault=mem.Faults;primed=true;return result;}
}
static class Native
{
    [StructLayout(LayoutKind.Sequential)]internal struct Io{public ulong Reads,Writes,Others,ReadBytes,WriteBytes,OtherBytes;}
    [StructLayout(LayoutKind.Sequential)]internal struct Memory{public uint Size,Faults;public nuint PeakWorking,Working,PeakPaged,Paged,PeakNonPaged,NonPaged,Pagefile,PeakPagefile,Private;}
    [DllImport("kernel32.dll")]internal static extern bool GetProcessIoCounters(nint process,out Io counters);
    [DllImport("psapi.dll")]internal static extern bool GetProcessMemoryInfo(nint process,ref Memory counters,uint size);
    [DllImport("user32.dll")]static extern short GetAsyncKeyState(int key);
    internal static bool Down(int key)=>(GetAsyncKeyState(key)&0x8000)!=0;
}
sealed class Report
{
    public string Label{get;set;}="";public DateTime StartedUtc{get;set;}public double DurationSeconds{get;set;}public string FrameSource{get;set;}="BetterGPS CPU Present cadence (focused, dominant swapchain)";
    public FrameStats Frames{get;set;}=new();public List<FramePoint> FrameTimes{get;set;}=[];public List<Timing> Timings{get;set;}=[];public List<ResourceSample> Resources{get;set;}=[];public List<Spike> Spikes{get;set;}=[];public List<string> Warnings{get;set;}=[];public List<string> Components{get;set;}=[];public Dictionary<string,string> BinaryHashes{get;set;}=[];public List<double> Markers{get;set;}=[];public List<NativeEvent> ManagedEvents{get;set;}=[];public long LostEvents{get;set;}public long TruncatedEvents{get;set;}public bool RawLogTruncated{get;set;}public int ExcludedUnfocusedFrames{get;set;}public int SwapChainCount{get;set;}public int? PresentMonExitCode{get;set;}
}
sealed class FrameStats{public int Count{get;set;}public double AverageFps{get;set;}public double Low1PercentFps{get;set;}public double MeanMs{get;set;}public double P50Ms{get;set;}public double P95Ms{get;set;}public double P99Ms{get;set;}public double P999Ms{get;set;}public double MaxMs{get;set;}public int Over33Ms{get;set;}public int Over50Ms{get;set;}public int Over100Ms{get;set;}}
record FramePoint(double Seconds,double Ms,bool Menu,double? GpuBusyMs=null,double? DisplayLatencyMs=null);
record Timing(string Name,int Pid,int Component,long Count,double MeanMs,double TotalMs,double? P99UpperMs,bool Sampled);
record NativeEvent(int Pid,int Component,int Metric,int Thread,double Seconds,double Milliseconds,ulong Value);
record ResourceSample(double Seconds,int Pid,string Name,double CpuCores,long PrivateBytes,long WorkingBytes,ulong ReadDelta,ulong WriteDelta,uint FaultDelta);
record Spike(double Seconds,double Ms,List<string> Overlapping);
