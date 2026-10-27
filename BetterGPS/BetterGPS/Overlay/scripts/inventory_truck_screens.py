"""Measure extracted stock ATS dynamic screen meshes; never modify game files.

Input: AssetInspector --screen-inventory output. Reports UI/render dimensions
separately from UV exposure and physical triangle aspect; no live-RT claims.
"""
from pathlib import Path, PurePosixPath
import argparse, csv, json, math, re, struct

NAMES={'freightliner.cascadia2019':'Freightliner Cascadia 2019','freightliner.cascadia2024':'Freightliner Cascadia 2024',
 'intnational.9900i':'International 9900i','intnational.lonestar':'International LoneStar','intnational.lt':'International LT',
 'kenworth.t680':'Kenworth T680 classique','kenworth.t680_2022':'Kenworth T680 nouvelle génération (ID 2022)',
 'kenworth.w900':'Kenworth W900','mack.anthem':'Mack Anthem','mack.pinnacle':'Mack Pinnacle',
 'peterbilt.389':'Peterbilt 389','peterbilt.579':'Peterbilt 579','volvo.vnl':'Volvo VNL ancien',
 'volvo.vnl2018':'Volvo VNL 2018','volvo.vnl2025':'Volvo VNL 2025','volvo.vnr_e':'Volvo VNR Electric',
 'westernstar.49x':'Western Star 49X','westernstar.5700xe':'Western Star 5700XE','westernstar.57x':'Western Star 57X'}

def u32(data, offset): return struct.unpack_from('<I', data, offset)[0]
def floats(data, offset, count): return struct.unpack_from('<'+'f'*count, data, offset)

def mesh_pieces(data, material):
    assert data[:4] == b'\x15gmP', 'Unsupported PMG; do not guess offsets'
    start = u32(data, 84)
    for i in range(u32(data, 4)):
        at = start+i*100
        if u32(data, at+16) != material: continue
        indices, vertices = struct.unpack_from('<II', data, at)
        stride, pos, _, uv = struct.unpack_from('<IIII', data, at+60)
        assert stride > 0 and pos < len(data) and uv < len(data)
        coords = [floats(data, uv+v*stride, 2) for v in range(vertices)]
        points = [floats(data, pos+v*stride, 3) for v in range(vertices)]
        idx = struct.unpack_from('<'+'H'*indices, data, u32(data, at+96))
        tiles={}
        for n in range(0, len(idx)-2, 3):
            tri=idx[n:n+3]
            tile=tuple(math.floor(sum(coords[v][k] for v in tri)/3+1e-7) for k in range(2))
            tiles.setdefault(tile,[]).append(tri)
        # A material piece may contain several separate atlas tiles/screens.
        # Their union is NOT one enormous screen (e.g. 5700XE and 57X).
        for tile,triangles in tiles.items():
            used=set(v for tri in triangles for v in tri)
            left,right=min(coords[v][0] for v in used),max(coords[v][0] for v in used)
            top,bottom=min(coords[v][1] for v in used),max(coords[v][1] for v in used)
            if right-left < 1e-6 or bottom-top < 1e-6: continue
            yield dict(piece=i,vertices=len(used),tile=tile,uv=[left,top,right,bottom])

