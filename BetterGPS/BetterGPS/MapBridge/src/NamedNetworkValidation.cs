namespace BetterGPSNavigation;

internal static class NamedNetworkValidation
{
    public static void Validate(NamedNetworkSnapshot data,DirectedRoadNetworkGraph? preparedGraph=null)
    {
        void Require(bool ok,string reason){if(!ok)throw new InvalidDataException("Named network: "+reason);}
        Require(data.Version==1&&data.Portions.Select(p=>p.Id).Distinct().Count()==data.Portions.Length,"schema/unique portions");
        var graph=preparedGraph??new DirectedRoadNetworkGraph(data.Region,data.Portions,data.Connections,data.Errors);
        var accesses=NativeAccessRoads.Find(graph);
        var portions=data.Portions.ToDictionary(p=>p.Id);var sources=data.Sources.ToDictionary(s=>s.Id);
        // Proof scopes are immutable for this graph and source set. Validate
        // every label, but compute each scope/confirmation only once per source.
        var scopes=new Dictionary<(string Rule,string Source),HashSet<string>>();
        var streetChecks=new Dictionary<string,bool>();var overlayChecks=new Dictionary<string,bool>();
        bool StreetConfirmed(NetworkNameSource s)
        {
            if(!streetChecks.TryGetValue(s.Id,out var ok))streetChecks[s.Id]=ok=NamedRoadNetwork.StreetPlateConfirmed(graph,s,data.Sources);
            return ok;
        }
        bool OverlayConfirmed(NetworkNameSource s)
        {
            if(!overlayChecks.TryGetValue(s.Id,out var ok))overlayChecks[s.Id]=ok=NamedRoadNetwork.CorroboratedMapOverlay(graph,s,data.Sources);
            return ok;
        }
        bool InScope(string rule,NetworkNameSource s,string portion)
        {
            if(!scopes.TryGetValue((rule,s.Id),out var set))
            {
                IEnumerable<string> ids=rule switch{
                    "JUNCTION_ROUTE_ARM"=>NamedRoadNetwork.SharedRouteArmScope(graph,s),
                    "SIGNED_OUTGOING_ROAD"=>NamedRoadNetwork.LocalTurnScope(graph,s),
                    "UNBRANCHED_NETWORK_CONTINUITY"=>s.Owners.SelectMany(id=>NamedRoadNetwork.UnbranchedContinuity(graph,id)),
                    "RECIPROCAL_GEOMETRIC_CONTINUITY"=>s.Owners.SelectMany(id=>RoadNameContinuity.Corridor(graph,id)),
                    "CORROBORATED_SIGNED_BRANCH_CONTINUITY"=>RoadNameContinuity.SignedCorridor(graph,s),
                    _=>throw new InvalidDataException("Unknown proof scope")};
                scopes[(rule,s.Id)]=set=ids.ToHashSet(StringComparer.Ordinal);
            }
            return set.Contains(portion);
        }
        var independentNames=data.Sources.Any(s=>s.Source=="CORROBORATED_STREET_INTERPRETATION")?
            NamedRoadNetwork.Solve(graph,data.Sources.Where(s=>s.Source!="CORROBORATED_STREET_INTERPRETATION").ToArray()):[];
        foreach(var p in data.Portions)
        {
            Require(p.Id==DirectedRoadNetworkGraph.Id(p.ItemUid,p.EntryNodeUid,p.ExitNodeUid)&&p.Paths.Length>0,"directed identity");
            Require(p.Paths.All(path=>path.Length>=2&&path.All(v=>BoardModelResolver.Valid(v,3))),"finite geometry");
            Require(p.EntryNodeUid.Length==16&&p.ExitNodeUid.Length==16&&p.ItemUid.Length==16&&
                (p.EntryNodeUid+p.ExitNodeUid+p.ItemUid).All(Uri.IsHexDigit),"native UIDs");
        }
        foreach(var c in data.Connections)
            Require(portions.ContainsKey(c.From)&&portions.ContainsKey(c.To)&&portions[c.From].ExitNodeUid==portions[c.To].EntryNodeUid&&
                portions[c.From].ItemUid!=portions[c.To].ItemUid&&c.Status is "NATIVE_PORT" or "UNRESOLVED_GEOMETRY","port connection");
        foreach(var s in data.Sources)
        {
            Require(RoadIdentityRules.Valid(s.Kind,s.Identity)&&BoardModelResolver.Valid(s.Position,3)&&
                s.Cardinal is "" or "NORTH" or "SOUTH" or "EAST" or "WEST"&&RoadIdentityRules.Confidence(s.ReadingConfidence),"source fields");
            Require(s.Normal is null||BoardModelResolver.Valid(s.Normal,3)&&Math.Abs(BoardModelResolver.V(s.Normal).Length()-1)<.01,"normal");
            Require(s.Owners.All(portions.ContainsKey)&&s.Alternatives.All(a=>portions.ContainsKey(a.PortionId)&&
                LiveProtocol.Finite(a.DistanceM,a.HeightM)&&a.DistanceM>=0&&a.HeightM>=0&&(a.FaceDot is null||double.IsFinite(a.FaceDot.Value))),"owners");
            Require(s.Role is not ("GuideTo" or "CrossStreet")||s.Owners.Length==0||
                s.Role=="GuideTo"&&(s.AssociationStatus=="DIRECTIONAL_BRANCH_TARGET"&&
                GuideSignLinker.Associate(graph,s with{Owners=[],AssociationStatus=""}).Owners.SequenceEqual(s.Owners)||
                s.AssociationStatus=="ADVANCE_EXIT_BRANCH_TARGET"&&AdvanceExitLinker.Valid(graph,s,sources)),"directional branch evidence");
            Require(s.Role!="OwnRoute"||s.Normal is not null||s.Cardinal=="","unoriented cardinal");
            if(s.Source=="DIRECTIONAL_STREET_ROWS")Require(
                StreetNameLinker.DirectionalArm(graph,s).Owners.SequenceEqual(s.Owners),"local directional street arm evidence");
            if(s.Source=="STREET_CONFIRMATION_SIGN")Require(
                NamedRoadNetwork.Reassociate(graph,s).Owners.SequenceEqual(s.Owners),"overhead street axis evidence");
            if(s.Source=="UNCLASSIFIED_STREET_SIGN")Require(s.Owners.Length==0&&s.AssociationStatus=="STREET_INTERPRETATION_UNRESOLVED",
                "unknown street interpretation cannot carry owners");
            if(s.Source=="CORROBORATED_STREET_INTERPRETATION")
            {
                var reproduced=StreetNameLinker.InterpretWithIndependentNames(graph,s with{Source="UNCLASSIFIED_STREET_SIGN",Owners=[],Role="CrossStreet"},data.Sources,independentNames);
                Require(reproduced.Source==s.Source&&reproduced.StreetAxisMode==s.StreetAxisMode&&reproduced.Owners.SequenceEqual(s.Owners)&&
                    reproduced.InterpretationProofIds.SequenceEqual(s.InterpretationProofIds),"independent street interpretation evidence");
            }
        }
        Require(data.Names.Length==data.Portions.Length&&data.Names.Select(n=>n.PortionId).Distinct().Count()==data.Portions.Length,"coverage");
        foreach(var n in data.Names)
        {
            Require(portions.ContainsKey(n.PortionId)&&(n.Status=="RESOLVED")== (n.Identities.Length>0),"name state");
            foreach(var label in n.Identities.Concat(n.Candidates))
            {
                Require(label.Confidence is "MEDIUM" or "LOW"&&label.CardinalConfidence is "MEDIUM" or "LOW" or "NONE","no unsupported HIGH");
                Require(label.SourceIds.Length>0&&label.SourceIds.All(id=>sources.TryGetValue(id,out var s)&&s.Identity==label.Identity&&s.Kind==label.Kind&&
                    (s.Role is "OwnRoute" or "MapAnnotation" or "StreetAxis"||s.Role=="GuideTo"&&label.Rule is "CORROBORATED_SIGNED_BRANCH_CONTINUITY" or "BRACKETED_SIGNED_NETWORK_CORRIDOR" or "SIGNED_OUTGOING_ROAD" or "JUNCTION_ROUTE_ARM")),"name evidence");
                var proof=label.SourceIds.Select(id=>sources[id]).ToArray();
                if(label.Confidence=="MEDIUM")Require(proof.All(StreetConfirmed),"bare street plate requires independent same-axis evidence");
                if(label.Rule is "BRACKETED_SIGNED_NETWORK_CORRIDOR" or "CORROBORATED_SIGNED_BRANCH_CONTINUITY" or "SIGNED_OUTGOING_ROAD" or "JUNCTION_ROUTE_ARM")
                    Require(!accesses.Contains(n.PortionId),"a signed destination cannot become an access road name");
                if(label.Rule=="BRACKETED_SIGNED_NETWORK_CORRIDOR")Require(proof.Where(s=>s.Role=="GuideTo").All(s=>RoadIdentityRules.ReferenceOnly(s.Raw)),"guide-to, junction and exit cannot name an access corridor");
                Require(label.Cardinal.Length==0||proof.All(s=>s.Cardinal==label.Cardinal),"official cardinal must be corroborated by each source");
                if(label.Confidence=="MEDIUM")Require(proof.All(s=>(s.ReadingConfidence is "HIGH" or "MEDIUM")&&
                    (s.AssociationStatus is "UNIQUE_PHYSICAL_ROAD_BOTH_SENSES" or "ORIENTED_GEOMETRIC_CANDIDATE" or "JUNCTION_STREET_AXIS"||OverlayConfirmed(s)||
                    s.AssociationStatus=="DIRECTIONAL_BRANCH_TARGET"&&label.Rule is "CORROBORATED_SIGNED_BRANCH_CONTINUITY" or "BRACKETED_SIGNED_NETWORK_CORRIDOR" or "SIGNED_OUTGOING_ROAD" or "JUNCTION_ROUTE_ARM")),"provisional ownership provenance");
                if(label.Rule=="JUNCTION_ROUTE_ARM")Require(label.Cardinal.Length==0&&proof.All(s=>InScope(label.Rule,s,n.PortionId)),"shared shield road arm scope");
                else if(label.Rule=="SIGNED_OUTGOING_ROAD")Require(proof.All(s=>InScope(label.Rule,s,n.PortionId)),"local signed turn scope");
                else if(label.Rule=="DIRECT_NETWORK_ANCHOR")Require(proof.All(s=>s.Owners.Contains(n.PortionId)),"direct ownership");
                else if(label.Rule=="UNBRANCHED_NETWORK_CONTINUITY")Require(proof.All(s=>InScope(label.Rule,s,n.PortionId)),"unbranched scope");
                else if(label.Rule=="RECIPROCAL_GEOMETRIC_CONTINUITY")Require(proof.All(s=>InScope(label.Rule,s,n.PortionId)),"reciprocal continuation scope");
                else if(label.Rule=="CORROBORATED_SIGNED_BRANCH_CONTINUITY")Require(proof.Select(s=>s.ObjectId).Distinct().Count()>1&&
                    proof.All(s=>InScope(label.Rule,s,n.PortionId)),"independent signed branch scope");
                else if(label.Rule is "BRACKETED_UNIQUE_DIRECTED_NETWORK_PATH" or "BRACKETED_MANDATORY_NETWORK_CORRIDOR" or "BRACKETED_SIGNED_NETWORK_CORRIDOR")
                    Require(proof.Select(s=>s.ObjectId).Distinct().Count()>1&&proof[0].Owners.Any(a=>proof[1].Owners.Any(b=>
                        (label.Rule=="BRACKETED_UNIQUE_DIRECTED_NETWORK_PATH"?NamedRoadNetwork.UniquePath(graph,a,b,5000):NamedRoadNetwork.MandatoryCorridor(graph,a,b,double.PositiveInfinity,true))?.Contains(n.PortionId)==true||
                        (label.Rule=="BRACKETED_UNIQUE_DIRECTED_NETWORK_PATH"?NamedRoadNetwork.UniquePath(graph,b,a,5000):NamedRoadNetwork.MandatoryCorridor(graph,b,a,double.PositiveInfinity,true))?.Contains(n.PortionId)==true)),"bracket scope");
                else Require(false,"unknown scope rule");
            }
        }
    }
}
