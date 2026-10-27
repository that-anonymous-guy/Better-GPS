# Final-render camera pose audit

The 3cfd034 build improved horizontal mirroring, but user screenshots still
showed upside-down Forward, tilted side views and Rear looking upward. The
prior input-pose tests did not validate SCS's final render transformation.
This diagnostic build deliberately keeps those views unchanged for measurement.

A guarded hook observes SCS mirror camera export at RVA 0x881ba0 AFTER the
original finishes. Only the owned active parking producer is sampled. Its
final renderer pose is copied from output+0x24 with unaligned-safe memcpy.
The callback preserves the original return value and never writes renderer,
camera or scene data. Other cameras/mirrors are ignored.

Each [CameraAudit] ACTUAL/WANTED pair in game.log.txt includes the camera name,
epoch, cab-relative origin/yaw/down/roll, world direction vectors and positional
and angular errors. ACTUAL also records the exported world quaternion. WANTED
is the mounted optic with requested cab angles, composed through SCS's own
native pose function. Requested cab yaw/down/roll: Forward 0/5/0, Left145/0/0,
Right215/0/0, Reverse180/45/0, degrees. The existing UV X correction is retained.

A pair is emitted once per second while the app is active, and immediately on
source changes. Disable dynamically using the plugin-root BetterGPSCamera.ini:

    [Camera]
    PoseAudit=0

The absent-file default is enabled (1); the setting is re-read at most once
per second while this camera is rendered. There is no image readback or file
log on every frame. Test: select each of the four sources for two seconds, then
inspect ACTUAL versus WANTED. Angles remain unresolved until these game values
are captured; passing mock tests do not establish a corrected rendered view.

Offline checks cover yaw/pitch/roll extraction including an inverted view,
rate limiting, source-change logging, disabling, unrelated-camera isolation,
and byte-for-byte preservation of the renderer output. Regressions and touch
passed in 20261026T225017Z-1ef12859. Build:20261026T224859Z-a4c4aa91.

## Correction based on the captured four-view audit

Captured game measurements showed correct optical origins (less than 0.0001 m
position error) but wrong FINAL render rotations:

| View | Native yaw | Native down | Native raw roll | Desired cab yaw/down/roll |
| --- | --- | --- | --- | --- |
| Forward | 182.82 | 25.48 | 10.52 | 0 / 5 / 0 |
| Left | 84.24 | 29.63 | 162.37 | 145 / 0 / 0 |
| Right | 305.86 | 19.52 | -160.19 | 215 / 0 / 0 |
| Reverse | 239.31 | -66.90 | 158.14 | 180 / 45 / 0 |

Evidence is preserved locally in build/camera-integration-20261026/measured-poses-2253.log.

The final owned-camera export now replaces only output quaternion bytes
0x34..0x43 AFTER SCS fitting. Position, projection/FOV and unrelated fields are
retained. It invokes the same output vtable+0x30 invalidation callback that SCS
itself uses after these writes, to rebuild the rendered camera matrix. Both
source ownership/epoch and executable callback/writable output are guarded.
Other cameras return immediately through the original export.

The material reverses U and V: the renderer orientation includes a camera-local
180-degree Z basis conversion, so the displayed image retains the requested
up/right axes. The audit now distinguishes SCS_BEFORE (including raw roll),
ACTUAL_DISPLAY (computed from read-back renderer output plus the UV basis),
rawRenderQ, and WANTED. Diagnostic disabling affects logs only, not correction.
No constant angle offsets fitted to these four examples are applied.

Tests replay the four recorded erroneous quaternions through the final output
layout, verify matrix invalidation, verify no bytes outside the orientation
change (including packed position sectors), and verify the complete renderer
plus UV right/up basis. Camera/native/swipe/touch pass in
20261026T225950Z-bea7433c; grouped build 20261026T225825Z-63086cac.
A new visual game check and post-correction audit are still required.
