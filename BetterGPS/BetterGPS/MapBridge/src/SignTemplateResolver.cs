using System.Collections;
using System.Globalization;
using System.Numerics;
using TruckLib;
using TruckLib.Sii;

namespace BetterGPSNavigation;

internal sealed record SignTemplateAreaBinding(
    string AreaName,
    string BoardToken,
    double? LocalX,
    double? LocalY,
    string PositionSource,
    string UnitClass,
    string UnitName);

internal sealed record SignTemplateInfo(
    string TemplateName,
    string TemplatePath,
    string Status,
    IReadOnlyList<SignTemplateAreaBinding> Areas,
    int UnitCount)
{
    public string[] Includes {get;init;}=[];
}

/// <summary>
/// v0.10 sign-template discovery/resolution.
///
/// ATS sign instances reference sign_templ.* definitions from /def/sign/templates*.sii.
/// The referenced template file is parsed generically with TruckLib.Sii so this code
/// remains useful when SCS adds new sign-template unit classes. Known information
/// (area_name, linked_board and any explicit position/offset attributes) is lifted
/// into a typed view; every raw unit/attribute is also written to the schema report.
/// </summary>
internal sealed class SignTemplateResolver
{
    private readonly IFileSystem fs;
    private readonly IReadOnlyList<ParsedSign> signs;
    private readonly Dictionary<string, string> templatePaths = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, SignTemplateInfo> infos = new(StringComparer.OrdinalIgnoreCase);
    private readonly List<SchemaRow> schemaRows = [];
    private bool loaded;
    private readonly bool lazy;

    private sealed record SchemaRow(
        string TemplateName,
        string TemplatePath,
        string Status,
        string UnitClass,
        string UnitName,
        string Attribute,
        string ValueType,
        string Value);

    public SignTemplateResolver(IFileSystem fs, IReadOnlyList<ParsedSign> signs,bool lazy=false)
    {
        this.fs = fs;
        this.signs = signs;
        this.lazy=lazy;
    }

    public int TemplateDefinitionCount { get { EnsureLoaded(); return templatePaths.Count; } }
    public int UsedTemplateCount { get { EnsureLoaded(); return infos.Count; } }
    public int ResolvedUsedTemplateCount { get { EnsureLoaded(); return infos.Values.Count(x => x.Status == "OK"); } }
    public int UnsupportedUsedTemplateCount { get { EnsureLoaded(); return infos.Values.Count(x => x.Status != "OK"); } }

    public bool TryGet(string templateName, out SignTemplateInfo? info)
    {
        EnsureLoaded();
        if(lazy&&!string.IsNullOrWhiteSpace(templateName)&&templateName!="0"&&!infos.ContainsKey(templateName)&&templatePaths.ContainsKey(templateName))
            infos[templateName]=LoadTemplate(templateName);
        return infos.TryGetValue(templateName ?? "", out info);
    }

    public SignTemplateAreaBinding? FindAreaBinding(string templateName, string areaName)
    {
        if (!TryGet(templateName, out var info) || info is null) return null;
        return info.Areas.FirstOrDefault(a => string.Equals(a.AreaName, areaName, StringComparison.OrdinalIgnoreCase));
    }

    public void WriteSchema(string path)
    {
        EnsureLoaded();
        using var writer = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        writer.WriteLine("template,template_path,status,unit_class,unit_name,attribute,value_type,value");
        foreach (var r in schemaRows.OrderBy(r => r.TemplateName, StringComparer.OrdinalIgnoreCase).ThenBy(r => r.UnitName, StringComparer.OrdinalIgnoreCase).ThenBy(r => r.Attribute, StringComparer.OrdinalIgnoreCase))
        {
            writer.WriteLine(string.Join(',', Csv(r.TemplateName), Csv(r.TemplatePath), Csv(r.Status), Csv(r.UnitClass), Csv(r.UnitName), Csv(r.Attribute), Csv(r.ValueType), Csv(r.Value)));
        }
    }

    public void WriteAreaBindings(string path)
    {
        EnsureLoaded();
        using var writer = new StreamWriter(path, false, new System.Text.UTF8Encoding(false));
        writer.WriteLine("template,template_path,status,area_name,board_token,local_x,local_y,position_source,unit_class,unit_name");
        foreach (var info in infos.Values.OrderBy(i => i.TemplateName, StringComparer.OrdinalIgnoreCase))
        {
            if (info.Areas.Count == 0)
            {
                writer.WriteLine(string.Join(',', Csv(info.TemplateName), Csv(info.TemplatePath), Csv(info.Status), "", "", "", "", "", "", ""));
                continue;
            }
            foreach (var a in info.Areas.OrderBy(a => a.AreaName, StringComparer.OrdinalIgnoreCase))
            {
                writer.WriteLine(string.Join(',', Csv(info.TemplateName), Csv(info.TemplatePath), Csv(info.Status), Csv(a.AreaName), Csv(a.BoardToken),
                    a.LocalX.HasValue ? F(a.LocalX.Value) : "", a.LocalY.HasValue ? F(a.LocalY.Value) : "", Csv(a.PositionSource), Csv(a.UnitClass), Csv(a.UnitName)));
            }
        }
    }

