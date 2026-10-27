"""Add one transparent marker over the Mack central cluster; preserve native LCDs."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
from PIL import Image
from validate_sii_units import validate_sii_units

parser=argparse.ArgumentParser()
parser.add_argument('--texture',type=Path,required=True)
parser.add_argument('--stock-root',type=Path,required=True)
args=parser.parse_args()
assets=Path(__file__).resolve().parents[1]/'BetterGPS_assets'
art=assets/'material/tomtom/mack_infotainment/mack_cluster_intro.dds'
art.parent.mkdir(parents=True,exist_ok=True)
# Supplied BC7 has non-block-aligned 1275x725 dimensions. D3D11 rejects
# that compressed resource; decode once offline and retain every RGBA pixel.
with Image.open(args.texture) as source:
    decoded=source.convert('RGBA')
    decoded.save(art,format='DDS')
    with Image.open(art) as stored:
        assert stored.size==decoded.size and stored.convert('RGBA').tobytes()==decoded.tobytes()
prefix=assets/'material/ui/dashboard/bettergps_overlaynew/mack_cluster_intro'
width,height=1284,728
header=bytearray((prefix.parent/'phone_layout_marker.dds').read_bytes()[:128])
struct.pack_into('<III',header,12,height,width,width*4)
pixels=bytearray(width*height*4)
for i in range(4):pixels[i*4:i*4+4]=bytes((0xa7,0x5b,0x31+151+i,255))
prefix.with_suffix('.dds').write_bytes(header+pixels)
path='/material/ui/dashboard/bettergps_overlaynew/mack_cluster_intro.dds'
tobj=bytearray((prefix.parent/'phone_layout_marker.tobj').read_bytes()[:48])
struct.pack_into('<I',tobj,40,len(path))
prefix.with_suffix('.tobj').write_bytes(tobj+path.encode())
prefix.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "/material/ui/dashboard/bettergps_overlaynew/mack_cluster_intro.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
report={'source_texture_sha256':hashlib.sha256(args.texture.read_bytes()).hexdigest(),'texture_sha256':hashlib.sha256(art.read_bytes()).hexdigest(),'texture_dimensions':list(decoded.size),'texture_encoding':'DDS RGBA32 lossless from decoded source','still_ms':3000,'live_cluster_fade_ms':500,'marker_role':151,'marker_dimensions':[width,height],'central_cluster_ui_rect':[0,0,800,454],'native':{}}
for truck in ('anthem','pinnacle'):
    name=f'mack_{truck}.sii'
    original=(args.stock_root/'ui/dashboard'/name).read_text(encoding='utf-8-sig')
    assert 'coords_r: 801' in original and 'coords_t: 800' in original
    assert original.split('}',1)[0].count('my_children: 9\n')==1 and 'bgmk_intro' not in original
    parent=re.search(r'ui::window\s*:\s*(\S+)\s*\{',original)[1]
    ui=original.replace('my_children: 9\n','my_children: 10\n',1)
    ui=ui.replace(' my_children[8]: _nameless._.display4\n',' my_children[8]: _nameless._.display4\n my_children[9]: _nameless.bgmk_intro\n',1)
    # Only the central 800x454 region. Other parts of this 801x800 atlas
    # drive separate clock/gear LCDs and must never receive the splash.
    node=f'''\nui::text : _nameless.bgmk_intro {{
 text: "<img src=/material/ui/dashboard/bettergps_overlaynew/mack_cluster_intro.mat left=0 right={1280/1284:.12f} top=0 bottom=1 xscale=stretch yscale=stretch>"
 coords_l: 0
 coords_r: 800
 coords_t: 454
 coords_b: 0
 area_l: 1
 area_r: 0
 area_t: 0
 area_b: 1
 id: 0
 layer: 127
 tab: -1
 pointer: -1
 my_parent: {parent}
}}
'''
    assert ui.rstrip().endswith('}')
    ui=ui.rstrip()[:-1]+node+'}\n'
    validate_sii_units(ui)
    restored=ui.replace(' my_children[9]: _nameless.bgmk_intro\n','',1).replace('my_children: 10\n','my_children: 9\n',1).replace(node,'',1)
    assert restored.rstrip()==original.rstrip(), 'Native cluster/LCDs must remain byte-preserved'
    (assets/'ui/dashboard'/name).write_text(ui,encoding='utf-8',newline='\n')
    report['native'][name]={'sha256':hashlib.sha256((args.stock_root/'ui/dashboard'/name).read_bytes()).hexdigest(),'original_units':len(re.findall(r'^ui[^\n]+\{',original,re.M)),'overlay_parent':parent}
# The original GPS UI can draw before the generated BetterGPS window is bound.
# This independent transparent marker contains NO cluster logo or intro clock.
guard=prefix.parent/'mack_gps_guard'
header=bytearray((prefix.parent/'phone_layout_marker.dds').read_bytes()[:128])
struct.pack_into('<III',header,12,4,8,8*4)
pixels=bytearray(8*4*4)
for i in range(4):pixels[i*4:i*4+4]=bytes((0xa7,0x5b,0x31+152+i,255))
guard.with_suffix('.dds').write_bytes(header+pixels)
path='/material/ui/dashboard/bettergps_overlaynew/mack_gps_guard.dds'
tobj=bytearray((prefix.parent/'phone_layout_marker.tobj').read_bytes()[:48])
struct.pack_into('<I',tobj,40,len(path));guard.with_suffix('.tobj').write_bytes(tobj+path.encode())
guard.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "/material/ui/dashboard/bettergps_overlaynew/mack_gps_guard.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
report['gps_guard']={'marker_role':152,'marker_dimensions':[8,4],'ui_paths':[]}
for truck in ('anthem','pinnacle'):
    target=assets/f'ui/gmaps_mack{truck}_gps.sii'
    original=target.read_text(encoding='utf-8-sig')
    if '\nui::text : _nameless.bgmk_guard' in original:
        original=original.split('\nui::text : _nameless.bgmk_guard',1)[0]+'}\n'
        original=original.replace(' my_children[2]: _nameless.bgmk_guard\n','',1).replace('my_children: 3\n','my_children: 2\n',1)
    assert original.split('}',1)[0].count('my_children: 2\n')==1
    parent=re.search(r'ui::window\s*:\s*(\S+)\s*\{',original)[1]
    ui=original.replace('my_children: 2\n','my_children: 3\n',1).replace(' my_children[1]: _nameless.a8f.f870\n',' my_children[1]: _nameless.a8f.f870\n my_children[2]: _nameless.bgmk_guard\n',1)
    # Crop out the four sentinel pixels: the stock artwork itself is transparent.
    node=f'''\nui::text : _nameless.bgmk_guard {{
 text: "<img src=/material/ui/dashboard/bettergps_overlaynew/mack_gps_guard.mat left=0.5625 right=0.9375 top=0.375 bottom=0.875 xscale=stretch yscale=stretch>"
 coords_l: 0
 coords_r: 256
 coords_t: 256
 coords_b: 0
 area_l: 1
 area_r: 0
 area_t: 0
 area_b: 1
 id: 0
 layer: 127
 tab: -1
 pointer: -1
 my_parent: {parent}
}}
'''
    ui=ui.rstrip()[:-1]+node+'}\n'
    validate_sii_units(ui)
    restored=ui.replace(' my_children[2]: _nameless.bgmk_guard\n','',1).replace('my_children: 3\n','my_children: 2\n',1).replace(node,'',1)
    assert restored.rstrip()==original.rstrip(), 'Native GPS map must remain unchanged under guard'
    target.write_text(ui,encoding='utf-8',newline='\n')
    report['gps_guard']['ui_paths'].append(f'/ui/gmaps_mack{truck}_gps.sii')
(art.parent/'cluster-import-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
