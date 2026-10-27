"""Measure and render three stock T680 NG rear-wall camera mounts offline."""
import argparse
import json
import math
from pathlib import Path
import struct
import sys

import bpy
from mathutils import Vector,Quaternion
from mathutils.bvhtree import BVHTree

sys.path.insert(0,str(Path(__file__).resolve().parent))
from scs_camera_mounts import geometry,read


def convert(p):return (p[0],-p[2],p[1])


def material(name,color):
    m=bpy.data.materials.new(name);m.use_nodes=True
    bsdf=m.node_tree.nodes.get('Principled BSDF');bsdf.inputs['Base Color'].default_value=color
    bsdf.inputs['Roughness'].default_value=.45
    return m


def load(path,mat,offset=(0,0,0),palette=None,rotation=(1,0,0,0)):
    data=path.read_bytes();u=lambda p:struct.unpack_from('<I',data,p)[0]
    objects=[]
    for i in range(u(4)):
        at=u(84)+100*i
        count,stride,pos,uv=u(at+4),u(at+60),u(at+64),u(at+72)
        points=[convert(Quaternion(rotation) @ Vector(struct.unpack_from('<3f',data,pos+j*stride))+Vector(offset)) for j in range(count)]
        indices=struct.unpack_from('<'+'H'*u(at),data,u(at+96))
        faces=list(zip(indices[::3],indices[1::3],indices[2::3]))
        mesh=bpy.data.meshes.new(path.stem);mesh.from_pydata(points,[],faces);mesh.update()
        obj=bpy.data.objects.new(path.stem,mesh);bpy.context.collection.objects.link(obj)
        obj.data.materials.append(mat)
        if palette and uv!=0xffffffff:
            layer=mesh.uv_layers.new(name='palette')
            for loop in mesh.loops:
                v=struct.unpack_from('<2f',data,uv+loop.vertex_index*stride)
                layer.data[loop.index].uv=(v[0],1-v[1])
        objects.append(obj)
    return objects


def fit_mount(tree,rear_max,base_points,contact,contact_min,contact_max,rear_rotation,y):
    def wall(x,height):
        hit,_,_,_=tree.ray_cast(Vector((x,height,rear_max+2)),Vector((0,0,-1)),5)
        assert hit is not None,('no cab wall behind mount',x,height)
        return float(hit.z)
    angle=0
    for _ in range(4):
        tilt=Quaternion((1,0,0),angle)
        tilted_contact=[tilt @ p for p in contact]
        lo=min(p.y for p in tilted_contact);hi=max(p.y for p in tilted_contact)
        angle=math.atan2(wall(0,y+hi)-wall(0,y+lo),hi-lo)
    tilt=Quaternion((1,0,0),angle)
    points=[tilt @ p for p in base_points]
    contact_z=min(p.z for p in contact)
    samples=[];constraints=[]
    for xi in range(5):
        x=contact_min[0]+(contact_max[0]-contact_min[0])*xi/4
        for yi in range(5):
            local_y=contact_min[1]+(contact_max[1]-contact_min[1])*yi/4
            p=tilt @ Vector((x,local_y,contact_z))
            z=wall(p.x,y+p.y);samples.append(z);constraints.append(z-p.z)
    depth=max(constraints+[wall(p.x,y+p.y)-p.z for p in points])
    return (tilt @ rear_rotation,points,angle,samples,[depth-c for c in constraints],[0,y,depth])


