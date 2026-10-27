#pragma once
#include <Windows.h>
#include <array>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>
#include "DesignStyle.h"
#include "GarminLayout.h"
#include "NavplusLayout.h"
#include "CopilotLayout.h"
#include "ZonarLayout.h"
#include "InternationalLayout.h"
#include "TomtomLayout.h"
#include "SygicLayout.h"
#include "Vnl2014Layout.h"
#include "generated/PhoneModuleLayout.h"
#include "generated/WazeModuleLayout.h"
#include "generated/HorizontalModules.h"
#include "generated/F150PlacementDefaults.h"
namespace bettergps_overlay {
constexpr size_t overlayModuleCount=sizeof(phoneModules)/sizeof(phoneModules[0]);
constexpr size_t overlayItemCount=overlayModuleCount+1;
struct Placement { int x{},y{},w{},h{}; };
struct OverlaySettings { bool horizontal=false;int navplus=0;int theme=0,format=0,garminLayout=0; std::array<Placement,overlayItemCount> items; std::array<double,overlayItemCount> scales; uint64_t revision=0; };
inline OverlaySettings overlay_base_settings(bool horizontal=false,int format=-1,int navplus=0){OverlaySettings s;s.navplus=navplus;s.horizontal=horizontal;s.format=format<0?(horizontal?1:0):format;s.scales.fill(1.0);if(sygic_style()){s.navplus=8;const auto modules=sygic_modules();for(size_t i=0;i<overlayItemCount;++i){const auto& m=modules[i];s.items[i]={m.x,m.y,m.w,m.h};}return s;}if(vnl2014_style()){s.navplus=6;const auto modules=vnl2014_modules();for(size_t i=0;i<overlayItemCount;++i){const auto& m=modules[i];s.items[i]={m.x,m.y,m.w,m.h};}return s;}if(tomtom_style()){const bool volvo=navplus==4||navplus==7;s.navplus=navplus==9?9:volvo?7:5;const auto modules=navplus==9?icc5_modules():tomtom_modules(volvo);for(size_t i=0;i<overlayItemCount;++i){const auto& m=modules[i];s.items[i]={m.x,m.y,m.w,m.h};}return s;}if(copilot_style()){s.navplus=navplus==12?12:zonar_layout(navplus)?navplus:4;const auto modules=navplus==12?international_modules():zonar_layout(navplus)?zonar_modules(navplus==11):copilot_modules();for(size_t i=0;i<overlayItemCount;++i){const auto& m=modules[i];s.items[i]={m.x,m.y,m.w,m.h};}return s;}if(garmin_style()){s.garminLayout=navplus?(navplus==3&&s.format==2?7:s.format==3?6:5):2;const auto modules=navplus?navplus_modules(s.format,navplus>=2,navplus==3):garmin_modules(s.format);for(size_t i=0;i<overlayItemCount;++i){auto& m=modules[i];s.items[i]={int(std::lround(m.x*(navplus?1.:.75))),int(std::lround(m.y*(navplus?1.:.75))),int(std::lround(m.w*(navplus?1.:.75))),int(std::lround(m.h*(navplus?1.:.75)))};}return s;}for(size_t i=0;i<overlayModuleCount;++i){auto& m=horizontal?(waze_style()?horizontalWaze[i]:horizontalGoogle[i]):waze_style()?wazeModules[i]:phoneModules[i];s.items[i]={m.x,m.y,int(m.width),int(m.height)};}
 s.items[overlayModuleCount]=waze_style()?Placement{10,510,92,98}:Placement{18,520,72,126};
 if(horizontal){
  auto place=[&](size_t i,int x,int y,double scale){const auto& m=waze_style()?horizontalWaze[i]:horizontalGoogle[i];s.items[i]={x,y,int(std::lround(m.width*scale)),int(std::lround(m.height*scale))};};
  for(size_t i:{size_t(0),size_t(2)})place(i,575,96,.9);
  if(waze_style()){
   place(0,575,46,.9);place(2,575,46,.9);place(1,90,42,.96);place(3,103,101,.9);
   for(size_t i:{size_t(4),size_t(5)})place(i,650,132,.95);
   for(size_t i:{size_t(6),size_t(7)})place(i,103,360,.97);
   for(size_t i:{size_t(8),size_t(9)})place(i,454,121,.8);
   for(size_t i:{size_t(10),size_t(11)})place(i,103,103,.8);
   for(size_t i:{size_t(12),size_t(13)})place(i,103,333,.9);
   s.items[overlayModuleCount]={100,262,85,90};
  }else{
   place(1,575,96,.9);place(2,575,46,.9);place(3,103,95,.8);
   for(size_t i:{size_t(4),size_t(5)})place(i,650,132,.85);
   for(size_t i:{size_t(6),size_t(7)})place(i,103,355,.85);
   for(size_t i:{size_t(8),size_t(9)})place(i,103,100,.85);
   for(size_t i:{size_t(10),size_t(11)})place(i,724,326,.85);
   for(size_t i:{size_t(12),size_t(13)})place(i,103,384,.85);
   s.items[overlayModuleCount]={110,240,62,112};
  }
 }
 return s;}
inline OverlaySettings default_overlay_settings(bool horizontal=false,int format=-1,int navplus=0){auto s=overlay_base_settings(horizontal,format,navplus);if(horizontal&&!garmin_style()&&!copilot_style()&&!tomtom_style()&&!vnl2014_style()&&!sygic_style())for(size_t i=0;i<overlayItemCount;++i){auto& p=f150PlacementDefaults[waze_style()?1:0][i];s.items[i].x=int(p[0]);s.items[i].y=int(p[1]);s.scales[i]=p[2];s.items[i].w=int(std::lround(s.items[i].w*p[2]));s.items[i].h=int(std::lround(s.items[i].h*p[2]));}return s;}
inline void overlay_apply_scale(OverlaySettings& s,size_t item,double scale){auto base=overlay_base_settings(s.horizontal,s.format,s.navplus).items[item];scale=std::clamp(scale,.10,3.0);s.scales[item]=scale;s.items[item].w=int(std::lround(base.w*scale));s.items[item].h=int(std::lround(base.h*scale));}
inline bool overlay_navplus_selected();
inline const char* overlay_name(size_t i){if(sygic_style()){static const auto names=sygic_modules();return names[std::min(i,names.size()-1)].name;}if(vnl2014_style()){static const auto names=vnl2014_modules();return names[std::min(i,names.size()-1)].name;}if(tomtom_style()){static const auto names=tomtom_modules();return names[std::min(i,names.size()-1)].name;}if(copilot_style()){static const auto names=copilot_modules();return names[std::min(i,names.size()-1)].name;}if(garmin_style()&&overlay_navplus_selected()){static const auto names=navplus_modules();return names[std::min(i,names.size()-1)].name;}if(garmin_style()){static const auto names=garmin_modules(1);return names[std::min(i,names.size()-1)].name;}static const char* names[]={"above_light","above_green","above_dark","nav_instructions","nav_buttons_light","nav_buttons_dark","nav_below_light","nav_below_dark","notrip_search_and_buttons_light","notrip_search_and_buttons_dark","notrip_buttons_below_light","notrip_buttons_below_dark","notrip_below_light","notrip_below_dark","speedometer / real speed"};static const char* wazeNames[]={"above_light","above_dark_navigation","above_dark","Waze instructions","Waze buttons light","Waze buttons dark","Waze trip footer light","Waze trip footer dark","Waze compass light","Waze compass dark","Waze menu light","Waze menu dark","Waze no trip footer light","Waze no trip footer dark","speedometer / real speed"};return waze_style()?wazeNames[i]:names[i];}
inline std::atomic<bool> overlayManagedSelected{false};
// Only old Garmin profiles are rebased. Preserve each module's custom offset
// and scale, and leave every Google/Waze/device-specific file untouched.
inline bool overlay_migrate_garmin(OverlaySettings& s,std::istream& in){
 if(s.navplus==12){if(!in)return false;std::string marker;int version=0;if(!(in>>marker>>version)||marker!="InternationalLayout")version=0;if(version>=3)return false;if(version<2)for(auto change:std::array<std::array<int,3>,5>{{{{4,0,12}},{{5,0,12}},{{8,-40,4}},{{11,-60,-33}},{{14,-60,-115}}}}){s.items[change[0]].x+=change[1];s.items[change[0]].y+=change[2];}s.items[3].x+=15;s.items[3].y+=23;return true;}
 // TomTom and Dynafleet have independent canvases. A nonzero navplus tag is
 // not proof of a Garmin layout; saved TomTom profiles otherwise gained the
 // Garmin 5:3 road-origin shift (+60px) while unsaved profiles stayed correct.
 if(s.navplus==5||s.navplus==6||s.navplus==7||s.navplus==8||s.navplus==9||s.navplus==12||zonar_layout(s.navplus))return false;
 if(s.navplus==4){if(!in)return false;std::string marker;int version=0;if(!(in>>marker>>version)||marker!="CoPilotLayout")version=0;if(version>=5)return false;if(version<2){s.items[3].y-=10;s.items[5].x+=20;s.items[5].y-=10;s.items[8].x+=10;}if(version<3){s.items[3].y+=3;s.items[4].x+=16;s.items[4].y+=17;s.items[6].y-=1;s.items[7].x+=10;s.items[7].y+=23;}if(version<4){s.items[13].x+=18;s.items[13].y+=337;}s.items[2].x+=120;s.items[2].y+=87;return true;}
 if(s.navplus){
  if(!in)return false;std::string marker;int version=0;
  if(!(in>>marker>>version)||marker!="GarminLayout")version=0;
  const int current=s.navplus==3&&s.format==2?7:s.format==3?6:5;if(version>=current)return false;
  // Preserve custom offsets/scales. Version 4 already enlarged/recentered
  // LIMIT; now its bottom follows the rendered m rather than the Speed label.
  if(version<4){s.items[14].x-=6;s.items[14].y-=23;}
  if(version<5)s.items[14].y-=22;
  // Recomputed base dimensions enlarge instructions/header at their saved
  // top-left. Move only the road origin, retaining every custom offset/scale.
  if(s.format==3&&version<6)s.items[5].x+=60;
  if(s.navplus==3&&s.format==2&&version<7){s.items[5].x+=77;s.items[4].y+=82;}
  s.garminLayout=current;return true;
 }
 if(!s.garminLayout)return false;std::string marker;int version=0;
 if(in>>marker>>version&&marker=="GarminLayout"&&version>=2)return false;
 const auto before=garmin_modules(s.format,true),after=garmin_modules(s.format);
 for(size_t i=0;i<overlayItemCount;++i){s.items[i].x+=int(std::lround(after[i].x*.75))-int(std::lround(before[i].x*.75));s.items[i].y+=int(std::lround(after[i].y*.75))-int(std::lround(before[i].y*.75));}
 return true;
}
inline std::mutex overlaySettingsMutex;
inline OverlaySettings overlaySettings=default_overlay_settings();
inline std::filesystem::path overlaySettingsPath;
inline uint64_t overlaySavedRevision=0;
inline bool overlay_navplus_selected(){std::lock_guard<std::mutex> l(overlaySettingsMutex);return overlaySettings.navplus;}
inline OverlaySettings overlay_snapshot(){std::lock_guard<std::mutex> l(overlaySettingsMutex);return overlaySettings;}
// Reading an app's saved profile must not temporarily change the menu/editor
// of a different physical device.
inline OverlaySettings overlay_read(const std::filesystem::path& path,bool horizontal=false,int format=-1,int navplus=0,bool* dirty=nullptr){auto next=default_overlay_settings(horizontal,format,navplus);std::ifstream f(path);std::string magic;int version=0;if(f>>magic>>version&&magic=="BetterGPSOverlay"&&(version==1||version==2)){int theme=0;if(f>>theme)next.theme=std::clamp(theme,0,2);for(size_t i=0;i<overlayItemCount;++i){int x,y,w,h;double scale;if(version==1){if(!(f>>x>>y>>w>>h)||w<=0||h<=0){next=default_overlay_settings(horizontal,format,navplus);break;}scale=double(w)/next.items[i].w;}else if(!(f>>x>>y>>scale)||!std::isfinite(scale)){next=default_overlay_settings(horizontal,format,navplus);break;}next.items[i].x=std::clamp(x,horizontal?-800:-360,horizontal?1600:720);next.items[i].y=std::clamp(y,horizontal?-500:-740,horizontal?1000:1480);overlay_apply_scale(next,i,scale);}}const bool migrated=overlay_migrate_garmin(next,f);if(dirty)*dirty=version==1||migrated;return next;}
inline void overlay_load(const std::filesystem::path& path,bool horizontal=false,int format=-1,int navplus=0){std::lock_guard<std::mutex> l(overlaySettingsMutex);bool dirty=false;auto next=overlay_read(path,horizontal,format,navplus,&dirty);overlaySettingsPath=path;next.revision=overlaySettings.revision+1;overlaySettings=next;overlaySavedRevision=dirty?0:next.revision;}
inline std::mutex overlaySaveMutex;
inline bool overlay_save(){std::lock_guard<std::mutex> writer(overlaySaveMutex);OverlaySettings saved;std::filesystem::path path;{std::lock_guard<std::mutex> l(overlaySettingsMutex);if(overlaySettingsPath.empty()||overlaySavedRevision==overlaySettings.revision)return true;saved=overlaySettings;path=overlaySettingsPath;}auto temp=path;temp+=L".tmp";std::ofstream f(temp,std::ios::trunc);if(!f)return false;f.precision(12);f<<"BetterGPSOverlay 2\n"<<saved.theme<<'\n';for(size_t i=0;i<overlayItemCount;++i){auto& p=saved.items[i];f<<p.x<<' '<<p.y<<' '<<saved.scales[i]<<'\n';}if(saved.navplus==4)f<<"CoPilotLayout 5\n";if(saved.navplus==12)f<<"InternationalLayout 3\n";if(saved.garminLayout)f<<"GarminLayout "<<saved.garminLayout<<'\n';f.close();if(!f||!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;{std::lock_guard<std::mutex> l(overlaySettingsMutex);if(overlaySettingsPath==path&&overlaySettings.revision==saved.revision)overlaySavedRevision=saved.revision;}return true;}
inline void overlay_theme(int theme){std::lock_guard<std::mutex> l(overlaySettingsMutex);overlaySettings.theme=std::clamp(theme,0,2);++overlaySettings.revision;}
inline void overlay_adjust(size_t item,int field,int delta){std::lock_guard<std::mutex> l(overlaySettingsMutex);if(field>=2)overlay_apply_scale(overlaySettings,item,overlaySettings.scales[item]+delta/100.0);else{auto& p=overlaySettings.items[item];if(field==0)p.x=std::clamp(p.x+delta,overlaySettings.horizontal?-800:-360,overlaySettings.horizontal?1600:720);else p.y=std::clamp(p.y+delta,overlaySettings.horizontal?-500:-740,overlaySettings.horizontal?1000:1480);}++overlaySettings.revision;}
inline void overlay_reset(int item){std::lock_guard<std::mutex> l(overlaySettingsMutex);auto defaults=default_overlay_settings(overlaySettings.horizontal,overlaySettings.format,overlaySettings.navplus);if(item<0){auto revision=overlaySettings.revision;overlaySettings=defaults;overlaySettings.revision=revision+1;}else{overlaySettings.items[item]=defaults.items[item];overlaySettings.scales[item]=defaults.scales[item];++overlaySettings.revision;}}
inline bool selected_dark(int theme,bool automatic){return theme==2||(theme==0&&automatic);}
// Straight-alpha compositing. Transform a module and its coded contents using
// the same local origin and scale; clip only at the phone boundary.
inline void overlay_blit(std::vector<uint8_t>& dst,const std::vector<uint8_t>& src,int sw,int sh,Placement out,int canvasWidth=360,int canvasHeight=740){if(sw<=0||sh<=0||src.size()!=size_t(sw)*sh*4||out.w<=0||out.h<=0)return;
 for(int y=std::max(0,out.y);y<std::min(canvasHeight,out.y+out.h);++y)for(int x=std::max(0,out.x);x<std::min(canvasWidth,out.x+out.w);++x){
  uint8_t sample[4];const uint8_t* a=nullptr;
  if(sw==out.w&&sh==out.h)a=&src[((y-out.y)*sw+x-out.x)*4];
  else{double u=(x-out.x+.5)*sw/out.w-.5,v=(y-out.y+.5)*sh/out.h-.5;int ix=int(std::floor(u)),iy=int(std::floor(v));double fx=u-ix,fy=v-iy,alpha=0,color[3]{};for(int j=0;j<2;++j)for(int i=0;i<2;++i){int sx=std::clamp(ix+i,0,sw-1),sy=std::clamp(iy+j,0,sh-1);auto* p=&src[(sy*sw+sx)*4];double weight=(i?fx:1-fx)*(j?fy:1-fy),coverage=weight*p[3];alpha+=coverage;for(int c=0;c<3;++c)color[c]+=coverage*p[c];}sample[3]=uint8_t(std::lround(alpha));for(int c=0;c<3;++c)sample[c]=alpha>0?uint8_t(std::lround(color[c]/alpha)):0;a=sample;}
  auto* b=&dst[(y*canvasWidth+x)*4];unsigned alpha=a[3];if(!alpha)continue;unsigned old=b[3],sum=alpha+(old*(255-alpha)+127)/255;for(int c=0;c<3;++c)b[c]=uint8_t((a[c]*alpha+(b[c]*old*(255-alpha)+127)/255+sum/2)/sum);b[3]=uint8_t(sum);}}
inline Placement attached_placement(const OverlaySettings& s,size_t item,Placement base){auto origin=default_overlay_settings(s.horizontal,s.format).items[item];auto p=s.items[item];double scale=s.scales[item];return {p.x+int(std::lround((base.x-origin.x)*scale)),p.y+int(std::lround((base.y-origin.y)*scale)),int(std::lround(base.w*scale)),int(std::lround(base.h*scale))};}
}
