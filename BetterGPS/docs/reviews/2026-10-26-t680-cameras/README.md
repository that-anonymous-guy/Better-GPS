# T680 NG camera integration — preparation and calibration review

Status: **VFS mounts and a read-only installed-optics observer installed for an in-game check; native video backend not wired yet**.
The two PNGs in this directory are a review of actual model geometry and view
axes, not screenshots of a working in-game video feed.

## User rules (override generic DEA600 manual defaults)

- Only the Camera application may show or activate automatic camera views.
- On entry: Forward if fitted, otherwise RGB(10,10,10); no fallback to another
  fitted camera. An already active reverse/indicator command selects its view.
- Reverse gear selects Rear, with priority over side indicators.
- Left/right indicator command selects the corresponding side. Read the command
  state, not the blinking lamp phase. Both indicators together select Forward
  unless Reverse is engaged.
- A manual selection of a fitted camera persists while commands remain unchanged.
  The next change of an automatic command reevaluates selection. Releasing a
  command returns to the remaining active command, or Forward.
- A requested but absent camera stays black; it never shows a different camera.
- Closing the application disables playback. A new vehicle epoch clears state.
- User calibration: Forward 5 degrees down; Rear 45 degrees down; Left/Right
  35 degrees from rear toward the exterior, mirrored, zero vertical pitch.
- User-selected horizontal FOV: 100 degrees for all four cameras.
- Future video output: one low-resolution native producer, 30 percent
  desaturation, slight blue tint and subtle grain; no CPU frame readback.

`CameraStateTest` exercises 24 selection/app-scope scenarios. It does not test
native renderer ABI, hardware detection or in-game playback.

## Actual hardware and optical positions

Source remains the user's `BetterGPS_PP804395_T680NG.zip`. The import receipt
records the ZIP hash and every copied file hash. Cameras remain optional parts.
No truck default or accessory auto-selection is added.

The optical point is measured on the supplied camera meshes. Non-visible PMG
locators identify installed physical cameras, rather than just loaded assets:
`bgps_v_f`, `bgps_v_l`, `bgps_v_r`, `bgps_v_b`.
The copied PMGs preserve vertex/index pools, original materials and locators.
Cabins also have a frame marker and a proposed `cam_parkbck` native producer.
These files are deployed for the read-only check. Mounting calibration is approved with all four FOV at 100 degrees.

- Rear follows the actual Day Cab / Sleeper 52 / Sleeper 76 attachment.
- Front follows the chassis bumper attachment and its `slot_0` camera hookup.
  The calibration receipt records 64 bumper/chassis combinations, not one
  fixed truck-space coordinate.
- Side optical origins follow the corresponding actual camera attachments.
- The review renders assemble actual SCS cab/chassis/hood/bumper/mirror meshes.
  Gray materials are illustrative; they are not a truck paint modification.
- Coordinates on the boards are in the selected chassis frame, not GPS/world
  coordinates or a claim about ground clearance.

## Reproduction

From the repository root, using Blender 4.4:

```
blender --background --python BetterGPS/Overlay/scripts/render_t680_camera_review.py -- --stock build/camera-audit-20261025/t680-all/extracted/vehicle/truck/kenworth_t680_2022 --hardware BetterGPS/Overlay/BetterGPS_assets/camera-hardware --output build/camera-integration-20261026/review
python BetterGPS/Overlay/scripts/compose_t680_camera_review.py --renders build/camera-integration-20261026/review --output docs/reviews/2026-10-26-t680-cameras
cmake --build build --config Release --target CameraStateTest
build/BetterGPS/Overlay/Release/CameraStateTest.exe
```

The review Blender scene is in the render output directory. The source SCS
truck extraction is local audit material, not distributed as a new truck mod.

## Remaining integration

Exact VFS mounts and read-only installed-locator observation are now wired.
Confirm the live `[CameraProbe]` positions on a truck with the front camera
installed before enabling video. Remaining: telemetry commands and Camera UI, then the guarded
native parking producer and GPU composition. Verify the live view in ATS before
claiming playback works. Do not substitute a cropped main camera or map image.
Existing CoPilot navigation, touch, alerts and other applications must keep
working. The read-only diagnostic build and camera assets were installed incrementally; no full package or native video activation was performed.

## Front origin verification after user feedback

The origin is chained through chassis `bumper` -> bumper `slot_0` -> actual
installed front-camera `bgps_v_f` optical tag, not through the cabin. Offline
ray checks of the eight physical bumper meshes found no solid wall along
the forward optical axis. Live native transforms still need verification
when the backend is connected; this offline result is not an in-game test.

The read-only observer runs on the existing native dashboard frame thread at
most once per second. It calls the same recursive locator-query interface and
pose composition helper used by native mirror discovery, with executable
signature/readability/finite-pose guards. It never writes native poses or
render-camera fields. Availability/origin diagnostics are logged only when
the installed camera mask changes. Other trucks are excluded.

The front FOV geometry check now covers 4,712 rays across eight bumper models
with a 100-degree horizontal / +/-32-degree vertical test envelope. None hit
a bumper surface in the first two metres. All 64 chassis/bumper attachment
compositions match the prepared optical origins. See `front-optics-check.json`.

Diagnostic deployment: 59 explicit files, 29,341,647 bytes including the DLL;
verified by workflow run `20261026T202254Z-b94ac916`. Backup is retained under
the installed plugin `backups/dev-20261026T202254Z-b94ac916`. Only the
requested camera icon assets were explicitly allowed to replace customization.

## Live locator-query correction

The first game run loaded all 29 VFS directories and offered the front-camera
accessory, but the observer reported mask 0. It had used only the recursive
truck lookup. Native mirror discovery first queries each directly attached
scene through helper 0x15e4de0, then uses the recursive fallback. The observer
now follows that order, checks scene-array bounds, and reports lookup failure
stages plus stock-mirror and plugin-cabin reference checks. Six regression
assertions cover direct-only locators, recursive fallback, unreadable trucks,
invalid poses/counts and invalid vtables. These are mocked lookup-path checks,
not proof of an in-game camera feed.

Second diagnostic DLL installed in workflow run
`20261026T203519Z-627bae43`; only the DLL changed. The first game log is preserved
locally as `build/camera-integration-20261026/probe-first-session.log`.
