namespace BetterGPSNavigation;

public sealed record LiveGuidance(long TimestampMs, string Status, string? RouteId, long Revision,
    string? ActionId, string? Instruction, double? DistanceM, double? DistanceMi,
    string? Road, string? Cardinal, string? Destination, string? Exit,
    string? ThenInstruction, double? ThenDistanceM, LiveProof[] Proofs,
    double? NavStationM, double? WorldStationM, string Detail)
{
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public string? PreparationStatus {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public string? PreparationDetail {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public LiveRoadContext? CurrentRoad {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public LiveRoadContext? TargetRoad {get;init;}
    public LiveRoadContext? JoinedRoad {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public string? DestinationAssociation {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public string[]? DestinationCandidates {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public double? ContinueDistanceM {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public LiveSignTarget[]? SignTargets {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public string? ProofReference {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public LiveRoadContext? ContinueToward {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public int? RoundaboutExitNumber {get;init;}
    [System.Text.Json.Serialization.JsonIgnore(Condition=System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)]
    public string? ServiceAreaKind {get;init;}
}

public sealed class LiveGuidanceEngine
{
    public LiveGuidance Suspend(long nowMs,string status,string detail)
    {
        if(nowMs<messageTime) throw new InvalidDataException("Suspension clock moved backwards.");
        timestamp=nowMs;
        return Empty(status,detail);
    }
    public const long StaleAfterMs = 2000;
    private string? session;
    private long sequence = -1, timestamp = -1, messageTime = -1, revision = -1, lastVehicle = -1;
    private LiveRoute? route;
    private Projection? previous;
    private readonly HashSet<string> completed = new(StringComparer.Ordinal);
    private bool paused;
    private bool sourceSuspended;
    private string? phaseAction;
    private int phaseRank;
    private LiveGuidance? last;
    private readonly Dictionary<string,VehicleLaneGeometry[]> laneCache=new(StringComparer.Ordinal);
    private readonly Dictionary<string,SignGroupProof[]> groupCache=new(StringComparer.Ordinal);
    // These facts depend only on the installed, validated route. Keep them until
    // Reset, including null/conflicting fields; never cache position or distances.
    private readonly Dictionary<(string Action,string Field),string?> fieldCache=new();
    private sealed record ActionFacts(bool Proven,LiveProof[] DisplayProofs);
    private readonly Dictionary<string,ActionFacts> actionFacts=new(StringComparer.Ordinal);
    private SignGroupProof[] Groups(LiveAction action)=>groupCache.TryGetValue(action.Id,out var groups)?groups:
        groupCache[action.Id]=action.Proofs.Where(p=>p.Source=="SIGN_GROUP").Select(SignGroupProof.Read).ToArray();
    private ActionFacts Facts(LiveAction action)=>actionFacts.TryGetValue(action.Id,out var facts)?facts:
        actionFacts[action.Id]=new(ProofSupportsAction(action),action.Proofs.Where(p=>p.Source!="LANE_NETWORK").ToArray());
    private sealed record Projection(int EdgeIndex, int SegmentIndex, double NavM, double WorldM, double Score);

    public LiveGuidance Accept(LiveMessage message)
    {
        LiveProtocol.Validate(message);
        if (message.Type == "Hello")
        {
            if (message.SessionId == session && sequence >= 0)
                throw new InvalidDataException("Hello must start a new session.");
            session = message.SessionId; sequence = -1; timestamp = -1; messageTime = -1; revision = -1;
            Reset();
        }
        if (session is null || message.SessionId != session)
            throw new InvalidDataException("Hello required for this session.");
        if (message.Sequence <= sequence || message.TimestampMs < messageTime)
            throw new InvalidDataException("Non-monotonic sequence or timestamp.");
        if (message.Type is "RouteSnapshot" or "RouteCleared" && message.Revision <= revision)
            throw new InvalidDataException("Route revision must increase.");
        sequence = message.Sequence; timestamp = message.TimestampMs; messageTime = message.TimestampMs;
        if (message.Type == "Hello") return Empty("WAITING_ROUTE", "Session started.");
        if (message.Type == "SourceState")
        { sourceSuspended=true; return Empty(message.Status!,message.Detail!); }
        if (message.Type == "RouteSnapshot")
        {
            if (message.Revision <= revision) throw new InvalidDataException("Route revision must increase.");
            Reset(); revision = message.Revision; route = RouteEvidence.Unpack(message.Route!);
            return Empty("WAITING_POSITION", "Compiled route installed; awaiting a matching vehicle frame.");
        }
        if (message.Type == "RouteCleared")
        {
            if (message.Revision <= revision) throw new InvalidDataException("Clear revision must increase.");
            Reset(); revision = message.Revision;
            return Empty("NO_ROUTE", "Route explicitly cleared.");
        }
        if (message.Type == "Heartbeat") return Tick(message.TimestampMs);
        if (route is null) return Empty("NO_ROUTE", "No compiled route.");
        if (message.Revision != revision) return Empty("WAITING_ROUTE", "Vehicle references a different route revision.");
        sourceSuspended=false;
        var v = message.Vehicle!;
        lastVehicle = timestamp;
        paused = v.Paused;
        if (paused) return Empty("PAUSED", "Progress frozen; no active instruction.");
        var projection = Project(v);
        if (projection is null)
        {
            var endpoint=EndpointGuidance(v);
            previous = null;
            if(endpoint is not null)return last=endpoint;
            if(ServiceAreaGuidance.Locate(route,v) is {} area)
                return last=new(timestamp,"SERVICE_AREA",route.RouteId,revision,null,"CONTINUE",null,null,null,null,null,null,null,null,[],null,null,
                    ServiceAreaGuidance.Instruction(area.Kind)){ServiceAreaKind=area.Kind};
            return Empty("OFF_ROUTE", "No compatible corridor edge; old guidance suppressed. Reacquire on next frame.");
        }
        if (previous is { } p && (projection.WorldM < p.WorldM - 15 ||
            projection.WorldM > p.WorldM + 200))
        {
            previous = null;
            return Empty("RELOCALIZING", "Discontinuous progress; new matching frame required.");
        }
        bool rebase=previous is null||projection.WorldM<previous.WorldM-2||v.Reversing;
        if(rebase){completed.Clear();phaseAction=null;phaseRank=0;}
        previous = projection;
        var geometricNav=projection.NavM;
        var distanceSource="CORRIDOR_NAVIGATION_STATION";
        // The snapshot and telemetry share ATS navigation metres, including the final
        // approach beyond the last map node. Use that origin only when the measured
        // station agrees with the independently located corridor edge.
        if(route.NavigationRemainingAtOriginM is double origin&&v.NavigationRemainingM is double remaining)
        {
            var measured=origin-remaining;var edge=route.Edges[projection.EdgeIndex];
            var tolerance=Math.Max(5,(edge.NavEndM-edge.NavStartM)/Math.Max(1,edge.WorldEndM-edge.WorldStartM)*3);
            if(measured>=edge.NavStartM-tolerance&&measured<=edge.NavEndM+tolerance&&
                Math.Abs(measured-projection.NavM)<=tolerance)
            {
                projection=projection with{NavM=Math.Clamp(measured,edge.NavStartM,edge.NavEndM)};
                previous=projection;distanceSource="ATS_REMAINING_CALIBRATED_TO_CORRIDOR";
            }
        }
        foreach (var a in route.Actions)
            if (projection.SegmentIndex > a.SegmentIndex && geometricNav >= a.NavStationM)
                completed.Add(a.Id);
        var active = route.Actions.FirstOrDefault(a => !completed.Contains(a.Id));
        bool arrived=!route.IsPartial&&route.Edges[^1].WorldEndM-projection.WorldM<=5&&
            Math.Sqrt(Math.Pow(v.Position.X-route.Edges[^1].End.X,2)+Math.Pow(v.Position.Z-route.Edges[^1].End.Z,2))<=10&&
            (v.NavigationRemainingM is double remainingAtEnd?remainingAtEnd<=10:
                route.Edges[^1].NavEndM-projection.NavM<=10);
        if(arrived) return Empty("COMPLETE", "Destination reached on the verified final corridor.");
        if(active is null&&!arrived&&(!route.IsPartial||projection.WorldM<route.Edges[^1].WorldEndM-20))
        {
            var span=route.RoadIdentities?.Spans.FirstOrDefault(s=>s.SegmentIndex==projection.SegmentIndex);
            return last=new(timestamp,"CRUISE",route.RouteId,revision,null,"CONTINUE",null,null,null,null,null,null,null,null,[],
                projection.NavM,projection.WorldM,route.IsPartial?
                    "Following the verified loaded corridor; subsequent manoeuvres are being prepared.":
                    "Continue along the final approach; destination has not been reached.")
                {CurrentRoad=RoadContext(span is null?[]:[span],"VEHICLE_SEGMENT"),ContinueToward=ConnectionDestination(span)};
        }
        if (active is null) return Empty(route.IsPartial?"PREPARING_ROUTE":"COMPLETE", route.IsPartial?
            "Preparing the next portion of the route; destination has not been reached.":"All decisions passed on their outgoing route segments.");
        var distance = Math.Max(0, active.NavStationM - projection.NavM);
        if (!ActionProven(active))
            return Empty("UNCERTAIN_ACTION", $"Action {active.Id} lacks exact PPD branch evidence.", active.Proofs);
        var actionWorld = WorldAt(active.NavStationM, active.SegmentIndex);
        var worldDistance = Math.Max(0, actionWorld - projection.WorldM);
        var phase = worldDistance <= Math.Max(12, v.SpeedMps * 2) ? "EXECUTE" :
            worldDistance <= Math.Max(80, v.SpeedMps * 12) ? "PREPARE" : "CRUISE";
        if (phaseAction != active.Id) { phaseAction = active.Id; phaseRank = 0; }
        // Urgency must follow actual distance after backing away or stopping.
        if(rebase||v.SpeedMps<0.5)phaseRank=0;
        phaseRank = Math.Max(phaseRank,phase == "EXECUTE" ? 2 : phase == "PREPARE" ? 1 : 0);
        phase = phaseRank == 2 ? "EXECUTE" : phaseRank == 1 ? "PREPARE" : "CRUISE";
        if(v.SpeedMps<0.5&&phase=="EXECUTE")phase="WAITING_AT_MANEUVER";
        var next = route.Actions.SkipWhile(a => a.Id != active.Id).Skip(1).FirstOrDefault(a => !completed.Contains(a.Id));
        var showThen = next is not null && ActionProven(next);
        var laneLocation=VehicleLaneLocator.Locate(v,projection.SegmentIndex,
            laneCache.TryGetValue(active.Id,out var cached)?cached:laneCache[active.Id]=VehicleLaneLocator.Read(active));
        last = new(timestamp, phase, route.RouteId, revision, active.Id, active.Instruction, distance,
            distance / 1609.344, Field(active, "road"), Field(active, "cardinal"), Field(active, "destination"),
            Field(active, "exit"), showThen ? next!.Instruction : null,
            showThen ? next!.NavStationM - active.NavStationM : null, Facts(active).DisplayProofs,
            projection.NavM, projection.WorldM,
            "Distance source="+distanceSource+"."+
            $" Lane={laneLocation.Status}:{laneLocation.LaneId??"unknown"}"){SignTargets=active.SignTargets,RoundaboutExitNumber=active.RoundaboutExitNumber};
        if(route.RoadIdentities is {} identities)
        {
            var current=identities.Spans.FirstOrDefault(s=>s.SegmentIndex==projection.SegmentIndex);
            var context=identities.Contexts.First(c=>c.ActionId==active.Id);
            var targets=identities.Spans.Where(s=>context.TargetSpanIds.Contains(s.Id)).ToArray();
            var target=RoadContext(targets,context.Reason,context.PreferredIdentity);
            var joined=RoadContext(identities.Spans.Where(s=>context.JoinedSpanIds.Contains(s.Id)).ToArray(),"SELECTED_NATIVE_CONNECTION_DESTINATION");
            var connection=current is {Status:"UNKNOWN"}?identities.Contexts.LastOrDefault(c=>c.ConnectorSpanIds.Contains(current.Id)):null;
            var continuing=connection is null?null:RoadContext(identities.Spans.Where(s=>connection.JoinedSpanIds.Contains(s.Id)).ToArray(),
                "CURRENT_CONNECTOR_DESTINATION_NOT_ITS_OWN_NAME");
            var chosen=target.Labels.Where(l=>context.PreferredIdentity is null||l.Identity==context.PreferredIdentity).ToArray();
            var groups=Groups(active);
            var matches=groups.Where(g=>g.Fields.IdentityConfidence=="HIGH"&&g.Fields.DestinationConfidence=="HIGH"&&
                g.Blockers.All(b=>b is "ROAD_SEGMENT_NOT_ASSOCIATED")&&chosen.Any(l=>l.Confidence=="HIGH"&&l.Identity==g.Road&&
                    (g.Cardinal.Length==0||l.CardinalConfidence=="HIGH"&&l.Cardinal==g.Cardinal))).ToArray();
            var dest=matches.Select(g=>g.Destination).Where(x=>x.Length>0).Distinct().ToArray();
            var roadName=chosen.Length>0?string.Join(" / ",chosen.Select(RoadIdentityRules.Display)):
                target.Status is "CONFLICT" or "AMBIGUOUS"?null:last.Road;
            last=last with{CurrentRoad=RoadContext(current is null?[]:[current],"VEHICLE_SEGMENT"),TargetRoad=target,JoinedRoad=joined,ContinueToward=continuing,
                ContinueDistanceM=ContinueDistance(identities.Spans,current,projection.NavM,active.NavStationM),
                Road=roadName,Cardinal=chosen.Length>0?null:last.Cardinal,Destination=last.Destination??(dest.Length==1?dest[0]:null),
                DestinationAssociation=last.Destination is not null?"CALIBRATED_SIGN_GROUP":dest.Length==1?"UNIQUE_GROUP_MATCHES_VERIFIED_TARGET":
                    groups.Length>0?"SIGN_GROUP_NOT_UNIQUELY_ASSOCIATED":"NO_DESTINATION_GROUP",
                DestinationCandidates=groups.Where(g=>g.Status!="OTHER_EXIT_CANDIDATE"&&g.Fields.DestinationConfidence=="HIGH"&&
                    chosen.Any(l=>l.Identity==g.Road&&(l.CardinalConfidence!="HIGH"||l.Cardinal==g.Cardinal)))
                    .Select(g=>g.Destination).Where(s=>s.Length>0).Distinct().Order(StringComparer.Ordinal).ToArray(),
                Proofs=last.Proofs.Concat(chosen.Select(l=>new LiveProof("targetRoad",l.Identity,l.Confidence,"ROAD_IDENTITY",string.Join(',',l.ProofIds),
                    active.BranchId,context.Reason))).ToArray()};
        }
        return last;
    }

    internal static double? ContinueDistance(RoadIdentitySpan[] spans,RoadIdentitySpan? current,double nav,double actionNav)
    {
        if(current is not {Status:"RESOLVED",Identities.Length:>0})return null;
        // A reference-only observation and a signed observation of the same road
        // are one identity, not a road change. Never bridge two explicit opposing cardinals.
        string Key(RoadIdentitySpan s)=>string.Join('|',s.Identities.Select(i=>$"{i.Kind}:{i.Identity}").Distinct().Order(StringComparer.Ordinal));
        var key=Key(current);double end=current.NavEndM;
        var cardinals=current.Identities.Where(i=>i.CardinalConfidence is "HIGH" or "MEDIUM"&&i.Cardinal.Length>0)
            .GroupBy(i=>i.Identity).ToDictionary(g=>g.Key,g=>g.Select(i=>i.Cardinal).ToHashSet());
        foreach(var s in spans.Where(s=>s.SegmentIndex>current.SegmentIndex).OrderBy(s=>s.SegmentIndex))
        {
            if(s.Status!="RESOLVED"||Key(s)!=key||Math.Abs(s.NavStartM-end)>.01)break;
            if(s.Identities.Any(i=>i.CardinalConfidence is "HIGH" or "MEDIUM"&&i.Cardinal.Length>0&&
                cardinals.TryGetValue(i.Identity,out var directions)&&directions.Any(c=>c!=i.Cardinal)))break;
            foreach(var i in s.Identities.Where(i=>i.CardinalConfidence is "HIGH" or "MEDIUM"&&i.Cardinal.Length>0))
            {
                if(!cardinals.TryGetValue(i.Identity,out var directions))cardinals[i.Identity]=directions=[];
                directions.Add(i.Cardinal);
            }
            end=s.NavEndM;
            if(end>=actionNav)break;
        }
        return Math.Max(0,Math.Min(end,actionNav)-nav);
    }

    private static LiveRoadContext RoadContext(RoadIdentitySpan[] spans,string reason,string? preferred=null)
    {
        var labels=spans.Where(s=>s.Status is "RESOLVED" or "UNKNOWN").SelectMany(s=>s.Identities.Length>0?s.Identities:
                s.Candidates.Length==1?s.Candidates:[])
            .Where(i=>preferred is null||i.Identity==preferred).GroupBy(i=>(i.Identity,i.Kind,i.Cardinal))
            .Select(g=>new RoadLabel(g.Key.Identity,g.Key.Kind,g.Key.Cardinal,g.All(i=>i.IdentityConfidence=="HIGH")?"HIGH":
                g.Any(i=>i.IdentityConfidence=="LOW")?"LOW":"MEDIUM",
                g.All(i=>i.CardinalConfidence=="HIGH")?"HIGH":g.Any(i=>i.CardinalConfidence=="LOW")?"LOW":g.Any(i=>i.Cardinal.Length>0)?"MEDIUM":"NONE",
                g.SelectMany(i=>i.ProofIds).Distinct().Order(StringComparer.Ordinal).ToArray())).ToArray();
        labels=labels.Where(l=>l.Cardinal.Length>0||!labels.Any(other=>other.Identity==l.Identity&&other.Kind==l.Kind&&other.Cardinal.Length>0)).ToArray();
        return new(labels.Length>0?(labels.Any(l=>l.Confidence=="LOW")?"CANDIDATE":"RESOLVED"):spans.Any(s=>s.Status=="CONFLICT")?"CONFLICT":
            spans.Any(s=>s.Status=="AMBIGUOUS")?"AMBIGUOUS":"UNKNOWN",labels,spans.Select(s=>s.Id).ToArray(),reason);
    }

    private LiveRoadContext? ConnectionDestination(RoadIdentitySpan? span)
    {
        if(span is null||span.Status is not ("UNKNOWN" or "AMBIGUOUS")||route?.RoadIdentities is not {} identities)return null;
        var c=identities.Contexts.LastOrDefault(c=>c.ConnectorSpanIds.Contains(span.Id));
        return c is null?null:RoadContext(identities.Spans.Where(s=>c.JoinedSpanIds.Contains(s.Id)).ToArray(),
            "CURRENT_CONNECTOR_DESTINATION_NOT_ITS_OWN_NAME");
    }

    public LiveGuidance Tick(long nowMs)
    {
        if (nowMs < messageTime) throw new InvalidDataException("Clock moved backwards.");
        timestamp = nowMs;
        if(sourceSuspended && last is { } source) return source with {TimestampMs=nowMs};
        if (lastVehicle >= 0 && nowMs - lastVehicle > StaleAfterMs)
            return Empty("STALE", "Vehicle data expired; old instruction suppressed.");
        if (paused) return Empty("PAUSED", "Progress frozen.");
        return last is { } l ? l with { TimestampMs = nowMs } : Empty(route is null ? "NO_ROUTE" : "WAITING_POSITION", "Awaiting vehicle state.");
    }

    private void Reset()
    {
        route = null; previous = null; completed.Clear(); lastVehicle = -1; paused = false; sourceSuspended=false; last = null;
        phaseAction = null; phaseRank = 0;
        laneCache.Clear();
        groupCache.Clear();fieldCache.Clear();actionFacts.Clear();
    }

    private LiveGuidance Empty(string status, string detail, LiveProof[]? proofs = null)
    {
        last = new(timestamp, status, route?.RouteId, revision, null, null, null, null,
            null, null, null, null, null, null, proofs ?? [], previous?.NavM, previous?.WorldM, detail);
        return last;
    }

    private bool ActionProven(LiveAction a)=>Facts(a).Proven;
    private static bool ProofSupportsAction(LiveAction a) => a.Confidence == "HIGH" && !string.IsNullOrWhiteSpace(a.BranchId) &&
        a.Proofs.Any(p => p.Field == "action" && p.Value == a.Instruction && p.Confidence == "HIGH" &&
            p.Source == "PPD" && !string.IsNullOrWhiteSpace(p.SourceId) && p.BranchId == a.BranchId);

    private string? Field(LiveAction a,string field){
        var key=(a.Id,field);if(fieldCache.TryGetValue(key,out var value))return value;
        return fieldCache[key]=ResolveField(a,field);
    }
    private string? ResolveField(LiveAction a, string field)
    {
        var groups=Groups(a);
        if(groups.Length>0)
        {
            var confirmed=groups.Where(g=>g.Confidence=="HIGH"&&g.SelectedBranchId==a.BranchId).ToArray();
            var refined=confirmed.Where(g=>g.SelectionStatus=="FUTURE_DISCRIMINATES").ToArray();
            var choices=(refined.Length>0?refined:confirmed)
                .GroupBy(g=>(g.Road,g.Cardinal,g.Destination)).ToArray();
            if(choices.Length!=1)return null;
            var g=choices[0].OrderBy(g=>g.GroupId,StringComparer.Ordinal).First();
            if(field=="cardinal"&&g.Fields.IdentityConfidence!="HIGH")return null;
            var (value,confidence)=field switch {"road"=>(g.Road,g.Fields.IdentityConfidence),
                "cardinal"=>(g.Cardinal,g.Fields.CardinalConfidence),"destination"=>(g.Destination,g.Fields.DestinationConfidence),
                "exit"=>(g.Exit??"",g.Fields.ExitConfidence),_ =>("","NONE")};
            return confidence=="HIGH"&&LexicallyValid(field,value)?value:null;
        }
        var values = a.Proofs.Where(p => p.Field == field && p.Confidence == "HIGH" &&
            p.Source == "SIGN_BOARD" && !string.IsNullOrWhiteSpace(p.SourceId) && p.BranchId == a.BranchId &&
            !string.IsNullOrWhiteSpace(p.Value) && LexicallyValid(field,p.Value)).Select(p => p.Value).Distinct(StringComparer.Ordinal).ToArray();
        // Never choose arbitrarily between contradictory fields.
        if (values.Length != 1) return null;
        if (field == "cardinal" && (Field(a,"road") is null || !a.Proofs.Any(p => p.Field == "road" &&
            p.Confidence == "HIGH" && p.BranchId == a.BranchId && p.Source == "SIGN_BOARD" &&
            a.Proofs.Any(c => c.Field == "cardinal" && c.Value == values[0] && c.SourceId == p.SourceId &&
                c.Confidence == "HIGH" && c.BranchId == a.BranchId && c.Source == "SIGN_BOARD")))) return null;
        return values[0];
    }

    private static bool LexicallyValid(string field,string value) => field switch
    {
        "road" => value.Split('|').All(SignTextClassifier.IsRoadIdentity),
        "cardinal" => value is "NORTH" or "SOUTH" or "EAST" or "WEST",
        "destination" => value.Split('|').All(SignTextClassifier.IsDestinationCandidate),
        "exit" => System.Text.RegularExpressions.Regex.IsMatch(value,@"^\d{1,4}[A-Za-z]?$"),
        _ => false
    };

    private double WorldAt(double nav, int segment)
    {
        var edges = route!.Edges.Where(e => e.SegmentIndex == segment).ToArray();
        var e = edges.FirstOrDefault(e => e.NavStartM <= nav && e.NavEndM >= nav) ?? edges[^1];
        var t = e.NavEndM > e.NavStartM ? Math.Clamp((nav-e.NavStartM)/(e.NavEndM-e.NavStartM), 0, 1) : 0;
        return e.WorldStartM + (e.WorldEndM-e.WorldStartM)*t;
    }

    private Projection? Project(LiveVehicle v)
    {
        Projection? best = null,local=null;
        double? measured=route!.NavigationRemainingAtOriginM is double origin&&v.NavigationRemainingM is double remaining?
            origin-remaining:null;
        if(measured is <0||measured>route.Edges[^1].NavEndM+10)measured=null;
        for (int i = 0; i < route!.Edges.Length; i++)
        {
            var e = route.Edges[i];
            bool nearPrevious=previous is not {} p||!(e.WorldEndM < p.WorldM-30 || e.WorldStartM > p.WorldM+250);
            if(!nearPrevious&&measured is null)continue;
            var dx = e.End.X-e.Start.X; var dz = e.End.Z-e.Start.Z;
            var len = dx*dx+dz*dz;
            if (len < 1e-8) continue;
            var t = Math.Clamp(((v.Position.X-e.Start.X)*dx+(v.Position.Z-e.Start.Z)*dz)/len, 0, 1);
            var x = e.Start.X+dx*t; var z = e.Start.Z+dz*t;
            var lateral = Math.Sqrt(Math.Pow(v.Position.X-x, 2)+Math.Pow(v.Position.Z-z, 2));
            var vertical = Math.Abs(v.Position.Y-(e.Start.Y+(e.End.Y-e.Start.Y)*t));
            var heading = Math.Atan2(dz, dx)*180/Math.PI;
            var angle = Math.Abs(Math.IEEERemainder(v.HeadingDeg-heading, 360));
            if (lateral > 25 || vertical > 6 || (v.SpeedMps > 1 && angle > 75)) continue;
            var world = e.WorldStartM+(e.WorldEndM-e.WorldStartM)*t;
            var score = lateral + vertical*3 + (v.SpeedMps > 1 ? angle*0.1 : 0) +
                (previous is { } prev ? Math.Abs(world-prev.WorldM)*0.01 : 0);
            var nav=e.NavStartM+(e.NavEndM-e.NavStartM)*t;
            var candidate=new Projection(i,e.SegmentIndex,nav,world,score);
            if(nearPrevious&&(local is null||score<local.Score))local=candidate;
            if(measured is double station)
            {
                var scale=(e.NavEndM-e.NavStartM)/Math.Max(1,e.WorldEndM-e.WorldStartM);
                var residual=Math.Abs(station-nav)/Math.Max(1,scale);
                if(residual>Math.Max(8,v.SpeedMps*2))continue;
                score+=residual;
                candidate=candidate with{Score=score};
            }
            if(best is null||score<best.Score)best=candidate;
        }
        return best??local;
    }

    private LiveGuidance? EndpointGuidance(LiveVehicle v)
    {
        if(route is null)return null;
        double Distance(LivePoint point)=>Math.Sqrt(Math.Pow(point.X-v.Position.X,2)+Math.Pow(point.Z-v.Position.Z,2));
        var first=route.Edges[0];var end=route.Edges[^1];
        var endDistance=Distance(end.End);
        if(!route.IsPartial&&endDistance<=150&&Math.Abs(v.Position.Y-end.End.Y)<=10&&
            v.NavigationRemainingM is <=10)
            return Empty("COMPLETE","Native destination reached near the verified final access.");
        if(!route.IsPartial&&endDistance<=150&&completed.Count==route.Actions.Length&&
            v.NavigationRemainingM is >=0&&
            (previous is {} approach&&approach.WorldM>=end.WorldEndM-30||
                (v.Position.X-end.End.X)*(end.End.X-end.Start.X)+(v.Position.Z-end.End.Z)*(end.End.Z-end.Start.Z)>=0))
            return new(timestamp,"FINAL_APPROACH",route.RouteId,revision,null,"CONTINUE",v.NavigationRemainingM,
                v.NavigationRemainingM/1609.344,null,null,null,null,null,null,[],null,null,
                "Final access outside the road corridor; follow the destination marker.");
        var startDistance=Distance(first.Start);
        var origin=route.NavigationRemainingAtOriginM;
        var dx=first.End.X-first.Start.X;var dz=first.End.Z-first.Start.Z;
        var length=Math.Max(1,Math.Sqrt(dx*dx+dz*dz));
        var along=((v.Position.X-first.Start.X)*dx+(v.Position.Z-first.Start.Z)*dz)/length;
        var lateral=Math.Abs((v.Position.X-first.Start.X)*dz-(v.Position.Z-first.Start.Z)*dx)/length;
        bool outsideEntry=along<=0||lateral>25;
        if(startDistance<=150&&Math.Abs(v.Position.Y-first.Start.Y)<=10&&
            outsideEntry&&
            (origin is null||v.NavigationRemainingM is double remaining&&remaining>=origin-10))
            return new(timestamp,"APPROACHING_ROUTE",route.RouteId,revision,null,"CONTINUE",null,null,null,null,null,null,null,null,[],null,null,
                "Join the planned route from the current access area.")
                {ContinueToward=RoadContext(route.RoadIdentities?.Spans.Take(1).ToArray()??[],"FIRST_VERIFIED_ROAD")};
        return null;
    }
}
