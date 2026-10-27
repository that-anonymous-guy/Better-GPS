using System.Numerics;

namespace BetterGPSNavigation;

/// <summary>Assigns separate direction/arrow plates to their own shield,
/// never to every shield sharing a support. Ties and unknown placement abstain.</summary>
internal static class ShieldAssemblyLinker
{
    internal static ParsedSignArea[] Companions(IReadOnlyList<ParsedSignArea> areas,ParsedSignArea target,string boardPath)
    {
        var shields=areas.Where(a=>a.RoadShields.Count==1&&a.Extraction is {Status:"OK"} p&&
            p.BoardPath.StartsWith("/def/sign/board/shield/",StringComparison.OrdinalIgnoreCase)&&
            p.BoardPath is not ("/def/sign/board/shield/card_dir_2.sii" or "/def/sign/board/shield/arrow_dir.sii")).ToArray();
        var companions=areas.Where(a=>a.Extraction is {Status:"OK",Reliable:true} p&&p.BoardPath==boardPath).ToArray();
        if(shields.Length==1)return shields[0].AreaName==target.AreaName?companions:[];
        if(shields.Length==0)return [];
        Vector3? Point(ParsedSignArea a)=>a.Extraction is {} p&&BoardModelResolver.Valid(p.Anchor.LocalPosition,3)&&
            p.BoardOffset.All(v=>Math.Abs(v)<1e-6)?BoardModelResolver.V(p.Anchor.LocalPosition!):null;
        var result=new List<ParsedSignArea>();
        foreach(var companion in companions)
        {
            var point=Point(companion);if(point is null)continue;
            // Missing placement on any competing shield prevents exclusion.
            var positions=shields.Select(s=>(Area:s,Point:Point(s))).ToArray();
            if(positions.Any(p=>p.Point is null))continue;
            var distances=positions.Select(p=>(p.Area,Distance:Vector3.Distance(point.Value,p.Point!.Value))).OrderBy(p=>p.Distance).ToArray();
            if(distances[0].Distance>3||distances[1].Distance-distances[0].Distance<.15f)continue;
            if(distances[0].Area.AreaName==target.AreaName)result.Add(companion);
        }
        return result.ToArray();
    }
}
