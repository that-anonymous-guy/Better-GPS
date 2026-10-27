using BetterGPSNavigation;
using System.Diagnostics;
using System.Text;

internal static class PhoneIntegrationTests
{
    internal static void Run(string native,string output)
    {
        Directory.CreateDirectory(output);
        var g=new LiveGuidance(1,"APPROACH","phone-test",1,"right","TURN_RIGHT",137.16,137.16/1609.344,"Navy Dr",null,null,null,"TURN_LEFT",100,[],0,0,"")
        {TargetRoad=new("RESOLVED",[new("Navy Dr","STREET","","HIGH","UNKNOWN",[])],[],"test")};
        var bytes=PhoneGuidancePublisher.Encode(g,DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
        BitConverter.GetBytes(2L).CopyTo(bytes,8);
        void Check(bool ok,string why){if(!ok)throw new Exception(why);}
        string Text(int start,int count)=>Encoding.UTF8.GetString(bytes,start,count).TrimEnd('\0');
        Check(bytes.Length==4096&&Text(64,256)=="TURN_RIGHT"&&Text(576,256)=="Navy Dr", "Phone lost the selected destination street");
        Check(Text(2368,512)==NativeBridgeCli.Describe(g),"Phone and existing guidance disagree");
        var ambiguous=g with{TargetRoad=new("AMBIGUOUS",[],[],"conflict"),Road=null};
        var unknown=Encoding.UTF8.GetString(PhoneGuidancePublisher.Encode(ambiguous,0),2368,512).TrimEnd('\0');
        Check(!unknown.Contains("Navy Dr"),"Phone invented a road on ambiguous evidence");
        var arrived=PhoneGuidancePublisher.Encode(g with{Instruction=null,Status="COMPLETE"},0);
        Check(Encoding.UTF8.GetString(arrived,2368,512).TrimEnd('\0')=="You have arrived"&&BitConverter.ToUInt32(arrived,24)==0,"Phone missed the engine's arrival status");
        var longText=PhoneGuidancePublisher.Encode(g with{Destination=new string('é',600)},0);
        var strict=new UTF8Encoding(false,true);strict.GetString(longText,832,256);strict.GetString(longText,2368,512);
        Check(longText[832+255]==0&&longText[2368+511]==0,"Long Unicode fields escaped wire boundaries");
        string fixture=Path.Combine(Path.GetFullPath(output),"guidance-wire.bin");File.WriteAllBytes(fixture,bytes);
        if(OperatingSystem.IsWindows())
        {
            using var publisher=new PhoneGuidancePublisher();publisher.Publish(g);
            using var mapping=System.IO.MemoryMappedFiles.MemoryMappedFile.OpenExisting(PhoneGuidancePublisher.MappingName);
            using var view=mapping.CreateViewAccessor();var published=new byte[4096];view.ReadArray(0,published,0,published.Length);
            Check(BitConverter.ToInt64(published,8)>0&&BitConverter.ToInt64(published,8)%2==0,"Publisher left an incomplete snapshot");
            Check(Encoding.UTF8.GetString(published,2368,512).TrimEnd('\0')==NativeBridgeCli.Describe(g),"Actual mapped publisher lost guidance");
            var paused=g with{Status="PAUSED",Instruction=null,DistanceM=null};
            publisher.Publish(paused);view.ReadArray(0,published,0,published.Length);
            Check(Encoding.UTF8.GetString(published,64,256).TrimEnd('\0')==g.Instruction&&BitConverter.ToDouble(published,32)==g.DistanceM,"Pause replaced last visible guidance");
            publisher.Publish(g with{Status="NO_ROUTE",Instruction=null});publisher.Publish(paused);view.ReadArray(0,published,0,published.Length);
            Check(BitConverter.ToUInt32(published,24)==0,"Pause resurrected a cleared route");
            publisher.Publish(g);publisher.Publish(paused with{Revision=g.Revision+1});view.ReadArray(0,published,0,published.Length);
            Check(BitConverter.ToUInt32(published,24)==0,"Pause resurrected another route revision");
        }
        using var process=Process.Start(new ProcessStartInfo(Path.GetFullPath(native)){ArgumentList={fixture},UseShellExecute=false})!;
        if(!process.WaitForExit(20000)){process.Kill(true);throw new Exception("Native phone wire test timed out");}
        Check(process.ExitCode==0,"C# / C++ phone integration failed");
        Console.WriteLine("PASS: one guidance formatter, accurate road/then fields, ambiguity handling, bounded Unicode, native wire.");
    }
}
