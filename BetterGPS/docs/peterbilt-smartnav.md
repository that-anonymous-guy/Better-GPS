# Peterbilt SmartNav and T680 instruction update

## 579-only follow-up

The 579's default instruction module is now **325x153** instead of 250x118
(+30%, integer rounding), retaining its top-left. Its bubble grows to 332x153;
the road starts at x=346 rather than 269. Zoom moves from y=82 to **164**,
below the bubble with an 11px gap. Home, footer and all other devices stay
unchanged, including 389 when selecting a 3by2 layout.

579 has a distinct runtime layout family, still stored in its existing
`_smartnav.cfg` path. `GarminLayout 7` rebases only that family's 3by2 profiles:
instruction x/y and custom scale are retained, road x gains 77 and zoom y gains
82, preserving each saved offset. Current profiles do not migrate twice.
The original wide-format T680/389 layout remains version 6.

## Measured displays

Both Peterbilts use `/ui/gps.sii` and `/vehicle/truck/share/gps.tobj` (native
256x256). Auto-configuration requires an observed, owned built-in layer/window
pair and the exact vehicle ID. The dashboard, phones and DLC windshield GPS
are separate; saved Original/disabled/style choices remain authoritative.

| Truck | GPS PMG piece / material | U bounds | V bounds | Visible texels | UV ratio |
|---|---|---|---|---|---|
| `peterbilt.389` | 14 / 20 | 3.00322914..3.99677086 | 3.20932460..3.79067516 | 254.34668 x 148.82574 | 1.7090234 |
| `peterbilt.579` | 6 / 18 | 3.00639534..3.99360561 | 3.16389394..3.83610606 | 252.72583 x 172.08630 | 1.4685993 |

389 is **4.006% wider in UV aspect** than the T680 (1.643200); 579 is
**1.433% narrower** than W900 (about 1.489948). These are source-UV measurements,
not a claim that logical DDS pixels describe physical mesh proportions.
Read-only extractions are in `build/p389-screen-inspection` and
`build/p579-screen-inspection`.

The supplied nominal 5by3 Peterbilt images are actually **1270x745** (1.704698),
not the T680's 1275x775. 389 therefore uses **900x528** with its own module
defaults, not a stretched T680 canvas. 579 uses the supplied **1200x800** art
in the W900-based **900x600** layout. It is uniformly fitted into its actual
UV exposure, with the small resulting margin retained instead of distortion.
Generated UI and inverse touch mapping share the same measured helper:
389 x=4..1018/y=214..809, 579 x=7..1018/y=175..849 on a 1024x1024 target.

## Device integration and assets

- `material/garmin/kenworth_navplus/`: unchanged Kenworth device files.
- `material/garmin/peterbilt_smartnav/`: the six supplied SmartNav images.
- `material/garmin/Nuvi1440_collab/`: four common Nav/Notrip footers (formerly
  `1440`). Import input follows the user's matching folder names.

Each family uses its own Home/splash/below-frame filename. SmartNav profiles
are `device_N_5by3_smartnav.cfg` or `device_N_3by2_smartnav.cfg`, separate from
Kenworth `_navplus.cfg` and Nuvi `_garmin.cfg`. The runtime family is derived
from vehicle/UI/alias identity; it is not inferred from texture dimensions.
All preferences, map views, module edits, player icons and theme remain per
device. Splash is opaque for five visible seconds, followed by the existing
conditional Home/Notrip/Nav state machine.

389 footer values/LIMIT/frame/cardinal placements follow its shorter image;
579 keeps the 3by2 arrangement. Home's Navigation target is measured in each
supplied image. Return retains its distinct square Nav / rectangular Notrip
targets, transformed with its editable footer and inverse global zoom. Both
Peterbilt dashboards expose ambient widget 1000; the readonly guarded native
reader and game-time AM/PM clock are reused (not static screenshot text).

## Anchored T680 / 389 instructions

The wide-format instruction module grows from **235x111 to 294x139** (+25%,
rounded) at the SAME top-left. Its bubble grows with it (240x111 to 300x139).
The road starts at x=313 instead of 253 so the modules do not overlap; bar
height and the rest of the interface are unchanged. 3by2 defaults stay 250x118.

`GarminLayout 6` applies only to wide profiles. Loading a version-5 T680 file
retains the instruction module's saved x/y and user scale, recomputes its base
size, and adds 60 to the road's saved x. Other module origins are preserved.
The migration is idempotent; missing files and current profiles are not rebased.

Validation: `OverlayPreviewTest <fresh-output> --smartnav`, `--navplus-t680`,
`--navplus`, `--touch-actions`, plus existing device regressions. The production
`TouchPhoneTest` exercises actual WARP copies, projection and depth through both
SmartNav Home buttons and both Return shapes, alongside phones/Nuvi/Sony.
CPU previews are inspected; these are not a substitute for live ATS validation.
