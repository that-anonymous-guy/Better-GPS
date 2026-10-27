# Volvo VNL 2025 — Sygic

The stock GPS producer `/ui/dashboard/volvo_vnl2025_gps.sii` and
`/vehicle/truck/share/gps.tobj` are acquired independently of the cluster.
Measured exposed UVs are 0.005–0.995 on both axes. A 900×500 layout follows the
approximately 1.8:1 physical screen; the static strip below it is untouched.

## Startup and pages

- Cluster: all 150 frames at 30 fps, then a 500 ms alpha crossfade of the last
  frame directly over the live instruments. BGC3/XPRESS-HUFF round-trip is
  verified for every decoded BGRA frame; no BC1 compression or video thread.
- Infotainment: opaque supplied background, bottom bar at 1000 ms, menu at
  1250 ms, cases at 1350 ms. Clock is white, 12-hour AM/PM, without outline.
- Timelines start on the first matching visible upload. Native UI markers
  protect cold/streamed/deferred bindings; pause and actor/session changes
  follow the same guards as the previous trucks.
- Trip computer opens Nav/Notrip. The house in the navigation bottom bar
  returns Home. Existing interaction-mode, power, page/actor generation and
  moved/scaled placement checks apply. Zoom uses this device's current view.

## Navigation

Style 6 has independent settings (`_sygic.cfg`, layout tag 8), light/dark native
palettes and player material. Existing theme/icon IDs 0–29 are preserved;
Sygic uses IDs 30–35. Route colors: light #713FEB, dark #23A8F3. Player blue
is darker (#117FCA). Supplied textures determine module colors and shapes.

Instructions show a white maneuver, distance, explicit road names (including
“Interstate”), separate route shields and the next maneuver. Footer numbers
are bold, units regular, centered; speed uses a round mph module and US limit.

Trip progress remembers the first valid remaining distance observed for the
route and shows `1 − remaining / total`. It survives page/theme/power changes;
missing telemetry does not reset it. Cancellation, a known destination change
or a new vehicle/session resets it. A longer recalculated route extends the
remembered total. If first observed mid-trip, that observed remainder is the
baseline (ATS's distance channel does not report the original planned total).
The player moves left-to-right, with #A7A3AA behind and #936BF5 ahead to the
fixed supplied arrival icon.

## Verification

`OverlayPreviewTest <new-output-directory> --sygic` checks acquisition, UVs,
opaque staged startup, palette isolation, proportional trip progress, mapped
touch, persistence and immediate/deferred cluster alpha compositing. Existing
`--startup-guards` also exercises the VNL 2025 managed infotainment.
Reimport via `tools/import_sygic.py`; stock native children are preserved and
validated, with only an additive overlay appended to each original UI.
