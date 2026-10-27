using System.Collections;
using System.Globalization;
using System.Text.RegularExpressions;
using TruckLib;
using TruckLib.Sii;

namespace BetterGPSNavigation;

internal sealed record SignBoardInfo(
    string BoardToken,
    string BoardPath,
    string Status,
    string SemanticKind,
    string ShieldPrefixHint,
    int UnitCount);

/// <summary>
/// v0.11 resolver for /def/sign/boards*.sii -> /def/sign/board/*.sii.
///
/// Templates tell us which board is bound to an AreaName, but the board definition
/// itself is useful context for interpreting otherwise ambiguous dynamic strings.
/// This resolver deliberately exposes schema/path metadata and only makes conservative
/// semantic inferences. A numeric string is never promoted to a route shield unless the
/// board path itself supplies a recognizable route-system prefix.
/// </summary>
internal sealed class SignBoardResolver
{
    private readonly IFileSystem fs;
    private readonly SignTemplateResolver templates;
    private readonly IReadOnlyList<ParsedSign> signs;
    private readonly Dictionary<string, string> boardPaths = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, SignBoardInfo> infos = new(StringComparer.OrdinalIgnoreCase);
    private readonly List<SchemaRow> schemaRows = [];
    private bool loaded;

    private sealed record SchemaRow(
        string BoardToken,
        string BoardPath,
        string Status,
        string UnitClass,
        string UnitName,
        string Attribute,
        string ValueType,
        string Value);

