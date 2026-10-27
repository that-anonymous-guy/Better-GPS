# BetterGPS touchscreen interaction

## Shared cluster/GPS identity and manual Home (2026-10-22)

The VNL 2025 broken-session capture recorded four positions exactly matching
the stock cluster (pieces 89/106/128/116 across its four interiors), not the
GPS. Both surfaces use six-index draws and the same UV tile. The previous
T680-only protection did not apply, and a per-slot 66 ms throttle let the
cluster suppress the real GPS. A valid copied texture is not a physical identity.

All eleven implemented stock integrated truck GPS identities now use measured
model-space bounds generated from their PMGs. The audited variants include 320
GPS vertices, all accepted, and 601 cluster vertices, all rejected. The margin
is 2 mm in model space, not an enlarged screen-space hitbox. Unknown/modified
integrated geometry fails closed. Phone/accessory/non-integrated producers do
not acquire a stock truck filter merely because of their vehicle name.

Cadence lanes also distinguish native draw range/base and actual vertex/index
bindings, including offsets, rather than only draw count/output. This handles
the VNL's equal-size shared-buffer draws. Main-view, immutable depth, actual
copy proof, focus, interaction mode and actor/power/page generation checks
remain mandatory. No shader replay, new thread or startup model parsing is added.

An explicit Home shortcut now has the same precedence/persistence as touch:
it overrides active guidance instead of being consumed by the route branch,
and Nav cannot immediately reopen on the next worker tick. Startup/power and
vehicle eligibility gates and per-device shortcut selection are preserved.

`TouchPhoneTest` exercises the real GPU copy/projection/depth/action path for
phones, accessories, Sony and all tactile truck families. VNL 2025 uses the
captured cluster positions, six-index cluster/GPS draws in both orders, shared
vertex pools with different base/index ranges and separate bindings. The page
suite covers active-route Home/resume and isolation for all page families.
These tests are not a substitute for the user's next live ATS acceptance run.

Regenerate/check the model table with `scripts/build_touch_gps_bounds.py` using
the stock screen inventory and `role-audit-full.json`; no runtime asset I/O.

## NavPlus page-specific Return and T680 (2026-10-20)

W900 NavPlus is confirmed functional live. Its Notrip Return uses a different
source-art rectangle than Nav; actions now follow those separate bounds rather
than reusing the square Nav button. T680 classic uses the same verified picking
backend with its own measured content fit, 1275x775 artwork and editable 5by3
placements. The T680 Home Navigation target is measured in its own image.
Rendering and targets use the same edge-anchored footer transformation, module
position/scale and inverse global zoom. Phone-only blue diagnostic app boxes
remain unchanged, as requested. See `kenworth-t680-screen.md` for dimensions,
asset paths and tests. Live T680 visual/click acceptance remains to be checked.

## Live GPS rejection, continuous hover and measured copy axes (2026-10-20)

The next live capture confirmed the 3D GPS meshes but rejected every Crown/Nuvi/
NavPlus copy with the old two-bit `identity=0 mismatch_mask=3`. That mask did NOT
identify whether RGB, alpha or UV orientation differed. Orange was only a
projected candidate. The previous copy path was press-only, explaining why GPS
outlines appeared after clicking and expired again instead of following hover.

With Touch diag visible, native copy acquisition now also refreshes without a
pointer press. Hover has trace zero and cannot enqueue an action. Outside this
explicit diagnostic mode, copying remains gesture-only. Repeated hover records
are limited to state changes and a five-second target heartbeat, rather than
exhausting the capture log at the 66 ms acquisition cadence.

Whole-image comparison now reports RGBA separately and measures four coordinate
orientations: identity, U reversal, V reversal and both. RGB must match across
the complete image under point or linear sampling, within the existing numeric
tolerance. A native pass forcing alpha opaque no longer invalidates otherwise
identical RGB/UV correspondence. Exactly ONE spatial orientation must pass;
uniform/symmetric, cropped, expired and unsupported mappings fail closed. This
is not an arbitrary rotation, color/tint, crop or compositing solver. The actual
cause in ATS must be read from the new evidence rather than assumed to be alpha
or an axis reversal solely from the old two-bit failure.

The inverse axis mapping is composed through every native copy parent, applied
before device content fit, and used by the picker, projected Home targets and
diagnostic UV probe. Result publication is synchronized with the existing CPU
worker. Native shaders are still neither queried nor replayed.

If NO RGB mapping passes with Touch diag enabled, one source/target pair per
device per capture is asynchronously staged, then saved by the existing worker:
`logs/touch/copy-slot-N-source.bmp`, `copy-slot-N-target.bmp`, `copy-slot-N.txt`.
These are only the tagged device UI textures, not the game/window framebuffer.
The previews show RGB opaque for inspection; text retains native alpha extrema,
formats, dimensions, gesture and capture ID. Game textures are not changed.
There is one pending readback and one queued report, no synchronous GPU wait,
render-hook file I/O or new thread. Filenames are reused to bound disk growth.
Normal successful picking still maps only its four-byte comparison verdict.

