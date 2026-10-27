using System.Text.Json;

namespace BetterGPSNavigation;

internal static class NamedNetworkView
{
    public static void Write(string directory,string stem,NamedNetworkSnapshot data,ProbeRoute probe,RouteCorridor corridor,LiveRoute route)
    {
        var joined=NamedRoadNetwork.Join(data,probe,corridor,route);
        Write(directory,stem,data,joined,route);
    }
    internal static void Write(string directory,string stem,NamedNetworkSnapshot data,RoadIdentitySnapshot joined,LiveRoute route)
    {
        var routePortions=joined.Spans.Select(s=>new{s.SegmentIndex,portionId=DirectedRoadNetworkGraph.Id(s.ItemUid,s.EntryNodeUid,s.ExitNodeUid),s.Status}).ToArray();
        var summary=new {route.RouteId,networkPortions=data.Portions.Length,roadPortions=data.Portions.Count(p=>p.Kind=="ROAD"),
            prefabBranches=data.Portions.Count(p=>p.Kind=="PREFAB"),connections=data.Connections.Length,
            namedPortions=data.Names.Count(n=>n.Status=="RESOLVED"),highPortions=0,
            sourceRoles=data.Sources.GroupBy(s=>s.Role).ToDictionary(g=>g.Key,g=>g.Count()),
            associations=data.Sources.GroupBy(s=>s.AssociationStatus).ToDictionary(g=>g.Key,g=>g.Count()),
            streetInterpretations=data.Sources.Where(s=>s.Kind=="StreetName").GroupBy(s=>s.InterpretationStatus.Length>0?s.InterpretationStatus:s.Source).ToDictionary(g=>g.Key,g=>g.Count()),
            unresolvedStreetSupports=data.Sources.Where(s=>s.Source=="UNCLASSIFIED_STREET_SIGN").Select(s=>new{s.Id,s.Identity,s.BoardPath,s.SupportModel,s.InterpretationStatus}).ToArray(),
            routeSegments=joined.Spans.Length,namedRouteSegments=joined.Spans.Count(s=>s.Status=="RESOLVED"),
            unjoinedRouteSegments=joined.Spans.Count(s=>s.Reason=="DIRECTED_PORTION_NOT_RESOLVED"),
            accuracy="Not measured: independent in-game labels are still required",data.ReusePolicy};
        WriteAtomic(Path.Combine(directory,stem+"_named-network.json"),JsonSerializer.Serialize(data,LiveProtocol.Json));
        WriteAtomic(Path.Combine(directory,stem+"_network-summary.json"),JsonSerializer.Serialize(summary,LiveProtocol.Json));
        var payload=JsonSerializer.Serialize(new{network=data,routePortions,route.Actions,contexts=joined.Contexts,summary},LiveProtocol.Json).Replace("<","\\u003c");
        WriteAtomic(Path.Combine(directory,stem+"_reseau_nomme.html"),Html.Replace("__DATA__",payload));
    }
    private static void WriteAtomic(string path,string text)
    {var temporary=path+".tmp";File.WriteAllText(temporary,text);File.Move(temporary,path,true);}
    private const string Html="""
<!doctype html><html lang="fr"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Carte nommée : routes et alternatives</title>
<style>body{margin:18px;background:#111923;color:#eef3fa;font:15px system-ui}main{display:grid;grid-template-columns:360px 1fr;gap:18px}select,button{padding:8px;background:#233346;color:white;border:1px solid #567}select{width:100%;margin:8px 0}canvas{width:100%;height:620px;background:#09111b;touch-action:none;border:1px solid #456}pre{white-space:pre-wrap;overflow:auto;max-height:620px;background:#192738;padding:12px}small{color:#bed0e4}label{display:block;margin:12px 0}h1{font-size:24px}@media(max-width:850px){main{display:block}}</style>
<h1>Carte nommée du réseau local</h1><p id="summary"></p><small>Toutes les branches sont extraites avant sélection GPS. Noms bleus : provisoires MEDIUM ; gris : inconnus ; orange : ambigus/conflits. Aucune attribution HIGH. Cliquez une route ou un panneau. Molette : zoom ; glisser : déplacer.</small>
<main><aside><h2>Trajet et action</h2><select id="action"><option value="">Trajet entier</option></select><p id="context"></p><h2>Portion orientée</h2><select id="portion"></select><h2>Source</h2><select id="source"><option value="">Aucune source sélectionnée</option></select><label><input type="checkbox" id="gps" checked> Dessiner le trajet GPS en violet</label><button id="whole">Réseau entier</button> <button id="local">Zoom sélection</button><p id="label"></p><small>Les normales jaunes sont des hypothèses issues des modèles, pas une calibration ingame. Les panneaux directionnels ne deviennent pas automatiquement des noms de route.</small></aside><section><canvas id="map" width="1100" height="620" aria-label="Réseau orienté, routes concurrentes et panneaux"></canvas><h2>Preuves et alternatives</h2><pre id="proof"></pre></section></main>
<script type="application/json" id="data">__DATA__</script><script>
const d=JSON.parse(document.getElementById('data').textContent),$=id=>document.getElementById(id),net=d.network,portions=new Map(net.portions.map(p=>[p.id,p])),names=new Map(net.names.map(n=>[n.portionId,n])),sources=new Map(net.sources.map(s=>[s.id,s])),gps=new Set(d.routePortions.map(s=>s.portionId));
const desc=n=>n.identities.length?n.identities.map(l=>l.identity+(l.cardinal?' '+l.cardinal:'')+' ['+l.confidence+']').join(' / '):n.candidates.length?'Candidats : '+n.candidates.map(l=>l.identity).join(' / '):'Inconnue';
$('summary').textContent=d.summary.routeId+' — '+d.summary.roadPortions+' portions routières orientées, '+d.summary.prefabBranches+' branches de prefabs, '+net.sources.length+' sources ; '+d.summary.namedRouteSegments+'/'+d.summary.routeSegments+' segments GPS nommés provisoirement ; '+d.summary.unjoinedRouteSegments+' sans géométrie orientée résolue.';
for(const p of net.portions){const o=document.createElement('option');o.value=p.id;o.textContent=p.kind+' '+p.itemUid+' — '+desc(names.get(p.id));$('portion').append(o)}
for(const s of net.sources){const o=document.createElement('option');o.value=s.id;o.textContent=s.role+' — '+s.identity+(s.cardinal?' '+s.cardinal:'')+' — '+s.associationStatus;$('source').append(o)}
for(const a of d.actions){const o=document.createElement('option');o.value=a.id;o.textContent=(a.eventIndex+1)+' — '+a.instruction;$('action').append(o)}
const canvas=$('map'),ctx=canvas.getContext('2d');let center=[0,0],scale=1,drag=null,moved=false;
function xy(p){return [(p[0]-center[0])*scale+550,(p[2]-center[1])*scale+310]}function world(x,y){return [(x-550)/scale+center[0],(y-310)/scale+center[1]]}
function fit(points){if(!points.length)return;const xs=points.map(p=>p[0]),zs=points.map(p=>p[2]),a=Math.min(...xs),b=Math.max(...xs),c=Math.min(...zs),e=Math.max(...zs);center=[(a+b)/2,(c+e)/2];scale=Math.min(1000/Math.max(100,b-a),540/Math.max(100,e-c));draw()}
function line(path,color,width){ctx.beginPath();path.forEach((p,i)=>{const [x,y]=xy(p);i?ctx.lineTo(x,y):ctx.moveTo(x,y)});ctx.strokeStyle=color;ctx.lineWidth=width;ctx.stroke()}
function draw(){ctx.clearRect(0,0,1100,620);const selected=$('portion').value,source=sources.get($('source').value),alternatives=new Set(source?.alternatives.map(a=>a.portionId)||[]),owners=new Set(source?.owners||[]);
for(const p of net.portions){const n=names.get(p.id);let color=n.status==='RESOLVED'?'#77bbff':n.status==='UNKNOWN'?'#66758a':'#ffae55';if(alternatives.has(p.id))color='#db944b';if(owners.has(p.id))color='#46d5c3';if(p.id===selected)color='#fff';for(const path of p.paths)line(path,color,p.id===selected?4:alternatives.has(p.id)?2.5:1.2);if($('gps').checked&&gps.has(p.id))for(const path of p.paths)line(path,'#c988ff',1);
if(p.id===selected){const path=p.paths[0],i=Math.floor(path.length/2),a=xy(path[Math.max(0,i-1)]),b=xy(path[i]);const angle=Math.atan2(b[1]-a[1],b[0]-a[0]);ctx.save();ctx.translate(b[0],b[1]);ctx.rotate(angle);ctx.fillStyle='#fff';ctx.beginPath();ctx.moveTo(9,0);ctx.lineTo(-6,-5);ctx.lineTo(-6,5);ctx.fill();ctx.restore()}}
for(const s of net.sources){const [x,y]=xy(s.position);ctx.fillStyle=s.id===source?.id?'#fff':s.role==='MapAnnotation'?'#6de0bc':'#f6d679';ctx.beginPath();ctx.arc(x,y,s.id===source?.id?6:3,0,Math.PI*2);ctx.fill();if(s.id===source?.id){ctx.fillText(s.identity+' '+s.role,x+9,y-9);if(s.normal)line([s.position,[s.position[0]+s.normal[0]*30,s.position[1]+s.normal[1]*30,s.position[2]+s.normal[2]*30]],'#f6d679',3)}}}
function show(){const p=portions.get($('portion').value),n=names.get(p?.id),s=sources.get($('source').value);$('label').textContent=p?desc(n)+' — '+p.entryNodeUid+' → '+p.exitNodeUid:'';const evidence=n?n.identities.concat(n.candidates).flatMap(l=>l.sourceIds).map(id=>sources.get(id)):[];$('proof').textContent=JSON.stringify({portion:p,names:n,sources:evidence,selectedSource:s,connections:net.connections.filter(c=>c.from===p?.id||c.to===p?.id)},null,2);draw()}
$('portion').onchange=()=>{$('source').value='';show()};$('source').onchange=()=>{const s=sources.get($('source').value);if(s?.owners.length)$('portion').value=s.owners[0];show()};$('gps').onchange=draw;
$('whole').onclick=()=>fit([[net.region.minX,0,net.region.minZ],[net.region.maxX,0,net.region.maxZ]]);
$('local').onclick=()=>{const s=sources.get($('source').value),p=portions.get($('portion').value);const pts=s?[s.position,...s.alternatives.flatMap(a=>portions.get(a.portionId).paths.flat())]:p?.paths.flat()||[];fit(pts)};
$('action').onchange=()=>{const a=d.actions.find(a=>a.id===$('action').value),c=d.contexts.find(c=>c.actionId===a?.id);if(!a){$('context').textContent='';return}const outgoing=d.routePortions.find(p=>p.segmentIndex===a.segmentIndex+1);$('context').textContent='Sortie immédiate : '+(outgoing&&names.has(outgoing.portionId)?desc(names.get(outgoing.portionId)):'Inconnue')+'. '+c.reason;if(outgoing&&portions.has(outgoing.portionId)){$('portion').value=outgoing.portionId;$('source').value='';show();$('local').click()}};
function coords(e){const r=canvas.getBoundingClientRect();return [(e.clientX-r.left)*1100/r.width,(e.clientY-r.top)*620/r.height]}
canvas.onwheel=e=>{e.preventDefault();const [x,y]=coords(e),before=world(x,y);scale=Math.max(.0001,Math.min(40,scale*Math.exp(-e.deltaY*.001)));const after=world(x,y);center=[center[0]+before[0]-after[0],center[1]+before[1]-after[1]];draw()};
canvas.onpointerdown=e=>{drag={point:coords(e),center:[...center]};moved=false;canvas.setPointerCapture(e.pointerId)};canvas.onpointermove=e=>{if(!drag)return;const [x,y]=coords(e),dx=x-drag.point[0],dy=y-drag.point[1];if(Math.hypot(dx,dy)>3)moved=true;center=[drag.center[0]-dx/scale,drag.center[1]-dy/scale];draw()};
canvas.onpointerup=e=>{drag=null;if(moved)return;const [x,y]=coords(e),[wx,wz]=world(x,y);const source=net.sources.map(s=>[s,Math.hypot(s.position[0]-wx,s.position[2]-wz)*scale]).sort((a,b)=>a[1]-b[1])[0];if(source&&source[1]<9){$('source').value=source[0].id;$('source').onchange();return}let best=null,distance=10/scale;for(const p of net.portions)for(const path of p.paths)for(let i=1;i<path.length;i++){const a=path[i-1],b=path[i],dx=b[0]-a[0],dz=b[2]-a[2],len=dx*dx+dz*dz;if(!len)continue;const t=Math.max(0,Math.min(1,((wx-a[0])*dx+(wz-a[2])*dz)/len)),r=Math.hypot(wx-a[0]-t*dx,wz-a[2]-t*dz);if(r<distance){distance=r;best=p}}if(best){$('portion').value=best.id;$('portion').onchange()}};
show();$('whole').click();window.auditReady=true;
</script></html>
""";
}
