# Kenworth W900 integrated GPS screen

Read-only inspection, 2026-10-18, installed ATS 1.61.3.18s assets with the
last session's mounted DLC/mod precedence. This is the integrated centre GPS,
not the Cabin Accessories windshield Garmin and not the instrument cluster.

## Source dimensions and visible aspect

- `/def/vehicle/truck/kenworth.w900/interior/computers.sui` selects
  `/ui/gps.sii` for the GPS. The latter has a **256 x 256** root layout.
- `/vehicle/truck/share/gps.tobj` resolves to a **256 x 256** DDS.
- The daycab PMD references `/automat/d8/d8187d0c1f48c659.mat`, which uses
  that GPS texture. The PMG screen is piece 18, material 18, 16 vertices.
- Its U bounds are `3.0064678192 .. 3.9935319424`, and V bounds are
  `3.1687588692 .. 3.8312411308`. Repeated texture coordinates cover
  `0.9870641232 x 0.6624822617` of the square texture.
- The visible region is therefore about **252.69 x 169.60 source texels**,
  with aspect **1.489948:1**, approximately **3:2**. Mesh edge lengths agree:
  about 159.14 mm wide by 106.81 mm high. Rounded screen corners apply.
- The `sl_72` and `sl_86` sleeper interiors have exactly the same GPS UV
  extents and visible aspect (pieces 20 and 18 respectively). Their interior
  definitions also include the same `computers.sui`.

The 256-square source is not a square physical screen. A future theme must
account for these mesh UVs and keep its visible modules within the actual
screen region. Do not stretch an entire square UI onto it or confuse DDS
dimensions with a separately observed runtime render-target capture.

The instrument cluster is separate: `computers.sui` declares
`dashboard_drawable_size: (256, 128)` and `/ui/dashboard/kenworth_w900.sii`
uses a 400 x 230 coordinate layout. Those are not the centre GPS dimensions.

## Reproduction

The existing AssetInspector can extract the virtual paths above plus:

- `/vehicle/truck/kenworth_w900/interior/model/daycab.pmd`
- `/vehicle/truck/kenworth_w900/interior/model/daycab.pmg`
- `--texture=/vehicle/truck/share/gps.tobj`

Local inspection output: `build/w900-screen-inspection/` (not deployed).
The PMG is version `0x15`; the existing `build_phone_surfaces.py` documents
the piece/vertex/UV offsets used for the read-only measurement.

SHA-256 of inspected inputs:

| Input | SHA-256 |
| --- | --- |
| GPS UI | `27B321372ED6849B0E90969ADA83E62D9CE8A6CE639CBFAEA09E7398C9465BAE` |
| computers.sui | `CBA96DC14602708C5E9817BB5512B65DC3A9E38892F1ECABEF55629B4C48E1B8` |
| daycab.pmg | `C66AF5B4B3EB0C7960BCCC655664B7DD5C443B63B240846B7A7423416A7EFE17` |

## NavPlus integration

The observed built-in W900 candidate (`/ui/gps.sii` plus
`/vehicle/truck/share/gps.tobj` and separate native window/layer slots) receives
its own managed profile, Garmin map palette and truck marker. The DLC windshield
Garmin and instrument cluster do not match this identity. Existing saved
Original/disabled/style choices take precedence over automatic defaults.

The 900x600 NavPlus presentation is fitted inside the measured UV region of
the supersampled native target, not into the full square. Supplied artwork is
under `BetterGPS/Overlay/BetterGPS_assets/material/garmin/kenworth_navplus/`.
Shared 1440 map footers live separately in `material/garmin/1440/` with
`_anyres_` filenames. Device backgrounds/frames/splashes use `_3by2` or `_5by3`.
`scripts/import_navplus_assets.py` preserves every pixel; it stores the two
1200x150 BC7 footers and 1275x775 T680 art as uncompressed DDS because their
dimensions are not multiples of four. The source files outside the repository
are not modified. Updated W900 Home art retains its original placements and
Navigation hitbox.

Home uses the complete supplied background, a 12-hour clock, Fahrenheit ambient
temperature and cardinal direction. Navigation has independently editable
header/instructions/road/zoom/footer/values/LIMIT/frame/compass slots. "Arrive In"
shows remaining travel time. NavPlus profiles use separate `_navplus.cfg` files;
Nuvi 465T profiles keep `_garmin.cfg`. Google/Waze, formats, skins, startup,
player icon, cameras and global zoom remain per-device settings.

The `garmin/kenworth_navplus` splash remains opaque for five seconds starting
with its first valid GPU upload. `garmin/nuvi465t` remains three seconds, even on
the same vehicle. Imported custom splashes retain their existing behavior.

Ambient temperature is read-only from the native dashboard's text-common group
0 (widget ID 1000, value formatted `%d|F` or `%d|C`). It does not use engine oil,
coolant or brake temperature. Source-layout instruction guards are checked
against the installed ATS executable. Invalid/unavailable data displays `--f`;
unsupported executable layouts disable this reader rather than guessing.

NavPlus footer typography uses `h:mm` with larger/heavier hours and smaller
minutes on a shared baseline. The last speed digit remains fixed under Speed,
with vertical `m`/`h` units beside it. Home shows the lighter clock and `85f`
style notation (still Fahrenheit); missing temperature displays `--f`.

The primary arrival and speed digits share the same 62-pixel heavy Arial font;
the speed text reserves its full line height to prevent automatic shrinking.
Minutes and the closer vertical m/h units are bold. The LIMIT module is 78x100
(+20% from 65x83), centered horizontally at the previous location. Its bottom
is aligned to the actual rendered `m` in the vertical speed units (canvas y=495),
not to a reference point in the footer artwork. Its night version is an exact
RGB inversion. The home clock and its separator move right together by 3 pixels;
the temperature stays fixed.
NavPlus `GarminLayout 5` migrates only this module's old origin, preserving saved
custom offsets/scales and all other devices/modules. Existing configuration
files are left untouched during DLL deployment and migrate when loaded.
Both version 3 and version 4 profiles are rebased directly to the new origin;
current profiles and missing files are not translated again.

Validation: `OverlayPreviewTest <output> --navplus`, plus existing Garmin,
managed-display/phone, native map/player, Waze, F150, Bronco and installed-layout
fixtures. These automated checks and CPU previews do not replace an in-game
visual validation on the user's truck.
