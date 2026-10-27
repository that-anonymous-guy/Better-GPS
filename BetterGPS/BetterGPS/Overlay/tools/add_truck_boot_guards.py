"""Add transparent guards to supported original GPS UIs, preserving native trees."""
import argparse,hashlib,json,re,struct
from pathlib import Path
from validate_sii_units import validate_sii_units

parser=argparse.ArgumentParser();parser.add_argument('--stock-root',type=Path,required=True);args=parser.parse_args()
assets=Path(__file__).resolve().parents[1]/'BetterGPS_assets'
p=assets/'material/ui/dashboard/bettergps_overlaynew/truck_gps_guard'
header=bytearray((p.parent/'phone_layout_marker.dds').read_bytes()[:128]);struct.pack_into('<III',header,12,4,20,80)
pixels=bytearray(20*4*4)
for i in range(4):pixels[i*4:i*4+4]=bytes((0xa7,0x5b,0x31+157+i,255))
p.with_suffix('.dds').write_bytes(header+pixels)
path='/'+p.relative_to(assets).as_posix()+'.dds';tobj=bytearray((p.parent/'phone_layout_marker.tobj').read_bytes()[:48]);struct.pack_into('<I',tobj,40,len(path));p.with_suffix('.tobj').write_bytes(tobj+path.encode())
p.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "'+path[:-4]+'.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
report={}
for name in ['ui/gps.sii',*[f'ui/gmaps_{truck}_gps.sii' for truck in ['kenworthw900','kenwortht680','peterbilt389','peterbilt579']],'ui/dashboard/kenworth_t680_2022_gps.sii']:
    target=assets/name;original=(target if target.exists() else args.stock_root/name).read_text(encoding='utf-8-sig')
    if '\nui::text : _nameless.bgtrk_guard' in original:
        original=original.split('\nui::text : _nameless.bgtrk_guard',1)[0]+'}\n'
        original=re.sub(r' my_children\[\d+\]: _nameless.bgtrk_guard\n','',original,count=1)
        m=re.search(r'my_children: (\d+)\n',original);original=original[:m.start()]+f'my_children: {int(m[1])-1}\n'+original[m.end():]
    first=original.split('}',1)[0];count=int(re.search(r'my_children:\s*(\d+)',first)[1]);parent=re.search(r'ui::window\s*:\s*(\S+)\s*\{',first)[1]
    coord=lambda name:int(re.search(name+r':\s*(\d+)',first)[1])
    left,right,top,bottom=[coord('coords_'+n) for n in ['l','r','t','b']]
    anchor=re.search(r' my_children\['+str(count-1)+r'\]:[^\n]*\n',first)[0]
    ui=original.replace(f'my_children: {count}\n',f'my_children: {count+1}\n',1).replace(anchor,anchor+f' my_children[{count}]: _nameless.bgtrk_guard\n',1)
    node=f'''\nui::text : _nameless.bgtrk_guard {{
 text: "<img src=/material/ui/dashboard/bettergps_overlaynew/truck_gps_guard.mat left=0.275 right=0.975 top=0.375 bottom=0.875 xscale=stretch yscale=stretch>"
 coords_l: {left}
 coords_r: {right}
 coords_t: {top}
 coords_b: {bottom}
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
    restored=ui.replace(f' my_children[{count}]: _nameless.bgtrk_guard\n','',1).replace(f'my_children: {count+1}\n',f'my_children: {count}\n',1).replace(node,'',1)
    assert restored.rstrip()==original.rstrip(),'Native tree changed: '+name
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(ui,encoding='utf-8',newline='\n')
    report[name]=dict(original_sha256=hashlib.sha256(original.encode()).hexdigest(),ui_rect=[left,bottom,right,top],marker_role=157)
(p.parent/'truck-guards-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
