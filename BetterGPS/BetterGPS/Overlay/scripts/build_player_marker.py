"""Repair the phone position marker's alpha without changing its blue arrow."""
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1]
data = (root/'theme_sources/player/player-original.dds').read_bytes()
height, width = struct.unpack_from('<II', data, 12)
assert (width, height) == (64, 64) and len(data) == 128+width*height*4
header = bytearray(data[:128])
pixels = bytearray(data[128:])
for offset in range(0, len(pixels), 4):
    b, g, r, alpha = pixels[offset:offset+4]
    if b == g == r:
        # Remove black RGB in the transparent fringe: bilinear interpolation
        # must mix white coverage, never a dark or tinted outline.
        pixels[offset:offset+3] = bytes((255, 255, 255))
    # Original interior alpha is 204 (80%). Restore an opaque white disc while
    # retaining its antialiased silhouette and the already opaque blue arrow.
    pixels[offset+3] = min(255, round(alpha*255/204))
struct.pack_into('<I', header, 8, 0x100F)   # caps, dimensions, pixel format, pitch
struct.pack_into('<I', header, 20, width*4)
struct.pack_into('<I', header, 28, 1)
struct.pack_into('<I', header, 108, 0x1000) # single 2D texture, no bogus mip chain
(root/'BetterGPS_assets/material/ui/map/player.dds').write_bytes(header+pixels)
