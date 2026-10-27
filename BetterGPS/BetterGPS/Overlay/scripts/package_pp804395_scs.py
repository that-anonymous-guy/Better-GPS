"""Assemble optional rear, side and front cameras as an unpacked ATS mod."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
from PIL import Image

from scs_camera_mounts import append_locator,read,validate_layout,geometry,validate_tuning_bindings


def assemble(stock,cache,project,output):
    root=output/'mod'
    root.mkdir(parents=True,exist_ok=True)
    relative=Path('vehicle/truck/upgrade/bettergps/pp804395')
    dest=root/relative;dest.mkdir(parents=True,exist_ok=True)
    files=set()
    def shipped(path):
        files.add(path)
        return path
    mats=set()
    # Direct tunable accessory: one small model, no permanent hookup.
    # The distant LOD remains in the editable source project only.
    for lod in (0,):
        name=f'pp804395_lod{lod}'
        for ext in ('pmd','pmg'):shutil.copy2(cache/relative/f'{name}.{ext}',shipped(dest/f'{name}.{ext}'))
        data=(dest/f'{name}.pmd').read_bytes()
        mats.update(m.decode() for m in re.findall(rb'/automat/[^\0]+\.mat',data))
        assert read(dest/f'{name}.pmg')['piece_count']==5,'Expected exterior, optics, glass, metal and dark interior pieces'
        validate_layout(dest/f'{name}.pmg')
    triangle_count=len(geometry(dest/'pp804395_lod0.pmg')[1])
    refinement=output.parent.parent/'camera-refinement'
    detail=json.loads((refinement/'geometry-report.json').read_text())
    assert triangle_count==next(p['triangles'] for p in detail['models'] if p['model']=='pp804395' and p['lod']==0)
    for name in ('palette','micro_normal','glass'):
        for ext in ('tobj','dds'):
            source=cache/'vehicle/truck/upgrade/bettergps/camera_finish'/f'{name}.{ext}'
            target=shipped(root/'vehicle/truck/upgrade/bettergps/camera_finish'/source.name)
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    icon=Path('material/ui/accessory/bettergps')
    for ext in ('dds','tobj','mat'):
        target=shipped(root/icon/f'pp804395.{ext}');target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(output/'project'/icon/f'pp804395.{ext}',target)
    for path in mats:
        target=shipped(root/path.lstrip('/'));target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(cache/path.lstrip('/'),target)
        assert any(shader in target.read_text() for shader in ('eut2.dif','eut2.glass'))
    accessory=shipped(root/'def/vehicle/truck/kenworth.t680_2022/accessory/bgps_cam/pp804395.sii')
    accessory.parent.mkdir(parents=True,exist_ok=True)
    accessory.write_text('''SiiNunit
{
accessory_addon_data : pp804395.kenworth.t680_2022.bgps_cam
{
    name: "Pana-Pacific PP804395 - Rear Camera"
    price: 150
    unlock: 0
    icon: "bettergps/pp804395"
    part_type: aftermarket
    exterior_model: "/vehicle/truck/upgrade/bettergps/pp804395/pp804395_lod0.pmd"
    suitable_for[]: "*.kenworth.t680_2022.cabin"
}
}
''',encoding='utf-8')
    side_source=output.parent.parent/'pp804396'
    side_rows=json.loads((side_source/'placement/mounts.json').read_text())
    # The two side models share the small PP804396 palette/material.
    side_materials=set()
    for part in (396,397):
        part_relative=Path(f'vehicle/truck/upgrade/bettergps/pp804{part}')
        for ext in ('pmd','pmg'):
            path=shipped(root/part_relative/f'pp804{part}_truck.{ext}');path.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(cache/part_relative/path.name,path)
        side_materials.update(m.decode() for m in re.findall(rb'/automat/[^\0]+\.mat',(root/part_relative/f'pp804{part}_truck.pmd').read_bytes()))
    for path in side_materials:
        target=shipped(root/path.lstrip('/'));target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(cache/path.lstrip('/'),target)
    # Native UI icon: small transparent drawing in the left 64x64 tile.
    icon_dir=root/icon
    image=Image.open(side_source/'icon/pp804396_square.png').convert('RGBA').resize((64,64),Image.Resampling.LANCZOS)
    canvas=Image.new('RGBA',(256,64));canvas.paste(image,(0,0))
    canvas.save(shipped(icon_dir/'pp804396.dds'),format='DDS')
    template=(icon_dir/'pp804395.tobj').read_bytes();header=bytearray(template[:48])
    texture_path=b'/material/ui/accessory/bettergps/pp804396.dds'
    struct.pack_into('<I',header,40,len(texture_path))
    shipped(icon_dir/'pp804396.tobj').write_bytes(header+texture_path)
    for part,side in ((396,'Right'),(397,'Left')):
        shipped(icon_dir/f'pp804{part}.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "pp804396.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
        token='bgps_right' if part==396 else 'bgps_left'
        definition=shipped(root/f'def/vehicle/truck/kenworth.t680_2022/accessory/{token}/pp804{part}.sii')
        definition.parent.mkdir(parents=True,exist_ok=True)
        definition.write_text(f'''SiiNunit
{{
accessory_addon_data : pp804{part}.kenworth.t680_2022.{token}
{{
    name: "Pana-Pacific PP804{part} - {side} Side Camera"
    price: 250
    unlock: 0
    icon: "bettergps/pp804{part}"
    part_type: aftermarket
    exterior_model: "/vehicle/truck/upgrade/bettergps/pp804{part}/pp804{part}_truck.pmd"
    suitable_for[]: "*.kenworth.t680_2022.cabin"
}}
}}
''',encoding='utf-8')
        # Keep save data paths and original prices readable. There is no host
        # locator for the obsolete type; the new tuning pin uses its own name.
        legacy='bgps_cam_r' if part==396 else 'bgps_cam_l'
        legacy_definition=shipped(root/f'def/vehicle/truck/kenworth.t680_2022/accessory/{legacy}/pp804{part}.sii')
        legacy_definition.parent.mkdir(parents=True,exist_ok=True)
        legacy_definition.write_text(definition.read_text().replace(f'.{token}\n',f'.{legacy}\n'),encoding='utf-8')
    rows=json.loads((output/'placement/mounts.json').read_text())
    records=[]
    for row in rows:
        model=Path('vehicle/truck/kenworth_t680_2022/cabin')/(row['cabin']+'.pmg')
        record=append_locator(stock/('cabin/'+row['cabin']+'.pmg'),shipped(root/model),
                              row['position_scs'],row['camera_bounds_scs'],row['rotation_wxyz'])
        additional=[]
        for side_row in (p for p in side_rows if p['cabin']==row['cabin']):
            stage=(root/model).with_suffix('.stage.pmg')
            additional.append(append_locator(root/model,stage,side_row['position_scs'],side_row['camera_bounds_scs'],
                side_row['rotation_wxyz'],side_row['locator']))
            stage.replace(root/model)
        record['side_locators']=[p['camera_locator'] for p in additional]
        record['bytes_added']=132
        record['output_sha256']=hashlib.sha256((root/model).read_bytes()).hexdigest()
        records.append({'cabin':row['cabin'],'path':model.as_posix(),**record})
    front_source=output.parent.parent/'pp807032'
    front_relative=Path('vehicle/truck/upgrade/bettergps/pp807032')
    front_materials=set()
    for model in ('pp807032',):
        relative_model=Path('vehicle/truck/upgrade/bettergps')/model
        for ext in ('pmd','pmg'):
            name=f'{model}_lod0.{ext}'
            target=shipped(root/relative_model/name);target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(cache/relative_model/name,target)
            if ext=='pmd':front_materials.update(m.decode() for m in re.findall(rb'/automat/[^\0]+\.mat',target.read_bytes()))
    for path in front_materials:
        target=shipped(root/path.lstrip('/'));target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(cache/path.lstrip('/'),target)
    image=Image.open(front_source/'pp807032_icon.png').convert('RGBA').resize((64,64),Image.Resampling.LANCZOS)
    canvas=Image.new('RGBA',(256,64));canvas.paste(image,(0,0));canvas.save(shipped(icon_dir/'pp807032.dds'),format='DDS')
    texture_path=b'/material/ui/accessory/bettergps/pp807032.dds'
    header=bytearray((icon_dir/'pp804395.tobj').read_bytes()[:48]);struct.pack_into('<I',header,40,len(texture_path))
    shipped(icon_dir/'pp807032.tobj').write_bytes(header+texture_path)
    shipped(icon_dir/'pp807032.mat').write_text('effect : "ui.rfx" { texture : "texture" { source : "pp807032.tobj" u_address : clamp v_address : clamp mip_filter : none } }\n')
    # Same system as stock steering-wheel knobs and light bars: register an
    # accessory_hookup_data, offered at empty slot_N locators in its parent.
    storage=shipped(root/'def/vehicle/addon_hookup_storage.bettergps.sii')
    storage.parent.mkdir(parents=True,exist_ok=True)
    storage.write_text('SiiNunit\n{\n@include "addon_hookups/bettergps_pp807032.sui"\n}\n',encoding='utf-8')
    definition=shipped(root/'def/vehicle/addon_hookups/bettergps_pp807032.sui')
    definition.parent.mkdir(parents=True,exist_ok=True)
    definition.write_text('''accessory_hookup_data : pp807032.bgps.addon_hookup
{
    name: "Pana-Pacific PP807032 - Front Camera"
    price: 250
    unlock: 0
    icon: "bettergps/pp807032"
    model: "/vehicle/truck/upgrade/bettergps/pp807032/pp807032_lod0.pmd"
    suitable_for[]: "bumper"
    conflict_with[]: "sign.kenworth.t680_2022.f_banner"
    conflict_with[]: "sign_low.kenworth.t680_2022.f_banner"
}
accessory_hookup_data : pp807032low.bgps.addon_hookup
{
    // Hidden alias keeps existing saves readable; all cameras use the standard model.
    name: "Pana-Pacific PP807032 - Front Camera (legacy)"
    price: 0
    unlock: 0
    icon: "bettergps/pp807032"
    model: "/vehicle/truck/upgrade/bettergps/pp807032/pp807032_lod0.pmd"
    suitable_for[]: "bumper"
    conflict_with[]: "sign.kenworth.t680_2022.f_banner"
    conflict_with[]: "sign_low.kenworth.t680_2022.f_banner"
}
''',encoding='utf-8')
    # Preserve the stock banner's bullbar conflict, suitability and pricing.
    # Add the front child IDs as reciprocal conflicts for both selection orders.
    for banner in ('sign','sign_low'):
        relative_banner=Path(f'def/vehicle/truck/kenworth.t680_2022/accessory/f_banner/{banner}.sii')
        text=(front_source/'stock-reference'/relative_banner).read_text(encoding='utf-8')
        anchor='    conflict_with[]: "small.kenworth.t680_2022.bullbar"'
        assert anchor.strip() in text
        text=text.replace(anchor.strip(),anchor.strip()+'\n\tconflict_with[]: "pp807032.bgps.addon_hookup"\n\tconflict_with[]: "pp807032low.bgps.addon_hookup"')
        target=shipped(root/relative_banner);target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text(text,encoding='utf-8')
    front_rows=json.loads((front_source/'placement/mounts.json').read_text())
    front_records=[]
    for row in front_rows:
        relative_bumper=Path('vehicle/truck/kenworth_t680_2022/accessory/bumper')/(row['bumper']+'.pmg')
        record=append_locator(stock/'accessory/bumper'/(row['bumper']+'.pmg'),shipped(root/relative_bumper),
            row['position_scs'],row['camera_bounds_scs'],row['rotation_wxyz'],'slot_0')
        front_records.append({'bumper':row['bumper'],'path':relative_bumper.as_posix(),
            'attachment_system':'optional slotted accessory_hookup_data','slot_name':'slot_0',**record})
    # Retire only the former direct front option, its alternative model and
    # our chassis overrides. The working rear/side units and poses stay intact.
    retired=[root/'vehicle/truck/kenworth_t680_2022/chassis'/p.name for p in (stock/'chassis').glob('*.pmg')]
    retired.extend(root/f'def/vehicle/truck/kenworth.t680_2022/accessory/{kind}/pp807032{suffix}.sii'
        for kind in ('bgps_front','bgps_cam_f') for suffix in ('','_std'))
    retired.extend(root/front_relative/f'pp807032_std.{ext}' for ext in ('pmd','pmg'))
    old_receipt=output/'package-validation.json'
    if old_receipt.exists():
        retired.extend(root/p for p in json.loads(old_receipt.read_text())['files'] if root/p not in files)
    for old in set(retired):
        if old.exists():old.unlink()
    shipped(root/'manifest.sii').write_text('''SiiNunit
{
mod_package : .package_name
{
    package_version: "0.13.2"
    display_name: "BetterGPS - Pana-Pacific Cameras T680 NG"
    author: "BetterGPS"
    category[]: "tuning_parts"
    description_file: "mod_description.txt"
    compatible_versions[]: "1.61.*"
}
}
''',encoding='utf-8')
    shipped(root/'mod_description.txt').write_text('''Pana-Pacific PP804395 rear camera, optional physical accessory.
Kenworth T680 Next Gen: Day Cab, 52-inch and 76-inch sleepers.
Individual rear-wall mounting coordinates. Refined rounded housings, recessed protective glass and detailed optics.
Shared satin normal map, darker sun-specular optics, separate metal hardware and transparent glass.
Five material draws for front/rear (including non-specular dark cavity), four per side. One exported object per camera.
One standard front camera for all eight bumpers, $250. The low-bumper option is hidden and retained only as a save-compatible standard-model alias.
Front camera conflicts with both Oversize Load banners; the bullbar remains compatible with the camera.
Minor flat/grille intersections on low bumpers are explicitly accepted by the user.
Each front flat has the same cross-section over its entire length; no reduced necks or insertion tips.
Hex-socket fixing heads, rounded pivots and rubber-lined cable passages. Thicker front flats and grille-fitting backing bridge.
Rear camera at 62.5% original size, with a flat wall plate and straight side tabs fitted to each cab.
Rear head tilted down 45 degrees. Sleeper rear cameras raised 10 cm; Day Cab unchanged.
Optional PP804396 right side and PP804397 left side cameras below the mirrors, $250 each, reduced to 65% size.
Optional PP807032 front camera on all eight stock front bumpers, $250, at the left grille beside the plate.
Front camera is an optional bumper child, using empty slot_0 and accessory_hookup_data like stock light bars and steering knobs.
Its mounting pose follows each bumper automatically, including short and long hoods. No front chassis/cabin socket or bumper-compatibility wildcard filters.
Buy or remove the camera at the rear-wall socket in the truck configurator.
No truck receives a camera by default. Rear: $150; each side: $250; available at level 0.
This mod adds a physical camera; the NavPlus video page is a separate feature.
For stock ATS 1.61 cabin geometry. Place above other mods replacing these cabins.
''',encoding='utf-8')
    # The current shared project (including textures) is already maintained
    # beside all four refined .blend files. Do not mix its intermediates into
    # the older palette-only Blender project's historical source directory.
    assert (refinement/'project'/relative/'pp804395_lod0.pim').exists()
    # Explicit manifest prevents obsolete permanent-hookup files from an older
    # build being silently shipped again. Also retire those owned staging files.
    for old in (root/'unit/hookup/models/bettergps_pp804395.sii',
                dest/'pp804395_lod1.pmd',dest/'pp804395_lod1.pmg'):
        if old.exists():old.unlink()
    assert not any('unit/hookup' in p.relative_to(root).as_posix() for p in files)
    assert not any(re.search(r'\b(defaults|overrides|require)\[',p.read_text())
                   for p in files if p.suffix=='.sii')
    locator_names=[l['name'] for model in (root/'vehicle/truck/kenworth_t680_2022').rglob('*.pmg')
        for part in read(model)['parts'] for l in part['locators'] if l['name'].startswith('bgps_')]
    accessory_types=[]
    for path in files:
        if path.suffix=='.sii':
            accessory_types.extend(re.findall(r'accessory_addon_data\s*:\s*[^\s]+\.(bgps_\w+)',path.read_text()))
    bindings=validate_tuning_bindings(locator_names,accessory_types)
    report={'status':'PASS','target':'ATS 1.61 stock T680 NG','triangles':triangle_count,
            'mount_type':'flat wall plate with straight side tabs to pivots',
            'optional':True,'permanent_hookups':False,'version':'0.13.2','uniform_scale':0.625,
            'package_type':'unpacked mod directory','side_cameras':side_rows,
            'camera_head_down_tilt_degrees':45,'height_reference':'Day Cab unchanged; sleepers +10 cm',
            'socket_faces':'rear (+Z in cabin space)','custom_icon':'256x64 RGBA8 DDS',
            'material_draws_per_lod':{'front_rear':5,'sides':4},'refinement':detail,
            'cabin_mounts':records,'front_camera_mounts':front_records,'front_camera_placement':front_rows,
            'front_attachment_system':'one visible standard accessory_hookup_data on bumper slot_0; suitable_for=bumper; hidden legacy alias',
            'front_banner_conflict':'bidirectional with sign and sign_low; stock banner/bullbar conflict retained; no camera/bullbar conflict',
            'ats_prefix_bindings':bindings,
            'package_bytes':sum(p.stat().st_size for p in files),'in_game_validation':'pending',
            'files':{p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in sorted(files)}}
    (output/'package-validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PACKAGE_PASS',root,'3 unchanged direct camera sockets per cabin; optional PP807032 hookup on 8 bumper slots.')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('stock','cache','project','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();assemble(a.stock.resolve(),a.cache.resolve(),a.project.resolve(),a.output.resolve())
