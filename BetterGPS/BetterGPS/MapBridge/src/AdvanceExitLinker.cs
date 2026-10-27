using System.Numerics;
using System.Text.RegularExpressions;
namespace BetterGPSNavigation;

// Advance notices reuse an independently linked exit sign, never a nearby road name.
internal static class AdvanceExitLinker
{
    private static string Exit(NetworkNameSource source)
    {
        var matches=Regex.Matches(source.Raw,@"\bEXIT\s+(\d+\s*[A-Z]?)\b",RegexOptions.IgnoreCase)
            .Select(m=>Regex.Replace(m.Groups[1].Value,@"\s+","").ToUpperInvariant()).Distinct().ToArray();
        return matches.Length==1?matches[0]:"";
    }
    internal static NetworkNameSource[] Link(DirectedRoadNetworkGraph graph,NetworkNameSource[] sources)=>sources.Select(source=>
    {
        if(source.Role!="GuideTo"||source.Owners.Length>0||source.Arrow.Length>0||Exit(source).Length==0)return source;
        var candidates=sources.Where(s=>Supports(graph,source,s)).ToArray();
        if(candidates.Length==0||candidates.SelectMany(s=>s.Owners).Distinct().Count()!=1)return source;
        return source with{AssociationStatus="ADVANCE_EXIT_BRANCH_TARGET",Owners=candidates[0].Owners,AdvanceGuideId=candidates[0].Id};
    }).ToArray();
    internal static bool Valid(DirectedRoadNetworkGraph graph,NetworkNameSource source,IReadOnlyDictionary<string,NetworkNameSource> sources)=>
        source.AdvanceGuideId.Length>0&&sources.TryGetValue(source.AdvanceGuideId,out var guide)&&Supports(graph,source with{Owners=[]},guide)&&
        source.Owners.SequenceEqual(guide.Owners);
    private static bool Supports(DirectedRoadNetworkGraph graph,NetworkNameSource notice,NetworkNameSource guide)
    {
        if(notice.Role!="GuideTo"||notice.Arrow.Length>0||notice.Normal is null||guide.Normal is null||notice.ObjectId==guide.ObjectId||
            guide.AssociationStatus!="DIRECTIONAL_BRANCH_TARGET"||guide.Owners.Length!=1||notice.Identity!=guide.Identity||
            notice.Kind!=guide.Kind||notice.Cardinal.Length>0&&guide.Cardinal.Length>0&&notice.Cardinal!=guide.Cardinal||
            notice.ReadingConfidence is not ("HIGH" or "MEDIUM")||guide.ReadingConfidence is not ("HIGH" or "MEDIUM")||
            Exit(notice).Length==0||Exit(notice)!=Exit(guide))return false;
        var normal=NetworkPortion.V(notice.Normal);normal.Y=0;
        var nextNormal=NetworkPortion.V(guide.Normal);nextNormal.Y=0;
        if(normal.LengthSquared()<.8||nextNormal.LengthSquared()<.8||Vector3.Dot(Vector3.Normalize(normal),Vector3.Normalize(nextNormal))<.85)return false;
        var position=NetworkPortion.V(notice.Position);var delta=NetworkPortion.V(guide.Position)-position;delta.Y=0;
        var along=Vector3.Dot(delta,-Vector3.Normalize(normal));
        if(along<10||delta.Length()>1500)return false;
        var approach=NamedRoadNetwork.Associate(graph,position,Vector3.Normalize(normal),"OwnRoute",notice.PoseStatus)
            .Where(a=>a.Status=="CANDIDATE"&&a.DistanceM<=30&&graph.Get(a.PortionId).Kind=="ROAD").ToArray();
        return approach.Any(a=>NamedRoadNetwork.MandatoryCorridor(graph,a.PortionId,guide.Owners[0],2000,true) is {Length:>0});
    }
}
