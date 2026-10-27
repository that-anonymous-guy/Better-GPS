"""Import supplied Sygic artwork and lossless VNL25 movie; preserve stock UI."""
import argparse, ctypes, hashlib, json, re, struct, subprocess
from pathlib import Path
from PIL import Image
from validate_sii_units import validate_sii_units

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--stock-root', type=Path, required=True)
parser.add_argument('--preview', type=Path, required=True)
args = parser.parse_args()
assets = Path(__file__).resolve().parents[1]/'BetterGPS_assets'
target = assets/'material/sygic'
args.preview.mkdir(parents=True, exist_ok=True)
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
report = {'artwork': [], 'native': {}}
for source in sorted(args.source.rglob('*.dds')):
    relative = source.relative_to(args.source)
    destination = target/relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    image = Image.open(source).convert('RGBA')
    image.save(destination, format='DDS')
    assert Image.open(destination).convert('RGBA').tobytes() == image.tobytes()
    report['artwork'].append(dict(path=relative.as_posix(), size=image.size,
                                 source_sha256=digest(source), sha256=digest(destination)))
    image.save(args.preview/(source.stem+'.png'))

source = args.source/'vnl2025/vnl2025_cluster_intro.mp4'
probe = json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0',
    '-show_entries','stream=width,height,r_frame_rate,nb_frames,duration','-of','json',str(source)]))['streams'][0]
