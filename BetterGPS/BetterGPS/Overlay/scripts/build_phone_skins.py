"""Prepare exact RGBA pixels from the supplied phone skins for native rendering."""
from pathlib import Path
from PIL import Image
import struct
import json

root = Path(__file__).resolve().parents[1]
assets = root / 'BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew'
generated = root / 'generated'
generated.mkdir(exist_ok=True)
# Retain one shared canvas/anchor throughout the unlock motion. Source PNGs
# include near-transparent keying residue; discard only alpha below 8/255.
unlock_paths = sorted((root/'startup/unlock').glob('*.png'))
assert unlock_paths, 'Missing unlock sequence'
unlock_frames=[]
for path in unlock_paths:
    image=Image.open(path).convert('RGBA')
    assert image.size == (640, 360)
    image.putalpha(image.getchannel('A').point(lambda a: 0 if a < 8 else a))
    image=image.crop((240,90,440,290)).resize((24,24),Image.Resampling.LANCZOS)
    unlock_frames.append(image.tobytes('raw','BGRA'))
(generated/'unlock.bgra').write_bytes(struct.pack('<III',24,24,len(unlock_frames))+b''.join(unlock_frames))
for name in ('lockscreen_2_background', 'lockscreen_2_toplayer'):
    image = Image.open(root/'startup'/f'{name}.dds').convert('RGBA')
    assert image.size == (360, 740)
    (generated/f'{name}.bgra').write_bytes(struct.pack('<II', *image.size)+image.tobytes('raw', 'BGRA'))
modules = root/'phone_modules'
layout = json.loads((modules/'layout.json').read_text())
(generated/'phone_modules').mkdir(exist_ok=True)
rows=[];resources=[]
for index, entry in enumerate(layout):
    image=Image.open(modules/entry['file']).convert('RGBA')
    target=Path(entry['file']).stem+'.bgra'
    (generated/'phone_modules'/target).write_bytes(struct.pack('<II', *image.size)+image.tobytes('raw','BGRA'))
    resource=270+index;mask=sum(1<<state for state in entry['states'])
    rows.append(f'{{{resource},{image.width},{image.height},{entry["x"]},{entry["y"]},{mask}}}')
    resources.append(f'{resource} RCDATA "generated/phone_modules/{target}"')
(generated/'PhoneModules.rc').write_text('\n'.join(resources)+'\n')
(generated/'PhoneModuleLayout.h').write_text('#pragma once\nnamespace bettergps_overlay {\nstruct PhoneModule {unsigned resource,width,height;int x,y;unsigned states;};\ninline constexpr PhoneModule phoneModules[]={'+','.join(rows)+'};\n}\n')

# The supplied needle leans about 30 degrees clockwise. Calibrate its rendered
# red tip (29.765 degrees, including downsampling) so north is exactly on top,
# retaining its alpha hole and original red/gray colors. The round button is
# already part of each supplied skin and does not rotate.
needle = Image.open(assets/'compass.dds').convert('RGBA')
canvas = Image.new('RGBA', (400, 400))
canvas.alpha_composite(needle, (75, 25))
canvas = canvas.rotate(29.765, resample=Image.Resampling.BICUBIC).resize((36, 36), Image.Resampling.LANCZOS)
(generated/'compass.bgra').write_bytes(struct.pack('<II', *canvas.size)+canvas.tobytes('raw', 'BGRA'))
# A distinct marker resource lets the plugin switch skins without changing or
# overwriting either of the user's originals.
header = bytearray((assets/'navmaneuver.dds').read_bytes()[:128])
# Extend the native instruction canvas while retaining its 3:1 pixel scale.
guidance_header=bytearray(header)
struct.pack_into('<III',guidance_header,12,480,960,960*4)
guidance_pixels=bytearray(960*480*4)
for i in range(4):guidance_pixels[i*4:i*4+4]=bytes((0xA7,0x5B,0x31+5+i,255))
(assets/'navmaneuver.dds').write_bytes(guidance_header+guidance_pixels)
struct.pack_into('<III', header, 12, 740, 360, 360*4)
pixels = bytearray(360*740*4)
pixels[3::4]=b'\xff'*(360*740)
for i in range(4):
    pixels[i*4:i*4+4] = bytes((0xA7, 0x5B, 0x31+6+i, 255))
