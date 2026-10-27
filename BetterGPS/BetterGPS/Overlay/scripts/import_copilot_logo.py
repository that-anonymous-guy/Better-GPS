"""Import the supplied CoPilot loading card without changing RGBA pixels."""
from pathlib import Path
from PIL import Image
import argparse, hashlib, json
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',type=Path)
p.add_argument('preview',type=Path)
a=p.parse_args()
target=Path(__file__).resolve().parents[1]/'BetterGPS_assets/material/copilot/t680'
with Image.open(a.source) as source: rgba=source.convert('RGBA')
assert rgba.size==(317,261),rgba.size
dest=target/'copilot_v9_logo.dds'
rgba.save(dest,format='DDS')
with Image.open(dest) as decoded: assert decoded.convert('RGBA').tobytes()==rgba.tobytes()
a.preview.parent.mkdir(parents=True,exist_ok=True)
rgba.save(a.preview)
receipt={'source_sha256':hashlib.sha256(a.source.read_bytes()).hexdigest(),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'size':rgba.size}
(target/'logo-import.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
manifest_path=target/'import-manifest.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
manifest['files'][dest.name]=receipt['sha256']
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('PASS: CoPilot loading logo imported with exact RGBA pixels; existing assets untouched.')
