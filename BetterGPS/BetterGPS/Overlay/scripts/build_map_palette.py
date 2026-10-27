from pathlib import Path
import re, json
root=Path(__file__).resolve().parents[1]
keys=['road_color','road_discovered_color','prefab_color','prefab_discovered_color',
      'prefab_obstacle_color','prefab_obstacle_discovered_color','prefab_grass_color',
      'prefab_grass_discovered_color','outline_color','navigation_color',
      'navigation_highlight_color','navigation_fade_color']
def palette(text):
    data={k:int(v,16) for k,v in re.findall(r'^\s*(\w+):\s*0x([0-9a-fA-F]{8})',text,re.M)}
    for name,a,b in re.findall(r'map_area_color_name\[\]:\s*"([^"]+)"\s*map_area_color\[\]:\s*0x([0-9a-fA-F]+)[^\n]*\s*map_area_discovered_color\[\]:\s*0x([0-9a-fA-F]+)',text):
        data['area_'+name]=int(a,16);data['area_discovered_'+name]=int(b,16)
    return data
day=palette((root/'theme_sources/day/colour_data.sui').read_text())
night=palette((root/'theme_sources/night/colour_data.sui').read_text())
source=palette((root/'theme_sources/source/map_data.sii').read_text())
for n in ['Road','Light','Dark','Green','Nav-Red']:
    for pre in ['area_','area_discovered_']:
        key=pre+n
        if key not in day: day[key]=day['navigation_color'] if n=='Nav-Red' else day['road_color']
        if key not in night: night[key]=night['navigation_color'] if n=='Nav-Red' else night['road_color']
        keys.append(key)
for data in [day,night]:
    for k in ['navigation_color','navigation_highlight_color','area_discovered_Nav-Red','area_Nav-Red']: data[k]=0xFFF48542
    data['navigation_fade_color']=0xFFF48542
    data['outline_color']=0xFFA64E17

# Keep the startup ATS route colors as aliases if mounted metadata describes
# a later palette than the one the engine actually used to draw the map.
for k in ['navigation_color','navigation_highlight_color','navigation_fade_color','area_discovered_Nav-Red']:
    alias='fallback_'+k
    source[alias]=source[k]
    day[alias]=night[alias]=0xFFF48542
    keys.append(alias)

# The live map combines phone road definitions mounted after SDK startup with
# navigation colors ATS cached earlier. The background exporter cannot see
# the plugin's in-process VFS mounts; keep those actual phone colors as aliases.
phone=palette((root/'BetterGPS_assets/def/map_data/road_colors.sui').read_text()+'\n'+
              (root/'BetterGPS_assets/def/map_data/gps_colors.sui').read_text())
for k in ['road_color','road_discovered_color','prefab_color','prefab_discovered_color',
          'prefab_obstacle_discovered_color','prefab_grass_discovered_color']:
    alias='phone_'+k
    source[alias]=phone[k]
    day[alias]=day[k]
    night[alias]=night[k]
    keys.append(alias)
def rgba(n):return ','.join(f'{((n>>s)&255)/255:.8f}f' for s in [0,8,16,24])
lines=['#pragma once','namespace bettergps_overlay {',f'inline constexpr unsigned mapPaletteCount={len(keys)};',
       'struct MapPalette {float source[32][4]{},day[32][4]{},night[32][4]{};};',
       'inline constexpr MapPalette builtinMapPalette{']
for data in [source,day,night]: lines+=['{'+','.join('{'+rgba(data.get(k,0))+'}' for k in keys)+'},']
lines+=['};','}']
(root/'MapPalette.h').write_text('\n'.join(lines)+'\n')
(root/'theme_sources/palette_keys.json').write_text(json.dumps(keys,indent=2)+'\n')
