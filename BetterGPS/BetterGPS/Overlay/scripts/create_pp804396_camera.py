"""Reconstruct the PP804396 right-side camera with simple palette UVs.

The exterior dimensions and six visible IR modules are photo-derived design
choices, not published OEM dimensions/specifications. The optical ball follows
the user's edited orientation independently of the fixed enclosure and base.
Run with Blender 3.6 --background --factory-startup --python-exit-code 1.
"""
import argparse
import json
import math
from pathlib import Path
import struct
import sys

import bpy
from mathutils import Vector,Matrix

USER_ADJUSTMENTS={}
MODEL_SCALE=.65


def ball_up_degrees():
    return -math.degrees(USER_ADJUSTMENTS.get('optical_ball_rotation_euler',[-math.radians(25),0,0])[0])


def apply_ir_template(obj,name,lod):
    template=USER_ADJUSTMENTS.get('ir_mesh_templates',{}).get(name)
    if not template:return obj
    if not lod:
        obj.data.clear_geometry();obj.data.from_pydata(template['vertices'],[],template['faces']);obj.data.update()
        for p in obj.data.polygons:p.use_smooth=len(p.vertices)==4
    else:
        # Keep the distant module's six sides, with the edited depth/inset.
        low,high=template['mesh_bounds'][2]
        old_low=min(v.co.z for v in obj.data.vertices);old_high=max(v.co.z for v in obj.data.vertices)
        for v in obj.data.vertices:v.co.z=low+(v.co.z-old_low)/(old_high-old_low)*(high-low)
    return obj

PALETTE=[(47,49,52,45),(15,17,19,22),(22,44,54,225),(154,146,125,175),
         (105,115,117,180),(5,10,14,225),(173,177,173,20),(28,30,32,45)]
SLOTS=['coque','caoutchouc','verre','bague_metal','diode_IR','pupille','etiquette','joint']
DIMENSIONS={'base_width':84,'base_length':106,'base_thickness':6,
            'housing_width':64,'housing_height_above_base':52,'housing_length':89,
            'ball_diameter':46.6,'lens_outer_diameter':23,'mount_hole_diameter':6.4}


def mesh_object(name,vertices,faces,slot,smooth=False):
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
    obj['palette_slot']=slot
    for p in mesh.polygons:p.use_smooth=smooth
    return obj


def rounded_plate(name,width,length,r,z0,z1,slot,steps=5):
    loop=[]
    for cx,cy,start in ((width/2-r,length/2-r,0),(-width/2+r,length/2-r,90),
                        (-width/2+r,-length/2+r,180),(width/2-r,-length/2+r,270)):
        for i in range(steps+1):
            a=math.radians(start+i*90/steps);loop.append((cx+r*math.cos(a),cy+r*math.sin(a)))
    n=len(loop);v=[(x,y,z) for z in (z0,z1) for x,y in loop]
    f=[tuple(reversed(range(n))),tuple(range(n,2*n))]
    f.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
    return mesh_object(name,v,f,slot)


def cylinder(name,loc,radius,depth,slot,direction=(0,-1,0),n=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=n,radius=radius,depth=depth,location=loc)
    obj=bpy.context.object;obj.name=name
    obj.rotation_euler=Vector(direction).to_track_quat('Z','Y').to_euler()
    obj['palette_slot']=slot
    for p in obj.data.polygons:p.use_smooth=len(p.vertices)==4
    return obj


def ring(name,loc,outer,inner,depth,slot,direction=(0,-1,0),n=32):
    q=Vector(direction).to_track_quat('Z','Y');c=Vector(loc)
    v=[tuple(c+q @ Vector((r*math.cos(i*math.tau/n),r*math.sin(i*math.tau/n),z)))
       for z,r in ((-depth/2,outer),(depth/2,outer),(depth/2,inner),(-depth/2,inner)) for i in range(n)]
    f=[]
    for k in range(4):
        for i in range(n):f.append((k*n+i,k*n+(i+1)%n,((k+1)%4)*n+(i+1)%n,((k+1)%4)*n+i))
    obj=mesh_object(name,v,f,slot)
    for index,p in enumerate(obj.data.polygons):p.use_smooth=index//n in (0,2)
    return obj


