"""Import the 5700XE Zonar artwork losslessly and append native startup guards."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from PIL import Image
from validate_sii_units import validate_sii_units


def append_guard(original, token, material, crop, width, height):
    first = original.split('}', 1)[0]
    count = int(re.search(r'my_children:\s*(\d+)', first)[1])
    parent = re.search(r'ui::window\s*:\s*(\S+)\s*\{', first)[1]
    anchor = re.search(r' my_children\[' + str(count-1) + r'\]:[^\n]*\n', first)[0]
    ui = original.replace(f'my_children: {count}\n', f'my_children: {count+1}\n', 1)
    child = f' my_children[{count}]: _nameless.{token}\n'
    ui = ui.replace(anchor, anchor + child, 1)
    node = f'''
ui::text : _nameless.{token} {{
 text: "<img src=/material/ui/dashboard/bettergps_overlaynew/{material}.mat {crop} xscale=stretch yscale=stretch>"
 coords_l: 0
 coords_r: {width}
 coords_t: {height}
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
    ui = ui.rstrip()[:-1] + node + '}\n'
    validate_sii_units(ui)
    restored = ui.replace(child, '', 1).replace(f'my_children: {count+1}\n', f'my_children: {count}\n', 1).replace(node, '', 1)
    assert restored.rstrip() == original.rstrip(), 'Native UI changed outside additive guard'
    return ui


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--stock-root', type=Path, required=True)
    p.add_argument('--preview', type=Path, required=True)
    p.add_argument('--gps-template', type=Path, required=True, help='Existing shipped GPS UI, preserving prior customizations')
    args = p.parse_args()
    assets = Path(__file__).resolve().parents[1] / 'BetterGPS_assets'
    target = assets / 'material/copilot/westernstar'
    args.preview.mkdir(parents=True, exist_ok=True)
    report = {'artwork': [], 'native': {}}
    digest = lambda data: hashlib.sha256(data).hexdigest()
    for name in ('western_star_5700xe_splash_screen.dds', 'western_star_zonar_5700xe_home.dds', 'western_star_zonar_5700xe_nav.dds', 'western_star_zonar_5700xe_notrip.dds'):
        source = args.source / name
        image = Image.open(source).convert('RGBA')
        destination = target / name
        image.save(destination, format='DDS')
        assert Image.open(destination).convert('RGBA').tobytes() == image.tobytes()
        image.save(args.preview / (source.stem + '.png'))
        report['artwork'].append(dict(path=name, size=image.size, source_sha256=digest(source.read_bytes()), sha256=digest(destination.read_bytes())))
    prefix = assets / 'material/ui/dashboard/bettergps_overlaynew/westernstar_5700_cluster'
    width, height, role = 1072, 300, 163
    header = bytearray((prefix.parent / 'phone_layout_marker.dds').read_bytes()[:128])
    struct.pack_into('<III', header, 12, height, width, width*4)
    pixels = bytearray(width*height*4)
    for i in range(4):
        pixels[i*4:i*4+4] = bytes((0xa7, 0x5b, 0x31+role+i, 255))
    prefix.with_suffix('.dds').write_bytes(header+pixels)
    path = '/' + prefix.relative_to(assets).as_posix() + '.dds'
    tobj = bytearray((prefix.parent / 'phone_layout_marker.tobj').read_bytes()[:48])
    struct.pack_into('<I', tobj, 40, len(path))
    prefix.with_suffix('.tobj').write_bytes(tobj+path.encode())
    prefix.with_suffix('.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "' + path[:-4] + '.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
    for name, token, material, crop, w, h in (
        ('westernstar_5700xe', 'bg5700_intro', prefix.name, f'left=0 right={1065/1072:.12f} top=0 bottom=1', 800, 214),
        ('westernstar_5700xe_gps', 'bgtrk_guard', 'truck_gps_guard', 'left=0.275 right=0.975 top=0.375 bottom=0.875', 512, 512),
    ):
        relative = Path('ui/dashboard') / (name+'.sii')
        original = (args.gps_template if name.endswith('_gps') else args.stock_root / relative).read_text(encoding='utf-8-sig')
        ui = append_guard(original, token, material, crop, w, h)
        (assets / relative).write_text(ui, encoding='utf-8', newline='\n')
        report['native'][relative.as_posix()] = digest(original.encode())
    (target / '5700xe-import-manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS: 4 pixel/alpha-preserved DDS; 5700XE cluster and GPS additive guards; stock UI unchanged')


if __name__ == '__main__':
    main()
