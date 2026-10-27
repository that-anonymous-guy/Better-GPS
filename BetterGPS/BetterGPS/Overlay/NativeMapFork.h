#pragma once
#include "LiveMapTheme.h"
#include "DisplayAssignments.h"
#include "MapViewSettings.h"
#include "DeviceManagerState.h"
#include "DisplayPowerSettings.h"
#include "NativePlayerMaterial.h"
namespace bettergps_overlay {
// Native ATS offscreen UI path, with an independently owned window, ui_map,
// render layer and texture alias. No map postprocessing or source replacement.
struct NativeMapForkApi {
 uintptr_t base=0;bool supported=false;using World=void*(__fastcall*)();World world{};
 using Setup=void(__fastcall*)(void*);using Update=void(__fastcall*)(void*,bool);
 using BuildGeometry=void(__fastcall*)(void*,void*);
 using Render=void(__fastcall*)(void*,bool);
 using DashboardCreate=bool(__fastcall*)(void*,void*,const char**,const char**,const unsigned*,void*);
 using Rotation=float*(__fastcall*)(const float*,float*);
 using Unpack=float*(__fastcall*)(float*,const void*);
 using RefreshPosition=void(__fastcall*)(void*,bool);
 // Stack slots: arg5 suspension sentinel, arg6 writable token slot, arg7 dependencies.
 using Queue=uint32_t*(__fastcall*)(uint32_t*,void*,void*,const char**,const float*,uintptr_t,void*);
 using LayerCreate=void*(__fastcall*)(void*,uintptr_t,uintptr_t,uint8_t,unsigned,unsigned,unsigned,const float*);
 using LayerCall=void(__fastcall*)(void*);using Append=bool(__fastcall*)(void*,size_t);
 using RegisterDynamic=void(__fastcall*)(void*,const char**);
 using Alias=uint16_t*(__fastcall*)(void*,uint16_t*,const char**);
 using Handle=void(__fastcall*)(void*,uint16_t);
 using Position=void(__fastcall*)(void*,const float*,bool,const float*,uintptr_t);
 using View=void(__fastcall*)(void*,unsigned,float,float,float,float,uint64_t,uint64_t,uint64_t);
 using UiPath=void(__fastcall*)(void*,const char**);UiPath uiPath{};
 Unpack unpack{};RefreshPosition refreshPosition{};DashboardCreate dashboardCreate{};Rotation rotation{};Render render{};BuildGeometry buildGeometry{};RegisterDynamic registerDynamic{};Append append{};Setup setup{};Update update{};Queue queue{};LayerCreate layerCreate{};LayerCall layerDestroy{},layerUpdate{},freeUnit{};Alias alias{};Handle handle{};Position position{};View view{};
};
inline NativeMapForkApi nativeFork;
// Like the map aliases, phone output aliases must exist before the engine
// first resolves their resources. Registering one after a dashboard was loaded
// makes ATS try to stream it as an ordinary TOBJ instead of a render target.
inline std::array<uint16_t,managedDeviceCount> managedOutputAliases=[](){std::array<uint16_t,managedDeviceCount> a{};a.fill(0x3fff);return a;}();
inline const char* managed_output_path(unsigned slot){static const auto paths=[](){std::array<std::string,managedDeviceCount> a;for(unsigned i=0;i<a.size();++i)a[i]="/material/ui/dashboard/bettergps_device_output_"+std::to_string(i)+".tobj";return a;}();return paths[slot].c_str();}
inline NativeMapForkApi::UiPath nativeForkOriginalUiPath=nullptr;

inline NativeMapForkApi::DashboardCreate nativeForkOriginalDashboardCreate=nullptr;
struct NativePhoneJob {uintptr_t owner=0,pair=0,layer=0;size_t source=0;};
inline std::mutex nativePhoneJobsMutex;
inline std::map<uintptr_t,NativePhoneJob> nativePhoneJobs;
inline std::atomic<unsigned> nativePhoneJobCount{0};
inline std::atomic<uint64_t> nativePhoneWakeRequests{0};
inline NativeMapForkApi::Render nativeForkOriginalRender=nullptr;
inline thread_local bool nativeForkDashboardScope=false;
inline NativeMapForkApi::Update nativeForkOriginalUpdate=nullptr;
inline NativeMapForkApi::BuildGeometry nativeForkOriginalBuild=nullptr;
inline NativeMapForkApi::Queue nativeForkOriginalQueue=nullptr;
inline NativeMapForkApi::Position nativeForkOriginalPosition=nullptr;
inline NativeMapForkApi::View nativeForkOriginalView=nullptr;
inline std::atomic<uint64_t> nativeForkLastSurfaceUse{0},nativeForkHorizontalLastUse{0};
inline std::atomic<int> nativeForkStatus{0};
inline std::atomic<bool> nativeForkInstalled{false},nativeForkPublished{false};
inline thread_local bool nativeForkInternal=false;
inline void __fastcall native_fork_ui_path(void* window,const char** path){nativeForkOriginalUiPath(window,path);if(!nativeForkInternal&&managedNativeUiPath&&path&&*path)managedNativeUiPath(window,*path);}
inline uintptr_t nativeForkSource=0,nativeForkSourceWindow=0,nativeForkSourceMap=0,nativeForkSourceLayer=0;
// One native producer per physical device family. Scope is only used while
// setting up/queuing a producer; deferred geometry uses the ui_map identity.
struct NativeMapDevice {
 alignas(16) std::array<uint8_t,0x1598> state{};
 alignas(16) std::array<uint8_t,0x168> layer{};
 bool layerCreated=false,releasing=false,keyValid=false,drawDark=false;
 uint32_t renderKey=0x7fffffff;uint16_t aliasHandle=0x3fff;
 uint64_t queued=0,retryAfter=0,viewRevision=~uint64_t(0);size_t drawStyle=~size_t(0);
};
inline std::array<NativeMapDevice,2+managedDeviceCount> nativeMapDevices;
inline thread_local unsigned nativeMapDevice=0;
struct NativeMapDeviceScope {unsigned previous;explicit NativeMapDeviceScope(unsigned d):previous(nativeMapDevice){nativeMapDevice=d;}~NativeMapDeviceScope(){nativeMapDevice=previous;}};
inline NativeMapDevice& native_map_device(){return nativeMapDevices[nativeMapDevice];}
inline std::atomic<uint64_t> nativeForkSourceGeneration{1};
inline std::atomic<unsigned> nativeForkQueuedThemeId{0};
struct NativeMapTheme {size_t style=0;bool dark=false;bool operator==(const NativeMapTheme& other)const{return style==other.style&&dark==other.dark;}unsigned id()const{return unsigned(style)*2+unsigned(dark);}};
inline std::mutex nativeForkThemesMutex;
inline std::map<uintptr_t,NativeMapTheme> nativeForkThemes;
inline bool native_fork_theme(uintptr_t map,NativeMapTheme& theme){std::lock_guard<std::mutex> lock(nativeForkThemesMutex);auto found=nativeForkThemes.find(map);if(found==nativeForkThemes.end())return false;theme=found->second;return true;}
inline void __fastcall native_fork_build_geometry(void* map,void* draw){
 std::lock_guard<std::recursive_mutex> serialized(liveMapGeometryMutex);NativeMapTheme theme;
 if(!native_fork_theme(uintptr_t(map),theme)){nativeForkOriginalBuild(map,draw);return;}
 // Queueing does not build the map. Keep its captured palette active for the
 // actual geometry callback, including jobs executed after a theme switch.
 NativePlayerMaterialScope playerMaterial(theme.style,theme.dark);PrivateMapPaletteScope palette(uintptr_t(map),theme.dark,theme.style,false);
 if(palette.applied)nativeForkOriginalBuild(map,draw);
}
inline uintptr_t& fork_word(size_t at){return *reinterpret_cast<uintptr_t*>(native_map_device().state.data()+at);}
inline int native_fork_device_for_map(void* map){for(unsigned d=0;d<nativeMapDevices.size();++d)if(uintptr_t(map)&&*(uintptr_t*)(nativeMapDevices[d].state.data()+0x1580)==uintptr_t(map))return int(d);return -1;}
inline const char* native_fork_map_path(){if(nativeMapDevice>=2){static const auto paths=[](){std::array<std::string,managedDeviceCount> p;for(unsigned i=0;i<p.size();++i)p[i]="/material/ui/dashboard/bettergps_map_device_"+std::to_string(i)+".tobj";return p;}();return paths[nativeMapDevice-2].c_str();}return nativeMapDevice?"/material/ui/dashboard/bettergps_map_f150.tobj":"/material/ui/dashboard/bettergps_map.tobj";}

inline bool native_fork_wanted(){return nativeForkLastSurfaceUse.load()!=0&&GetTickCount64()-nativeForkLastSurfaceUse.load()<500&&displayGlobalEnabled.load()&&(display_power_target()||managed_any_power()||(nativeForkHorizontalLastUse.load()!=0&&GetTickCount64()-nativeForkHorizontalLastUse.load()<500))&&(!displayPolicyReady.load()||(displayEnabledMask.load()&displaySeenMask.load())!=0||managedAnyActive.load());}
inline bool native_fork_resolve(const MapImage& image){
 nativeFork={};struct Check {size_t rva;std::array<uint8_t,16> bytes;};static const Check checks[]={
    {0xffd690,{0x48,0x83,0xec,0x18,0x80,0x3d,0x0f,0x45,0x05,0x02,0x00,0x0f,0xbf,0x42,0x0c,0xf3}},
    {0x10030a0,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x84,0xd2,0x74,0x13,0x48,0x8b,0x01}},
    {0x54b150,{0x40,0x55,0x56,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0xd8,0x00,0x00,0x00,0x49,0x8b}},
    {0x165240,{0x48,0x83,0xec,0x68,0xf3,0x0f,0x10,0x11,0x33,0xc0,0x0f,0x29,0x74,0x24,0x50,0x0f}},
    {0x538d40,{0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57}},
    {0x2ee0b0,{0x48,0x89,0x4c,0x24,0x08,0x56,0x48,0x83,0xec,0x20,0x48,0x8b,0x0a,0x48,0xc7,0xc2}},
    {0x32a320,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xfa,0x48,0x8b,0xd9}},
    {0x3f3870,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x1d,0x13,0x75,0x2c,0x03,0x48,0x85,0xdb}},
    {0x54b490,{0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xd9}},
    {0x54b8f0,{0x40,0x55,0x56,0x48,0x81,0xec,0x08,0x01,0x00,0x00,0x48,0x89,0x9c,0x24,0x20,0x01}},
    {0x538850,{0x40,0x55,0x53,0x57,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24,0x30,0xf3,0xff,0xff}},
    {0x15688d0,{0x40,0x53,0x48,0x83,0xec,0x20,0x66,0xc7,0x41,0x01,0x00,0x00,0x4c,0x8d,0x05,0x95}},
    {0x1568ac0,{0x40,0x53,0x48,0x83,0xec,0x40,0x8b,0x81,0x4c,0x01,0x00,0x00,0x48,0x8b,0xd9,0x0f}},
    {0x1569440,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x30,0x48}},
    {0x2ecc10,{0x44,0x89,0x4c,0x24,0x20,0x55,0x56,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x38,0x4c}},
    {0x1ee710,{0x48,0x89,0x5c,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x0f,0xb7,0xda,0x48,0x8b,0xf9}},
    {0xfd290,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48}},
    {0x10029b0,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18,0x4c}},
    {0x100d840,{0x48,0x8b,0xc4,0x55,0x56,0x41,0x56,0x48,0x8d,0xa8,0xf8,0xf7,0xff,0xff,0x48,0x81}},
    {0x1002500,{0x40,0x53,0x48,0x83,0xec,0x40,0x48,0x8b,0x01,0x48,0x8b,0xd9,0x0f,0x29,0x74,0x24}},
 };
 for(auto& check:checks)if(!image.contains(uintptr_t(image.base)+check.rva,16)||!image.executable(uintptr_t(image.base)+check.rva)||memcmp(image.base+check.rva,check.bytes.data(),16)!=0)return false;
 auto base=uintptr_t(image.base);if(!image.text(base+0x223ea60,"/material/ui/dashboard/bare_map.tobj")||!image.contains(base+0x368dee0,8))return false;
 static const uint8_t uiPrefix[]={0x40,0x53,0x56,0x41,0x56,0x48,0x83,0xec,0x20};if(!image.contains(base+0x5333c0,sizeof(uiPrefix))||memcmp(image.base+0x5333c0,uiPrefix,sizeof(uiPrefix)))return false;nativeFork.uiPath=(NativeMapForkApi::UiPath)(base+0x5333c0);nativeFork.unpack=(NativeMapForkApi::Unpack)(base+0xffd690);nativeFork.refreshPosition=(NativeMapForkApi::RefreshPosition)(base+0x10030a0);nativeFork.dashboardCreate=(NativeMapForkApi::DashboardCreate)(base+0x54b150);nativeFork.rotation=(NativeMapForkApi::Rotation)(base+0x165240);nativeFork.render=(NativeMapForkApi::Render)(base+0x538d40);nativeFork.buildGeometry=(NativeMapForkApi::BuildGeometry)(base+0x100d840);nativeFork.registerDynamic=(NativeMapForkApi::RegisterDynamic)(base+0x2ee0b0);nativeFork.append=(NativeMapForkApi::Append)(base+0x32a320);nativeFork.base=base;nativeFork.supported=true;nativeFork.world=(NativeMapForkApi::World)(base+0x3f3870);nativeFork.setup=(NativeMapForkApi::Setup)(base+0x54b490);nativeFork.update=(NativeMapForkApi::Update)(base+0x54b8f0);nativeFork.queue=(NativeMapForkApi::Queue)(base+0x538850);nativeFork.layerCreate=(NativeMapForkApi::LayerCreate)(base+0x15688d0);nativeFork.layerDestroy=(NativeMapForkApi::LayerCall)(base+0x1568ac0);nativeFork.layerUpdate=(NativeMapForkApi::LayerCall)(base+0x1569440);nativeFork.alias=(NativeMapForkApi::Alias)(base+0x2ecc10);nativeFork.handle=(NativeMapForkApi::Handle)(base+0x1ee710);nativeFork.freeUnit=(NativeMapForkApi::LayerCall)(base+0xfd290);nativeFork.position=(NativeMapForkApi::Position)(base+0x10029b0);nativeFork.view=(NativeMapForkApi::View)(base+0x1002500);return true;
}
// Must run before any dashboard material is loaded. Material compilation
// resolves dynamic aliases once; a late alias leaves the DDS placeholder bound.
inline bool native_fork_alias_register_current(){
 if((native_map_device().aliasHandle&0x3fff)!=0x3fff)return true;
 if(!nativeFork.registerDynamic||!nativeFork.alias||!nativeFork.handle||!map_memory(nativeFork.base+0x368dee0,8))return false;
 auto* manager=*(void**)(nativeFork.base+0x368dee0);if(!manager)return false;
 const char* path=native_fork_map_path();uint16_t id=0x3fff;nativeFork.registerDynamic(manager,&path);nativeFork.alias(manager,&id,&path);if((id&0x3fff)==0x3fff||!(id&0x8000))return false;
 nativeFork.handle(&native_map_device().aliasHandle,id);return true;
}
inline bool native_fork_alias_register(){for(unsigned d=0;d<nativeMapDevices.size();++d){if(d>=2&&managedBase.empty())continue;NativeMapDeviceScope scope(d);if(!native_fork_alias_register_current())return false;}if(!managedBase.empty()){auto* manager=*(void**)(nativeFork.base+0x368dee0);for(unsigned i=0;i<managedDeviceCount;++i){const char* path=managed_output_path(i);nativeFork.registerDynamic(manager,&path);}}return true;}
// Declaration must precede material loading, but resolving a new TOBJ also
// initializes its resource. Do that only after the plugin's VFS is mounted.
inline bool native_fork_output_alias_register(){if(managedBase.empty())return true;if(!nativeFork.supported||!nativeFork.alias||!map_memory(nativeFork.base+0x368dee0,8))return false;auto* manager=*(void**)(nativeFork.base+0x368dee0);if(!manager)return false;for(unsigned i=0;i<managedDeviceCount;++i){if((managedOutputAliases[i]&0x3fff)!=0x3fff)continue;const char* path=managed_output_path(i);uint16_t handle=0x3fff;nativeFork.alias(manager,&handle,&path);if((handle&0x3fff)==0x3fff||!(handle&0x8000))return false;nativeFork.handle(&managedOutputAliases[i],handle);}return true;}
inline void native_fork_alias_release(){for(unsigned d=0;d<nativeMapDevices.size();++d){NativeMapDeviceScope scope(d);if(nativeFork.handle&&(native_map_device().aliasHandle&0x3fff)!=0x3fff)nativeFork.handle(&native_map_device().aliasHandle,0x3fff);native_map_device().aliasHandle=0x3fff;}for(auto& handle:managedOutputAliases){if(nativeFork.handle&&(handle&0x3fff)!=0x3fff)nativeFork.handle(&handle,0x3fff);handle=0x3fff;}}
inline void native_fork_unref(uintptr_t object,bool release){if(!object)return;if(release){auto** table=*reinterpret_cast<void***>(object);reinterpret_cast<void(__fastcall*)(void*)>(table[1])((void*)object);}auto old=InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(object+8),-1);if(((old-1)&0x1ffffff)==0)nativeFork.freeUnit((void*)object);}
inline void native_fork_release(){if(native_map_device().releasing)return;native_map_device().releasing=true;++nativeForkSourceGeneration;nativeForkPublished=false;native_map_device().keyValid=false;nativeForkStatus=0;auto map=fork_word(0x1580),window=fork_word(0x1578);fork_word(0x1580)=fork_word(0x1578)=0;if(map)native_fork_unref(map,false);if(window)native_fork_unref(window,true);if(native_map_device().layerCreated){nativeFork.layerDestroy(native_map_device().layer.data());native_map_device().layerCreated=false;}native_map_device().state.fill(0);native_map_device().layer.fill(0);native_map_device().queued=0;native_map_device().viewRevision=~uint64_t(0);native_map_device().releasing=false;}
inline void native_fork_release_all(){map_view_reset_modes();for(unsigned d=0;d<nativeMapDevices.size();++d){NativeMapDeviceScope scope(d);native_fork_release();}}
inline void native_fork_forget(void* object){{std::lock_guard<std::mutex> lock(nativeForkThemesMutex);nativeForkThemes.erase(uintptr_t(object));}if(native_map_device().releasing)return;if(uintptr_t(object)==nativeForkSourceMap){native_fork_release_all();nativeForkSource=nativeForkSourceWindow=nativeForkSourceMap=nativeForkSourceLayer=0;}}
inline void native_fork_observe(void* object){
 auto source=uintptr_t(object);if(!map_memory(source,0x1598))return;
 auto window=*(uintptr_t*)(source+0x1578),map=*(uintptr_t*)(source+0x1580),layer=*(uintptr_t*)(source+0x1588);unsigned mode=*(unsigned*)(source+0x12a4);
 if(mode>3)return;
 if(nativeForkSource&&(nativeForkSource!=source||nativeForkSourceMap!=map))native_fork_release_all();
 nativeForkSource=source;nativeForkSourceWindow=window;nativeForkSourceMap=map;nativeForkSourceLayer=layer;map_view_observe_mode(mode);if(!nativeForkPublished)nativeForkStatus=1;
 auto world=uintptr_t(nativeFork.world());if(map_memory(world,0xa0)){auto unit=*(uintptr_t*)(world+0x98);if(map_memory(unit,liveMapLayout.bytes)&&*(uintptr_t*)unit==liveMapLayout.vtable){std::lock_guard<std::mutex> lock(liveMapMutex);if(std::find(liveMapUnits.begin(),liveMapUnits.end(),unit)==liveMapUnits.end()){auto at=std::find(liveMapUnits.begin(),liveMapUnits.end(),uintptr_t(0));if(at!=liveMapUnits.end()){*at=unit;++liveMapGeneration;}}}}
}
inline void __fastcall native_fork_update(void* object,bool force){nativeForkOriginalUpdate(object,force);if(!nativeForkInternal)native_fork_observe(object);}
inline void __fastcall native_fork_view(void* object,unsigned mode,float height,float pitch,float player,float road,uint64_t icons,uint64_t city,uint64_t company){
 const int device=native_fork_device_for_map(object);
 if(device>=0&&mode<4){
  height=float(height/map_view_snapshot_for(unsigned(device),mode).zoom);
  // Native dashboard view modes 0/1 are 3D. Scale the mesh, not its DDS,
  // the map camera or other markers. Use this producer's captured choice.
  NativeMapTheme theme;
  if(mode<2&&native_fork_theme(uintptr_t(object),theme)&&native_player_is_truck(theme.style))player*=1.33f;
 }
 nativeForkOriginalView(object,mode,height,pitch,player,road,icons,city,company);
}
inline void __fastcall native_fork_position(void* object,const float* placement,bool follow,const float* center,uintptr_t extra){
 nativeForkOriginalPosition(object,placement,follow,center,extra);
 const int device=native_fork_device_for_map(object);if(device<0||!placement)return;
 // The named Bronco fallback is beyond managed slots and has no pan context.
 const auto settings=map_view_snapshot_for(unsigned(device),map_view_mode_for(unsigned(device)));const bool pan=unsigned(device)<mapPan.enabled.size()&&mapPan.enabled[unsigned(device)].load();if(settings.x==0&&settings.z==0&&!pan)return;
 // ATS packs sector coordinates in the placement and converts them into
 // map space. Never feed a fabricated center back through that conversion.
 // Translate the final native camera, leaving center and player data intact.
 auto* camera=(float*)((uintptr_t)object+0x1a8);std::array<float,3> target{};
 nativeFork.unpack(target.data(),(const void*)((uintptr_t)object+0x1b4));
 double bx=camera[0]-target[0],bz=camera[2]-target[2];const double back=std::hypot(bx,bz);
 double rx=0,rz=0;
 if(back>1e-4){bx/=back;bz/=back;rx=bz;rz=-bx;}
 else{ // Orthographic/top-down: yaw is held by the native camera itself.
  alignas(16) std::array<float,16> basis{};nativeFork.rotation((const float*)((uintptr_t)object+0x1d0),basis.data());
  rx=basis[0];rz=basis[2];const double right=std::hypot(rx,rz);if(right<1e-6)return;rx/=right;rz/=right;bx=-rz;bz=rx;
 }
 camera[0]+=float(settings.x*rx+settings.z*bx);camera[2]+=float(settings.x*rz+settings.z*bz);
 const bool displaced=pan&&mapPan.frame(unsigned(device),mapViewSourceStamp.load(),GetTickCount64(),target,camera,rx,rz,bx,bz,back);
 if(settings.x!=0||settings.z!=0||displaced)nativeFork.refreshPosition(object,false);

}
inline bool __fastcall native_fork_dashboard_create(void* owner,void* output,const char** ui,const char** alias,const unsigned* dimensions,void* extra){
 const bool ok=nativeForkOriginalDashboardCreate(owner,output,ui,alias,dimensions,extra);
 if(ok&&managedNativeObserve&&ui&&*ui&&alias&&*alias)managedNativeObserve(owner,output,*ui,*alias,dimensions,extra);
 std::lock_guard<std::mutex> lock(nativePhoneJobsMutex);
 for(auto it=nativePhoneJobs.begin();it!=nativePhoneJobs.end();)if(it->second.pair==uintptr_t(output))it=nativePhoneJobs.erase(it);else ++it;
 if(ok&&ui&&*ui&&map_memory(uintptr_t(output),16)){
  for(size_t i=0;i<gpsDisplays.size();++i)if(gpsDisplays[i].ready&&gpsDisplays[i].layout=="vertical"&&gpsDisplays[i].source.empty()&&gpsDisplays[i].ui==*ui){
   auto* pair=(uintptr_t*)output;if(pair[0]&&pair[1]&&nativePhoneJobs.size()<managedCandidateLimit)nativePhoneJobs[pair[1]]={uintptr_t(owner),uintptr_t(output),pair[0],i};break;
  }
 }
 nativePhoneJobCount=unsigned(nativePhoneJobs.size());return ok;
}
inline bool native_fork_phone_consumer(void* window,void* layer){
 if(!nativeForkDashboardScope)return false;
 std::lock_guard<std::mutex> lock(nativePhoneJobsMutex);auto found=nativePhoneJobs.find(uintptr_t(window));if(found==nativePhoneJobs.end())return false;
 const auto& job=found->second;if(job.owner!=nativeForkSource||job.layer!=uintptr_t(layer)||!map_memory(job.pair,16))return false;
 const auto* pair=(const uintptr_t*)job.pair;if(pair[0]!=job.layer||pair[1]!=uintptr_t(window))return false;
 for(size_t i=0;i<gpsDisplays.size();++i)if(display_source_index(i)==job.source&&display_active(i)&&(displaySeenMask.load()&(uint32_t(1)<<i)))return true;
 return false;
}
inline bool native_fork_prepare(){
 if(fork_word(0x1578)&&fork_word(0x1580)&&native_map_device().layerCreated)return true;
 nativeFork.setup(native_map_device().state.data());if(!fork_word(0x1578)||!fork_word(0x1580)){native_fork_release();return false;}
 auto manager=*(void**)(nativeFork.base+0x368dee0);if(!manager){native_fork_release();return false;}
 const float black[]={0,0,0,0};nativeFork.layerCreate(native_map_device().layer.data(),0,0,29,1024,1024,3,black);native_map_device().layerCreated=true;nativeForkStatus=2;
 if((native_map_device().aliasHandle&0x3fff)==0x3fff){native_fork_release();return false;}
 nativeFork.handle(native_map_device().layer.data()+0x160,native_map_device().aliasHandle);nativeFork.layerUpdate(native_map_device().layer.data());fork_word(0x1588)=uintptr_t(native_map_device().layer.data());return true;
}
// The engine exports each layer's render job through the dependency table of
// dashboard consumers. Registering a TOBJ alias alone does not publish a frame.
inline bool native_fork_export_current(void* dependencies){
 if(!native_map_device().keyValid||!dependencies||!native_map_device().layerCreated||!map_memory(uintptr_t(dependencies),32))return false;
 auto* table=(uintptr_t*)dependencies;const auto count=table[2],capacity=table[3];if(count>128||count>capacity||(count&&!map_memory(table[1],count*16)))return false;
 const auto originalHandle=map_memory(nativeForkSourceLayer+0x160,2)?*(uint16_t*)(nativeForkSourceLayer+0x160):uint16_t(0x3fff),privateHandle=*(uint16_t*)(native_map_device().layer.data()+0x160);bool source=false;
 for(size_t i=0;i<count;++i){auto* item=(uint8_t*)(table[1]+i*16);auto handle=*(uint16_t*)item;if(handle==privateHandle){return *(uint32_t*)(item+4)==native_map_device().renderKey;}if(handle==originalHandle)source=true;}
 if((!source&&!nativeForkDashboardScope)||!nativeFork.append(dependencies,1))return false;
 auto* item=(uint8_t*)(table[1]+(table[2]-1)*16);nativeFork.handle(item,privateHandle);*(uint32_t*)(item+4)=native_map_device().renderKey; // append initializes remaining fields, including the invalid secondary handle.
 nativeForkStatus=4;return true;
}
inline bool native_fork_export(void* dependencies){bool any=false;for(unsigned d=0;d<nativeMapDevices.size();++d){NativeMapDeviceScope scope(d);any=native_fork_export_current(dependencies)||any;}return any;}
inline bool native_fork_prepare_frame(){
 if(nativeForkInternal||!render_gpu_allowed()||!native_fork_wanted()||!liveMapReady)return false;
 const auto now=GetTickCount64();native_map_device().queued=now;
 bettergps_diag::Scope timing(bettergps_diag::MapSourcePrepare);nativeForkInternal=true;struct Exit{~Exit(){nativeForkInternal=false;}} exit;
 const NativeMapTheme theme{size_t(managed_native_style(nativeMapDevice)),managed_native_dark(nativeMapDevice,liveMapDark.load())};NativePlayerMaterialScope playerMaterial(theme.style,theme.dark);NativeMapTheme prior;
 // Rebuild only this private UI source on a palette change: no old road or
 // area buffers can survive into the newly selected theme. Old jobs retain
 // their map-to-theme identity until the engine destroys that ui_map.
 if(fork_word(0x1580)&&native_fork_theme(fork_word(0x1580),prior)&&!(prior==theme))native_fork_release();
 if(now<native_map_device().retryAfter)return false;if(!native_fork_prepare()){native_map_device().retryAfter=now+1000;nativeForkStatus=-1;return false;}native_map_device().retryAfter=0;
 const auto revision=mapViewRevision.load();const auto mode=map_view_mode_for(nativeMapDevice);if(*reinterpret_cast<unsigned*>(native_map_device().state.data()+0x12a4)!=mode||native_map_device().viewRevision!=revision)native_map_device().state[0x12a8]=1;
 {std::lock_guard<std::mutex> lock(nativeForkThemesMutex);nativeForkThemes[fork_word(0x1580)]=theme;}
 if(native_map_device().drawDark!=theme.dark||native_map_device().drawStyle!=theme.style||native_map_device().viewRevision!=revision)*reinterpret_cast<uint8_t*>(fork_word(0x1580)+0x370)=1;
 const auto& selectedPalette=device_map_palette(theme.style,theme.dark);for(auto& field:liveMapLayout.cacheColors)*(uint32_t*)(fork_word(0x1580)+field.second)=selectedPalette.colors.at(field.first);
 *reinterpret_cast<unsigned*>(native_map_device().state.data()+0x12a4)=mode;nativeForkOriginalUpdate(native_map_device().state.data(),false);native_map_device().drawDark=theme.dark;native_map_device().drawStyle=theme.style;native_map_device().viewRevision=revision;
 const auto clear=native_map_clear(theme.id());memcpy(native_map_device().layer.data()+0x14,clear.data(),16);
 // Argument five is the native suspended-display sentinel, NOT a clear colour.
 // Passing null requests UI generation even while the vehicle ignition is off.
 nativeForkQueuedThemeId=theme.id();
 static const auto deviceLabels=[](){std::array<std::array<std::string,nativeMapThemeCount>,2+managedDeviceCount> out;for(unsigned i=0;i<out.size();++i)for(unsigned t=0;t<nativeMapThemeCount;++t)out[i][t]="BetterGPS_Device_"+std::to_string(i)+"_theme_"+std::to_string(t);return out;}();const char* name=deviceLabels[nativeMapDevice][theme.id()].c_str();uint32_t key=0x7fffffff;nativeForkOriginalQueue(&key,(void*)fork_word(0x1578),native_map_device().layer.data(),&name,nullptr,0,nullptr);native_map_device().renderKey=key;native_map_device().keyValid=key!=0x7fffffff;nativeForkPublished=native_map_device().keyValid;if(nativeForkStatus<3)nativeForkStatus=3;
 return native_map_device().keyValid;
}
inline void __fastcall native_fork_render(void* object,bool active){
 // The dashboard graph is built even with ignition off. Own the map's update
 // and job here, before any consumers, rather than waiting for bare_map to run.
 // Never run native UI from Present, a worker, or a telemetry callback.
 if(managedNativeFrame&&render_gpu_allowed()&&!nativeForkDashboardScope)managedNativeFrame(object);
 if(nativeForkDashboardScope||!render_gpu_allowed()||!native_fork_wanted()){nativeForkOriginalRender(object,active);return;}
 native_fork_observe(object);for(auto& device:nativeMapDevices)device.keyValid=false;
 nativeForkDashboardScope=true;
 struct Exit{~Exit(){nativeForkDashboardScope=false;for(auto& device:nativeMapDevices)device.keyValid=false;}} exit;
 {NativeMapDeviceScope phone(0);native_fork_prepare_frame();}
 const auto seen=nativeForkHorizontalLastUse.load();if(seen&&GetTickCount64()-seen<500){NativeMapDeviceScope f150(1);native_fork_prepare_frame();}
 const auto managedVehicle=managed_vehicle();for(unsigned slot=0;slot<managedDeviceCount;++slot){auto d=managed_snapshot(slot);if(!managedDeviceActive[slot].load()||!managed_power_target(slot)||!d.used||!d.enabled||d.vehicle!=managedVehicle||d.known>=0&&size_t(d.known)<gpsDisplays.size()&&ford_console_id(gpsDisplays[d.known].id))continue;NativeMapDeviceScope device(2+slot);native_fork_prepare_frame();}
 nativeForkOriginalRender(object,active);
}
inline uint32_t* __fastcall native_fork_queue(uint32_t* result,void* window,void* layer,const char** label,const float* clear,uintptr_t token,void* dependencies){
 if(nativeForkInternal)return nativeForkOriginalQueue(result,window,layer,label,clear,token,dependencies);
 if(managedNativeRouteQueue)managedNativeRouteQueue(window,layer);
 const bool source=uintptr_t(window)==nativeForkSourceWindow&&uintptr_t(layer)==nativeForkSourceLayer;
 if(!nativeForkDashboardScope&&source)native_map_device().keyValid=false;
 if(managedNativeSeen)managedNativeSeen(window);
 if(render_gpu_allowed()&&native_fork_wanted()&&(!source||nativeForkDashboardScope)){native_fork_export(dependencies);if(managedNativeExport)managedNativeExport(dependencies);}
 // A private map job is insufficient: ATS also suspends the phone's UI
 // composition before first ignition. Wake only a positively identified phone
 // source; instrument panels, Original phones and all other layers stay native.
 const bool wake=clear&&render_gpu_allowed()&&((native_fork_phone_consumer(window,layer)&&native_fork_wanted())||(managedNativeWake&&managedNativeWake(window)));
 if(wake)++nativePhoneWakeRequests;
 auto* original=nativeForkOriginalQueue(result,window,layer,label,wake?nullptr:clear,token,dependencies);
 // Retain the source-driven fallback only for hosts without the dashboard hook.
 if(!nativeForkOriginalRender&&source)native_fork_prepare_frame();
 return original;
}
inline bool native_fork_install(){if(!nativeFork.supported)return false;struct Hook{void* address;void* callback;void** original;};Hook hooks[]={{(void*)nativeFork.uiPath,(void*)&native_fork_ui_path,(void**)&nativeForkOriginalUiPath},{(void*)nativeFork.dashboardCreate,(void*)&native_fork_dashboard_create,(void**)&nativeForkOriginalDashboardCreate},{(void*)nativeFork.render,(void*)&native_fork_render,(void**)&nativeForkOriginalRender},{(void*)nativeFork.buildGeometry,(void*)&native_fork_build_geometry,(void**)&nativeForkOriginalBuild},{(void*)nativeFork.update,(void*)&native_fork_update,(void**)&nativeForkOriginalUpdate},{(void*)nativeFork.queue,(void*)&native_fork_queue,(void**)&nativeForkOriginalQueue},{(void*)nativeFork.position,(void*)&native_fork_position,(void**)&nativeForkOriginalPosition},{(void*)nativeFork.view,(void*)&native_fork_view,(void**)&nativeForkOriginalView}};size_t done=0;for(auto& h:hooks){if(MH_CreateHook(h.address,h.callback,h.original)!=MH_OK)break;++done;}bool ok=done==std::size(hooks);if(ok){std::vector<void*> addresses;for(auto& h:hooks)addresses.push_back(h.address);ok=enable_hook_batch(addresses)==MH_OK;}if(!ok){for(size_t i=0;i<done;++i){MH_DisableHook(hooks[i].address);MH_RemoveHook(hooks[i].address);}return false;}liveMapForgetObserver=&native_fork_forget;nativeForkInstalled=true;return true;}
inline void native_fork_stop(){native_player_stop();if(managedNativeReset)managedNativeReset();if(!nativeForkInstalled.exchange(false)){native_fork_alias_release();return;}liveMapForgetObserver=nullptr;for(auto p:{(void*)nativeFork.uiPath,(void*)nativeFork.dashboardCreate,(void*)nativeFork.render,(void*)nativeFork.buildGeometry,(void*)nativeFork.update,(void*)nativeFork.queue,(void*)nativeFork.position,(void*)nativeFork.view}){MH_DisableHook(p);MH_RemoveHook(p);}native_fork_release_all();{std::lock_guard<std::mutex> lock(nativePhoneJobsMutex);nativePhoneJobs.clear();nativePhoneJobCount=0;}{std::lock_guard<std::mutex> lock(nativeForkThemesMutex);nativeForkThemes.clear();}native_fork_alias_release();nativeForkSource=nativeForkSourceWindow=nativeForkSourceMap=nativeForkSourceLayer=0;}
}