def tube(name,points,radius,slot,n=8):
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.bevel_depth=radius
    curve.bevel_resolution=1;curve.resolution_u=1
    spline=curve.splines.new('POLY');spline.points.add(len(points)-1)
    for p,co in zip(spline.points,points):p.co=(*co,1)
    obj=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active=obj;obj.select_set(True)
    bpy.ops.object.convert(target='MESH');obj=bpy.context.object;obj['palette_slot']=slot
    obj.select_set(False)
    return obj


def make_geometry(lod=0):
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    root=bpy.data.objects.new('PP804396_ROOT',None);bpy.context.collection.objects.link(root)
    root['units']='metres';root['dimensions_status']='estimated from photos, not OEM dimensions'
    root['mounting_plane']='Z=0; fixed enclosure';root['optical_ball_up_degrees']=ball_up_degrees()
    parts=[]
    def add(obj,parent=root):obj.parent=parent;parts.append(obj);return obj
    add(rounded_plate('rubber_gasket',.084,.106,.009,0,.0018,1,4))
    base=add(rounded_plate('mounting_flange',.084,.106,.009,.0018,.006,0,6 if not lod else 3))
    # Actual through-holes, with small metal liners as in the reference photos.
    for x in (-.033,.033):
        for y in (-.045,.044):
            if not lod:
                cutter=cylinder('hole_cutter',(x,y,.004),.0032,.016,0,(0,0,1),16)
                bpy.context.view_layer.objects.active=base
                modifier=base.modifiers.new('mount_hole','BOOLEAN');modifier.operation='DIFFERENCE';modifier.object=cutter
                bpy.ops.object.modifier_apply(modifier=modifier.name)
                bpy.data.objects.remove(cutter,do_unlink=True)
                add(ring('mount_hole_liner',(x,y,.00605),.00355,.0026,.0006,3,(0,0,1),16))
    add(rounded_plate('lower_housing_lip',.073,.098,.014,.006,.009,7,6 if not lod else 3))
    # Half-elliptical cross-sections form the rounded moulded hood and tail.
    stations=[(-.040,.032,.052),(-.005,.032,.052),(.018,.0315,.051),(.030,.030,.048),
              (.039,.025,.040),(.045,.018,.029),(.048,.009,.015),(.049,.001,.002)]
    n=24 if not lod else 12;v=[]
    for k,(y,rx,rz) in enumerate(stations):
        for i in range(n+1):
            a=math.pi*i/n;z=.007+rz*math.sin(a)
            v.append((rx*math.cos(a),y+(.25*(z-.007) if k==0 else 0),z))
    f=[]
    for k in range(len(stations)-1):
        for i in range(n):
            a=k*(n+1)+i;f.append((a,a+n+1,a+n+2,a+1))
        a=k*(n+1);f.append((a+n,a+2*n+1,a+n+1,a))
    # Front is deliberately open: the fixed bezel surrounds the rotating ball.
    f.append(tuple(reversed(range((len(stations)-1)*(n+1),len(v)))))
    add(mesh_object('arched_housing',v,f,0,True))
    front_direction=(0,-math.cos(math.atan(.25)),math.sin(math.atan(.25)))
    # Close the moulded front between its D-shaped outline and circular socket.
    # Its upper edge leans backwards, matching the hood rather than floating
    # forward as an independent hoop.
    front_vertices=[];tilt=math.atan(.25);delta=.0325-.007
    for i in range(48 if not lod else 24):
        a=i*math.tau/(48 if not lod else 24);dx=math.cos(a);dz=math.sin(a)*math.cos(tilt)
        aa=dx*dx/.032**2+dz*dz/.052**2;bb=2*delta*dz/.052**2;cc=delta**2/.052**2-1
        t=(-bb+math.sqrt(bb*bb-4*aa*cc))/(2*aa)
        if dz<0:t=min(t,-delta/dz)
        z=.0325+t*dz
        front_vertices.extend(((t*dx,-.040+.25*(z-.007),z),
            (.0245*dx,-.034+.0245*math.sin(a)*math.sin(tilt),.0325+.0245*dz)))
    count=len(front_vertices)//2
    add(mesh_object('fixed_socket_surround',front_vertices,
        [(2*i,2*((i+1)%count),2*((i+1)%count)+1,2*i+1) for i in range(count)],0))
    add(ring('fixed_front_rim',(0,-.034,.0325),.0272,.0245,.0028,7,front_direction,40 if not lod else 20))
    ball=bpy.data.objects.new('PP804396_OPTICAL_BALL',None);bpy.context.collection.objects.link(ball)
    ball.parent=root;ball.location=(0,-.0275,.0325)
    ball['adjustment']='Local X rotation: negative values aim upward; default follows user-edited project'
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32 if not lod else 16,ring_count=16 if not lod else 8,radius=.0233)
    sphere=bpy.context.object;sphere.name='rotating_optical_ball';sphere['palette_slot']=1
    for p in sphere.data.polygons:p.use_smooth=True
    add(sphere,ball)
    # These coordinates are ball-local. All optics and IR pods rotate together.
    add(ring('lens_bezel',(0,-.0245,0),.0115,.0097,.0038,7,n=32 if not lod else 16),ball)
    add(cylinder('lens_glass',(0,-.0267,0),.0097,.0012,2,n=32 if not lod else 16),ball)
    if not lod:
        add(ring('inner_optical_ring',(0,-.0274,0),.0073,.0057,.0004,4,n=32),ball)
        add(cylinder('optical_pupil',(0,-.0277,0),.0057,.0003,5,n=32),ball)
        add(cylinder('lens_reflection',(0,-.0279,0),.0024,.0001,2,n=24),ball)
    for i in range(6):
        a=math.tau*i/6;radial=.0174
        p=Vector((radial*math.cos(a),-math.sqrt(.0233**2-radial**2),radial*math.sin(a)))
        direction=p.normalized()
        add(apply_ir_template(cylinder('IR_pod_'+str(i+1),p+direction*.0005,.0032,.0017,7,direction,12 if not lod else 6),'IR_pod_1',lod),ball)
        add(apply_ir_template(cylinder('IR_lens_'+str(i+1),p+direction*.0015,.002,.0008,4,direction,12 if not lod else 6),'IR_lens_1',lod),ball)
    ball.rotation_euler=USER_ADJUSTMENTS.get('optical_ball_rotation_euler',[-math.radians(25),0,0])
    if not lod:
        # Discreet top cover seams, approximated from the product photographs.
        xs=[-.009,-.0045,0,.0045,.009]
        label=[(x,y,.007+.052*math.sqrt(1-(x/.032)**2)+.00015) for y in (-.020,-.012) for x in xs]
        add(mesh_object('small_housing_label',label,[(i,i+1,i+6,i+5) for i in range(4)],6))
        for side in (-1,1):
            a=math.pi/2+side*.48;points=[]
            for y,rx,rz in stations[1:6]:points.append((rx*math.cos(a),y,.007+rz*math.sin(a)+.0001))
            add(tube('moulding_seam',points,.00028,7))
        add(cylinder('cable_gland',(0,.049,.004),.0036,.006,1,(0,1,0),16))
        add(tube('short_cable',[(0,.051,.004),(0,.060,.004),(.004,.064,.004),(.013,.066,.004)],.0017,1))
    bpy.context.view_layer.update()
    root.scale=(MODEL_SCALE,)*3
    bpy.context.view_layer.update()
    return root,ball,parts


