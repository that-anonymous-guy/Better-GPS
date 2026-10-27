using System.Text.Json;

namespace BetterGPSNavigation;

internal static class NavigationPreview
{
    public static void Write(LiveRoute route,string output)
    {
        var engine=new LiveGuidanceEngine();
        engine.Accept(new(2,"Hello","preview",0,0,0));
        engine.Accept(new(2,"RouteSnapshot","preview",1,0,1,route));
        var frames=new List<object>();long sequence=2;
        foreach(var edge in route.Edges)
        {
            int steps=Math.Max(1,(int)Math.Ceiling((edge.WorldEndM-edge.WorldStartM)/40));
            for(int i=0;i<steps;i++)
            {
                var t=(i+.5)/steps;
                var vehicle=new LiveVehicle(new(edge.Start.X+(edge.End.X-edge.Start.X)*t,
                    edge.Start.Y+(edge.End.Y-edge.Start.Y)*t,edge.Start.Z+(edge.End.Z-edge.Start.Z)*t),
                    Math.Atan2(edge.End.Z-edge.Start.Z,edge.End.X-edge.Start.X)*180/Math.PI,10,false);
                var guidance=engine.Accept(new(2,"VehicleState","preview",sequence,sequence++*100,1,Vehicle:vehicle));
                string Label(LiveRoadContext? road)=>road is {Status:"RESOLVED"}?
                    string.Join(" / ",road.Labels.Select(l=>l.Identity+(l.Cardinal.Length>0?" "+l.Cardinal:""))):"Nom non résolu";
                var signed=guidance.SignTargets?.Select(s=>s.Road+(s.Cardinal.Length>0?" "+s.Cardinal:"")).Distinct().ToArray()??[];
                var target=Label(guidance.TargetRoad);
                if(target=="Nom non résolu"&&guidance.JoinedRoad is {Status:"RESOLVED"})target="Vers "+Label(guidance.JoinedRoad);
                if(target=="Nom non résolu"&&signed.Length==1)target="Vers "+signed[0];
                var current=Label(guidance.CurrentRoad);
                var continuing=guidance.Status=="CRUISE";
                var headline=continuing&&current!="Nom non résolu"?"↑ Continue on "+current:null;
                frames.Add(new{instruction=NativeBridgeCli.Describe(guidance),action=guidance.Instruction,actionId=guidance.ActionId,
                    distance=guidance.DistanceMi,current,target,headline,
                    cruising=guidance.Status=="CRUISE"&&guidance.CurrentRoad is {Status:"RESOLVED"},
                    continueDistance=guidance.ContinueDistanceM/1609.344,
                    then=guidance.ThenInstruction is null?"—":NativeBridgeCli.SimpleAction(guidance.ThenInstruction),
                    status=guidance.Status,segment=edge.SegmentIndex,
                    progress=(edge.NavStartM+(edge.NavEndM-edge.NavStartM)*t)/route.Edges[^1].NavEndM});
            }
        }
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        File.WriteAllText(output,"""
<!doctype html><html lang="fr"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>BetterGPS — aperçu du trajet</title><style>
body{margin:0;background:#0b111c;color:#ecf3ff;font:18px system-ui}main{max-width:950px;margin:60px auto;padding:24px}
h1{font-size:26px;color:#72b9ff}.note{color:#a8b8cd;font-size:14px}.card{margin:30px 0;padding:32px;background:#172339;border-radius:20px}
#action{font-size:40px;font-weight:700}#distance{font-size:30px;color:#80d3b5;margin:12px 0}.roads{display:flex;gap:40px;flex-wrap:wrap}
label{display:block;font-size:14px;color:#a8b8cd}strong{font-size:23px;display:block;margin:8px 0 20px}#instruction{line-height:1.7}
input{width:100%;accent-color:#72b9ff}button,select{background:#233a59;color:white;border:0;border-radius:7px;padding:12px;margin:10px 8px 10px 0}
</style><main><h1>BetterGPS · Navigation</h1><p class="note">Trajet enregistré dans ATS. Les positions de cet aperçu sont simulées sur le trajet ; les noms affichés proviennent du moteur de guidage. Un nom non résolu reste signalé.</p>
<div class="card"><div id="action"></div><div id="distance"></div><div class="roads"><div><label>Route actuelle</label><strong id="current"></strong></div><div><label>Route visée</label><strong id="target"></strong></div><div><label>Puis</label><strong id="then"></strong></div></div><p id="instruction"></p></div>
<select id="decisions" aria-label="Choisir une action"></select><button id="play">▶ Lire le trajet</button><input id="position" aria-label="Position sur le trajet" type="range" min="0" value="0"><p id="progress" class="note"></p>
<script>const frames=
"""+JsonSerializer.Serialize(frames,LiveProtocol.Json)+"""
;const slider=document.getElementById('position');slider.max=frames.length-1;let timer;
const names={TURN_LEFT:'↰ Turn left',TURN_RIGHT:'↱ Turn right',KEEP_LEFT:'↖ Keep left',KEEP_RIGHT:'↗ Keep right',MERGE_LEFT:'↖ Merge left',MERGE_RIGHT:'↗ Merge right',CONTINUE:'↑ Continue',STAY:'↑ Stay'};
function show(){const f=frames[+slider.value];for(const id of ['current','target','then','instruction'])document.getElementById(id).textContent=f[id];document.getElementById('action').textContent=f.headline||names[f.action]||f.action||f.status;document.getElementById('distance').textContent=f.cruising?(f.continueDistance>=0.1?'For '+f.continueDistance.toFixed(1)+' mi':''):f.distance==null?'':f.status==='EXECUTE'?'Now':'In '+f.distance.toFixed(1)+' mi';document.getElementById('progress').textContent=Math.round(f.progress*100)+' % du trajet · portion '+f.segment;const option=Array.from(document.getElementById('decisions').options).find(o=>o.dataset.actionId===f.actionId);if(option)document.getElementById('decisions').value=option.value;}
slider.addEventListener('input',show);const choices=document.getElementById('decisions');const seen=new Set();
frames.forEach((f,i)=>{if(f.actionId&&!seen.has(f.actionId)){seen.add(f.actionId);const opt=document.createElement('option');opt.value=i;opt.dataset.actionId=f.actionId;opt.textContent=(names[f.action]||f.action)+' · '+(f.progress*100).toFixed(0)+' %';choices.appendChild(opt);}});
choices.addEventListener('change',()=>{slider.value=choices.value;show();});document.getElementById('play').addEventListener('click',()=>{if(timer){clearInterval(timer);timer=null;return;}timer=setInterval(()=>{slider.value=Math.min(+slider.value+1,+slider.max);show();if(slider.value===slider.max){clearInterval(timer);timer=null;}},100);});show();</script></main></html>
""");
    }
}
