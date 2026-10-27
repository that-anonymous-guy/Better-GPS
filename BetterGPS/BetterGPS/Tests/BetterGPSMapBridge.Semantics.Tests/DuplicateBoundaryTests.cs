using BetterGPSNavigation;
using System.Text;
using System.Text.Json;

internal static class DuplicateBoundaryTests
{
    internal static void Run(string capture,string output)
    {
        int count=0;void Check(bool ok,string why){count++;if(!ok)throw new Exception(why);}
        var frames=File.ReadLines(capture).Select(l=>NativeCaptureProtocol.Decode(Encoding.UTF8.GetBytes(l)))
            .Where(m=>m.Type=="RawRouteSnapshot").ToArray();
        foreach(var frame in frames)
        {
            var before=frame.Route!;var after=before.WithoutZeroDistanceDuplicates();
            NativeCaptureProtocol.Validate(frame with{Route=after});
            Check(after.Physical[0].TotalDistanceTillEndM==before.Physical[0].TotalDistanceTillEndM,"Departure navigation station changed");
            Check(after.Physical[^1]==before.Physical[^1],"Destination changed");
            Check(after.Logical.Where(p=>p.Type==1).SequenceEqual(before.Logical.Where(p=>p.Type==1)),"Native lane evidence changed");
            Check(after.Nodes!.Length==after.Physical.Length,"Native node evidence lost alignment");
            Check(after.WithoutZeroDistanceDuplicates()==after,"Normalization is not idempotent");
            Check(before.Physical.Length-after.Physical.Length==(frame.Generation==5?1:0),"Unexpected normalization in actual capture");
        }
        var a=new NativePhysical("0000000000000001",0,100,0,10);var b=new NativePhysical("0000000000000002",100,0,10,0);
        var la=new NativeLogical(0,uint.MaxValue,a.NodeUid,0,100,0,10);var lb=new NativeLogical(0,uint.MaxValue,b.NodeUid,100,0,10,0);
        var raw=new NativeRoute("0000000000000001",[a,a,b],[la,la,lb]);
        Check(raw.WithoutZeroDistanceDuplicates().Physical.Length==2,"Redundant marker retained");
        Check((raw with{Physical=[a,a,a,b],Logical=[la,la,la,lb]}).WithoutZeroDistanceDuplicates().Physical.Length==2,"Multiple duplicate markers retained");
        Check((raw with{Logical=[la,new(1,0,"0000000000000003",0,100,0,10),la,lb]}).WithoutZeroDistanceDuplicates().Physical.Length==3,"A true same-node prefab loop was erased");
        Check((raw with{Physical=[a,a with{DistanceFromPreviousM=20},b]}).WithoutZeroDistanceDuplicates().Physical.Length==3,"Nonzero physical travel erased");
        Check((raw with{Logical=[la,la with{DistanceFromPreviousM=20},lb]}).WithoutZeroDistanceDuplicates().Physical.Length==3,"Nonzero logical travel erased");
        Check((raw with{Physical=[a,a with{TotalDistanceTillEndM=90},b]}).WithoutZeroDistanceDuplicates().Physical.Length==3,"Different station erased");
        Directory.CreateDirectory(output);File.WriteAllText(Path.Combine(output,"duplicate-boundary-tests.json"),JsonSerializer.Serialize(new{status="PASS",count,capturedRoutes=frames.Length},LiveProtocol.Json));
        Console.WriteLine($"PASS: {count} duplicate-boundary and native loop preservation assertions.");
    }
}
