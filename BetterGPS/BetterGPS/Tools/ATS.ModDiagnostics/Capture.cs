using System.Diagnostics;
using System.Runtime.InteropServices;
using Microsoft.Diagnostics.Tracing;
using Microsoft.Diagnostics.Tracing.Parsers;
using Microsoft.Diagnostics.Tracing.Session;
using System.Text.RegularExpressions;

static class Capture
{
    internal static void CleanupStaleSessions()
    {
        foreach(var name in TraceEventSession.GetActiveSessionNames())
        {
            var match=Regex.Match(name,@"^ATSModDiagnostics-(\d+)-[0-9a-f]{32}$");if(!match.Success||!int.TryParse(match.Groups[1].Value,out int pid))continue;
            try{using var process=Process.GetProcessById(pid);continue;}catch(ArgumentException){}
            // Only our exact session prefix with a dead owner. Never cancel other profilers.
            using var stale=new TraceEventSession(name,TraceEventSessionOptions.Attach);stale.Stop();
        }
    }
    static readonly Guid Dxgi=new("CA11C036-0102-4A2D-A6AD-F03CFED5D3C9");
    [DllImport("user32.dll")] static extern nint GetForegroundWindow();
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(nint hwnd,out uint pid);
    [DllImport("user32.dll")] static extern short GetAsyncKeyState(int key);
    internal static async Task<CaptureData> Run(Process game,int seconds,string docs,bool cpuSamples,bool includeUnfocused)
    {
        var data=new CaptureData{Pid=game.Id,StartedUtc=DateTime.UtcNow,IncludeUnfocused=includeUnfocused,DeepCapture=cpuSamples};
        var initialLog=Inventory.ReadLog(docs);data.Mounts=Inventory.Mounts(initialLog);data.LogIssues=Inventory.Issues(initialLog);
        if(initialLog.Length==0)data.Warnings.Add("game.log.txt introuvable ou vide dans le dossier utilisateur choisi : attribution aux mods indisponible. Utiliser --documents si nécessaire.");
        if(data.Mounts.Count==0)data.Warnings.Add("Aucun montage trouvé dans le journal ; les chemins restent observés, mais leur propriétaire n'est pas deviné.");
        using var ownership=new Semaphore(1,1,"Local\\ATSModDiagnostics-"+game.Id);
        if(!ownership.WaitOne(0))throw new InvalidOperationException("Une capture de mods est déjà en cours pour ce jeu");
        using var gpu=new GpuMonitor();var gpuOffset=(DateTime.UtcNow-data.StartedUtc).TotalSeconds;gpu.Start();
        SnapshotModules(game,data);
        using var session=new TraceEventSession("ATSModDiagnostics-"+Environment.ProcessId+"-"+Guid.NewGuid().ToString("N"));
        session.StopOnDispose=true;session.BufferSizeMB=32;
        var keywords=KernelTraceEventParser.Keywords.FileIO|KernelTraceEventParser.Keywords.FileIOInit|KernelTraceEventParser.Keywords.DiskFileIO|KernelTraceEventParser.Keywords.Process|KernelTraceEventParser.Keywords.Thread;
        if(cpuSamples)keywords|=KernelTraceEventParser.Keywords.Profile|KernelTraceEventParser.Keywords.ImageLoad;
        session.EnableKernelProvider(keywords,cpuSamples?KernelTraceEventParser.Keywords.Profile:KernelTraceEventParser.Keywords.None);
        session.EnableProvider(Dxgi,TraceEventLevel.Verbose,0x8000000000000002UL);
        var source=session.Source;var gate=new object();var names=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        var pendingReads=new List<(double Seconds,ulong Key,long Offset,int Bytes,int ThreadId)>();
        var stacks=new Dictionary<(int Second,int Thread,string Frames),int>();
        var moduleNames=new Dictionary<ulong,string?>();var orderedModules=data.Modules.OrderBy(m=>unchecked((ulong)m.Base)).ToArray();
        string? ModuleName(ulong address)
        {
            if(moduleNames.TryGetValue(address,out var cached))return cached;
            int lo=0,hi=orderedModules.Length;while(lo<hi){int mid=(lo+hi)/2;if(unchecked((ulong)orderedModules[mid].Base)<=address)lo=mid+1;else hi=mid;}
            var m=lo>0?orderedModules[lo-1]:null;
            string? name=m is not null&&address-unchecked((ulong)m.Base)<(ulong)m.Size?Path.GetFileName(m.Path):null;
            if(moduleNames.Count<65536)moduleNames[address]=name;return name;
        }
        var wantedKeys=new HashSet<ulong>();var lateNames=new Dictionary<ulong,string?>();
        void RememberName(Microsoft.Diagnostics.Tracing.Parsers.Kernel.FileIONameTraceData item)
        {
            if(!wantedKeys.Contains(item.FileKey))return;
            var path=Inventory.Normalize(item.FileName);if(path.Length==0||path.Length>2048)return;
            if(lateNames.TryGetValue(item.FileKey,out var old)&&old!=path)lateNames[item.FileKey]=null;
            else if(!lateNames.ContainsKey(item.FileKey))lateNames[item.FileKey]=path;
        }
        source.Kernel.FileIOName+=RememberName;source.Kernel.FileIOFileCreate+=RememberName;source.Kernel.FileIOFileDelete+=RememberName;source.Kernel.FileIOFileRundown+=RememberName;
        double Time(DateTime timestamp)=>(timestamp.ToUniversalTime()-data.StartedUtc).TotalSeconds;
        source.Kernel.FileIORead+=e=>
        {
            if(e.ProcessID!=game.Id)return;
            data.TotalReadEvents++;
            if(data.Reads.Count+pendingReads.Count>=300000){data.DroppedReads++;return;}
            string path=Inventory.Normalize(e.FileName);
            if(path.Length==0)
            {
                data.UnnamedReads++;data.UnnamedReadBytes+=Math.Max(0,e.IoSize);
                pendingReads.Add((Time(e.TimeStamp),e.FileKey,unchecked((long)e.Offset),e.IoSize,e.ThreadID));if(e.FileKey!=0)wantedKeys.Add(e.FileKey);return;
            }
            lock(gate)
            {
                if(data.Reads.Count>=300000||path.Length>2048){data.DroppedReads++;return;}
                if(!names.TryGetValue(path,out var shared)){if(names.Count>=50000){data.DroppedReads++;return;}names[path]=shared=path;}
                data.Reads.Add(new(Time(e.TimeStamp),shared,unchecked((long)e.Offset),e.IoSize,e.ThreadID));
            }
        };
        if(cpuSamples)
        {
            source.Kernel.PerfInfoSample+=e=>{if(e.ProcessID==game.Id){if(data.CpuSamples.Count<750000)data.CpuSamples.Add(new(Time(e.TimeStamp),e.InstructionPointer,e.ThreadID));else data.DroppedCpuSamples++;}};
            source.Kernel.StackWalkStack+=e=>
            {
                if(e.ProcessID!=game.Id)return;
                var parts=new List<string>();
                for(int i=0;i<Math.Min(e.FrameCount,48);i++)
                {
                    string? name=ModuleName(e.InstructionPointer(i));if(name is null)continue;
                    if(parts.Count==0||parts[^1]!=name)parts.Add(name);
                }
                if(parts.Count==0)return;
                var key=((int)Time(e.TimeStamp),e.ThreadID,string.Join(" → ",parts.Take(12)));
                if(stacks.TryGetValue(key,out int count))stacks[key]=count+1;
                else if(stacks.Count<50000)stacks[key]=1;else data.DroppedCpuSamples++;
            };
        }
        source.Dynamic.All+=e=>
        {
            if(e.ProviderGuid!=Dxgi||e.ProcessID!=game.Id||(int)e.ID!=42)return;
            ulong chain=Payload(e,"SwapChain"),flags=Payload(e,"Flags");if((flags&1)!=0)return;
            lock(gate){if(data.Presents.Count>=100000)data.DroppedPresents++;else data.Presents.Add(new(Time(e.TimeStamp),chain));}
        };
        var processing=Task.Run(()=>source.Process());
        var clock=Stopwatch.StartNew();bool cancelled=false,keyWasDown=false;ConsoleCancelEventHandler cancel=(_,e)=>{e.Cancel=true;cancelled=true;};Console.CancelKeyPress+=cancel;
        var monitors=new Dictionary<int,(Process Process,double Cpu,double Seconds)>();
        var threadTimes=new Dictionary<int,(double Cpu,double Seconds)>();
        void Discover()
        {
            foreach(var process in Process.GetProcessesByName("BetterGPSNavigation").Append(Process.GetProcessById(game.Id)).Append(Process.GetCurrentProcess()))
            {
                if(monitors.ContainsKey(process.Id)){process.Dispose();continue;}
                try{monitors[process.Id]=(process,process.TotalProcessorTime.TotalSeconds,clock.Elapsed.TotalSeconds);}catch{process.Dispose();}
            }
        }
        Console.WriteLine($"Capture {seconds}s. Retourne dans ATS. Ctrl+Maj+F8 : marqueur ; Ctrl+C : arrêter. Aucun mod n'est modifié.");
        double nextResources=0;bool? lastFocus=null;
        try
        {
            Discover();
            while(!cancelled&&!game.HasExited&&clock.Elapsed.TotalSeconds<seconds)
            {
                if(processing.IsFaulted)await processing;
                var now=(DateTime.UtcNow-data.StartedUtc).TotalSeconds;
                GetWindowThreadProcessId(GetForegroundWindow(),out var foreground);bool focused=foreground==game.Id;
                if(lastFocus!=focused){data.Focus.Add(new(now,focused));lastFocus=focused;}
                bool key=GetAsyncKeyState(0x11)<0&&GetAsyncKeyState(0x10)<0&&GetAsyncKeyState(0x77)<0;
                if(key&&!keyWasDown){data.Markers.Add(now);Console.WriteLine($"Marqueur à {now:0.00}s");}keyWasDown=key;
                if(clock.Elapsed.TotalSeconds>=nextResources)
                {
                    nextResources=clock.Elapsed.TotalSeconds+.5;Discover();
                    foreach(var item in monitors.ToArray())try
                    {
                        var p=item.Value.Process;p.Refresh();double cpu=p.TotalProcessorTime.TotalSeconds,t=clock.Elapsed.TotalSeconds,dt=t-item.Value.Seconds;
                        data.Resources.Add(new(now,p.ProcessName,dt>0?Math.Max(0,(cpu-item.Value.Cpu)/dt):0,p.PrivateMemorySize64,p.WorkingSet64));monitors[item.Key]=(p,cpu,t);
                    }catch(InvalidOperationException){}catch(System.ComponentModel.Win32Exception){}
                    try
                    {
                        game.Refresh();
                        var busy=new List<ThreadReading>();var alive=new HashSet<int>();
                        foreach(ProcessThread thread in game.Threads)using(thread)try
                        {
                            int id=thread.Id;double cpu=thread.TotalProcessorTime.TotalSeconds;alive.Add(id);
                            if(threadTimes.TryGetValue(id,out var previous)&&now>previous.Seconds)busy.Add(new(now,id,Math.Clamp((cpu-previous.Cpu)/(now-previous.Seconds),0,1)));
                            threadTimes[id]=(cpu,now);
                        }catch(InvalidOperationException){}catch(System.ComponentModel.Win32Exception){}
                        foreach(int id in threadTimes.Keys.Where(id=>!alive.Contains(id)).ToArray())threadTimes.Remove(id);
                        data.Threads.AddRange(busy.OrderByDescending(v=>v.CpuCores).Take(5));
                    }catch(InvalidOperationException){}catch(System.ComponentModel.Win32Exception){}
                }
                await Task.Delay(25);
            }
        }
        finally
        {
            Console.CancelKeyPress-=cancel;data.Seconds=(DateTime.UtcNow-data.StartedUtc).TotalSeconds;
            session.Stop();await processing;data.LostEvents=source.EventsLost;
            await gpu.Stop();data.Gpu=gpu.Samples.Select(v=>v with{Seconds=v.Seconds+gpuOffset}).ToList();
            if(gpu.Warning is not null)data.Warnings.Add(gpu.Warning);
            foreach(var item in monitors.Values)item.Process.Dispose();ownership.Release();
        }
        foreach(var read in pendingReads)
        {
            if(!lateNames.TryGetValue(read.Key,out var path)||path is null)continue;
            if(!names.TryGetValue(path,out var shared)){if(names.Count>=50000)continue;names[path]=shared=path;}
            data.Reads.Add(new(read.Seconds,shared,read.Offset,read.Bytes,read.ThreadId));data.RecoveredReads++;data.UnnamedReads--;data.UnnamedReadBytes-=Math.Max(0,read.Bytes);
        }
        var finalLog=Inventory.ReadLog(docs);data.LogIssues=Inventory.Issues(finalLog);
        if(initialLog.Contains("[ATSModDiagnostics: middle log omitted]")||finalLog.Contains("[ATSModDiagnostics: middle log omitted]"))data.Warnings.Add("Journal >16 Mio : début (montages) et fin conservés, partie centrale omise. Au maximum 500 lignes d'erreurs/avertissements uniques sont présentées.");
        var finalMounts=Inventory.Mounts(finalLog);
        if(!data.Mounts.Select(m=>m.Path).SequenceEqual(finalMounts.Select(m=>m.Path),StringComparer.OrdinalIgnoreCase))data.Warnings.Add("Les ressources montées ont changé pendant la capture : utiliser les chemins physiques, pas une priorité supposée stable.");
        data.Mounts=data.Mounts.Concat(finalMounts).DistinctBy(m=>m.Path,StringComparer.OrdinalIgnoreCase).ToList();
        data.Reads.Sort((a,b)=>a.Seconds.CompareTo(b.Seconds));data.Presents.Sort((a,b)=>a.Seconds.CompareTo(b.Seconds));
        data.Stacks=stacks.Select(v=>new StackBucket(v.Key.Second,v.Key.Thread,v.Key.Frames,v.Value)).ToList();
        return data;
    }
    static ulong Payload(TraceEvent e,string fragment)
    {
        foreach(var name in e.PayloadNames)if(name.Contains(fragment,StringComparison.OrdinalIgnoreCase))try{return Convert.ToUInt64(e.PayloadByName(name));}catch{}
        return 0;
    }
    static void SnapshotModules(Process game,CaptureData data)
    {
        try{foreach(ProcessModule module in game.Modules)data.Modules.Add(new(module.FileName,module.BaseAddress.ToInt64(),module.ModuleMemorySize));}
        catch(System.ComponentModel.Win32Exception){data.Warnings.Add("Liste des modules CPU partielle : accès Windows refusé.");}
    }
}
