"""Import AOSP DeskClock 4.2.2 reference assets without changing NavPlus artwork."""
import argparse,hashlib,json,pathlib,shutil,subprocess,xml.etree.ElementTree as ET
from PIL import Image
parser=argparse.ArgumentParser();parser.add_argument('source',type=pathlib.Path);args=parser.parse_args()
repo=pathlib.Path(__file__).resolve().parents[3];source=args.source.resolve();dest=repo/'BetterGPS/Overlay/BetterGPS_assets/material/copilot/t680/deskclock';dest.mkdir(parents=True,exist_ok=True)
icons=['ic_tab_timer_normal','ic_tab_timer_activated','ic_tab_clock_normal','ic_tab_clock_activated','ic_tab_stopwatch_normal','ic_tab_stopwatch_activated','ic_alarm','ic_alarm_small','ic_location','ic_add','ic_delete_normal','ic_backspace_normal','ic_lap_normal','ic_reset_normal','ic_start_normal','ic_stop_normal','ic_menu_done_holo_dark','ic_menu_revert_holo_dark','ic_ringtone','clock_analog_dial','clock_analog_hour','clock_analog_minute','clock_analog_second','stat_notify_timer','stat_notify_stopwatch','stat_notify_alarm']
report={'tag':'android-4.2.2_r1','commit':(source/'.git/HEAD').read_text().strip(),'license':'Apache-2.0','files':{}}
for name in icons:
 candidates=[source/'res'/density/(name+'.png') for density in ['drawable-xhdpi','drawable-hdpi','drawable-mdpi']];p=next((p for p in candidates if p.is_file()),None)
 if p is None:raise RuntimeError('Missing '+name)
 im=Image.open(p).convert('RGBA');out=dest/(name+'.dds');im.save(out,format='DDS')
 assert Image.open(out).convert('RGBA').tobytes()==im.tobytes()
 report['files'][out.name]={'source':str(p.relative_to(source)),'source_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'size':im.size}
ffmpeg=shutil.which('ffmpeg');assert ffmpeg,'ffmpeg required for Android OGG sounds'
for name,relative in [('timer','assets/sounds/Timer_Expire.ogg'),('alarm','res/raw/fallbackring.ogg')]:
 out=dest/(name+'.wav');subprocess.run([ffmpeg,'-v','error','-y','-i',str(source/relative),'-ac','1','-ar','22050','-c:a','pcm_s16le',str(out)],check=True)
 report['files'][out.name]={'source':relative,'sha256':hashlib.sha256(out.read_bytes()).hexdigest()}
shutil.copy2(source/'NOTICE',dest/'NOTICE-AOSP.txt')
arrays={a.attrib['name']:[item.text for item in a] for a in ET.parse(source/'res/values/array.xml').getroot() if 'name' in a.attrib}
build=repo/'build/deskclock';build.mkdir(parents=True,exist_ok=True);zones=build/'iana-zones.json';zones.write_text(json.dumps(arrays['cities_tz']),encoding='utf-8')
ps="Get-Content -Raw -LiteralPath '"+str(zones).replace("'","''")+"' | ConvertFrom-Json | ForEach-Object { $w=''; $ok=[TimeZoneInfo]::TryConvertIanaIdToWindowsId([string]$_,[ref]$w); [pscustomobject]@{iana=$_;windows=$w;ok=$ok} } | ConvertTo-Json -Compress"
rows=json.loads(subprocess.run(['pwsh.exe','-NoProfile','-NonInteractive','-Command',ps],check=True,capture_output=True,text=True,encoding='utf-8').stdout);mapping={r['iana']:r['windows'] if r['ok'] else '' for r in rows}
missing=[z for z in arrays['cities_tz'] if not mapping.get(z)];assert not missing,missing
header=['#pragma once','#include <array>','namespace bettergps_deskclock {','struct City {const char* name;const wchar_t* windows;};','inline constexpr std::array<City,'+str(len(arrays['cities_names']))+'> cities{{']
for name,zone in zip(arrays['cities_names'],arrays['cities_tz']):header.append('{u8'+json.dumps(name,ensure_ascii=True)+',L'+json.dumps(mapping[zone])+'},')
header+=['}};','}'];(repo/'BetterGPS/Overlay/DeskClockCities.h').write_text('\n'.join(header)+'\n',encoding='utf-8')
ref=repo/'docs/references/android-deskclock';ref.mkdir(parents=True,exist_ok=True)
for rel in ['NOTICE','MODULE_LICENSE_APACHE2','AndroidManifest.xml','res/values/colors.xml','res/values/dimens.xml','res/layout/clock_fragment.xml','res/layout/stopwatch_fragment.xml','res/layout/timer_fragment.xml','res/layout/set_alarm.xml','src/com/android/deskclock/timer/TimerObj.java','src/com/android/deskclock/stopwatch/Stopwatches.java','src/com/android/deskclock/Alarm.java','src/com/android/deskclock/CircleTimerView.java']:
 target=ref/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/rel,target)
report['cities']=len(arrays['cities_names']);(dest/'import.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');(ref/'provenance.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('Imported',len(report['files']),'assets and',report['cities'],'cities. RGBA pixels verified; no existing NavPlus asset changed.')
