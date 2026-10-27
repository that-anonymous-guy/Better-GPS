"""Create lightweight PP804395 SCS sources using the installed SCS Blender Tools.

Run with Blender 3.6 --factory-startup --python-exit-code 1. Addon is loaded
from an isolated working copy; user preferences and the original UV kit stay
unchanged. A tiny color/specular palette uses simple per-part UV samples.
"""
import argparse
import json
import math
from pathlib import Path
import struct
import sys

import bpy
from mathutils import Vector,Matrix


def cube(name,loc,size,bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    obj=bpy.context.object;obj.name=name;obj.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod=obj.modifiers.new('small_bevel','BEVEL');mod.width=bevel;mod.segments=1
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def disc(name,loc,radius,depth,vertices=8,rot=(math.pi/2,0,0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,
                                      location=loc,rotation=rot)
    obj=bpy.context.object;obj.name=name
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    return obj


def palette_uv(obj,slot):
    while obj.data.uv_layers:obj.data.uv_layers.remove(obj.data.uv_layers[0])
    uv=obj.data.uv_layers.new(name='palette')
    # Each part samples the centre of one solid swatch, no atlas unwrap required.
    for loop in uv.data:loop.uv=((slot+0.5)/8,0.5)
    colors=obj.data.color_attributes.new(name='Col',type='FLOAT_COLOR',domain='CORNER')
    for c in colors.data:c.color=(0.21404114,0.21404114,0.21404114,1)
    alpha=obj.data.color_attributes.new(name='Col_alpha',type='FLOAT_COLOR',domain='CORNER')
    for c in alpha.data:c.color=(0.21404114,0.21404114,0.21404114,1)
    obj.data.update()


def geometry(lod):
    result=[]
    def add(obj,slot):result.append((obj,slot));return obj
    add(cube('housing',(0,0,.071),(.098,.060,.062),.004 if lod==0 else 0),0)
    add(cube('sunshield',(0,-.003,.110),(.112,.072,.006),.001 if lod==0 else 0),0)
    # Direct rear mounting: one flat plate and two straight side tabs to the
    # existing pivots. No lower U-bracket, bent adapter or horizontal shelf.
    add(cube('wall_plate',(0,.046,.071),(.120,.004,.045)),0)
    for side in (-1,1):
        add(cube('straight_mount_tab',(side*.052,.023,.071),(.006,.046,.012)),0)
    add(cube('front_insert',(0,-.0308,.071),(.079,.002,.046)),1)
    add(disc('lens_bezel',(0,-.033,.071),.013,.003,12 if lod==0 else 8),1)
    add(disc('lens_glass',(0,-.035,.071),.008,.001,12 if lod==0 else 8),3)
    if lod==0:
        add(disc('lens_pupil',(0,-.0356,.071),.004,.0003,12),4)
        for side in (-1,1):
            add(cube('shield_side',(side*.052,-.031,.103),(.006,.016,.016)),0)
            add(disc('pivot',(side*.052,0,.072),.021,.005,12,(0,math.pi/2,0)),0)
            add(disc('pivot_bolt',(side*.056,0,.072),.005,.005,6,(0,math.pi/2,0)),2)
            for n in range(9):
                angle=-math.pi/2+math.pi*n/8
                x=side*(.026+.005*math.cos(angle));z=.071+.021*math.sin(angle)
                # Front-only IR lenses: the hidden back is covered by the insert.
                vertices=[(x+.0035*math.cos(a*math.tau/8),-.0321,z+.0035*math.sin(a*math.tau/8)) for a in range(8)]
                mesh=bpy.data.meshes.new('ir_lens');mesh.from_pydata(vertices,[],[list(range(8))]);mesh.update()
                obj=bpy.data.objects.new('ir_led',mesh);bpy.context.collection.objects.link(obj)
                # Winding above points toward +Y; reverse for the exterior -Y face.
                # X/Z CCW yields -Y, so these fronts already face outward.
                add(obj,2)
        add(disc('connector',(0,.035,.071),.007,.012,8),1)
    return result


def build(addon,project,output):
    palette_dir=project/'vehicle/truck/upgrade/bettergps/pp804395'
    palette_dir.mkdir(parents=True,exist_ok=True)
    palette=palette_dir/'palette.tga'
    if not palette.exists():
        colors=[(30,33,36,32),(14,16,17,14),(170,177,180,190),(25,53,64,245),
                (5,9,16,220),(60,62,65,80),(80,83,86,100),(255,0,255,255)]
        header=struct.pack('<BBBHHBHHHHBB',0,0,2,0,0,0,0,0,128,16,32,0x28)
        pixels=bytes(c for _y in range(16) for x in range(128)
                     for c in (colors[x//16][2],colors[x//16][1],colors[x//16][0],colors[x//16][3]))
        palette.write_bytes(header+pixels)
    if not (palette_dir/'palette.png').exists():
        image=bpy.data.images.load(str(palette));image.filepath_raw=str(palette_dir/'palette.png')
        image.file_format='PNG';image.save();bpy.data.images.remove(image)
    sys.path.insert(0,str(addon.parent))
    import addon_utils
    addon_utils.enable('io_scs_tools',default_set=True)
    from io_scs_tools.internals.callbacks import persistent
    persistent.disable()
    from io_scs_tools.utils import get_scs_globals,save_scs_globals_to_blend
    from io_scs_tools.internals.containers import config
    from io_scs_tools.internals import shader_presets
    from io_scs_tools.utils import material as material_utils
    from io_scs_tools.exp import pix
    from io_scs_tools.internals import looks
    from io_scs_tools.utils import printout
    # Blender Tools' progress messenger tries to redraw a window even in
    # background mode. Suppress only the UI redraw, retaining export logging.
    printout.immediate_messenger.can_report=lambda timeout:False
    g=get_scs_globals();g.config_update_lock=True;g.config_storage_place='BlendFile'
    g.scs_project_path=str(project);g.export_output_type='5';g.export_scale=1
    config.update_shader_presets_path(str(addon/'shader_presets.txt'))
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    mat=bpy.data.materials.new('pp804395_palette')
    preset=shader_presets.get_section('dif.spec')
    assert preset is not None,'SCS shader preset library unavailable'
    material_utils.set_shader_data_to_material(mat,preset)
    mat.scs_props.active_shader_preset_name='dif.spec'
    mat.scs_props.mat_effect_name='eut2.dif.spec'
    mat.scs_props.shader_attribute_diffuse=(1,1,1)
    mat.scs_props.shader_attribute_specular=(0.35,0.35,0.35)
    mat.scs_props.shader_attribute_shininess=40
    mat.scs_props.shader_texture_base='//vehicle/truck/upgrade/bettergps/pp804395/palette.tga'
    # Texture mappings are collections in Blender Tools. Assign the UV alias.
    for mapping in mat.scs_props.shader_texture_base_uv:
        mapping.value='palette'
    destination=project/'vehicle/truck/upgrade/bettergps/pp804395'
    destination.mkdir(parents=True,exist_ok=True);output.mkdir(parents=True,exist_ok=True)
    counts={}
    for lod in (0,1):
        parts=geometry(lod)
        # Only the camera head articulates around the two lateral pivots.
        # The wall plate, straight tabs and pivot fasteners stay fixed.
        pivot=Vector((0,0,.072))
        tilt=Matrix.Translation(pivot) @ Matrix.Rotation(math.pi/4,4,'X') @ Matrix.Translation(-pivot)
        fixed_names={'wall_plate','straight_mount_tab','pivot','pivot_bolt'}
        bpy.context.view_layer.update()
        fixed=[obj for obj,_ in parts if obj.name.split('.')[0] in fixed_names]
        before=[tuple(obj.matrix_world @ v.co) for obj in fixed for v in obj.data.vertices]
        for obj,_ in parts:
            if obj not in fixed:obj.matrix_world=tilt @ obj.matrix_world
        bpy.context.view_layer.update()
        assert before==[tuple(obj.matrix_world @ v.co) for obj in fixed for v in obj.data.vertices]
        for obj,slot in parts:
            palette_uv(obj,slot);obj.data.materials.clear();obj.data.materials.append(mat)
        bpy.ops.object.select_all(action='DESELECT')
        for obj,_ in parts:obj.select_set(True)
        bpy.context.view_layer.objects.active=parts[0][0]
        bpy.ops.object.join();obj=bpy.context.object;obj.name=f'camera_lod{lod}'
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        # Rear-facing tuning socket is yawed 180 degrees in SCS. Counter-yaw
        # the mesh (Blender Z == SCS Y) so lens placement stays unchanged.
        obj.data.transform(Matrix.Rotation(math.pi,4,'Z'))
        # 25% larger than the previous half-size version: 0.5 * 1.25 = 0.625.
        obj.data.transform(Matrix.Scale(0.625,4))
        obj.data.update()
        root=bpy.data.objects.new(f'pp804395_lod{lod}',None)
        bpy.context.collection.objects.link(root)
        bpy.context.view_layer.objects.active=root
        bpy.context.view_layer.update()
        root.scs_props.empty_object_type='SCS_Root'
        root.scs_props.scs_root_object_export_enabled=True
        root.scs_props.object_identity=root.name
        obj.parent=root
        bpy.context.view_layer.objects.active=root
        if not root.scs_object_part_inventory:root.scs_object_part_inventory.add()['name']='defaultpart'
        assert len(root.scs_object_part_inventory)==1
        obj.scs_props.scs_part='defaultpart'
        obj.data.calc_loop_triangles();counts[str(lod)]=len(obj.data.loop_triangles)
        looks.add_materials(root,[mat])
        bpy.context.view_layer.update()
        assert pix.export(str(destination),'',root,[obj]),'SCS export failed'
        root.hide_render=(lod!=0);obj.hide_render=(lod!=0)
        if lod:root.hide_set(True);obj.hide_set(True)
    # Conversion fixture: the official exporter/converter establishes hookup
    # string offsets and transforms for independent binary mounting checks.
    host=bpy.data.objects.new('hook_fixture',None);bpy.context.collection.objects.link(host)
    bpy.context.view_layer.objects.active=host;bpy.context.view_layer.update()
    host.scs_props.empty_object_type='SCS_Root'
    if not host.scs_object_part_inventory:host.scs_object_part_inventory.add()['name']='defaultpart'
    locator=bpy.data.objects.new('bgps_cam',None);bpy.context.collection.objects.link(locator)
    locator.parent=host;locator.scs_props.empty_object_type='Locator';locator.scs_props.locator_type='Model'
    locator.scs_props.scs_part='defaultpart'
    locator.scs_props.locator_model_hookup='lod_model_hookup : lod_model.bgpscam'
    locator.location=(.1,-.3,.2)
    sample=cube('fixture_mesh',(0,0,0),(.01,.01,.01));palette_uv(sample,0)
    sample.data.materials.append(mat);sample.parent=host;sample.scs_props.scs_part='defaultpart'
    looks.add_materials(host,[mat]);bpy.context.view_layer.update()
    pix.export(str(destination),'',host,[sample,locator])
    for item in (sample,locator,host):bpy.data.objects.remove(item,do_unlink=True)
    # Keep this source usable with the real addon in a later interactive session.
    save_scs_globals_to_blend()
    bpy.context.scene.render.engine='BLENDER_EEVEE'
    bpy.ops.object.select_all(action='DESELECT')
    root=bpy.data.objects['pp804395_lod0'];root.select_set(True)
    bpy.context.view_layer.objects.active=root
    for area in bpy.context.screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_distance=.35
            area.spaces.active.region_3d.view_location=Vector((0,0,.06))
    bpy.ops.wm.save_as_mainfile(filepath=str(output/'pp804395_scs.blend'))
    (output/'geometry-report.json').write_text(json.dumps({'triangles':counts,'materials':1,'texture_size':[128,16],
          'uniform_scale_relative_to_original':0.625,
          'camera_head_down_tilt_degrees':45,'fixed_mount_geometry_preserved':True,
          'pivot_scs_local_m':[0,.045,0],
          'mount_type':'flat wall plate with straight side tabs to pivots',
          'uv':'single palette sampling','source_uv_kit_preserved':True},indent=2)+'\n',encoding='utf-8')
    print('SCS_EXPORT_PASS',counts)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--addon',type=Path,required=True)
    parser.add_argument('--project',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    build(args.addon.resolve(),args.project.resolve(),args.output.resolve())
