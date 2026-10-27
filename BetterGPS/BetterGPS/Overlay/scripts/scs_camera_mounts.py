"""Inspect PMG v21 attachment frames and append one lossless camera locator.

Existing geometry, skeleton, materials and locator bytes are preserved. The
new socket is inserted before the piece table; absolute offsets are relocated.
The native table/string/vertex/index order is retained. Never
write the stock input path. All changes are confined to an output mod folder.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

ALPHABET='\0'+'0123456789abcdefghijklmnopqrstuvwxyz_'


def validate_tuning_bindings(locator_names,accessory_types):
    """Emulate ATS's prefix match, not a desktop reader's exact-name lookup.

    SCS matches an upgrade type against every locator beginning with that
    type. In particular bgps_cam incorrectly consumes bgps_cam_f/r/l.
    Historical definition types may stay readable provided no locator matches
    them. Active locators must each resolve to exactly their own type.
    """
    names=set(locator_names);types=set(accessory_types)
    for name in names:
        matches=sorted(t for t in types if name.startswith(t))
        assert matches==[name],('Ambiguous ATS prefix binding',name,matches)
    return {t:sorted(n for n in names if n.startswith(t)) for t in sorted(types)}


def token(value):
    if isinstance(value,str):
        assert len(value)<=12
        return sum(ALPHABET.index(c)*38**i for i,c in enumerate(value))
    result=''
    while value:
        value,c=divmod(value,38);result+=ALPHABET[c]
    return result


def read(path):
    data=path.read_bytes()
    assert data[:4]==b'\x15gmP',path
    u=lambda offset:struct.unpack_from('<I',data,offset)[0]
    parts=[]
    for index in range(u(8)):
        at=u(76)+24*index
        name,pieces,first_piece,locs,first_loc=struct.unpack_from('<Q4I',data,at)
        locators=[]
        for j in range(first_loc,first_loc+locs):
            loc_at=u(80)+44*j
            name_i,*values=struct.unpack_from('<Q8fi',data,loc_at)
            locators.append({'name':token(name_i),'position':values[:3],'scale':values[3],
                             'rotation_wxyz':values[4:8],'hookup_offset':values[8],
                             'raw_hex':data[loc_at:loc_at+44].hex()})
        parts.append({'name':token(name),'pieces':pieces,'first_piece':first_piece,'locators':locators})
    return {'file':str(path),'sha256':hashlib.sha256(data).hexdigest(),
            'bounds':list(struct.unpack_from('<6f',data,48)),
            'parts':parts,'locator_count':u(20),'piece_count':u(4)}


def inspect(root, output):
    rows=[]
    for relative in ('cabin/dc','cabin/s52m','cabin/s76h','accessory/rear_cab/day_standard',
                     'accessory/rear_cab/day_standard2','accessory/rear_cab/standard',
                     'accessory/rear_cab/standard_aircon'):
        row=read(root/(relative+'.pmg'));row['model']=relative
        rows.append(row)
        print(relative,'bounds=',row['bounds'])
        print('  locators=',[(p['name'],[(l['name'],[round(n,4) for n in l['position']]) for l in p['locators']]) for p in row['parts'] if p['locators']])
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')


def geometry(path):
    """Read positions and triangles without altering the PMG vertex pools."""
    data=path.read_bytes();assert data[:4]==b'\x15gmP'
    u=lambda p:struct.unpack_from('<I',data,p)[0]
    points=[];triangles=[]
    for i in range(u(4)):
        at=u(84)+100*i;start=len(points)
        count,stride,pos=u(at+4),u(at+60),u(at+64)
        points.extend(struct.unpack_from('<3f',data,pos+j*stride) for j in range(count))
        indices=struct.unpack_from('<'+'H'*u(at),data,u(at+96))
        triangles.extend(tuple(start+j for j in indices[k:k+3]) for k in range(0,len(indices),3))
    return points,triangles


def validate_layout(path):
    """Validate native section order and all interleaved stream/index ranges.

    A permissive desktop parser also reads our old tables-after-pools layout.
    This additional check rejects that layout before it can reach ATS.
    """
    data=path.read_bytes();u=lambda off:struct.unpack_from('<I',data,off)[0]
    assert data[:4]==b'\x15gmP'
    assert 112<=u(72)<=u(76)<u(80)<=u(84)<=u(88)<=u(96)<=u(104), 'Noncanonical PMG section order'
    assert u(76)+24*u(8)<=u(80)
    assert u(80)+44*u(20)==u(84)
    assert u(84)+100*u(4)<=u(88)
    assert u(88)+u(92)<=u(96)
    assert u(96)+u(100)<=u(104)
    assert u(104)+u(108)==len(data)
    for i in range(u(4)):
        at=u(84)+100*i;count,stride=u(at+4),u(at+60)
        # Positions, normals, UVs, colors, factors, tangents, bone streams.
        widths=(12,12,8*u(at+12),4,4,16,4,4)
        for off,width in zip(range(64,96,4),widths):
            pointer=u(at+off)
            if pointer!=0xffffffff:
                assert u(96)<=pointer and pointer+(count-1)*stride+width<=u(96)+u(100), (i,off,'stream outside vertex pool')
        pointer=u(at+96)
        assert u(104)<=pointer and pointer+2*u(at)<=u(104)+u(108), (i,'indices outside index pool')
    return {'native_section_order':True,'all_stream_ranges_valid':True}


def append_locator(source, target, position, bounds, rotation=(0,0,1,0),locator_name='bgps_cam'):
    """Add an EMPTY tuning socket or slot_N, matching stock model locators.

    A permanent model hookup is not a truck tuning accessory. In particular,
    configurator code must never receive a lod_model hookup as a tuning socket.
    Stock T680 sockets use offset zero into an empty, NUL-terminated string.
    A slot_N in an accessory model is offered through the separate
    accessory_hookup_data catalogue, like stock steering knobs and light bars.
    """
    validate_layout(source)
    original=source.read_bytes()
    assert original[:4]==b'\x15gmP' and target.resolve()!=source.resolve()
    u=lambda off:struct.unpack_from('<I',original,off)[0]
    part_count,old_loc_count=u(8),u(20)
    assert part_count==1,'Only verified one-part stock T680 NG models are supported'
    name,pieces,first_piece,loc_count,first_loc=struct.unpack_from('<Q4I',original,u(76))
    assert token(name)=='part1' and first_loc==0 and loc_count==old_loc_count
    locators=original[u(80):u(80)+44*old_loc_count]
    assert all(token(struct.unpack_from('<Q',locators,i*44)[0])!=locator_name for i in range(old_loc_count))
    strings=original[u(88):u(88)+u(92)]
    assert not strings or strings[0]==0,'Expected empty hookup string at offset zero'
    record=struct.pack('<Q8fi',token(locator_name),*position,1,*rotation,0)
    insertion=u(84);table_delta=len(record)
    # Bumpers originally have no locators and no string pool. Introduce the
    # NUL required by an empty tuning hookup before the untouched vertex pool.
    # Match stock's four-byte padded empty string pool. Vertex streams remain
    # aligned as they are in converter-produced models and stock cabins.
    string_delta=0 if strings else 4
    delta=table_delta+string_delta
    data=bytearray(original[:insertion]+record+original[insertion:u(88)]+b'\0'*string_delta+original[u(88):])
    struct.pack_into('<I',data,u(76)+16,loc_count+1)
    struct.pack_into('<I',data,20,old_loc_count+1)
    for off in (84,88):struct.pack_into('<I',data,off,u(off)+table_delta)
    for off in (96,104):struct.pack_into('<I',data,off,u(off)+delta)
    struct.pack_into('<I',data,92,u(92)+string_delta)
    for i in range(u(4)):
        old=u(84)+100*i;new=old+table_delta
        for off in range(64,100,4):
            pointer=u(old+off)
            if pointer!=0xffffffff:
                assert pointer>=insertion
                struct.pack_into('<I',data,new+off,pointer+delta)
    # Extend culling bounds to contain the child camera as well as the cabin.
    minimum=list(struct.unpack_from('<3f',original,48));maximum=list(struct.unpack_from('<3f',original,60))
    for i in range(3):minimum[i]=min(minimum[i],bounds[0][i]);maximum[i]=max(maximum[i],bounds[1][i])
    center=[(a+b)/2 for a,b in zip(minimum,maximum)]
    diagonal=math.sqrt(sum((a-b)**2 for a,b in zip(minimum,maximum)))
    struct.pack_into('<3ff6f',data,32,*center,diagonal,*minimum,*maximum)
    # Vertex/index byte pools are relocated intact, including every UV/color,
    # not reconstructed through the preview renderer's subset of attributes.
    for off,size in ((96,100),(104,108)):
        assert original[u(off):u(off)+u(size)]==data[u(off)+delta:u(off)+delta+u(size)]
    assert original[u(80):insertion]==data[u(80):insertion]
    target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    before,after=read(source),read(target)
    assert after['piece_count']==before['piece_count']
    assert after['parts'][0]['locators'][:-1]==before['parts'][0]['locators']
    assert after['locator_count']==before['locator_count']+1
    assert after['parts'][0]['locators'][-1]['hookup_offset']==0
    assert geometry(source)==geometry(target),'Vertex or triangle data changed'
    return {'source_sha256':before['sha256'],'output_sha256':after['sha256'],
            'original_geometry_and_locators_preserved':True,'bytes_added':len(data)-len(original),
            'vertex_and_index_pools_byte_identical':True,**validate_layout(target),
            'camera_locator':after['parts'][0]['locators'][-1]}


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    inspect(a.root,a.output)
