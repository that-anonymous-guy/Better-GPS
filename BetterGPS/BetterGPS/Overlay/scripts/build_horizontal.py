"""Import supplied horizontal modules; retain aspect and use an 800x500 canvas."""
from pathlib import Path
from PIL import Image
import shutil,struct
root=Path(__file__).resolve().parents[1]
source=Path('C:/Users/Crym2/Downloads/newgpsoverlay')
out=root/'horizontal_modules';out.mkdir(exist_ok=True)
gen=root/'generated/horizontal';gen.mkdir(exist_ok=True)
paths=[]
def add(path):
 dest=out/path;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/path,dest)
 im=Image.open(dest).convert('RGBA')
 # In landscape the microphone lives in the supplied right-hand controls.
 # Remove its duplicate from the reused portrait banner, keeping its frame.
 if path=='google/vertical/navmode/nav_instructions.dds':
  for y in range(25,93):
   for x in range(288,350):im.putpixel((x,y),im.getpixel((270,y)))
 idx=400+len(paths);name=f'module_{idx}.bgra'
 (gen/name).write_bytes(struct.pack('<II',*im.size)+im.tobytes('raw','BGRA'));paths.append((idx,im.width,im.height,name));return (idx,im.width,im.height)
def entry(path,x,y,states):
 r,w,h=add(path);return (r,w,h,x,y,states)
google=[
 entry('google/horizontal/above_light.dds',530,0,3),entry('google/horizontal/above_dark.dds',530,0,0),entry('google/horizontal/above_dark.dds',530,0,12),
 entry('google/vertical/navmode/nav_instructions.dds',20,0,5),
 entry('google/horizontal/navmode/horizontal_nav_buttons_light.dds',590,35,1),entry('google/horizontal/navmode/horizontal_nav_buttons_dark.dds',590,35,4),
 entry('google/horizontal/navmode/horizontal_nav_below_light.dds',20,352.5,1),entry('google/horizontal/navmode/horizontal_nav_below_dark.dds',20,352.5,4),
 entry('google/horizontal/notripmode/horizontal_notrip_search_light.dds',20,0,2),entry('google/horizontal/notripmode/horizontal_notrip_search_dark.dds',20,0,8),
 entry('google/horizontal/notripmode/horizontal_notrip_below_buttons_light.dds',665,352.5,2),entry('google/horizontal/notripmode/horizontal_notrip_below_buttons_dark.dds',665,352.5,8),
 entry('google/horizontal/notripmode/horizontal_notrip_below_light.dds',20,387.5,2),entry('google/horizontal/notripmode/horizontal_notrip_below_dark.dds',20,387.5,8)]
waze=[
 entry('google/horizontal/above_light.dds',530,0,2),entry('waze/horizontal/nav/wave_nav_above.dds',0,0,5),entry('google/horizontal/above_dark.dds',530,0,8),
 entry('waze/horizontal/nav/navwaze_horizontal.dds',20,30,5),
 entry('waze/horizontal/waze_buttons_topright_horizontal_light.dds',600,30,3),entry('waze/horizontal/waze_buttons_topright_horizontal_dark.dds',600,30,12),
 entry('waze/horizontal/nav/waze_navbelow_horizontal_light.dds',20,372.5,1),entry('waze/horizontal/nav/waze_navbelow_horizontal_dark.dds',20,372.5,4),
 entry('waze/waze_compass_light.dds',390,30,1),entry('waze/waze_compass_dark.dds',390,30,4),
 entry('waze/waze_menu_light.dds',20,30,2),entry('waze/waze_menu_dark.dds',20,30,8),
 entry('waze/horizontal/notrip/waze_notripbelow_horizontal_light.dds',20,332.5,2),entry('waze/horizontal/notrip/waze_notripbelow_horizontal_dark.dds',20,332.5,8)]
extra=[add(p) for p in ['google/horizontal/navmode/horizontal_nav_buttons_light_mute.dds','google/horizontal/navmode/horizontal_nav_buttons_dark_mute.dds','waze/horizontal/waze_buttons_topright_mute_horizontal_light.dds','waze/horizontal/waze_buttons_topright_mute_horizontal_dark.dds','google/horizontal/notripmode/horizontal_notrip_above_button_light.dds','google/horizontal/notripmode/horizontal_notrip_above_button_dark.dds']]
carplay=[]
for style in ['Google','Waze']:
 path=Path('C:/Users/Crym2/Downloads/F150_NAV_textures_1280x800')/f'F150_Carplay_{style}.dds'
 dest=root/'BetterGPS_assets/material/ford/f150/carplay'/path.name
 dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
 im=Image.open(dest).convert('RGBA')
 # Retain the live native clock/temperature widgets above this frame; erase
 # only their example text from the derived resource, not the supplied DDS.
 for left,right in [(0,150),(1160,1270)]:
  for y in range(12,65):
   a=im.getpixel((left,y));b=im.getpixel((right,y))
   for x in range(left,right):
    t=(x-left)/(right-left)
    im.putpixel((x,y),tuple(round(v+(w-v)*t) for v,w in zip(a,b)))
 im=im.resize((800,500),Image.Resampling.LANCZOS)
 idx=400+len(paths);name=f'module_{idx}.bgra'
 (gen/name).write_bytes(struct.pack('<II',*im.size)+im.tobytes('raw','BGRA'));paths.append((idx,im.width,im.height,name));carplay.append(idx)
# Positions and module dimensions are scaled uniformly from the reference.
scale=800/740
def cpp(entries):return ','.join('{'+','.join(map(str,[r,w,h,round(x*scale),round(y*scale),states]))+'}' for r,w,h,x,y,states in entries)
(root/'generated/HorizontalModules.h').write_text('#pragma once\nnamespace bettergps_overlay {inline constexpr PhoneModule horizontalGoogle[]={'+cpp(google)+'};\ninline constexpr PhoneModule horizontalWaze[]={'+cpp(waze)+'};\ninline constexpr unsigned horizontalExtras[]={'+','.join(str(x[0]) for x in extra)+'};\ninline constexpr unsigned horizontalCarplay[]={'+','.join(map(str,carplay))+'};}\n')
(root/'generated/HorizontalModules.rc').write_text('\n'.join(f'{r} RCDATA "generated/horizontal/{name}"' for r,w,h,name in paths)+'\n')
