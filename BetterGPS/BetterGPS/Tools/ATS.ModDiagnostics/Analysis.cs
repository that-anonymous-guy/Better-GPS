using System.Globalization;
using System.Net;
using System.Text;
using System.Text.Json;

record Frame(double Seconds,double Ms);
sealed class Suspect
{
    public string Name {get;set;}="";
    public string Path {get;set;}="";
    public string Category {get;set;}="";
    public long ReadBytes {get;set;}
    public long NearSpikeBytes {get;set;}
    public int ReadCount {get;set;}
    public HashSet<int> SpikeIds {get;set;}=[];
    public List<AssetReading> Assets {get;set;}=[];
    public string IndexStatus {get;set;}="";
}
record AssetReading(string Name,long ReadBytes,long ExpandedBytes,int Spikes,string Precision);
record Spike(int Id,double Seconds,double Ms,string[] Archives);
sealed class Findings
{
    public string Label {get;set;}="";
    public string Confidence {get;set;}="CORRELATION_ONLY";
    public string ConfirmedCause {get;set;}="Aucune : confirmer par essai A/B sur le même passage";
    public double? AverageFps {get;set;}
    public double? Low1PercentFps {get;set;}
    public double? P99Ms {get;set;}
    public double? MaxMs {get;set;}
    public int Frames {get;set;}
    public int Over50Ms {get;set;}
    public int LostEvents {get;set;}
    public int DroppedEvents {get;set;}
    public List<Suspect> Suspects {get;set;}=[];
    public List<Spike> Spikes {get;set;}=[];
    public List<string> Notes {get;set;}=[];
    public List<string> LogIssues {get;set;}=[];
    public List<Mount> Mounts {get;set;}=[];
    public List<ResourceEvent> ResourcePeaks {get;set;}=[];
    public List<object> CpuModules {get;set;}=[];
    public List<PerformanceWindow> PerformanceWindows {get;set;}=[];
}
static class Analysis
{
    static string E(object? value)=>WebUtility.HtmlEncode(Convert.ToString(value,CultureInfo.InvariantCulture)??"");
    static string MiB(long bytes)=>(bytes/1048576d).ToString("0.0",CultureInfo.InvariantCulture);
    internal static List<Frame> Frames(CaptureData data)
    {
        bool Focused(double t)
        {
            if(data.IncludeUnfocused)return true;
            int lo=0,hi=data.Focus.Count;while(lo<hi){int mid=lo+(hi-lo)/2;if(data.Focus[mid].Seconds<=t)lo=mid+1;else hi=mid;}
            return lo>0&&data.Focus[lo-1].Active;
        }
        var groups=data.Presents.GroupBy(p=>p.Chain).OrderByDescending(g=>g.Count(p=>Focused(p.Seconds))).ToArray();if(groups.Length==0)return [];
        var result=new List<Frame>();PresentEvent? previous=null;
        foreach(var present in groups[0].OrderBy(p=>p.Seconds))
        {
            if(!Focused(present.Seconds)){previous=null;continue;}
            if(previous is not null)
            {
                // Exclude a focus transition even if both endpoint presents were focused.
                bool lostFocus=!data.IncludeUnfocused&&data.Focus.Any(f=>f.Seconds>previous.Seconds&&f.Seconds<=present.Seconds&&!f.Active);
                var ms=(present.Seconds-previous.Seconds)*1000;if(!lostFocus&&ms>0)result.Add(new(present.Seconds,ms));
            }
            previous=present;
        }
        return result;
    }
    internal static Findings Build(CaptureData data,bool resolveAssets=true)
    {
        var frames=Frames(data);var result=new Findings{Label=data.Label,Frames=frames.Count,LostEvents=data.LostEvents,DroppedEvents=data.DroppedPresents+data.DroppedReads+data.DroppedCpuSamples,LogIssues=data.LogIssues,Mounts=data.Mounts};
        result.PerformanceWindows=DeepAnalysis.Windows(data,frames);
        if(frames.Count>0)
        {
            var ordered=frames.Select(f=>f.Ms).Order().ToArray();int worst=Math.Max(1,(int)Math.Ceiling(frames.Count*.01));
            result.AverageFps=1000/ordered.Average();result.Low1PercentFps=1000/ordered.TakeLast(worst).Average();result.P99Ms=ordered[Math.Max(0,(int)Math.Ceiling(ordered.Length*.99)-1)];result.MaxMs=ordered[^1];result.Over50Ms=frames.Count(f=>f.Ms>50);
        }
        var spikes=frames.Where(f=>f.Ms>1000d/30).OrderByDescending(f=>f.Ms).Take(30).OrderBy(f=>f.Seconds).ToArray();
        bool Near(ReadEvent read,Frame frame)=>read.Seconds>=frame.Seconds-frame.Ms/1000-.25&&read.Seconds<=frame.Seconds+.25;
        var byPath=new Dictionary<string,Suspect>(StringComparer.OrdinalIgnoreCase);var readsByOwner=new Dictionary<string,List<ReadEvent>>(StringComparer.OrdinalIgnoreCase);
        foreach(var read in data.Reads)
        {
            var owner=Inventory.Owner(data.Mounts,read.Path);string category=owner?.Pool=="mod"?"Mod":owner is not null?"Jeu / DLC":read.Path.Contains("\\BetterGPS\\",StringComparison.OrdinalIgnoreCase)?"BetterGPS":"Autre / non attribué";
            string path=owner?.Path??read.Path;
            if(!byPath.TryGetValue(path,out var suspect))
            {
                byPath[path]=suspect=new(){Path=path,Name=owner?.Name??System.IO.Path.GetFileName(path),Category=category};readsByOwner[path]=[];
            }
            suspect.ReadCount++;suspect.ReadBytes+=Math.Max(0,read.Bytes);readsByOwner[path].Add(read);bool near=false;
            for(int i=0;i<spikes.Length;i++)if(Near(read,spikes[i])){near=true;suspect.SpikeIds.Add(i);}
            if(near)suspect.NearSpikeBytes+=Math.Max(0,read.Bytes);
        }
        result.Suspects=byPath.Values.OrderByDescending(s=>s.SpikeIds.Count).ThenByDescending(s=>s.NearSpikeBytes).ThenByDescending(s=>s.ReadBytes).ToList();
        foreach(var suspect in result.Suspects)
        {
            var mount=Inventory.Owner(data.Mounts,suspect.Path);
            if(resolveAssets&&mount?.Pool=="mod")suspect.Name=Inventory.DisplayName(mount);
            if(mount?.Directory==true)
            {
                suspect.IndexStatus="Fichiers physiques dans un mod dossier";
                suspect.Assets=readsByOwner[suspect.Path].GroupBy(r=>r.Path,StringComparer.OrdinalIgnoreCase).Select(g=>new AssetReading(System.IO.Path.GetRelativePath(mount.Path,g.Key),g.Sum(r=>(long)Math.Max(0,r.Bytes)),0,spikes.Count(f=>g.Any(r=>Near(r,f))),"Chemin physique exact")).OrderByDescending(a=>a.Spikes).ThenByDescending(a=>a.ReadBytes).Take(40).ToList();
            }
            else if(resolveAssets&&mount is not null&&mount.Pool=="mod")
            {
                suspect.Name=Inventory.DisplayName(mount);
                if(result.Suspects.Where(s=>s.Category=="Mod").Take(20).Contains(suspect))try
                {
                    var index=Inventory.ZipIndex(suspect.Path);var assets=new Dictionary<string,(long Bytes,long Expanded,HashSet<int> Spikes)>(StringComparer.Ordinal);
                    foreach(var read in readsByOwner[suspect.Path])foreach(var entry in Inventory.Overlaps(index,read.Offset,read.Bytes))
                    {
                        long overlap=Math.Max(0,Math.Min(entry.End,read.Offset+read.Bytes)-Math.Max(entry.Start,read.Offset));
                        if(!assets.TryGetValue(entry.Name,out var asset))asset=(0,entry.ExpandedBytes,[]);asset.Bytes+=overlap;
                        for(int i=0;i<spikes.Length;i++)if(Near(read,spikes[i]))asset.Spikes.Add(i);assets[entry.Name]=asset;
                    }
                    suspect.IndexStatus="ZIP : plages physiques des entrées (données ou en-têtes), sans décompression";
                    suspect.Assets=assets.Select(p=>new AssetReading(p.Key,p.Value.Bytes,p.Value.Expanded,p.Value.Spikes.Count,"Entrée ZIP couvrant la plage lue ; pas une mesure de coût GPU")).OrderByDescending(a=>a.Spikes).ThenByDescending(a=>a.ReadBytes).Take(40).ToList();
                }
                catch(Exception e) when(e is IOException or InvalidDataException or UnauthorizedAccessException or OverflowException or ArgumentException){suspect.IndexStatus="Conteneur identifié, fichier interne non résolu : "+e.Message;}
                else suspect.IndexStatus="Indexation limitée aux 20 premiers mods lus ; le conteneur reste identifié";
            }
        }
        for(int i=0;i<spikes.Length;i++)result.Spikes.Add(new(i,spikes[i].Seconds,spikes[i].Ms,result.Suspects.Where(s=>s.SpikeIds.Contains(i)).Select(s=>s.Name).ToArray()));
        result.ResourcePeaks=data.Resources.GroupBy(r=>r.Name).Select(g=>g.MaxBy(r=>r.PrivateBytes)!).ToList();
        result.CpuModules=data.CpuSamples.GroupBy(s=>data.Modules.FirstOrDefault(m=>s.Address>=unchecked((ulong)m.Base)&&s.Address-unchecked((ulong)m.Base)<(ulong)m.Size)?.Path??"Adresse non résolue").OrderByDescending(g=>g.Count()).Take(25).Select(g=>(object)new{module=g.Key,samples=g.Count(),share=data.CpuSamples.Count>0?g.Count()/(double)data.CpuSamples.Count:0}).ToList();
        result.Notes.AddRange(data.Warnings);
        if(data.DeepCapture)result.Notes.Add("Capture approfondie : échantillonnage CPU avec piles, cinq threads ATS les plus occupés à chaque relevé, et télémétrie NVIDIA globale si disponible. La surcharge du diagnostic peut affecter les mesures : conserver le même mode pour comparer.");
        long totalReads=data.TotalReadEvents>0?data.TotalReadEvents:data.Reads.Count+data.UnnamedReads+data.DroppedReads;
        if(totalReads>0)result.Notes.Add($"Couverture des noms : {data.Reads.Count}/{totalReads} événements de lecture ({100d*data.Reads.Count/totalReads:0.0} %). {data.RecoveredReads} lectures récupérées par les noms tardifs. Un classement partiel ne permet pas d'innocenter les fichiers non identifiés.");
        if(data.IncludeUnfocused)result.Notes.Add("MODE DE TEST : les Present hors premier plan sont inclus, les statistiques ne sont pas comparables au mode normal.");
        if(data.UnnamedReads>0)result.Notes.Add($"{data.UnnamedReads} lectures ATS sans nom de fichier résolu ; elles ne sont attribuées à aucun mod.");
        result.Notes.Add("Classement par corrélation avec les 30 plus grands intervalles > 33,33 ms ; fenêtre : intervalle d'image et ±250 ms. Des lectures fréquentes peuvent coïncider par hasard.");
        result.Notes.Add("Cadence CPU des Present DXGI, swapchain dominante et premier plan. Ce ne sont pas les images réellement affichées, ni une mesure du temps GPU.");
        result.Notes.Add("Lectures logiques demandées par ATS ; pas le trafic physique du SSD. Les accès mémoire mappés ou déjà en cache/VRAM peuvent échapper à ce classement.");
        result.Notes.Add("Une archive lue n'est pas une cause prouvée. Les textures et géométries déjà chargées, shaders, IA et scripts ne sont pas automatiquement attribuables à un mod par ces lectures.");
        result.Notes.Add("HashFS SCS et archives non ZIP : attribution au conteneur uniquement. Les mods actifs sans lecture observée ne sont pas déclarés innocents.");
        if(frames.Count==0)result.Notes.Add("FPS indisponibles : aucun intervalle DXGI exploitable au premier plan. Aucun classement causal des saccades n'est possible ; marqueurs et lectures restent consultables.");
        if(result.LostEvents>0||result.DroppedEvents>0)result.Notes.Add("CAPTURE PARTIELLE : des événements ont été perdus ou plafonnés ; les résultats peuvent manquer des lectures ou des saccades.");
        return result;
    }
    internal static void Save(CaptureData data,string output)
    {
        Directory.CreateDirectory(output);var findings=Build(data);File.WriteAllText(System.IO.Path.Combine(output,"rapport.json"),JsonSerializer.Serialize(findings,App.Json));
        var b=new StringBuilder(Header("ATS — diagnostic des mods"));
        b.Append($"<h1>ATS — diagnostic des mods</h1><p>Capture : {E(data.StartedUtc.ToLocalTime())} · {data.Seconds:0.0} s · {findings.Frames} intervalles · pertes ETW : {data.LostEvents} · plafonnés : {findings.DroppedEvents}</p>");
        b.Append($"<div class='cards'><article>FPS moyens<strong>{findings.AverageFps?.ToString("0.0")??"Indisponible"}</strong></article><article>1 % low<strong>{findings.Low1PercentFps?.ToString("0.0")??"Indisponible"}</strong></article><article>Maximum<strong>{findings.MaxMs?.ToString("0.0")??"—"} ms</strong></article><article>Intervalles > 50 ms<strong>{findings.Over50Ms}</strong></article></div>");
        b.Append(DeepAnalysis.Html(data,findings.PerformanceWindows));
        b.Append("<h2>Fichiers et mods observés autour des saccades</h2><p>Suspects à vérifier, jamais verdict automatique. Ouvrir une ligne pour voir les fichiers. Un gros fichier ou une erreur de journal ne prouve pas un ralentissement.</p><input id='filter' placeholder='Filtrer un mod ou un chemin…'><div id='suspects'>");
        foreach(var s in findings.Suspects.Take(100))
        {
            b.Append($"<details class='suspect'><summary><b>{E(s.Name)}</b> · {E(s.Category)} · {s.SpikeIds.Count} pics voisins · {MiB(s.NearSpikeBytes)} Mio voisins / {MiB(s.ReadBytes)} Mio lus</summary><p><code>{E(s.Path)}</code></p><p>{E(s.IndexStatus)}</p><table><tr><th>Fichier</th><th>Lecture attribuée</th><th>Taille décompressée</th><th>Pics voisins</th></tr>");
            foreach(var a in s.Assets)b.Append($"<tr><td title='{E(a.Precision)}'>{E(a.Name)}</td><td>{MiB(a.ReadBytes)} Mio</td><td>{(a.ExpandedBytes>0?MiB(a.ExpandedBytes)+" Mio":"—")}</td><td>{a.Spikes}</td></tr>");b.Append("</table></details>");
        }
        b.Append("</div><h2>Les pics</h2><table><tr><th>Moment</th><th>Intervalle</th><th>Conteneurs lus dans la fenêtre</th></tr>");
        foreach(var spike in findings.Spikes)b.Append($"<tr><td>{spike.Seconds:0.00} s</td><td>{spike.Ms:0.00} ms</td><td>{E(string.Join(" · ",spike.Archives))}</td></tr>");
        b.Append($"</table><h2>Marqueurs Ctrl+Maj+F8</h2><p>{E(data.Markers.Count>0?string.Join(" · ",data.Markers.Select(m=>m.ToString("0.00")+" s")):"Aucun marqueur reçu")}</p>");
        foreach(var marker in data.Markers)
        {
            b.Append($"<details><summary>Lectures près du marqueur {marker:0.00} s</summary><ul>");
            foreach(var group in data.Reads.Where(r=>Math.Abs(r.Seconds-marker)<=.75).GroupBy(r=>r.Path).OrderByDescending(g=>g.Sum(r=>(long)r.Bytes)).Take(15))b.Append($"<li>{E(group.Key)} · {MiB(group.Sum(r=>(long)r.Bytes))} Mio</li>");b.Append("</ul></details>");
        }
        b.Append("<h2>Ressources des processus</h2><p>Pic mémoire privée et activité CPU échantillonnée dans les données brutes. CPU exprimé en cœurs, pas en % machine.</p><table><tr><th>Processus</th><th>Pic privé</th><th>CPU moyen</th></tr>");
        foreach(var g in data.Resources.GroupBy(r=>r.Name))b.Append($"<tr><td>{E(g.Key)}</td><td>{MiB(g.Max(r=>r.PrivateBytes))} Mio</td><td>{g.Average(r=>r.CpuCores):0.000}</td></tr>");
        b.Append("</table><h2>Modules CPU (option --cpu-samples)</h2><p>Adresse exécutée au moment de l'échantillon ; pas une pile complète, pas un temps CPU par asset.</p><pre>"+E(JsonSerializer.Serialize(findings.CpuModules,App.Json))+"</pre>");
        b.Append("<h2>Mods montés et noms</h2><ul>");foreach(var m in data.Mounts.Where(m=>m.Pool=="mod"))b.Append($"<li>{E(m.Name)} — <code>{E(m.Path)}</code></li>");
        b.Append("</ul><h2>Erreurs et avertissements du journal</h2><p>Contexte de la session entière, sans attribution temporelle automatique aux pics.</p><pre>"+E(string.Join('\n',findings.LogIssues))+"</pre><h2>Interprétation et limites</h2><ul>");
        foreach(var note in findings.Notes)b.Append("<li>"+E(note)+"</li>");
        b.Append("</ul><h2>Confirmer un suspect</h2><p>Rejouer la même sauvegarde, le même passage et les mêmes réglages. Tester le suspect avec ses dépendances, puis comparer les rapports. Ne pas désactiver au hasard les dépendances de cartes d'une sauvegarde. Cet outil ne modifie ni mods ni profils.</p><script>document.getElementById('filter').oninput=function(){let q=this.value.toLowerCase();document.querySelectorAll('.suspect').forEach(e=>e.hidden=!e.textContent.toLowerCase().includes(q))}</script></body></html>");
        File.WriteAllText(System.IO.Path.Combine(output,"rapport.html"),b.ToString());
    }
    internal static void Compare(string baseline,string candidate,string output)
    {
        var a=App.ReadCapture(baseline);var b=App.ReadCapture(candidate);
        var x=Build(a,false);var y=Build(b,false);var html=new StringBuilder(Header("ATS — comparaison"));html.Append("<h1>Comparaison A/B</h1><p>Cette différence ne prouve pas une cause si parcours, météo, trafic, cache et réglages diffèrent.</p><table><tr><th>Mesure</th><th>Référence</th><th>Candidat</th></tr>");
        foreach(var row in new[]{("FPS moyens",x.AverageFps,y.AverageFps),("1 % low",x.Low1PercentFps,y.Low1PercentFps),("P99 ms",x.P99Ms,y.P99Ms),("Maximum ms",x.MaxMs,y.MaxMs)})html.Append($"<tr><td>{E(row.Item1)}</td><td>{row.Item2?.ToString("0.00")??"Indisponible"}</td><td>{row.Item3?.ToString("0.00")??"Indisponible"}</td></tr>");
        html.Append("</table><h2>Montages différents</h2><pre>Retirés :\n"+E(string.Join('\n',a.Mounts.Select(m=>m.Path).Except(b.Mounts.Select(m=>m.Path),StringComparer.OrdinalIgnoreCase)))+"\n\nAjoutés :\n"+E(string.Join('\n',b.Mounts.Select(m=>m.Path).Except(a.Mounts.Select(m=>m.Path),StringComparer.OrdinalIgnoreCase)))+"</pre></body></html>");
        output=System.IO.Path.GetFullPath(output);Directory.CreateDirectory(System.IO.Path.GetDirectoryName(output)!);File.WriteAllText(output,html.ToString());Console.WriteLine(output);
    }
    static string Header(string title)=>"<!doctype html><html lang='fr'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>"+E(title)+"</title><style>body{font:16px system-ui;margin:32px auto;padding:0 24px;max-width:1300px;background:#151719;color:#e7e7e7}h1,h2{color:#e5a052}p,li{line-height:1.6}code,pre,td{overflow-wrap:anywhere}pre{white-space:pre-wrap;background:#202326;padding:16px}.cards{display:flex;gap:16px;flex-wrap:wrap}article,details{background:#25292d;padding:16px;border-radius:8px;margin:12px 0}strong{display:block;font-size:28px}summary{cursor:pointer}table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:12px;border-bottom:1px solid #42464a}input{padding:12px;width:80%;background:#25292d;color:white;border:1px solid #62686d;border-radius:6px}</style><body>";
}
