"""Prepare non-visual optical tags and camera calibration from the approved meshes.

No vertex, index, material or original locator is changed. Camera angles are
user requirements, independent of the mesh bracket angle. These source assets
are prepared for review; installing them is a separate workflow step.
"""
from pathlib import Path
import argparse,json,math,struct,hashlib
from scs_camera_mounts import read,token,geometry,validate_layout

CAMERAS={
 'forward':('pp807032','pp807032_lod0','bgps_v_f',100,0,5),
 'left':('pp804397','pp804397_truck','bgps_v_l',100,145,0),
 'right':('pp804396','pp804396_truck','bgps_v_r',100,215,0),
 'reverse':('pp804395','pp804395_lod0','bgps_v_b',100,180,45),
}


def qmul(a,b):
 w,x,y,z=a;W,X,Y,Z=b
 return [w*W-x*X-y*Y-z*Z,w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W]


def rotate(q,p):
 return qmul(qmul(q,[0,*p]),[q[0],-q[1],-q[2],-q[3]])[1:]


def direction(yaw,down):
 a,b=map(math.radians,(yaw,down));return [-math.sin(a)*math.cos(b),-math.sin(b),-math.cos(a)*math.cos(b)]


def append_tag(path,name,position,rotation=(1,0,0,0)):
 before=path.read_bytes();u=lambda p:struct.unpack_from('<I',before,p)[0]
 assert before[:4]==b'\x15gmP' and u(8)==1
 first=u(76);part,pieces,firstpiece,count,firstloc=struct.unpack_from('<Q4I',before,first)
 assert count==u(20) and firstloc==0
 if any(l['name']==name for p in read(path)['parts'] for l in p['locators']):raise ValueError('Already tagged: '+str(path))
 oldstrings=before[u(88):u(88)+u(92)];assert not oldstrings or oldstrings[0]==0
 record=struct.pack('<Q8fi',token(name),*position,1,*rotation,0)
 ins=u(84);pad=0 if oldstrings else 4;delta=44+pad
 data=bytearray(before[:ins]+record+before[ins:u(88)]+bytes(pad)+before[u(88):])
 struct.pack_into('<I',data,first+16,count+1);struct.pack_into('<I',data,20,count+1)
 for p in [84,88]:struct.pack_into('<I',data,p,u(p)+44)
 for p in [96,104]:struct.pack_into('<I',data,p,u(p)+delta)
 struct.pack_into('<I',data,92,u(92)+pad)
 for i in range(u(4)):
  old=ins+100*i
  for offset in range(64,100,4):
   ptr=u(old+offset)
   if ptr!=0xffffffff:struct.pack_into('<I',data,old+44+offset,ptr+delta)
 for offset,size in [(96,100),(104,108)]:assert data[u(offset)+delta:u(offset)+delta+u(size)]==before[u(offset):u(offset)+u(size)]
 original=read(path);path.write_bytes(data);validate_layout(path);after=read(path)
 assert all(p['locators']==after['parts'][i]['locators'][:-1] for i,p in enumerate(original['parts']))
 return dict(name=name,position=position,rotation=list(rotation),source_sha256=hashlib.sha256(before).hexdigest(),sha256=hashlib.sha256(data).hexdigest(),geometryPreserved=True)


def pupil(path):
 b=path.read_bytes();u=lambda p:struct.unpack_from('<I',b,p)[0];found=[]
 for i in range(u(4)):
  at=u(84)+100*i;n=u(at+4);stride=u(at+60);pos=u(at+64);uv=u(at+72)
  if uv==0xffffffff:continue
  for v in range(n):
   s,t=struct.unpack_from('<2f',b,uv+v*stride)
   if abs(s-.5625)<.0001:found.append(struct.unpack_from('<3f',b,pos+v*stride))
 assert len(found)>3,(path,'optical pupil missing')
 # Bounding-box centre avoids triangulation/seam duplication bias.
 return [(min(p[a] for p in found)+max(p[a] for p in found))/2 for a in range(3)]


def locator(path,name):
 return next(l for p in read(path)['parts'] for l in p['locators'] if l['name']==name)


def run(root,stock):
 rows=[];optics={};tags=[]
 for view,(part,model,name,fov,yaw,down) in CAMERAS.items():
  path=root/f'vehicle/truck/upgrade/bettergps/{part}/{model}.pmg';center=pupil(path)
  tags.append(append_tag(path,name,center));optics[view]=dict(part=part,model=model,tag=name,pupilLocal=center,fovHorizontal=fov,yaw=yaw,down=down,direction=direction(yaw,down))
 for cab in ['dc','s52m','s76h']:
  path=root/f'vehicle/truck/kenworth_t680_2022/cabin/{cab}.pmg'
  tags.append(append_tag(path,'bgps_frame',[0,0,0]))
  # An engine-owned parking producer is declared once per cab. It stays
  # disabled outside Camera; only installed optical tags count as hardware.
  tags.append(append_tag(path,'cam_parkbck',[0,1.7,-1.9]))
  for view,optical in optics.items():
   if view=='forward':continue
   name={'left':'bgps_left','right':'bgps_right','reverse':'bgps_cam'}[view];mount=locator(path,name)
   p=[a+b for a,b in zip(mount['position'],rotate(mount['rotation_wxyz'],optical['pupilLocal']))]
   rows.append(dict(cabin=cab,view=view,positionCab=p,mount=mount['position'],mountRotation=mount['rotation_wxyz'],**optical))
 front=[]
 for path in (root/'vehicle/truck/kenworth_t680_2022/accessory/bumper').glob('*.pmg'):
  mount=locator(path,'slot_0');optical=optics['forward'];p=[a+b for a,b in zip(mount['position'],rotate(mount['rotation_wxyz'],optical['pupilLocal']))]
  for chassis in (stock/'chassis').glob('*.pmg'):
   parent=locator(chassis,'bumper');world=[a+b for a,b in zip(parent['position'],rotate(parent['rotation_wxyz'],p))]
   front.append(dict(bumper=path.stem,chassis=chassis.stem,positionChassis=world,positionBumper=p,**optical))
 manifest=root/'hardware-manifest.json';j=json.loads(manifest.read_text())
 for row in j['files']:
  data=(root/row['path']).read_bytes();row['source_sha256']=row['sha256'];row['sha256']=hashlib.sha256(data).hexdigest();row['bytes']=len(data)
 j['opticalTags']=tags;j['calibration']=dict(approved=False,views=optics,cabinPoses=rows,frontPoses=front)
 manifest.write_text(json.dumps(j,indent=2)+'\n')
 (root/'calibration-review.json').write_text(json.dumps(j['calibration'],indent=2)+'\n')
 print('Prepared 4 optical tags, 3 cab frames and parking producers;',len(front),'front combinations; validation pending.')


if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--stock',type=Path,required=True);a=p.parse_args();run(a.root,a.stock)
