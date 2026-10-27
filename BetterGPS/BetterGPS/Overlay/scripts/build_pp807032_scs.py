"""Rebuild the user's PP807032 front-camera sketch with coherent rigid parts.

Blender 3.6 + official SCS Tools. Dimensions are reconstruction values, not an
OEM drawing. The user-edited angle and straight strap length are recorded in
user-adjustments.json; the separate PP804395 rear model is never overwritten.
"""
import argparse,json,math,shutil,sys
from pathlib import Path
import bpy
from mathutils import Matrix,Vector
sys.path.insert(0,str(Path(__file__).resolve().parent))
from build_pp804395_scs import geometry,cube,palette_uv


def build(addon,output):
 output=output.resolve();output.mkdir(parents=True,exist_ok=True)
 prefs=json.loads((output/'user-adjustments.json').read_text())
 angle=math.radians(prefs['head_down_degrees']);length=prefs['strap_length_m']/.625
 project=output/'project';destination=project/'vehicle/truck/upgrade/bettergps/pp807032';destination.mkdir(parents=True,exist_ok=True)
 rear=output.parent/'pp804395/scs-light/project/vehicle/truck/upgrade/bettergps/pp804395'
 for name in ('palette.tga','palette.png'):shutil.copy2(rear/name,destination/name)
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
 g=get_scs_globals();g.config_update_lock=True;g.config_storage_place='BlendFile';g.scs_project_path=str(project);g.export_output_type='5';g.export_scale=1
 config.update_shader_presets_path(str(addon/'shader_presets.txt'))
 bpy.context.preferences.filepaths.save_version=0
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 mat=bpy.data.materials.new('PP807032_palette');material_utils.set_shader_data_to_material(mat,shader_presets.get_section('dif.spec'))
 mat.scs_props.active_shader_preset_name='dif.spec';mat.scs_props.mat_effect_name='eut2.dif.spec'
 mat.scs_props.shader_attribute_diffuse=(1,1,1);mat.scs_props.shader_attribute_specular=(.35,.35,.35);mat.scs_props.shader_attribute_shininess=40
 mat.scs_props.shader_texture_base='//vehicle/truck/upgrade/bettergps/pp807032/palette.tga'
 for mapping in mat.scs_props.shader_texture_base_uv:mapping.value='palette'
 counts={}
 poses=json.loads((output/'placement/mounts.json').read_text())
 low=next(p for p in poses if p['bumper']=='low_chrome_fog')['position_scs']
 standard=next(p for p in poses if p['bumper']=='standard_paint_fog')['position_scs']
 offset=[b-a for a,b in zip(low,standard)]
 for lod in (0,1):
  parts=[]
  for obj,slot in geometry(lod):
   if obj.name.split('.')[0] in ('wall_plate','straight_mount_tab'):
    bpy.data.objects.remove(obj,do_unlink=True)
   else:parts.append((obj,slot))
  # Narrow straight blades fit the real perforations; the broad mounting
  # washers at the camera pivots are retained from the user's sketch.
  for side in (-1,1):parts.append((cube('straight_grille_strap_'+('left' if side<0 else 'right'),(side*.0496,length/2,.072),(.0024,length,.0048)),0))
  pivot=Vector((0,0,.072));tilt=Matrix.Translation(pivot)@Matrix.Rotation(angle,4,'X')@Matrix.Translation(-pivot)
  basis=Matrix.Scale(.625,4)@Matrix.Translation(-pivot)@Matrix.Rotation(math.pi,4,'Z')
  bpy.context.view_layer.update()
  for obj,slot in parts:
   fixed=obj.name.startswith(('pivot','straight_grille'))
   if not fixed:obj.matrix_world=tilt@obj.matrix_world
   obj.matrix_world=basis@obj.matrix_world
   palette_uv(obj,slot);obj.data.materials.clear();obj.data.materials.append(mat)
  root=bpy.data.objects.new(f'pp807032_lod{lod}',None);bpy.context.collection.objects.link(root)
  bpy.context.view_layer.objects.active=root;bpy.context.view_layer.update();root.scs_props.empty_object_type='SCS_Root'
  if not root.scs_object_part_inventory:root.scs_object_part_inventory.add()['name']='defaultpart'
  for obj,_ in parts:obj.parent=root;obj.scs_props.scs_part='defaultpart'
  looks.add_materials(root,[mat]);bpy.context.view_layer.update()
  if lod==0:
   save_scs_globals_to_blend()
   for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
     area.spaces.active.region_3d.view_distance=.23;area.spaces.active.region_3d.view_location=(0,-.02,0)
     area.spaces.active.region_3d.view_rotation=Vector((.15,.21,.09)).to_track_quat('Z','Y');area.spaces.active.shading.type='MATERIAL'
   bpy.ops.wm.save_as_mainfile(filepath=str(output/'pp807032_editable.blend'))
  bpy.ops.object.select_all(action='DESELECT')
  for obj,_ in parts:obj.select_set(True)
  bpy.context.view_layer.objects.active=parts[0][0];bpy.ops.object.join();obj=bpy.context.object;obj.name=f'camera_lod{lod}'
  bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
  obj.data.calc_loop_triangles();counts[str(lod)]=len(obj.data.loop_triangles)
  assert pix.export(str(destination),'',root,[obj])
  for extension in ('pim','pit'):
   exported=destination/(root.name+'.'+extension)
   exported.write_text(exported.read_text().rstrip()+'\n')
  if lod==0:
   # A direct accessory socket must live in the chassis. The standard bumper
   # family uses a translated copy to retain its independently measured pose.
   host=bpy.data.objects.new('pp807032_std',None);bpy.context.collection.objects.link(host)
   bpy.context.view_layer.objects.active=host;bpy.context.view_layer.update();host.scs_props.empty_object_type='SCS_Root'
   if not host.scs_object_part_inventory:host.scs_object_part_inventory.add()['name']='defaultpart'
   copy=obj.copy();copy.data=obj.data.copy();bpy.context.collection.objects.link(copy)
   copy.name='camera_standard_bumper';copy.parent=host;copy.data.transform(Matrix.Translation((offset[0],-offset[2],offset[1])))
   copy.scs_props.scs_part='defaultpart';looks.add_materials(host,[mat]);bpy.context.view_layer.update()
   assert pix.export(str(destination),'',host,[copy])
   for extension in ('pim','pit'):
    exported=destination/(host.name+'.'+extension);exported.write_text(exported.read_text().rstrip()+'\n')
   host.hide_set(True);copy.hide_set(True);host.hide_render=True;copy.hide_render=True
  if lod:root.hide_set(True);obj.hide_set(True);root.hide_render=True;obj.hide_render=True
 bpy.ops.object.select_all(action='DESELECT');bpy.data.objects['pp807032_lod0'].select_set(True);bpy.context.view_layer.objects.active=bpy.data.objects['pp807032_lod0']
 save_scs_globals_to_blend();bpy.ops.wm.save_as_mainfile(filepath=str(output/'pp807032_scs.blend'))
 (output/'geometry-report.json').write_text(json.dumps({'part_number':'PP807032','triangles':counts,'materials':1,'texture_size':[128,16],
  'user_head_down_degrees':prefs['head_down_degrees'],'strap_length_m':prefs['strap_length_m'],
  'strap_size_m':[.0015,prefs['strap_length_m'],.003],'strap_centres_spacing_m':.062,'pivot_origin':True,'front_scs':'-Z','up_scs':'+Y',
  'standard_bumper_model_offset_scs':offset,
  'uv':'constant palette swatches; no complex atlas','dimensions_status':'user sketch and existing camera reconstruction; not OEM dimensions'},indent=2)+'\n')
 print('FRONT_MODEL_PASS',counts)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--addon',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);build(a.addon.resolve(),a.output.resolve())
