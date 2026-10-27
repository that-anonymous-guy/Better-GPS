"""Import supplied T680 gauge DDS files with pixel-exact RGBA verification."""
from pathlib import Path
from PIL import Image
import argparse, hashlib, json
p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('preview',type=Path);p.add_argument('--background-only',action='store_true');p.add_argument('--no-shared-manifest',action='store_true');a=p.parse_args()
target=Path(__file__).resolve().parents[1]/'BetterGPS_assets/material/copilot/t680'
files=[(a.source/'navplushd_gauges_background.dds',(169,48))]
if not a.background_only: files += [(a.source/'gauges'/('navplushd_gauge_'+n+'.dds'),(329,286)) for n in ('oiltemp','trans','boost','dpf','mpg','torque')]
if not a.background_only: files += [(a.source/'gauges/navplushd_needle.dds',(163,75))]
a.preview.mkdir(parents=True,exist_ok=True);report=[]
for source,size in files:
    with Image.open(source) as image:
        rgba=image.convert('RGBA')
        assert rgba.size in ((size,(325,284)) if source.name=='navplushd_gauge_mpg.dds' else (size,)),(source,rgba.size)
    dest=target/source.name;rgba.save(dest,format='DDS')
    with Image.open(dest) as image: assert image.convert('RGBA').tobytes()==rgba.tobytes()
    rgba.save(a.preview/(source.stem+'.png'))
    report.append(dict(name=source.name,size=rgba.size,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
receipt='gauges-background-import.json' if a.background_only else 'gauges-import.json'
(target/receipt).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if not a.no_shared_manifest:
    manifest_path=target/'import-manifest.json';manifest=json.loads(manifest_path.read_text(encoding='utf-8'));manifest['files'].update({r['name']:r['sha256'] for r in report});manifest['gauges_background_manifest' if a.background_only else 'gauges_manifest']=receipt;manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(f'PASS: {len(report)} gauge DDS files imported with exact RGBA pixels.')