def run(stock,converted,output,palette):
    output.mkdir(parents=True,exist_ok=True)
    cam_path=converted/'vehicle/truck/upgrade/bettergps/pp804395/pp804395_lod0.pmg'
    raw_points,_=geometry(cam_path)
    rear_rotation=Quaternion((0,0,1,0))
    base_points=[rear_rotation @ Vector(p) for p in raw_points]
    base_min=[min(p[i] for p in base_points) for i in range(3)]
    contact=[p for p in base_points if abs(p[2]-base_min[2])<1e-6]
    assert len(contact)>=4,'Mount contact face was not found'
    contact_min=[min(p[i] for p in contact) for i in range(2)]
    contact_max=[max(p[i] for p in contact) for i in range(2)]
    reference=json.loads((output.parent/'placement-reference.json').read_text())
    pivot_local=Vector(reference['pivot_scs_local_m'])
    rows=[]
    for cabin,rear in (('dc','day_standard2'),('s52m','standard_aircon'),('s76h','standard_aircon')):
        bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
        path=stock/f'cabin/{cabin}.pmg';points,faces=geometry(path)
        tree=BVHTree.FromPolygons([Vector(p) for p in points],faces,all_triangles=True)
        bounds=read(path)['bounds'];rear_max=bounds[5]
        top_candidates=[p[1] for p in points if abs(p[0])<.5 and p[2]>rear_max-.18]
        assert top_candidates,cabin
        rear_top=max(top_candidates)
        # Keep the approved Day Cab pivot height on every cabin. Fit only the
        # wall-plate inclination/depth; the camera head's 45-degree articulation
        # is already baked in the mesh and must not rotate the fixed bracket.
        sleeper_raise=.10 if cabin!='dc' else 0
        target_pivot_height=reference['pivot_height_scs_m']+sleeper_raise
        y=reference['socket_height_scs_m']+sleeper_raise
        for _ in range(5):
            rotation,cam_points,angle,samples,contact_gaps,position=fit_mount(
                tree,rear_max,base_points,contact,contact_min,contact_max,rear_rotation,y)
            if cabin!='dc':y=target_pivot_height-(rotation @ pivot_local).y
        rotation,cam_points,angle,samples,contact_gaps,position=fit_mount(
            tree,rear_max,base_points,contact,contact_min,contact_max,rear_rotation,y)
        pivot_world=Vector(position)+rotation @ pivot_local
        assert abs(pivot_world.y-target_pivot_height)<.00001,(cabin,'pivot height differs from requested height')
        cam_min=[min(p[i] for p in cam_points) for i in range(3)]
        cam_max=[max(p[i] for p in cam_points) for i in range(3)]
        assert max(samples)-min(samples)<.04,(cabin,'rear wall too curved',samples)
        wall=max(samples)
        assert min(contact_gaps)>=-1e-6 and min(contact_gaps)<.0001,(cabin,'mount is not touching the cab')
        assert max(contact_gaps)<.001,(cabin,'flat plate does not fit wall within 1 mm',max(contact_gaps))
        row={'cabin':cabin,'rear_top_y':rear_top,'wall_z':wall,'wall_samples_z':samples,
             'position_scs':position,'rotation_wxyz':list(rotation),
             'wall_pitch_degrees':math.degrees(angle),
             'height_reference':'approved Day Cab; sleepers raised 10 cm',
             'sleeper_raise_m':sleeper_raise,
             'pivot_height_scs_m':pivot_world.y,
             'camera_head_down_tilt_degrees':45,
             'optical_down_pitch_degrees':45+math.degrees(angle),
             'camera_size_m':[b-a for a,b in zip(cam_min,cam_max)],
             'mount_contact_sample_count':len(contact_gaps),
             'mount_contact_gap_min_m':min(contact_gaps),'mount_contact_gap_max_m':max(contact_gaps),
             'camera_bounds_scs':[[a+b for a,b in zip(cam_min,position)],[a+b for a,b in zip(cam_max,position)]],
             'roof_clearance_m':rear_top-(cam_max[1]+y)}
        assert row['roof_clearance_m']>.05
        # Test all camera vertices against the measured rear wall. The adapter's
        # contact face may touch the wall; the optical housing must stay outside.
        penetrations=[]
        for p in cam_points:
            world=[a+b for a,b in zip(p,position)]
            hit,_,_,_=tree.ray_cast(Vector((world[0],world[1],rear_max+2)),Vector((0,0,-1)),5)
            if hit is not None and world[2]<hit.z-.0001:penetrations.append(world)
        row['penetrating_camera_vertices']=len(penetrations)
        assert not penetrations,(cabin,'camera intersects rear wall',len(penetrations))
        rows.append(row)
        cabmat=material('stock_cabin_context',(.36,.42,.46,1))
        load(path,cabmat)
        locator=next(l for part in read(path)['parts'] for l in part['locators'] if l['name']=='rear_cab')
        equipment_points,equipment_faces=geometry(stock/f'accessory/rear_cab/{rear}.pmg')
        equipment_points=[Quaternion(locator['rotation_wxyz']) @ Vector(p)+Vector(locator['position']) for p in equipment_points]
        camera_faces=geometry(cam_path)[1]
        camera_tree=BVHTree.FromPolygons([Vector(position)+rotation @ Vector(p) for p in raw_points],camera_faces,all_triangles=True)
        equipment_tree=BVHTree.FromPolygons(equipment_points,equipment_faces,all_triangles=True)
        overlaps=camera_tree.overlap(equipment_tree)
        row['rear_equipment_intersecting_triangle_pairs']=len(overlaps)
        assert not overlaps,(cabin,'camera intersects stock rear equipment',len(overlaps))
        load(stock/f'accessory/rear_cab/{rear}.pmg',material('stock_rear_equipment',(.10,.13,.15,1)),
             locator['position'],rotation=locator['rotation_wxyz'])
        mat=material('camera_palette',(.03,.03,.03,1));bsdf=mat.node_tree.nodes.get('Principled BSDF')
        image=bpy.data.images.load(str(palette),check_existing=True)
        tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
        mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
        load(cam_path,mat,position,palette=True,rotation=rotation)
        scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE_NEXT'
        scene.render.resolution_x=1200;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
        scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.16,.18,.2,1)
        scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.5
        for xyz,power,size in (((-3,5,6),1000,5),((3,5,4),700,4)):
            bpy.ops.object.light_add(type='AREA',location=convert(xyz));light=bpy.context.object
            light.data.energy=power;light.data.size=size
            light.rotation_euler=(Vector(convert((0,2,rear_max)))-light.location).to_track_quat('-Z','Y').to_euler()
        bpy.ops.object.camera_add();camera=bpy.context.object;scene.camera=camera;camera.data.lens=55
        for view,distance in (('rear',max(5.2,(bounds[4]-bounds[1])*2.35)),('detail',.50),('mount',.22)):
            target=Vector(convert([position[i]+(cam_min[i]+cam_max[i])/2 for i in range(3)]))
            if view=='rear':target=Vector(convert((0,(bounds[4]+bounds[1])/2,rear_max)))
            camera.location=target+(Vector((distance,-distance*.35,distance*.1)) if view=='mount'
                                    else Vector((distance*.25,-distance,distance*.15)))
            camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
            camera.data.clip_start=.005
            scene.render.filepath=str(output/f'{cabin}-{view}.png')
            bpy.ops.render.render(write_still=True)
        print('PLACEMENT_PASS',cabin,position,'contact gaps mm',
              min(contact_gaps)*1000,max(contact_gaps)*1000,'penetrations',len(penetrations))
    (output/'mounts.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('stock','converted','output','palette'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
    run(a.stock.resolve(),a.converted.resolve(),a.output.resolve(),a.palette.resolve())
