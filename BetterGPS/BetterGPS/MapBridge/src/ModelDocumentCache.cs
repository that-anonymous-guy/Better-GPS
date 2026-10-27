using System.Runtime.CompilerServices;
using TruckLib;
using TruckLib.Models;
namespace BetterGPSNavigation;

// Consumers only inspect model geometry. Share the parsed model across sign
// extraction, support faces and lane placement within the same mounted assets.
internal static class ModelDocumentCache
{
    private sealed class Store
    {
        internal readonly Dictionary<string,(Model Value,long Size)> Values=new(StringComparer.Ordinal);
        internal readonly Queue<string> Order=[];
        internal long Size;
    }
    private static readonly ConditionalWeakTable<IFileSystem,Store> Models=new();
    internal static Model Open(string path,IFileSystem fs)
    {
        var models=Models.GetValue(fs,_=>new());
        lock(models)
        {
            if(models.Values.TryGetValue(path,out var known))return known.Value;
            using var timing=PerformanceTrace.DetailObserver is null?null:PerformanceTrace.Detail("model:"+path);
            var model=Model.Open(path,fs);
            var size=model.Parts.Sum(p=>p.Pieces.Sum(piece=>(long)piece.Vertices.Count*96)+p.Locators.Count*256L)+4096;
            while(models.Order.Count>0&&(models.Size+size>128*1024*1024||models.Order.Count>=64))
            {var old=models.Order.Dequeue();models.Size-=models.Values[old].Size;models.Values.Remove(old);}
            if(size<=128*1024*1024){models.Values[path]=(model,size);models.Order.Enqueue(path);models.Size+=size;}
            return model;
        }
    }
}
