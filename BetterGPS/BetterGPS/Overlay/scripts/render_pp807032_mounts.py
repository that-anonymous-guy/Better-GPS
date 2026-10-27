"""Fit the optional front camera to all eight stock T680 NG bumpers.

Ignore shadow-only meshes when measuring contact and strap clearance. Keep
stock vertex/index pools intact: output is only poses, previews and evidence.
"""
import argparse,json,math,re,struct,sys
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
from scs_camera_mounts import geometry,read
from render_pp804395_mounts import load,material,convert


def visible_geometry(path,extracted):
 data=path.read_bytes();u=lambda o:struct.unpack_from('<I',data,o)[0]
 mats=[p.decode() for p in re.findall(rb'/automat/[^\0]+\.mat',path.with_suffix('.pmd').read_bytes())]
 points=[];faces=[];hidden=[];grids=[]
 for i in range(u(4)):
  at=u(84)+100*i;mat=(extracted/mats[u(at+16)].lstrip('/')).read_text()
  if 'shadowonly' in mat or 'eut2.glass.' in mat:hidden.append(i);continue
  if 'bumper_grid_difopa' in mat:
   vv=[Vector(struct.unpack_from('<3f',data,u(at+64)+j*u(at+60))) for j in range(u(at+4))]
   uv=[Vector(struct.unpack_from('<2f',data,u(at+72)+j*u(at+60))) for j in range(u(at+4))]
   ii=struct.unpack_from('<'+'H'*u(at),data,u(at+96));ff=list(zip(ii[::3],ii[1::3],ii[2::3]))
   grids.append({'piece':i,'vertices':vv,'uv':uv,'faces':ff,'tree':BVHTree.FromPolygons(vv,ff,all_triangles=True)})
   continue
  start=len(points);points.extend(struct.unpack_from('<3f',data,u(at+64)+j*u(at+60)) for j in range(u(at+4)))
  indices=struct.unpack_from('<'+'H'*u(at),data,u(at+96))
  faces.extend(tuple(start+n for n in indices[k:k+3]) for k in range(0,len(indices),3))
 return points,faces,hidden,grids


def grid_hole(grid,x,y):
 from mathutils.geometry import barycentric_transform
 p,_,face,_=grid['tree'].ray_cast(Vector((x,y,-1)),Vector((0,0,1)),2)
 if p is None:return True
 ids=grid['faces'][face];v=[grid['vertices'][i] for i in ids];uv=[Vector((*grid['uv'][i],0)) for i in ids]
 sample=barycentric_transform(p,*v,*uv)
 # Stock 8x8 tile: opaque one-pixel border, transparent 6x6 centre.
 # Leave a quarter pixel margin against bilinear filtering at each border.
 return .15625<(sample.x%1)<.84375 and .15625<(sample.y%1)<.84375


