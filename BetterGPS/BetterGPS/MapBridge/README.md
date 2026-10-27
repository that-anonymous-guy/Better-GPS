# BetterGPSMapBridge PPD v0.11

BetterGPSMapBridge turns the **route actually selected by ATS** into a structured navigation plan by joining the runtime route snapshot with the installed ATS map, PPD lane graph and sign assets.

The `--capture` commands connect the new BetterGPSProbe live 0.2.0 DLL to MapBridge. MapBridge loads the installed map once, prepares incoming raw GPS routes automatically, and displays guidance in the console. See [NATIVE_CAPTURE.md](NATIVE_CAPTURE.md) and [the French installation guide](../INSTALLATION_LIVE.md). `--live` retains the compiled-route protocol and shared replay engine described in [LIVE_PROTOCOL.md](LIVE_PROTOCOL.md).

v0.11 is deliberately conservative. It keeps every v0.10 report for A/B comparison, then adds an evidence layer whose job is to answer five questions before a fact is allowed into `BetterGPSPlan v3`:

1. does the driver really need to act?
2. which exact PPD branch is selected, and which branches were rejected?
3. which **board/AreaName** on a physical sign contains the useful semantic data?
4. is `NORTH/SOUTH/EAST/WEST` explicitly signed, or is the road merely pointing that way locally?
5. is the evidence exclusive enough to deserve `HIGH`?

## Architecture

```text
ATS route_task / BetterGPSProbe
        |  exact route order + ATS navigation distance
        v
TruckLib map topology
        |
        +--> exact PPD lane branches
        |       selected BranchId + alternatives
        |
        +--> selected world route corridor
        |
        +--> Sign / SignOverride / BoardOverride
        |       AreaName + typed object attributes
        |
        +--> sign template + board definitions
        v
v0.11 evidence resolvers
        |
        +--> DriverActionResolver
        +--> BoardSemanticResolver
        +--> Comparative BranchBoardMatcher
        +--> SignedCardinalResolver
        +--> field-level confidence
        v
BetterGPSPlan v3
```

The program **does not calculate a replacement route**. ATS remains the routing oracle.

## What v0.11 adds

### Exact BranchId instead of angle-only branch matching

Each decision prefab enumerates every lane path through the directed PPD NavCurve graph. Branches are grouped by exact target control node and receive a stable identifier:

```text
<PREFAB_UID>:<PPD_ENTRY_NODE>-><PPD_TARGET_NODE>
```

This lets v0.11 distinguish two branches that happen to have the same geometric class such as `SLIGHT_RIGHT`.

### Driver-action gating

The old maneuver class remains diagnostic, but the final guidance now asks whether the driver really needs to do anything.

- Straight-through/fork `KEEP_*` guidance is suppressed if every input lane reaches the exact selected BranchId. A real left/right diverging branch is **not** suppressed merely because every lane can technically reach it.
- If only an edge-contiguous subset of lanes reaches the branch, v0.11 emits `KEEP_LEFT` / `KEEP_RIGHT`.
- Non-edge lane subsets become `USE_SELECTED_LANES` instead of an invented left/right instruction.
- `ActiveLaneOrdinals` is currently kept equal to the immediately valid exact-lane set. v0.11 intentionally does **not** infer cross-item lane continuity from ordinal equality alone; early lane positioning is deferred until lane endpoints can be matched through every intervening item.
- `EXIT_*` only survives when explicit exit semantics are matched `HIGH` to the selected branch. Otherwise it is conservatively downgraded to `KEEP_*`.

### Board-level semantics

A physical `Sign` is **not** assigned wholesale to a branch. v0.11 preserves:

```text
Sign
  -> AreaName
  -> BoardOverride / template linked_board
  -> SignOverride object id
  -> attribute index
  -> attribute type
  -> raw value
```

This matters for multi-board gantries where one physical sign can advertise several mutually exclusive routes.

### Multi-board safety

ATS map data gives one physical anchor for the whole `Sign`, while `AreaName` separates the logical boards. v0.11 therefore refuses to promote a multi-board sign to `HIGH` from whole-sign proximity alone when two or more semantic boards remain plausible. A future validated model-locator resolver can improve this further without weakening the current safety rule.

### Comparative branch matching

For every board near a decision:

```text
score(selected branch)
score(alternative A)
score(alternative B)
...
```

`HIGH` requires both a strong absolute match **and a uniqueness margin** over the best competing branch. Strong evidence that fits two branches is `AMBIGUOUS`; if an alternative scores materially better it is `CONFLICT`.

### Signed cardinal != travel bearing

v0.11 keeps two separate concepts:

```text
signed_cardinal       // NORTH / SOUTH / EAST / WEST from explicit semantic evidence
travel_bearing_deg    // geometry only
```

Local geometry is never allowed to manufacture a signed cardinal. Interstate / US route parity is retained only as an `expected_axis_hint`, not as proof of NORTH/SOUTH/EAST/WEST.

### Field-level confidence

Confidence is attached to individual facts, not to an entire guidance event:

- `EXACT` — selected ATS / PPD structural fact.
- `HIGH` — explicit semantic evidence uniquely associated with the selected branch.
- `MEDIUM` — conservative propagation / probable association.
- `LOW` — weak inference.
- `AMBIGUOUS` — competing explanations remain plausible.
- `CONFLICT` — strong evidence disagrees.
- `NONE` — no supported value.

### Semantic validation before HIGH

