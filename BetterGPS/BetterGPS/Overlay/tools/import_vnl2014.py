"""Import supplied VNL2014 artwork, lossless movie and additive native guards."""
import argparse,ctypes,hashlib,json,re,shutil,struct,subprocess
from pathlib import Path
from PIL import Image
from validate_sii_units import validate_sii_units

parser=argparse.ArgumentParser()
parser.add_argument('--source',type=Path,default=Path('C:/Users/Crym2/Downloads/newgpsoverlay/garmin'))
parser.add_argument('--stock-root',type=Path,required=True)
parser.add_argument('--preview',type=Path)
args=parser.parse_args()
assets=Path(__file__).resolve().parents[1]/'BetterGPS_assets'
target=assets/'material/garmin'
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
report={'assets':[]}
for folder in ('volvodynafleet','nuvi600'):
    for path in sorted((args.source/folder).glob('*.dds')):
        destination=target/folder/path.name;destination.parent.mkdir(parents=True,exist_ok=True)
        image=Image.open(path).convert('RGBA')
        # These 1181px BC7 inputs cannot be uploaded to D3D11 as-is.
        image.save(destination,format='DDS')
        assert Image.open(destination).convert('RGBA').tobytes()==image.tobytes()
        report['assets'].append(dict(path=str(destination.relative_to(assets)),size=image.size,source_sha256=digest(path),sha256=digest(destination)))
        if args.preview:
            out=args.preview/folder/path.with_suffix('.png').name;out.parent.mkdir(parents=True,exist_ok=True);image.save(out)
