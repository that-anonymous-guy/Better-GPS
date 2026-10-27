using System.Text.RegularExpressions;

namespace BetterGPSNavigation;

/// <summary>Lexical roles, independent of which route branch a sign belongs to.</summary>
internal static class SignTextClassifier
{
    private const RegexOptions Flags = RegexOptions.IgnoreCase | RegexOptions.CultureInvariant;
    internal const string Prefix = "I|US|SR|CA|NV|AZ|OR|WA|UT|ID|MT|WY|CO|NM|TX|OK|KS|NE|SD|ND|MN|IA|MO|AR|LA|WI|IL|MI|IN|OH|KY|TN|MS|AL|GA|FL|SC|NC|VA|WV|PA|NY|VT|NH|ME|MA|RI|CT|NJ|DE|MD|AK|HI|DC";
    private const string Cardinal = "NORTH|SOUTH|EAST|WEST";
    private const string CardinalToken = Cardinal + "|N|S|E|W";
    private const string Road = "(?:" + Prefix + @")[-\s]?\d{1,4}[A-Z]?";

    public static string Normalize(string text) => Regex.Replace(text.Trim(), @"\s+", " ");

    public static bool IsInstruction(string text)
    {
        text = Normalize(text);
        // Match phrases, not direction words inside place names (North Las Vegas,
        // Turners Falls, Right Hand Canyon, etc.).
        return text.Equals("EXIT",StringComparison.OrdinalIgnoreCase) || Regex.IsMatch(text, @"^(?:(?:USE|KEEP|STAY|TAKE|TURN|MERGE|BEAR)\s+)?(?:LEFT|RIGHT|CENTER|MIDDLE)(?:\s+\d+)?(?:\s+(?:LANES?|TURN|ONLY))?$", Flags)
            || Regex.IsMatch(text, @"^(?:IN|USE|TAKE)\s+(?:\d+\s+)?(?:(?:LEFT|RIGHT|CENTER|MIDDLE)\s+)?LANES?$", Flags)
            || Regex.IsMatch(text, @"^(?:\d+\s+)?(?:LEFT|RIGHT|CENTER|MIDDLE)?\s*LANES?(?:\s+(?:ONLY|ENDS?|CLOSED|MERGE(?:S)?))?$", Flags)
            || Regex.IsMatch(text, @"^(?:FIRST|SECOND|THIRD|FOURTH|1ST|2ND|3RD|4TH|NEXT|LAST)\s+(?:LEFT|RIGHT|EXIT|TURN)(?:\s+TURN)?$", Flags)
            || Regex.IsMatch(text, @"^(?:EXIT ONLY|NEXT EXIT|NO (?:LEFT|RIGHT|U)[ -]?TURN|DO NOT ENTER|WRONG WAY|KEEP CLEAR|THRU TRAFFIC|THROUGH TRAFFIC|ALL TRUCKS|TRUCKS ONLY|PREPASS|WEIGH STATION|TRUCK SCALE|STOP|YIELD|ONLY|RAMP|FREEWAY|TO|VIA|JCT|JUNCTION|INTERCHANGE|DETOUR|CLOSED|ROAD CLOSED)$", Flags)
            || Regex.IsMatch(text, @"^(?:LANE|ROAD) (?:ENDS?|CLOSED)|^(?:LEFT|RIGHT) LANE (?:ENDS?|CLOSED|MUST TURN)", Flags);
    }

    public static bool IsRouteText(string text) => Regex.IsMatch(Normalize(text),
        @"^(?:TO\s+)?(?:(?:" + CardinalToken + @")\s+)?(?:" + Road + @"|STATE\s+(?:ROUTE|HWY|HIGHWAY)\s+\d{1,4}[A-Z]?)(?:\s+(?:" + CardinalToken + @"))?$", Flags);

    public static bool IsRoadIdentity(string identity) => Regex.IsMatch(identity, @"^(?:(?:" + Prefix + @")-\d{1,4}[A-Z]?(?:-BUS)?|US-\d{1,4}[A-Z]?-ALT|MO-[A-Z]{1,2}|(?:"+Prefix+@")-CR-[A-Z]?\d{1,4}|TX-(?:FM|RM|PR|LOOP)-\d{1,4}|KS-TURNPIKE)$", Flags);

    internal static string[] RoadReferences(string text)=>IsInstruction(text)||!IsRouteText(text)&&Regex.IsMatch(text,@"\b(?:MPH|MILES?|MI|FEET|FT|LANES?|KM)\b",Flags)?[]:
        Regex.Matches(Normalize(text),@"\b("+Prefix+@")[ -]?(\d{1,4}[A-Z]?)\b",Flags)
        .Select(m=>m.Groups[1].Value.ToUpperInvariant()+"-"+m.Groups[2].Value.ToUpperInvariant()).Distinct().ToArray();

