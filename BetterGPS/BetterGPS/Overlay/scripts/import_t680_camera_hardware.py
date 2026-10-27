"""Import the approved optional T680 camera hardware into plugin-owned VFS assets.

The source ZIP is preserved. No profile, save, global mirror setting or installed
mod is changed. Only explicit assets requested by the user are imported.
"""
import argparse
import hashlib
import json
import struct
from io import BytesIO
from pathlib import Path, PurePosixPath
from zipfile import ZipFile
from PIL import Image


def sha(data):
    return hashlib.sha256(data).hexdigest()


def rgba(source):
    data = bytearray(source.read_bytes())
    if data[84:88] == b'DX10':
        value = struct.unpack_from('<I', data, 128)[0]
        struct.pack_into('<I', data, 128, {29:28, 72:71, 75:74, 78:77, 99:98}.get(value,value))
    return Image.open(BytesIO(data)).convert('RGBA')


def run(source, assets, artwork):
    root = assets / 'camera-hardware'
    entries, mounts = [], set()
    with ZipFile(source) as archive:
        for info in archive.infolist():
            if info.is_dir():
                continue
            path = PurePosixPath(info.filename)
            if path.is_absolute() or '..' in path.parts:
                raise ValueError('Unsafe archive entry: '+info.filename)
            if path.parts[0] not in {'automat','def','material','vehicle'}:
                continue
            data = archive.read(info)
            target = root / path.as_posix()
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(data)
            mounts.add(path.parent.as_posix())
            entries.append(dict(path=path.as_posix(),bytes=len(data),sha256=sha(data)))
    expected = [f'vehicle/truck/kenworth_t680_2022/cabin/{name}.pmg' for name in ['dc','s52m','s76h']]
    assert all((root/p).is_file() for p in expected)
    assert len([e for e in entries if '/accessory/bumper/' in e['path'] and e['path'].endswith('.pmg')]) == 8
    manifest = dict(source_sha256=sha(source.read_bytes()),sourceName=source.name,files=entries,
                    mountDirectories=sorted(mounts),optional=True,defaultAccessoriesAdded=False)
    root.mkdir(parents=True,exist_ok=True)
    (root/'hardware-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    (root/'mount-directories.txt').write_text('\n'.join(sorted(mounts))+'\n',encoding='utf-8')
    report=[]
    target=assets/'material/copilot/t680'
    paths=[artwork/'navplushd_camera_icon.dds']+sorted((artwork/'camera icons').glob('*.dds'))
    for path in paths:
        image=rgba(path)
        dest=target/path.name
        image.save(dest,format='DDS')
        assert rgba(dest).tobytes()==image.tobytes()
        report.append(dict(name=path.name,size=image.size,source_sha256=sha(path.read_bytes()),sha256=sha(dest.read_bytes())))
    (target/'camera-selector-import.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Imported {len(entries)} hardware files, {len(mounts)} exact VFS directories and {len(report)} selector assets; source ZIP preserved.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zip',type=Path,required=True)
    parser.add_argument('--assets',type=Path,required=True)
    parser.add_argument('--artwork',type=Path,required=True)
    args=parser.parse_args()
    run(args.zip,args.assets,args.artwork)
