"""Temporary asset-only SCS rear-camera smoke test, with guarded rollback.

Uses the unmodified installed DLL. Only the T680 GPS overlay is temporarily
set disabled, exposing the native SCS UI (Ford ID 910 / park_1x1_ui.mat).
The experiment stages files outside production sources and deploys four assets.
"""
import argparse,datetime,hashlib,importlib.util,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(Path(__file__).resolve().parent))
from scs_camera_mounts import read,token,geometry
spec=importlib.util.spec_from_file_location('bettergps_workflow',ROOT/'BetterGPS/Dev/workflow.py');wf=importlib.util.module_from_spec(spec);spec.loader.exec_module(wf)
GAME=Path('H:/SteamLibrary/steamapps/common/American Truck Simulator');DEST=GAME/'bin/win_x64/plugins/BetterGPS'
WORK=ROOT/'build/camera-integration-20261026/native-smoke';STATE=WORK/'state.json'
UI='BetterGPS_assets/ui/dashboard/kenworth_t680_2022_gps.sii'

def config_patch(data,value):
 chunks=re.split(rb'(?m)(?=^\[)',data);found=[]
 for i,b in enumerate(chunks):
  if re.search(rb'(?m)^vehicle=kenworth\.t680_2022\r?$',b) and re.search(rb'(?m)^ui=/ui/dashboard/kenworth_t680_2022_gps\.sii\r?$',b):found.append(i)
 assert len(found)==1,('Expected one exact T680 GPS profile',len(found))
 i=found[0];m=re.search(rb'(?m)^enabled=([01])(?=\r?$)',chunks[i]);assert m
 previous=int(m[1]);chunks[i]=chunks[i][:m.start(1)]+str(value).encode()+chunks[i][m.end(1):]
 return b''.join(chunks),previous

def camera_ui(source):
 a=source.index('ui::window');b=source.index('\n}',a);root=source[a:b];name=re.search(r'ui::window\s*:\s*(\S+)',root)[1]
 n=int(re.search(r'\bmy_children:\s*(\d+)',root)[1]);root=re.sub(r'(\bmy_children:\s*)\d+',lambda m:m[1]+str(n+1),root,count=1)
 root+=f'\n my_children[{n}]: _nameless.bgps_cam\n';source=source[:a]+root+source[b:]
 def rect():return ' coords_l: 0\n coords_r: 1280\n coords_t: 720\n coords_b: 0\n area_l: 0\n area_r: 1280\n area_t: 720\n area_b: 0\n'
 group='ui::group : _nameless.bgps_cam {\n fitting: false\n my_children: 2\n my_children[0]: _nameless.bgps_black\n my_children[1]: _nameless.bgps_view\n'+rect()+' id: 910\n layer: 20\n tab: -1\n pointer: -1\n my_parent: '+name+'\n}\n'
 def image(unit,text,layer):return 'ui::text : _nameless.'+unit+' {\n text: "'+text+'"\n'+rect()+' id: 0\n layer: '+str(layer)+'\n tab: -1\n pointer: -1\n my_parent: _nameless.bgps_cam\n}\n'
 group+=image('bgps_black','<color value=FF0A0A0A><img src=/material/ui/white.mat xscale=stretch yscale=stretch>',20)
 group+=image('bgps_view','<img src=/material/environment/mirror/park_1x1_ui.mat xscale=stretch yscale=stretch top=1 bottom=0>',21)
 end=source.rfind('\n}');return source[:end]+'\n'+group+source[end:]

