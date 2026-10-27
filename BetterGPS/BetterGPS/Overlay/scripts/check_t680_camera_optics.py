"""Verify front optics against actual SCS bumper geometry (run with Blender)."""
import argparse,json,math,sys
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
from prepare_camera_pose_assets import direction,locator,rotate
from render_pp807032_mounts import visible_geometry

def run(hardware,extracted,output):
 c=json.loads((hardware/'calibration-review.json').read_text());assert all(v['fovHorizontal']==100 for v in c['views'].values())
 stock=extracted/'vehicle/truck/kenworth_t680_2022';rows=[]
 for name in sorted({r['bumper'] for r in c['frontPoses']}):
  item=next(r for r in c['frontPoses'] if r['bumper']==name)
  vertices,faces,_,grids=visible_geometry(stock/f'accessory/bumper/{name}.pmg',extracted)
  # The grille alpha plane is checked conservatively as solid as well.
  for grid in grids:
   start=len(vertices);vertices.extend(grid['vertices']);faces.extend(tuple(start+i for i in face) for face in grid['faces'])
  tree=BVHTree.FromPolygons([Vector(v) for v in vertices],faces,all_triangles=True);origin=Vector(item['positionBumper']);blocked=0
  for x in range(31):
   for y in range(19):
    # 100 horizontal FOV, a conservative +/- 32 vertical view envelope.
    yaw=-50+100*x/30;down=5-32+64*y/18;ray=Vector(direction(yaw,down))
    hit,_,_,distance=tree.ray_cast(origin+ray*.002,ray,2)
    if hit is not None:blocked+=1
  center=Vector(direction(0,5));hit,_,_,distance=tree.ray_cast(origin+center*.002,center,2)
  assert hit is None,(name,'solid bumper wall in front of optical axis',distance)
  rows.append(dict(bumper=name,originBumper=list(origin),rays=31*19,blockedRays=blocked,axisClear=True))
 for r in c['frontPoses']:
  frame=locator(stock/f"chassis/{r['chassis']}.pmg",'bumper');p=[a+b for a,b in zip(frame['position'],rotate(frame['rotation_wxyz'],r['positionBumper']))]
  assert max(abs(a-b) for a,b in zip(p,r['positionChassis']))<1e-6,r
 result=dict(horizontalFov=100,bumperCount=len(rows),chassisCombinations=len(c['frontPoses']),physicalBumperRayTests=rows,liveNativeVerification='pending')
 output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,indent=2)+'\n')
 print('PASS',len(rows),'clear forward axes,',len(c['frontPoses']),'attachment chains; total blocked FOV rays',sum(r['blockedRays'] for r in rows))

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['hardware','extracted','output']:p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);run(a.hardware,a.extracted,a.output)
