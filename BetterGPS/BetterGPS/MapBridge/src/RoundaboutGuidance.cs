using TruckLib.Models.Ppd;
namespace BetterGPSNavigation;

internal static class RoundaboutGuidance
{
    internal static LiveAction Annotate(LiveAction action,ManeuverFeatureResult feature,PrefabDescriptor descriptor)
    {
        // A loop at an ordinary junction is not sufficient evidence of a roundabout.
        if(!feature.PpdPath.Contains("roundabout",StringComparison.OrdinalIgnoreCase)||
            feature.SelectedCurveIds.Count<2||feature.ReachableExitCount<2)return action;
        var selected=feature.Branches.FirstOrDefault(b=>b.Selected&&b.CurveIds.SequenceEqual(feature.SelectedCurveIds));
        if(selected is null)return action;
        var ring=RingCurves(descriptor);
        int lastRing=selected.CurveIds.Select((id,index)=>(id,index)).Where(p=>ring.Contains(p.id)).Select(p=>p.index).DefaultIfEmpty(-1).Max();
        if(lastRing<0)return action;
        var paths=feature.Branches.Where(b=>b.StartCurveId==selected.StartCurveId)
            .Select(b=>(b.TargetPpdNodeIndex,b.CurveIds.ToArray())).ToArray();
        bool simpleRing=ring.All(id=>descriptor.NavCurves[id].NextLines.Count(ring.Contains)==1)&&
            descriptor.Nodes.All(n=>n.InputLines.Count(id=>id>=0&&id<descriptor.NavCurves.Count)<=1&&n.OutputLines.Count(id=>id>=0&&id<descriptor.NavCurves.Count)<=1);
        if(simpleRing)
        {
            var visited=new HashSet<int>();int cursor=ring.First();
            while(visited.Add(cursor))cursor=descriptor.NavCurves[cursor].NextLines.Single(ring.Contains);
            simpleRing=visited.Count==ring.Count; // Separate lane cycles cannot establish one ordered ring.
        }
        int? ordinal=!simpleRing||feature.ReachableExitIndices.Except(paths.Select(p=>p.TargetPpdNodeIndex)).Any()?null:
            ExitOrdinal(selected.CurveIds.ToArray(),feature.PpdExitIndex,paths,lastRing+1);
        var proofs=action.Proofs.Where(p=>p.Field!="action").Append(new LiveProof("action","ROUNDABOUT","HIGH","PPD",
            action.Proofs.First(p=>p.Field=="action").SourceId,action.BranchId,
            "Roundabout asset family and selected cyclic native lane path; exits ordered by shared lane divergence.")).ToArray();
        return action with{Instruction="ROUNDABOUT",RoundaboutExitNumber=ordinal,Proofs=proofs};
    }
    internal static int? ExitOrdinal(int[] selected,int selectedExit,(int Exit,int[] Curves)[] paths,int exitPivot)
    {
        if(selected.Length<2)return null;
        // Exits diverging at the last chosen exit junction are ahead of that exit,
        // rather than already passed. Multiple lane paths count as one physical arm.
        int Shared(int[] other){int i=0;while(i<selected.Length&&i<other.Length&&selected[i]==other[i])i++;return i;}
        var alternatives=paths.Where(p=>p.Exit!=selectedExit).Select(p=>(p.Exit,Prefix:Shared(p.Curves))).ToArray();
        if(alternatives.Any(p=>p.Prefix==0))return null;
        if(alternatives.GroupBy(p=>p.Exit).Any(g=>g.Select(p=>p.Prefix<exitPivot).Distinct().Count()>1))return null;
        return 1+alternatives.Where(p=>p.Prefix<exitPivot).Select(p=>p.Exit).Distinct().Count();
    }
    private static HashSet<int> RingCurves(PrefabDescriptor descriptor)
    {
        int n=descriptor.NavCurves.Count;var terminal=descriptor.Nodes.SelectMany(node=>node.OutputLines).ToHashSet();
        var next=Enumerable.Range(0,n).Select(i=>terminal.Contains(i)?[]:descriptor.NavCurves[i].NextLines.Where(j=>j>=0&&j<n).Distinct().ToArray()).ToArray();
        var previous=Enumerable.Range(0,n).Select(_=>new List<int>()).ToArray();
        for(int i=0;i<n;i++)foreach(var j in next[i])previous[j].Add(i);
        var incoming=previous.Select(p=>p.Count).ToArray();var outgoing=next.Select(p=>p.Length).ToArray();var removed=new bool[n];
        var queue=new Queue<int>(Enumerable.Range(0,n).Where(i=>incoming[i]==0||outgoing[i]==0));
        while(queue.TryDequeue(out var i))
        {
            if(removed[i])continue;removed[i]=true;
            foreach(var j in next[i])if(--incoming[j]==0)queue.Enqueue(j);
            foreach(var j in previous[i])if(--outgoing[j]==0)queue.Enqueue(j);
        }
        return Enumerable.Range(0,n).Where(i=>!removed[i]).ToHashSet();
    }
}
