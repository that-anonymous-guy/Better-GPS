# Startup anti-flash audit

This pass covers the already implemented truck displays, without changing
their artwork, video lengths, Home timings, final fades or direct cuts.

- Mack Anthem/Pinnacle: cluster and original GPS guards now enter the same
  cold-SRV registration and first synchronous signature check as every other
  startup marker. The previously handwritten registration list omitted both.
- Kenworth T680 2022: existing cluster guard retained; a transparent additive
  marker covers the original infotainment producer before generated UI takeover.
- Kenworth W900/T680 and Peterbilt 389/579: original GPS and mapped aliases
  receive the same additive guard. Their clusters have no BetterGPS intro and
  remain native. Both original and mapped UI identities are recognized.
- Volvo VNL14/VNL18/VNR: existing dedicated protections retained and included
  in the common first-view registry. The common `/ui/gps.sii` fallback also
  respects VNL14/Mack startup shades if that path is used.
- F150/Bronco: existing startup/handoff mechanisms retained, tested separately.
- Nuvi 465T: existing managed first-visible-upload protection and native producer
  routing retained; no new private export or startup animation is introduced.

The guard is RGB0 on classic Garmin, RGB10 on CoPilot/Mack/Volvo pre-contact,
and RGB245 on the VNL14 GPS after contact. The native marker artwork stays
transparent if the plugin is absent, the family is not supported, or the user
selects Original/disables BetterGPS. It never paints a new intro on a cluster
which did not previously have one.

Automatic truck power waits for a known ignition=ON value instead of treating
an unavailable SDK value as ON; manual/always-on preferences are preserved.
Movies still start on their first valid visible GPU upload, not on detection.

`all_bridges()` is now the sole SRV-registration source. `is_cluster_bridge()`
covers all startup markers for synchronous first-bind identification. Readback
is limited to the four sentinel pixels once per previously unidentified marker;
verified paths do not add a per-frame readback or a new worker/timer.

Importer: `BetterGPS/Overlay/tools/add_truck_boot_guards.py`. It preserves the
native UI tree byte-for-byte after removal of its one added child, validates
SCS unit token lengths, and crops the sentinel pixels out of the native artwork.

Regression entry: `OverlayPreviewTest <output> --startup-guards`; covers cold
SRV creation, immediate/deferred rendering, streamed first bind, exact shades,
unknown ignition, disabled/Original and wrong-vehicle isolation. Existing Mack,
T680, Volvo and Ford fixtures continue to check the movie/fade/cut handoffs.
Offline tests do not replace final verification in ATS.

## Infotainment handoff and saved-profile follow-up

Native-producer protection alone did not cover the generated overlay's first
paused/loading binding. That SRV may not have a normal view-classification tag
yet. Generated infotainment overlays now initialize their opaque startup shade
at SRV creation and use the already signature-verified COM texture identity
on that first cached bind. This is not a dimension-only screen guess.

Every uploaded managed image carries the session and player-initialization
epoch used when composing it, in addition to ignition, phase and power serial.
Both active and cached/paused binds validate this identity. An old navigation
image therefore cannot authorize a new boot before the worker samples power,
nor in the small interval when the worker has advanced its power state but
has not uploaded the corresponding image. Missing/stale pixels remain on the
appropriate opaque guard until the first valid image of the current boot.
Valid same-boot paused navigation, Original and global disable remain intact.
No new per-frame disk access, GPU readback, timer or worker is added.

The expanded `--startup-guards` fixture renders and reads back 10 generated
infotainments: W900, T680, T680 2022, Peterbilt 389/579, Mack Anthem/Pinnacle,
Volvo VNL 2014/VNL 2018/VNR. It checks immediate/deferred contexts, cold active
and paused binds, old actor/session images, same-ignition transitions, stale
uploads, fresh startup, valid paused navigation and Original/global disable.
The existing 16 native/cluster guard cases are retained without changing
cluster artwork, durations, fades or cuts.

Saved TomTom profiles were also incorrectly accepted by the generic Garmin
layout migration because they have a nonzero `navplus` family tag. Its 5:3
road-origin adjustment added 60 pixels to the left controls, unlike an unsaved
default profile. TomTom (Mack/Volvo) and Dynafleet are now explicitly excluded
from Garmin migrations. Saved coordinates/scales are retained exactly; no
user profile is deleted, rebased or overwritten by the installer.

### Observed ATS session, 22 October 2026, 17:42

The pre-fix session log was copied to
`build/infotainment-audit-20261022/game-before.log` before further testing.
It records nine player-initialization generations and integrated GPS adoption
for W900, T680, T680 2022, Peterbilt 389/579, Mack Anthem/Pinnacle and Volvo
VNL 2014/VNL 2018. The VNR was not loaded in that session; its coverage here
is automated only. Observed windshield Nuvi and cabin-phone instances remain
separate device slots from the integrated screens.

Measured loader SDK initialization is 975.331 ms (968.750 ms thread CPU),
against 1382.472 ms in the preceding instrumented session. Graphics/input
setup is about 141 ms, including one 51.915 ms cohort activation. The log
also has a 1.929 s interval between ATS's loader line and SDK callback entry;
that interval is not assigned to Probe code or to measured hook installation.

No BetterGPS error or failed/unavailable initialization is reported in this
session, and shutdown reaches `unloaded 'BetterGPS'`. This does not prove
visual correctness: the reported misplaced controls and startup flashes are
covered by the code fixes and new GPU/readback tests above.
Separate errors concern missing Workshop package `2941313000`, an empty
`/unit/hookup/config/.sii` reference, and the Chevrolet Tahoe mod's malformed
`/tahoe_2007/upg/wheel/empty.pmg` geometry. Those files are outside this fix
and were not modified.
