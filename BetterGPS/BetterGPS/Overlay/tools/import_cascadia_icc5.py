"""Import ICC5 artwork, lossless 30 fps cluster movie and additive stock guards."""
import argparse, ctypes, hashlib, json, re, struct, subprocess
from pathlib import Path
from PIL import Image
from validate_sii_units import validate_sii_units

p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--stock-root',type=Path,required=True)
p.add_argument('--preview',type=Path,required=True)
p.add_argument('--westernstar',action='store_true')
args=p.parse_args()
assets=Path(__file__).resolve().parents[1]/'BetterGPS_assets'
target=assets/('material/tomtom/westernstar' if args.westernstar else 'material/tomtom/cascadia/icc5')
target.mkdir(parents=True,exist_ok=True);args.preview.mkdir(parents=True,exist_ok=True)
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report={'artwork':[],'native':{}}
for source in sorted(args.source.glob('*.dds')):
    image=Image.open(source).convert('RGBA');destination=target/source.name
    image.save(destination,format='DDS')
    assert Image.open(destination).convert('RGBA').tobytes()==image.tobytes()
    image.save(args.preview/(source.stem+'.png'))
    report['artwork'].append(dict(path=source.name,size=image.size,source_sha256=digest(source),sha256=digest(destination)))

source=args.source/('westernstar_icc5_cluster_intro.mp4' if args.westernstar else 'freightliner_icc5_cluster_intro.mp4')
probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=width,height,r_frame_rate,nb_frames,duration','-of','json',str(source)]))['streams'][0]
assert probe['r_frame_rate'] in ('30/1','60/1') and abs(float(probe['duration'])-10)<.01,probe
width,height,count=1288,480,300
api=ctypes.WinDLL('cabinet',use_last_error=True)
for name in ('CreateCompressor','CreateDecompressor'):getattr(api,name).argtypes=[ctypes.c_ulong,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
for name in ('Compress','Decompress'):getattr(api,name).argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
for name in ('CloseCompressor','CloseDecompressor'):getattr(api,name).argtypes=[ctypes.c_void_p]
compressor,decoder=ctypes.c_void_p(),ctypes.c_void_p()
assert api.CreateCompressor(4,None,ctypes.byref(compressor)) and api.CreateDecompressor(4,None,ctypes.byref(decoder))
size=width*height*4;compressed=ctypes.create_string_buffer(size+65536);restored=ctypes.create_string_buffer(size)
process=subprocess.Popen(['ffmpeg','-v','error','-i',str(source),'-vf','fps=30,scale=1280:480:flags=lanczos,pad=1288:480:0:0:color=black','-frames:v',str(count),'-f','rawvideo','-pix_fmt','bgra','pipe:1'],stdout=subprocess.PIPE)
unique=[];lookup={};timeline=[];hashes=[]
try:
    for frame in range(count):
        raw=process.stdout.read(size);assert len(raw)==size
        written=ctypes.c_size_t();assert api.Compress(compressor,raw,size,compressed,len(compressed),ctypes.byref(written))
        block=compressed.raw[:written.value];decoded=ctypes.c_size_t()
        assert api.Decompress(decoder,block,len(block),restored,size,ctypes.byref(decoded)) and decoded.value==size and restored.raw==raw
        if block not in lookup:lookup[block]=len(unique);unique.append(block)
        timeline.append(lookup[block]);hashes.append(hashlib.sha256(raw).hexdigest())
        if frame in (0,150,299):Image.frombytes('RGBA',(width,height),raw,'raw','BGRA').save(args.preview/f'cluster-{frame}.png')
    assert process.wait()==0
finally:
    if process.poll() is None:process.kill();process.wait()
    api.CloseCompressor(compressor);api.CloseDecompressor(decoder)
movie=target/('westernstar_icc5_cluster_intro.bga' if args.westernstar else 'cascadia_icc5_cluster_intro.bga')
movie.write_bytes(struct.pack('<5I',0x33434742,width,height,count,len(unique))+struct.pack(f'<{count}H',*timeline)+struct.pack(f'<{len(unique)}I',*(len(b) for b in unique))+b''.join(unique))
assert movie.stat().st_size<80*1024*1024,'Movie exceeds bounded runtime reader'
report['movie']=dict(source_sha256=digest(source),sha256=digest(movie),width=width,height=height,frames=count,fps=30,play_ms=10000,fade_ms=0,verified_frames=count,frame_sha256=hashes)

def marker(name,w,h,role):
    dest=assets/'material/ui/dashboard/bettergps_overlaynew'/name
    header=bytearray((dest.parent/'phone_layout_marker.dds').read_bytes()[:128]);struct.pack_into('<III',header,12,h,w,w*4)
    pixels=bytearray(w*h*4)
    for i in range(4):pixels[i*4:i*4+4]=bytes((0xa7,0x5b,0x31+role+i,255))
    dest.with_suffix('.dds').write_bytes(header+pixels)
    path='/'+dest.relative_to(assets).as_posix()+'.dds'
    tobj=bytearray((dest.parent/'phone_layout_marker.tobj').read_bytes()[:48]);struct.pack_into('<I',tobj,40,len(path));dest.with_suffix('.tobj').write_bytes(tobj+path.encode())
    dest.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "'+path[:-4]+'.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
    return '/'+dest.relative_to(assets).as_posix()+'.mat'

cluster=marker('cascadia_icc5_cluster_intro',width,height,160)
guard=marker('cascadia_icc5_gps_guard',32,4,161)
for truck in (('westernstar_57x',) if args.westernstar else ('freightliner_cascadia_icc5','freightliner_cascadia_2024')):
    for suffix,name,material,crop in [('', 'bgicc5_intro',cluster,f'left=0 right={1280/1288:.12f} top=0 bottom=1'),('_gps','bgicc5_guard',guard,'left=0.3 right=0.9 top=0.375 bottom=0.875')]:
        relative=Path('ui/dashboard')/(truck+suffix+'.sii')
        original=(args.stock_root/relative).read_text(encoding='utf-8-sig')
        # Preserve the already shipped GPS UI, including its Original-mode
        # content. The stock inventory may contain a preprocessed map copy.
        if suffix:
            original=subprocess.check_output(['git','show','HEAD:BetterGPS/Overlay/BetterGPS_assets/'+relative.as_posix()],cwd=assets.parents[2]).decode('utf-8-sig')
            existing=re.search(r'\nui::text : _nameless\.'+name+r' \{.*?\n\}\n',original,re.S)
            if existing:
                original=original[:existing.start()]+original[existing.end():]
                original=re.sub(r' my_children\[\d+\]: _nameless\.'+name+r'\n','',original,count=1)
                count=re.search(r'my_children: (\d+)\n',original)
                original=original[:count.start()]+f'my_children: {int(count[1])-1}\n'+original[count.end():]
        first=original.split('}',1)[0];children=int(re.search(r'my_children:\s*(\d+)',first)[1]);parent=re.search(r'ui::window\s*:\s*(\S+)\s*\{',original)[1]
        anchor=re.search(r' my_children\['+str(children-1)+r'\]:[^\n]*\n',first)[0]
        ui=original.replace(f'my_children: {children}\n',f'my_children: {children+1}\n',1).replace(anchor,anchor+f' my_children[{children}]: _nameless.{name}\n',1)
        node=f'''\nui::text : _nameless.{name} {{
 text: "<img src={material} {crop} xscale=stretch yscale=stretch>"
 coords_l: 0
 coords_r: 1440
 coords_t: 540
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
        ui=ui.rstrip()[:-1]+node+'}\n';validate_sii_units(ui)
        restored=ui.replace(f' my_children[{children}]: _nameless.{name}\n','',1).replace(f'my_children: {children+1}\n',f'my_children: {children}\n',1).replace(node,'',1)
        assert restored.rstrip()==original.rstrip()
        (assets/relative).parent.mkdir(parents=True,exist_ok=True);(assets/relative).write_text(ui,encoding='utf-8',newline='\n')
        report['native'][relative.as_posix()]=hashlib.sha256(original.encode()).hexdigest()
(target/'import-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'Imported {len(report["artwork"])} textures; {count} lossless frames/30fps, {movie.stat().st_size} bytes; selected stock UIs preserved.')
