using System.Globalization;
using System.Net;
using System.Text;

record WindowModule(string Name,int Samples,double Share);
record PerformanceWindow(int Start,double CoveredSeconds,double Fps,double? GpuPercent,double? UsedMiB,double? TotalMiB,double? HotThreadCores,int? HotThreadId,string Signal,List<WindowModule> Cpu);
static class DeepAnalysis
{
    internal static string Signal(double? gpu,double? cpu,double? used,double? total)
    {
        if(gpu>=95&&cpu>=.85)return "GPU très occupé et thread ATS très chargé : limites mixtes possibles";
        if(gpu>=95)return "GPU très occupé : piste de limite graphique (charge globale de la carte)";
        if(cpu>=.85&&gpu<85)return "Un thread ATS très chargé avec GPU moins occupé : piste CPU / soumission du rendu";
        if(total>0&&used/total>=.95)return "Mémoire GPU globale proche de la capacité : pression possible, évictions non mesurées";
        return "Limite non déterminée : aucune preuve suffisante de saturation CPU ou GPU";
    }
    internal static List<PerformanceWindow> Windows(CaptureData data,List<Frame> frames)
    {
        var result=new List<PerformanceWindow>();
        foreach(var group in frames.GroupBy(f=>(int)f.Seconds/2*2))
        {
            int start=group.Key;double covered=group.Sum(f=>f.Ms)/1000;
            if(covered<1.5)continue;
            var gpu=data.Gpu.Where(v=>v.Seconds-.5>=start&&v.Seconds-.5<start+2).GroupBy(v=>v.Index).OrderByDescending(g=>g.Average(v=>v.Utilization??0)).FirstOrDefault()?.ToArray()??[];
            double? Avg(Func<GpuReading,double?> select){var a=gpu.Select(select).Where(v=>v.HasValue).Select(v=>v!.Value).ToArray();return a.Length>0?a.Average():null;}
            var thread=data.Threads.Where(v=>v.Seconds-.25>=start&&v.Seconds-.25<start+2).GroupBy(v=>v.ThreadId).Select(g=>new{Id=g.Key,Cores=g.Average(v=>v.CpuCores)}).OrderByDescending(v=>v.Cores).FirstOrDefault();
            var samples=data.CpuSamples.Where(v=>v.Seconds>=start&&v.Seconds<start+2).ToArray();
            var modules=samples.GroupBy(v=>Module(data.Modules,v.Address)).OrderByDescending(g=>g.Count()).Take(8).Select(g=>new WindowModule(g.Key,g.Count(),samples.Length>0?g.Count()/(double)samples.Length:0)).ToList();
            double? util=Avg(v=>v.Utilization),used=Avg(v=>v.UsedMiB),total=Avg(v=>v.TotalMiB);
            result.Add(new(start,covered,group.Count()/covered,util,used,total,thread?.Cores,thread?.Id,Signal(util,thread?.Cores,used,total),modules));
        }
        return result;
    }
    internal static string Module(List<CpuModule> modules,ulong address)
    {
        var m=modules.FirstOrDefault(m=>address>=unchecked((ulong)m.Base)&&address-unchecked((ulong)m.Base)<(ulong)m.Size);
        return m is null?"Adresse non résolue":Path.GetFileName(m.Path);
    }
    internal static string Html(CaptureData data,List<PerformanceWindow> windows)
    {
        static string E(object? v)=>WebUtility.HtmlEncode(Convert.ToString(v,CultureInfo.InvariantCulture)??"");
        static string N(double? v,string suffix="")=>v.HasValue?v.Value.ToString("0.0",CultureInfo.InvariantCulture)+suffix:"Indisponible";
        var b=new StringBuilder("<h2>Zones lentes : CPU, GPU et mémoire</h2><p>Fenêtres de 2 secondes, triées des plus lentes aux plus rapides. La télémétrie NVIDIA représente toute la carte : elle ne donne pas le coût GPU d'un mod. Les threads sont échantillonnés toutes les 0,5 s ; ce tableau n'explique pas à lui seul un pic de quelques millisecondes.</p>");
        if(windows.All(v=>v.GpuPercent is null&&v.HotThreadCores is null))b.Append("<p>Ancienne capture : ces mesures manquent. Impossible de les reconstruire après coup.</p>");
        b.Append("<table><tr><th>Moment</th><th>FPS</th><th>GPU global</th><th>Mémoire GPU globale</th><th>Thread ATS le plus chargé</th><th>Interprétation</th></tr>");
        foreach(var v in windows.OrderBy(v=>v.Fps).Take(40))b.Append($"<tr><td>{v.Start}–{v.Start+2} s</td><td>{v.Fps:0.0}</td><td>{N(v.GpuPercent," %")}</td><td>{N(v.UsedMiB)} / {N(v.TotalMiB)} Mio</td><td>{N(v.HotThreadCores*100," % d'un cœur")} (ID {E(v.HotThreadId)})</td><td>{E(v.Signal)}</td></tr>");
        b.Append("</table><h2>Code exécuté dans ATS pendant les zones lentes</h2><p>Échantillons CPU exclusifs, toutes DLL comprises. Un faible temps CPU ne disculpe pas un coût GPU, des attentes ou un hook. Les ressources de mods exécutées par amtrucks.exe ne deviennent pas des DLL identifiables.</p>");
        foreach(var v in windows.OrderBy(v=>v.Fps).Take(8))
        {
            b.Append($"<details><summary>{v.Start}–{v.Start+2} s · {v.Fps:0.0} FPS</summary><ul>");
            foreach(var m in v.Cpu)b.Append($"<li>{E(m.Name)} : {100*m.Share:0.0} % des échantillons CPU ({m.Samples})</li>");
            b.Append("</ul></details>");
        }
        var slowSeconds=windows.Where(v=>v.Fps<65).Select(v=>v.Start).ToHashSet();var stacks=data.Stacks.Where(v=>slowSeconds.Contains(v.Second/2*2)).ToArray();
        b.Append("<h2>Piles CPU dans les fenêtres sous 65 FPS</h2><p>Chaînes de modules, du code exécuté vers ses appelants. Les nombres sont inclusifs ; une chaîne n'est ni un temps GPU ni une preuve qu'un asset est responsable. Aucun symbole privé ATS n'est disponible.</p><table><tr><th>Thread</th><th>Chaîne de modules</th><th>Échantillons</th></tr>");
        foreach(var g in stacks.GroupBy(v=>(v.ThreadId,v.Frames)).OrderByDescending(g=>g.Sum(v=>v.Samples)).Take(30))b.Append($"<tr><td>{g.Key.ThreadId}</td><td>{E(g.Key.Frames)}</td><td>{g.Sum(v=>v.Samples)}</td></tr>");
        if(stacks.Length==0)b.Append("<tr><td colspan='3'>Aucune pile CPU exploitable dans ces fenêtres.</td></tr>");
        return b.Append("</table>").ToString();
    }
}
