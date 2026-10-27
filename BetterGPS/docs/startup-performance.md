# Plugin initialization performance

This optimization changes initialization work only; no device, overlay,
navigation protocol, keyboard shortcut, page rule, shader, timing or palette
is disabled or deferred. In particular, all anti-flash guards remain ready
before the native UI is first used.

## Measured bottleneck

The existing ATS log showed cluster loading completing in about 65 ms, while
graphics/input hook installation occupied seconds. A dedicated local benchmark
using copies of the installed settings measured:

| Phase | Before (two runs) | After (two runs) |
| --- | --- | --- |
| Graphics/input hooks | 2850–2975 ms | 544–755 ms |
| Managed device templates | 564–566 ms | 278–347 ms |
| Cluster asset loading | 65–80 ms | 66–69 ms |

These are local Windows/D3D benchmark measurements, not a promise that the
entire ATS pause will take this long. DLL loading, other plugins and the game
have their own costs. Real ATS timings must be checked on the next launch.

## Second pass: complete graphics/input cohort and faster map candidates

After the detailed trace, a fresh same-machine before/after benchmark measured
graphics/input setup at 705 ms before and 470 ms after this pass. Resolving
map reflection from the actual inert, relocated ATS executable took 100 ms
before and 66 ms after. These remain offline measurements, not a measurement
of the full in-game freeze. Driver/device creation costs remain synchronous.

- Graphics discovery now constructs all touch, core, deferred-context,
  cursor/DirectInput and Present hooks, then activates exactly that completed
  cohort once, before releasing the discovery objects or returning. All
  original pointers are populated before any callback in the cohort runs.
- Cursor/DirectInput hooks also batch their activation when installed alone.
  Standalone touch and input entry points still return with active hooks;
  only an explicit owning installer can collect prepared hooks. Native map
  lifecycle/player installers and runtime behavior are not deferred.
- The map resolver uses a bounded `memchr` candidate prefilter instead of
  testing every byte for two instruction patterns. Full pattern, reflection,
  ownership, uniqueness, executable-section and ABI checks are unchanged.
  Searches still include the last legal candidate and overlapping candidates.
- The complete map-layout fingerprint (all resolved RVA pointers, property
  offsets and cache fields) is identical before/after on the installed ATS
  image. The 403 generated files/96,089,485 bytes are SHA-256 identical too.

`OpcodeScanTest` compares 4500 boundary/missing/overlap cases against the
original bytewise iteration and checks a no-access next page. Menu regression
coverage includes five standalone ANSI/Wide installation cycles and three
combined cursor/input installation cycles. Existing failure rollback and
unrelated-hook isolation tests remain in place. No persistent cache, worker,
reduced texture quality or skipped device/feature is introduced.

## Changes

- MinHook already supports queued activation. New hooks of a completely
  constructed subsystem are queued, then enabled in one thread-freeze pass
  instead of suspending/resuming every game thread for each individual hook.
- Batches cover native geometry/touch, immediate and deferred D3D bindings,
  native map lifecycle, private map routing, and player-material hooks.
- No global `MH_ALL_HOOKS` enable: unrelated hooks are not enabled. Hook
  addresses, callback functions, original pointers and ABI guards are unchanged.
- Queue failures cancel the pending cohort. Partial apply failures queue its
  disable/rollback; existing caller cleanup remains in place.
- Standalone additions through `touch_add_hook` keep immediate activation.
  Only `touch_hooks_install` defers its new cohort, with a thread-local RAII
  scope so errors cannot leave the mode active.
- Generated marker DDS/TObj readers read the 128/48-byte headers they use,
  rather than repeatedly reading and discarding a megabyte of source pixels.
  The generated directory is still fully regenerated as before: no stale
  cache, changed invalidation policy or deferred file generation is introduced.

The before/after snapshots contain exactly the same 403 generated files and
96,089,485 bytes; SHA-256 comparisons of every file report zero differences.
The timings/observations refer to the installed configuration at test time.

## Diagnostics and verification

ATS now logs `[StartupTiming] phase=... ms=... total_ms=...` for settings,
cluster assets, native address validation, generated displays, graphics/input
hooks and native-map/palette/VFS setup. This is initialization-only logging.

### Detailed trace before the first Probe message

`[StartupTrace]` starts at the loader's SDK callback entry, before either
internal DLL is loaded. Paired `event=begin` / `event=end` lines cover
`load-probe-dll`, `probe-sdk-init`, `load-overlay-dll`, `overlay-sdk-init`.
Inside the Probe they separate object construction, opening its log,
diagnostics shared-memory creation, config loading, signature scans,
SDK subscriptions, capture startup and sampling-thread creation.
The overlay additionally separates its diagnostics, individual cluster
assets, car settings/first frames, generated displays, managed templates,
player materials, graphics/input hooks, player observer and map/VFS setup.

Each line contains `wall_ms`, `cpu_ms` (this thread only), `elapsed_ms`,
the shared monotonic `qpc_ms` clock and the thread ID. QPC avoids relying
on potentially cached game-log timestamps. Large wall time with little
thread CPU indicates waiting/descheduling rather than busy work on that
thread; it does NOT on its own identify disk, antivirus or another cause.
Nested parent and child durations must not be added together.
`event=leave` means the callback returned/unwound, not necessarily success;
existing SDK error logs retain that information.

The interval BEFORE `component=loader event=enter` is explicitly outside
the SDK callback measurement. It must not be called slow Probe code just
because it lies between ATS's `loading` line and a later plugin message.
DLL mapping before SDK entry and any warning-dialog delay are not separated
by this trace. No logging or new work is added inside `DllMain`.

These diagnostics use the supplied SDK logger only, on the initializing
thread: no extra files, worker threads, sleeps, deferred initialization or
per-frame tracing. Windows last-error is preserved around diagnostic calls.
`StartupTraceTest` and `LoaderTest` verify a known 40ms delay and both
normal and failing loader lifecycles.

`OverlayPreviewTest <fresh-output> --startup-performance` measures the real
generation and hook paths in a separate process, using copied settings and
without modifying the installed plugin. `HookBatchTest` checks group isolation,
single-apply semantics, queue failure and partial-apply rollback.

Regression coverage includes phone native capture/depth/swipes, independent
apps on multiple devices, manual unlock, menus and shortcuts, module placement,
camera settings, map palettes/player icons, all integrated GPS families,
anti-flash/cold binds, pause/resume and shutdown. Final live ATS verification
is still required; offline tests cannot certify every third-party interaction.

Two legacy preview scenarios are not counted as passes: `display-assignments`
still references the removed `BetterGPSOverlay/` source directory, and
`f150-info` assumes that OFF immediately follows an imported still in a sorted
list (the existing `splash_info` entry intervenes). Neither production path
was changed to satisfy these obsolete fixtures. Current phone-control,
managed-device, installed-layout, F150 startup/horizontal and anti-flash
fixtures pass; generated-display equality is also checked separately.