def write_palette(output):
    target=output/'scs/project/vehicle/truck/upgrade/bettergps/pp804396';target.mkdir(parents=True,exist_ok=True)
    tga=target/'palette.tga'
    header=struct.pack('<BBBHHBHHHHBB',0,0,2,0,0,0,0,0,128,16,32,0x28)
    pixels=bytes(c for _ in range(16) for x in range(128)
                 for c in (PALETTE[x//16][2],PALETTE[x//16][1],PALETTE[x//16][0],PALETTE[x//16][3]))
    tga.write_bytes(header+pixels)
    image=bpy.data.images.load(str(tga),check_existing=False);image.name='PP804396_palette'
    assert len(image.pixels)==128*16*4
    _=image.pixels[0] # Force lazy image loading before changing its file path.
    image.filepath_raw=str(output/'palette.png');image.file_format='PNG';image.save()
    image.pack()
    return target,image


def assign_materials(parts,image):
    materials=[]
    for i,name in enumerate(SLOTS):
        mat=bpy.data.materials.new(name);mat.use_nodes=True
        mat.diffuse_color=tuple(c/255 for c in PALETTE[i][:3])+(1,)
        bsdf=mat.node_tree.nodes.get('Principled BSDF')
        bsdf.inputs['Roughness'].default_value=.65 if i in (0,1,6,7) else .16 if i in (2,4,5) else .3
        bsdf.inputs['Metallic'].default_value=.65 if i==3 else 0
        tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
        mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
        materials.append(mat)
    for obj in parts:
        slot=obj['palette_slot'];obj.data.materials.clear();obj.data.materials.append(materials[slot])
        while obj.data.uv_layers:obj.data.uv_layers.remove(obj.data.uv_layers[0])
        uv=obj.data.uv_layers.new(name='palette')
        for loop in uv.data:loop.uv=((slot+.5)/8,.5)


def focus_view():
    for area in bpy.context.screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active;space.region_3d.view_distance=.21*MODEL_SCALE
            space.region_3d.view_location=(0,.005*MODEL_SCALE,.03*MODEL_SCALE)
            space.region_3d.view_rotation=Vector((.15,-.215,.10)).to_track_quat('Z','Y')
            space.shading.type='MATERIAL'


def studio(output,root,parts):
    scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    scene.render.engine='BLENDER_EEVEE';scene.eevee.use_gtao=True;scene.eevee.gtao_distance=.02
    scene.eevee.gtao_factor=1.15;scene.eevee.taa_render_samples=128
    scene.render.resolution_x=1300;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
    scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.22,.25,.29,1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value=.45
    scene.view_settings.view_transform='Standard';scene.view_settings.look='Medium High Contrast'
    for location,power,size in (((.10,-.16,.22),.8,.20),((-.16,-.06,.12),.5,.18),((.04,.16,.20),.8,.16)):
        bpy.ops.object.light_add(type='AREA',location=location);light=bpy.context.object
        light.data.energy=power;light.data.size=size
        light.rotation_euler=(Vector((0,0,.025))-light.location).to_track_quat('-Z','Y').to_euler()
    bpy.ops.object.camera_add();camera=bpy.context.object;scene.camera=camera;camera.data.lens=58;camera.data.clip_start=.001
    for view,location in (('preview',(.15,-.21,.13)),('front',(0,-.24,.11)),('side',(.25,-.04,.07))):
        camera.location=Vector(location)*MODEL_SCALE;camera.rotation_euler=(Vector((0,.005,.026))*MODEL_SCALE-camera.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(output/f'pp804396_{view}.png');bpy.ops.render.render(write_still=True)
    # Restore the reference-matching three-quarter view in the saved project.
    camera.location=Vector((.15,-.21,.13))*MODEL_SCALE;camera.rotation_euler=(Vector((0,.005,.026))*MODEL_SCALE-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.film_transparent=True
    scene.render.resolution_x=256;scene.render.resolution_y=256
    (output/'icon').mkdir(exist_ok=True)
    scene.render.filepath=str(output/'icon/pp804396_square.png');bpy.ops.render.render(write_still=True)
    scene.render.film_transparent=False
    scene.render.resolution_x=1300;scene.render.resolution_y=1000
    bpy.ops.object.select_all(action='DESELECT')
    for obj in root.children_recursive:obj.select_set(True)
    root.select_set(True);bpy.context.view_layer.objects.active=parts[0]
    focus_view()
    bpy.ops.wm.save_as_mainfile(filepath=str(output/'pp804396_reconstruction.blend'))
    bpy.ops.export_scene.gltf(filepath=str(output/'pp804396_reconstruction.glb'),export_format='GLB',use_selection=True,
                              export_materials='EXPORT',export_cameras=False,export_lights=False,export_extras=True)


def export_scs(output,addon):
    # The authoring file is already saved; discard its plain PBR materials
    # before addon registration, which otherwise tries SCS node callbacks on them.
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    for material in list(bpy.data.materials):bpy.data.materials.remove(material)
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from build_pp804395_scs import palette_uv
    sys.path.insert(0,str(addon.parent));import addon_utils
    addon_utils.enable('io_scs_tools',default_set=True)
    from io_scs_tools.internals.callbacks import persistent
    persistent.disable()
    from io_scs_tools.utils import get_scs_globals,save_scs_globals_to_blend
    from io_scs_tools.internals.containers import config
    from io_scs_tools.internals import shader_presets,looks
    from io_scs_tools.utils import material as material_utils,printout
    from io_scs_tools.exp import pix
    printout.immediate_messenger.can_report=lambda timeout:False
    g=get_scs_globals();g.config_update_lock=True;g.config_storage_place='BlendFile'
    g.scs_project_path=str(output/'scs/project');g.export_output_type='5';g.export_scale=1
    config.update_shader_presets_path(str(addon/'shader_presets.txt'))
    mat=bpy.data.materials.new('PP804396_SCS_palette')
    material_utils.set_shader_data_to_material(mat,shader_presets.get_section('dif.spec'))
    mat.scs_props.active_shader_preset_name='dif.spec';mat.scs_props.mat_effect_name='eut2.dif.spec'
    mat.scs_props.shader_attribute_diffuse=(1,1,1);mat.scs_props.shader_attribute_specular=(.35,.35,.35)
    mat.scs_props.shader_attribute_shininess=35
    mat.scs_props.shader_texture_base='//vehicle/truck/upgrade/bettergps/pp804396/palette.tga'
    for mapping in mat.scs_props.shader_texture_base_uv:mapping.value='palette'
    counts={}
    for variant,lod in ((396,1),(396,0),(397,1),(397,0)):
        root,ball,parts=make_geometry(lod)
        for obj in parts:
            palette_uv(obj,obj['palette_slot']);obj.data.materials.clear();obj.data.materials.append(mat)
        bpy.ops.object.select_all(action='DESELECT')
        for obj in parts:obj.select_set(True)
        bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();obj=bpy.context.object
        world=obj.matrix_world.copy();obj.parent=None;obj.matrix_world=world
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        if variant==397:
            obj.data.transform(Matrix.Diagonal((-1,1,1,1)));obj.data.flip_normals();obj.data.update()
        for empty in (ball,root):bpy.data.objects.remove(empty,do_unlink=True)
        host=bpy.data.objects.new(f'pp804{variant}_lod{lod}',None);bpy.context.collection.objects.link(host)
        bpy.context.view_layer.objects.active=host;bpy.context.view_layer.update()
        host.scs_props.empty_object_type='SCS_Root'
        if not host.scs_object_part_inventory:host.scs_object_part_inventory.add()['name']='defaultpart'
        obj.parent=host;obj.scs_props.scs_part='defaultpart'
        looks.add_materials(host,[mat]);bpy.context.view_layer.update()
        destination=output/f'scs/project/vehicle/truck/upgrade/bettergps/pp804{variant}'
        destination.mkdir(parents=True,exist_ok=True)
        assert pix.export(str(destination),'',host,[obj])
        obj.data.calc_loop_triangles();counts[str(lod) if variant==396 else 'left_'+str(lod)]=len(obj.data.loop_triangles)
        if lod==0:
            save_scs_globals_to_blend();focus_view()
            bpy.ops.wm.save_as_mainfile(filepath=str(output/f'scs/pp804{variant}_scs.blend'))
        # Truck sockets face outward from the wall, so configurator pins are
        # visible from the corresponding side. Reorient the mesh inside them
        # independently; the camera housing continues to point rearwards.
        vehicle_basis=Matrix(((0,-1,0,0),(0,0,1,0),(-1,0,0,0),(0,0,0,1))) if variant==396 else Matrix(((0,1,0,0),(0,0,1,0),(1,0,0,0),(0,0,0,1)))
        obj.data.transform(vehicle_basis);obj.data.update()
        host.name=f'pp804{variant}_truck'+('_lod1' if lod else '')
        bpy.context.view_layer.update()
        assert pix.export(str(destination),'',host,[obj])
        if lod==0:
            bpy.ops.wm.save_as_mainfile(filepath=str(output/f'scs/pp804{variant}_truck_scs.blend'))
    return counts


def build(output,addon):
    global USER_ADJUSTMENTS
    output=output.resolve();output.mkdir(parents=True,exist_ok=True)
    preferences=output/'user-adjustments.json'
    USER_ADJUSTMENTS=json.loads(preferences.read_text()) if preferences.exists() else {}
    import addon_utils
    if 'io_scs_tools' in bpy.context.preferences.addons:
        addon_utils.disable('io_scs_tools',default_set=False)
    bpy.context.preferences.filepaths.save_version=0
    root,ball,parts=make_geometry(0)
    _,image=write_palette(output);assign_materials(parts,image)
    for obj in parts:obj.data.calc_loop_triangles()
    triangles=sum(len(obj.data.loop_triangles) for obj in parts)
    studio(output,root,parts)
    counts=export_scs(output,addon.resolve()) if addon else {}
    report={'part_number':'PP804396','type':'right side view camera','units':'metres',
            'confirmed_features':['1/3 inch color CMOS','right side mounting','waterproof housing','night vision','paintable housing'],
            'estimated_dimensions_mm':DIMENSIONS,'dimensions_status':'photo-derived estimates; OEM enclosure dimensions unavailable',
            'current_uniform_scale':MODEL_SCALE,'current_base_size_mm':[84*MODEL_SCALE,106*MODEL_SCALE],
            'photo_interpretation':{'visible_IR_modules':6,'IR_count_is_not_an_OEM_specification':True},
            'user_adjustment':{'ball_up_degrees':ball_up_degrees(),'IR_depth_from_user_template':bool(USER_ADJUSTMENTS),
                               'housing_and_base_fixed':True,'separate_editable_ball_group':True},
            'blender_axes':{'front':'-Y','up':'+Z','mounting_plane':'Z=0'},
            'source_triangles':triangles,'scs_lod_triangles':counts,'uv':'8 solid palette swatches, no complex atlas',
            'source_urls':['https://peterbiltparts.com/products/pana-pacific-camera-kit-pp804396',
              'https://www.panapacific.com/products/pro-view-right-side-view-color-cmos-camera-kit-for-kenworth-navplus-system-or-peterbilt-smartnav-system-and-for-kenworth-navplus-hd-system-or-peterbilt-smartnav-2nd-generation-system']}
    (output/'pp804396_dimensions.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('MODEL_PASS',triangles,'triangles; SCS LODs',counts)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--scs-addon',type=Path)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);build(args.output,args.scs_addon)