def prepare():
 if STATE.exists() and json.loads(STATE.read_text()).get('status')=='installed':raise RuntimeError('Restore the active smoke test first')
 stage=WORK/'staged';paths=[];cal=json.loads((ROOT/'BetterGPS/Overlay/BetterGPS_assets/camera-hardware/calibration-review.json').read_text())
 for cab in ['dc','s52m','s76h']:
  relative=f'BetterGPS_assets/camera-hardware/vehicle/truck/kenworth_t680_2022/cabin/{cab}.pmg';original=DEST/relative;raw=original.read_bytes();u=lambda o:struct.unpack_from('<I',raw,o)[0];data=bytearray(raw)
  matches=[i for i in range(u(20)) if struct.unpack_from('<Q',raw,u(80)+i*44)[0]==token('cam_parkbck')];assert len(matches)==1
  offset=u(80)+matches[0]*44;row=next(p for p in cal['cabinPoses'] if p['cabin']==cab and p['view']=='reverse')
  struct.pack_into('<3f',data,offset+8,*row['positionCab']);struct.pack_into('<4f',data,offset+24,0,0,math.cos(math.pi/8),math.sin(math.pi/8))
  allowed=set(range(offset+8,offset+20))|set(range(offset+24,offset+40));assert all(a==b or i in allowed for i,(a,b) in enumerate(zip(raw,data)))
  target=stage/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);assert geometry(original)==geometry(target);paths.append(relative)
 source=(WORK/'stock/ui/dashboard/kenworth_t680_2022_gps.sii').read_text(encoding='utf-8-sig');result=camera_ui(source)
 assert result.count('id: 910')==1 and 'park_1x1_ui.mat' in result and result.count('{')==result.count('}')
 target=stage/UI;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(result,encoding='utf-8');paths.append(UI)
 config=DEST/'BetterGPSDevices.ini';before=config.read_bytes();after,enabled=config_patch(before,0);assert len(before)==len(after) and sum(a!=b for a,b in zip(before,after))<=1
 (WORK/'devices-before.ini').write_bytes(before);(WORK/'devices-test.ini').write_bytes(after)
 state=dict(status='prepared',paths=paths,stage=str(stage),configBefore=wf.digest(config),originalEnabled=enabled,dllHash=wf.digest(DEST/'BetterGPSOverlay.dll'))
 STATE.write_text(json.dumps(state,indent=2));plan=wf.make_plan(paths,DEST,replace_custom=True,resolver=lambda p,c:stage/p)
 print(json.dumps(dict(changes=len(plan),bytes=sum(p['bytes'] for p in plan),files=paths,temporarySetting='T680 GPS device enabled=0 only',dll='UNCHANGED',trigger='Reverse gear, stationary, ignition on'),indent=2))

def apply():
 state=json.loads(STATE.read_text());assert state['status']=='prepared';wf.require_closed(GAME)
 config=DEST/'BetterGPSDevices.ini';assert wf.digest(config)==state['configBefore'],'Settings changed since preparation'
 stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ');backup=DEST/'backups'/('dev-camera-smoke-'+stamp)
 plan=wf.make_plan(state['paths'],DEST,replace_custom=True,resolver=lambda p,c:Path(state['stage'])/p)
 wf.install_plan(plan,DEST,backup,lambda:wf.require_closed(GAME))
 try:
  assert wf.digest(config)==state['configBefore'];wf.atomic_copy(WORK/'devices-test.ini',config);assert wf.digest(config)==wf.digest(WORK/'devices-test.ini');assert wf.digest(DEST/'BetterGPSOverlay.dll')==state['dllHash']
 except Exception:
  wf.atomic_copy(WORK/'devices-before.ini',config);wf.rollback(backup,DEST,True,lambda:wf.require_closed(GAME));raise
 state.update(status='installed',backup=str(backup));STATE.write_text(json.dumps(state,indent=2));print('Installed native camera smoke test; DLL unchanged. Backup:',backup)

def restore():
 state=json.loads(STATE.read_text());assert state['status']=='installed';wf.require_closed(GAME)
 config=DEST/'BetterGPSDevices.ini';after,_=config_patch(config.read_bytes(),state['originalEnabled']);staged=WORK/'devices-restore.ini';staged.write_bytes(after)
 wf.rollback(Path(state['backup']),DEST,True,lambda:wf.require_closed(GAME));wf.atomic_copy(staged,config)
 state['status']='restored';STATE.write_text(json.dumps(state,indent=2));print('Native smoke test restored; other user settings preserved.')

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('mode',choices=['prepare','apply','restore']);a=parser.parse_args();globals()[a.mode]()
