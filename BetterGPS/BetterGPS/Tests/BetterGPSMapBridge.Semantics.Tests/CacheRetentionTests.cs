using BetterGPSNavigation;
internal static class CacheRetentionTests
{
    internal static void Run(string output)
    {
        var root=Path.Combine(output,Guid.NewGuid().ToString("N"));var markers=Path.Combine(root,"maintenance-v1");
        Directory.CreateDirectory(markers);var now=DateTime.UtcNow;
        string Key(int i)=>i.ToString("X64");
        for(int i=0;i<6;i++)
        {
            var key=Key(i);File.WriteAllText(Path.Combine(markers,key+".lease"),"");
            File.SetLastWriteTimeUtc(Path.Combine(markers,key+".lease"),now.AddDays(-40+i));
            File.WriteAllText(Path.Combine(root,key+"-fixture.json"),"cache");
            var folder=Path.Combine(root,"world-v1","map",key);Directory.CreateDirectory(folder);
            File.WriteAllText(Path.Combine(folder,"0_0.wgc"),"cache");
        }
        File.WriteAllText(Path.Combine(root,"historical-recording.json"),"keep");
        using(var active=new FileStream(Path.Combine(markers,Key(0)+".lease"),FileMode.Open,FileAccess.ReadWrite,FileShare.ReadWrite))
        {
            if(CacheMaintenance.Prune(root,Key(5),now)!=2)throw new Exception("Retention did not exclude active namespaces.");
            foreach(int i in new[]{0,3,4,5})if(!File.Exists(Path.Combine(root,Key(i)+"-fixture.json")))throw new Exception("Active/recent cache was removed.");
            foreach(int i in new[]{1,2})if(Directory.Exists(Path.Combine(root,"world-v1","map",Key(i))))throw new Exception("Expired cache remained.");
        }
        if(!File.Exists(Path.Combine(root,"historical-recording.json")))throw new Exception("Recording was removed.");
        Console.WriteLine("PASS: retention removes expired registered versions, retains three newest and active namespaces, preserves recordings.");
    }
}
