"""Render the actual SCS T680 and supplied camera meshes for optical calibration review.

Illustrative clay materials; geometry and attachment transforms come from PMG.
This is a pose/FOV review, not a simulated screenshot of the native camera feed.
"""
import argparse,json,math,re,struct,sys
from pathlib import Path
import bpy
from mathutils import Vector,Quaternion
sys.path.insert(0,str(Path(__file__).resolve().parent))
from scs_camera_mounts import read
from render_pp804395_mounts import load,material,convert
from prepare_camera_pose_assets import direction

COLORS={'forward':(.12,.7,1,1),'left':(.21,.9,.47,1),'right':(1,.55,.12,1),'reverse':(.83,.32,1,1)}

def loc(path,name):return next(l for p in read(path)['parts'] for l in p['locators'] if l['name']==name)

def compose(parent,p,q=(1,0,0,0)):
 return Vector(parent['position'])+Quaternion(parent['rotation_wxyz'])@Vector(p),Quaternion(parent['rotation_wxyz'])@Quaternion(q)

def stock_mesh(path,mat,root,p=(0,0,0),q=(1,0,0,0)):
 objects=load(path,mat,p,rotation=q)
 descriptor=path.with_suffix('.pmd')
 if not descriptor.exists():descriptor=root/Path(*path.parts[path.parts.index('vehicle'):]).with_suffix('.pmd')
 data=descriptor.read_bytes();mats=[s.decode() for s in re.findall(rb'/automat/[^\0]+\.mat',data)]
 raw=path.read_bytes();u=lambda o:struct.unpack_from('<I',raw,o)[0]
 for i,obj in enumerate(objects):
  shader=(root/mats[u(u(84)+100*i+16)].lstrip('/'))
  text=shader.read_text() if shader.exists() else ''
  if 'shadowonly' in text or 'fakeshadow' in text:bpy.data.objects.remove(obj,do_unlink=True)
  elif 'eut2.glass.' in text:obj.data.materials[0]=material('window',(.035,.065,.085,1))
 return objects

def stroke(name,points,color,width=.015):
 curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.bevel_depth=width;curve.bevel_resolution=2
 spl=curve.splines.new('POLY');spl.points.add(len(points)-1)
 for dst,p in zip(spl.points,points):dst.co=(*convert(p),1)
 obj=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(obj);curve.materials.append(material(name,color));return obj

def frustum(name,origin,view,length=3):
 color=COLORS[name];fov=view['fovHorizontal'];yaw=view['yaw'];down=view['down']
 center=Vector(direction(yaw,down));horizontal=Vector((math.cos(math.radians(yaw)),0,-math.sin(math.radians(yaw))))
 edge1=origin+center*length+horizontal*(length*math.tan(math.radians(fov/2)))
 edge2=origin+center*length-horizontal*(length*math.tan(math.radians(fov/2)))
 stroke(name+'-axis',[origin,origin+center*length],color,.008)
 stroke(name+'-fov',[edge1,origin,edge2],color,.009)
 # A small outlined pupil remains visible at close range.
 bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,radius=.022,location=convert(origin));bpy.context.object.name=name+'-pupil';bpy.context.object.data.materials.append(material(name+'-pupil',color))
 return [origin,edge1,edge2]

def aim(cam,target,offset,ortho=None):
 t=Vector(convert(target));cam.location=t+Vector(convert(offset));cam.rotation_euler=(t-cam.location).to_track_quat('-Z','Y').to_euler()
 cam.data.type='ORTHO' if ortho else 'PERSP';cam.data.ortho_scale=ortho or 8;cam.data.lens=48;cam.data.clip_start=.005

