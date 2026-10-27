"""Fit mirrored, rear-facing optional side cameras below the T680 NG mirrors."""
import argparse
import json
import math
import struct
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Vector,Matrix,Quaternion
from mathutils.bvhtree import BVHTree

sys.path.insert(0,str(Path(__file__).resolve().parent))
from scs_camera_mounts import geometry,read
from render_pp804395_mounts import load,material,convert


def frame(normal):
    z=-normal;up=Vector((0,1,0));y=(up-z*up.dot(z)).normalized();x=y.cross(z).normalized()
    return Matrix((x,y,z)).transposed()


def fit(tree,points,y,z):
    def surface(side,height,length):
        p,_,_,_=tree.ray_cast(Vector((side*3,height,length)),Vector((-side,0,0)),6)
        assert p is not None and side*p.x>.5,('No outside cab panel',side,height,length)
        return side*p.x
    base=[p for p in points if abs(p[2])<1e-6]
    hx=max(abs(p[0]) for p in base);hy=max(abs(p[1]) for p in base);radius=hx*.009/.053
    grid=[Vector((x,dy,0)) for x in np.linspace(-hx,hx,5) for dy in np.linspace(-hy,hy,5)
          if max(abs(x)-(hx-radius),0)**2+max(abs(dy)-(hy-radius),0)**2<=radius**2+1e-12]
    rotation=frame(Vector((1,0,0)))
    for _ in range(3):
        a=[];b=[]
        for p in grid:
            q=rotation @ p;height=y+q.y;length=z+q.z
            a.append((height-y,length-z,1));b.append((surface(1,height,length)+surface(-1,height,length))/2)
        slope=np.linalg.lstsq(np.array(a),np.array(b),rcond=None)[0]
        rotation=frame(Vector((1,-float(slope[0]),-float(slope[1]))).normalized())
    transformed=[rotation @ Vector(p) for p in points]
    contact=[rotation @ p for p in grid]
    x=max(surface(side,y+p.y,z+p.z)-p.x for p in transformed+contact for side in (-1,1))
    gaps={side:[x+p.x-surface(side,y+p.y,z+p.z) for p in contact] for side in (-1,1)}
    for side in (-1,1):
        assert min(gaps[side])<.001,(side,'mount not touching',min(gaps[side]))
        assert max(gaps[side])<.001,(side,'panel too curved',max(gaps[side]))
    return rotation,x,gaps


