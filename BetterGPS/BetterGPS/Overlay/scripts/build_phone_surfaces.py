"""Build two-surface phone models; preserve SCS's UI source behind our display.
Format references: ConverterPIX src/structs/{pmd,pmg_0x15}.h.
Only unskinned, single-part native phone models are accepted.
"""
from pathlib import Path
import struct, json, hashlib
ROOT=Path(__file__).resolve().parents[2]
SOURCES=ROOT/'validation/phone-native-power'
GEOMETRY=ROOT/'validation/phone-independent'
OUT=ROOT/'BetterGPS/Overlay/layouts/vertical/surfaces'
u32=lambda b,o:struct.unpack_from('<I',b,o)[0]
i32=lambda b,o:struct.unpack_from('<i',b,o)[0]
def put(b,o,v):struct.pack_into('<I',b,o,v)
def build(display,model,screen,source=None):
    pmd=((source or SOURCES)/(model+'.pmd')).read_bytes();pmg=((source or GEOMETRY)/(model+'.pmg')).read_bytes()
    assert u32(pmd,0)==4 and u32(pmd,8)==1
    n=u32(pmd,4);assert pmg[:4]==b'\x15gmP' and u32(pmg,4)==n and u32(pmg,8)==1 and u32(pmg,12)==0 and u32(pmg,20)==0
    po=u32(pmg,84);vo,vs,io,isz=struct.unpack_from('<4I',pmg,96)
    assert po+n*100==vo and vo+vs==io and io+isz==len(pmg)
    piece=bytearray(pmg[po+screen*100:po+(screen+1)*100]);edges,nv=struct.unpack_from('<2I',piece);stride=u32(piece,60);pos,norm,uv=struct.unpack_from('<3I',piece,64)
    assert u32(piece,16)==screen and stride in (36,52) and norm-pos==12 and uv-pos+8<=stride
    vertices=bytearray(pmg[vo:io]);extra=bytearray(pmg[pos:pos+nv*stride]);assert len(extra)==nv*stride
    # The front surface retains every original position, normal and index.
    # Its V axis is static (top=0), independent of ATS's dynamic-UI toggle.
    for v in range(nv):
        normal=struct.unpack_from('<3f',pmg,norm+v*stride)
        assert abs(sum(x*x for x in normal)-1)<.001
        original=struct.unpack_from('<3f',pmg,pos+v*stride)
        struct.pack_into('<3f',vertices,pos-vo+v*stride,*(original[k]-.0005*normal[k] for k in range(3)))
        v_offset=uv-pos+v*stride+4;value=struct.unpack_from('<f',extra,v_offset)[0];struct.pack_into('<f',extra,v_offset,1-value)
    # Keep the native source plane 0.5mm behind the unchanged visible screen.
    old_screen=bytearray(piece);normal=struct.unpack_from('<3f',pmg,norm)
    for offset in (20,36,48):
        values=struct.unpack_from('<3f',old_screen,offset);struct.pack_into('<3f',old_screen,offset,*(values[k]-.0005*normal[k] for k in range(3)))
    vertex_delta=100;new_vo=vo+vertex_delta;new_io=io+vertex_delta+len(extra)
    pieces=[]
    for i in range(n):
        p=old_screen if i==screen else bytearray(pmg[po+i*100:po+(i+1)*100])
        for offset in range(64,96,4):
            if i32(p,offset)>=0:put(p,offset,u32(p,offset)+vertex_delta)
        put(p,96,u32(p,96)+new_io-io);pieces.append(p)
    front=bytearray(piece);put(front,16,n)
    for offset in range(64,96,4):
        if i32(front,offset)>=0:put(front,offset,new_vo+vs+u32(front,offset)-pos)
    put(front,96,new_io+isz);pieces.append(front)
    indices=pmg[u32(piece,96):u32(piece,96)+edges*2];assert len(indices)==edges*2
    assert max(struct.unpack('<'+'H'*edges,indices))<nv
    tail=indices+b'\0'*((-len(indices))%4)
    header=bytearray(pmg[:po]);put(header,4,n+1);put(header,u32(pmg,76)+8,n+1)
    put(header,88,u32(pmg,88)+100);put(header,96,new_vo);put(header,100,vs+len(extra));put(header,104,new_io);put(header,108,isz+len(tail))
    patched=bytes(header)+b''.join(pieces)+vertices+extra+pmg[io:]+tail
    assert len(patched)==new_io+isz+len(tail)
    # PMD keeps all native materials, plus the independent display material.
    mo,md=u32(pmd,56),u32(pmd,60);assert md==mo+n*4
    name=('/material/ui/bettergps_displays/'+display+'_surface.mat').encode()+b'\0'
    desc=bytearray(pmd[:mo]);put(desc,4,n+1);put(desc,32,u32(desc,32)+len(name));put(desc,60,md+4)
    table=struct.pack('<'+'I'*(n+1),*([u32(pmd,mo+i*4)+4 for i in range(n)]+[len(pmd)+4]))
    patched_pmd=bytes(desc)+table+pmd[md:]+name
    target=OUT/display/model;target.parent.mkdir(parents=True,exist_ok=True);target.with_suffix('.pmd').write_bytes(patched_pmd);target.with_suffix('.pmg').write_bytes(patched)
    # Verify the front surface is identical except for V, and the backing still
    # references the original UI material. The model's other parts are unchanged.
    for v in range(nv):
        old=pmg[pos+v*stride:pos+(v+1)*stride];new=extra[v*stride:(v+1)*stride];k=uv-pos+4
        assert old[:k]==new[:k] and old[k+4:]==new[k+4:]
    assert pmd[md:] in patched_pmd
    return dict(display=display,model=model,originalPieces=n,newPieces=n+1,addedVertices=nv,addedTriangles=edges//3,frontPositionsUnchanged=True,frontVFlipped=True,nativeSourceRetained=True,sha256=hashlib.sha256(patched).hexdigest())
result=[build('road_trip_phone','vehicle/car/upgrade/interior_decors/set_lglass/rt_glass_phoneholder',5),build('cabin_phone','vehicle/truck/upgrade/interior_decors/gps/cellphone_gps',1)]
# These variants use the same native UI/TOBJ, but have distinct 3D models.
# Keep the shared Road Trip display/profile and adapt each original mesh.
variants=ROOT/'validation/ford-phone-variants/extracted'
for name,screen in [('rt_glass_phoneholder1',5),('rt_glass_phoneholder2',5),('rt_ac_phoneholder',2),('rt_ac_phoneholder3',2)]:
    result.append(build('road_trip_console' if name.startswith('rt_ac_') else 'road_trip_phone','vehicle/car/upgrade/interior_decors/set_lglass/'+name,screen,variants))
(ROOT/'validation/phone-independent/model-audit.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
