# Source pose before native visibility; rear view 30 degrees

The user confirmed that the chassis reappears on the native rear path inside
the CURRENT four-view application. The camera model and native stream can draw
the truck correctly; the managed mutation path caused the regression. This
comparison changed several managed pose fields together, so it does not prove
which individual renderer exclusion was responsible.

Further verified native detail: mirror source virtual+0x100 dispatches through
0x881b90 to 0x87ffa0. That native update computes source pose at +0x40. Source
frustum construction reads that pose, and native export 0x881ba0 copies it then
applies a fixed quaternion right-product (0,0,0,-1). The exact export mapping was
computed from its instruction stream (export-quaternion-map.json locally):
(w,x,y,z) -> (z,-y,x,-w), up to the engine's cos(pi/2) rounding.

The previous managed path changed raw calibrated inputs and later rewrote the
exported renderer quaternion while source state remained different. Both sets
of writes are now removed. A guarded post-update hook sets the owned camera's
full source world pose (+0x40..+0x5f) for Forward/Left/Right, before native
frustum/export consumption. Calibration inputs (+0x480 onward), eye cache,
parent transforms and projection are untouched. The exported render camera is
read-only again; there is no extra invalidation callback or final pose rewrite.
Rear remains entirely native. Only the FOV field is restored on release, since
this implementation no longer owns the other native input fields.

Readiness for managed sources is published only when the source update was
actually applied, with matching vehicle epoch and selection. Other mirrors,
other trucks and closed Camera apps retain their original update. The existing
material U/V basis is retained, and native export performs its own half turn.
Audit includes sourceUpdates and exportPoseWrites=0 in addition to ACTUAL/WANTED.
The native rear path remains explicit in the log.

Latest user request changes Rear downward view from45 to30 degrees. All three
cam_parkbck PMG rotations now use (0,0,cos15,sin15); geometry, optical origins,
other locators and physical hardware models are byte-identical. The camera
angle table, calibration review and three hardware hashes are updated together.
No old DLL or managed-display disabling is used.

Verification: all source bytes outside the optical pose remain unchanged;
stale epochs and native Rear cannot be patched; source/native-export/UI axes
agree; native scene lookup/alignment, CoPilot and swipes pass in
20261026T233913Z-60dde1ce. CameraStateTest passes 28 checks. Three PMGs are
independently verified at30 degrees with no other changed bytes, and hardware
hashes match. Overlay build:20261026T234150Z-315b9841. A new in-game check of
geometry visibility for the other three views remains required.
