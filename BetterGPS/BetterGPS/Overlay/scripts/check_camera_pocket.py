"""Trace one-sided opaque rays through converted camera window border gaps."""
import argparse,json,math,re,struct,zipfile
from pathlib import Path


def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])


def opaque_triangles(read,part):
    path=f'vehicle/truck/upgrade/bettergps/{part}/{part}_lod0'
    materials=re.findall(rb'/automat/[^\0]+\.mat',read(path+'.pmd'))
    glass={i for i,p in enumerate(materials) if '"eut2.glass.rfx"' in read(p.decode().lstrip('/')).decode()}
    data=read(path+'.pmg');u=lambda n:struct.unpack_from('<I',data,n)[0]
    triangles=[]
    for k in range(u(4)):
        piece=u(84)+100*k
        if u(piece+16) in glass:continue
        stride=u(piece+60)
        vertices=[struct.unpack_from('<3f',data,u(piece+64)+i*stride) for i in range(u(piece+4))]
        normals=[struct.unpack_from('<3f',data,u(piece+68)+i*stride) for i in range(u(piece+4))]
        indices=struct.unpack_from('<'+'H'*u(piece),data,u(piece+96))
        for i in range(0,len(indices),3):
            ids=indices[i:i+3];a,b,c=[vertices[j] for j in ids]
            e1=sub(b,a);e2=sub(c,a);normal=cross(e1,e2)
            average=tuple(sum(normals[j][axis] for j in ids) for axis in range(3))
            if dot(normal,average)<0:normal=tuple(-n for n in normal)
            triangles.append((a,e1,e2,normal))
    return triangles


def hit(triangles,origin,direction):
    for a,e1,e2,normal in triangles:
        if dot(normal,direction)>=-1e-12:continue
        p=cross(direction,e2);det=dot(e1,p)
        if abs(det)<1e-14:continue
        t=sub(origin,a);u=dot(t,p)/det
        if not 0<=u<=1:continue
        q=cross(t,e1);v=dot(direction,q)/det
        if v<0 or u+v>1:continue
        distance=dot(e2,q)/det
        if 1e-7<distance<.015:return True
    return False


def probes(read,front_angle,include_low=True):
    output=[]
    # Oblique rays enter through the visible seal aperture, then skim beyond
    # the slightly smaller PCB. Frontal rays alone are blocked by the seal.
    points=[(x,z,.11*(1 if x>0 else -1),0) for x in (-.0366,.0366) for z in (-.012,0,.012)]
    points.extend((x,z,0,.10*(1 if z>0 else -1)) for x in (-.02,0,.02) for z in (-.0202,.0202))
    models=[('pp804395',math.pi/4),('pp807032',math.radians(front_angle))]
    if include_low:models.append(('pp807032_low',math.radians(front_angle)))
    for part,angle in models:
        triangles=opaque_triangles(read,part);escaped=[]
        co,si=math.cos(angle),math.sin(angle)
        for x,dz,dx,dz_direction in points:
            y=-.0278
            origin=(-x*.625,(.072+y*si+dz*co-(.072 if part.startswith('pp807032') else 0))*.625,(y*co-dz*si)*.625)
            direction=(-dx,si+dz_direction*co,co-dz_direction*si)
            length=math.sqrt(dot(direction,direction));direction=tuple(n/length for n in direction)
            if not hit(triangles,origin,direction):escaped.append([x,dz])
        output.append({'model':part,'opaque_front_face_rays':len(points),'escaped_rays':escaped})
    return output


def run(mod,authoring,baseline):
    angle=json.loads((authoring.parent/'pp807032/user-adjustments.json').read_text())['head_down_degrees']
    include_low=(mod/'vehicle/truck/upgrade/bettergps/pp807032_low/pp807032_low_lod0.pmg').exists()
    current=probes(lambda name:(mod/name).read_bytes(),angle,include_low=include_low)
    assert all(not row['escaped_rays'] for row in current),current
    result={'status':'PASS','checks':current,'transparent_glass_excluded':True,'backfaces_culled':True}
    if baseline:
        with zipfile.ZipFile(baseline) as archive:
            before=probes(archive.read,angle,include_low='vehicle/truck/upgrade/bettergps/pp807032_low/pp807032_low_lod0.pmg' in archive.namelist())
        result['baseline']=before
        assert any(row['escaped_rays'] for row in before),'Baseline did not reproduce the visible opening'
    (authoring/'pocket-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print('POCKET_PASS:',sum(row['opaque_front_face_rays'] for row in current),'front-facing opaque native rays; no window-border see-through.')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('mod','authoring'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--baseline',type=Path)
    a=p.parse_args();run(a.mod.resolve(),a.authoring.resolve(),a.baseline)
