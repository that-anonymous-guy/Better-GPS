using System.Text.Json.Serialization;

namespace BetterGPSNavigation;

public sealed record RoadOwnershipCandidate(string ItemUid,double DistanceM,double HeightM,string Reason);
public sealed record RoadIdentityAnchor(string Id,string Source,string SourceId,string Raw,string Identity,
    string Kind,string Cardinal,string ReadingConfidence,string AssociationStatus,string AssociationConfidence,
    string OwnerUid,int SegmentIndex,double WorldStationM,double NavStationM,RoadOwnershipCandidate[] Alternatives);
public sealed record SegmentRoadIdentity(string Identity,string Kind,string Cardinal,string IdentityConfidence,
    string CardinalConfidence,string[] ProofIds,string Rule);
public sealed record RoadIdentitySpan(string Id,int SegmentIndex,string ItemUid,string EntryNodeUid,string ExitNodeUid,
    double WorldStartM,double WorldEndM,double NavStartM,double NavEndM,string Status,
    SegmentRoadIdentity[] Identities,SegmentRoadIdentity[] Candidates,string Reason);
public sealed record ActionRoadContext(string ActionId,string BranchId,string[] ApproachSpanIds,string[] TargetSpanIds,
    string[] FutureSpanIds,string? PreferredIdentity,string Reason)
{
    public string[] JoinedSpanIds {get;init;}=[];
    public string[] ConnectorSpanIds {get;init;}=[];
}
public sealed record RoadIdentitySnapshot(int Version,string MapFingerprint,RoadIdentityAnchor[] Anchors,
    RoadIdentitySpan[] Spans,ActionRoadContext[] Contexts);

public sealed record RoadLabel(string Identity,string Kind,string Cardinal,string Confidence,string CardinalConfidence,string[] ProofIds);
public sealed record LiveRoadContext(string Status,RoadLabel[] Labels,string[] SpanIds,string Reason);

internal static class RoadIdentityRules
{
    public static bool Reference(string s)=>s is {Length:>0}&&SignTextClassifier.IsRoadIdentity(s);
    public static bool Street(string s)=>s is {Length:>0 and <=160} && !SignTextClassifier.IsInstruction(s) &&
        !SignTextClassifier.IsRouteText(s) && s.Any(char.IsLetter) &&
        s.All(c=>char.IsLetterOrDigit(c)||char.IsWhiteSpace(c)||c is '-' or '\'' or '’' or '.' or '&');
    public static bool Valid(string kind,string s)=>kind=="RouteReference"?Reference(s):kind=="StreetName"&&Street(s);
    public static bool Confidence(string s)=>s is "HIGH" or "MEDIUM" or "LOW" or "NONE";
    public static string Display(RoadLabel l)=>string.Join(" ",new[]{l.Identity,l.CardinalConfidence=="HIGH"?l.Cardinal:""}.Where(x=>x.Length>0));
    public static bool ReferenceOnly(string raw)=>!System.Text.RegularExpressions.Regex.IsMatch(raw,
        @"\b(?:TO|JCT|JUNCTION|VIA|EXIT)\b",System.Text.RegularExpressions.RegexOptions.IgnoreCase);
}
