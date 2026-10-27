"""Import Volvo TomTom artwork/movies and additive native startup guards."""
import argparse,ctypes,hashlib,json,re,struct,subprocess
from pathlib import Path
from PIL import Image
from validate_sii_units import validate_sii_units

parser=argparse.ArgumentParser()
parser.add_argument('--source',type=Path,default=Path('C:/Users/Crym2/Downloads/newgpsoverlay/tomtom/mack_volvo_infotainment'))
parser.add_argument('--stock-root',type=Path,required=True)
parser.add_argument('--preview',type=Path)
args=parser.parse_args()
assets=Path(__file__).resolve().parents[1]/'BetterGPS_assets'
target=assets/'material/tomtom/volvo_infotainment'
target.mkdir(parents=True,exist_ok=True)
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report={'assets':[],'movies':{},'native':{}}
for source in [args.source/'volvo_backtohome_left.dds',*sorted((args.source/'home_screen').glob('volvo*.dds'))]:
    relative=source.relative_to(args.source);destination=target/relative
    destination.parent.mkdir(parents=True,exist_ok=True)
    image=Image.open(source).convert('RGBA');image.save(destination,format='DDS')
    assert Image.open(destination).convert('RGBA').tobytes()==image.tobytes()
    report['assets'].append(dict(path=relative.as_posix(),size=image.size,source_sha256=digest(source),sha256=digest(destination)))
    if args.preview:
        p=args.preview/relative.with_suffix('.png');p.parent.mkdir(parents=True,exist_ok=True);image.save(p)

