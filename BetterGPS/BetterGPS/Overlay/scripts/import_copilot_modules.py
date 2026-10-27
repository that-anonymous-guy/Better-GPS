"""Import only the modular T680 NavPlus HD DDS artwork; never re-encode intros."""
from pathlib import Path
import argparse,hashlib,json
from PIL import Image

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--t680',type=Path,required=True)
p.add_argument('--v9',type=Path,required=True)
p.add_argument('--preview',type=Path,required=True)
a=p.parse_args()
target=Path(__file__).resolve().parents[1]/'BetterGPS_assets/material/copilot/t680'
target.mkdir(parents=True,exist_ok=True);a.preview.mkdir(parents=True,exist_ok=True)
report=[]
for source,size in [(a.t680/'navplushd_top_5by3.dds',(1324,92)),(a.t680/'navplushd_bottom_5by3.dds',(1324,90)),(a.t680/'navplushd_gradient_background.dds',(1324,800)),(a.t680/'navplushd_homescreen_toplayer.dds',(1324,641)),(a.v9/'copilot_v9_zoom.dds',(1276,60)),(a.v9/'copilot_v9_belowfooter.dds',(1324,152)),(a.v9/'copilot_v9_notrip.dds',(167,82))]:
    with Image.open(source) as original:
        image=original.convert('RGBA');assert image.size==size,(source,image.size)
    dest=target/source.name;image.save(dest,format='DDS')
    with Image.open(dest) as decoded:assert decoded.convert('RGBA').tobytes()==image.tobytes()
    image.save(a.preview/(source.stem+'.png'))
    report.append(dict(name=source.name,size=size,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
(target/'modules-import-manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
manifest_path=target/'import-manifest.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
manifest['files'].update({row['name']:row['sha256'] for row in report})
manifest['modular_artwork_manifest']='modules-import-manifest.json'
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('PASS: seven modular DDS assets imported with exact RGBA pixels; movies untouched.')
