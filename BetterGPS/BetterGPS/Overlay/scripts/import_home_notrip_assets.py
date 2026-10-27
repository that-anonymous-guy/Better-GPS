"""Import supplied home/notrip artwork pixel-exactly; never alter the originals."""
from pathlib import Path
from io import BytesIO
import shutil
import struct
import sys
from PIL import Image

source = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[1] / 'BetterGPS_assets/material'

def image(path):
    data = bytearray(path.read_bytes())
    if data[84:88] == b'DX10':
        fmt = struct.unpack_from('<I', data, 128)[0]
        if fmt in {72, 75, 78, 99}:
            struct.pack_into('<I', data, 128, fmt-1)
    return Image.open(BytesIO(data))

def install(src, dst):
    dst.parent.mkdir(parents=True, exist_ok=True)
    with image(src) as im:
        rgba = im.convert('RGBA')
        if im.width % 4 or im.height % 4:
            rgba.save(dst, format='DDS')
        else:
            shutil.copy2(src, dst)
        with image(dst) as imported:
            assert imported.size == im.size
            assert imported.convert('RGBA').tobytes() == rgba.tobytes()
    print(dst.relative_to(root), 'pixel-exact', rgba.size)

for directory in ('home', 'notrip'):
    for src in sorted((source/'garmin/Nuvi465T'/directory).glob('*.dds')):
        install(src, root/'garmin/nuvi465t'/directory/src.name)
navplus = source/'garmin/kenworth_navplus'
for shape in ('3by2', '5by3'):
    name = f'home_kenworth_navplus_{shape}.dds'
    install(navplus/name, root/'garmin/kenworth_navplus'/name)
for name in ('notrip_1440_darkfooter_anyres_light.dds',
             'notrip_1440_darkfooter_anyres_dark.dds'):
    install(source/'garmin/Nuvi1440_collab'/name, root/'garmin/Nuvi1440_collab'/name)
icons = source/'phone/lockscreen-home/icons'
for os in ('iphone',):
    for mix in ('normalmix', 'overlaymix'):
        name = f'{os}_lock_{mix}.dds'
        install(icons/name, root/'ui/phone_lockscreen/icons'/name)
        name = f'{os}_home_{mix}.dds'
        install(icons/name, root/'ui/phone_home/icons'/name)
for name in ('googlemaps_transition.dds', 'waze_transition.dds'):
    install(source/'phone/lockscreen-home/transitions'/name,
            root/'ui/phone_home/transitions'/name)
install(source/'misc/crownvic/sonygpshome.dds', root/'sony/crown_victoria/sonygpshome.dds')
