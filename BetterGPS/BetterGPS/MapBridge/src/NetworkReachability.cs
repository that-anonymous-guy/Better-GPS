namespace BetterGPSNavigation;

// Same shortest paths and immediate-dominator proof, using shared integer
// adjacency rather than hashing native identifiers at each traversal.
internal sealed class NetworkReachability
{
    private readonly int[] previous;
    private readonly int[] dominators;
    private readonly int start;
    public bool Complete {get;}
    public NetworkReachability(DirectedRoadNetworkGraph graph,string source,CancellationToken token=default)
    {
        start=graph.Ordinals[source];var count=graph.Portions.Length;
        previous=new int[count];dominators=new int[count];Array.Fill(previous,-1);Array.Fill(dominators,-1);
        var distance=new double[count];Array.Fill(distance,double.PositiveInfinity);distance[start]=graph.IndexedLengths[start];
        var queue=new PriorityQueue<int,double>();queue.Enqueue(start,distance[start]);
        while(queue.TryDequeue(out var node,out var cost))
        {
            token.ThrowIfCancellationRequested();
            if(cost>distance[node])continue;
            foreach(var next in graph.NextIndices[node])
            {
                var value=cost+graph.IndexedLengths[next];
                if(distance[next]<=value)continue;
                distance[next]=value;previous[next]=node;queue.Enqueue(next,value);
            }
        }
        var seen=new bool[count];var postorder=new List<int>();
        var stack=new Stack<(int Node,bool Exit)>();stack.Push((start,false));
        while(stack.TryPop(out var frame))
        {
            token.ThrowIfCancellationRequested();
            if(frame.Exit){postorder.Add(frame.Node);continue;}
            if(seen[frame.Node])continue;seen[frame.Node]=true;
            stack.Push((frame.Node,true));
            var children=graph.NextIndices[frame.Node];
            for(int i=children.Length-1;i>=0;i--)if(!seen[children[i]])stack.Push((children[i],false));
        }
        postorder.Reverse();var rank=new int[count];
        for(int i=0;i<postorder.Count;i++)rank[postorder[i]]=i;
        dominators[start]=start;bool changed;
        do
        {
            changed=false;
            for(int i=1;i<postorder.Count;i++)
            {
                token.ThrowIfCancellationRequested();
                var node=postorder[i];int parent=-1;
                foreach(var other in graph.PreviousIndices[node])
                {
                    if(dominators[other]<0)continue;
                    if(parent<0){parent=other;continue;}
                    var a=parent;var b=other;
                    while(a!=b)
                    {
                        while(rank[a]>rank[b])a=dominators[a];
                        while(rank[b]>rank[a])b=dominators[b];
                    }
                    parent=a;
                }
                if(parent>=0&&dominators[node]!=parent){dominators[node]=parent;changed=true;}
            }
        }while(changed);
        Complete=true;
    }
    public string[]? Corridor(DirectedRoadNetworkGraph graph,string destination,bool singleRoadPass)
    {
        var end=graph.Ordinals[destination];if(dominators[end]<0)return null;
        var path=new List<int>{end};while(path[^1]!=start)path.Add(previous[path[^1]]);path.Reverse();
        if(singleRoadPass&&path.Select(i=>graph.Portions[i]).Where(p=>p.Kind=="ROAD").GroupBy(p=>p.ItemUid).Any(g=>g.Count()>1))return null;
        var mandatory=new HashSet<int>{end};var current=end;
        while(current!=start){current=dominators[current];mandatory.Add(current);}
        return path.Where(mandatory.Contains).Select(i=>graph.Portions[i].Id).ToArray();
    }
}
