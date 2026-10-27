# NavPlus camera integration after the successful native rear proof

Use the SCS parking producer (7) and `/material/environment/mirror/park_1x1_ui.mat` directly in a dedicated generated Camera UI. No camera texture interception, GPU readback or CPU image copies. The discarded SRV experiment is preserved locally under `build/camera-integration-20261026/parked-capture`.

The normal managed UI retains its map/primer. The Camera variant removes both, uses the native material under an alpha overlay, vertically flipped and centre cropped to the app body. RGB(10,10,10) covers unavailable/unready sources. Top/bottom chrome and selector remain managed. The native window variant is restored after closing Camera. Existing page timing remains unchanged; the native video itself is an underlay, not part of the saved slide snapshot.

Installed optical tags are queried in the actual tuning scenes and separate hookup array. Front position composes through the bumper; rear follows the actual cab frame. World positions include packed 512-unit sectors before conversion to the exterior cabin frame. One camera producer is repositioned for the selected source; all four FOV values are 100 degrees. Reverse has automatic priority, indicator command edges select sides, and manual choices persist until a new trigger. None of these triggers opens the app.

The backend requires supported executable signatures, its own tagged cab, the known parking-camera vtable and original placeholder pose. It changes only its own producer bit in the native mask, saves/restores poses and FOV, and clears readiness on reset/suspension. Other trucks and mirrors retain their native mask.

Validation: selection/pose math, native scene/hookup composition, native-mask ownership/activation/shutdown, original-pose restoration, Camera layout/UV/source isolation, opaque fallback/transparent ready body, tactile selector and page history are tested offline. The original rear-only SCS proof is confirmed in game. The integrated four-view backend still requires a real T680 game check; offline tests cannot confirm a native engine call's runtime effect.

Remaining: confirm four views in game, then grain/30-percent desaturation/slight blue grading. Those effects are deliberately not included in this direct native integration build.

## World-load crash correction

The first integrated game run crashed in BetterGPSOverlay at RVA 0xC7FBC.
The faulting instruction was MOVAPS [RDI], with RDI ending in 0xA08: readable
native scene memory aligned to 8 bytes, not the 16 bytes declared by our local
PackedPose. Typed whole-pose copies incorrectly allowed aligned SIMD loads.

External poses are now copied as bytes into aligned local storage before the
native compose call. All direct, hookup and recursive attachment-frame copies
also use byte copies. The native-call ABI remains 16-byte aligned. No camera
feature or other application is disabled by this correction.

A new test supplies an actual scene frame at 8 modulo 16. Against the previous
implementation it exits 3221225477 (0xC0000005); with the fix it passes. Previous
mocks used aligned parents and did not expose the issue. Camera, native-fork,
CoPilot and device-page suites pass. A new in-game world load remains to verify.

Evidence: build/dev-runs/20261026T221305Z-5f3a89d1 (reproduced failure),
20261026T221357Z-aba134b2 (passing regressions), and
20261026T221453Z-a540e83f (corrected Overlay build).

## Correct native optical basis and source-switch origin

User confirmed that all four sources were selectable after the crash fix, but
rear framing differed from the earlier native asset-only proof, all views
appeared rolled, Forward faced backward, and the Right feed appeared mirrored.

The previous writer incorrectly treated native offsets 0x480, 0x4a0 and 0x4c0
as three identical input poses. Native initialization at 0x5363ae..0x5364bd
stores RAW locator poses at 0x480/0x4c0, but POSTMULTIPLIES the orientation at
0x4a0 by its SCS optical adjustment. Fixed-camera composition (0x881d00) uses
that adjusted quaternion and a separate cached eye at 0x52c. Replacing all
three poses with the same quaternion discarded the native basis, while leaving
the old cached eye could retain the placeholder/previous source origin.

The writer now derives the adjustment from the saved raw/adjusted quaternions,
applies it on the right of each new raw orientation, updates the cached eye
on every source switch, and restores all touched fields on release. Only the
three input poses are saved/restored; the native parent at 0x4e0 is untouched.
Cam optical positions still follow actual mounted pieces. User yaw/pitch is
set directly in cabin axes, rather than round-tripping a bracket/world rotation.
The rear quaternion is (0,0,cos(pi/8),sin(pi/8)), exactly the provisional PMG
proof. Actual s52m rear origin in the log matches its calibration at
(0,2.708565858,0.533921469); dc and s76h have their own measured origins.
The camera-only material UVs now reverse X once (left=1,right=0) to compensate
the mirror feed; normal navigation UVs are unchanged.

Regression coverage includes all four raw/adjusted pose pairs, postmultiply
order with noncommuting rotations, cached eye update, parent preservation,
exact provisional rear quaternion, and camera-only X flip. Camera, CoPilot,
native-fork and swipe suites pass (20261026T223354Z-7006f3f8). Overlay build:
20261026T223517Z-a3dada14. The resulting four views still need visual game
confirmation; mathematical/layout tests cannot prove their rendered framing.