assert probe['r_frame_rate']=='30/1' and int(probe['nb_frames'])==150, probe
width, height, count = 1604, 600, 150
api = ctypes.WinDLL('cabinet', use_last_error=True)
for name in ('CreateCompressor','CreateDecompressor'):
    getattr(api,name).argtypes=[ctypes.c_ulong,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
for name in ('Compress','Decompress'):
    getattr(api,name).argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
for name in ('CloseCompressor','CloseDecompressor'): getattr(api,name).argtypes=[ctypes.c_void_p]
compressor, decoder = ctypes.c_void_p(), ctypes.c_void_p()
assert api.CreateCompressor(4,None,ctypes.byref(compressor)) and api.CreateDecompressor(4,None,ctypes.byref(decoder))
size=width*height*4
compressed=ctypes.create_string_buffer(size+65536); restored=ctypes.create_string_buffer(size)
process=subprocess.Popen(['ffmpeg','-v','error','-i',str(source),'-vf',
    'scale=1600:600:flags=lanczos,pad=1604:600:0:0:color=0x0a0a0a',
    '-frames:v',str(count),'-f','rawvideo','-pix_fmt','bgra','pipe:1'],stdout=subprocess.PIPE)
unique=[]; lookup={}; timeline=[]; hashes=[]
try:
    for frame in range(count):
        raw=process.stdout.read(size); assert len(raw)==size
        written=ctypes.c_size_t()
        assert api.Compress(compressor,raw,size,compressed,len(compressed),ctypes.byref(written))
        block=compressed.raw[:written.value]; decoded=ctypes.c_size_t()
        assert api.Decompress(decoder,block,len(block),restored,size,ctypes.byref(decoded)) and decoded.value==size and restored.raw==raw
        if block not in lookup: lookup[block]=len(unique); unique.append(block)
        timeline.append(lookup[block]); hashes.append(hashlib.sha256(raw).hexdigest())
        if frame in (0,75,149): Image.frombytes('RGBA',(width,height),raw,'raw','BGRA').save(args.preview/f'cluster-{frame}.png')
    assert process.wait()==0
finally:
    if process.poll() is None: process.kill(); process.wait()
    api.CloseCompressor(compressor); api.CloseDecompressor(decoder)
movie=target/'vnl2025/vnl2025_cluster_intro.bga'
movie.write_bytes(struct.pack('<5I',0x33434742,width,height,count,len(unique))+struct.pack(f'<{count}H',*timeline)+struct.pack(f'<{len(unique)}I',*(len(b) for b in unique))+b''.join(unique))
report['movie']=dict(source_sha256=digest(source),sha256=digest(movie),width=width,height=height,
    frames=count,fps=30,play_ms=5000,fade_ms=500,verified_frames=count,frame_sha256=hashes)

def marker(name,w,h,role):
    p=assets/'material/ui/dashboard/bettergps_overlaynew'/name
    header=bytearray((p.parent/'phone_layout_marker.dds').read_bytes()[:128]); struct.pack_into('<III',header,12,h,w,w*4)
    pixels=bytearray(w*h*4)
    for i in range(4): pixels[i*4:i*4+4]=bytes((0xa7,0x5b,0x31+role+i,255))
    p.with_suffix('.dds').write_bytes(header+pixels)
    path='/'+p.relative_to(assets).as_posix()+'.dds'
    tobj=bytearray((p.parent/'phone_layout_marker.tobj').read_bytes()[:48]);struct.pack_into('<I',tobj,40,len(path))
    p.with_suffix('.tobj').write_bytes(tobj+path.encode())
    p.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "'+path[:-4]+'.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
    return '/'+p.relative_to(assets).as_posix()+'.mat'

for suffix,name,material,crop in [('', 'bgv25_intro',marker('volvo25_cluster_intro',width,height,158),f'left=0 right={1600/1604:.12f} top=0 bottom=1'),
    ('_gps','bgv25_guard',marker('volvo25_gps_guard',24,4,159),'left=0.3 right=0.9 top=0.375 bottom=0.875')]:
    path=Path('ui/dashboard')/('volvo_vnl2025'+suffix+'.sii')
    original=(args.stock_root/path).read_text(encoding='utf-8-sig')
    first=original.split('}',1)[0]; children=int(re.search(r'my_children:\s*(\d+)',first)[1])
    parent=re.search(r'ui::window\s*:\s*(\S+)\s*\{',original)[1]
    w,h=(1280,720) if suffix else (1440,540)
    anchor=re.search(r' my_children\['+str(children-1)+r'\]:[^\n]*\n',first)[0]
    ui=original.replace(f'my_children: {children}\n',f'my_children: {children+1}\n',1).replace(anchor,anchor+f' my_children[{children}]: _nameless.{name}\n',1)
    node=f'''\nui::text : _nameless.{name} {{
 text: "<img src={material} {crop} xscale=stretch yscale=stretch>"
 coords_l: 0
 coords_r: {w}
 coords_t: {h}
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
    ui=ui.rstrip()[:-1]+node+'}\n'; validate_sii_units(ui)
    restored=ui.replace(f' my_children[{children}]: _nameless.{name}\n','',1).replace(f'my_children: {children+1}\n',f'my_children: {children}\n',1).replace(node,'',1)
    assert restored.rstrip()==original.rstrip()
    (assets/path).parent.mkdir(parents=True,exist_ok=True); (assets/path).write_text(ui,encoding='utf-8',newline='\n')
    report['native'][path.as_posix()]=hashlib.sha256(original.encode()).hexdigest()

base=(assets/'native_themes/light/colour_data.sui').read_text()
def packed(rgb):
    r,g,b=bytes.fromhex(rgb);return f'0xFF{b:02X}{g:02X}{r:02X}'
for dark in (False,True):
    road,ground,building,green,water,route,outline=('587580','394D53','30434D','344E4C','243D4E','23A8F3','163F54') if dark else ('FFFFFF','E9E8EF','D1D4DD','CCE2C9','B9D7ED','713FEB','32186B')
    fields={k:road for k in ('road_color','road_discovered_color','prefab_color','prefab_discovered_color')}
    fields.update({k:building for k in ('prefab_obstacle_color','prefab_obstacle_discovered_color')})
    fields.update({k:green for k in ('prefab_grass_color','prefab_grass_discovered_color')})
    fields.update({k:route for k in ('navigation_color','navigation_current_color','navigation_highlight_color','navigation_fade_color','navigation_arrow_color')})
    fields['outline_color']=outline
    text=re.sub(r'(?m)^(\s*\w+):\s*0x[0-9a-fA-F]+',lambda m:f'{m[1]}: {packed(fields[m[1].strip()])}' if m[1].strip() in fields else m[0],base)
    def area(m):
        name=m[1];rgb=route if name.startswith('Nav-') else road if name=='Road' else green if name=='Green' else building if name=='Dark' else water if name=='Water' else ground
        return f'map_area_color_name[]: "{name}"\n map_area_color[]: {packed(rgb)}\n map_area_discovered_color[]: {packed(rgb)}'
    text=re.sub(r'map_area_color_name\[\]:\s*"([^"]+)"\s*map_area_color\[\]:\s*0x[0-9a-fA-F]+[^\n]*\s*map_area_discovered_color\[\]:\s*0x[0-9a-fA-F]+',area,text)
    text=re.sub(r'^//[^\n]*','// Sygic palette from supplied references; native ATS map semantics preserved.',text,count=1)
    folder=assets/'native_themes'/('sygic_dark' if dark else 'sygic_light');folder.mkdir(parents=True,exist_ok=True)
    (folder/'colour_data.sui').write_text('\n'.join(line.rstrip() for line in text.splitlines())+'\n')
(target/'import-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'Imported {len(report["artwork"])} textures; verified {count} lossless movie frames; stock UIs preserved.')