The board-to-branch score describes association only. It no longer promotes every
field on that board to HIGH. Board and branch-match CSVs append
`identity_confidence`, `cardinal_confidence`, `destination_confidence`,
`exit_confidence`, `validated_toward`, and `validation_notes`.

- Lane/action text such as `LEFT LANE`, `RIGHT 2 LANES`, `SECOND RIGHT`, `TO`, and
  `PREPASS` is not classified as a destination.
- Destinations enter plan v3 only when a typed text override matches a city name
  loaded through the mounted `/def/city*.sii` entry points and their `.sui` includes, and the board has a HIGH
  branch association. Plausible streets, scenery places, untranslated localization
  tokens, and other unverified names cannot become HIGH. Place candidates remain
  in diagnostic `toward`; `validated_toward` contains only verified names.
- Route identity requires matching explicit shield/text evidence, or an unambiguous
  number on a resolved shield board with the correct route-family prefix.
- Signed cardinals require an explicit standalone or route-direction expression
  associated with a validated route in the same AreaName. `North Las Vegas` does
  not supply NORTH; disagreeing cardinal anchors produce CONFLICT.
- Exit numbers require explicit `EXIT <number>` text or a resolved exit board;
  bare shield numbers cannot become exit numbers.
- Identity intervals, including aliases, need an identity-specific direct anchor
  for HIGH. An alias cannot borrow the primary route's confidence.

The plan v3 CSV appends `exit_confidence`. Existing columns retain their order.
The additional `semantic_field_validation_failures` hard guard validates emitted
fields independently of their branch association. Unverified destinations are
omitted from final guidance, rather than displayed with guessed meaning.

## New global outputs

- `SIGN_AREA_ATTRIBUTES.csv` — `AreaName`, object id, attribute index/type/value.
- `SIGN_BOARD_INDEX.csv` — used `sign_board.*` definitions and resolved paths.
- `SIGN_BOARD_SCHEMA.csv` — generic SII dump of the used board definitions.

The existing v0.10 outputs (`SIGN_TEMPLATE_SCHEMA.csv`, `SIGN_TEMPLATE_AREAS.csv`, etc.) are preserved.

## New per-route outputs

- `route_*_driver_actions_v11.csv`
- `route_*_board_semantics_v11.csv`
- `route_*_branch_board_matches_v11.csv`
- `route_*_identity_intervals_v11.csv`
- `route_*_cruise_spans_v11.csv`
- `route_*_guidance_events_v11.csv`
- `route_*_bettergps_plan_v3.csv`
- `route_*_v11_regression.txt`

`route_*_bettergps_plan_v3.csv` is the intended high-level consumer format: continuous `ROUTE_SPAN`, intermediate `GUIDANCE`, then `ARRIVAL`.

## Regression guards

Each route writes five hard guards that must remain zero:

```text
geometry_derived_high_signed_cardinals=0
destinations_without_high_branch_match=0
straight_keep_events_when_all_input_lanes_reach_selected_branch=0
high_matches_below_required_branch_margin=0
semantic_field_validation_failures=0
```

`multi_high_identity_events` is diagnostic rather than a hard failure because signed-route concurrency can be legitimate.

## Requirements

- Windows / ATS installation and Documents profile matching the captured route.
- .NET 10 SDK.
- NuGet package `TruckLib` 0.5.1 (the project file restores it automatically).
- BetterGPSProbe route snapshot(s).

TruckLib 0.5.1 targets ATS map format 907 / game versions 1.59-1.61.

## Build

From a Visual Studio 2026 Developer PowerShell or any terminal with .NET 10 in PATH:

```bat
build_release.bat
```

or:

```powershell
dotnet restore
dotnet build -c Release
```

## Run

`run_example.bat` contains the same command shape as v0.10. Use the ATS root, Documents folder and probe capture matching your installation.

Example in PowerShell:

```powershell
dotnet run -c Release -- `
  --game-root "C:\Program Files (x86)\Steam\steamapps\common\American Truck Simulator" `
  --documents "$env:USERPROFILE\Documents\American Truck Simulator" `
  --probe "C:\path\to\probe-output" `
  --output "C:\path\to\mapbridge-v011-output"
```

Run `--help` for the authoritative option list.

## Semantic regression tests

From this directory:

```powershell
dotnet run --project ../tests/BetterGPSMapBridge.Semantics.Tests -c Release
```

The test executable checks instruction/place classification, genuine place names
containing compass words, typed evidence, route-family mismatches, cardinal
conflicts, numeric shields versus exits, and independent field confidence.
`../validation/verify-semantic-replay.ps1` verifies the saved 91-snapshot replay
and compares it with the original reports without modifying them.

## Important limitations

- `gps_voice_events` remains intentionally unused: its 48-byte entries are not publicly decoded well enough to treat as an oracle.
- Board SII schemas are discovered dynamically. Attribute indexes are preserved rather than globally hard-coded to meanings SCS has not documented.
- Multi-board physical locator positions are not yet treated as proven; unresolved multi-board branch attribution is capped below `HIGH` instead of guessing.
- Roundabout synthesis remains a later step; `reachable_cycles_seen` is not a roundabout count.
- v0.9/v0.10 diagnostic reports may still contain their historical geometry-derived cardinal fields. **BetterGPSPlan v3 does not use geometry as signed-cardinal proof.**

See `IMPLEMENTATION_NOTES_v0.11.txt`, `RESEARCH_v0.11.md` and `VALIDATION_v0.11.txt` for the design and validation details.
