"""Import registered NavPlus Home content and independent page indicators."""
from pathlib import Path
from PIL import Image
import argparse, hashlib, json
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',type=Path);p.add_argument('preview',type=Path)
p.add_argument('--foreground-only',action='store_true',help='Keep the installed/repository gradient unchanged')
p.add_argument('--no-shared-manifest',action='store_true',help='Preserve unrelated pending import-manifest edits')
a=p.parse_args();target=Path(__file__).resolve().parents[1]/'BetterGPS_assets/material/copilot/t680';a.preview.mkdir(parents=True,exist_ok=True)
assets=[('navplushd_homescreen_toplayer.dds',(1244,586),73,126),('navplushd_page_full.dds',(34,34),None,84),('navplushd_page_empty.dds',(34,34),None,84)]
if not a.foreground_only:assets.insert(0,('navplushd_gradient_background.dds',(1324,800),0,0))
report=[]
for name,size,x,y in assets:
 source=a.source/name
 with Image.open(source) as original:rgba=original.convert('RGBA')
 if rgba.size!=size:raise ValueError(f'{name}: expected {size}, received {rgba.size}')
 dest=target/name;rgba.save(dest,format='DDS')
 with Image.open(dest) as decoded:assert decoded.convert('RGBA').tobytes()==rgba.tobytes()
 rgba.save(a.preview/(Path(name).stem+'.png'))
 report.append(dict(name=name,size=size,source_x=x,source_y=y,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
receipt=target/'home-layers-import.json';old=json.loads(receipt.read_text(encoding='utf-8')) if receipt.exists() else [];changed={r['name'] for r in report};merged=[r for r in old if r['name'] not in changed]+report;receipt.write_text(json.dumps(merged,indent=2)+'\n',encoding='utf-8')
if not a.no_shared_manifest:
 manifest_path=target/'import-manifest.json';manifest=json.loads(manifest_path.read_text(encoding='utf-8'));manifest['files'].update({r['name']:r['sha256'] for r in report});manifest['home_layers_manifest']='home-layers-import.json';manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('PASS: Home foreground and page indicators imported with exact RGBA pixels; source registration preserved.')
