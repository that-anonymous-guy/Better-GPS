using System.Text.Json;

namespace BetterGPSNavigation;

internal static class GroupGuidanceView
{
    public static void Refresh(string jsonPath)
    {
        var json=File.ReadAllText(jsonPath);
        using var document=JsonDocument.Parse(json);
        var groups=document.RootElement.GetProperty("groups").Deserialize<SignGroupProof[]>(SignGroupProof.Json)!;
        if(!jsonPath.EndsWith("_groups.json",StringComparison.OrdinalIgnoreCase))throw new ArgumentException("Expected *_groups.json");
        File.WriteAllText(Path.ChangeExtension(jsonPath,"html"),Html.Replace("__DATA__",json.Replace("<","\\u003c")));
        BoardCalibrationTools.WritePacket(Path.GetDirectoryName(jsonPath)!,Path.GetFileName(jsonPath)[..^"_groups.json".Length],groups);
    }
    public static void Write(string directory,string stem,LiveRoute route,IReadOnlyList<SignGroupProof> groups,LaneRouteGraph graph)
    {
        var data=new {version=1,route.RouteId,actions=route.Actions.Select(a=>new{a.EventIndex,a.SegmentIndex,a.Instruction,a.BranchId,a.NavStationM}),groups,
            pieces=graph.Pieces.Select(p=>new{p.Id,p.SegmentIndex,p.Selected,p.Kind,p.StartPort,p.EndPort,
                startNodeUid=p.StartNodeUid.ToString("X16"),endNodeUid=p.EndNodeUid.ToString("X16"),p.Blockers,points=p.Points.Select(v=>new[]{v.X,v.Y,v.Z})}),graph.Boundaries};
        var json=JsonSerializer.Serialize(data,SignGroupProof.Json);
        File.WriteAllText(Path.Combine(directory,stem+"_groups.json"),json);
        File.WriteAllText(Path.Combine(directory,stem+"_groups.html"),Html.Replace("__DATA__",json.Replace("<","\\u003c")));
        BoardCalibrationTools.WritePacket(directory,stem,groups);
    }
    private const string Html="""
<!doctype html><html lang="fr"><meta charset="utf-8"><title>Groupes de panneaux → voies → GPS</title>
<style>body{margin:0;background:#111923;color:#e5edf5;font:15px system-ui}header{padding:20px;background:#192634}h1{font-size:23px;margin:0 0 10px}main{display:grid;grid-template-columns:360px 1fr;gap:16px;padding:16px}select,button{padding:8px;background:#263b50;color:white;border:1px solid #5a748c;border-radius:5px}#groups button{display:block;width:100%;text-align:left;margin:8px 0}#groups button.active{outline:2px solid #63c8ff}small{display:block;color:#c6d4e2;margin:6px 0}.badge{font-size:12px}svg{width:100%;height:550px;background:#0c121b;border:1px solid #304b64}#proof{white-space:pre-wrap;overflow:auto;max-height:420px}#summary,#reason{padding:12px;background:#1b2b3b;margin:12px 0}.legend{display:flex;gap:18px;flex-wrap:wrap;padding:10px}.legend span{border-bottom:3px solid;padding:3px}@media(max-width:850px){main{display:block}svg{height:400px}}a{color:#7cd3ff}</style>
<header><h1>Panneau → voies → sortie GPS</h1><div id="route"></div><small>Les placements provisoires restent visibles. Aucune hypothèse n'est choisie parce qu'elle correspond au GPS.</small><label>Action <select id="event"></select></label> <label>Placement <select id="placement"><option value="ALL">Toutes les hypothèses</option></select></label></header>
<main><aside><div id="groups"></div></aside><section><div id="summary"></div><small>Repère du template : toutes les flèches de la même plaque. Ceci n'est pas une capture du jeu.</small><svg id="board" style="height:230px" role="img" aria-label="Flèches et groupes dans le repère de dessin de la plaque"></svg><svg id="map" role="img" aria-label="Voies et flèches de la plaque sélectionnée"></svg><div class="legend"><span style="border-color:#75879a">Voies du trajet</span><span style="border-color:#fa8c88">Autres sorties</span><span style="border-color:#68e3ce">Chaîne du groupe</span><span style="border-color:#ffd179">Flèche du groupe sélectionné</span><span style="border-color:#78bcf5">Autres groupes de la même plaque</span><span style="border-color:#b899df">Contour et normale avant</span></div><div id="reason"></div><details><summary>Preuves et raccordements</summary><pre id="proof"></pre></details></section></main>
<script type="application/json" id="data">__DATA__</script><script>
const data=JSON.parse(document.getElementById('data').textContent),$=id=>document.getElementById(id);let group=null;
const identityStatus='Route lue — segment routier encore à associer';
const statuses={SELECTED_EXIT_CONFIRMED:'Sortie GPS confirmée',GPS_COMPATIBLE_CANDIDATE:'Compatible avec le GPS — provisoire',OTHER_EXIT_CANDIDATE:'Autre sortie — placement provisoire',AMBIGUOUS_PLACEMENT:'Les placements ne donnent pas la même sortie',AMBIGUOUS_EXITS:'Plusieurs sorties accessibles',CHAIN_BLOCKED:'Raccordement incomplet',NO_ARROW_GEOMETRY:'Aucune flèche positionnée',GROUP_UNRESOLVED:'Groupe incomplet'};
const reasons={PLACEMENT_NOT_RENDER_VERIFIED:'Origine et placement de la plaque à vérifier dans le rendu',ACTIVE_LOCATOR_VARIANT_UNRESOLVED:'Partie active du portique non déterminée',LANE_WIDTH_4_5M_ASSUMED:'Largeur de voie estimée à 4,5 m',BOUNDARY_NOT_MATCHED:'Raccordement de voies manquant',GROUP_OBJECT_OR_SCHEMA_UNVERIFIED:'Objet ou attribut du groupe à vérifier',BOARD_OFFSET_UNCALIBRATED:'Déplacement de la plaque non calibré',FOLLOW_ROAD_DIR_UNRESOLVED:'Orientation automatique du panneau non résolue',NO_LANE_WITHIN_DIAGNOSTIC_WINDOW:'Aucune voie dans la fenêtre de projection',LANE_REACHES_MULTIPLE_EXITS:'La voie permet plusieurs sorties',ARROW_PLACEMENT_MISSING:'Une flèche du groupe ne possède pas de position exploitable',ARROW_MISSING:'Pas de flèche exploitable pour ce groupe'};
$('route').textContent=data.routeId;
for(const name of [...new Set(data.groups.flatMap(g=>g.arrows.map(a=>a.origin)))].sort()){const o=document.createElement('option');o.value=name;o.textContent=name;$('placement').append(o);}
for(const a of data.actions){const o=document.createElement('option');o.value=a.eventIndex;o.textContent=`${a.eventIndex+1} — ${a.instruction} — ${(a.navStationM/1609.344).toFixed(2)} mi depuis le départ`;$('event').append(o);}
statuses.ROAD_IDENTITY_UNASSIGNED=identityStatus;reasons.ROAD_SEGMENT_NOT_ASSOCIATED='Identifiant de route lu sans exigence de flèche ; rattachement à une route encore absent';
function label(g){return [g.road,g.cardinal,g.destination].filter(Boolean).join(' — ')}
function refresh(){const groups=data.groups.filter(g=>g.eventIndex===Number($('event').value));$('groups').replaceChildren();group=null;for(const g of groups){const b=document.createElement('button');b.textContent=label(g);const s=document.createElement('small');s.textContent=statuses[g.status]||g.status;b.append(s);b.onclick=()=>{group=g;for(const x of $('groups').children)x.classList.toggle('active',x===b);draw()};$('groups').append(b)}if($('groups').firstChild)$('groups').firstChild.click();else draw();}
function draw(){const svg=$('map');svg.replaceChildren();$('board').replaceChildren();if(!group){$('summary').textContent='Aucun groupe de panneau pour cette action.';$('reason').textContent='';$('proof').textContent='';return}
$('summary').textContent=label(group)+' : '+(statuses[group.status]||group.status);
$('reason').textContent=group.blockers.length?group.blockers.map(b=>reasons[b]||b).join(' · '):'Toutes les dépendances utilisées sont vérifiées.';
const action=data.actions.find(a=>a.eventIndex===group.eventIndex),chains=new Set(group.continuations.flatMap(c=>c.pieceIds));
const pieces=data.pieces.filter(p=>chains.has(p.id)||(p.segmentIndex>=action.segmentIndex-2&&p.segmentIndex<=action.segmentIndex));
const peers=data.groups.filter(g=>g.eventIndex===group.eventIndex&&g.signUid===group.signUid&&g.areaName===group.areaName);
const board=$('board'),w=group.boardWidthM,h=group.boardHeightM;board.setAttribute('viewBox','0 0 1000 230');
if(w>0&&h>0){const k=Math.min(880/w,155/h),bx=55,by=190;function bn(tag,attrs){const n=document.createElementNS(board.namespaceURI,tag);for(const [key,value]of Object.entries(attrs))n.setAttribute(key,value);board.append(n);return n}
bn('rect',{x:bx,y:by-h*k,width:w*k,height:h*k,fill:'#193f36',stroke:'#b899df'});const title=bn('text',{x:bx,y:22,fill:'#e5edf5','font-size':13});title.textContent=`${group.areaName} — ${w} × ${h} m — ${group.boardGeometry?.modelPath||'modèle non résolu'}`;
const seen=new Set();for(const peer of peers)for(const a of peer.arrows){if(!a.placement||seen.has(peer.groupId+'/'+a.objectId))continue;seen.add(peer.groupId+'/'+a.objectId);const p=a.placement.drawingPoint,color=peer.groupId===group.groupId?'#ffd179':'#78bcf5';bn('circle',{cx:bx+p[0]*k,cy:by-p[1]*k,r:4,fill:color});const text=bn('text',{x:bx+p[0]*k+7,y:by-p[1]*k-9,fill:color,'font-size':12});text.textContent=label(peer)+' · '+a.objectId;}const axis=bn('text',{x:bx,y:215,fill:'#c6d4e2','font-size':12});axis.textContent='Origine dessin (0,0) · X vers la droite · Y vers le haut · pointe provisoire tant que non mesurée';}
const arrows=peers.flatMap(g=>g.arrows.map(a=>({...a,groupLabel:label(g),groupId:g.groupId}))).filter(a=>$('placement').value==='ALL'||a.origin===$('placement').value);const points=pieces.flatMap(p=>p.points).concat(arrows.map(a=>a.position),arrows.flatMap(a=>a.placement?.contour||[]));
if(!points.length)return;let minX=Math.min(...points.map(p=>p[0])),maxX=Math.max(...points.map(p=>p[0])),minZ=Math.min(...points.map(p=>p[2])),maxZ=Math.max(...points.map(p=>p[2]));
const scale=Math.min(900/Math.max(10,maxX-minX),480/Math.max(10,maxZ-minZ));svg.setAttribute('viewBox','0 0 1000 550');const xy=p=>[50+(p[0]-minX)*scale,35+(maxZ-p[2])*scale];
function node(tag,attrs){const n=document.createElementNS('http://www.w3.org/2000/svg',tag);for(const [k,v]of Object.entries(attrs))n.setAttribute(k,v);svg.append(n);return n}
for(const p of pieces){const line=node('polyline',{points:p.points.map(xy).map(p=>p.join(',')).join(' '),fill:'none',stroke:chains.has(p.id)?'#68e3ce':p.selected?'#75879a':'#fa8c88','stroke-width':chains.has(p.id)?3:1.5});const t=document.createElementNS(svg.namespaceURI,'title');t.textContent=p.id;line.append(t)}
for(const a of arrows){const [x,y]=xy(a.position);if(a.placement){node('polygon',{points:a.placement.contour.map(xy).map(p=>p.join(',')).join(' '),fill:'none',stroke:'#b899df','stroke-width':1});const n=a.placement.worldNormal;const end=xy([a.position[0]+n[0]*3,a.position[1]+n[1]*3,a.position[2]+n[2]*3]);node('line',{x1:x,y1:y,x2:end[0],y2:end[1],stroke:'#b899df','stroke-width':2});const c=a.placement.contour;for(const [end,color] of [[c[1],'#ed8585'],[c[3],'#86d9a9']]){const s=xy(c[0]),e=xy(end);node('line',{x1:s[0],y1:s[1],x2:e[0],y2:e[1],stroke:color,'stroke-width':2});}}const color=a.groupId===group.groupId?'#ffd179':'#78bcf5';node('circle',{cx:x,cy:y,r:6,fill:color});const t=node('text',{x:x+9,y:y-8,fill:color,'font-size':12});t.textContent=a.groupLabel+' · '+a.origin+' · '+a.objectId}
const ports=new Set(pieces.flatMap(p=>[p.startPort,p.endPort]));const boundaries=data.boundaries.filter(b=>ports.has(b.fromPort)&&ports.has(b.toPort));
$('proof').textContent=JSON.stringify({group,otherGroupsOnSameBoard:peers.filter(g=>g.groupId!==group.groupId),selectedAction:action,boundaries},null,2);}
$('event').onchange=refresh;$('placement').onchange=draw;refresh();
</script></html>
""";
}
