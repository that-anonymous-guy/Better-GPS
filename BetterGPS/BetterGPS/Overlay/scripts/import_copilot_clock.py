"""Import the two T680 Clock strips without changing other artwork."""
from pathlib import Path
from PIL import Image
import argparse, hashlib, json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',type=Path)
a=p.parse_args()
target=Path(__file__).resolve().parents[1]/'BetterGPS_assets/material/copilot/t680'
report=[]
for name,size in [('navplushd_clock_top_time.dds',(267,39)),('navplushd_clock_bottom.dds',(1324,64))]:
    source=a.source/name
    with Image.open(source) as original: rgba=original.convert('RGBA')
    assert rgba.size==size,(name,rgba.size)
    dest=target/name
    rgba.save(dest,format='DDS')
    with Image.open(dest) as decoded: assert decoded.convert('RGBA').tobytes()==rgba.tobytes()
    report.append(dict(name=name,size=size,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
(target/'clock-import.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
manifest_path=target/'import-manifest.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
manifest['files'].update({r['name']:r['sha256'] for r in report})
manifest['clock_manifest']='clock-import.json'
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('PASS: both Clock textures imported with exact RGBA pixels.')
