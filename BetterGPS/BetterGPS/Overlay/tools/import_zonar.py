"""Import supplied ICUC/Zonar DDS pixels and overlay only the central cluster LCD."""
import argparse, hashlib, json, re, struct
from pathlib import Path
from PIL import Image
from validate_sii_units import validate_sii_units

p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--stock-root',type=Path,required=True);p.add_argument('--preview',type=Path,required=True);p.add_argument('--westernstar',action='store_true');args=p.parse_args()
assets=Path(__file__).resolve().parents[1]/'BetterGPS_assets';target=assets/('material/copilot/westernstar' if args.westernstar else 'material/copilot/cascadia/icuc');target.mkdir(parents=True,exist_ok=True);args.preview.mkdir(parents=True,exist_ok=True)
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report={'artwork':[],'native':{},'central_cluster_ui_rect':[0,0,600,800]}
for source in sorted(args.source.glob('*.dds')):
    image=Image.open(source).convert('RGBA');destination=target/source.name;image.save(destination,format='DDS')
    assert Image.open(destination).convert('RGBA').tobytes()==image.tobytes()
    image.save(args.preview/(source.stem+'.png'));report['artwork'].append(dict(path=source.name,size=image.size,source_sha256=digest(source),sha256=digest(destination)))
name='cascadia_icuc_cluster';prefix=assets/'material/ui/dashboard/bettergps_overlaynew'/name;width,height,role=456,600,162
header=bytearray((prefix.parent/'phone_layout_marker.dds').read_bytes()[:128]);struct.pack_into('<III',header,12,height,width,width*4);pixels=bytearray(width*height*4)
for i in range(4):pixels[i*4:i*4+4]=bytes((0xa7,0x5b,0x31+role+i,255))
prefix.with_suffix('.dds').write_bytes(header+pixels);path='/'+prefix.relative_to(assets).as_posix()+'.dds';tobj=bytearray((prefix.parent/'phone_layout_marker.tobj').read_bytes()[:48]);struct.pack_into('<I',tobj,40,len(path));prefix.with_suffix('.tobj').write_bytes(tobj+path.encode());prefix.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "'+path[:-4]+'.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
for name in (('westernstar_49x',) if args.westernstar else ('freightliner_cascadia_2019','freightliner_cascadia_2024_gauge')):
    relative=Path('ui/dashboard')/(name+'.sii');original=(args.stock_root/relative).read_text(encoding='utf-8-sig');first=original.split('}',1)[0];count=int(re.search(r'my_children:\s*(\d+)',first)[1]);parent=re.search(r'ui::window\s*:\s*(\S+)\s*\{',original)[1]
    assert 'coords_r: 800' in first and 'coords_t: 800' in first
    anchor=re.search(r' my_children\['+str(count-1)+r'\]:[^\n]*\n',first)[0];ui=original.replace(f'my_children: {count}\n',f'my_children: {count+1}\n',1).replace(anchor,anchor+f' my_children[{count}]: _nameless.bgicuc_intro\n',1)
    node=f'''\nui::text : _nameless.bgicuc_intro {{
 text: "<img src=/material/ui/dashboard/bettergps_overlaynew/cascadia_icuc_cluster.mat left=0 right={450/456:.12f} top=0 bottom=1 xscale=stretch yscale=stretch>"
 coords_l: 0
 coords_r: 600
 coords_t: 800
 coords_b: 0
 area_l: 1
 area_r: 0
 area_t: 0
 area_b: 1
 id: 0
 layer: 127
 tab: -1
 pointer: -1
 my_parent: {parent}
}}
'''
    ui=ui.rstrip()[:-1]+node+'}\n';validate_sii_units(ui)
    restored=ui.replace(f' my_children[{count}]: _nameless.bgicuc_intro\n','',1).replace(f'my_children: {count+1}\n',f'my_children: {count}\n',1).replace(node,'',1);assert restored.rstrip()==original.rstrip()
    (assets/relative).write_text(ui,encoding='utf-8',newline='\n');report['native'][relative.as_posix()]=hashlib.sha256(original.encode()).hexdigest()
(target/'import-manifest.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: DDS pixel/alpha-preserved; selected central cluster LCDs covered, auxiliary displays unchanged')
