"""Check camera fixing clearances and cable routing in saved editable meshes.

Run in background Blender; inspects saved geometry, not generator constants.
"""
import argparse,json,sys
from pathlib import Path
import bpy
import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree


def triangles(obj):
    obj.data.calc_loop_triangles()
    return [obj.matrix_world@v.co for v in obj.data.vertices],[tuple(t.vertices) for t in obj.data.loop_triangles]


def tree(objects):
    points=[];faces=[]
    for obj in objects:
        v,f=triangles(obj);start=len(points);points.extend(v)
        faces.extend(tuple(start+i for i in t) for t in f)
    return BVHTree.FromPolygons(points,faces,all_triangles=True)


def run(root):
    checks=[]
    for name,head,count in [('pp804395','rear_wall_fixing',2),('pp804396','flange_corner_fixing',4),('pp804397','flange_corner_fixing',4),('pp807032','grille_strap_fixing',2),('pp807032_low','grille_strap_fixing',2)]:
        bpy.ops.wm.open_mainfile(filepath=str(root/(name+'_editable.blend')))
        objects=list(bpy.data.objects);heads=[o for o in objects if o.name.split('.')[0]==head]
        assert len(heads)==count,(name,head,len(heads))
        assert not any('cross_recess' in o.name for o in objects),'Obsolete Phillips heads'
        for obj in heads:assert bpy.data.objects.get(obj.name.split('.')[0]+'_hex_socket_floor') or any(o.name.startswith(head+'_hex_socket_floor') for o in objects)
        if name in ('pp804395','pp807032','pp807032_low'):
            housing=bpy.data.objects['rounded_camera_shell']
            bm=bmesh.new();bm.from_mesh(housing.data)
            assert all(e.is_manifold for e in bm.edges),(name,'open edge in optical pocket')
            bm.free()
            roles=housing.data.attributes.get('finish_role')
            assert roles and any(v.value==4 for v in roles.data),(name,'no interior faces assigned to cavity material')
            for p in housing.data.polygons:
                if roles.data[p.index].value==4:
                    mat=housing.data.materials[p.material_index]
                    # Saved editable preview can be inspected without enabling
                    # the addon. Native material values are checked separately.
                    assert mat.name=='Dark_optical_cavity',(name,'wrong cavity finish')
            opaque=tree([housing]);direction=(housing.matrix_world.to_3x3()@Vector((0,1,0))).normalized()
            probes=[(x,z) for x in (-.0372,.0372) for z in (-.012,0,.012)]
            probes.extend((x,z) for x in (-.02,0,.02) for z in (-.02075,.02075))
            for x,z in probes:
                origin=housing.matrix_world@Vector((x,-.028,.071+z))
                hit,normal,_,distance=opaque.ray_cast(origin,direction,.015)
                assert hit and normal.dot(direction)<-.8,(name,'backface-culling ray escapes optical pocket',x,z)
            checks.append({'model':name,'closed_optical_pocket':True,'front_facing_opaque_probe_hits':len(probes)})
        if name.startswith('pp807032'):
            assert not any(o.name.startswith(('wider_grille_flat','straight_grille_strap_tip')) for o in objects),'Narrowed insertion geometry remains'
            expected_width,expected_thickness=(.0055,.0015) if name.endswith('_low') else (.0105,.0025)
            for obj in objects:
                if obj.name.startswith('straight_grille_flat'):
                    points=[obj.matrix_world@v.co for v in obj.data.vertices]
                    width=max(p.z for p in points)-min(p.z for p in points)
                    thickness=max(p.x for p in points)-min(p.x for p in points)
                    assert abs(width-expected_width)<.000001 and abs(thickness-expected_thickness)<.000001,(name,'wrong constant section',width,thickness)
                    for axis in ('x','z'):
                        values=[getattr(p,axis) for p in points];lo,hi=min(values),max(values)
                        assert all(min(abs(v-lo),abs(v-hi))<.0000001 for v in values),(name,'flat narrows along its length')
        if name in ('pp804396','pp804397'):
            assert not any(o.name.startswith('small_housing_label') for o in objects),(name,'removable label remains')
            ball=bpy.data.objects[name.upper()+'_OPTICAL_BALL']
            assert len(ball.children)>=18 and abs(ball['up_degrees']-57.337061)<.00001
            shell=[o for o in objects if o.name.startswith(('arched_housing','lower_housing_lip','fixed_socket_surround','fixed_front_rim'))]
            housing=tree(shell)
            for obj in heads:assert not tree([obj]).overlap(housing),(name,obj.name,'fixing head clips housing')
            cable=bpy.data.objects['side_routed_cable']
            assert not tree([cable]).overlap(housing),(name,'cable clips housing')
            assert bpy.data.objects.get('side_cable_grommet')
            checks.append({'model':name,'fixing_heads':count,'head_housing_intersections':0,'cable_housing_intersections':0,'grommet':True})
        elif name=='pp804395':
            cable=bpy.data.objects['rear_camera_cable']
            headparts=[o for o in objects if o.type=='MESH' and o.name.startswith(('rounded_camera_shell','sunshield','straight_mount_tab','pivot'))]
            assert not tree([cable]).overlap(tree(headparts)),(name,'cable clips head or mount tabs')
            assert bpy.data.objects.get('rear_cable_grommet')
            z=[sum((o.matrix_world@v.co).z for v in o.data.vertices)/len(o.data.vertices) for o in heads]
            assert all(abs(value-.055*.625)<.00001 for value in z),(name,'fixings must be in lower visible corners',z)
            checks.append({'model':name,'fixing_heads':count,'cable_head_and_tab_intersections':0,'central_grommet':True})
        else:
            assert bpy.data.objects.get('behind_grille_backing_bridge')
            checks.append({'model':name,'fixing_heads':count,'backing_bridge':True})
    (root/'editable-validation.json').write_text(json.dumps({'status':'PASS','checks':checks},indent=2)+'\n')
    print('EDITABLES_PASS',checks)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True)
    a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);run(a.root.resolve())