    private void EnsureLoaded()
    {
        if (loaded) return;
        loaded = true;
        var paths=AssetIndexCache.GetShared(fs,"template-paths",()=>{LoadTemplateDefinitions();return templatePaths;});
        foreach(var pair in paths)templatePaths[pair.Key]=pair.Value;
        if(lazy)return;

        var used = signs.Select(s => s.Template).Where(t => !string.IsNullOrWhiteSpace(t) && t != "0").Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(t => t, StringComparer.OrdinalIgnoreCase).ToList();
        foreach (var template in used)
            infos[template] = LoadTemplate(template);
    }

    private void LoadTemplateDefinitions()
    {
        if (!fs.DirectoryExists("/def/sign/")) return;
        var files = fs.GetFiles("/def/sign/")
            .Where(f => f.EndsWith(".sii", StringComparison.OrdinalIgnoreCase))
            .Where(f => Path.GetFileName(f).StartsWith("templates", StringComparison.OrdinalIgnoreCase))
            .OrderBy(f => f, StringComparer.OrdinalIgnoreCase)
            .ToList();

        foreach (var file in files)
        {
            SiiFile sii;
            try { sii = SiiDocumentCache.Open(file, fs); }
            catch { continue; }
            foreach (var unit in sii.Units)
            {
                if (!unit.Name.StartsWith("sign_templ.", StringComparison.OrdinalIgnoreCase)) continue;
                if (!unit.Attributes.TryGetValue("path", out var rawPath)) continue;
                object? rawPathObject = rawPath;
                string path = NormalizePath(CleanToken(rawPathObject));
                if (path.Length == 0) continue;
                // AssetLoader already resolves mod priority. The last assignment here is
                // only within the already-resolved virtual file contents.
                templatePaths[unit.Name] = path;
            }
        }
    }