    public static bool SupportsIdentity(string text, string identity)
    {
        if (IsRouteText(text))
        {
            var m = Regex.Match(text, @"\b(?<prefix>" + Prefix + @")[-\s]?(?<number>\d{1,4}[A-Z]?)\b", Flags);
            if (m.Success) return (m.Groups["prefix"].Value + "-" + m.Groups["number"].Value).Equals(identity, StringComparison.OrdinalIgnoreCase);
            var state = Regex.Match(text, @"STATE\s+(?:ROUTE|HWY|HIGHWAY)\s+(\d{1,4}[A-Z]?)", Flags);
            return state.Success && ("SR-" + state.Groups[1].Value).Equals(identity, StringComparison.OrdinalIgnoreCase);
        }
        var atlas = Regex.Match(text, @"^/def/sign/atlas/(?<prefix>is|" + Prefix + @")[_-]?(?<number>\d{1,4}[A-Z]?)(?:[_-][^/]*)?\.(?:mat|sii)$", Flags);
        var prefix = atlas.Groups["prefix"].Value.Equals("is", StringComparison.OrdinalIgnoreCase) ? "I" : atlas.Groups["prefix"].Value;
        return atlas.Success && (prefix + "-" + atlas.Groups["number"].Value).Equals(identity, StringComparison.OrdinalIgnoreCase);
    }

    public static IReadOnlyList<string> ExplicitCardinals(string text)
    {
        text = Normalize(text);
        if (!Regex.IsMatch(text, @"^(?:" + CardinalToken + @")$", Flags) && !IsRouteText(text)) return [];
        return Regex.Matches(text, @"\b(?:" + CardinalToken + @")\b", Flags)
            .Select(m => m.Value.ToUpperInvariant() switch {"N"=>"NORTH","S"=>"SOUTH","E"=>"EAST","W"=>"WEST",var full=>full}).Distinct().ToList();
    }

    public static bool IsExplicitExit(string text, string number) => Regex.IsMatch(Normalize(text),
        @"^EXITS?\s+" + Regex.Escape(number).Replace(@"\ ", @"\s*") + "$", Flags)
        || (Regex.Match(Normalize(text), @"^EXITS?\s+(\d{1,4})\s*([A-Z]?)$", Flags) is { Success: true } m
            && (m.Groups[1].Value + m.Groups[2].Value).Equals(number, StringComparison.OrdinalIgnoreCase));

    public static bool IsExitNumber(string value) => Regex.IsMatch(value, @"^\d{1,4}[A-Z]?$", Flags);

    public static bool IsDestinationCandidate(string text)
    {
        text = Normalize(text);
        if (text.Length == 0 || IsInstruction(text) || IsRouteText(text)) return false;
        if (ExplicitCardinals(text).Count > 0 || Regex.IsMatch(text, @"^EXITS?\s+\d", Flags)) return false;
        if (text.Contains('/') || text.Contains('\\') || text.Contains('@') || text.Contains('|') || text.Contains('_')) return false;
        if (Regex.IsMatch(text, @"\b(?:MPH|MILES?|FEET|FT|KM|METERS?|TONS?|LBS|TRAFFIC_RULE|ARROW)\b", Flags)) return false;
        if (Regex.IsMatch(text, @"^\d+(?:\.\d+)?(?:\s*[A-Z])?$", Flags)) return false;
        // A candidate is not a proven destination. Names absent from the game's
        // city definitions are capped at MEDIUM by SignSemanticValidator.
        return text.Any(char.IsLetter) && text.All(c => char.IsLetterOrDigit(c) || char.IsWhiteSpace(c) || c is '-' or '\'' or '’' or '.' or '&' or '(' or ')');
    }

    public static string DirectionHint(IEnumerable<string> source)
    {
        var directions = new HashSet<string>();
        foreach (var value in source)
        {
            if (!IsInstruction(value) && !value.Contains("arrow", StringComparison.OrdinalIgnoreCase)) continue;
            if (Regex.IsMatch(value, @"\bLEFT\b|ARROW[^| ]*LEFT|LEFT[^| ]*ARROW", Flags)) directions.Add("LEFT");
            if (Regex.IsMatch(value, @"\bRIGHT\b|ARROW[^| ]*RIGHT|RIGHT[^| ]*ARROW", Flags)) directions.Add("RIGHT");
        }
        return directions.Count == 1 ? directions.Single() : "";
    }
}
