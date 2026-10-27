# International LT / LoneStar — T8520 / CoPilot V9

Stock vehicle IDs are `intnational.lt` and `intnational.lonestar`. Both GPS
producers use `/ui/gps.sii` and the shared GPS alias. The measured visible UV
rectangle is (0.0010614395, 0.2263684273) to (0.9972796440, 0.7708802223),
physical aspect 1.8408704. The shared 1473×800 canvas uses layout 12; each truck
has separately generated model-space touch bounds excluding cluster vertices.

The 301-frame, 30 fps `international_t8520_intro` is a lossless BGRA/XPRESS-HUFF
timeline, decoded by the existing preparation worker only. All frames are
round-trip verified by the importer. The movie opens lazily for the selected
device, not during DLL initialization. Playback starts at its first visible
upload, cuts directly to `lt8520_home` at 10034 ms, and uses existing contact,
session, pause, unknown-ignition and cold/streamed anti-flash protections.
The supplied Home and four requested CoPilot V9 textures preserve source pixels
and alpha; `copilot_v9_topallternative` is intentionally excluded.

Home and both map pages show a white bold 12-hour clock with smaller AM/PM,
without outline. Map presentation reuses T680 maneuver/distance, street/shield,
speed limit, real speed, remaining distance, ETA and the existing CoPilot theme.
Only their module positions differ. Navigation on Home expands the app from its
tile using the existing phone-style easing and rounded raster compositor,
followed by the CoPilot card from the supplied switch reference, then a direct
crossfade into the actual live map. Nav and Notrip Apps targets use their own
footer rectangles; edited placements/global zoom and the Home shortcut remain
supported. Existing phone transitions are unchanged.

Layout version 2 enlarges speed/limit about 70%, lowers zoom and instruction
text, and enlarges/repositions the clock about 10%. Existing saved layouts are
rebased by the default-position deltas while preserving custom offsets/scales;
`InternationalLayout 2` prevents repeated migration.

The inverse Home animation now uses the existing iPhone GPU compositor. The
International's own current native map input and actual CoPilot UI are composed
on a private deferred context (or full D3D11.1 context-state swap), then frozen
and shrunk toward its Navigation tile over 350 ms. Padded marker rows are cropped
out of that snapshot. It never substitutes the launch-logo image on closing.
The last app frame is retained before the first closing composite is ready, so
Home cannot flash ahead of the animation. Actor/session/power/revision checks
reject stale frames. No extra worker, CPU map readback or native shader replay
is introduced; other trucks and stable Home pages do not run this compositor.

The native International logo is extracted from the stock LT atlas at the exact
UVs used by its existing page-zero logo node. A high-layer gated image covers
only the changing info area (logical x=0..800, y=90..480) for three seconds.
It does not write the native page selector: the previously chosen page remains
untouched behind it and reappears at the cut. Shared gear/odometer and separate
auxiliary dashboard screens remain native. No private UI jobs, synthesized key
presses or savegame edits are introduced. Each original cluster UI is preserved
with only one appended node/child.

Checks: `OverlayPreviewTest <fresh-dir> --international`, `--startup-guards`,
`--zonar`, `--icc5`, and `TouchPhoneTest <fresh-dir>`. Tests cover both producer
identities, Original mode, 301 decoded frames, direct cuts, first-visible/pause
semantics, both themes/pages, animated touch entry, different Apps targets,
layout persistence, native 2D composition/depth picking and bounded packing at
slots 0/31/32/127. These are offline checks, not live ATS certification.
