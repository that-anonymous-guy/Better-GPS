"""Collect converted PP804396 assets and validate their simple material/optics."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct

from scs_camera_mounts import geometry,read,validate_layout


def package(cache,output):
    relative=Path('vehicle/truck/upgrade/bettergps/pp804396')
    runtime=output/'scs/converted';dest=runtime/relative;dest.mkdir(parents=True,exist_ok=True)
    rows=[];materials=set()
    for part,name in ((part,name) for part in (396,397) for name in (f'pp804{part}_lod0',f'pp804{part}_lod1',f'pp804{part}_truck',f'pp804{part}_truck_lod1')):
        part_relative=Path(f'vehicle/truck/upgrade/bettergps/pp804{part}')
        part_dest=runtime/part_relative;part_dest.mkdir(parents=True,exist_ok=True)
        for ext in ('pmd','pmg'):shutil.copy2(cache/part_relative/f'{name}.{ext}',part_dest/f'{name}.{ext}')
        pmd=(part_dest/f'{name}.pmd').read_bytes()
        materials.update(m.decode() for m in re.findall(rb'/automat/[^\0]+\.mat',pmd))
        path=part_dest/f'{name}.pmg'
        layout=validate_layout(path);info=read(path);points,faces=geometry(path)
        assert info['piece_count']==1 and info['locator_count']==0
        if '_truck' in name:assert abs(max(p[2] for p in points))<.000001,'Vehicle mounting plane must stay at Z=0'
        else:assert min(p[1] for p in points)>-.000001,'Generic mounting base must stay on Y=0'
        rows.append({'model':name,'triangles':len(faces),'pieces':1,**layout})
    assert len(materials)==1
    for path in materials:
        target=runtime/path.lstrip('/');target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(cache/path.lstrip('/'),target)
        assert 'eut2.dif.spec' in target.read_text()
    for name in ('palette.dds','palette.tobj'):shutil.copy2(cache/relative/name,dest/name)
    assert b'/vehicle/truck/upgrade/bettergps/pp804396/palette.dds' in (dest/'palette.tobj').read_bytes()
    # The pupil has its own constant UV swatch; its cap normals prove that the
    # actual converted lens faces upward, not merely an editable parent empty.
    data=(dest/'pp804396_lod0.pmg').read_bytes();u=lambda p:struct.unpack_from('<I',data,p)[0]
    at=u(84);stride=u(at+60);normal=u(at+68);uv=u(at+72)
    angle=json.loads((output/'pp804396_dimensions.json').read_text())['user_adjustment']['ball_up_degrees']
    direction=(0,math.sin(math.radians(angle)),math.cos(math.radians(angle)));fronts=0
    for i in range(u(at+4)):
        if abs(struct.unpack_from('<f',data,uv+i*stride)[0]-.6875)>.0001:continue
        n=struct.unpack_from('<3f',data,normal+i*stride)
        if sum(x*y for x,y in zip(n,direction))>.9999:fronts+=1
    assert fronts>=12,('Lens does not match user-edited inclination',fronts)
    glb=(output/'pp804396_reconstruction.glb').read_bytes()
    magic,version,length=struct.unpack_from('<3I',glb)
    assert magic==0x46546c67 and version==2 and length==len(glb)
    document=json.loads(glb[20:20+struct.unpack_from('<I',glb,12)[0]])
    assert any(n.get('name')=='PP804396_OPTICAL_BALL' for n in document['nodes'])
    assert len(document.get('images',[]))==1,'Only the tiny palette should be embedded'
    assert len((dest/'palette.dds').read_bytes())<8192
    result={'status':'PASS','scs_models':rows,'material_count':1,'texture_size':[128,16],
            'converted_pupil_up_degrees':angle,'glb_ball_hierarchy_preserved':True,
            'truck_mounting':'separate right and left vehicle models included',
            'files':{p.relative_to(output).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in sorted(runtime.rglob('*')) if p.is_file()}}
    (output/'validation.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('ASSET_PASS:',len(rows),'native models; GLB hierarchy, tiny palette and user optical angle verified.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();package(args.cache.resolve(),args.output.resolve())
