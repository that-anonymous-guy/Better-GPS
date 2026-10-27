# Nuvi 465T native-display publication

## Ignition crash regression, 2026-10-20

The user's new session provides direct evidence absent from the earlier logs:

- `00:01:18.872 [DeviceRoute] slot=21 Nuvi owned UI job submitted`:
  ATS already queues the replacement UI on the Peterbilt 579.
- `00:01:19.074 Ignition telemetry state=1`.
- `00:01:19.105 [DeviceQueue] slot=21 output=35390 key=54`:
  the added manual export submits that same accessory UI again.
- At the same timestamp ATS aborts with
  `rendergraph_image_t: Index outside array boundaries: 4294967295 >= 60`.
  The latest crash record is a noncontinuable engine exception with the native
  queue and BetterGPS hook in the stack, not a missing DDS or a black splash.

The manual-export change was incorrect: an accessory's owner already submits
its replacement UI, and exporting it from inside the consumer queue can insert
its own output into its dependencies. `managed_native_export` is phone-only
again. Nuvi and integrated dashboards keep their native owner-driven UI jobs;
their private map producers remain unchanged. No exception suppression, shader
replay, scanner reset or asset/profile replacement is involved.

The exact-retained-pair fallback remains restricted to the positively identified
Nuvi UI and TOBJ, enabled device, current vehicle and current replacement owner.
It only substitutes the pointers of an existing native job; it never adds a
second UI job. Current owned pairs pass unchanged.

## Regression coverage and limits

`OverlayPreviewTest <fresh-output> --nuvi-installed` copies the actual installed
profiles into an isolated output directory and reads installed assets. It
checks high-slot marker binding, splash readiness and nonempty Home/Notrip/Nav
images without modifying installed inputs.

The powered native-queue regression enables the real export callback inside
`native_fork_queue`, unlike the earlier forwarding test. Before this fix it
fails with `Nuvi native consumer submitted its owned UI a second time during
export`. After the fix it verifies exactly one engine submission and no change
to the consumer's dependency table across three frames, as well as no explicit
Nuvi export to unrelated consumers. It also retains the separate independent
phone producer's manual export and once-per-frame deduplication checks.

Exact pair routing, argument preservation and disabled/Original/recycled/mixed
pair rejection remain tested. The engine endpoint is stubbed: these assertions
test our graph construction policy, not Prism3D's internal renderer. A live
ignition run is still required to confirm crash-free BetterGPS substitution.

Bounded per-activation traces:

- `[DeviceRoute] slot=N Nuvi owned UI job submitted`: native consumer uses it.
- `[DeviceRoute] slot=N exact cached original Nuvi job redirected ...`: exact
  retained-pair fallback actually fired (not observed in the crash session).
- `[DeviceQueue]` should now only refer to independent phone UI producers,
  never a Nuvi accessory.

The original report that a Nuvi showed ATS's GPS was not reproduced by CPU/WARP
tests. Do not claim that separate visual issue is certified fixed by removing
this crash regression.
