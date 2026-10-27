"""Import only the three supplied T680 camera-page DDS assets, preserving RGBA."""
from pathlib import Path
from PIL import Image
import argparse, hashlib, json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',type=Path)
p.add_argument('preview',type=Path)
a=p.parse_args()
target=Path(__file__).resolve().parents[1]/'BetterGPS_assets/material/copilot/t680'
a.preview.mkdir(parents=True,exist_ok=True)
report=[]
for name,size in [('navplushd_simple_background.dds',(1324,800)),('navplushd_camera_icon.dds',(253,488)),('navplushd_camera_noavailable.dds',(547,114))]:
    source=a.source/name
    with Image.open(source) as original: rgba=original.convert('RGBA')
    assert rgba.size==size,(name,rgba.size)
    if 'background' in name: assert rgba.getextrema()==((10,10),(10,10),(10,10),(255,255))
    dest=target/name
    rgba.save(dest,format='DDS')
    with Image.open(dest) as decoded: assert decoded.convert('RGBA').tobytes()==rgba.tobytes()
    rgba.save(a.preview/(Path(name).stem+'.png'))
    report.append(dict(name=name,size=size,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
(target/'camera-import.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
manifest_path=target/'import-manifest.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
manifest['files'].update({r['name']:r['sha256'] for r in report})
manifest['camera_manifest']='camera-import.json'
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('PASS: camera background RGB(10,10,10) and selector imported with exact RGBA pixels.')