def run(stock,cache,output):
 output=output.resolve();output.mkdir(parents=True,exist_ok=True)
 extracted=stock.parents[2]
 camera=cache/'vehicle/truck/upgrade/bettergps/pp807032/pp807032_lod0.pmg'
 cv,cf=geometry(camera);cv=[Vector(p) for p in cv]
 # Only the two long straps extend beyond Z=0.06; their boxes are separate.
 strap_faces=[];body_faces=[]
 for f in cf:
  if all(.0302<abs(cv[i].x)<.0318 and abs(cv[i].y)<.00151 and (abs(cv[i].z)<.00001 or abs(cv[i].z-.0890166722)<.00001) for i in f):strap_faces.append(f)
  else:body_faces.append(f)
 assert len(strap_faces)==24,'Two box straps expected'
 body_indices=set(i for f in body_faces for i in f)
 rows=[]
 for path in sorted((stock/'accessory/bumper').glob('*.pmg')):
  points,faces,hidden,grids=visible_geometry(path,extracted)
  if not grids:
   intake=stock/'accessory/f_intake_chs/standard_plastic.pmg'
   iv,iff,_,_=visible_geometry(intake,extracted);start=len(points)
   points.extend((p[0],p[1]-.2395932674407959,p[2]-.0023708343505859375) for p in iv)
   faces.extend(tuple(i+start for i in f) for f in iff)
  tree=BVHTree.FromPolygons(points,faces,all_triangles=True)
  contact_trees=[tree]+[g['tree'] for g in grids]
  def body_overlap(world):
   body=BVHTree.FromPolygons(world,body_faces,all_triangles=True)
   return any(body.overlap(t) for t in contact_trees)
  def body_gap(world):return min(t.find_nearest(world[i])[3] for t in contact_trees for i in body_indices)
  def hit(x,y):
   hits=[t.ray_cast(Vector((x,y,-1)),Vector((0,0,1)),2)[0] for t in [tree]+[g['tree'] for g in grids]]
   hits=[p for p in hits if p is not None]
   return min(hits,key=lambda p:p.z) if hits else None
  # Search only a small neighbourhood of the indicated left-hand grille area.
  # A shared pose is preferred; variants are individually remeasured.
  candidates=[];blocked={'samples':0,'straps':0,'body':0}
  for iy in range(-15,16):
   y=-.15+iy*.001
   for ix in range(-15,16):
    x=-.35+ix*.001
    if any(not grid_hole(g,x+side*.031+dx,y+dy) for g in grids for side in (-1,1)
           for dx in (-.00075,0,.00075) for dy in (-.0015,-.00075,0,.00075,.0015)):
     blocked['straps']+=1;continue
    samples=[hit(x+dx,y+dy) for dx in (-.03,-.015,0,.015,.03) for dy in (-.015,0,.015)]
    samples=[p for p in samples if p is not None and p.z<.16]
    if len(samples)<5:blocked['samples']+=1;continue
    # The rigid back sits immediately in front of the nearest mesh strands.
    grill_front=min(p.z for p in samples)
    z=grill_front-max(cv[i].z for i in body_indices)-.0003
    world=[Vector((x,y,z))+p for p in cv]
    strap_tree=BVHTree.FromPolygons(world,strap_faces,all_triangles=True)
    if strap_tree.overlap(tree):blocked['straps']+=1;continue
    if body_overlap(world):blocked['body']+=1;continue
    # Dense face samples supplement vertex-only intersection checks.
    near=body_gap(world)
    candidates.append((abs(ix)+abs(iy)+near*1000,x,y,z,near,grill_front))
  assert candidates,(path.stem,'No pose with both straps clear of mesh strands',blocked)
  _,x,y,z,near,grill_front=min(candidates)
  # Close the last gap without introducing an intersection, using actual
  # triangle contact rather than the camera's bounding box.
  z_best=z
  for step in range(1,101):
   trial=z+step*.0001
   world=[Vector((x,y,trial))+p for p in cv]
   if body_overlap(world) or BVHTree.FromPolygons(world,strap_faces,all_triangles=True).overlap(tree):break
   z_best=trial
  z=z_best
  world=[Vector((x,y,z))+p for p in cv]
  assert not body_overlap(world)
  assert not BVHTree.FromPolygons(world,strap_faces,all_triangles=True).overlap(tree)
  nearest=body_gap(world)
  assert nearest<.003,(path.stem,'body too far from grille',nearest)
  assert z+.089>grill_front+.035,'Straps must extend through the grille'
  bounds=[[min(p[i] for p in world) for i in range(3)],[max(p[i] for p in world) for i in range(3)]]
  row={'bumper':path.stem,'locator':'slot_0','position_scs':[x,y,z],'rotation_wxyz':[1,0,0,0],
   'camera_bounds_scs':bounds,'body_to_grille_vertex_gap_m':nearest,'body_grille_intersections':0,'strap_grille_intersections':0,
   'strap_depth_past_grille_m':z+.089-grill_front,'grille_front_z':grill_front,'ignored_nonphysical_pieces':hidden,
   'alpha_grille_holes_checked':bool(grids),'side':'vehicle left (-X), beside license plate','socket_faces':'front (-Z)'}
  rows.append(row)
  bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
  objects=load(path,material('bumper_finish',(.40,.44,.47,1)))
  for i,obj in enumerate(objects):
   if i in hidden:bpy.data.objects.remove(obj,do_unlink=True)
  objects=[o for i,o in enumerate(objects) if i not in hidden]
  if not grids:load(intake,material('intake_plastic',(.03,.035,.04,1)),(0,-.2395932674407959,-.0023708343505859375))
  for grid in grids:
   obj=objects[[i for i in range(len(objects)+len(hidden)) if i not in hidden].index(grid['piece'])]
   gm=material('stock_alpha_grille',(.015,.015,.015,1));gm.surface_render_method='DITHERED'
   tex=gm.node_tree.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(extracted/'vehicle/truck/kenworth_t680_2022/bumper_grid_difopa.tobj.dds.png'),check_existing=True)
   gm.node_tree.links.new(tex.outputs['Alpha'],gm.node_tree.nodes['Principled BSDF'].inputs['Alpha'])
   layer=obj.data.uv_layers.new(name='grid')
   for loop in obj.data.loops:
    uv=grid['uv'][loop.vertex_index];layer.data[loop.index].uv=(uv.x,1-uv.y)
   obj.data.materials[0]=gm
  black=material('grille_black',(.025,.03,.035,1))
  # Preview materials are illustrative; native truck materials stay untouched.
  for obj in objects:
   if len(obj.data.vertices)>2500:obj.data.materials[0]=black
  mat=material('camera_palette',(.03,.03,.03,1));tex=mat.node_tree.nodes.new('ShaderNodeTexImage')
  tex.image=bpy.data.images.load(str(cache/'vehicle/truck/upgrade/bettergps/pp807032/palette.dds'),check_existing=True)
  mat.node_tree.links.new(tex.outputs['Color'],mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'])
  load(camera,mat,(x,y,z),palette=True)
  scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE_NEXT';scene.render.resolution_x=1200;scene.render.resolution_y=700;scene.render.resolution_percentage=100
  scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.17,.20,.23,1)
  for loc,power in (((-2,3,-3),600),((2,1,-2),500)):
   bpy.ops.object.light_add(type='AREA',location=convert(loc));light=bpy.context.object;light.data.energy=power;light.data.size=3
   light.rotation_euler=(Vector(convert((0,-.15,0)))-light.location).to_track_quat('-Z','Y').to_euler()
  bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam;cam.data.clip_start=.001;cam.data.lens=55
  for view in ('front','detail','straps'):
   target=Vector(convert((0,-.15,.05) if view=='front' else (x,y,z+.02)))
   cam.location=target+Vector(convert((0,.2,-2.6) if view=='front' else ((-.15,.08,-.28) if view=='detail' else (-.15,.025,.04))))
   cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(output/f'{path.stem}-{view}.png');bpy.ops.render.render(write_still=True)
  print('FRONT_MOUNT_PASS',path.stem,row)
 (output/'mounts.json').write_text(json.dumps(rows,indent=2)+'\n')

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('stock','cache','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);run(a.stock.resolve(),a.cache.resolve(),a.output.resolve())
