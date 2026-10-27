namespace BetterGPSNavigation;

// One budget for the whole mounted asset set, rather than one budget per DLC/mod.
// Buffers are immutable once published. Open() exposes them through a non-writable,
// non-publicly-visible MemoryStream; ReadAllBytes() retains its defensive-copy contract.
internal sealed class AssetReadCache(long capacity=32L*1024*1024)
{
    private sealed record Entry((string Source,string Path) Key,byte[] Bytes);
    private readonly object gate=new();
    private readonly Dictionary<(string,string),LinkedListNode<Entry>> entries=[];
    private readonly LinkedList<Entry> lru=[];
    private long resident;
    internal long ResidentBytes {get{lock(gate)return resident;}}
    internal bool TryGet(string source,string path,out byte[] bytes)
    {
        lock(gate){if(entries.TryGetValue((source,path),out var node)){lru.Remove(node);lru.AddLast(node);bytes=node.Value.Bytes;return true;}bytes=null!;return false;}
    }
    internal bool Store(string source,string path,byte[] bytes)
    {
        if(bytes.Length>Math.Min(capacity,2L*1024*1024))return false;
        lock(gate){if(entries.ContainsKey((source,path)))return true;
            while(resident+bytes.Length>capacity&&lru.First is {} oldest){entries.Remove(oldest.Value.Key);resident-=oldest.Value.Bytes.Length;lru.RemoveFirst();}
            var entry=new Entry((source,path),bytes);entries[entry.Key]=lru.AddLast(entry);resident+=bytes.Length;return true;
        }
    }
}
