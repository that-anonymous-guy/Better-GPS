# T680 NavPlus HD Home layers

Home uses `navplushd_gradient_background.dds` (1324x800) followed by
`navplushd_homescreen_toplayer.dds` (1324x641, transparent). The foreground is
registered at source x=0, y=71 inside the 1324x800 Home frame, preserving the
positions and size of the former navigation, gauges and Camera tiles.

Both layers use the existing Home artwork placement, including saved edits and
global zoom. The same composition is used during application opening/closing
swipes. Shared top/bottom controls, clock and all touch regions are unchanged.

`scripts/import_copilot_home.py` imports only these assets as uncompressed DDS
with exact decoded RGBA verification and records the registration offset.
