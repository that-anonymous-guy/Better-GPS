# Pana-Pacific camera family — v0.13.2 catalogue / v0.13.0 models

Current shared authoring project for all four cameras, plus the archived front
mount for low bumpers (not offered in the current catalogue). Named components are in `*_editable.blend`; `*_scs.blend`
contains one export mesh per LOD. Blender 3.6 + SCS Blender Tools; metres.

| Camera / mount | Editable source | Native triangles | Material pieces |
| --- | --- | ---: | ---: |
| PP804395 rear | `pp804395_editable.blend` | 5,562 | 5 |
| PP807032 standard front | `pp807032_editable.blend` | 4,966 | 5 |
| PP807032 low front | `pp807032_low_editable.blend` | 4,966 | 5 |
| PP804396 right | `pp804396_editable.blend` | 7,229 | 4 |
| PP804397 left | `pp804397_editable.blend` | 7,229 | 4 |

Only the standard front mount is shipped in the current package. Far meshes (1,873–2,210 triangles) are exported in the source project;
the accessory definitions still load LOD0 directly, without automatic LOD switching.

## Front option in ATS

One visible option: **Pana-Pacific PP807032 - Front Camera**, $250, level 0,
on the empty `slot_0` of all eight stock bumpers. The proven parent-type filter
`suitable_for[]: "bumper"` is restored. The v0.13.1 full-ID filter did not work
in the user's game and has been reverted. Offline name-matching checks did not
exercise the engine's different handling of hookup suitability.

All bumpers now use the standard model: two 10.5 × 2.5 mm constant-section flats,
62 mm spacing, 89.017 mm long. Model size, materials, angles and all socket poses
are unchanged. Flat intersections with the finer low-bumper grille are explicitly
accepted by the user and reported separately; they are not labelled collision-free.
The archived low-mount Blender source remains available but is no longer shipped.
Its previous hookup ID is retained as a hidden zero-price alias to the standard
model so saved trucks can still resolve it; it is not a second purchase option.

Both front Oversize Load banners (`sign` and `sign_low`) declare conflicts with
the front camera's active and legacy hookup IDs. Both camera IDs reciprocally
declare those two banner units as conflicts. This follows the stock banner's
`conflict_with` convention. Stock banner suitability, price, model and existing
bullbar conflict are retained. No camera/bullbar conflict is declared and the
bullbar definition is not overridden. See `../pp807032/compatibility-reference.json`.
These are native definition checks; both configuration selection orders still
require verification in ATS without this workflow controlling the user's PC.

The backing clamp, fixing heads and stock bumper geometry remain unchanged.
Only the optional standard camera model is loaded. No camera is fitted by default.

## Geometry retained

Front/rear shells have rounded cheeks and pivots, a recessed sealed window,
protective glass, lens and 18 IR diodes in staggered rectangular banks.
The manifold optical pocket has inward-facing walls and an opaque back cap.
Those inner faces now use their own dark material and dark atlas swatch.
Pivot washers have 32 sides, rounded edges and recessed hex-socket fixing heads.

Rear position and scale remain approved: 75 mm overall width, head 45 degrees
down, straight 11.25 mm mounting tabs and a flat wall plate. Two lower-corner
fixings remain visible. The cable enters a drilled central rubber-lined port.
Day/52/76 cabin poses are unchanged, including the earlier sleeper height increase.

Sides retain their approved scale, mirrored cabin poses, 57.337061-degree optical
ball angle and user IR/POD depths. Each flange has four low hex-socket heads clear
of the housing. The cable enters its rubber-lined flange port. Grey removable
labels remain omitted. The ball is darker/glossier than the satin housing.

## Native SCS shaders and reference basis

