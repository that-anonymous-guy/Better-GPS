"""Lossless T8520 media import and additive native International logo gate."""
import argparse, ctypes, hashlib, json, re, struct, subprocess
from pathlib import Path
from PIL import Image
from import_5700_zonar import append_guard
from validate_sii_units import validate_sii_units

p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--v9',type=Path,required=True);p.add_argument('--stock',type=Path,required=True);p.add_argument('--icons',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);p.add_argument('--preview',type=Path,required=True);a=p.parse_args()
assets=Path(__file__).resolve().parents[1]/'BetterGPS_assets';target=assets/'material/copilot/international';target.mkdir(parents=True,exist_ok=True);a.preview.mkdir(parents=True,exist_ok=True)
digest=lambda b:hashlib.sha256(b).hexdigest();report={'artwork':[],'native':{}}
for source in [a.source/'lt8520_home.dds',*(a.v9/n for n in ('copilot_v9_top.dds','copilot_v9_zoom.dds','copilot_v9_belowfooter.dds','copilot_v9_notrip.dds'))]:
    im=Image.open(source).convert('RGBA');dest=target/source.name;im.save(dest,format='DDS');assert Image.open(dest).convert('RGBA').tobytes()==im.tobytes();im.save(a.preview/(source.stem+'.png'));report['artwork'].append(dict(name=source.name,size=im.size,source_sha256=digest(source.read_bytes()),sha256=digest(dest.read_bytes())))
# Reuse the exact stock logo rectangle; no guessed logo or external artwork.
raw=a.icons.read_bytes();h,w=struct.unpack_from('<2I',raw,12);assert struct.unpack_from('<I',raw,128)[0]==91 and len(raw)==148+w*h*4
icons=Image.frombytes('RGBA',(w,h),raw[148:],'raw','BGRA')
logo=icons.resize((303,333),Image.Resampling.BILINEAR,box=(w*.088,h*.412,w*.36,h*.71));logo.save(target/'native_logo.dds',format='DDS');logo.save(a.preview/'native_logo.png')
# Gate only the changing native info area. Shared gear/odometer and auxiliary
# dashboard screens remain live; no page selector or saved game is mutated.
report['native_logo']=dict(source_sha256=digest(raw),uv=[.088,.412,.36,.71],coords=[249,107,552,440])
card=Image.open(a.reference).convert('RGBA').crop((545,296,902,588));launch=Image.new('RGBA',(1473,800),(0,0,0,255));launch.paste(card,((1473-card.width)//2,(800-card.height)//2));launch.save(target/'copilot_launch.dds',format='DDS')
report['launch_reference_sha256']=digest(a.reference.read_bytes())
# The marker occupies only the info-page rectangle, not the entire 801x800 atlas.
prefix=assets/'material/ui/dashboard/bettergps_overlaynew/international_cluster';mw,mh,role=808,390,164
header=bytearray((prefix.parent/'phone_layout_marker.dds').read_bytes()[:128]);struct.pack_into('<III',header,12,mh,mw,mw*4);pixels=bytearray(mw*mh*4)
for i in range(4):pixels[i*4:i*4+4]=bytes((0xa7,0x5b,0x31+role+i,255))
prefix.with_suffix('.dds').write_bytes(header+pixels);path='/'+prefix.relative_to(assets).as_posix()+'.dds';tobj=bytearray((prefix.parent/'phone_layout_marker.tobj').read_bytes()[:48]);struct.pack_into('<I',tobj,40,len(path));prefix.with_suffix('.tobj').write_bytes(tobj+path.encode());prefix.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "'+path[:-4]+'.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
for truck in ('international_lt','international_lonestar'):
    rel=Path('ui/dashboard')/(truck+'.sii');original=(a.stock/rel).read_text(encoding='utf-8-sig');ui=append_guard(original,'bgintl_intro',prefix.name,f'left=0 right={800/808:.12f} top=0 bottom=1',800,480)
    # Native page content is above the shared odometer/gear strip.
    ui=ui.replace(' coords_t: 480\n coords_b: 0\n area_l: 1\n area_r: 0\n area_t: 0\n area_b: 1\n id: 0\n layer: 127',' coords_t: 480\n coords_b: 90\n area_l: 1\n area_r: 0\n area_t: 0\n area_b: 1\n id: 0\n layer: 127')
    validate_sii_units(ui);(assets/rel).write_text(ui,encoding='utf-8',newline='\n');report['native'][rel.as_posix()]=digest(original.encode())
# Same lossless BGRA/XPRESS-HUFF timeline already used for truck movies.
video=a.source/'international_t8520_intro.mp4';width,height,count=1480,800,301
api=ctypes.WinDLL('cabinet',use_last_error=True)
for name in ('CreateCompressor','CreateDecompressor'):getattr(api,name).argtypes=[ctypes.c_ulong,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
for name in ('Compress','Decompress'):getattr(api,name).argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
for name in ('CloseCompressor','CloseDecompressor'):getattr(api,name).argtypes=[ctypes.c_void_p]
compressor,decoder=ctypes.c_void_p(),ctypes.c_void_p();assert api.CreateCompressor(4,None,ctypes.byref(compressor)) and api.CreateDecompressor(4,None,ctypes.byref(decoder))
size=width*height*4;packed=ctypes.create_string_buffer(size+65536);restored=ctypes.create_string_buffer(size);unique=[];lookup={};timeline=[];hashes=[]
proc=subprocess.Popen(['ffmpeg','-v','error','-i',str(video),'-map','0:v:0','-vf','fps=30,scale=1473:800:flags=lanczos,pad=1480:800:0:0:color=black','-frames:v',str(count),'-f','rawvideo','-pix_fmt','bgra','pipe:1'],stdout=subprocess.PIPE)
try:
    for frame in range(count):
        raw=proc.stdout.read(size);assert len(raw)==size;n=ctypes.c_size_t();assert api.Compress(compressor,raw,size,packed,len(packed),ctypes.byref(n));block=packed.raw[:n.value];decoded=ctypes.c_size_t();assert api.Decompress(decoder,block,len(block),restored,size,ctypes.byref(decoded)) and decoded.value==size and restored.raw==raw
        if block not in lookup:lookup[block]=len(unique);unique.append(block)
        timeline.append(lookup[block]);hashes.append(digest(raw))
        if frame in (0,150,300):Image.frombytes('RGBA',(width,height),raw,'raw','BGRA').save(a.preview/f'intro-{frame}.png')
    assert proc.wait()==0
finally:
    if proc.poll() is None:proc.kill();proc.wait()
    api.CloseCompressor(compressor);api.CloseDecompressor(decoder)
movie=target/'international_t8520_intro.bga';movie.write_bytes(struct.pack('<5I',0x33434742,width,height,count,len(unique))+struct.pack(f'<{count}H',*timeline)+struct.pack(f'<{len(unique)}I',*(len(b) for b in unique))+b''.join(unique));assert movie.stat().st_size<80*1024*1024
report['movie']=dict(source_sha256=digest(video.read_bytes()),sha256=digest(movie.read_bytes()),frames=count,fps=30,frame_sha256=hashes)
(target/'import-manifest.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: lossless DDS + 301 verified movie frames; native logo and both additive cluster gates')
