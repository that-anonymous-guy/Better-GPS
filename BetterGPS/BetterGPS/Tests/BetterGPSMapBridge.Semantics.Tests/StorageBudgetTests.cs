using BetterGPSNavigation;
internal static class StorageBudgetTests
{
    internal static void Run(string output)
    {
        string Root()=>Path.Combine(output,Guid.NewGuid().ToString("N"));
        string Cache(string root,int id)=>Path.Combine(root,id.ToString("X64")+"-route-fixture.json");
        void Check(bool test,string reason){if(!test)throw new Exception(reason);}
        var root=Root();Directory.CreateDirectory(root);StorageBudget.Register(root,1024);
        var old=Cache(root,1);Check(StorageBudget.Write(old,new byte[600]),"Initial cache not written");
        var newer=Cache(root,2);Check(StorageBudget.Write(newer,new byte[600]),"Cache eviction prevented new data");
        Check(!StorageBudget.Write(Cache(root,99),new byte[600],allowEviction:false)&&File.Exists(newer),"Background preparation evicted useful cached data");
        Check(!File.Exists(old)&&File.Exists(newer),"Old regenerable cache not evicted");
        Check(!StorageBudget.Write(Cache(root,3),new byte[2048]),"Oversized optional cache accepted");
        var proof=Path.Combine(root,"live-route-evidence",4.ToString("X64")+".json");
        Check(StorageBudget.Write(proof,new byte[400],true),"Session proof could not be written");
        Check(!StorageBudget.Write(Cache(root,5),new byte[800]),"Session proof deleted to make room");
        Check(File.Exists(proof),"Required active evidence was evicted");
        StorageBudget.RetainEvidence(root,[proof]);Check(File.Exists(proof),"Retained route proof removed");
        StorageBudget.RetainEvidence(root,[]);Check(!File.Exists(proof),"Cleared route proof accumulated");
        var unknown=Path.Combine(root,"user-note.txt");File.WriteAllBytes(unknown,new byte[700]);StorageBudget.Rescan(root);
        Check(!StorageBudget.Write(Cache(root,6),new byte[600])&&File.Exists(unknown),"Unknown user data removed by budget");
        var logs=Root();Directory.CreateDirectory(logs);
        using(var journal=new GuidanceJournal(logs,true))for(int i=0;i<100000;++i)
            journal.Write(new(i*1000L,"CRUISE","route",1,"action","CONTINUE",1000,1,"I-5","NORTH",null,null,null,null,[],1,1,new string('X',4096)));
        Check(new FileInfo(Path.Combine(logs,"guidance.jsonl")).Length<=8L*1024*1024,"Compact session log exceeded budget");
        Check(!File.Exists(Path.Combine(logs,"guidance-proofs.jsonl")),"Compact mode wrote repeated proof packets");
        Console.WriteLine("PASS: disk budget evicts regenerable cache, protects active evidence and user files, releases cleared evidence, bounds compact session logs.");
    }
}
