"""Convert the supplied PNG sequence to bounded GPU-native BC1 frames."""
from pathlib import Path
from PIL import Image
import io, struct, sys

root=Path(__file__).resolve().parents[1]
source=Path(sys.argv[1])
assets=root/'BetterGPS_assets'
target=assets/'material/ui/f150_startup'
target.mkdir(parents=True,exist_ok=True)
frames=sorted(source.glob('*.png'))
assert frames and len(frames)<=300
with (target/'animation.bga').open('wb') as out:
    out.write(struct.pack('<4I',0x31534742,1284,800,len(frames)))
    for path in frames:
        image=Image.open(path).convert('RGB')
        assert image.size==(1280,800), (path,image.size)
        padded=Image.new('RGB',(1284,800))
        padded.paste(image,(4,0))
        padded.paste(image.crop((0,0,1,800)).resize((4,800)),(0,0))
        data=io.BytesIO();padded.save(data,format='DDS',pixel_format='DXT1')
        out.write(data.getvalue()[128:])
# The opaque signature lives in a four-pixel gutter outside the UI crop.
# Safe even before telemetry, marker recognition or the worker's first result.
# The renderer explicitly substitutes transparency only after both loops.
marker=Image.new('RGBA',(1284,800),(5,5,5,255))
for i in range(4):marker.putpixel((i,0),(0x31+10+i,0x5b,0xa7,255))
marker.save(target/'marker.dds')
template=(assets/'material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.tobj').read_bytes()
name=b'/material/ui/f150_startup/marker.dds'
tobj=bytearray(template[:48]);struct.pack_into('<I',tobj,40,len(name));tobj.extend(name)
(target/'marker.tobj').write_bytes(tobj)
(target/'marker.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "/material/ui/f150_startup/marker.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
ui=(root.parent/'validation/f150-display/_ui_dashboard_ford_f150_gps_23.sii').read_text()
ui=ui.replace('my_children: 7','my_children: 8',1).replace(' my_children[6]: _nameless._.parking2',' my_children[6]: _nameless._.parking2\n my_children[7]: _nameless.bettergps.f150startup',1)
overlay='''
ui::text : _nameless.bettergps.f150startup {
 text: "<img src=/material/ui/f150_startup/marker.mat left=0.0031152648 right=1 bottom=1 top=0 xscale=stretch yscale=stretch>"
 coords_l: 0
 coords_r: 1280
 coords_t: 800
 coords_b: 0
 area_l: 1
 area_r: 0
 area_t: 0
 area_b: 1
 id: 0
 layer: 100
 tab: -1
 pointer: -1
 my_parent: _nameless.wnd
}
'''
pos=ui.rfind('}');ui=ui[:pos]+overlay+ui[pos:]
(assets/'ui/dashboard/ford_f150_gps_23.sii').write_text(ui)
print(f'{len(frames)} frames, 30 fps, two loops; {(target/"animation.bga").stat().st_size/1048576:.2f} MiB')