def run(root, output):
    data = json.loads((root/'inventory.json').read_text())
    extracted = root/'extracted'
    def read(path): return (extracted/path.lstrip('/')).read_text(encoding='utf-8-sig')
    def expand(path, seen=None):
        seen = set() if seen is None else seen
        if path in seen: return ''
        seen.add(path)
        try: text = read(path)
        except FileNotFoundError: return ''
        def include(m):
            inc = m[1] if m[1].startswith('/') else str(PurePosixPath(path).parent/m[1])
            parts=[]
            for part in inc.split('/'):
                if part=='..': parts.pop()
                elif part and part!='.': parts.append(part)
            return expand('/'+'/'.join(parts), seen)
        return re.sub(r'@include\s+"([^"]+)"', include, text)
    records=[]
    for truck in data['trucks']:
        for definition in truck['definitions']:
            if not definition.endswith('.sii') or '/interior/' not in definition: continue
            text = expand(definition)
            models = re.findall(r'(?:^|\s)model\s*:\s*"([^"]+\.pmd)"', text)
            if not models: continue
            for material in truck['materials']:
                if material['model'] not in models: continue
                kind = 'GPS' if material['texture'].endswith('/gps.tobj') else 'Dashboard'
                prefix = 'gps' if kind=='GPS' else 'dashboard'
                paths = re.findall(r'\b'+prefix+r'_path\s*:\s*"([^"]+)"', text)
                if not paths: continue
                size = re.findall(r'\b'+prefix+r'_drawable_size\s*:\s*\(\s*(\d+)\s*,\s*(\d+)\s*\)', text)
                rw,rh = map(int,size[-1]) if size else (256,256)
                ui = paths[-1]
                try:
                    window=read(ui).split('}',1)[0]
                    limits={k:float(v) for k,v in re.findall(r'\bcoords_([lrtb]):\s*([-\d.]+)',window)}
                    ui_size=[limits['r']-limits['l'],limits['t']-limits['b']]
                except FileNotFoundError: ui_size=None
                pmg=(extracted/material['pmg'].lstrip('/')).read_bytes()
                for piece in mesh_pieces(pmg,material['material']):
                    left,top,right,bottom=piece['uv'];vw,vh=(right-left)*rw,(bottom-top)*rh
                    records.append(dict(truck=truck['id'],kind=kind,definition=definition,model=material['model'],
                        ui=ui,render=[rw,rh],uiSize=ui_size,sourceTexture=[material['width'],material['height']],
                        texture=material['texture'],visible=[vw,vh],aspect=vw/vh,**piece))
    for r in records:
        shared=r['texture'] in ('/vehicle/truck/share/gps.tobj','/vehicle/truck/share/dashboard.tobj')
        r['regionType']='dynamic screen' if shared and min(r['tile'])>=2 else 'auxiliary atlas' if shared else 'static dial artwork'
    output.mkdir(parents=True,exist_ok=True)
    (output/'screens-full.json').write_text(json.dumps(dict(scope=data['scope'],screens=records,errors=data['errors']),indent=2))
    unique={}
    for r in records:
        key=(r['truck'],r['kind'],r['ui'],tuple(r['render']),tuple(round(v,1) for v in r['visible']))
        if key not in unique: unique[key]=dict(r,variants=[])
        unique[key]['variants'].append(r['definition'])
    rows=list(unique.values())
    (output/'screens.json').write_text(json.dumps(rows,indent=2))
    with (output/'screens.csv').open('w',newline='',encoding='utf-8-sig') as f:
        writer=csv.writer(f);writer.writerow(['Truck','Producer (not physical role)','Region type','UI','Render W','Render H','UV exposure W','UV exposure H','UV aspect (NOT physical format)','Variants'])
        for r in rows:writer.writerow([r['truck'],r['kind'],r['regionType'],r['ui'],*r['render'],*(round(v,3) for v in r['visible']),round(r['aspect'],6),len(set(r['variants']))])
    # UV exposure is not physical format. Audit connected XYZ surfaces and
    # native UI/material identity before publishing any format guide.
    from audit_truck_display_roles import run as audit_roles
    audited=audit_roles(root,output,output/'roles')
    from audit_truck_display_roles import markdown as role_markdown
    (output/'truck-display-formats.md').write_text(role_markdown(audited),encoding='utf-8')
    for r in rows:print(f"{r['truck']:28} {r['kind']:10} {r['render']} {r['visible'][0]:.2f} x {r['visible'][1]:.2f} {r['aspect']:.5f}:1 piece={r['piece']} {r['ui']}")
    print(f'{len(rows)} distinct measured regions; {len(records)} interior/mesh records')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('root',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();run(args.root,args.output)
