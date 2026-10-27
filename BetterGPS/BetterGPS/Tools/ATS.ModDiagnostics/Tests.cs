using System.IO.Compression;
using System.Text;

static class Tests
{
    internal static async Task Integration(string fixture,string output)
    {
        output=Path.GetFullPath(output);Directory.CreateDirectory(output);string zip=Path.Combine(output,"diagnostic-fixture.scs");
        using(var file=File.Create(zip))using(var archive=new ZipArchive(file,ZipArchiveMode.Create))
        {using var stream=archive.CreateEntry("vehicle/test.dds",CompressionLevel.NoCompression).Open();var bytes=new byte[1024*1024];new Random(42).NextBytes(bytes);stream.Write(bytes);}
        File.WriteAllText(Path.Combine(output,"game.log.txt"),$"[fs] device {zip} mounted to mod pool.\n");
        var start=new System.Diagnostics.ProcessStartInfo(Path.GetFullPath(fixture)){UseShellExecute=false,CreateNoWindow=true};start.ArgumentList.Add(zip);start.ArgumentList.Add("18");
        using var process=System.Diagnostics.Process.Start(start)??throw new Exception("Fixture did not start");
        try
        {
            await Task.Delay(700); // The fixture file is already open when tracing starts.
            var data=await Capture.Run(process,12,output,true,true);Analysis.Save(data,output);File.WriteAllText(Path.Combine(output,"capture.json"),System.Text.Json.JsonSerializer.Serialize(data,App.Json));
            var findings=Analysis.Build(data);
            if(data.Reads.Count==0||data.Presents.Count<100||findings.MaxMs is not >50||!findings.Suspects.Any(s=>s.Category=="Mod"&&s.Assets.Any(a=>a.Name=="vehicle/test.dds")))throw new Exception($"ETW integration incomplete: reads={data.Reads.Count}, presents={data.Presents.Count}, max={findings.MaxMs}");
            if(!data.DeepCapture||data.CpuSamples.Count==0||data.Stacks.Count==0||data.Threads.Count==0)throw new Exception($"Deep capture incomplete: samples={data.CpuSamples.Count}, stacks={data.Stacks.Count}, threads={data.Threads.Count}");
            Console.WriteLine($"PASS: real kernel file reads, DXGI presents, deliberate stutter and ZIP asset attribution; reads={data.Reads.Count}, presents={data.Presents.Count}, lost={data.LostEvents}.");
        }
        finally{if(!process.HasExited)process.Kill();}
    }
    internal static void Run()
    {
        string root=Path.Combine(Path.GetTempPath(),"ATSModDiagnostics-test-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);int assertions=0;
        void Check(bool value,string why){assertions++;if(!value)throw new Exception(why);}
        var gpu=GpuMonitor.Parse("0, NVIDIA Test, 99, 1024, 2048, 65, 150.25, 2500",2);
        Check(gpu is {Utilization:99,UsedMiB:1024,TotalMiB:2048,Watts:150.25,GraphicsMHz:2500},"NVIDIA telemetry parse failed");
        Check(GpuMonitor.Parse("0, NVIDIA Test, N/A, N/A, 2048, 65, N/A, N/A",2)?.Utilization is null,"Missing GPU utilization became zero");
        Check(GpuMonitor.Parse("bad output",0) is null,"Malformed NVIDIA line accepted");
        Check(DeepAnalysis.Signal(99,.2,1000,2000).Contains("GPU très occupé"),"GPU pressure not classified");
        Check(DeepAnalysis.Signal(50,.95,1000,2000).Contains("piste CPU"),"Hot thread hidden by low aggregate CPU");
        Check(DeepAnalysis.Signal(null,null,null,null).Contains("non déterminée"),"Missing hardware data invented a bottleneck");
        var deep=new CaptureData{IncludeUnfocused=true,Modules=[new("C:\\game\\BetterGPSOverlay.dll",0x1000,0x1000)],CpuSamples=[new(.5,0x1500,42)],Gpu=[new(.5,0,"GPU",99,1000,2000,50,100,2000)],Threads=[new(.5,42,.5)]};
        var windows=DeepAnalysis.Windows(deep,Enumerable.Range(1,100).Select(i=>new Frame(i*.02,20)).ToList());
        Check(windows[0].Fps==50&&windows[0].GpuPercent==99&&windows[0].Cpu.Single().Name=="BetterGPSOverlay.dll","Slow window lost GPU or injected module");
        Check(DeepAnalysis.Html(deep,windows).Contains("BetterGPSOverlay.dll"),"Deep report missing module");
        var zip=Path.Combine(root,"mod 'test'&.scs");
        using(var file=File.Create(zip))using(var archive=new ZipArchive(file,ZipArchiveMode.Create))
        {
            using(var stream=archive.CreateEntry("vehicle/échantillon.dds",CompressionLevel.NoCompression).Open())stream.Write(new byte[4096]);
            using(var stream=archive.CreateEntry("model/test.pmg",CompressionLevel.NoCompression).Open())stream.Write(new byte[2048]);
        }
        var ranges=Inventory.ZipIndex(zip);Check(ranges.Count==2,"ZIP entries missing");Check(ranges[0].Name=="vehicle/échantillon.dds","ZIP Unicode changed");
        Check(Inventory.Overlaps(ranges,ranges[0].Start+100,128).Single().Name==ranges[0].Name,"Wrong offset entry");
        Check(Inventory.Overlaps(ranges,ranges[0].End-16,32).Count()==2,"Read crossing entries lost an overlap");Check(!Inventory.Overlaps(ranges,ranges[^1].End,10).Any(),"Central directory blamed on an asset");
        var truncated=Path.Combine(root,"bad.scs");File.WriteAllText(truncated,"SCS# not zip");try{Inventory.ZipIndex(truncated);throw new Exception("Bad archive accepted");}catch(InvalidDataException){assertions++;}
        string log=$"[fs] device {zip} mounted to mod pool.\n[fs] device {root} mounted to scs pool.\n<ERROR> missing /model/test.pmg";
        var mounts=Inventory.Mounts(log);Check(mounts.Count==2,"Mounts lost");Check(Inventory.Owner(mounts,Inventory.Normalize(zip))?.Pool=="mod","Exact archive ownership lost");
        Check(Inventory.Owner([new("C:\\mods\\a","mod",0,"a",true)],"C:\\mods\\another\\file.dds")is null,"Directory prefix matched unrelated mod");
        var data=new CaptureData{StartedUtc=DateTime.UtcNow,Seconds=3,Mounts=mounts,IncludeUnfocused=true};
        data.Presents=[new(0,1),new(.01,1),new(.02,1),new(.1,1),new(.11,1),new(.12,1),new(0,2)];data.Reads=[new(.08,Inventory.Normalize(zip),ranges[0].Start+100,512)];data.LogIssues=Inventory.Issues(log);
        var result=Analysis.Build(data);Check(result.Frames==5,"Dominant swapchain not used");Check(result.MaxMs is >79 and <81,"Stutter not detected");
        Check(result.Suspects.Single().Category=="Mod"&&result.Suspects.Single().SpikeIds.Count==1,"Archive not correlated with spike");
        Check(result.Suspects.Single().Assets.Single().Name==ranges[0].Name,"Asset offset not attributed");Check(result.Confidence=="CORRELATION_ONLY","Correlation mislabeled as proof");
        data.Presents.Clear();var empty=Analysis.Build(data,false);Check(empty.AverageFps is null&&empty.Low1PercentFps is null,"FPS invented without frames");
        data.IncludeUnfocused=false;data.Presents=[new(0,1),new(.01,1),new(.02,1),new(.5,1),new(.51,1)];data.Focus=[new(0,true),new(.03,false),new(.49,true)];
        Check(Analysis.Frames(data).Count==3,"Focus transition created a fake stutter");
        data.Markers=[.1];Analysis.Save(data,root);var html=File.ReadAllText(Path.Combine(root,"rapport.html"));Check(!html.Contains("mod 'test'&"),"HTML filename not escaped");
        var compressed=Path.Combine(root,"capture.json.gz");App.SaveCapture(data,compressed);var roundtrip=App.ReadCapture(compressed);
        Check(roundtrip.Reads.SequenceEqual(data.Reads)&&roundtrip.Presents.SequenceEqual(data.Presents),"Compressed capture changed event data");
        Analysis.Compare(compressed,compressed,Path.Combine(root,"compare.html"));Check(File.Exists(Path.Combine(root,"compare.html")),"A/B comparison failed");
        var retention=Path.Combine(root,"retention");Directory.CreateDirectory(retention);var active=Path.Combine(retention,"active");
        foreach(var name in new[]{"old1","old2","active","foreign"})
        {
            var dir=Path.Combine(retention,name);Directory.CreateDirectory(dir);File.WriteAllText(Path.Combine(dir,"capture.owner"),"ATSModDiagnostics:1");File.WriteAllText(Path.Combine(dir,"rapport.html"),"data");
            if(name=="foreign")File.WriteAllText(Path.Combine(dir,"user.txt"),"keep");
        }
        CaptureRetention.Apply(retention,active,1,1024*1024);
        Check(Directory.Exists(active)&&Directory.Exists(Path.Combine(retention,"foreign")),"Retention removed active capture or user files");
        Check(!Directory.Exists(Path.Combine(retention,"old1"))&&!Directory.Exists(Path.Combine(retention,"old2")),"Retention failed to remove old owned captures");
        var zip64=Path.Combine(root,"zip64.scs");var original=File.ReadAllBytes(zip);int oldEnd=original.Length-22;var end=original.AsSpan(oldEnd,22).ToArray();
        using(var file=File.Create(zip64))using(var writer=new BinaryWriter(file))
        {
            writer.Write(original,0,oldEnd);writer.Write(0x06064b50u);writer.Write(44UL);writer.Write((ushort)45);writer.Write((ushort)45);writer.Write(0u);writer.Write(0u);writer.Write(2UL);writer.Write(2UL);
            writer.Write((ulong)System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(end.AsSpan(12)));writer.Write((ulong)System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(end.AsSpan(16)));
            writer.Write(0x07064b50u);writer.Write(0u);writer.Write((ulong)oldEnd);writer.Write(1u);
            System.Buffers.Binary.BinaryPrimitives.WriteUInt16LittleEndian(end.AsSpan(8),ushort.MaxValue);System.Buffers.Binary.BinaryPrimitives.WriteUInt16LittleEndian(end.AsSpan(10),ushort.MaxValue);System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(end.AsSpan(16),uint.MaxValue);writer.Write(end);
        }
        Check(Inventory.ZipIndex(zip64).SequenceEqual(ranges),"ZIP64 index changed physical ranges");
        var logRoot=Path.Combine(root,"large-log");Directory.CreateDirectory(logRoot);
        using(var file=File.Create(Path.Combine(logRoot,"game.log.txt")))
        {file.Write(Encoding.UTF8.GetBytes(log+"\n"));var padding=new byte[1024*1024];for(int i=0;i<17;i++)file.Write(padding);file.Write(Encoding.UTF8.GetBytes("\n<ERROR> latest error\n"));}
        var sampledLog=Inventory.ReadLog(logRoot);Check(Inventory.Mounts(sampledLog).Count==2&&sampledLog.Contains("latest error")&&sampledLog.Contains("middle log omitted"),"Large log lost startup mounts or latest error");
        Console.WriteLine($"PASS: {assertions} archive offsets, cross-entry reads, Unicode, malformed archive, mount boundaries, correlations, no-frame handling, focus and HTML assertions. Fixtures: {root}");
    }
}