    private static readonly Regex PureRouteNumber = new(@"^\d{1,4}[A-Z]?$", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex StateRoutePath = new(@"(?:^|[/_])rte_(?<state>[a-z]{2})(?:[_/]|$)", RegexOptions.IgnoreCase | RegexOptions.Compiled);

    private readonly bool lazy;
    public SignBoardResolver(IFileSystem fs, SignTemplateResolver templates, IReadOnlyList<ParsedSign> signs,bool lazy=false)
    {
        this.fs = fs;
        this.templates = templates;
        this.signs = signs;
        this.lazy=lazy;
    }

    public int BoardDefinitionCount { get { EnsureLoaded(); return boardPaths.Count; } }
    public int UsedBoardCount { get { EnsureLoaded(); return infos.Count; } }
    public int ResolvedUsedBoardCount { get { EnsureLoaded(); return infos.Values.Count(x => x.Status == "OK"); } }

    public SignBoardInfo? Find(string boardToken)
    {
        EnsureLoaded();
        if(lazy&&!string.IsNullOrWhiteSpace(boardToken)&&!infos.ContainsKey(boardToken)&&boardPaths.ContainsKey(boardToken))infos[boardToken]=LoadBoard(boardToken);
        return infos.TryGetValue(boardToken ?? "", out var info) ? info : null;
    }

    public string EffectiveBoardToken(ParsedSign sign, ParsedSignArea area)
    {
        if (!string.IsNullOrWhiteSpace(area.BoardToken) && area.BoardToken != "0")
            return area.BoardToken.StartsWith("sign_board.",StringComparison.OrdinalIgnoreCase) ? area.BoardToken : "sign_board."+area.BoardToken;
        return templates.FindAreaBinding(sign.Template, area.AreaName)?.BoardToken ?? "";
    }

    public string InferShieldIdentity(ParsedSign sign, ParsedSignArea area)
    {
        if (area.RoadShields.Count == 1) return area.RoadShields[0];
        if (area.RoadShields.Count > 1) return "";

        var token = EffectiveBoardToken(sign, area);
        var info = Find(token);
        if (info is null || info.Status != "OK" || !info.SemanticKind.Contains("SHIELD", StringComparison.Ordinal)) return "";
        if (string.IsNullOrWhiteSpace(info.ShieldPrefixHint)) return "";

        var numbers = area.RawStrings
            .Select(x => x.Trim().ToUpperInvariant())
            .Where(x => PureRouteNumber.IsMatch(x))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToList();
        if (numbers.Count != 1) return "";
        return $"{info.ShieldPrefixHint}-{numbers[0]}";
    }

    public void WriteIndex(string path)
    {
        EnsureLoaded();
        using var w = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        w.WriteLine("board_token,board_path,status,semantic_kind,shield_prefix_hint,unit_count");
        foreach (var i in infos.Values.OrderBy(x => x.BoardToken, StringComparer.OrdinalIgnoreCase))
            w.WriteLine(string.Join(',', Csv(i.BoardToken), Csv(i.BoardPath), Csv(i.Status), Csv(i.SemanticKind), Csv(i.ShieldPrefixHint), i.UnitCount));
    }

    public void WriteSchema(string path)
    {
        EnsureLoaded();
        using var w = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        w.WriteLine("board_token,board_path,status,unit_class,unit_name,attribute,value_type,value");
        foreach (var r in schemaRows.OrderBy(r => r.BoardToken, StringComparer.OrdinalIgnoreCase).ThenBy(r => r.UnitName, StringComparer.OrdinalIgnoreCase).ThenBy(r => r.Attribute, StringComparer.OrdinalIgnoreCase))
            w.WriteLine(string.Join(',', Csv(r.BoardToken), Csv(r.BoardPath), Csv(r.Status), Csv(r.UnitClass), Csv(r.UnitName), Csv(r.Attribute), Csv(r.ValueType), Csv(r.Value)));
    }

    private void EnsureLoaded()
    {
        if (loaded) return;
        loaded = true;
        var paths=AssetIndexCache.GetShared(fs,"board-paths",()=>{LoadBoardDefinitions();return boardPaths;});
        foreach(var pair in paths)boardPaths[pair.Key]=pair.Value;
        if(lazy)return;

        var used = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var sign in signs)
        {
            foreach (var area in sign.Areas)
            {
                var token = EffectiveBoardToken(sign, area);
                if (!string.IsNullOrWhiteSpace(token)) used.Add(token);
            }
        }
        foreach (var token in used.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
            infos[token] = LoadBoard(token);
    }

    private void LoadBoardDefinitions()
    {
        if (!fs.DirectoryExists("/def/sign/")) return;
        foreach (var file in fs.GetFiles("/def/sign/")
                     .Where(f => f.EndsWith(".sii", StringComparison.OrdinalIgnoreCase))
                     .Where(f => Path.GetFileName(f).StartsWith("boards", StringComparison.OrdinalIgnoreCase))
                     .OrderBy(f => f, StringComparer.OrdinalIgnoreCase))
        {
            SiiFile sii;
            try { sii = SiiDocumentCache.Open(file, fs); }
            catch { continue; }
            foreach (var unit in sii.Units)
            {
                if (!unit.Name.StartsWith("sign_board.", StringComparison.OrdinalIgnoreCase)) continue;
                if (!unit.Attributes.TryGetValue("path", out var rawPath)) continue;
                var path = NormalizePath(SignTemplateResolver.CleanToken(rawPath));
                if (path.Length > 0) boardPaths[unit.Name] = path;
            }
        }
    }

    private SignBoardInfo LoadBoard(string token)
    {
        var key=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(token)));
        var cached=AssetIndexCache.GetShared(fs,"board-info-"+key,()=>{
            int start=schemaRows.Count;var info=LoadBoardFresh(token);var rows=schemaRows.Skip(start).ToArray();schemaRows.RemoveRange(start,rows.Length);
            return new CachedBoard(info,rows);
        });
        schemaRows.AddRange(cached.Rows);return cached.Info;
    }
    private sealed record CachedBoard(SignBoardInfo Info,SchemaRow[] Rows);
    private SignBoardInfo LoadBoardFresh(string token)
    {
        if (!boardPaths.TryGetValue(token, out var path))
        {
            schemaRows.Add(new SchemaRow(token, "", "NO_BOARD_DEF", "", "", "", "", ""));
            return new SignBoardInfo(token, "", "NO_BOARD_DEF", "UNKNOWN", "", 0);
        }
        if (!fs.FileExists(path))
        {
            schemaRows.Add(new SchemaRow(token, path, "FILE_MISSING", "", "", "", "", ""));
            return new SignBoardInfo(token, path, "FILE_MISSING", ClassifyKind(path, []), InferShieldPrefix(path), 0);
        }

        SiiFile sii;
        try { sii = SiiDocumentCache.Open(path, fs); }
        catch (Exception ex)
        {
            schemaRows.Add(new SchemaRow(token, path, "PARSE_ERROR", "", "", "", ex.GetType().Name, ex.Message));
            return new SignBoardInfo(token, path, "PARSE_ERROR", ClassifyKind(path, []), InferShieldPrefix(path), 0);
        }

        var attrs = new List<string>();
        foreach (var unit in sii.Units)
        {
            if (unit.Attributes.Count == 0)
                schemaRows.Add(new SchemaRow(token, path, "OK", unit.Class, unit.Name, "", "", ""));
            foreach (var kv in unit.Attributes)
            {
                var v = ValueToString(kv.Value);
                attrs.Add(kv.Key + "=" + v);
                schemaRows.Add(new SchemaRow(token, path, "OK", unit.Class, unit.Name, kv.Key, kv.Value?.GetType().FullName ?? "null", v));
            }
        }
        return new SignBoardInfo(token, path, "OK", ClassifyKind(path, attrs), InferShieldPrefix(path), sii.Units.Count);
    }

    private static string ClassifyKind(string path, IReadOnlyList<string> attrs)
    {
        var s = (path + " " + string.Join(' ', attrs)).ToLowerInvariant();
        var kinds = new List<string>();
        if (s.Contains("shield") || s.Contains("rte_") || s.Contains("route")) kinds.Add("SHIELD");
        if (s.Contains("exit")) kinds.Add("EXIT");
        if (s.Contains("arrow")) kinds.Add("ARROW");
        if (s.Contains("street") || s.Contains("road_name") || s.Contains("destination")) kinds.Add("TEXT");
        return kinds.Count == 0 ? "UNKNOWN" : string.Join('+', kinds.Distinct());
    }

    private static string InferShieldPrefix(string path)
    {
        var p = path.ToLowerInvariant().Replace('\\', '/');
        if (p.Contains("interstate") || Regex.IsMatch(p, @"(?:^|[/_])(?:is|i)_?shield(?:[_/]|$)")) return "I";
        if (p.Contains("us_highway") || p.Contains("ushw") || Regex.IsMatch(p, @"(?:^|[/_])us_(?:hw|shield|rte)")) return "US";
        var m = StateRoutePath.Match(p);
        if (m.Success) return m.Groups["state"].Value.ToUpperInvariant();
        return "";
    }

    private static string NormalizePath(string path)
    {
        var p = (path ?? "").Trim().Trim('"').Replace('\\', '/');
        if (p.Length == 0) return "";
        return p.StartsWith('/') ? p : "/" + p;
    }

    private static string ValueToString(object? value)
    {
        if (value is null) return "";
        var cleaned = SignTemplateResolver.CleanToken(value);
        if (!cleaned.StartsWith(value.GetType().Name + " {", StringComparison.Ordinal) && cleaned != value.ToString())
            return cleaned;
        if (value is string s) return s;
        if (value is System.Collections.IEnumerable sequence)
        {
            var vals = new List<string>();
            foreach (var item in sequence) vals.Add(SignTemplateResolver.CleanToken(item));
            return string.Join('|', vals);
        }
        return Convert.ToString(value, CultureInfo.InvariantCulture) ?? value.ToString() ?? "";
    }

    private static string Csv(string? value)
    {
        value ??= "";
        if (!value.Contains(',') && !value.Contains('"') && !value.Contains('\n') && !value.Contains('\r')) return value;
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}
