namespace BetterGPSNavigation;

internal sealed record SignFieldValidation(
    string IdentityConfidence,
    string CardinalConfidence,
    string DestinationConfidence,
    string ExitConfidence,
    string ValidatedToward,
    string Notes);

internal static class SignSemanticValidator
{
    public static SignFieldValidation Validate(ParsedSignArea area, SignBoardInfo? board,
        string identity, string cardinal, Func<string, bool> isKnownDestination)
    {
        var strings = area.RawStrings.Select(SignTextClassifier.Normalize).ToList();
        var textAttributes = area.Attributes.Where(a => a.ValueType == "System.String")
            .Select(a => SignTextClassifier.Normalize(a.Value)).ToHashSet(StringComparer.OrdinalIgnoreCase);
        // RawStrings are derived from these typed overrides; retain the raw normalized
        // spelling as well (e.g. HTML line breaks) without treating numeric attributes as text.
        bool HasTextEvidence(string value) => textAttributes.Contains(value) ||
            area.Attributes.Any(a => a.ValueType == "System.String" &&
                SignRoadIdentityResolver.NormalizeText(a.Value).Equals(value, StringComparison.OrdinalIgnoreCase));

        var identityConfidence = string.IsNullOrWhiteSpace(identity) ? "NONE" : "MEDIUM";
        if (SignTextClassifier.IsRoadIdentity(identity) && area.RoadShields.Count <= 1 &&
            (area.RoadShields.Count == 1 && area.RoadShields[0].Equals(identity, StringComparison.OrdinalIgnoreCase) &&
                strings.Any(s => HasTextEvidence(s) && SignTextClassifier.SupportsIdentity(s, identity))
             || area.RoadShields.Count == 0 && board is { Status: "OK" } && board.SemanticKind.Contains("SHIELD", StringComparison.Ordinal) &&
                !string.IsNullOrWhiteSpace(board.ShieldPrefixHint) && identity.StartsWith(board.ShieldPrefixHint + "-", StringComparison.OrdinalIgnoreCase) &&
                strings.Count(s => s.Equals(identity[(identity.IndexOf('-') + 1)..], StringComparison.OrdinalIgnoreCase) && HasTextEvidence(s)) == 1))
            identityConfidence = "HIGH";
        if (area.RoadShields.Count > 1) identityConfidence = "AMBIGUOUS";
        if (area.Extraction is { Reliable: true } extracted && area.RoadShields.Count == 1 &&
            extracted.Objects.Any(o => o.Visible && o.Reliable && o.Road == identity))
            identityConfidence = "HIGH";

        var cardinalConfidence = string.IsNullOrWhiteSpace(cardinal) ? "NONE" : "MEDIUM";
        if (area.Cardinals.Count > 1) cardinalConfidence = "CONFLICT";
        else if (identityConfidence == "HIGH" && area.Cardinals.Count == 1 && strings.Any(s => HasTextEvidence(s) &&
                     SignTextClassifier.ExplicitCardinals(s).Contains(cardinal, StringComparer.OrdinalIgnoreCase)))
            cardinalConfidence = "HIGH";

        var destinations = area.Destinations.Where(SignTextClassifier.IsDestinationCandidate).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        var validated = destinations.Where(d => HasTextEvidence(d) && isKnownDestination(d)).ToList();
        var knownPlaces=destinations.Where(isKnownDestination).ToHashSet(StringComparer.OrdinalIgnoreCase);
        var destinationConfidence = destinations.Count == 0 ? "NONE" : validated.Count == destinations.Count ? "HIGH" : "MEDIUM";

        var exitConfidence = area.ExitNumbers.Count == 0 ? "NONE" : area.ExitNumbers.Count > 1 ? "AMBIGUOUS" : "MEDIUM";
        if (area.ExitNumbers.Count == 1 && SignTextClassifier.IsExitNumber(area.ExitNumbers[0]))
        {
            var exit = area.ExitNumbers[0];
            if (strings.Any(s => HasTextEvidence(s) && SignTextClassifier.IsExplicitExit(s, exit)) ||
                board is { Status: "OK" } && board.SemanticKind.Contains("EXIT", StringComparison.Ordinal) &&
                !board.SemanticKind.Contains("SHIELD", StringComparison.Ordinal) &&
                strings.Any(s => HasTextEvidence(s) && s.Replace(" ", "").Equals(exit, StringComparison.OrdinalIgnoreCase)))
                exitConfidence = "HIGH";
        }
        if (area.Extraction is { } panel && (!panel.Reliable || !panel.SchemaValidated || panel.GroupStatus == "MULTI_GROUP_UNRESOLVED"))
        {
            if(identityConfidence=="HIGH") identityConfidence="MEDIUM";
            if(cardinalConfidence=="HIGH") cardinalConfidence="MEDIUM";
            if(destinationConfidence=="HIGH") destinationConfidence="MEDIUM";
            if(exitConfidence=="HIGH") exitConfidence="MEDIUM";
            validated=[];
        }
        var notes = $"identity={identityConfidence};cardinal={cardinalConfidence};exit={exitConfidence};" +
            (area.Extraction is {} extraction ? $"extraction={extraction.Status};group={extraction.GroupStatus};schema={(extraction.SchemaValidated?"SUPPORTED":"INFERRED_ATS161")};" : "") +
            string.Join(';', destinations.Select(d => $"destination:{d}={(knownPlaces.Contains(d) ? "KNOWN_CITY" : "UNVERIFIED_PLACE")}"));
        return new(identityConfidence, cardinalConfidence, destinationConfidence, exitConfidence, string.Join('|', validated), notes);
    }

    public static string Combine(string association, string field) => field is "NONE" or "CONFLICT" or "AMBIGUOUS" ? field :
        association is "CONFLICT" or "AMBIGUOUS" ? association :
        association == "HIGH" && field == "HIGH" ? "HIGH" :
        association is "HIGH" or "MEDIUM" && field is "HIGH" or "MEDIUM" ? "MEDIUM" : "LOW";
}