(assets/'phone_layout_marker.dds').write_bytes(header+pixels)
path = b'/material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.dds'
tobj = bytearray((assets/'navmaneuver.tobj').read_bytes()[:48])
struct.pack_into('<I', tobj, 40, len(path))
(assets/'phone_layout_marker.tobj').write_bytes(tobj+path)
(assets/'phone_layout_marker.mat').write_text((assets/'navmaneuver.mat').read_text().replace('navmaneuver.tobj', 'phone_layout_marker.tobj'))

# Keep the game map's resource name so ATS updates it normally. Its signature
# identifies the source texture before its first live map update.
dashboard = assets.parent
for name, width, height, role in [('bare_map', 256, 256, 7),
                                  ('phone_map_background', 360, 720, 8)]:
    marker_header = bytearray(header)
    struct.pack_into('<III', marker_header, 12, height, width, width*4)
    marker_pixels = bytearray(width*height*4)
    for i in range(4):
        marker_pixels[i*4:i*4+4] = bytes((0xA7, 0x5B, 0x31+role+i, 255))
    (dashboard/f'{name}.dds').write_bytes(marker_header+marker_pixels)
    if name == 'phone_map_background':
        marker_path = f'/material/ui/dashboard/{name}.dds'.encode()
        marker_tobj = bytearray(tobj)
        struct.pack_into('<I', marker_tobj, 40, len(marker_path))
        (dashboard/f'{name}.tobj').write_bytes(marker_tobj+marker_path)
        (dashboard/f'{name}.mat').write_text((assets/'navmaneuver.mat').read_text().replace('navmaneuver.tobj', f'{name}.tobj'))

# Alternate Waze overlay modules use independent placements but the shared status bar.
waze = modules/'waze'
entries = json.loads((waze/'layout.json').read_text())
(generated/'waze').mkdir(exist_ok=True)
rows=[]; resources=[]
for index, entry in enumerate(entries):
    image=Image.open(waze/entry['file']).convert('RGBA')
    target=f'module_{index}.bgra'
    (generated/'waze'/target).write_bytes(struct.pack('<II',*image.size)+image.tobytes('raw','BGRA'))
    rows.append('{'+f'{340+index},{image.width},{image.height},{entry["x"]},{entry["y"]},{sum(1<<state for state in entry["states"])}'+'}')
    resources.append(f'{340+index} RCDATA "generated/waze/{target}"')
for index, name in enumerate(['waze_report_light.dds','waze_report_dark.dds','waze_compass_light.dds','waze_compass_dark.dds','us_limit.dds']):
    image=Image.open(waze/name).convert('RGBA')
    target=f'extra_{index}.bgra'
    (generated/'waze'/target).write_bytes(struct.pack('<II',*image.size)+image.tobytes('raw','BGRA'))
    resources.append(f'{354+index} RCDATA "generated/waze/{target}"')
(generated/'WazeModules.rc').write_text('\n'.join(resources)+'\n')
(generated/'WazeModuleLayout.h').write_text('#pragma once\nnamespace bettergps_overlay {inline constexpr PhoneModule wazeModules[]={'+','.join(rows)+'};}\n')

# Supplied voice-navigation mute variants: the same module dimensions/anchors.
mute_files=[modules/'navmode/nav_buttons_light_mute.dds',modules/'navmode/nav_buttons_dark_mute.dds',waze/'vertical/waze_buttons_topright_mute_vertical_light.dds',waze/'vertical/waze_buttons_topright_mute_vertical_dark.dds']
extra=[]
for index,path in enumerate(mute_files):
    image=Image.open(path).convert('RGBA');target=f'mute_{index}.bgra'
    (generated/'waze'/target).write_bytes(struct.pack('<II',*image.size)+image.tobytes('raw','BGRA'))
    extra.append(f'{360+index} RCDATA "generated/waze/{target}"')
with (generated/'WazeModules.rc').open('a') as file:file.write('\n'.join(extra)+'\n')
