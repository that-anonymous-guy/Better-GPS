# Freightliner Cascadia 2019 / 2024 — analog ICUC / Zonar

## Western Star 5700XE

The 5700XE reuses Zonar Home data, CoPilot navigation/theme and the same
Navigation/Messaging actions. Its supplied 1240×724 artwork has independent
module positions (layout 11); Cascadia and 49X retain their 1240×800 layout 10.
The GPS producer is `/ui/dashboard/westernstar_5700xe_gps.sii`, not `/ui/gps.sii`.
Placement uses the measured UV rectangle (0.0025229454, 0.0038890839) to
(0.9974770546, 0.5848579407), with physical aspect 1.7125791. Model-space touch
bounds are generated independently and exclude all cluster vertices.

Cluster startup shows `western_star_5700xe_splash_screen.dds` immediately for
2 seconds, then cuts directly to the native instruments. It does **not** inherit
the Cascadia/49X initial blank stage or their five-second image hold. The
1065×300 source pixels are preserved in a padded 1072×300 role-163 marker,
cropped to content across the native 800×214 UI. The first visible upload starts
the timer; contact, actor/session, pause and cross-vehicle protections are shared.
Infotainment remains opaque RGB(20,20,20) for 4 seconds, then fades into
`western_star_zonar_5700xe_home` over 0.5 seconds. The native GPS receives only
an additive startup guard, retaining its previously shipped customizations.

`tools/import_5700_zonar.py` imports the four DDS images with pixel/alpha
round-trip checks and verifies that removing each appended guard restores its
input UI exactly. The 5700XE tests cover both themes/pages, edited/scaled touch,
saved layout isolation, first-bind/streamed guards, GPU packing at slots
0/31/32/127 and the cluster's immediate/deferred hard cut at 2000 ms.

The actual 5700XE touch capture exposed an origin mismatch: the generated UI
rectangle `coords_b=4, coords_t=599` occupies source texture rows 425..1020
(on a 1024-high target), not rows 4..599. End-marker touch metadata now converts
SCS bottom-origin coordinates to texture top-origin before any independently
certified native-copy axis flips. The production tag plus the captured vertical
flip is exercised by `TouchPhoneTest`, not a hand-authored substitute tag.
The test fails on the previous version; it covers Navigation/Messaging and
Home/Nav/Notrip on the 5700XE and production end-marker tagging for every other
integrated GPS. No draw, hitbox, mesh bounds or copy-verification checks change.

## Existing Cascadia / 49X system

Western Star 49X reuses this exact runtime, GPS artwork, layout, live Home fields,
navigation and touch actions. Its only visual substitution is
`copilot/westernstar/western_star_49x_splash_screen.dds`, with the same 2-second
RGB13 delay, 5-second still and hard cut. Its stock `westernstar_49x.sii` receives
only the central LCD overlay; auxiliary instruments remain native. The measured
GPS size and UVs match the Cascadia analog screen, while model-space touch bounds
are generated independently for the 49X. Changing vehicle invalidates the shared
cluster clock/serial before either the worker or first GPU binding can reuse a
previous truck's image. Missing 49X artwork leaves its native instruments intact
without disabling the Cascadia intro (and vice versa).

Managed profile capacity is 128. Existing slots and their files stay in
place; a full old configuration can acquire the 49X at slot 32 without deleting
or replacing a phone/truck profile. Native marker dimensions, publication,
touch capture/history and picker indexing cover the expanded range. Bronco's
named fallback camera is kept outside managed camera IDs, while its persisted
INI section names stay unchanged. A genuinely full table now reports the cause
instead of silently skipping automatic GPS acquisition.

All 128 slots ship five native files each: map MAT/TOBJ/DDS and output MAT/TOBJ.
`tools/managed_map_assets.py --generate` adds missing slots without overwriting
existing artwork; its default check follows every MAT→TOBJ→DDS reference.
Packaging validates all 640 files before replacing staging and again after
copying assets. Negative regressions remove the exact slot-32 map material,
corrupt a last-slot TOBJ and break an output material; each must reject the
package input. The native phone UI consumes both inputs for every managed slot,
while retaining its original visible layer range.

The analog variants are identified by the vehicle plus `/ui/gps.sii` and
`/vehicle/truck/share/gps.tobj`. Digital ICC5 producers keep their independent
TomTom settings and geometry. Explicit Original/disabled choices are retained.

## Startup

- Central cluster LCD only: opaque RGB(13,13,13) for 2 seconds, the supplied
  `cascadia_icuc_cluster_splash.dds` for 5 seconds, then a direct cut to instruments.
- The 800×800 native atlas is preserved; the intro covers only its 600×800
  central LCD. Auxiliary displays are not covered.
- GPS: opaque RGB(20,20,20) for 4 seconds, followed by a 0.5-second fade into
  `zonar_home`. The fade composites against RGB20, never the live native GPS.
- Existing first-visible-upload, actor/session, contact and pause protections
  are reused. Cold and streamed bindings have matching sRGB gray guards.

## Interface

Source artwork is imported pixel-for-pixel to uncompressed DDS; no recompression
artifacts or runtime video decoder are introduced. The 1240×800 canvas fits the
measured 195.309×126.048 mm screen and its fractional atlas UVs. Publication uses
the bounded shared marker packer on both axes.

Home shows `Week N Wed`, a 12-hour AM/PM clock, the active ATS profile name,
navigation time remaining as a blue seven-segment `H:MM` display (only with a
route), and duty state. Long profile names shrink and, if necessary, truncate at
UTF-8 codepoint boundaries. No placeholder name is substituted.

Duty state follows the public SDK `job` configuration: a nonempty job means
orange On Duty; an empty job means white Off Duty, independently of a GPS route.
The active profile is read incrementally from the selected profile line in
`game.log.txt`, by the existing preparation worker only, at most 64 KiB per second.
It does not enumerate profiles, modify the log or create another worker.

Navigation reuses the T680 CoPilot theme, red player, maneuver, street/shields,
speed, distance and ETA functions. Only placement is adapted to Zonar artwork.
Home Navigation opens Nav/Notrip according to route state; Messaging on either
map returns Home. Existing interaction-mode and splash touch gates remain.

Physical touch bounds are separately generated for analog and ICC5 GPS models;
all sibling cluster vertices must fail the GPS bounds test.

## Checks

`OverlayPreviewTest <fresh-output> --zonar` covers both analog identities,
source pixels, startup sequence, live fields, duty configuration, layout fitting,
saved profiles, edited/scaled touch, bounded GPU publication, cluster hard cuts
on immediate/deferred contexts, pause and first-upload timing.

`--startup-guards` checks exact RGB13/RGB20 on cold and streamed SRVs, before
gameplay, during actor/session changes and on immediate/deferred contexts.
`TouchPhoneTest <fresh-output>` exercises the native composed-texture path,
depth-based picking and Home/Nav/Notrip actions for both analog models alongside
existing truck and phone cases. These are offline tests; final cabin appearance
and contact behavior still need a normal in-game check.
