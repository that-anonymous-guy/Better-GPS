# Peterbilt 579 integrated GPS

Read-only mounted ATS asset inspection, 2026-10-20. This is the original
`peterbilt.579`, not a newer 579 generation or the instrument cluster.

`/def/vehicle/truck/peterbilt.579/interior/exclusive.sii` references
`/vehicle/truck/peterbilt_579/interior/model.pmd` and `/ui/gps.sii`.
The GPS material is `/automat/d8/d8187d0c1f48c659.mat` (material 18), sampling
`/vehicle/truck/share/gps.tobj`. The native DDS/root UI are 256x256.

The actual GPS quad is PMG piece 6, four vertices, version 0x15:

- U: 3.0063953399658203 .. 3.993605613708496
- V: 3.163893938064575 .. 3.836106061935425
- Visible source region: **252.72583 x 172.08630 texels**
- Visible UV aspect: **1.46859933:1**

For artwork matching this source-UV region, use **1200x817** or **1275x868**
(integer rounding). The 256x256 source is not the visible screen ratio.
This measures native UV exposure, not a future runtime supersampling size
or the physical mesh aspect. This initial inspection was read-only; the later
SmartNav integration uses the supplied 3by2 art with uniform fitting. See
`peterbilt-smartnav.md` for implementation and tests.

Reproduction: AssetInspector with the paths above and
`/vehicle/truck/peterbilt_579/interior/model.pmg`,
`--texture=/vehicle/truck/share/gps.tobj`.
Local extraction: `build/p579-screen-inspection/`.