source=args.source/'volvodynafleet/vnl2014_cluster.mp4'
probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=width,height,r_frame_rate,nb_frames,duration','-of','json',str(source)]))['streams'][0]
assert probe['width']==1124 and probe['height']==700 and probe['r_frame_rate']=='30/1' and int(probe['nb_frames'])==180
width,height,count=1128,700,180
frame_bytes=width*height*4
api=ctypes.WinDLL('cabinet',use_last_error=True)
for name in ('CreateCompressor','CreateDecompressor'):getattr(api,name).argtypes=[ctypes.c_ulong,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
for name in ('Compress','Decompress'):getattr(api,name).argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
for name in ('CloseCompressor','CloseDecompressor'):getattr(api,name).argtypes=[ctypes.c_void_p]
compressor,decoder=ctypes.c_void_p(),ctypes.c_void_p()
assert api.CreateCompressor(4,None,ctypes.byref(compressor)) and api.CreateDecompressor(4,None,ctypes.byref(decoder))
compressed=ctypes.create_string_buffer(frame_bytes+65536);restored=ctypes.create_string_buffer(frame_bytes)
process=subprocess.Popen(['ffmpeg','-v','error','-i',str(source),'-frames:v',str(count),'-f','rawvideo','-pix_fmt','bgra','pipe:1'],stdout=subprocess.PIPE)
unique=[];lookup={};timeline=[];hashes=[]
try:
    for frame in range(count):
        visible=process.stdout.read(1124*700*4);assert len(visible)==1124*700*4
        raw=b''.join(visible[y*1124*4:(y+1)*1124*4]+bytes((10,10,10,255))*4 for y in range(height))
        raw=raw.replace(bytes((9,9,9,255)),bytes((10,10,10,255)))
        written=ctypes.c_size_t();assert api.Compress(compressor,raw,len(raw),compressed,len(compressed),ctypes.byref(written))
        block=compressed.raw[:written.value];size=ctypes.c_size_t();assert api.Decompress(decoder,block,len(block),restored,frame_bytes,ctypes.byref(size)) and size.value==frame_bytes and restored.raw==raw
        if block not in lookup:lookup[block]=len(unique);unique.append(block)
        timeline.append(lookup[block]);hashes.append(hashlib.sha256(raw).hexdigest())
        if args.preview and frame in (0,90,179):Image.frombytes('RGBA',(width,height),raw,'raw','BGRA').save(args.preview/f'cluster-{frame}.png')
    assert process.wait()==0
finally:
    if process.poll() is None:process.kill();process.wait()
    api.CloseCompressor(compressor);api.CloseDecompressor(decoder)
movie=target/'volvodynafleet/vnl2014_cluster.bga'
movie.write_bytes(struct.pack('<5I',0x33434742,width,height,count,len(unique))+struct.pack('<180H',*timeline)+struct.pack(f'<{len(unique)}I',*(len(b) for b in unique))+b''.join(unique))
report['movie']=dict(source_sha256=digest(source),sha256=digest(movie),frames=count,fps=30,width=width,height=height,unique_frames=len(unique),verified_frames=count,frame_sha256=hashes,play_ms=6000,live_fade_ms=500)

def marker(name,w,h,role):
    prefix=assets/'material/ui/dashboard/bettergps_overlaynew'/name
    header=bytearray((prefix.parent/'phone_layout_marker.dds').read_bytes()[:128]);struct.pack_into('<III',header,12,h,w,w*4)
    pixels=bytearray(w*h*4)
    for i in range(4):pixels[i*4:i*4+4]=bytes((0xa7,0x5b,0x31+role+i,255))
    prefix.with_suffix('.dds').write_bytes(header+pixels)
    path='/'+prefix.relative_to(assets).as_posix()+'.dds';tobj=bytearray((prefix.parent/'phone_layout_marker.tobj').read_bytes()[:48]);struct.pack_into('<I',tobj,40,len(path));prefix.with_suffix('.tobj').write_bytes(tobj+path.encode())
    prefix.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "'+path[:-4]+'.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
    return '/'+prefix.relative_to(assets).as_posix()+'.mat'

def add_overlay(original,name,material,w,h,crop,bottom=0):
    first=original.split('}',1)[0];count=int(re.search(r'my_children:\s*(\d+)',first)[1]);parent=re.search(r'ui::window\s*:\s*(\S+)\s*\{',original)[1]
    anchor=re.search(r' my_children\['+str(count-1)+r'\]:[^\n]*\n',first)[0]
    ui=original.replace('my_children: '+str(count)+'\n','my_children: '+str(count+1)+'\n',1).replace(anchor,anchor+f' my_children[{count}]: _nameless.{name}\n',1)
    node=f'''\nui::text : _nameless.{name} {{
 text: "<img src={material} {crop} xscale=stretch yscale=stretch>"
 coords_l: 0
 coords_r: {w}
 coords_t: {bottom+h}
 coords_b: {bottom}
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

original=(args.stock_root/'ui/dashboard/volvo_vnl.sii').read_text(encoding='utf-8-sig')
material=marker('vnl14_cluster_intro',1128,700,153)
# SCS UI coordinates start at the BOTTOM of the 800x800 atlas. The visible
# LCD occupies the TOP 600 rows: y=200..800, not y=0..600.
(assets/'ui/dashboard/volvo_vnl.sii').write_text(add_overlay(original,'bgvnl_intro',material,800,600,f'left=0 right={1124/1128:.12f} top=0 bottom=1',bottom=200),encoding='utf-8',newline='\n')
report['native_cluster_sha256']=hashlib.sha256(original.encode()).hexdigest()
guard=assets/'ui/gmaps_volvovnl_gps.sii';original=guard.read_text(encoding='utf-8-sig')
if '\nui::text : _nameless.bgvnl_guard' in original:
    original=original.split('\nui::text : _nameless.bgvnl_guard',1)[0]+'}\n';original=original.replace(' my_children[2]: _nameless.bgvnl_guard\n','',1).replace('my_children: 3\n','my_children: 2\n',1)
material=marker('vnl14_gps_guard',12,4,154)
guard.write_text(add_overlay(original,'bgvnl_guard',material,256,256,'left=0.375 right=0.958333333333 top=0.375 bottom=0.875'),encoding='utf-8',newline='\n')
(target/'volvodynafleet/import-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v if k!='movie' else {a:b for a,b in v.items() if a!='frame_sha256'} for k,v in report.items()},indent=2))
