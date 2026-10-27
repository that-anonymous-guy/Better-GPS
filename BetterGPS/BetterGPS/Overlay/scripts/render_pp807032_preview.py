"""Render the converted front camera and its transparent SCS shop icon."""
import argparse,sys
from pathlib import Path
import bpy
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).resolve().parent))
from render_pp804395_mounts import load,material,convert


def run(cache,output):
 output.mkdir(parents=True,exist_ok=True)
 camera=cache/'vehicle/truck/upgrade/bettergps/pp807032'
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 mat=material('camera_palette',(.03,.03,.03,1));tex=mat.node_tree.nodes.new('ShaderNodeTexImage')
 tex.image=bpy.data.images.load(str(camera/'palette.dds'))
 mat.node_tree.links.new(tex.outputs['Color'],mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'])
 load(camera/'pp807032_lod0.pmg',mat,palette=True)
 scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE_NEXT'
 scene.render.resolution_x=1200;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
 scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.22,.25,.29,1)
 for pos,power in (((-.15,.20,-.20),1),((.16,.1,-.05),.5),((0,.15,.15),.7)):
  bpy.ops.object.light_add(type='AREA',location=convert(pos));light=bpy.context.object
  light.data.energy=power;light.data.size=.15
  light.rotation_euler=(-light.location).to_track_quat('-Z','Y').to_euler()
 bpy.ops.object.camera_add(location=convert((-.16,.10,-.18)));cam=bpy.context.object;scene.camera=cam
 cam.data.lens=50;cam.data.clip_start=.001
 cam.rotation_euler=(Vector(convert((0,0,.018)))-cam.location).to_track_quat('-Z','Y').to_euler()
 scene.render.filepath=str(output/'pp807032_preview.png');bpy.ops.render.render(write_still=True)
 scene.render.film_transparent=True;scene.render.resolution_x=256;scene.render.resolution_y=256
 scene.render.filepath=str(output/'pp807032_icon.png');bpy.ops.render.render(write_still=True)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('cache','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);run(a.cache.resolve(),a.output.resolve())