The two-phone capture showed one `Open Google Maps` on slot 8 at tick 309193375;
slot 9 left Home at 309194500 without a touch command or style change. This is
consistent with its independent initial Home dwell, not input broadcasting.
The configured default two-second dwell is preserved. `device-page` now records
`cause=boot-home-timer`, `touch-map`, `touch-home`, `shortcut`, etc. A test opens
one device early and confirms the other stays Home until its own timer expires.

The production WARP/debug fixture now tests flipped U/V and two-hop flip chains,
forced opaque alpha, repeated hover-only mesh refresh, correctly fitted GPS
button actions, rejected cropped/symmetric mappings, and actual asynchronous
failure-image export. Phone ownership, manual unlock, startup, frame coherence,
four depth formats and 48 OS/layout/zoom target checks remain covered. These
tests do not certify live ATS acceptance; the new native-copy verdict and any
failure images are the evidence to inspect on the next in-game capture.

Deployed DLL: 18,841,600 bytes, SHA-256
`6994D1247179E8BD9FB048DE25259B454D65298CE38EFF064B010C6E442FF6C4`.
Prior DLL/receipt and the user's live capture are backed up in
`plugins/BetterGPS/backups/touch-gps-mapping-before-20261020-3716902`.
All 1,772 installed settings/layout/assets checked retained identical hashes
before/after installation (aggregate
`8AC3170E2DA9129BF2740FFDFF3B9B50A0B551ABE9334BA1272B909B624F37B7`).

## Multiple devices and native UI composition (2026-10-20)

This section supersedes the historical phone-only restrictions below.

Live phone interaction was confirmed by the user with the preceding build.
The next correction removes two remaining phone-only assumptions:

- Each projected surface is evaluated using its OWN gesture/depth ticket.
  A newer ticket belonging to another screen cannot steal the contact. Samples
  are consumed per device, and an anchored swipe remains locked to that device.
- Phone Home app selection directly changes that physical device's saved-style
  profile. It does not select another device in the settings editor. Independent
  Home/page, theme, camera and power preferences remain per-device.

The common CPU picker now accepts Phone, Sony and Garmin page devices, but only
after the same generation, native projection, backbuffer and depth checks.
F150/Bronco remain outside these page/touch actions. Their detection, rendering,
layouts and assets are not modified.