This export uses native SCS diffuse/specular/normal-map effects, not a Principled
PBR material copied into the game. The [official supported-shader documentation](https://modding.scssoft.com/wiki/Documentation/Tools/SCS_Blender_Tools/Supported_shaders)
describes normal-map and environment passes and the texture-alpha specular mask.
The [official dif.spec implementation](https://github.com/SCSSoftware/BlenderTools/blob/master/addon/io_scs_tools/internals/shaders/eut2/dif_spec/__init__.py)
was also inspected. This does not establish which other workflows ATS may support.

Four exterior finishes are shared by the family. Box cameras add a fifth finish:

| Role | Effect | Source specular | Shininess | Extra reflection |
| --- | --- | ---: | ---: | --- |
| Satin coating | `dif.spec.tsnmap` | 0.48 | 48 | None |
| Glossy ball, lens, IR cups | `dif.spec` | 0.85 | 150 | None |
| Protective cover | `glass` | 0.50 | 95 | Stock vehicle environment |
| Metal fixings | `dif.spec.add.env` | 0.90 | 110 | Fuzzy environment, factor 0.16 |
| Interior walls/back cap | `dif` | 0 | 1 | None |

All have zero additional ambient and no emissive/luminance pass. The cavity has
no normal map or specular response; its dark swatch is multiplied by diffuse 0.5
(compiled linear value 0.214041). This approximates interior occlusion under
the game's lighting. It cannot supply dynamic ray-traced occlusion.

Glass Fresnel `(0.2, 0.9)` and shininess 95 were compared with the extracted T680
glass material `automat/f2/f2988fc421bf27f3.mat`. Soft metal reflection follows
the stock fuzzy environment reference found in `automat/20/20215ca47da9e8ce.mat`,
with a restrained factor for these small fixings. Glass tint opacity is 0.008.
Source material colours are converted to linear values by SCS Conversion Tools;
source and compiled values must not be compared as if they used the same space.

The micro-normal gradient is reduced from 0.028 to 0.020. Textures remain simple
2048 × 256 padded swatches with local box projections and mipmaps: no complex
unique unwrap. `palette` stores RGB colour and alpha specularity; `micro_normal`
uses `usage tsnormal` and `color_space linear`; `glass` is a small shared texture.
The six DDS/TOBJ files total 1,399,117 bytes (~1.34 MiB). Built-in environment
maps are referenced, not replaced or bundled. Editable PNG/TGA sources live in
`project/vehicle/truck/upgrade/bettergps/camera_finish/`.

## Verification and packaging

- `geometry-report.json`: budgets, source finishes and mount profiles.
- `material-validation.json`: converted effects, dark non-specular cavity,
  real normal data, mipmaps, source references and runtime dependencies.
- `editable-validation.json`: closed manifold shell, assigned cavity faces,
  constant flats, head/housing/cable clearances and removed side labels.
- `pocket-validation.json`: 24 front-facing opaque native ray hits, excluding
  transparent glass and culling backfaces; no sampled window-border escape.
- `mount-validation.json`: rear walls/equipment, side mirrors/symmetry,
  full front flats and backing-clamp contact on eight bumpers; low-grille
+  flat clipping is reported as an accepted exception.
- `../pp804395/scs-light/independent-model-check.json`: independent TruckLib
  native reader, five materials on box cameras, optical angles/glass placement,
  registered optional hookups, stock geometry and 64 parent transformations.
- `preservation-validation.json`: unchanged rear/side sockets/definitions,
  exact tiny front pose deltas and texture cost.

Current installed package keeps the user's active name
`Documents/American Truck Simulator/mod/BetterGPS_PP804395_T680NG.zip`.
Installation preserves unrelated entries and backs up the old ZIP and config.
No settings/profiles are modified. No game launch or desktop control is used.
Offline studio previews are illustrative and do not reproduce ATS lighting;
final appearance and menu interaction require manual in-game verification.

Rebuild: `refine_camera_family.py` in background Blender 3.6 with the official
addon; convert `project/` with SCS Conversion Tools; assemble using
`package_pp804395_scs.py`. Run the five checkers listed above plus the independent
`CameraModelCheck` reader. Earlier single-camera generators describe historical
palette-only models; do not run them over this shared refinement.
