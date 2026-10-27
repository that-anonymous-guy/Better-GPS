# Kenworth T680 classic integrated GPS screen

Read-only inspection, 2026-10-19, using the installed game's mounted assets
and the last session's DLC/mod precedence. The initial inspection did not
change the game; the subsequent NavPlus integration is documented below.

The game log identifies `vehicle.kenworth.t680`, not `kenworth.t680_2022`.
The last selected interior is `/def/vehicle/truck/kenworth.t680/interior/cab_a_n_g.sii`
(Vantage gray, model `/vehicle/truck/kenworth_t680/interior/model.pmd`, variant
`cab_a_n`). Its `computers.sui` selects `/ui/gps.sii` for the centre GPS.

## GPS source and visible dimensions

- `/ui/gps.sii`: 256 x 256 root layout.
- `/vehicle/truck/share/gps.tobj`: 256 x 256 source DDS.
- GPS material: `/automat/d8/d8187d0c1f48c659.mat`, material index 16.
- Interior PMG version `0x15`, GPS piece 17, four vertices.
- U bounds: `3.0013706684 .. 3.9986293316`.
- V bounds: `3.1965498924 .. 3.8034501076`.
- UV span: `0.9972586632 x 0.6069002151`.
- Visible source region: approximately **255.30 x 155.37 texels**.
- Visible aspect: **1.643200:1** (not square and not exactly 3:2).
- Mesh edges are about 154.70 mm wide by 86.59 mm high. The physical mesh
  aspect is about 1.787:1 (close to 16:9): its native UV mapping is not physically isotropic.
  A future theme must distinguish source-UV bounds from physical screen aspect
  when preserving text/icon proportions; the DDS dimensions alone are not enough.

These are the native asset dimensions and UV measurements, not an assertion
about the current runtime GPU render target or the resolution of a future
BetterGPS supersampled presentation.

## Instrument cluster is separate

`/ui/dashboard/kenworth_t680.sii` uses an 800 x 800 root layout. Its first
background references `/material/ui/dashboard/kenworth_t680/display.mat`,
whose source DDS is 256 x 256. Interior piece 10, material 17, instead uses
`/vehicle/truck/share/dashboard.tobj`. Its UV span is approximately
`0.9850478172 x 0.7592456341`. None of these cluster dimensions should be used
for the centre GPS.

## Reproduction

Use the existing AssetInspector with the paths above and:

- `/vehicle/truck/kenworth_t680/interior/model.pmg`
- `--texture=/vehicle/truck/share/gps.tobj`
- `--texture=/material/ui/dashboard/kenworth_t680/display.tobj`

Local extraction: `build/t680-screen-inspection/` (not deployed).
The PMG piece/vertex offsets are documented in
`BetterGPS/Overlay/scripts/build_phone_surfaces.py`.

SHA-256:

- GPS UI: `27B321372ED6849B0E90969ADA83E62D9CE8A6CE639CBFAEA09E7398C9465BAE`
- Interior PMG: `7A0AD6F4E8FADFD825E0017728E359B141123CEBCA588507D3F67EBA541B6103`

## NavPlus integration (2026-10-20)

Update: wide instructions now use anchored 294x139 sizing (+25%) and
`GarminLayout 6`, preserving saved offsets/scales. The shared asset directory
is now `Nuvi1440_collab`. See `peterbilt-smartnav.md` for migration details.

The observed built-in `/ui/gps.sii` + `/vehicle/truck/share/gps.tobj` is now
automatically managed as **Kenworth T680 NavPlus** on `kenworth.t680` only.
The separate built-in window/layer ownership must be observed. Dashboard,
phones, windshield accessories, T680 2022/2025 and other generic truck GPS units
are not matched. Saved Original/disabled/custom style choices always win.

The supplied `_5by3` Home, splash and frame are actually **1275x775** (1.64516),
not exact 5:3. They fit the measured 1.64320 UV exposure to pixel rounding.
The CPU canvas is 900x547, within the existing per-slot marker surface; the
native square target is still supersampled to 1024x1024. Generated UI and
inverse touch tags use the SAME `managed_navplus_rect` measured region,
approximately x=1..1022, y=201..822 on that target. No phone marker dimensions
or native geometry/projection/picking backend is changed.

T680 uses its own format ID 3, menu label `5:3 / T680`, and
`device_N_5by3_navplus.cfg`. W900 keeps format ID 2/900x600 and its existing
`device_N_3by2_navplus.cfg`. All themes, placements, module scales, cameras,
player icons, preferences and startup/page state remain per managed device.
Home/splash/frame select the matching `_3by2` or `_5by3` asset by format.
The NavPlus splash remains opaque for five visible seconds on both trucks.

T680 defaults relocate clock/temperature and compass to the actual supplied
artwork. Nav header, zoom, road area, footer values and LIMIT use format-specific
placements. Footer halves remain edge anchored: their local scale is 16/17 of
the W900 canvas and the gap grows in the middle; text/buttons/plates are not
stretched to fill the wider format. The shared original 1440 footers stay
1200x150. Footer values use the identical two-half transform as their artwork.

The Return target now follows its actual PAGE as well as the editable footer:
Nav is the left square (source x=0..136), Notrip the wider rectangle
(source x=60..309), both y=17..136 in the 1200x150 source. The 5by3 target uses
the same 16/17 left-edge transform as rendering. Zoom is inverted before hit
testing; splash and unrelated controls still do nothing. Diagnostic phone-only
blue app rectangles are deliberately unchanged.

Asset roots are `material/garmin/kenworth_navplus/` (complete device) and
`material/garmin/1440/` (shared map footer). The importer verifies exact decoded
pixels and dimensions. The six device images plus four shared footers replace
the seven legacy source filenames; Git retains the old versions.

Validation: `OverlayPreviewTest <fresh-output> --navplus-t680`, `--navplus`,
`--touch-actions` and the existing non-regression fixtures; `TouchPhoneTest`
also drives real WARP native copies/3D projection/depth through both Return
shapes on W900 and T680, alongside phones, Nuvi and Sony. Synthetic GPU tests
and inspected CPU previews are not a certification of live ATS rendering.
