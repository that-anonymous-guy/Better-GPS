# Temporary direct SCS camera test

Purpose: prove one live rear-camera image before continuing the experimental
multi-view/GPU implementation. No C++ build or DLL replacement is involved.

- Original SCS T680 GPS UI, with the Ford-style parking group ID 910 appended.
- Native material `/material/environment/mirror/park_1x1_ui.mat`, top=1 bottom=0.
- Existing native parking locator in each of the three plugin cab PMGs moved
  to the measured rear optical position, facing rearward/down 45 degrees.
- Every original vertex/index and all other locators are unchanged.
- Only the T680 GPS profile is temporarily set `enabled=0`, so the plugin does
  not replace the original SCS UI. This does not turn off the physical screen.
- Stock GPS remains available outside reverse; the parking group is controlled
  by SCS. Test at rest, ignition on, reverse selected, wait two seconds.
- No installed-camera detection, selector, effects, SDK triggers or SRV capture
  are part of this proof. The unfinished implementation is not deployed.

Installed: four assets, 2,325,073 bytes. DLL SHA-256 is verified unchanged.
Transaction backup: `backups/dev-camera-smoke-20261026T214526Z` in the plugin.
Staging, original settings and receipt: `build/camera-integration-20261026/native-smoke`.

After ATS closes, restore the four assets and only the original enabled value:

```
python BetterGPS/Overlay/scripts/camera_native_smoke.py restore
```

Restoration checks asset hashes and preserves other settings edited during the
test. The authoring asset sources and the experimental C++ work are untouched.
User confirmed in ATS: a real rear image appears and matches the physical rear-camera position.