    private SignTemplateInfo LoadTemplate(string templateName)
    {
        var key=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(templateName)));
        var cached=AssetIndexCache.GetShared(fs,"template-info-"+key,()=>{
            int start=schemaRows.Count;var info=LoadTemplateFresh(templateName);var rows=schemaRows.Skip(start).ToArray();schemaRows.RemoveRange(start,rows.Length);
            return new CachedTemplate(info,rows);
        });
        schemaRows.AddRange(cached.Rows);return cached.Info;
    }
    private sealed record CachedTemplate(SignTemplateInfo Info,SchemaRow[] Rows);
    private SignTemplateInfo LoadTemplateFresh(string templateName)
    {
        if (!templatePaths.TryGetValue(templateName, out var path))
        {
            schemaRows.Add(new SchemaRow(templateName, "", "NO_TEMPLATE_DEF", "", "", "", "", ""));
            return new SignTemplateInfo(templateName, "", "NO_TEMPLATE_DEF", [], 0);
        }
        if (!fs.FileExists(path))
        {
            schemaRows.Add(new SchemaRow(templateName, path, "FILE_MISSING", "", "", "", "", ""));
            return new SignTemplateInfo(templateName, path, "FILE_MISSING", [], 0);
        }

        SiiFile sii;
        try { sii = SiiDocumentCache.Open(path, fs); }
        catch (Exception ex)
        {
            schemaRows.Add(new SchemaRow(templateName, path, "PARSE_ERROR", "", "", "", ex.GetType().Name, ex.Message));
            return new SignTemplateInfo(templateName, path, "PARSE_ERROR", [], 0);
        }

        var byArea = new Dictionary<string, SignTemplateAreaBinding>(StringComparer.OrdinalIgnoreCase);
        foreach (var unit in sii.Units)
        {
            if (unit.Attributes.Count == 0)
                schemaRows.Add(new SchemaRow(templateName, path, "OK", unit.Class, unit.Name, "", "", ""));

            foreach (var kv in unit.Attributes)
            {
                object? value = kv.Value;
                schemaRows.Add(new SchemaRow(templateName, path, "OK", unit.Class, unit.Name, kv.Key, ValueType(value), ValueToString(value)));
            }

            if (!unit.Attributes.TryGetValue("area_name", out var rawArea)) continue;
            object? rawAreaObject = rawArea;
            string areaName = CleanToken(rawAreaObject);
            if (areaName.Length == 0) continue;

            string board = "";
            if (unit.Attributes.TryGetValue("linked_board", out var rawBoard))
            {
                object? rawBoardObject = rawBoard;
                board = CleanToken(rawBoardObject);
            }
            var (x, y, source) = FindLocalPosition(unit.Attributes);

            if (byArea.TryGetValue(areaName, out var existing) && existing is not null)
            {
                byArea[areaName] = existing with
                {
                    BoardToken = board.Length > 0 ? board : existing.BoardToken,
                    LocalX = x ?? existing.LocalX,
                    LocalY = y ?? existing.LocalY,
                    PositionSource = source.Length > 0 ? source : existing.PositionSource,
                    UnitClass = existing.UnitClass.Length > 0 ? existing.UnitClass : unit.Class,
                    UnitName = existing.UnitName.Length > 0 ? existing.UnitName : unit.Name
                };
            }
            else
            {
                byArea[areaName] = new SignTemplateAreaBinding(areaName, board, x, y, source, unit.Class, unit.Name);
            }
        }

        return new SignTemplateInfo(templateName, path, "OK", byArea.Values.OrderBy(a => a.AreaName, StringComparer.OrdinalIgnoreCase).ToList(), sii.Units.Count)
            {Includes=(sii.Includes??[]).ToArray()};
    }

    private static (double? X, double? Y, string Source) FindLocalPosition(IDictionary<string, dynamic> attributes)
    {
        // SCS template schemas have changed over time. Prefer explicit position-like
        // attributes when they exist, but never infer from unrelated numeric fields.
        foreach (var key in new[] { "position", "pos", "offset", "item_pos", "local_pos", "coords" })
        {
            if (!attributes.TryGetValue(key, out var rawValue)) continue;
            object? value = rawValue;
            if (TryVector2(value, out double x, out double y)) return (x, y, key);
        }
        return (null, null, "");
    }

    private static bool TryVector2(object? value, out double x, out double y)
    {
        x = y = 0;
        if (value is null) return false;
        switch (value)
        {
            case Vector2 v2: x = v2.X; y = v2.Y; return true;
            case Vector3 v3: x = v3.X; y = v3.Y; return true;
            case Vector4 v4: x = v4.X; y = v4.Y; return true;
        }

        var t = value.GetType();
        var px = t.GetProperty("X");
        var py = t.GetProperty("Y");
        if (px is null || py is null) return false;
        try
        {
            x = Convert.ToDouble(px.GetValue(value), CultureInfo.InvariantCulture);
            y = Convert.ToDouble(py.GetValue(value), CultureInfo.InvariantCulture);
            return true;
        }
        catch { return false; }
    }

    private static string ValueType(object? value) => value?.GetType().FullName ?? "null";

    private static string ValueToString(object? value)
    {
        if (value is null) return "";
        // TruckLib.Token also exposes a public Value property, but that Value is the
        // packed numeric representation. Preserve the semantic token string before
        // applying the generic LinkPointer-style unwrapping below.
        if (value is Token token) return token.ToString();
        if (value is string s) return s;
        if (TryUnwrapValue(value, out var unwrapped) && !ReferenceEquals(unwrapped, value))
            return ValueToString(unwrapped);
        if (value is IEnumerable sequence)
        {
            var values = new List<string>();
            foreach (var item in sequence) values.Add(ValueToString(item));
            return string.Join('|', values);
        }
        return Convert.ToString(value, CultureInfo.InvariantCulture) ?? value.ToString() ?? "";
    }

    // TruckLib.Sii represents unit links as LinkPointer objects. Their ToString() is
    // diagnostic ("LinkPointer { Value = sign_board.123 }") rather than the token
    // itself, so unwrap a public Value property/field before normalizing. Reflection
    // keeps this compatible with TruckLib minor schema changes without taking a hard
    // dependency on the concrete LinkPointer type.
    internal static string CleanToken(object? value)
    {
        if (value is null) return "";
        // Do not unwrap TruckLib.Token.Value: it is the packed ulong, not the token
        // text. LinkPointer and similar SII wrapper objects are handled afterwards.
        if (value is Token token) return token.ToString().Trim().Trim('"');
        if (value is string s) return s.Trim().Trim('"');
        if (TryUnwrapValue(value, out var unwrapped) && !ReferenceEquals(unwrapped, value))
            return CleanToken(unwrapped);
        return (Convert.ToString(value, CultureInfo.InvariantCulture) ?? "").Trim().Trim('"');
    }

    private static bool TryUnwrapValue(object value, out object? unwrapped)
    {
        unwrapped = value;
        var type = value.GetType();
        var prop = type.GetProperty("Value");
        if (prop is not null && prop.GetIndexParameters().Length == 0)
        {
            try { unwrapped = prop.GetValue(value); return true; } catch { }
        }
        var field = type.GetField("Value");
        if (field is not null)
        {
            try { unwrapped = field.GetValue(value); return true; } catch { }
        }
        return false;
    }

    private static string NormalizePath(string path)
    {
        var p = path.Replace('\\', '/');
        if (p.Length == 0) return "";
        return p.StartsWith('/') ? p : "/" + p;
    }

    private static string F(double value) => value.ToString("0.######", CultureInfo.InvariantCulture);
    private static string Csv(string? value)
    {
        value ??= "";
        if (!value.Contains(',') && !value.Contains('"') && !value.Contains('\n') && !value.Contains('\r')) return value;
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}
