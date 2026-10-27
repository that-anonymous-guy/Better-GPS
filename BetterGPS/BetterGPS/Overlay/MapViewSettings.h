#pragma once
#include "DisplayRegistry.h"
#include "DeviceLimits.h"
#include "MapPanState.h"
#include <array>
namespace bettergps_overlay {
struct MapViewSetting {double zoom=1,x=0,z=0;};
inline const std::array<MapViewSetting,4> mapViewDefaults={MapViewSetting{1,0,0},MapViewSetting{0.6,10,-120},MapViewSetting{1,0,0},MapViewSetting{1,0,0}};
inline const std::array<MapViewSetting,4> mapF150Defaults={MapViewSetting{0.8,-40,0},MapViewSetting{0.5,-270,-120},MapViewSetting{1,-500,-100},MapViewSetting{1,-1850,-600}};
inline const char* mapViewNames[]={"3D - Close","3D - Far","2D - Close","2D - Far"};
inline std::array<MapViewSetting,4> mapViews=mapViewDefaults,mapF150Views=mapF150Defaults;
// Managed camera IDs stay slot+2. The named Bronco fallback lives beyond them;
// its persisted [device.bronco_infotainment.view.*] sections are unchanged.
inline constexpr unsigned mapBroncoDevice=managedDeviceCount+2;
inline std::array<MapViewSetting,4> mapBroncoViews=mapF150Defaults;
inline unsigned (*mapViewDeviceOverride)()=nullptr;
inline MapViewSetting (*mapViewCustomSnapshot)(unsigned,unsigned)=nullptr;
inline void (*mapViewCustomAdjust)(unsigned,unsigned,int,int)=nullptr;
inline void (*mapViewCustomReset)(unsigned,unsigned)=nullptr;
inline unsigned (*mapViewNativeDeviceOverride)(unsigned)=nullptr;
inline unsigned map_view_builtin_device(){auto i=selectedDisplay.load();return i<gpsDisplays.size()&&gpsDisplays[i].id=="bronco_infotainment"?mapBroncoDevice:i<gpsDisplays.size()&&ford_console_id(gpsDisplays[i].id)?1:0;}
inline unsigned map_view_device(){return mapViewDeviceOverride?mapViewDeviceOverride():map_view_builtin_device();}
inline const char* mapViewDevices[]={"phone","f150_infotainment","bronco_infotainment"};
inline std::mutex mapViewMutex;
inline std::filesystem::path mapViewPath;
inline std::atomic<uint64_t> mapViewRevision{0};
inline std::atomic<bool> mapViewDirty{false};
inline std::atomic<unsigned> mapViewEditing{1},mapViewCurrent{0};
// Transient native-preset selection, per rendered GPS. Low two source bits
// hold the game's mode; the remaining bits invalidate choices on a real key
// change or source teardown. No key injection or saved camera edits.
inline std::atomic<uint64_t> mapViewSourceStamp{0};
inline std::array<std::atomic<uint64_t>,2+managedDeviceCount> mapViewModeChoices{};
inline void map_view_observe_mode(unsigned mode){
 if(mode>3)return;auto stamp=mapViewSourceStamp.load();
 while((stamp&3)!=mode&&!mapViewSourceStamp.compare_exchange_weak(stamp,((stamp+4)&~uint64_t(3))|mode)){}
 mapViewCurrent=mode;
}
inline void map_view_reset_modes(){mapViewSourceStamp.fetch_add(4);for(auto& choice:mapViewModeChoices)choice=0;mapPan.reset_all();}
inline unsigned map_view_mode_for(unsigned device){
 const auto stamp=mapViewSourceStamp.load();if(device<mapViewModeChoices.size()){const auto choice=mapViewModeChoices[device].load();if((choice&7)>=1&&(choice&7)<=4&&(choice>>3)==stamp)return unsigned((choice&7)-1);}
 return mapViewCurrent.load();
}
inline unsigned map_view_step_mode(unsigned device,int direction){
 if(device<2||device>=mapViewModeChoices.size()||!direction)return map_view_mode_for(device);
 auto& cell=mapViewModeChoices[device];for(;;){const auto stamp=mapViewSourceStamp.load();auto choice=cell.load();const unsigned mode=(choice&7)>=1&&(choice&7)<=4&&(choice>>3)==stamp?unsigned((choice&7)-1):unsigned(stamp&3);
  const unsigned next=unsigned(std::clamp(int(mode)+(direction>0?1:-1),0,3));if(next==mode)return mode;
  if(!cell.compare_exchange_weak(choice,(stamp<<3)|(next+1)))continue;
  if(mapViewSourceStamp.load()!=stamp)continue;++mapViewRevision;return next;
 }
}
inline MapViewSetting map_view_snapshot_for(unsigned device,unsigned i){if(mapViewNativeDeviceOverride)device=mapViewNativeDeviceOverride(device);if(device==mapBroncoDevice){std::lock_guard<std::mutex> lock(mapViewMutex);return mapBroncoViews[std::min(i,3u)];}if(device>=2&&mapViewCustomSnapshot)return mapViewCustomSnapshot(device,i);std::lock_guard<std::mutex> lock(mapViewMutex);return (device?mapF150Views:mapViews)[std::min(i,3u)];}
inline MapViewSetting map_view_snapshot(unsigned i){return map_view_snapshot_for(0,i);}
inline MapViewSetting map_view_selected_snapshot(unsigned i){return map_view_snapshot_for(map_view_device(),i);}
inline void map_views_init(const std::filesystem::path& base){
 map_view_reset_modes();
 std::lock_guard<std::mutex> lock(mapViewMutex);mapViewPath=base/"BetterGPSMapViews.ini";mapViews=mapViewDefaults;mapF150Views=mapF150Defaults;mapBroncoViews=mapF150Defaults;
 std::ifstream file(mapViewPath);std::string line;int section=-1,device=0;bool legacy=false,fordExplicit=false;
 while(std::getline(file,line)){
  line=display_trim(line);
  if(!line.empty()&&line.front()=='['){section=-1;device=0;
   for(unsigned i=0;i<4;++i){if(line=="[view."+std::to_string(i)+"]"){section=int(i);legacy=true;}
    for(unsigned d=0;d<3;++d)if(line=="[device."+std::string(mapViewDevices[d])+".view."+std::to_string(i)+"]"){section=int(i);device=d==2?int(mapBroncoDevice):int(d);if(d==1)fordExplicit=true;}}
   continue;
  }
  auto at=line.find('=');if(section<0||at==std::string::npos)continue;
  try{auto key=display_trim(line.substr(0,at));size_t used=0;auto text=display_trim(line.substr(at+1));double v=std::stod(text,&used);if(used!=text.size()||!std::isfinite(v))continue;auto& view=(device==mapBroncoDevice?mapBroncoViews:device?mapF150Views:mapViews)[section];if(key=="Zoom")view.zoom=std::clamp(v,.25,4.0);else if(key=="OffsetX")view.x=std::clamp(v,-5000.,5000.);else if(key=="OffsetZ")view.z=std::clamp(v,-5000.,5000.);}catch(...){}
 }
 if(legacy&&!fordExplicit)mapF150Views=mapViews;
 mapViewDirty=legacy;++mapViewRevision;
}
inline void map_view_adjust(int field,int steps){if(map_view_device()!=mapBroncoDevice&&map_view_device()>=2&&mapViewCustomAdjust){mapViewCustomAdjust(map_view_device(),mapViewEditing.load(),field,steps);return;}std::lock_guard<std::mutex> lock(mapViewMutex);auto& v=(map_view_device()==mapBroncoDevice?mapBroncoViews:map_view_device()?mapF150Views:mapViews)[std::min(mapViewEditing.load(),3u)];if(field==0)v.zoom=std::clamp(std::round((v.zoom+steps*.05)*100)/100,.25,4.);else if(field==1)v.x=std::clamp(v.x+steps*5.,-5000.,5000.);else v.z=std::clamp(v.z+steps*5.,-5000.,5000.);mapViewDirty=true;++mapViewRevision;}
inline void map_view_reset(){if(map_view_device()!=mapBroncoDevice&&map_view_device()>=2&&mapViewCustomReset){mapViewCustomReset(map_view_device(),mapViewEditing.load());return;}std::lock_guard<std::mutex> lock(mapViewMutex);auto i=std::min(mapViewEditing.load(),3u);(map_view_device()==mapBroncoDevice?mapBroncoViews:map_view_device()?mapF150Views:mapViews)[i]=(map_view_device()?mapF150Defaults:mapViewDefaults)[i];mapViewDirty=true;++mapViewRevision;}
inline bool map_views_save(){
 if(!mapViewDirty.exchange(false))return true;std::array<std::array<MapViewSetting,4>,3> views;std::filesystem::path path;
 {std::lock_guard<std::mutex> lock(mapViewMutex);views={mapViews,mapF150Views,mapBroncoViews};path=mapViewPath;}
 if(path.empty())return true;auto tmp=path;tmp+=L".tmp";std::ofstream file(tmp);
 file<<"# BetterGPS cameras, independently rendered for phone and F150 infotainment.\n# Zoom >1 closer, <1 farther. X/Z are camera-relative game metres.\n";
 for(unsigned d=0;d<views.size();++d)for(unsigned i=0;i<4;++i){auto& v=views[d][i];file<<"[device."<<mapViewDevices[d]<<".view."<<i<<"]\nZoom="<<v.zoom<<"\nOffsetX="<<v.x<<"\nOffsetZ="<<v.z<<"\n";}
 file.close();if(file&&MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;mapViewDirty=true;return false;
}
}