def run(stock,cache,output,palette):
    output.mkdir(parents=True,exist_ok=True)
    relative=Path('vehicle/truck/upgrade/bettergps')
    camera={396:cache/relative/'pp804396/pp804396_truck.pmg',397:cache/relative/'pp804397/pp804397_truck.pmg'}
    points={};faces={}
    for part in camera:points[part],faces[part]=geometry(camera[part])
    rows=[];reflect=Matrix.Diagonal((-1,1,1))
    mirror_path=stock/'accessory/mirror/standard_paint.pmg'
    mirror_points,mirror_faces=geometry(mirror_path)
    glass=read(mirror_path)['bounds']
    data=mirror_path.read_bytes();u=lambda at:struct.unpack_from('<I',data,at)[0]
    at=u(84)+4*100 # Verified stock main-mirror glass piece.
    left_glass=[struct.unpack_from('<3f',data,u(at+64)+i*u(at+60)) for i in range(u(at+4))]
    left_glass=[p for p in left_glass if p[0]<-1]
    reference_z=(min(p[2] for p in left_glass)+max(p[2] for p in left_glass))/2-.090
    for cabin in ('dc','s52m','s76h'):
        cab_path=stock/f'cabin/{cabin}.pmg';cab_points,cab_faces=geometry(cab_path)
        tree=BVHTree.FromPolygons([Vector(p) for p in cab_points],cab_faces,all_triangles=True)
        loc=next(l for p in read(cab_path)['parts'] for l in p['locators'] if l['name']=='mirror')
        # Use the left mirror shown in the user's screenshot, then mirror the
        # camera pose exactly to the right (stock mirrors are staggered).
        y=loc['position'][1]+glass[1]-.04
        requested_z=loc['position'][2]+reference_z
        for adjustment in (0,.005,.010,.015,.020,.025,.030):
            z=requested_z+adjustment
            try:right,x,gaps=fit(tree,points[396],y,z)
            except AssertionError as error:last_error=error;continue
            break
        else:raise last_error
        rotations={396:right.to_quaternion(),397:(reflect @ right @ reflect).to_quaternion()}
        mirror_world=[Quaternion(loc['rotation_wxyz']) @ Vector(p)+Vector(loc['position']) for p in mirror_points]
        mirror_tree=BVHTree.FromPolygons(mirror_world,mirror_faces,all_triangles=True)
        for part,side in ((396,1),(397,-1)):
            rotation=rotations[part];position=Vector((side*x,y,z))
            world=[position+rotation @ Vector(p) for p in points[part]]
            cam_tree=BVHTree.FromPolygons(world,faces[part],all_triangles=True)
            assert not cam_tree.overlap(mirror_tree),(cabin,part,'camera intersects mirror')
            outward=rotation @ Vector((0,0,-1))
            forward=rotation @ Vector((1 if side==1 else -1,0,0))
            assert outward.x*side>.8 and forward.z>.8,(part,'socket visibility or housing direction is wrong')
            rows.append({'cabin':cabin,'part_number':f'PP804{part}','side':'right' if side==1 else 'left',
                'locator':'bgps_right' if side==1 else 'bgps_left','position_scs':list(position),
                'fore_aft_fit_adjustment_m':adjustment,
                'rotation_wxyz':list(rotation),'camera_bounds_scs':[[min(p[i] for p in world) for i in range(3)],
                    [max(p[i] for p in world) for i in range(3)]],
                'mount_contact_gap_min_m':min(gaps[side]),'mount_contact_gap_max_m':max(gaps[side]),
                'configurator_faces_outward':list(outward),'housing_rearward_direction':list(forward),
                'mirror_intersections':0,'price':250})
        # Validate symmetry of the actual converted vehicle meshes and poses.
        right_world=[Vector((x,y,z))+rotations[396] @ Vector(p) for p in points[396]]
        left_world=[Vector((-x,y,z))+rotations[397] @ Vector(p) for p in points[397]]
        assert len(right_world)==len(left_world)
        max_error=max(min((reflect @ a-b).length for b in left_world) for a in right_world)
        assert max_error<.000002,(cabin,'asymmetric side cameras',max_error)
        for row in rows[-2:]:row['mirror_symmetry_max_error_m']=max_error
        bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
        load(cab_path,material('cabin_context',(.38,.43,.47,1)))
        load(mirror_path,material('stock_mirror',(.09,.11,.13,1)),loc['position'],rotation=loc['rotation_wxyz'])
        cam_mat=material('camera_palette',(.03,.03,.03,1));bsdf=cam_mat.node_tree.nodes.get('Principled BSDF')
        tex=cam_mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(palette),check_existing=True)
        cam_mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
        for row in rows[-2:]:load(camera[int(row['part_number'][-3:])],cam_mat,row['position_scs'],palette=True,rotation=row['rotation_wxyz'])
        scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE_NEXT'
        scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
        scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.17,.19,.22,1)
        scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.6
        for pos,power in (((4,4,1),900),((-4,4,1),900),((0,5,-4),600)):
            bpy.ops.object.light_add(type='AREA',location=convert(pos));light=bpy.context.object;light.data.energy=power;light.data.size=4
            light.rotation_euler=(Vector(convert((0,1.8,-1.5)))-light.location).to_track_quat('-Z','Y').to_euler()
        bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam;cam.data.lens=55;cam.data.clip_start=.003
        for side in (1,-1):
            for view in ('profile','detail'):
                target=Vector(convert((side*x,y,z))) if view=='detail' else Vector(convert((0,1.9,-1.2)))
                cam.location=target+Vector(convert((side*.36,.09,.20) if view=='detail' else (side*4.2,.25,.1)))
                cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
                scene.render.filepath=str(output/(f'{cabin}-'+('right' if side==1 else 'left')+f'-{view}.png'))
                bpy.ops.render.render(write_still=True)
        print('SIDE_MOUNT_PASS',cabin,'positions',x,y,z,'max gap mm',max(max(v) for v in gaps.values())*1000)
    (output/'mounts.json').write_text(json.dumps(rows,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('stock','cache','output','palette'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);run(a.stock.resolve(),a.cache.resolve(),a.output.resolve(),a.palette.resolve())
