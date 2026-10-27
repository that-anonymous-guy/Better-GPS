using System.Text.Json;

namespace BetterGPSNavigation;

internal static class RoadIdentityView
{
    public static void Write(string directory,LiveRoute route)
    {
        if(Path.GetFileName(route.RouteId)!=route.RouteId||route.RouteId.IndexOfAny(Path.GetInvalidFileNameChars())>=0||route.RouteId.Contains(".."))
            throw new InvalidDataException("Route ID cannot be used as an audit filename.");
        var data=route.RoadIdentities!;
        double total=data.Spans.Sum(s=>s.NavEndM-s.NavStartM);
        var report=new {route.RouteId,segments=data.Spans.Length,namedSegments=data.Spans.Count(s=>s.Status=="RESOLVED"),
            candidateSegments=data.Spans.Count(s=>s.Identities.Length==0&&s.Candidates.Length>0),
            unknownSegments=data.Spans.Count(s=>s.Identities.Length==0&&s.Candidates.Length==0),
            highSegments=data.Spans.Count(s=>s.Identities.Any(i=>i.IdentityConfidence=="HIGH")),
            namedDistanceRatio=total>0?data.Spans.Where(s=>s.Status=="RESOLVED").Sum(s=>s.NavEndM-s.NavStartM)/total:0,
            anchors=data.Anchors.Length,associationStatuses=data.Anchors.GroupBy(a=>a.AssociationStatus).ToDictionary(g=>g.Key,g=>g.Count()),
            accuracy="Not measured without independent labelled observations",policy="MEDIUM and LOW never reported as confirmed"};
        File.WriteAllText(Path.Combine(directory,route.RouteId+"_identity-summary.json"),JsonSerializer.Serialize(report,LiveProtocol.Json));
        var payload=JsonSerializer.Serialize(new{route.RouteId,route.Edges,route.Actions,identities=data,report},LiveProtocol.Json).Replace("<","\\u003c");
        File.WriteAllText(Path.Combine(directory,route.RouteId+"_identites.html"),Html.Replace("__DATA__",payload));
    }
    private const string Html="""
<!doctype html><html lang="fr"><meta charset="utf-8"><title>Identités sur tout le trajet</title>
<style>body{background:#111923;color:#eef3fa;font:15px system-ui;margin:20px}main{display:grid;grid-template-columns:340px 1fr;gap:20px}select{width:100%;padding:10px;background:#233346;color:white}svg{width:100%;height:550px;background:#09111b;border:1px solid #456}table{border-collapse:collapse;width:100%}td,th{padding:7px;border-bottom:1px solid #345;text-align:left}pre{white-space:pre-wrap;max-height:500px;overflow:auto;background:#192738;padding:15px}button{padding:7px;background:#23435c;color:white;border:1px solid #567;cursor:pointer}small{color:#bdcddd}@media(max-width:850px){main{display:block}}</style>
<h1>Identités du trajet</h1><p id="summary"></p><small>Couleur = identité calculée. Bleu : provisoire ; vert : preuves HIGH ; gris : inconnue ; orange : ambiguïté/conflit. La couverture ne mesure pas l'exactitude.</small>
<main><aside><h2>Action</h2><select id="action"><option value="">Trajet entier</option></select><p id="context"></p><h2>Segment</h2><select id="segment"></select><p id="label"></p><button id="whole">Vue entière</button><button id="local">Zoom segment</button></aside><section><svg id="map" role="img" aria-label="Segments et noms sur tout le trajet"></svg><details open><summary>Preuves, alternatives et portée</summary><pre id="proof"></pre></details></section></main><h2>Toutes les portions</h2><table><thead><tr><th>Segment</th><th>Distance GPS depuis le départ</th><th>Identité</th><th>État</th></tr></thead><tbody id="rows"></tbody></table>
<script type="application/json" id="data">__DATA__</script><script>
const d=JSON.parse(document.getElementById('data').textContent),$=id=>document.getElementById(id),spans=d.identities.spans;
const text=s=>s.identities.length?s.identities.map(i=>i.identity+(i.cardinalConfidence==='HIGH'?' '+i.cardinal:'')+' ['+i.identityConfidence+']').join(' / '):s.candidates.length?'Hypothèses : '+s.candidates.map(i=>i.identity+(i.cardinal?' '+i.cardinal:'')+' ['+i.identityConfidence+']').join(' / '):'Inconnue';
$('summary').textContent=d.routeId+' — '+d.report.namedSegments+'/'+d.report.segments+' segments nommés ; '+d.report.highSegments+' HIGH ; '+d.report.candidateSegments+' avec hypothèses ; '+d.report.unknownSegments+' sans identité ; '+(100*d.report.namedDistanceRatio).toFixed(1)+' % de distance nommée.';
for(const s of spans){const o=document.createElement('option');o.value=s.id;o.textContent=s.segmentIndex+' — '+text(s);$('segment').append(o);const tr=document.createElement('tr');for(const v of [s.segmentIndex,(s.navStartM/1609.344).toFixed(2)+' mi',text(s),s.status]){const td=document.createElement('td');td.textContent=v;tr.append(td)}tr.onclick=()=>{$('segment').value=s.id;show(true)};$('rows').append(tr)}
for(const a of d.actions){const o=document.createElement('option');o.value=a.id;o.textContent=(a.eventIndex+1)+' — '+a.instruction;$('action').append(o)}
let zoom=false;
function draw(){const s=spans.find(s=>s.id===$('segment').value),svg=$('map');svg.replaceChildren();const edges=zoom?d.edges.filter(e=>Math.abs(e.segmentIndex-s.segmentIndex)<4):d.edges;const points=edges.flatMap(e=>[e.start,e.end]);if(!points.length)return;const minX=Math.min(...points.map(p=>p.x)),maxX=Math.max(...points.map(p=>p.x)),minZ=Math.min(...points.map(p=>p.z)),maxZ=Math.max(...points.map(p=>p.z));const scale=Math.min(950/Math.max(1,maxX-minX),500/Math.max(1,maxZ-minZ));svg.setAttribute('viewBox','0 0 1000 550');const xy=p=>[25+(p.x-minX)*scale,25+(maxZ-p.z)*scale];for(const e of edges){const span=spans.find(s=>s.segmentIndex===e.segmentIndex),line=document.createElementNS(svg.namespaceURI,'line'),a=xy(e.start),b=xy(e.end);const color=span.status==='RESOLVED'?(span.identities.some(i=>i.identityConfidence==='HIGH')?'#66e0ad':'#77bbff'):span.status==='UNKNOWN'?'#7d8998':'#ffbb66';for(const [k,v]of Object.entries({x1:a[0],y1:a[1],x2:b[0],y2:b[1],stroke:e.segmentIndex===s.segmentIndex?'#fff':color,'stroke-width':e.segmentIndex===s.segmentIndex?5:3}))line.setAttribute(k,v);const title=document.createElementNS(svg.namespaceURI,'title');title.textContent=e.segmentIndex+' — '+text(span);line.append(title);line.onclick=()=>{$('segment').value=span.id;show()};svg.append(line)}}
function show(local){if(local!==undefined)zoom=local;const s=spans.find(s=>s.id===$('segment').value);$('label').textContent=text(s)+' — '+s.reason;const anchors=d.identities.anchors.filter(a=>s.identities.concat(s.candidates).some(i=>i.proofIds.includes(a.id)));const context=d.identities.contexts.find(c=>c.actionId===$('action').value);$('proof').textContent=JSON.stringify({segment:s,anchors,actionContext:context},null,2);draw()}
$('segment').onchange=()=>show();$('whole').onclick=()=>show(false);$('local').onclick=()=>show(true);$('action').onchange=()=>{const c=d.identities.contexts.find(c=>c.actionId===$('action').value);const names=ids=>ids.map(id=>spans.find(s=>s.id===id)).map(text).join(' / ')||'Inconnue';$('context').textContent=c?'Approche : '+names(c.approachSpanIds)+'. Cible : '+names(c.targetSpanIds)+'. Objectif après comparaison : '+(c.preferredIdentity||'non distingué')+'. Segments examinés : '+c.futureSpanIds.join(', ')+'. Règle : '+c.reason:'';if(c?.targetSpanIds.length)$('segment').value=c.targetSpanIds[0];show(!!c)};show(false);
</script></html>
""";
}
