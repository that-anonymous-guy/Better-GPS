"""Prepare the supplied cluster movie offline; keep native instruments under it."""
import argparse
import hashlib
import ctypes
import json
from pathlib import Path
import struct
import subprocess
from PIL import Image
from validate_sii_units import validate_sii_units

parser=argparse.ArgumentParser()
parser.add_argument('--video',type=Path,required=True)
parser.add_argument('--native-ui',type=Path,required=True)
args=parser.parse_args()
assets=Path(__file__).resolve().parents[1]/'BetterGPS_assets'
movie=assets/'material/copilot/t680/t680_clusterintro.bga'
ui=args.native_ui.read_text(encoding='utf-8-sig')
assert 'coords_r: 1440' in ui and 'coords_t: 540' in ui
assert ui.count('my_children: 5\n')==1 and 'bgcl_intro' not in ui
ui=ui.replace('my_children: 5\n','my_children: 6\n',1)
ui=ui.replace(' my_children[4]: _nameless._.dislplay3\n',' my_children[4]: _nameless._.dislplay3\n my_children[5]: _nameless.bgcl_intro\n',1)
assert 'my_children[5]: _nameless.bgcl_intro' in ui
node='''
ui::text : _nameless.bgcl_intro {
 text: "<img src=/material/ui/dashboard/bettergps_overlaynew/t680_cluster_intro.mat left=0 right=0.9961089494 top=0 bottom=1 xscale=stretch yscale=stretch>"
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
 my_parent: _nameless.wnd
}
'''
assert ui.rstrip().endswith('}')
ui=ui.rstrip()[:-1]+node+'}\n'
validate_sii_units(ui)
ui_path=assets/'ui/dashboard/kenworth_t680_2022.sii'
ui_path.write_text(ui,encoding='utf-8',newline='\n')
prefix=assets/'material/ui/dashboard/bettergps_overlaynew/t680_cluster_intro'
template=prefix.parent/'phone_layout_marker.dds'
header=bytearray(template.read_bytes()[:128])
struct.pack_into('<III',header,12,512,1028,1028*4)
pixels=bytearray(1028*512*4)
for i in range(4):pixels[i*4:i*4+4]=bytes((0xa7,0x5b,0x31+150+i,255))
prefix.with_suffix('.dds').write_bytes(header+pixels)
path='/material/ui/dashboard/bettergps_overlaynew/t680_cluster_intro.dds'
tobj=bytearray((prefix.parent/'phone_layout_marker.tobj').read_bytes()[:48])
struct.pack_into('<I',tobj,40,len(path));prefix.with_suffix('.tobj').write_bytes(tobj+path.encode())
prefix.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "/material/ui/dashboard/bettergps_overlaynew/t680_cluster_intro.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
# Replace the LAST second, not an extra black tail: 270 frames + 1s crossfade.
command=['ffmpeg','-v','error','-i',str(args.video),'-vf','fps=30,scale=1024:512:flags=lanczos','-frames:v','270','-f','rawvideo','-pix_fmt','rgb24','pipe:1']
process=subprocess.Popen(command,stdout=subprocess.PIPE)
unique=[];lookup={};timeline=[]
api=ctypes.WinDLL('cabinet',use_last_error=True)
for name in ['CreateCompressor','CreateDecompressor']:
    getattr(api,name).argtypes=[ctypes.c_ulong,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
for name in ['Compress','Decompress']:
    getattr(api,name).argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
for name in ['CloseCompressor','CloseDecompressor']:getattr(api,name).argtypes=[ctypes.c_void_p]
compressor=ctypes.c_void_p();decoder=ctypes.c_void_p()
assert api.CreateCompressor(4,None,ctypes.byref(compressor))
assert api.CreateDecompressor(4,None,ctypes.byref(decoder))
frame_bytes=1028*512*4
compressed=ctypes.create_string_buffer(frame_bytes+65536);restored=ctypes.create_string_buffer(frame_bytes)
frame_hashes=[];last_fnv=0
try:
    for frame in range(270):
        rgb=process.stdout.read(1024*512*3)
        assert len(rgb)==1024*512*3, f'short frame {frame}'
        visible=Image.frombytes('RGB',(1024,512),rgb).convert('RGBA').tobytes('raw','BGRA')
        # Pad AFTER RGB conversion; a YUV pad filter can tint neutral black.
        padded=bytearray(frame_bytes)
        for y in range(512):padded[y*1028*4:(y+1)*1028*4]=visible[y*1024*4:(y+1)*1024*4]+bytes((10,10,10,255))*4
        raw=bytes(padded)
        # H.264 limited-range conversion rounds the authored RGB10 black to 9.
        # Restore that neutral background to the explicitly requested RGB10.
        raw=raw.replace(bytes((9,9,9,255)),bytes((10,10,10,255)))
        written=ctypes.c_size_t()
        assert api.Compress(compressor,raw,len(raw),compressed,len(compressed),ctypes.byref(written)),ctypes.get_last_error()
        blocks=compressed.raw[:written.value]
        decoded_size=ctypes.c_size_t()
        assert api.Decompress(decoder,blocks,len(blocks),restored,frame_bytes,ctypes.byref(decoded_size)),ctypes.get_last_error()
        assert decoded_size.value==len(raw) and restored.raw==raw, f'lossy frame {frame}'
        frame_hashes.append(hashlib.sha256(raw).hexdigest())
        if frame==269:
            last_fnv=14695981039346656037
            for byte in raw:last_fnv=((last_fnv^byte)*1099511628211)&((1<<64)-1)
        if blocks not in lookup:lookup[blocks]=len(unique);unique.append(blocks)
        timeline.append(lookup[blocks])
    assert process.wait()==0
finally:
    if process.poll() is None:process.kill();process.wait()
    api.CloseCompressor(compressor);api.CloseDecompressor(decoder)
movie.write_bytes(struct.pack('<5I',0x33434742,1028,512,270,len(unique))+struct.pack('<270H',*timeline)+struct.pack(f'<{len(unique)}I',*(len(frame) for frame in unique))+b''.join(unique))
report=dict(source=str(args.video),source_sha256=hashlib.sha256(args.video.read_bytes()).hexdigest(),source_frames=300,source_fps=30,kept_frames=270,crossfade_ms=1000,total_ms=10000,encoded_width=1028,encoded_height=512,unique_frames=len(unique),encoding='BGC3 BGRA8 XPRESS_HUFF lossless',lossless_verified_frames=270,frame_269_fnv1a64=hex(last_fnv),frame_sha256=frame_hashes,bga_sha256=hashlib.sha256(movie.read_bytes()).hexdigest(),native_ui_sha256=hashlib.sha256(args.native_ui.read_bytes()).hexdigest())
(movie.parent/'cluster-import-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({key:value for key,value in report.items() if key!='frame_sha256'},indent=2))
