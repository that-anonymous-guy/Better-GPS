using System.Numerics;
using System.Text;
using System.Text.Json;
using System.Globalization;
using System.Net;
using TruckLib.ScsMap;
namespace BetterGPSNavigation;

internal sealed record NodeComparison(int Index,string Uid,LivePoint Native,LivePoint? Map,
    double? PositionErrorM,double? RotationErrorDeg,string Status,string[] Issues);
internal static class NativeMapAudit
{
    internal static NodeComparison Compare(int index,string uid,NativeNode evidence,Vector3? mapPosition,
        Quaternion? mapRotation,ulong backward,ulong forward)
    {
        var point=new LivePoint(evidence.RawX/256d,evidence.RawY/256d,evidence.RawZ/256d);
        var issues=new List<string>(); double? error=null,angle=null;
        if(mapPosition is { } p && mapRotation is { } q)
        {
            error=Math.Sqrt(Math.Pow(point.X-p.X,2)+Math.Pow(point.Y-p.Y,2)+Math.Pow(point.Z-p.Z,2));
            var nativeQ=Quaternion.Normalize(new((float)evidence.Qx,(float)evidence.Qy,(float)evidence.Qz,(float)evidence.Qw));
            angle=2*Math.Acos(Math.Clamp(Math.Abs(Quaternion.Dot(nativeQ,Quaternion.Normalize(q))),0,1))*180/Math.PI;
            if(!double.IsFinite(error.Value)||error>0.05)issues.Add("POSITION_MISMATCH");
            if(!double.IsFinite(angle.Value)||angle>1)issues.Add("ROTATION_MISMATCH");
            if(evidence.BackwardValid && NativeCaptureProtocol.Uid(evidence.BackwardUid)!=backward)issues.Add("BACKWARD_MISMATCH");
            if(evidence.ForwardValid && NativeCaptureProtocol.Uid(evidence.ForwardUid)!=forward)issues.Add("FORWARD_MISMATCH");
        }
        else issues.Add("MAP_NODE_MISSING");
        if(evidence.Status!="READ_OK"||!evidence.BackwardValid||!evidence.ForwardValid)issues.Add("INCOMPLETE_NATIVE_READ");
        return new(index,uid,point,mapPosition is { } mp?new(mp.X,mp.Y,mp.Z):null,error,angle,
            issues.Count==0?"MATCH":"MISMATCH",issues.ToArray());
    }
    public static void Check(ProbeRoute route,Map map,string output)
    {
        if(route.NativeEvidence?.Nodes is not { } nodes)return;
        var rows=nodes.Select((n,i)=>{
            var uid=route.NativeEvidence.Physical[i].NodeUid;
            map.Nodes.TryGetValue(NativeCaptureProtocol.Uid(uid),out var mapped);
            return Compare(i,uid,n,mapped?.Position,mapped?.Rotation,mapped?.BackwardItem?.Uid??0,mapped?.ForwardItem?.Uid??0);
        }).ToArray();
        var warnings=new List<string>();
        for(int i=1;i<route.Physical.Count;i++)
            if(route.Physical[i].TotalDistanceTillEndM>route.Physical[i-1].TotalDistanceTillEndM+1)
                warnings.Add($"REMAINING_DISTANCE_INCREASE:{i}");
        var status=rows.All(r=>r.Status=="MATCH")?"NATIVE_MAP_MATCH":"NATIVE_MAP_MISMATCH";
        RouteDiagnostics.Try(()=>Directory.CreateDirectory(output));
        var report=new {status,coordinateConversion="raw / 256; verified against loaded node coordinates",positionToleranceM=0.05,
            rotationToleranceDeg=1,metricsStatus=warnings.Count==0?"ORDER_CHECK_PASSED":"NEEDS_REVIEW",
            warnings,route.NativeEvidence.CapturedAtTickMs,rows};
        var stem=Path.Combine(output,route.Stem+"_native_audit");
        RouteDiagnostics.WriteText(stem+".json",()=>JsonSerializer.Serialize(report,LiveProtocol.Json));
        if(RouteDiagnostics.CompactOutput is null)RouteDiagnostics.Try(()=>WriteView(stem+".html",status,rows,warnings));
        Console.WriteLine($"Capture : {status} ; {rows.Count(r=>r.Status=="MATCH")}/{rows.Length} noeuds ; audit : {stem}.html");
        if(status!="NATIVE_MAP_MATCH"||warnings.Count>0)
            throw new InvalidDataException(status+" — consulter l'audit natif avant d'utiliser ce trajet.");
    }
    internal static void WriteView(string path,string status,NodeComparison[] rows,List<string> warnings)
    {
        string H(string s)=>WebUtility.HtmlEncode(s);
        string F(double? d)=>d?.ToString("0.000",CultureInfo.InvariantCulture)??"—";
        var all=rows.SelectMany(r=>r.Map is { } m?new[]{r.Native,m}:new[]{r.Native}).ToArray();
        double minX=all.Min(p=>p.X),minZ=all.Min(p=>p.Z),scale=Math.Max(1,Math.Max(all.Max(p=>p.X)-minX,all.Max(p=>p.Z)-minZ));
        string XY(LivePoint p)=>$"{F(30+940*(p.X-minX)/scale)},{F(30+940*(p.Z-minZ)/scale)}";
        var b=new StringBuilder("<!doctype html><html lang='fr'><meta charset='utf-8'><title>Capture native / carte</title><style>body{font:16px system-ui;background:#101820;color:#eee;margin:24px}svg{background:#192733;width:100%;max-height:65vh}table{border-collapse:collapse;width:100%}td,th{padding:8px;border-bottom:1px solid #456}.bad{color:#ff8787}summary{cursor:pointer}a{color:#80cfff}</style>");
        b.Append($"<h1>Capture native / carte : {H(status)}</h1><p>Bleu : nœuds natifs. Orange : carte chargée. Les traits suivent l'ordre du GPS, pas les courbes des voies. Cliquer un point pour voir sa ligne. Axe horizontal X, axe vertical Z ; hauteur Y dans le tableau.</p><p>{H(string.Join(" ; ",warnings))}</p><svg viewBox='0 0 1000 1000' role='img' aria-label='Comparaison du trajet natif et de la carte'>");
        b.Append($"<polyline points='{string.Join(' ',rows.Select(r=>XY(r.Native)))}' fill='none' stroke='#60c5ff' stroke-width='2'/>");
        foreach(var r in rows)
        {
            if(r.Map is { } p)b.Append($"<circle cx='{XY(p).Split(',')[0]}' cy='{XY(p).Split(',')[1]}' r='5' fill='none' stroke='#ffa64d'/>");
            b.Append($"<a href='#row{r.Index}'><circle cx='{XY(r.Native).Split(',')[0]}' cy='{XY(r.Native).Split(',')[1]}' r='2.5' fill='#60c5ff'><title>{H(r.Uid+" "+r.Status)}</title></circle></a>");
        }
        b.Append("</svg><p>La correspondance valide les nœuds et connexions capturés. Elle ne confirme pas les panneaux, les voies internes ou les distances du Route Advisor.</p><table><thead><tr><th>Nœud</th><th>Natif X/Y/Z</th><th>Carte X/Y/Z</th><th>Écart m</th><th>Rotation °</th><th>Résultat</th></tr></thead><tbody>");
        foreach(var r in rows)b.Append($"<tr id='row{r.Index}' class='{(r.Status=="MATCH"?"":"bad")}'><td>{r.Index} · {H(r.Uid)}</td><td>{F(r.Native.X)} / {F(r.Native.Y)} / {F(r.Native.Z)}</td><td>{F(r.Map?.X)} / {F(r.Map?.Y)} / {F(r.Map?.Z)}</td><td>{F(r.PositionErrorM)}</td><td>{F(r.RotationErrorDeg)}</td><td>{H(r.Status+" "+string.Join(", ",r.Issues))}</td></tr>");
        b.Append("</tbody></table></html>");RouteDiagnostics.WriteText(path,b.ToString());
    }
}
