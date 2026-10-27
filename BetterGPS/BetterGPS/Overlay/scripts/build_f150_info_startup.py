from pathlib import Path
from PIL import Image,ImageStat
import struct,io,shutil,sys
root=Path('BetterGPS/Overlay/BetterGPS_assets')
target=root/'material/ui/f150_info_startup';target.mkdir(parents=True,exist_ok=True)
sequence=Path(sys.argv[1]) if len(sys.argv)>1 else Path('C:/Users/Crym2/Downloads/splash_info');frames=sorted(sequence.glob('*.png'));assert frames and len(frames)<=300
# Drop the source's nearly-black end cards so the moving image fades directly
# into the live instrument cluster, without an intermediate black screen.
while len(frames)>1 and max(ImageStat.Stat(Image.open(frames[-1]).convert('RGB')).mean)<0.25:
 frames.pop()
with (target/'animation.bga').open('wb') as f:
 f.write(struct.pack('<4I',0x31534742,1444,540,len(frames)))
 for path in frames:
  im=Image.open(path).convert('RGBA');assert im.size==(1440,540)
  bg=Image.new('RGBA',im.size,(5,5,5,255));bg.alpha_composite(im);im=bg.convert('RGB');pad=Image.new('RGB',(1444,540));pad.paste(im,(4,0));pad.paste(im.crop((0,0,1,540)).resize((4,540)),(0,0));buf=io.BytesIO();pad.save(buf,format='DDS',pixel_format='DXT1');f.write(buf.getvalue()[128:])
marker=Image.open(frames[0]).convert('RGBA');pad=Image.new('RGBA',(1444,540),(5,5,5,255));pad.paste(marker,(4,0));
for i in range(4):pad.putpixel((i,0),(0x31+11+i,0x5b,0xa7,255))
pad.save(target/'marker.dds')
template=(root/'material/ui/f150_startup/marker.tobj').read_bytes();name=b'/material/ui/f150_info_startup/marker.dds';tobj=bytearray(template[:48]);struct.pack_into('<I',tobj,40,len(name));tobj.extend(name);(target/'marker.tobj').write_bytes(tobj)
(target/'marker.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "/material/ui/f150_info_startup/marker.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
ui=Path('validation/f150-display/_ui_dashboard_ford_f150_23.sii').read_text();ui=ui.replace('my_children: 6','my_children: 7',1).replace('my_children[5]: _nameless._.service','my_children[5]: _nameless._.service\n my_children[6]: _nameless.bettergps.f150info',1)
overlay='''
ui::text : _nameless.bettergps.f150info {
 text: "<img src=/material/ui/f150_info_startup/marker.mat left=0.002770083102493 right=1 bottom=1 top=0 xscale=stretch yscale=stretch>"
 coords_l: 0
 coords_r: 1440
 coords_t: 540
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
pos=ui.rfind('}');ui=ui[:pos]+overlay+ui[pos:];(root/'ui/dashboard/ford_f150_23.sii').write_text(ui)
# Composite alpha over the opaque waiting color for every displayed still frame.
central=root/'material/ui/f150_startup';shutil.copy2(Path(sys.argv[2]) if len(sys.argv)>2 else Path('C:/Users/Crym2/Downloads/Ford_simple.dds'),central/'Ford_simple.dds');im=Image.open(central/'Ford_simple.dds').convert('RGBA');bg=Image.new('RGBA',im.size,(5,5,5,255));bg.alpha_composite(im);pad=Image.new('RGB',(1284,800));pad.paste(bg.convert('RGB'),(4,0));pad.paste(bg.crop((0,0,1,800)).resize((4,800)).convert('RGB'),(0,0));buf=io.BytesIO();pad.save(buf,format='DDS',pixel_format='DXT1');blocks=bytearray(buf.getvalue()[128:])
# BC1 can represent RGB(5,5,5) exactly as the interpolated palette entry.
# Preserve the waiting background without a green tint from RGB565 rounding.
for y in range(0,800,4):
 for x in range(0,1284,4):
  if pad.crop((x,y,x+4,y+4)).getextrema()==((5,5),(5,5),(5,5)):
   offset=((y//4)*(1284//4)+x//4)*8;blocks[offset:offset+8]=struct.pack('<HHI',(1<<11)|(2<<5)|1,0,0xAAAAAAAA)
(central/'Ford_simple.bga').write_bytes(struct.pack('<4I',0x31534742,1284,800,1)+blocks)
print(f'Built info splash: {len(frames)} x 1444x540 BC1 frames; black end cards trimmed; transparent Ford_simple over RGB(5,5,5).')
