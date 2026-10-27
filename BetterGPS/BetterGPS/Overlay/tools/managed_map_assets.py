"""Generate/check the native material graph for every configured managed slot.

The C++ DeviceLimits.h is the sole capacity definition. Checks follow MAT ->
TOBJ -> DDS references; existence of a generated overlay alone is insufficient.
"""
import argparse
from pathlib import Path
import re
import struct

OVERLAY = Path(__file__).resolve().parents[1]

def capacity():
    return int(re.search(r'managedDeviceCount\s*=\s*(\d+)', (OVERLAY/'DeviceLimits.h').read_text())[1])

def graph(root, count):
    prefix = 'material/ui/dashboard/'
    for slot in range(count):
        yield prefix+f'bettergps_map_device_{slot}.mat', prefix+f'bettergps_map_device_{slot}.tobj'
        yield prefix+f'bettergps_map_device_{slot}.tobj', prefix+f'bettergps_map_device_{slot}.dds'
        yield prefix+f'bettergps_map_device_{slot}.dds', None
        yield prefix+f'bettergps_device_output_{slot}.mat', prefix+f'bettergps_device_output_{slot}.tobj'
        yield prefix+f'bettergps_device_output_{slot}.tobj', prefix+'bettergps_map.dds'

def validate(root, count=None):
    root=Path(root);count=capacity() if count is None else count
    for relative, target in graph(root,count):
        path=root/relative
        if not path.is_file(): raise RuntimeError(f'Missing managed map asset: {relative}')
        data=path.read_bytes()
        if path.suffix=='.mat':
            sources=re.findall(r'source\s*:\s*"([^"]+)"', data.decode('utf-8-sig'))
            if sources!=['/'+target]: raise RuntimeError(f'Invalid material source: {relative}')
        elif path.suffix=='.tobj':
            if len(data)<48 or struct.unpack_from('<I',data,40)[0]!=len(data)-48 or data[48:].decode()!=('/'+target):
                raise RuntimeError(f'Invalid TOBJ reference/length: {relative}')
        elif len(data)<128 or data[:4]!=b'DDS ':
            raise RuntimeError(f'Invalid placeholder DDS: {relative}')
        if target and not (root/target).is_file(): raise RuntimeError(f'Dangling asset reference: {relative} -> {target}')
    shared=(root/'material/ui/dashboard/bettergps_map.dds').read_bytes()
    if len(shared)<128 or shared[:4]!=b'DDS ': raise RuntimeError('Invalid shared output placeholder')
    return count*5

def generate(root):
    root=Path(root);folder=root/'material/ui/dashboard';count=capacity()
    # Preserve all existing authored slots byte-for-byte; never replace or
    # silently repair a damaged existing file. Validation will name it instead.
    template=(folder/'bettergps_map_device_0.dds').read_bytes()
    for relative,target in graph(root,count):
        path=root/relative
        if path.exists(): continue
        if path.suffix=='.dds': data=template
        elif path.suffix=='.mat':
            extra=' u_address : clamp v_address : clamp' if 'device_output_' in path.name else ''
            data=('effect : "ui.rfx" { texture : "texture" { source : "/'+target+'"'+extra+' mip_filter : none } }\n').encode()
        else:
            data=bytearray((folder/'bettergps_map_device_0.tobj').read_bytes()[:48]);reference=('/'+target).encode()
            struct.pack_into('<I',data,40,len(reference));data+=reference
        path.write_bytes(data)
    return validate(root,count)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--generate',action='store_true');p.add_argument('--root',type=Path,default=OVERLAY/'BetterGPS_assets');args=p.parse_args()
    files=generate(args.root) if args.generate else validate(args.root)
    print(f'PASS: {capacity()} managed slots, {files} native files, complete MAT -> TOBJ -> DDS graph: {args.root}')
