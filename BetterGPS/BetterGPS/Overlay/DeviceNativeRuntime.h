#pragma once
#include "DeviceTemplates.h"
#include "NavplusTelemetry.h"
#include "T680GaugeTelemetry.h"
#include "T680ClusterIntro.h"
#include "MackClusterIntro.h"
#include "MackBootGuard.h"
#include "VnlClusterIntro.h"
#include "Volvo18ClusterIntro.h"
#include "Volvo25ClusterIntro.h"
#include "CascadiaClusterIntro.h"
#include "IcucClusterIntro.h"
#include "TruckBootGuard.h"
namespace bettergps_overlay {
void log(scs_log_type_t type,const char* fmt,...);
struct ManagedNativeApi {
 using LayerAssign=void*(__fastcall*)(uintptr_t*,uintptr_t);
 using WindowAssign=void*(__fastcall*)(uintptr_t*,uintptr_t*);
 LayerAssign layerAssign{};WindowAssign windowAssign{};bool supported=false;
 using ResolveModel=bool(__fastcall*)(void*,const char**,uintptr_t*,uint64_t*);ResolveModel resolveModel{};
};
inline ManagedNativeApi managedNativeApi;
struct ManagedNativeState {
 std::array<uintptr_t,2> held{},created{};uintptr_t pair=0,owner=0,model=0,windowSlot=0;int candidate=-1;
 uint64_t applied=0,queuedFrame=0,failedRevision=0,retryAfter=0;uint32_t key=0x7fffffff;uint16_t alias=0x3fff;
 bool cameraRearLayout=false,cameraLayout=false,phone=false,replaced=false,reportedQueue=false,reportedRoute=false,reportedProducer=false;std::string vehicle;
};
inline std::array<ManagedNativeState,managedDeviceCount> managedNative;
inline ManagedNativeState managedPreview;
inline uint64_t managedNativeFrameSerial=0;
inline bool managedNativeBusy=false;
inline std::map<uintptr_t,std::string> managedWindowPaths;
inline bool managed_native_readable(uintptr_t at,size_t bytes){MEMORY_BASIC_INFORMATION info{};if(!at||!bytes||at+bytes<at||!VirtualQuery((void*)at,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;const auto p=info.Protect&255;return (p==PAGE_READONLY||p==PAGE_READWRITE||p==PAGE_WRITECOPY||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY)&&at+bytes<=uintptr_t(info.BaseAddress)+info.RegionSize;}
inline uintptr_t managed_window_slot(const DeviceCandidate& c){return c.windowSlot?c.windowSlot:c.pair+8;}
inline uintptr_t managed_state_window_slot(const ManagedNativeState& s){return s.windowSlot?s.windowSlot:s.pair+8;}
} // namespace bettergps_overlay
#include "CameraNativeProbe.h"
#include "CameraNativeBackend.h"
namespace bettergps_overlay {
inline void managed_native_ui_path(void* window,const char* path){if(managedNativeBusy||managedBase.empty()||!window||!path||!display_ui_path(path)||std::string(path).find("/bettergps_device_")!=std::string::npos)return;std::lock_guard<std::mutex> l(managedMutex);if(managedWindowPaths.size()>256)managedWindowPaths.clear();managedWindowPaths[uintptr_t(window)]=path;}

inline bool managed_native_resolve(const MapImage& image){
 const auto base=uintptr_t(image.base);static const uint8_t prefix[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20};
 for(auto rva:{0x412780u,0x33def0u})if(!image.contains(base+rva,sizeof(prefix))||!image.executable(base+rva)||memcmp(image.base+rva,prefix,sizeof(prefix)))return false;
 static const uint8_t resolvePrefix[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18};if(!image.contains(base+0x54afb0,sizeof(resolvePrefix))||memcmp(image.base+0x54afb0,resolvePrefix,sizeof(resolvePrefix)))return false;
 managedNativeApi.resolveModel=(ManagedNativeApi::ResolveModel)(base+0x54afb0);managedNativeApi.layerAssign=(ManagedNativeApi::LayerAssign)(base+0x412780);managedNativeApi.windowAssign=(ManagedNativeApi::WindowAssign)(base+0x33def0);managedNativeApi.supported=true;navplus_temperature_resolve(image);t680_gauge_resolve(image);log(SCS_LOG_TYPE_message,"[CameraProbe] native locator observer %s",cameraNativeProbe.resolve(image)?"ready":"unavailable; no native camera calls");cameraNativeBackend.resolve(image);return true;
}
inline void managed_native_drop(std::array<uintptr_t,2>& p){if(!managedNativeApi.supported)return;uintptr_t empty=0;if(p[0])managedNativeApi.layerAssign(&p[0],0);if(p[1])managedNativeApi.windowAssign(&p[1],&empty);}
inline bool managed_native_pair(const DeviceCandidate& c){if(!map_memory(c.pair,16)||!map_memory(c.owner,0x1598)||(!c.windowSlot&&!map_memory(c.model,0x198)))return false;return map_memory(managed_window_slot(c),8)&&*(uintptr_t*)c.pair==c.layer&&*(uintptr_t*)managed_window_slot(c)==c.window;}
inline void managed_native_bind_layer(uintptr_t layer,uint16_t alias){if(layer&&map_memory(layer,0x168)){nativeFork.handle((void*)(layer+0x160),alias);nativeFork.layerUpdate((void*)layer);}}
inline void managed_native_refresh_candidate(int index){if(index<0)return;std::lock_guard<std::mutex> l(managedMutex);if(size_t(index)>=managedCandidates.size())return;auto& c=managedCandidates[index];if(map_memory(c.pair,16)){auto* p=(uintptr_t*)c.pair;c.layer=p[0];c.window=*(uintptr_t*)managed_window_slot(c);}}
inline void managed_native_restore(ManagedNativeState& state,bool restore){
 restore=restore&&state.vehicle==managed_vehicle();
 if(state.replaced&&map_memory(state.pair,16)){auto* p=(uintptr_t*)state.pair;auto* window=(uintptr_t*)managed_state_window_slot(state);if(map_memory(uintptr_t(window),8)&&p[0]==state.created[0]&&*window==state.created[1]&&restore){std::array<uintptr_t,2> current{p[0],*window};if(state.cameraLayout)log(SCS_LOG_TYPE_message,"[CameraGraph] restore-original owner=%p cameraWindow=%p originalWindow=%p tick=%llu",(void*)state.owner,(void*)*window,(void*)state.held[1],(unsigned long long)GetTickCount64());p[0]=state.held[0];*window=state.held[1];state.held={};managed_native_bind_layer(p[0],state.alias);managed_native_drop(current);managed_native_refresh_candidate(state.candidate);}}
 if(!state.replaced)managed_native_drop(state.created);managed_native_drop(state.held);state={};
}
inline void managed_native_observe(void* owner,void* output,const char* ui,const char* alias,const unsigned* dimensions,void* model){
 if(nativeForkInternal||managedNativeBusy||managedBase.empty()||!ui||!alias||!dimensions||!display_ui_path(ui)||!managed_alias_path(alias)||!map_memory(uintptr_t(output),16)||!model)return;
 if(std::string(ui).find("/bettergps_device_")!=std::string::npos)return;
 auto* pair=(uintptr_t*)output;if(!pair[0]||!pair[1])return;
 auto vehicle=managed_vehicle();std::vector<int> known;
 for(size_t i=0;i<gpsDisplays.size();++i)if(gpsDisplays[i].ready&&gpsDisplays[i].ui==ui)known.push_back(int(i));
 if(known.empty()){if(std::string(ui).rfind("/ui/dashboard/",0)!=0||std::string(ui).find("bare_map")!=std::string::npos)return;known.push_back(-1);}
 std::lock_guard<std::mutex> l(managedMutex);
 for(int match:known){DeviceCandidate next;next.owner=uintptr_t(owner);next.pair=uintptr_t(output);next.layer=pair[0];next.window=pair[1];next.model=uintptr_t(model);next.width=dimensions[0];next.height=dimensions[1];next.ui=ui;next.alias=alias;next.vehicle=vehicle;next.known=match;next.name=match>=0?gpsDisplays[match].name:std::filesystem::path(ui).stem().u8string();
  auto found=std::find_if(managedCandidates.begin(),managedCandidates.end(),[&](auto& c){return c.pair==next.pair&&c.known==match;});if(found!=managedCandidates.end())*found=next;else if(managedCandidates.size()<managedCandidateLimit)managedCandidates.push_back(std::move(next));
 }
 ++managedRevision;
}
inline void managed_native_seen(void* window){if(managedNativeBusy||managedBase.empty())return;auto vehicle=managed_vehicle();std::lock_guard<std::mutex> l(managedMutex);const auto now=GetTickCount64();for(auto& c:managedCandidates)if(c.window==uintptr_t(window)&&map_memory(c.pair,16)&&map_memory(managed_window_slot(c),8)&&*(uintptr_t*)managed_window_slot(c)==c.window){c.seen=now;c.vehicle=vehicle;}}
inline int managed_native_find(const ManagedDevice& d){std::lock_guard<std::mutex> l(managedMutex);for(size_t i=0;i<managedCandidates.size();++i){auto& c=managedCandidates[i];if(c.ui==d.ui&&c.alias==d.alias&&c.known==d.known&&c.vehicle==d.vehicle&&managed_native_pair(c))return int(i);}return -1;}
inline DeviceCandidate managed_native_candidate(int i){std::lock_guard<std::mutex> l(managedMutex);return i>=0&&size_t(i)<managedCandidates.size()?managedCandidates[i]:DeviceCandidate{};}
inline bool managed_is_dlc_gps(const DeviceCandidate& c){return c.known<0&&c.ui=="/ui/dashboard/gps.dlc_toys.sii"&&c.alias=="/vehicle/truck/upgrade/interior_decors/toyglass/gps_ui.tobj";}
inline void managed_native_defaults(void* owner,uint64_t now){
 const auto vehicle=managed_vehicle();if(vehicle.empty())return;std::lock_guard<std::mutex> lock(managedMutex);
 for(const auto& c:managedCandidates){
  if(!(managed_is_dlc_gps(c)||(c.windowSlot&&c.known<0&&(managed_international_identity(c.vehicle,c.ui,c.alias)||managed_navplus_identity(c.vehicle,c.ui,c.alias)||managed_copilot_identity(c.vehicle,c.ui,c.alias)||managed_mack_identity(c.vehicle,c.ui,c.alias)||managed_vnl2014_identity(c.vehicle,c.ui,c.alias)||managed_volvo18_identity(c.vehicle,c.ui,c.alias)||managed_volvo25_identity(c.vehicle,c.ui,c.alias)||managed_icc5_identity(c.vehicle,c.ui,c.alias)||managed_zonar_identity(c.vehicle,c.ui,c.alias))))||c.owner!=uintptr_t(owner)||c.vehicle!=vehicle||!c.seen||now<c.seen||now-c.seen>2000||!managed_native_pair(c))continue;
  // A saved choice, including Original/disabled or another style, always wins.
  if(std::any_of(managedDevices.begin(),managedDevices.end(),[&](const auto& d){return d.used&&d.vehicle==vehicle&&d.ui==c.ui&&d.alias==c.alias&&d.known==c.known;}))continue;
  auto free=std::find_if(managedDevices.begin(),managedDevices.end(),[](const auto& d){return !d.used;});if(free==managedDevices.end()){managedError=managed_capacity_error();static std::string reportedVehicle;if(reportedVehicle!=vehicle){reportedVehicle=vehicle;log(SCS_LOG_TYPE_warning,"[DeviceAuto] Cannot acquire GPS vehicle=%s: all %u device slots are occupied; saved profiles retained",vehicle.c_str(),managedDeviceCount);}continue;}
  ManagedDevice d;d.used=d.enabled=d.horizontal=d.hasPrefs=true;d.known=-1;d.style=2;d.format=1;d.skin=2;d.startup=2;d.splash="garmin/nuvi465t";d.name="Garmin Nuvi 465T - Windshield GPS";d.vehicle=vehicle;d.ui=c.ui;d.alias=c.alias;d.nativeWidth=c.width;d.nativeHeight=c.height;d.prefs.power=1;d.prefs.key=0;d.prefs.mods=0;d.prefs.firstDelay=0;
  if(managed_navplus_identity(c.vehicle,c.ui,c.alias)){d.name=managed_dash_name(c.vehicle);d.format=managed_dash_format(c.vehicle);d.splash=managed_dash_splash(c.vehicle);}
  if(managed_copilot_identity(c.vehicle,c.ui,c.alias)){d.name="Kenworth T680 2022 NavPlus HD";d.style=3;d.format=3;d.splash="copilot/t680";}if(managed_mack_identity(c.vehicle,c.ui,c.alias)){d.name=vehicle=="mack.anthem"?"Mack Anthem Infotainment":"Mack Pinnacle Infotainment";d.style=4;d.format=3;d.splash="tomtom/mack";}if(managed_volvo18_identity(c.vehicle,c.ui,c.alias)){d.name=vehicle=="volvo.vnl2018"?"Volvo VNL 2018 TomTom":"Volvo VNR TomTom";d.style=4;d.format=3;d.splash="tomtom/volvo18";}if(managed_volvo25_identity(c.vehicle,c.ui,c.alias)){d.name="Volvo VNL 2025 Sygic";d.style=6;d.format=3;d.splash="sygic/vnl2025";}if(managed_international_identity(c.vehicle,c.ui,c.alias)){d.name=vehicle=="intnational.lt"?"International LT T8520":"International LoneStar T8520";d.style=3;d.format=3;d.splash="copilot/international";}if(managed_zonar_identity(c.vehicle,c.ui,c.alias)){d.name=vehicle=="westernstar.5700xe"?"Western Star 5700XE - Zonar":vehicle=="westernstar.49x"?"Western Star 49X - Zonar":"Freightliner Cascadia analog - Zonar";d.style=3;d.format=2;d.splash="copilot/zonar";}if(managed_icc5_identity(c.vehicle,c.ui,c.alias)){d.name=vehicle=="westernstar.57x"?"Western Star 57X ICC5":"Freightliner Cascadia ICC5";d.style=4;d.format=1;d.splash="tomtom/cascadia_icc5";}if(managed_vnl2014_identity(c.vehicle,c.ui,c.alias)){d.name="Volvo VNL 2014 Dynafleet";d.style=5;d.format=3;d.splash="garmin/vnl2014";}
  {DisplayStyleScope style(d.style);d.overlay=default_overlay_settings(true,d.format,managed_dash_kind(d));}const auto slot=size_t(free-managedDevices.begin());*free=std::move(d);managedAppliedStyle[slot]=free->style;managedAppliedStartup[slot]=2;managedDirty=true;++managedRevision;
  log(SCS_LOG_TYPE_message,"[DeviceAuto] Garmin accessory observed vehicle=%s slot=%u",vehicle.c_str(),unsigned(slot));
 }
}
inline bool managed_native_create(unsigned slot,const ManagedDevice& d,int selected,ManagedNativeState& state,bool orange,bool camera=false,bool nativeRear=false){
 auto c=managed_native_candidate(selected);if(!managedNativeApi.supported||!nativeForkOriginalDashboardCreate||!managed_native_pair(c))return false;
 // A configurator reload may replace the interior model while retaining its UI.
 if(c.windowSlot){const char* alias=c.alias.c_str();uintptr_t model=0;uint64_t part=0;if(!managedNativeApi.resolveModel||!managedNativeApi.resolveModel((void*)c.owner,&alias,&model,&part)||!map_memory(model,0x198))return false;c.model=model;}
 const bool phone=c.known>=0&&size_t(c.known)<gpsDisplays.size()&&gpsDisplays[c.known].independent;
 if(phone&&((managedOutputAliases[slot]&0x3fff)==0x3fff||!(managedOutputAliases[slot]&0x8000)))return false;
 // DashboardCreate validates this alias against the actual model's drawable
 // materials (54b1f5 -> 54ac80). The private output is not a model material.
 // Create using the model alias, then assign only the new layer a private one.
 const auto sourceAlias=*(uint16_t*)(c.layer+0x160);
 std::array<uintptr_t,2> pair{};const auto path=managed_ui_name(slot,d.revision,orange,camera,nativeRear);const char* ui=path.c_str();const char* alias=c.alias.c_str();
 const auto renderSize=c.known<0?managed_render_dimensions(c.width,c.height):std::pair<unsigned,unsigned>{c.width,c.height};const unsigned dimensions[]={renderSize.first,renderSize.second};
 managedNativeBusy=true;const bool ok=nativeForkOriginalDashboardCreate((void*)c.owner,pair.data(),&ui,&alias,dimensions,(void*)c.model);managedNativeBusy=false;
 if(!ok||!pair[0]||!pair[1]){managed_native_drop(pair);managed_native_bind_layer(c.layer,sourceAlias);return false;}
 state.vehicle=c.vehicle;state.pair=c.pair;state.windowSlot=c.windowSlot;state.owner=c.owner;state.model=c.model;state.candidate=selected;state.phone=phone;state.alias=sourceAlias;state.created=pair;state.cameraLayout=camera;state.cameraRearLayout=nativeRear;state.applied=d.revision;state.failedRevision=state.retryAfter=0;
 if(phone){
  managed_native_bind_layer(pair[0],managedOutputAliases[slot]);
  managed_native_bind_layer(c.layer,sourceAlias);
 }else{
  auto* original=(uintptr_t*)c.pair;auto* window=(uintptr_t*)managed_window_slot(c);state.held={original[0],*window};original[0]=pair[0];*window=pair[1];state.replaced=true;managed_native_refresh_candidate(selected);
 }
 if(&state==&managedNative[slot])managedCameraLayoutActive[slot]=camera;
 if(d.style==3&&managed_is_copilot(d)&&d.vehicle.find("t680_2022")!=std::string::npos)log(SCS_LOG_TYPE_message,"[CameraGraph] replacement-ready slot=%u camera=%u rear=%u originalUi=%s generatedUi=%s window=%p tick=%llu",slot,unsigned(camera),unsigned(nativeRear),d.ui.c_str(),path.c_str(),(void*)pair[1],(unsigned long long)GetTickCount64());
 log(SCS_LOG_TYPE_message,"[DeviceConfig] slot=%u known=%d revision=%llu style=%s layout=%s output=%u",slot,d.known,(unsigned long long)d.revision,navigation_style_name(d.style),d.horizontal?"horizontal":"vertical",unsigned(*(uint16_t*)(pair[0]+0x160)));
 return true;
}
// Integrated GPS/cluster UI uses owner+0xc0/0xc8 and separate layers+0x120/0x128.
// This is the native render ABI (538d40), rather than the accessory pair ABI.
inline void managed_native_builtin(void* owner){
 if(!managedNativeApi.resolveModel||!nativeFork.base||!map_memory(uintptr_t(owner),0x1598))return;const auto base=uintptr_t(owner);const auto vehicle=managed_vehicle();
 for(unsigned type=0;type<2;++type){const auto layerSlot=base+0x120+type*8,windowSlot=base+0xc0+type*8;const auto layer=*(uintptr_t*)layerSlot,window=*(uintptr_t*)windowSlot;if(!layer||!window||!map_memory(layer,0x168))continue;std::string path;bool exists=false;{std::lock_guard<std::mutex> l(managedMutex);auto at=managedWindowPaths.find(window);if(at!=managedWindowPaths.end())path=at->second;for(auto& c:managedCandidates)if(c.pair==layerSlot&&c.layer==layer&&c.window==window&&(path.empty()||c.ui==path)){c.seen=GetTickCount64();c.vehicle=vehicle;exists=true;}}if(exists||path.empty())continue;
  const auto token=nativeFork.base+(type?0x2d64458:0x2d64450);if(!managed_native_readable(token,8))continue;const char* alias=*(const char**)token;if(!managed_native_readable(uintptr_t(alias),64))continue;uintptr_t model=0;uint64_t part=0;if(!managedNativeApi.resolveModel(owner,&alias,&model,&part)||!map_memory(model,0x198))continue;
  DeviceCandidate c;c.owner=base;c.pair=layerSlot;c.windowSlot=windowSlot;c.layer=layer;c.window=window;c.model=model;c.width=*(unsigned*)(layer+8);c.height=*(unsigned*)(layer+12);if(c.width<4||c.width>1024||c.height<4||c.height>1024)continue;c.ui=path;c.alias=alias;c.vehicle=vehicle;c.name=type?"Infotainment":"Instrument display";c.seen=GetTickCount64();for(size_t i=0;i<gpsDisplays.size();++i)if(gpsDisplays[i].ready&&gpsDisplays[i].ui==path){c.known=int(i);c.name=gpsDisplays[i].name;break;}
  {std::lock_guard<std::mutex> l(managedMutex);auto at=std::find_if(managedCandidates.begin(),managedCandidates.end(),[&](auto& other){return other.pair==c.pair;});if(at==managedCandidates.end()){if(managedCandidates.size()<managedCandidateLimit)managedCandidates.push_back(c);}else *at=c;}++managedRevision;
 }
}
inline void managed_native_frame(void* owner){
 if(managedBase.empty()||managedNativeBusy)return;managed_native_builtin(owner);navplus_temperature_sample(owner);t680_gauge_sample(owner);managedAnyActive=false;++managedNativeFrameSerial;const auto vehicle=managed_vehicle();const auto now=GetTickCount64();cameraNativeProbe.sample(owner,vehicle,vehicleStart.epoch.load(),now);cameraNativeBackend.sample(owner,vehicle,now);managed_native_defaults(owner,now);
 const int preview=managedScan.load()?managedCandidate.load():-1;
 if(managedPreview.candidate>=0&&(managedPreview.candidate!=preview||managedPreview.owner!=uintptr_t(owner)||managedPreview.vehicle!=vehicle))managed_native_restore(managedPreview,true);
 if(preview>=0&&managedPreview.candidate!=preview){auto c=managed_native_candidate(preview);if(c.owner==uintptr_t(owner)&&c.known<0&&managed_native_pair(c)){ManagedDevice orange;orange.horizontal=c.width>=c.height;managed_native_create(0,orange,preview,managedPreview,true);}}
 for(unsigned i=0;i<managedDeviceCount;++i){auto d=managed_snapshot(i);auto& state=managedNative[i];bool active=d.used&&d.enabled&&displayGlobalEnabled&&d.vehicle==vehicle&&(d.known<0||display_active(size_t(d.known)));
  // The Ford uses the original page tree and dedicated map/overlay markers.
  // Its profile must not depend on accessory DashboardCreate or scanner slots.
  if(active&&d.known>=0&&size_t(d.known)<gpsDisplays.size()&&ford_console_id(gpsDisplays[d.known].id)){
   managedDeviceActive[i]=true;managedAnyActive=true;managedAppliedStyle[i]=d.style;
   if(managedTemplateReady[i]==d.revision&&managedOverlayReady[i]==d.revision){if(managedAppliedRevision[i]!=d.revision)log(SCS_LOG_TYPE_message,"[DeviceApply] slot=%u known=%d revision=%llu path=ford-native style=%d layout=%s skin=%d startup=%d theme=%d",i,d.known,(unsigned long long)d.revision,d.style,d.horizontal?"horizontal":"vertical",d.skin,d.startup,d.theme);managedAppliedStartup[i]=d.startup;managedAppliedRevision[i]=d.revision;}
   nativeForkLastSurfaceUse=now;nativeForkHorizontalLastUse=now;continue;
  }
  if(active&&d.known>=0&&size_t(d.known)<gpsDisplays.size()&&gpsDisplays[d.known].independent){
   // The physical phone surface exists already. Only its native map producer
   // needs activation; CPU overlay/startup is composed directly at Present.
   managedDeviceActive[i]=true;managedAnyActive=true;managedAppliedStyle[i]=d.style;
   if(managedOverlayReady[i]==d.revision){if(managedAppliedRevision[i]!=d.revision)log(SCS_LOG_TYPE_message,"[DeviceApply] slot=%u known=%d revision=%llu path=phone-front style=%d layout=%s theme=%d startup=%d",i,d.known,(unsigned long long)d.revision,d.style,d.horizontal?"horizontal":"vertical",d.theme,d.startup);managedAppliedStartup[i]=d.startup;managedAppliedRevision[i]=d.revision;}
   nativeForkLastSurfaceUse=now;continue;
  }
  const int selected=active?managed_native_find(d):-1;if(selected<0)active=false;
  if(!active){managedDeviceActive[i]=false;if(state.applied)managed_native_restore(state,true);continue;}
  auto c=managed_native_candidate(selected);if(c.owner!=uintptr_t(owner))continue;
  if(d.known<0&&c.width>=4&&c.height>=4&&(d.nativeWidth!=c.width||d.nativeHeight!=c.height)){
   std::lock_guard<std::mutex> l(managedMutex);auto& current=managedDevices[i];current.nativeWidth=c.width;current.nativeHeight=c.height;++current.revision;d=current;managedDirty=true;++managedRevision;
  }
  managedDeviceActive[i]=false;
  if(managedTemplateReady[i].load()!=d.revision||managedOverlayReady[i].load()!=d.revision)continue;
  if(d.known>=0&&size_t(d.known)<gpsDisplays.size()&&ford_console_id(gpsDisplays[d.known].id)){if(!managedAppliedRevision[i]||managedAppliedStartup[i].load()!=d.startup){uint64_t origin=now;if(!managedAppliedRevision[i]&&managedAppliedStartup[i].load()==d.startup&&d.known>=0&&size_t(d.known)<gpsDisplays.size()&&gpsDisplays[d.known].independent){auto phase=startup_elapsed(now);if(phase>=0&&uint64_t(phase)<=now)origin=now-uint64_t(phase);}managedActivationTime[i]=origin;}managedAppliedStartup[i]=d.startup;managedAnyActive=true;managedDeviceActive[i]=true;managedAppliedStyle[i]=d.style;managedAppliedRevision[i]=d.revision;nativeForkLastSurfaceUse=now;nativeForkHorizontalLastUse=now;continue;}
  if(preview>=0&&managedPreview.candidate==selected)continue;
  const auto visual=managed_page_visual(i);const bool cameraLayout=d.style==3&&managed_is_copilot(d)&&d.vehicle.find("t680_2022")!=std::string::npos&&(copilot_camera_live(visual)||(visual.transition<0&&visual.fromPage==DevicePage::Camera));
  const bool nativeRear=cameraLayout&&bettergps_camera::cameras.snapshot().selected==bettergps_camera::View::Reverse;
  if(state.applied!=d.revision||state.cameraLayout!=cameraLayout||state.cameraRearLayout!=nativeRear){if(state.failedRevision==d.revision&&now<state.retryAfter)continue;if(state.applied)managed_native_restore(state,true);if(!managed_native_create(i,d,selected,state,false,cameraLayout,nativeRear)){state.failedRevision=d.revision;state.retryAfter=now+1000;std::lock_guard<std::mutex> l(managedMutex);managedError="Native screen replacement failed; original display retained.";continue;}}
  if(!managedAppliedRevision[i]||managedAppliedStartup[i].load()!=d.startup){uint64_t origin=now;if(!managedAppliedRevision[i]&&managedAppliedStartup[i].load()==d.startup&&d.known>=0&&size_t(d.known)<gpsDisplays.size()&&gpsDisplays[d.known].independent){auto phase=startup_elapsed(now);if(phase>=0&&uint64_t(phase)<=now)origin=now-uint64_t(phase);}managedActivationTime[i]=origin;}managedAppliedStartup[i]=d.startup;managedAnyActive=true;managedDeviceActive[i]=true;managedAppliedStyle[i]=d.style;managedAppliedRevision[i]=d.revision;nativeForkLastSurfaceUse=now;nativeForkHorizontalLastUse=now;
 }
 for(auto& active:managedDeviceActive)if(active.load())managedAnyActive=true;
}
inline void managed_native_export(void* dependencies){if(managedNativeBusy||!dependencies)return;
 // Only independent phone producers need manual submission. Replaced
 // accessory UIs (including Nuvi) are already queued by ATS through their
 // owner. Exporting one here can make its output depend on itself and abort
 // the render graph at ignition with image index UINT32_MAX.
 for(unsigned i=0;i<managedDeviceCount;++i){auto& state=managedNative[i];if(!state.applied||!state.phone||!managed_power_target(i)||!state.created[0]||!state.created[1])continue;
  if(state.queuedFrame!=managedNativeFrameSerial){state.queuedFrame=managedNativeFrameSerial;const char* label="BetterGPS managed phone";uint32_t key=0x7fffffff;managedNativeBusy=true;nativeForkOriginalQueue(&key,(void*)state.created[1],(void*)state.created[0],&label,nullptr,0,dependencies);managedNativeBusy=false;state.key=key;}
  if(!state.reportedQueue){log(SCS_LOG_TYPE_message,"[DeviceQueue] slot=%u output=%u key=%u",i,unsigned(*(uint16_t*)(state.created[0]+0x160)),state.key);state.reportedQueue=true;}
  if(state.key==0x7fffffff)continue;auto* table=(uintptr_t*)dependencies;const auto handle=*(uint16_t*)(state.created[0]+0x160);bool present=false;for(size_t n=0;n<table[2];++n)if(*(uint16_t*)(table[1]+n*16)==handle)present=true;
  if(!present&&nativeFork.append(dependencies,1)){auto* entry=(uint8_t*)(table[1]+(table[2]-1)*16);nativeFork.handle(entry,handle);*(uint32_t*)(entry+4)=state.key;}
 }
}
// Accessory render jobs may retain the ORIGINAL pair copied before the owner
// field was replaced. Redirect only that exact retained Nuvi pair, while the
// current owner still contains our pair. No material/size/shared-map guessing.
inline bool managed_native_route_queue(void*& window,void*& layer){
 if(managedNativeBusy||!render_gpu_allowed()||!displayGlobalEnabled)return false;const auto vehicle=managed_vehicle();
 for(unsigned i=0;i<managedDeviceCount;++i){auto& s=managedNative[i];if(!s.applied||!s.replaced||!managedDeviceActive[i]||s.vehicle!=vehicle||!s.created[0]||!s.created[1])continue;
  const bool cached=s.held[0]==uintptr_t(layer)&&s.held[1]==uintptr_t(window),owned=s.created[0]==uintptr_t(layer)&&s.created[1]==uintptr_t(window);if(!cached&&!owned)continue;
  const auto d=managed_snapshot(i);if(!d.used||!d.enabled||d.known>=0||d.vehicle!=vehicle||d.ui!="/ui/dashboard/gps.dlc_toys.sii"||d.alias!="/vehicle/truck/upgrade/interior_decors/toyglass/gps_ui.tobj")continue;
  if(!map_memory(s.pair,8)||!map_memory(managed_state_window_slot(s),8)||*(uintptr_t*)s.pair!=s.created[0]||*(uintptr_t*)managed_state_window_slot(s)!=s.created[1])continue;
  if(owned){if(!s.reportedProducer){s.reportedProducer=true;log(SCS_LOG_TYPE_message,"[DeviceRoute] slot=%u Nuvi owned UI job submitted",i);}return false;}
  window=(void*)s.created[1];layer=(void*)s.created[0];if(!s.reportedRoute){s.reportedRoute=true;log(SCS_LOG_TYPE_message,"[DeviceRoute] slot=%u exact cached original Nuvi job redirected to current owned UI",i);}return true;
 }return false;
}
inline bool managed_native_wake(void* window){
 if(volvo25ClusterQueued&&displayIgnition==1&&displayGlobalEnabled&&managed_vehicle()=="volvo.vnl2025"){std::unique_lock<std::mutex> lock(managedMutex,std::try_to_lock);if(lock){const auto at=managedWindowPaths.find(uintptr_t(window));if(at!=managedWindowPaths.end()&&at->second=="/ui/dashboard/volvo_vnl2025.sii")return true;}}
 if(icucClusterQueued&&displayIgnition==1&&displayGlobalEnabled&&icuc_cluster_vehicle(managed_vehicle())){std::unique_lock<std::mutex> lock(managedMutex,std::try_to_lock);if(lock){const auto at=managedWindowPaths.find(uintptr_t(window));if(at!=managedWindowPaths.end()&&at->second==icuc_cluster_ui(managed_vehicle()))return true;}}
 if(icc5ClusterQueued&&displayIgnition==1&&displayGlobalEnabled&&managed_icc5_vehicle(managed_vehicle())){std::unique_lock<std::mutex> lock(managedMutex,std::try_to_lock);if(lock){const auto at=managedWindowPaths.find(uintptr_t(window));if(at!=managedWindowPaths.end()&&at->second==managed_icc5_cluster_ui(managed_vehicle()))return true;}}
 if(volvo18ClusterQueued&&displayIgnition==1&&displayGlobalEnabled&&managed_volvo18_vehicle(managed_vehicle())){std::unique_lock<std::mutex> lock(managedMutex,std::try_to_lock);if(lock){const auto at=managedWindowPaths.find(uintptr_t(window));if(at!=managedWindowPaths.end()&&at->second==(managed_vehicle()=="volvo.vnl2018"?"/ui/dashboard/volvo_vnl2018.sii":"/ui/dashboard/volvo_vnr_e.sii"))return true;}}
 if(vnlClusterQueued&&displayIgnition==1&&displayGlobalEnabled&&managed_vehicle()=="volvo.vnl"){std::unique_lock<std::mutex> lock(managedMutex,std::try_to_lock);if(lock){const auto at=managedWindowPaths.find(uintptr_t(window));if(at!=managedWindowPaths.end()&&at->second=="/ui/dashboard/volvo_vnl.sii")return true;}}
 if(mackClusterQueued&&displayIgnition==1&&displayGlobalEnabled&&mack_cluster_vehicle(managed_vehicle())){
  std::unique_lock<std::mutex> lock(managedMutex,std::try_to_lock);if(lock){const auto at=managedWindowPaths.find(uintptr_t(window));if(at!=managedWindowPaths.end()&&(at->second=="/ui/dashboard/mack_anthem.sii"||at->second=="/ui/dashboard/mack_pinnacle.sii"))return true;}
 }
 if(clusterIntroQueued&&displayIgnition==1&&displayGlobalEnabled&&managed_vehicle()=="kenworth.t680_2022"){
  std::unique_lock<std::mutex> lock(managedMutex,std::try_to_lock);if(lock){const auto at=managedWindowPaths.find(uintptr_t(window));if(at!=managedWindowPaths.end()&&at->second=="/ui/dashboard/kenworth_t680_2022.sii")return true;}
 }
 if(managedPreview.created[1]==uintptr_t(window))return true;for(unsigned i=0;i<managedDeviceCount;++i){auto& s=managedNative[i];if(s.applied&&s.created[1]==uintptr_t(window)&&(s.phone&&managed_power_target(i)||managed_visible_splash(managed_snapshot(i))))return true;}return false;
}
// ATS tears down its UI manager before it invokes telemetry shutdown. Keeping
// an addon_ui alive until plugin shutdown prevents that manager from exiting.
// Release our native windows when gameplay is suspended, while it is alive;
// completed GPU snapshots remain available to the paused cabin.
inline void managed_native_suspend(){cameraNativeBackend.release(true);managed_power_pause(GetTickCount64());managed_native_restore(managedPreview,true);for(auto& s:managedNative)managed_native_restore(s,true);managedAnyActive=false;for(auto& active:managedDeviceActive)active=false;}
inline void managed_native_reset(){cameraNativeBackend.release(false);bettergps_camera::cameras.sample(false,false,false,false);managed_native_restore(managedPreview,false);for(auto& s:managedNative)managed_native_restore(s,false);{std::lock_guard<std::mutex> l(managedMutex);managedWindowPaths.clear();}}
inline void managed_native_install(){managedNativeUiPath=&managed_native_ui_path;managedNativeObserve=&managed_native_observe;managedNativeFrame=&managed_native_frame;managedNativeSeen=&managed_native_seen;managedNativeExport=&managed_native_export;managedNativeWake=&managed_native_wake;managedNativeRouteQueue=&managed_native_route_queue;managedNativeReset=&managed_native_reset;managed_bind_cameras();}
}