def run(stock,hardware,out):
 out.mkdir(parents=True,exist_ok=True);review=json.loads((hardware/'calibration-review.json').read_text());root=stock.parents[2]
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 body=material('SCS body - clay',(.27,.36,.43,1));dark=material('SCS chassis - clay',(.055,.075,.09,1));metal=material('SCS bumper - clay',(.42,.47,.51,1))
 cameraMat=material('camera supplied palette',(.025,.025,.025,1));tex=cameraMat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(hardware/'vehicle/truck/upgrade/bettergps/camera_finish/palette.dds'),check_existing=True) if (hardware/'vehicle/truck/upgrade/bettergps/camera_finish/palette.dds').exists() else None
 if tex.image:cameraMat.node_tree.links.new(tex.outputs['Color'],cameraMat.node_tree.nodes['Principled BSDF'].inputs['Base Color'])
 scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE_NEXT';scene.eevee.taa_render_samples=16
 scene.render.resolution_x=1250;scene.render.resolution_y=800;scene.render.resolution_percentage=100
 scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.075,.09,.115,1);scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.7
 scene.view_settings.view_transform='Standard';scene.view_settings.look='Medium High Contrast'
 for xyz,power in [((0,9,-4),1600),((6,6,3),1500),((-6,5,-3),1200)]:
  bpy.ops.object.light_add(type='AREA',location=convert(xyz));o=bpy.context.object;o.data.energy=power*.35;o.data.size=6;o.rotation_euler=(Vector(convert((0,2,0)))-o.location).to_track_quat('-Z','Y').to_euler()
 bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam
 reports=[]
 for cab,chs in [('s76h','s76_224_6x4'),('dc','dc_179_6x4'),('s52m','s52_200_6x4')]:
  prior=set(bpy.data.objects)
  cp=hardware/f'vehicle/truck/kenworth_t680_2022/cabin/{cab}.pmg';sp=stock/f'chassis/{chs}.pmg';bp=hardware/'vehicle/truck/kenworth_t680_2022/accessory/bumper/low_chrome_fog.pmg'
  cframe=loc(sp,'cabin');bframe=loc(sp,'bumper');hframe=loc(sp,'hood');cpos,cq=compose(cframe,(0,0,0))
  stock_mesh(sp,dark,root);stock_mesh(cp,body,root,cpos,cq)
  stock_mesh(stock/'accessory/hood/long.pmg',body,root,hframe['position'],hframe['rotation_wxyz'])
  stock_mesh(bp,metal,root,bframe['position'],bframe['rotation_wxyz'])
  for name,relative in [('mirror','mirror/standard_paint.pmg'),('rear_cab','rear_cab/day_standard2.pmg' if cab=='dc' else 'rear_cab/standard_aircon.pmg')]:
   m=loc(cp,name);p,q=compose(cframe,m['position'],m['rotation_wxyz']);stock_mesh(stock/'accessory'/relative,dark,root,p,q)
  # Grille is attached to the hood, not to the cabin.
  hood=stock/'accessory/hood/long.pmg'
  try:
   m=loc(hood,'grill');p,q=compose(hframe,m['position'],m['rotation_wxyz']);stock_mesh(stock/'accessory/grill/standard_chrome.pmg',metal,root,p,q)
  except StopIteration:pass
  poses={}
  for view,v in review['views'].items():
   m=loc(bp,'slot_0') if view=='forward' else loc(cp,{'left':'bgps_left','right':'bgps_right','reverse':'bgps_cam'}[view])
   p,q=compose(bframe if view=='forward' else cframe,m['position'],m['rotation_wxyz'])
   load(hardware/f"vehicle/truck/upgrade/bettergps/{v['part']}/{v['model']}.pmg",cameraMat,p,palette=True,rotation=q)
   origin=p+q@Vector(v['pupilLocal']);poses[view]=origin;frustum(view,origin,v,3 if cab=='s76h' else 1.6)
   reports.append({'cabin':cab,'chassis':chs,'view':view,'opticalCenterChassis':list(origin),'yaw':v['yaw'],'down':v['down'],'horizontalFov':v['fovHorizontal']})
  def render(name,target,offset,ortho=None):
   aim(cam,target,offset,ortho);scene.render.filepath=str(out/(name+'.png'));bpy.ops.render.render(write_still=True)
  if cab=='s76h':
   render('overview-top',(0,1,-.1),(0,15,.001),21)
   # Render each mounting close-up, retaining the real model around the camera.
   for o in bpy.data.objects:
    if o.type=='CURVE' or o.name.startswith(tuple(v+'-pupil' for v in COLORS)):o.hide_render=True
   for view in ['forward','left','right','reverse']:
    p=poses[view];offset={'forward':(-.36,.20,-.55),'left':(-.72,.20,.44),'right':(.48,-.50,.70),'reverse':(.55,.28,.65)}[view]
    render(view+'-mount',p,offset)
   for o in bpy.data.objects:
    if o.type=='CURVE' or o.name.startswith(tuple(v+'-pupil' for v in COLORS)):o.hide_render=not o.name.startswith('reverse-')
   render('reverse-side',poses['reverse']+Vector((0,-.9,1)),(6,.1,0),5.7)
   for o in bpy.data.objects:
    if o.type=='CURVE' or o.name.startswith(tuple(v+'-pupil' for v in COLORS)):o.hide_render=not o.name.startswith('forward-')
   render('forward-side',poses['forward']+Vector((0,.15,-.9)),(-6,.1,0),5.7)
   for o in bpy.data.objects:
    if o.type=='CURVE' or o.name.startswith(tuple(v+'-pupil' for v in COLORS)):o.hide_render=False
   bpy.ops.wm.save_as_mainfile(filepath=str(out/'T680-four-cameras-review.blend'))
  for o in bpy.data.objects:
   if o.type=='CURVE' or o.name.startswith(tuple(v+'-pupil' for v in COLORS)):o.hide_render=True
  render(cab+'-rear',poses['reverse'],(.5,.3,1.5))
  for o in list(set(bpy.data.objects)-prior):bpy.data.objects.remove(o,do_unlink=True)
 (out/'review-poses.json').write_text(json.dumps({'approval':'pending','materials':'illustrative clay; actual PMG geometry','poses':reports},indent=2)+'\n')
 print('REVIEW_RENDER_DONE',out)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['stock','hardware','output']:p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);run(a.stock.resolve(),a.hardware.resolve(),a.output.resolve())