The Crown and Garmin captures showed `vs.bytecode-not-retained
creation=unobserved` on an early native 2D UI pass, not texture-size/budget
rejection. Shader bytecode cannot be retroactively invented. `TouchComposition`
instead snapshots the actual tagged source and native destination AFTER that
draw, and compares the ENTIRE destination against a normalized point OR linear
sample of the source. UNORM/sRGB view conversion and 8-bit target rounding are
accounted for, together with subtexel filter-weight rounding on odd-size native
viewports (maximum linear-space channel difference 0.0085). The association
is per captured image, not a claim about all future executions of that shader.
The 8-bit minimum subtexel precision is specified in Microsoft's
[D3D11 functional specification, 7.18.16](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#TextureSamplingPrecision).

Only our compute shader runs on our private deferred context, executed with
state restoration TRUE. No native shader getter, replacement or draw replay is
used; the production replay guard remains disabled. The native draw runs once.
Only a four-byte verdict is mapped asynchronously; no screenshot/full-frame CPU
readback, Flush, GPU wait or additional thread is added. Capture runs on pointer
requests only: at most three jobs, one megapixel per image, 66 ms/target cadence.
The cadence follows the exact native target, not the device: several composition
hops for the same GPS can therefore be certified in the same frame. Copy verdicts
also require their complete bounded parent chain to succeed.
Pending/failed/expired copy ancestry cannot authorize a contact. Composition
tags are invalidated on subsequent writes and conservatively before external
command-list execution. The final 3D material still needs observed shader
metadata and valid native geometry/depth; arbitrary compositions, cropped image
transfers and native deferred geometry remain unsupported rather than guessed.

The WARP/D3D debug integration test covers two phones with reversed draw order
and newer unrelated tickets, plus Nuvi horizontal/3:2, NavPlus and Sony through
actual native UI draws with missing shader metadata, changing texture sizes,
nontrivial content fit, point/linear sampling, UNORM/sRGB views, 3D projection,
depth and Home/Notrip/Nav actions. Cropped transfers fail closed. The physical
phone front fixture also checks that opening an app on one device leaves the
other device's displayed pixels, Home state and selected settings unchanged.
These automated fixtures are not live ATS/Snowymoon certification for the GPSs.

With Touch diag enabled, `device-page` logs include each device slot, known index,
style, page and serial. `actual-image-copy` / `actual-image-verdict` distinguish
an acquired native composition from a rejected transfer. `surface-hit` and
`action` retain the selected physical device's slot throughout the gesture.

Deployed DLL SHA-256:
`DFC5BF55C5FF529AB924BE4F114E21DD21EDB37F9650E0E9DF705E7DA44E05D6`
(18,824,192 bytes). The preceding user-confirmed phone DLL, receipt and live
captures are preserved in
`plugins/BetterGPS/backups/touch-multidevice-before-20261020-f8b2109`.
Only the DLL and its receipt entry were updated. All 1,774 installed settings,
layout/generated-layout and current/legacy asset files retained identical hashes
before and after deployment (aggregate
`A76B9F81D96B2162CABE12B1758B6E03F7DAB147C06BA80D05BD8B9349E12711`).

## Coherent frame/passes and verified Home targets (2026-10-20)

The previous live capture accepted no contacts. It alternated between depth
state rejection and inconsistent pixel values, including a value outside [0,1].
The previous path kept a native depth resource reference while CPU geometry was
prepared, then read that mutable resource at a later Present. A readback age of
16 ms was not the age of the geometry. The following path supersedes that design.

Native geometry captures now carry the actual gesture trace, recorded pointer,
client size, frame, viewport, typed DSV format and depth/stencil state together.
Depth is snapshotted before ClearDepthStencilView, ClearView, discard or a
tracked copy overwrites its resource, or at the native frame boundary. A small
depth-ALWAYS three/six-vertex draw is also treated as a possible clear boundary;
that is a conservative heuristic, not general final-color/occlusion proof.
The original draw still runs once. No shader replacement/getter/replay is used.
Native execution on deferred contexts remains unsupported by the CPU path.

The full depth copy stays on the GPU, and only the recorded target texel is
mapped nonblockingly. CPU geometry is not published for input before its depth
snapshot completes. The picker no longer rereads native depth: it consumes the
immutable pixel captured with that geometry and rejects mismatched trace,
frame-ticket, resized client, page/power/vehicle generations or stale evidence.
Typed DSV formats drive decoding; the raw eight-byte maximum, source frame,
snapshot ID, actual geometry age, copy boundary and full depth-state flags are
logged with the verdict (`backend=cpu-frame`). Invalid values fail closed.
Private snapshot allocations share a 64 MiB cap and remain bounded by the
existing four-job queues. No production Flush or synchronous Map is added.

A depth-disabled secondary material pass no longer replaces a fresh valid
phone pass. Missing shader bytecode does not consume the capture throttle for
a later usable pass. StencilEnable alone is no longer an unconditional failure;
the native depth result is checked, with stencil state recorded for diagnosis.
This does not emulate every stencil/alpha/color-composition rule.

Phone Home hitboxes were inspected against both supplied OS artworks and
their installed copies (matching hashes). In the source 360x740 canvas:

| OS | Google Maps (x,y,w,h) | Waze (x,y,w,h) |
| --- | --- | --- |
| iPhone | 194,63,57,57 | 277,63,57,57 |
| Android | 198,64,55,55 | 282,64,55,55 |

Actions and diagnostic borders now share one forward target transform, including
the renderer's rounded fit sizes and actual centered zoom crop. With Touch diag
on, blue app borders on Home show those same action regions projected onto the
native screen. A persistent short-lived pink marker records a measured geometry
candidate independently of the depth verdict; green still means accepted depth.
For phone diagnostics, orange now draws the convex outer silhouette; unwelded
internal triangle seams cannot masquerade as the contour. Actual hit testing
still uses the original triangles/UVs, not that diagnostic silhouette.

The CPU integration fixture now tests frame A captured before clear followed by
native buffer reuse and changed vertex/camera data before CPU completion; a
later actual occluder in that same frame; a secondary depth-disabled material;
and native stencil-enabled draws. It also checks 48 OS/app/orientation/format/zoom
combinations against the actual fit/zoom raster, within two canvas pixels at
resampled edges, and writes annotated Home previews. These are not live ATS or
Snowymoon certification; the remaining physical projection must be checked in
the game. No settings/layout/assets are rewritten by this correction.

Deployed DLL SHA-256:
`CCD236F8CD792DD717C67FF4AD0B1EEC4FBF668918BAAAC144AB2CB1EFEC5811`.
Previous DLL/receipt and the live diagnostic capture are preserved at
`plugins/BetterGPS/backups/touch-frame-before-20261020-66f9c24d`.
All 1,771 audited configuration/resource files were unchanged after DLL
replacement, before updating only the DLL entry of the package receipt.
The CPU test (D3D debug layer enabled) and all sixteen standalone/preview
regressions passed against the final sources. Annotated previews and the 48
target checks are in `build/validation/touch-frame-projected-release-20261020`.
There has been no live ATS certification of this deployed DLL yet.

## Phone CPU actions (2026-10-20)

The phone surface observed in the live capture (slot 14 in that session) now
feeds the existing gesture/action worker. Device slots are not hard-coded:
the tagged surface must identify a current, enabled **Phone** on the initialized
player vehicle. Other device types remain disabled in this backend. F150 and
Bronco behavior, layouts, saved preferences and startup/page timelines are not
modified. Native GPU shader replay remains compile-time blocked in production.

Ctrl+F9 alone enables phone input; Touch diag is optional. Without diagnostics,
native geometry readback is requested only during an actual pending gesture.
The diagnostic overlay now draws boundary edges instead of internal triangle
diagonals. Pink marks a geometric candidate, not a proven touch; green marks a
recent depth-accepted sample during a gesture. Menu controls take priority both
at press and at subsequent sample positions.

Each requested sample intersects the measured CPU triangles, applies the native
sampler addressing and tagged content fit, and rejects ambiguous surfaces.
Only the full-viewport scene path matching the backbuffer is enabled; window
scaling samples the actual target texel center. Missing depth, stencil tests,
depth bias, custom pixel depth/coverage, intermediate-composition proof tokens,
multisampling and unsupported formats fail closed with explicit diagnostics.

At Present, the phone draw's native depth resource is copied in full **on the
GPU**, as required by D3D11. A second copy from a private typeless, non-depth-bound
texture transfers just one texel to a staging texture. There is no native draw,
shader getter/setter, replay or render-state restoration in this path. Query
polling uses DONOTFLUSH; Map uses DO_NOT_WAIT. One pending sample is allowed,
at most once per 16 ms while a gesture is pending, with a 150 ms expiry and a
3840x2160 resource limit. Final depth must equal the projected phone depth,
within two depth quantization units; a merely nearer candidate or a cleared
buffer does not establish a hit. D16, D24/S8, D32 and D32/S8 are supported.

Trace IDs connect `press`, `surface-hit backend=cpu-depth`, `gesture-queued` and
`action`/`action-rejected` in `logs/touch/touch.log` and the opt-in status capture.
Old asynchronous results cannot attach to a newer press. Vehicle, power, page,
focus and interaction-mode changes invalidate gestures. Actions remain on the
existing preparation worker, including manual unlock, explicit Home swipes,
and per-phone Google/Waze launch animations. Pressing Home repeatedly does not
toggle the app back open.

`TouchPhoneTest <fresh-output>` compiles with the production GPU replay guard,
captures actual native draws on WARP, and exercises projection -> one-texel
depth -> swipe -> Home -> app buttons -> Nav. It checks independent settings,
all four depth formats, a rendered late occluder, cleared depth, stale geometry,
power/capture/trace generations, overlapping devices, controls, startup and
manual unlock. The D3D debug layer checks API errors when available. Existing
geometry, page, menu and device regressions are separate tests.

This is **not live ATS/Snowymoon certification**. The CPU mesh may lag the
camera by up to its 150 ms freshness window. Depth equality is conservative,
but is not a full final-color/alpha/composition proof and does not establish
support for mirrors, inset targets or every external post-processing chain.
Initial live validation must check actual phone swipe and both app buttons;
unexpected rejections should be diagnosed by their trace, not by guessing a
different rectangle or borrowing the selected menu device.

Deployment: Release DLL SHA-256
`66F9C24D3ADFA572FC9476AA8EAEF9B6643722A4A230C733C5BB7993163B0A26`.
The previous DLL and manifest are backed up at
`plugins/BetterGPS/backups/touch-phone-actions-before-20261020-7a997569`.
All 1,771 audited installed configuration/resource files were unchanged after
DLL replacement; only the DLL entry in the package receipt was then updated.
The CPU phone integration test passed with the D3D debug layer enabled and no
API errors. The two standalone tests plus fourteen preview regressions passed
with fresh output directories (17 cases total). Live ATS input remains to be
validated by the user; no successful game session is claimed by these tests.

The following sections document the preceding diagnostic stages and the
quarantined replay design; their historical “actions disabled” statements do
not describe the new CPU phone path above.

## In-game status HUD and bounded captures (2026-10-20)

Ctrl+F9 retains the compact draggable header. The lower-right **Touch diag**
button toggles an opt-in, read-only status panel, even with settings collapsed.
Drag the diagnostic panel's own title to keep it away from the tested device.
It shows client mouse coordinates and ownership, press/release state, the
**selected device (not a claim of a hovered device)**, its published page,
power/startup state, vehicle initialization and generation IDs. Recent input and
outcome events share trace IDs with the status log. Panel clicks/drags and its
**New capture** button do not leak to device actions or settings underneath.

Production still blocks draw replay and readback processing of the experimental
GPU touch backend, with no UI switch to re-enable it. Read-only geometry observers
are now installed instead. The original status-only build reported projection
as unavailable; the new CPU prototype reports measured **candidates**, never
validated touch hits or a target borrowed from the selected-device setting.
Isolated test executables retain their compile-time GPU fixture. This diagnostic
build is not a touchscreen fix or live compatibility certification.

### CPU projection prototype (2026-10-20)

With Touch diag enabled during active gameplay, tagged native draws can produce
orange projected triangle edges, a numeric device-slot label and a pink cross
where the cursor intersects a triangle. The HUD shows raw material UVs and
coordinates after sampler addressing and the tagged content fit. Shared triangle
edges with matching UVs/device do not become two different hits. Overlapping
different candidates are marked ambiguous. **No result reaches TouchInput or
TouchActions.** Occlusion and final render-target-to-window correspondence are
unverified: this is specifically the visual alignment experiment, not proof that
an orange triangle is the visible, frontmost physical screen.

The observer retains bytecode and input declarations at native creation/bind
calls, without calling VSGetShader/PSGetShader or replacing/restoring native
shaders. Draws are executed once. It snapshots actual vertex/index streams,
constant buffers (including D3D11.1 ranges), viewport, target dimensions, sampler
addressing and texture-associated device identity into private staging buffers.
Query completion uses DONOTFLUSH and Map uses DO_NOT_WAIT; no wait/Flush is added
to the production readback path. Shader analysis and CPU evaluation run on the
existing worker. See Microsoft's [buffer-region copy contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copysubresourceregion)
and [nonblocking Map behavior](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map).

Supported initial path: immediate-context triangle lists with known per-vertex
input declarations, float/half/normalized formats and straight-line vertex
arithmetic whose actual dependencies are resolved. The sampled UV must be a
direct, unambiguous pixel input matching a vertex output. There is no guessed
matrix slot, TEXCOORD number, semantic packing, or global UV modulo. Control
flow, calculated pixel UVs, unsupported instructions needed by position/UV,
instancing, deferred/indirect draws, missing bytecode and excessive buffers
produce explicit failure instead of fabricated contours. ClearState and
non-restoring command-list execution invalidate observed shader state.

Bounds: 4 pending and 4 ready jobs, 8 MiB per copied range / 16 MiB per job, 1,536
indices, 512 unique vertices and 512 vertex instructions. Captures are throttled
to one per device slot per 66 ms. Readbacks expire after 200 ms; contours after
150 ms and on vehicle-epoch changes. Debug wireframe uploads are capped at 30 Hz
and 3840x2160. Sampling delay is visible during camera movement and is not
claimed to be zero. Live FPS/latency and Snowymoon/ReShade compatibility still
need measurement. No configuration/layout/assets are rewritten.

The worker saves at most eight shader-pair evidence sets per process to
`logs/touch/geometry-<hash>.txt` (draw/layout/VS/PS) plus `.inputs` (BGPGEOM1:
surface, viewport, layout, actual constant ranges and up to 512 decoded vertex
inputs). Pointer and geometry-probe records share the status capture's trace IDs.
Missing inputs or unsupported native shaders are useful diagnostic outcomes;
do not instruct the user to keep swiping while these failures persist.

Initial prototype deployment: DLL SHA-256
`49F64809E3806DE45C6C8CB79C7DB024F19AF9591B8D24BFD63DDCC4D6FC0A07`.
The previous status-only DLL and manifest are preserved at
`plugins/BetterGPS/backups/touch-geometry-before-20261020-b2b83285`.
All 815 checked configuration/layout/asset files were unchanged across this
installation. The two standalone tests plus 14 preview cases passed with fresh
output directories. Live ATS alignment and proxy compatibility are still pending.

### Acquisition correction after live captures (2026-10-20)

The first live prototype observed 22,815 candidate draws but completed zero
captures. Its logs alternated between missing vertex bytecode and the ambiguous
`index buffer readback unavailable/budget` message. No orange contour could have
been drawn; this was not a hover/input error. The generic message did not prove
that the native index pool exceeded the limit, as it also covered missing buffers,
unsupported formats and staging failures.

Two code defects are corrected: native shader bindings are now observed even
before enabling diagnostics and remain valid when a capture restarts. Only
native ClearState/non-restoring command-list execution (or actual rebinding)
invalidates them. Capture generations still reject previous asynchronous geometry;
they must not invalidate a shader still bound in the native context.

Index copies now use exactly `IA byte offset + StartIndex * index size` and
`IndexCount * index size`, not the whole shared pool. VB ranges preserve the
original stream offsets and BaseVertex, including negative bases. Nonindexed
draws have an exact vertex interval; R16 draws use their entire guaranteed index
domain intersected with the actual VB. R32 draws use the full possible interval
within the bound VB and still reject an excessive range, rather than guessing a
maximum index or combining data from different frames. Constant buffers keep
their whole snapshot with the actual shader-visible range. All copies are
same-draw, asynchronous and private; no native draw/shader replay is enabled.

Shader failures distinguish missing bindings, dynamic linkage, missing creation
metadata, per-shader size/retention-budget rejection, metadata storage failure,
interface queries and bytecode reads. Buffer errors include role, slot, native
size, copied offset/length, job bytes, source flags and HRESULT, distinguishing
absent buffers, invalid ranges, range/job limits, staging creation and Map/query
failures. Geometry status is logged without requiring a new mouse press and is
rate-limited to one summary per second (plus gesture/capture changes), with
cumulative failure-class counts so alternating errors cannot flood or hide one
another.

The WARP acquisition regression now binds shaders while diagnostics are closed,
restarts captures without rebinding them, rejects old readbacks, and projects
from 20 MiB index / 32 MiB vertex pools using six-byte R16 and twelve-byte R32
index copies. It exercises nonzero offsets, split streams, positive/negative
BaseVertex, and deliberately injects E_OUTOFMEMORY from CreateBuffer to distinguish
API errors from budget errors. These reproduce code failure cases; they do not
constitute a successful live ATS acquisition or alignment measurement.

Acquisition-correction DLL installed SHA-256:
`7A997569AF6F66D4FF789EC374170DE62F52D9CA0BE0FE84118A2B826C818EF9`.
Previous DLL/manifest: `plugins/BetterGPS/backups/touch-acquisition-before-20261020-49f64809`.
All 815 checked configuration/layout/asset files remained unchanged. Clean
Release rebuilds and the two standalone / fourteen preview regression cases
passed; live ATS acquisition, alignment and proxy-chain validation remain pending.

Captures are written by the existing worker to
`plugins/BetterGPS/logs/touch/touch-status.log`. Starting a fresh capture preserves
the preceding file as `touch-status.previous.log`. Each capture has a UTC header,
process/capture ID, explicit backend status, context and pointer edge records,
and at most 1,024 accepted records with a 128-entry queue. Exhaustion and dropped
records are shown in the panel; use New capture to continue. Log open/write
failure is visible in-game. Older `touch.log` / material captures are untouched.

No new thread, file I/O from a pointer/Draw/Present callback or device-profile
write is introduced. HUD rasterization is on the preparation worker, capped at
10 Hz (1 Hz when unchanged), and texture uploads occur only for new images.
Diagnostics start disabled; closing Ctrl+F9 hides the overlay. Native transform
constants and CPU projection candidates are measured when supported; occlusion
and final-composition proof remain unavailable.

Validation:

- `TouchGeometryMathTest` checks perspective-correct barycentrics, clipping,
  viewport scaling, addressing, real register/swizzle arithmetic and fail-closed
  missing/unsupported dependencies.
- `OverlayPreviewTest <fresh-output> --touch-geometry` observes a real WARP draw
  with split streams, index start/base and a non-identity matrix in b3. It checks
  CPU projection and sampled TEXCOORD2, changes the camera constants, adds a
  second device, invalidates native state and expires old contours, without
  shader replacement, native replay or queued device actions. It is not ATS.
- `TouchStatusTest <fresh-output>` checks the production replay guard, opt-in
  capture, ownership, missing/stale/invalid coordinates, bounded queues/rotation,
  control isolation, cancellation and failed log creation.
- `OverlayPreviewTest <fresh-output> --touch-status` exercises the actual HUD
  upload and menu rendering path on WARP, saves a layout preview and verifies
  restoration of the pixel shader and render target. It is not a test of the
  user's live Snowymoon/ReShade chain.

The diagnostic DLL was installed on 2026-10-20 with SHA-256
`B2B832850EFEADA981124BF77E6C393F7BC4BBB3D662953DAE5D93F6D475A5CA`.
The prior DLL and package manifest remain in
`plugins/BetterGPS/backups/touch-status-before-20261020-43537c5a`.
All 815 checked configuration/layout/asset files retained the same hashes.
The production status test and 13 preview regressions passed using fresh output
directories (status rendering, menu mouse/shortcuts, gameplay, device pages,
managed devices/phone controls, Garmin, NavPlus, Bronco, F150 horizontal,
installed layouts and shutdown). Existing fixture directories are not reusable
as fresh-test inputs: they contain intentionally saved settings and assets.
Live ATS interaction and proxy-chain compatibility have not been validated.

## Experimental picking release blocked: Snowymoon interoperability

The `d639e89` experimental build crashes on the first live touch with the
user's Snowymoon DXGI + ReShade D3D11 chain. Its standalone GPU tests do not
cover that chain and must not be treated as compatibility certification.
After that incident the installed overlay was restored to `dbd4d8e` (SHA-256
`43537C5A7F4D50004F45A0DDD94D4A877C691D1C62882BB83FBF9922FD71ABA8`).
The experimental package is quarantined, not a releasable build.

The saved 2026-10-19 23:25:26 crash is an access violation in the installed
Snowymoon `dxgi.dll` at RVA `0x97bae`. Its current-pixel-shader state at RVA
`0xc76838` equals the native shader captured/restored by the last NavPlus
replay (`material-2.txt`). The crashing function derives metadata from the
shader's vtable minus eight bytes, but receives a system D3D11 vtable instead.
This local evidence narrows the failure to native/proxy shader identity and
state restoration; the exact interoperable replacement path remains unverified.
Do not ask the user to remove Snowymoon/ReShade or ship another GPU replay
change as fixed without a relevant proxy-chain regression and live validation.

Ctrl+F9 opens interaction mode with a compact **Better GPS Menu** header.
Click the header to expand/collapse the existing settings panel; drag it to
move the panel. Position is stored separately in `BetterGPSMenu.ini` as
normalized client coordinates. A Reset button stays at the lower-right corner.
Closing interaction mode, losing focus or entering ATS menus cancels gestures
and restores the previous cursor/mouse capture. A drag never also becomes a tap.
The five category icons are in `material/ui/settings_menu`.

## Interaction and state

- All input is gated by Ctrl+F9 interaction mode, foreground and active gameplay.
  The menu/header/reset take input priority; clicks cannot pass through them.
- A gesture captures one actual device, power generation, vehicle generation
  and page serial. Stale hits, ambiguous devices, page transitions, power-off,
  splashes and unsupported shader/depth paths are ignored.
- Home and Map are explicit destinations, not the keyboard toggle. Home can
  be selected during guidance without cancelling the route; opening Map then
  shows Nav if a route still exists, otherwise Notrip. Repeating a Home swipe
  never launches an app. Vehicle-initialization eligibility is retained.
- Phone bottom-to-top swipe: Home. On Home, Google Maps/Waze icons select only
  that device's saved style/layout and run the existing app-opening animation.
- Auto-unlock defaults to enabled, independently per device (`auto_unlock=1`).
  Disable it in Phone specific to hold the lockscreen until a bottom swipe.
  The original swipe animation runs, then Home. The normal first-start Home
  dwell is retained when pending; otherwise manual unlock selects manual Home.
- Nuvi: the Home View Map image opens Map; the blue Menu button returns Home.
- W900 NavPlus: Home Navigation opens Map; the footer Return button returns Home.
- Crown Sony: the updated Home Map circle opens Map; the central navigation
  skin circle returns Home. Its live clock and splash remain unchanged.
- F150/Bronco are excluded from touch actions and retain their existing pages.

Actions are queued to the existing preparation worker; texture decoding,
profile switching and file saving never run in a mouse/Draw callback. Touch
targets use the saved module placements, selected format, fitted content area
and inverse global zoom. The Sony Home target uses its uniformly fitted image.

## Finding the actual surface

The end marker of each managed native UI tags its own final render target;
independent phone front surfaces tag their private completed drawable. This
does not use shared texture names, display dimensions as identities, vehicle
name guesses or Add Display recognition changes. Whole-resource copies retain
the association. The tag includes the fitted content rectangle so texture UVs
can be converted back into the displayed layout's coordinates.

Native pixel shader creation records reflection plus the direct texture
coordinate/texture-slot association from its bytecode. The picker reconstructs
the **complete input signature**, including unused fields, component types,
registers, masks and interpolation. Its compiled signature is checked against
the original before it can run. Keeping only Position and the sampled TEXCOORD
changes hardware registers and is not a compatible shader replacement.
No TEXCOORD0/slot-zero
assumption is made. Unresolved calculated UVs or multiple different tagged
devices in one material fail closed.

Only while a gesture is pending, a private ID/UV/depth draw reuses the native
vertex/geometry pipeline, viewport and readonly depth/stencil comparison. A
one-pixel scissor limits rasterization. The original native draw always runs;
its targets, shader, constant buffer, blend, depth/stencil, raster/scissors and
OM UAV bindings are restored. Immediate and deferred draw variants are hooked.

Before Present, a private deferred pass copies the final scene depth and
rejects pixels hidden by geometry rendered after the screen as well. Only the
filtered single pixel is copied into a three-entry staging ring and mapped with
`DO_NOT_WAIT`; there is no full-frame CPU readback or synchronous GPU wait.
Generation tokens reject discarded command lists and stale render targets.
Resource/state/shader caches are bounded and released on mode close/device change
or shutdown. Single-sample D32, D32/S8, D24/S8 and D16 paths are supported; unknown paths
remain non-interactive rather than falling back to a guessed screen rectangle.

### Intermediate native UI composition

A completed, already-tagged UI may be rendered into another texture using an
early 2D shader whose bytecode was not observed. For a single tagged input, a
single no-depth target, base mip and a full viewport, a private
replay substitutes a floating-point UV/checker texture **only in the replay**.
It retains the actual native VS/PS, geometry, viewport, sampler and scissor.
The native draw and visible textures are untouched. Render target, input SRV
and blend bindings are restored immediately.

A fresh occlusion query must report actual samples from that replay (an EVENT
query alone can already be signalled without executing a discarded command
list). Query polling uses DONOTFLUSH and expires after 200 ms. A compute pass
checks every output pixel against the identity UV/checker image sampled with
the native sampler, including full-image copies between different resolutions;
only a four-byte verdict is read with DO_NOT_WAIT. The destination association
carries that same-frame proof token. Physical-screen picks wait for a successful
verdict; cropped/flipped/partial/tinted copies fail closed. Matching dimensions
alone never establish a clickable screen. There are at most three proof jobs,
256 lightweight verdicts and one-megapixel probe textures, released on mode close.
There is no new thread, synchronous GPU wait or CPU framebuffer readback.

## Research references

- [PrismTextureStreamer](https://github.com/Baldywaldy09/PrismTextureStreamer)
  demonstrates ATS/ETS2 native screen replacement, not this touch system.
- [SPF Framework](https://github.com/TrackAndTruckDevs/SPF-Framework) provides
  game input and camera integrations. BetterGPS does not add it as a dependency.
- Microsoft's [CopySubresourceRegion rules](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copysubresourceregion)
  require copying the whole depth subresource. Depth stays on the GPU here.
- [DO_NOT_WAIT](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_map_flag)
  lets the readback be skipped when the GPU is busy.
- Microsoft's [shader signatures](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-signatures)
  explain why preserving a TEXCOORD name without its native register/order is
  insufficient for replacing a pixel shader.
- The [Direct3D query specification](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#20.3.2%20HRESULT%20CreateQuery)
  specifies initial signalled query state and asynchronous execution evidence.

## Validation

`OverlayPreviewTest <fresh-output> --touchscreen` exercises real WARP draw hooks
and the production PSSetShaderResources hook (no manual binding-mask injection)
in immediate/deferred contexts, native and substituted screen textures, clearing
stale identities on unrelated textures, TEXCOORD3 and texture slot 5, D32/D24 depth,
occluders before and after the screen draw, state restoration and no picking
outside interaction mode. CPU cases exercise all requested device actions,
active-route Home, idempotent gestures, independent app/profile selection,
module movement, splash rejection, manual unlock and saved menu position/reset.
The existing 15 device/map/power/menu-regression cases must also pass.

The captured ATS seven-field input order (COLOR0, Position, TEXCOORD0/1/2,
TAADATA0/1) is exercised on WARP and NVIDIA hardware, including off-centre UVs.
The former two-field picker fails that fixture. Additional immediate/deferred
tests exercise an unobserved 2D producer followed by the physical 3D consumer,
same-frame proof gating, rejection of same-size cropped copies and discarded
native command lists, an actual GPU-picked bottom-to-top swipe, and byte-exact
preservation of the visible texture and native input/shader state.

These are automated tests, not a claim of in-game visual validation. The first
live run must verify the native material shaders, UV orientation and touch zones
on the user's actual phone, Nuvi, NavPlus and Crown models. `[Touch]` startup and
gesture diagnostics are intended to distinguish an unsupported render path
from a state or hit-zone issue without changing screen recognition.

## Captured ATS depth/copy path and research

The user's live capture reached tagged screens but rejected depth resource
format 19 (`R32G8X24_TYPELESS`). The picker now copies that 64-bit depth/stencil
family without conversion and samples only depth with `R32_FLOAT_X8X24_TYPELESS`;
the typed `D32_FLOAT_S8X24_UINT` resource variant is supported as well. Native
depth/stencil remain read-only to the private replay. Tests use both typed and
typeless resources, check native depth and stencil bytes, and run format 19 on
the hardware driver as well as immediate/deferred WARP contexts. Slot 6 and
TEXCOORD2 reproduce the captured phone material's direct coordinate mapping.

Screen identity now follows complete explicit-box native copies, not only
`CopyResource` and null-box copies. Tracking also occurs before Ctrl+F9 opens,
so a completed cached display can already carry its identity. The mapping is
transferred only for equal-size base levels at zero offsets, never partial,
offset or scaled copies. This changes touch metadata only, not Add Display
recognition or any original copy/render call.

The other captured path lacked pixel-shader metadata. Public Direct3D has no
bytecode getter on an existing `ID3D11PixelShader`. Its report now includes the
target dimensions, depth presence/format and depth enable state so a 2D producer
is not mistaken for the physical 3D screen consumer. No UV fallback is guessed
for an unobserved shader, and this case is not declared fixed merely because
the depth/copy tests pass. The next live run must establish whether it remains
on the physical surface after copy identity is retained.

Primary online sources consulted:

- Microsoft's [depth/stencil guide](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-depth-stencil),
  [format support tables](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/format-support-for-direct3d-11-0-feature-level-hardware)
  and [CopyResource rules](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copyresource)
  describe typed views and same-family copies, without CPU format conversion.
- [ID3D11PixelShader](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nn-d3d11-id3d11pixelshader)
  exposes no shader-specific methods;
  [CreatePixelShader](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createpixelshader)
  receives the original bytecode.
- [3Dmigoto's device implementation](https://github.com/bo3b/3Dmigoto/blob/master/DirectX11/HackerDevice.cpp)
  captures/exports shader bytecode at creation. This is evidence for creation-time
  capture, not retrospective recovery from an arbitrary COM shader object.
- [PrismTextureStreamer](https://github.com/Baldywaldy09/PrismTextureStreamer),
  [PrismMedia](https://github.com/foodmaher/PrismMedia),
  [SPF](https://github.com/TrackAndTruckDevs/SPF-Framework) and
  [RoadBoard](https://www.roadboard.eu/en/documentation.html) were checked for
  existing ATS/ETS2 work. They demonstrate display injection, draw routing,
  input/UI infrastructure or external interactive clients. The material reviewed
  does not establish an already validated in-cab 3D touch picker reusable here.
  None is installed or added as a dependency, and no project code was copied.

## In-game diagnosis (not a touchscreen fix)

The diagnostic build records at most the first 64 pointer presses in a session.
`plugins/BetterGPS/logs/touch/touch.log` follows each gesture ID through pointer
ownership (header/settings/reset/world), final screen bindings, draw, shader
analysis, replay, asynchronous readback, fitted UVs, gesture and action checks.
It distinguishes unobserved shaders, reflection/disassembly failures, unresolved
UVs, ambiguous devices, unsupported depth/targets, GPU wait/errors, pixel misses,
stale reads and precise device-state/button rejections. Ordinary scene draws can
report `no-screen-binding`; a later screen binding/hit for that gesture takes
precedence. A pixel miss does not by itself establish which geometry occluded it.

Up to 12 distinct screen-material reports (`material-N.txt`) contain the device
slot, SRV mask, immediate/deferred context, bytecode fingerprint, analysis counters,
coordinate mappings and the retained native shader disassembly (up to 64 KiB).
Shader evidence is retained during the existing creation-time analysis, with a
16 MiB total session budget. A shader created before the observer cannot be
recovered retrospectively: the report says it was not observed rather than
guessing its texture coordinates. Missing evidence/budget limits are explicit.

Render/input callbacks enqueue bounded, deduplicated records using try-locks;
they do not write files, wait for the GPU or change picking/state decisions.
The existing preparation worker writes the reports, with `[TouchDiag]` summaries
in `game.log.txt`. The log keeps one previous copy; numbered material files are
reused on the next session. Restart ATS to capture another set of 64 presses.
No screenshot, full-frame readback or new background thread is used.

For a useful live capture: open Ctrl+F9, leave its settings panel collapsed or
away from the device, wait until splash/transition finishes, then click one
known Home/Map button and try one phone swipe. Close ATS normally, then inspect
the matching gesture IDs in `touch.log` and their `material-N.txt` reports.
These reports diagnose the real material path; synthetic tests cannot certify
that the user's native material is supported.