api=ctypes.WinDLL('cabinet',use_last_error=True)
for name in ('CreateCompressor','CreateDecompressor'):getattr(api,name).argtypes=[ctypes.c_ulong,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
for name in ('Compress','Decompress'):getattr(api,name).argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
for name in ('CloseCompressor','CloseDecompressor'):getattr(api,name).argtypes=[ctypes.c_void_p]
for name,width,height,count in [('vnl2018_cluster_intro',1328,800,241),('vnl2018_infotainment_intro',900,544,122)]:
    source=args.source/(name+'.mp4')
    probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=width,height,r_frame_rate,nb_frames,duration','-of','json',str(source)]))['streams'][0]
    assert probe['r_frame_rate']=='30/1' and int(probe['nb_frames'])==count and (probe['width'],probe['height'])==(1322,800),probe
    compressor,decoder=ctypes.c_void_p(),ctypes.c_void_p()
    assert api.CreateCompressor(4,None,ctypes.byref(compressor)) and api.CreateDecompressor(4,None,ctypes.byref(decoder))
    size=width*height*4;compressed=ctypes.create_string_buffer(size+65536);restored=ctypes.create_string_buffer(size)
    filters=f'pad={width}:{height}:0:0:color=0x0a0a0a' if width==1328 else f'scale={width}:{height}:flags=lanczos'
    process=subprocess.Popen(['ffmpeg','-v','error','-i',str(source),'-vf',filters,'-frames:v',str(count),'-f','rawvideo','-pix_fmt','bgra','pipe:1'],stdout=subprocess.PIPE)
    unique=[];lookup={};timeline=[];hashes=[]
    try:
        for frame in range(count):
            raw=process.stdout.read(size);assert len(raw)==size
            written=ctypes.c_size_t();assert api.Compress(compressor,raw,size,compressed,len(compressed),ctypes.byref(written))
            block=compressed.raw[:written.value];decoded=ctypes.c_size_t()
            assert api.Decompress(decoder,block,len(block),restored,size,ctypes.byref(decoded)) and decoded.value==size and restored.raw==raw
            if block not in lookup:lookup[block]=len(unique);unique.append(block)
            timeline.append(lookup[block]);hashes.append(hashlib.sha256(raw).hexdigest())
            if args.preview and frame in (0,count//2,count-1):Image.frombytes('RGBA',(width,height),raw,'raw','BGRA').save(args.preview/f'{name}-{frame}.png')
        assert process.wait()==0
    finally:
        if process.poll() is None:process.kill();process.wait()
        api.CloseCompressor(compressor);api.CloseDecompressor(decoder)
    movie=target/(name+'.bga');movie.write_bytes(struct.pack('<5I',0x33434742,width,height,count,len(unique))+struct.pack(f'<{count}H',*timeline)+struct.pack(f'<{len(unique)}I',*(len(b) for b in unique))+b''.join(unique))
    report['movies'][name]=dict(source_sha256=digest(source),sha256=digest(movie),width=width,height=height,frames=count,fps=30,play_ms=(count*1000+29)//30,fade_ms=0,verified_frames=count,frame_sha256=hashes)

def marker(name,w,h,role):
    p=assets/'material/ui/dashboard/bettergps_overlaynew'/name
    header=bytearray((p.parent/'phone_layout_marker.dds').read_bytes()[:128]);struct.pack_into('<III',header,12,h,w,w*4)
    pixels=bytearray(w*h*4)
    for i in range(4):pixels[i*4:i*4+4]=bytes((0xa7,0x5b,0x31+role+i,255))
    p.with_suffix('.dds').write_bytes(header+pixels)
    path='/'+p.relative_to(assets).as_posix()+'.dds';tobj=bytearray((p.parent/'phone_layout_marker.tobj').read_bytes()[:48]);struct.pack_into('<I',tobj,40,len(path));p.with_suffix('.tobj').write_bytes(tobj+path.encode())
    p.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "'+path[:-4]+'.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
    return '/'+p.relative_to(assets).as_posix()+'.mat'

def add_overlay(original,name,material,crop):
    first=original.split('}',1)[0];count=int(re.search(r'my_children:\s*(\d+)',first)[1]);parent=re.search(r'ui::window\s*:\s*(\S+)\s*\{',original)[1]
    anchor=re.search(r' my_children\['+str(count-1)+r'\]:[^\n]*\n',first)[0]
    ui=original.replace('my_children: '+str(count)+'\n','my_children: '+str(count+1)+'\n',1).replace(anchor,anchor+f' my_children[{count}]: _nameless.{name}\n',1)
    node=f'''\nui::text : _nameless.{name} {{
 text: "<img src={material} {crop} xscale=stretch yscale=stretch>"
 coords_l: 0
 coords_r: 800
 coords_t: 480
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
    restored=ui.replace(f' my_children[{count}]: _nameless.{name}\n','',1).replace('my_children: '+str(count+1)+'\n','my_children: '+str(count)+'\n',1).replace(node,'',1)
    assert restored.rstrip()==original.rstrip()
    return ui

cluster=marker('volvo18_cluster_intro',1328,800,155)
guard=marker('volvo18_gps_guard',16,4,156)
for truck in ('volvo_vnl2018','volvo_vnr_e'):
    for suffix,name,mat,crop in [('', 'bgv18_intro',cluster,f'left=0 right={1322/1328:.12f} top=0 bottom=1'),('_gps','bgv18_guard',guard,'left=0.34375 right=0.96875 top=0.375 bottom=0.875')]:
        path=Path('ui/dashboard')/(truck+suffix+'.sii')
        original=(args.stock_root/path).read_text(encoding='utf-8-sig') if not suffix or not (assets/path).exists() else (assets/path).read_text(encoding='utf-8-sig')
        if '\nui::text : _nameless.'+name in original:
            original=original.split('\nui::text : _nameless.'+name,1)[0]+'}\n'
            original=re.sub(r' my_children\[\d+\]: _nameless\.'+name+r'\n','',original,count=1)
            m=re.search(r'my_children: (\d+)\n',original);original=original[:m.start()]+f'my_children: {int(m[1])-1}\n'+original[m.end():]
        (assets/path).parent.mkdir(parents=True,exist_ok=True)
        (assets/path).write_text(add_overlay(original,name,mat,crop),encoding='utf-8',newline='\n')
        report['native'][path.as_posix()]=hashlib.sha256(original.encode()).hexdigest()
(target/'import-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'assets':len(report['assets']),'movies':{k:{a:b for a,b in v.items() if a!='frame_sha256'} for k,v in report['movies'].items()},'native':list(report['native'])},indent=2))
