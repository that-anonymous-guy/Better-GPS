#pragma once
namespace fork_fixture {
using namespace bettergps_overlay;
inline std::vector<uint8_t> source,world,unit,sourceMap,sourceMap2,originalCache,ownMap,ownWindow,otherMap,otherWindow;
inline std::vector<uint32_t> areas,discovered;
inline NativePalette original;
inline unsigned originalCalls=0,privateCalls=0,creates=0,destroys=0,freed=0,aliases=0,dynamicDeclarations=0;
inline bool deferredGeometry=false;
inline unsigned geometryCalls=0;
inline float poseX=100;
inline bool dashboardActive=false;
inline unsigned dashboardCalls=0,phoneQueueCalls=0;
inline std::array<uintptr_t,2> phonePair{};
inline std::array<uint8_t,0x168> phoneLayer{};
inline std::array<uint8_t,64> phoneWindow{};
inline double cameraHeading=0;
inline bool expectPhoneWake=true;
inline float height=0,playerSize=0;inline std::array<uint64_t,3> viewDetails{};inline std::array<float,4> camera{};inline std::array<float,8> player{};
inline std::array<void*,2> windowTable{};
inline void update(void* object,bool force){if(object==native_map_device().state.data()){Check(!force,"Private window update was skipped by force-only camera refresh");for(auto& f:liveMapLayout.colors)Check(*(uint32_t*)(unit.data()+f.second)==original.colors.at(f.first),"Placement update polluted native source palette");const auto mode=*reinterpret_cast<unsigned*>(native_map_device().state.data()+0x12a4);float pose[8]={poseX,2,300,0,0,0,0,1};native_fork_view((void*)fork_word(0x1580),mode,mode==0?50.f:mode==1?220.f:mode==2?900.f:4000.f,40,35,1,0,0,0);native_fork_position((void*)fork_word(0x1580),pose,false,nullptr,0);}}
inline uint32_t* queue(uint32_t* out,void* window,void*,const char**,const float* suspended,uintptr_t token,void* dependencies){if(window==phoneWindow.data()||window==phoneWindow.data()+16){Check(token==0x12345678&&dependencies,"Phone wake changed unrelated queue arguments");Check((suspended==nullptr)==(window==phoneWindow.data()&&expectPhoneWake),"Phone UI stayed suspended or a foreign dashboard was woken");++phoneQueueCalls;*out=4321;}else if(window==(void*)nativeForkSourceWindow){Check(token==0x67584923&&dependencies==sourceMap2.data(),"Native queue lost its sixth token or seventh dependency argument");++originalCalls;for(auto& f:liveMapLayout.colors)Check(*(uint32_t*)(unit.data()+f.second)==original.colors.at(f.first),"Original queue inherited Google palette");*out=1234;}else if(window==sourceMap2.data()+16){auto* table=(uintptr_t*)dependencies;if(nativeForkDashboardScope){Check(table&&table[2]==1&&*(uint16_t*)table[1]==0x8010&&*(uint32_t*)(table[1]+4)==native_map_device().renderKey,"Ignition-off consumer did not export the independent map job");}else Check(table&&table[2]==2&&*(uint16_t*)(table[1]+16)==0x8010&&*(uint32_t*)(table[1]+20)==native_map_device().renderKey,"Dashboard consumer did not receive private texture export before native rendering");*out=1234;}else{Check(suspended==nullptr&&token==0&&dependencies==nullptr,"Private map inherited foreign queue stack arguments");++privateCalls;for(auto& f:liveMapLayout.colors)Check(*(uint32_t*)(unit.data()+f.second)==original.colors.at(f.first),"Scheduling a private job changed native source palette");if(!deferredGeometry)native_fork_build_geometry((void*)fork_word(0x1580),nullptr);*out=9876+nativeMapDevice;}return out;}
inline void build(void* object,void*){NativeMapTheme theme;const bool owned=native_fork_theme(uintptr_t(object),theme);++geometryCalls;if(owned)Check(nativePlayerMaterial&&std::string(nativePlayerMaterial)==native_player_material(theme.style,theme.dark),"Deferred native player creation lost its per-map material scope");const auto& palette=owned?device_map_palette(theme.style,theme.dark):original;for(auto& f:liveMapLayout.colors)Check(*(uint32_t*)(unit.data()+f.second)==palette.colors.at(f.first),"Deferred geometry received wrong source palette");if(owned)for(auto& f:liveMapLayout.cacheColors)Check(*(uint32_t*)((uint8_t*)object+f.second)==palette.colors.at(f.first),"Deferred private cache received wrong palette");Check(areas==palette.areas&&discovered==palette.discovered,"Deferred geometry mixed area colours");}
inline void run(const std::filesystem::path& out){
 std::filesystem::create_directories(out);nativeForkLastSurfaceUse=GetTickCount64();{std::ofstream f(out/"BetterGPSMapViews.ini");f<<"[view.1]\nZoom=0.75\nOffsetX=0\nOffsetZ=0\n";}map_views_init(out);displayGlobalEnabled=true;displayPolicyReady=false;displayManualPower=-1;displayPowerMode=0;renderGameplayEnabled=false;renderActivityEnabled=false;renderStopping=false;
 auto assets=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets");for(int i=0;i<2;++i)liveMapPalettes[i]=read_native_palette(assets/"native_themes"/(i?"dark":"light")/"colour_data.sui");for(int i=0;i<2;++i)wazeMapPalettes[i]=read_native_palette(assets/"native_themes"/(i?"waze_dark":"waze_light")/"colour_data.sui");for(int i=0;i<2;++i)garminMapPalettes[i]=read_native_palette(assets/"native_themes"/(i?"garminmap_dark":"garminmap_light")/"colour_data.sui");for(int i=0;i<2;++i)copilotMapPalettes[i]=read_native_palette(assets/"native_themes"/(i?"copilot_dark":"copilot_light")/"colour_data.sui");original=read_native_palette("BetterGPS/Overlay/theme_sources/source/map_data.sii");original.colors["navigation_current_color"]=original.colors.at("navigation_color");
 liveMapLayout={};liveMapLayout.bytes=0x310;liveMapLayout.cacheBytes=0xdd8;liveMapLayout.vtable=0x1234560;liveMapLayout.areas=0x1e8;liveMapLayout.discovered=0x210;size_t at=0;for(auto& f:liveMapPalettes[0].colors)if(f.first=="road_color"||f.first=="road_discovered_color"||f.first.rfind("prefab",0)==0||f.first=="outline_color"||f.first.rfind("navigation",0)==0){liveMapLayout.colors[f.first]=0x100+4*at;liveMapLayout.cacheColors[f.first]=0x200+4*at++;}Check(at==14,"Fixture native color fields missing");
 source.assign(0x1598,0);world.assign(0xa0,0);unit.assign(0x310,0);sourceMap.assign(0xdd8,0);sourceMap2.assign(0xdd8,0);originalCache.assign(0xdd8,0x7b);ownMap.assign(0xdd8,0);ownWindow.assign(128,0);otherMap.assign(0xdd8,0);otherWindow.assign(128,0);areas=original.areas;discovered=original.discovered;
 *(uintptr_t*)unit.data()=liveMapLayout.vtable;*(uintptr_t*)(unit.data()+0x1f0)=uintptr_t(areas.data());*(size_t*)(unit.data()+0x1f8)=areas.size();*(uintptr_t*)(unit.data()+0x218)=uintptr_t(discovered.data());*(size_t*)(unit.data()+0x220)=discovered.size();for(auto& f:liveMapLayout.colors)*(uint32_t*)(unit.data()+f.second)=original.colors.at(f.first);*(uintptr_t*)(world.data()+0x98)=uintptr_t(unit.data());
 *(uintptr_t*)(source.data()+0x1578)=uintptr_t(source.data());*(uintptr_t*)(source.data()+0x1580)=uintptr_t(sourceMap.data());*(uintptr_t*)(source.data()+0x1588)=uintptr_t(source.data()+16);*(unsigned*)(source.data()+0x12a4)=1;
 nativeFork={};nativeFork.world=+[]()->void*{return world.data();};nativeFork.base=uintptr_t(world.data())-0x368dee0;*(void**)world.data()=world.data();nativeForkOriginalUpdate=&update;nativeForkOriginalQueue=&queue;nativeForkOriginalBuild=&build;
 nativeFork.rotation=+[](const float*,float* m)->float*{std::fill(m,m+16,0.f);m[0]=float(std::cos(cameraHeading));m[2]=float(-std::sin(cameraHeading));m[8]=float(std::sin(cameraHeading)*.7);m[10]=float(std::cos(cameraHeading)*.7);m[5]=m[15]=1;return m;};
 nativeForkOriginalView=+[](void*,unsigned,float h,float pitch,float size,float road,uint64_t icons,uint64_t city,uint64_t company){height=h;playerSize=size;viewDetails={icons,city,company};Check(pitch==40&&road==1,"Player scaling changed pitch/road size");};nativeFork.unpack=+[](float* out,const void* packed)->float*{const auto* p=(const float*)packed;const auto* sectors=(const int16_t*)((const uint8_t*)packed+12);out[0]=p[0]+sectors[0]*512.f;out[1]=p[1];out[2]=p[2]+sectors[1]*512.f;return out;};
 nativeForkOriginalPosition=+[](void* object,const float* pose,bool,const float* center,uintptr_t){memcpy(player.data(),pose,32);auto* bytes=(uint8_t*)object;memcpy(bytes+0x1b4,center?center:pose,16);memcpy(bytes+0x258,pose,32);
  std::array<float,3> target{};nativeFork.unpack(target.data(),bytes+0x1b4);const bool top=map_view_mode_for(nativeMapDevice)>=2;const auto angle=top?0:cameraHeading;const double distance=top?0:100;
  auto* position=(float*)(bytes+0x1a8);position[0]=target[0]-float(std::sin(angle)*distance);position[1]=float(height);position[2]=target[2]+float(std::cos(angle)*distance);memcpy(camera.data(),target.data(),12);
 };
 nativeFork.refreshPosition=+[](void* object,bool follow){Check(!follow,"Camera offsets requested native player-follow correction");const auto* position=(float*)((uint8_t*)object+0x1a8);const bool top=map_view_mode_for(nativeMapDevice)>=2;const auto angle=top?0:cameraHeading;const double distance=top?0:100;
  camera[0]=position[0]+float(std::sin(angle)*distance);camera[1]=position[1];camera[2]=position[2]-float(std::cos(angle)*distance);
 };
 nativeFork.setup=+[](void* state){auto* bytes=(uint8_t*)state;auto& map=nativeMapDevice?otherMap:ownMap;auto& window=nativeMapDevice?otherWindow:ownWindow;windowTable[1]=(void*)+[](void* w){native_fork_unref(*(uintptr_t*)((uint8_t*)w+16),false);};*(uintptr_t*)window.data()=uintptr_t(windowTable.data());*(uintptr_t*)(window.data()+16)=uintptr_t(map.data());*(LONG*)(window.data()+8)=1;*(LONG*)(map.data()+8)=2;for(auto& f:liveMapLayout.cacheColors)*(uint32_t*)(map.data()+f.second)=original.colors.at(f.first);*(uintptr_t*)(bytes+0x1578)=uintptr_t(window.data());*(uintptr_t*)(bytes+0x1580)=uintptr_t(map.data());};
 nativeFork.layerCreate=+[](void* p,uintptr_t,uintptr_t,uint8_t format,unsigned w,unsigned h,unsigned flags,const float*)->void*{Check(format==29&&w==1024&&h==1024&&flags==3,"Private source changed native render format");++creates;return p;};nativeFork.layerDestroy=+[](void*){++destroys;};nativeFork.layerUpdate=+[](void*){};nativeFork.freeUnit=+[](void*){++freed;};nativeFork.registerDynamic=+[](void*,const char** name){Check(std::string(*name)==native_fork_map_path(),"Wrong dynamic declaration path");++dynamicDeclarations;};nativeFork.alias=+[](void*,uint16_t* id,const char** name)->uint16_t*{Check(std::string(*name)==native_fork_map_path(),"Private source reused the original texture alias");Check(dynamicDeclarations==1+nativeMapDevice,"Alias resolved as a static DDS before native dynamic declaration");++aliases;*id=uint16_t(0x8010+nativeMapDevice);return id;};nativeFork.handle=+[](void* destination,uint16_t id){Check(id==0x8010||id==0x8011||id==0x8012||id==0x3fff,"Private texture handle lost");*(uint16_t*)destination=id;};
 Check(native_fork_alias_register()&&native_fork_alias_register()&&aliases==2&&dynamicDeclarations==2,"Dynamic alias must be registered once before source/UI setup");
 liveMapUnits={};liveMapCaches={};liveMapReady=true;nativeForkSource=nativeForkSourceWindow=nativeForkSourceMap=nativeForkSourceLayer=0;native_map_device().queued=0;
 const auto originalUnit=unit;const auto originalAreas=areas,originalDiscovered=discovered;const auto cache=originalCache;
 native_fork_update(source.data(),false);Check(mapViewCurrent==1&&liveMapUnits[0]==uintptr_t(unit.data()),"Source / preloaded map unit not detected");uint32_t result=0;const char* name="ATS original";float clear[4]{};
 for(int i=0;i<20;++i){liveMapDark=i%2;native_map_device().queued=0;native_fork_queue(&result,source.data(),source.data()+16,&name,clear,0x67584923,sourceMap2.data());Check(result==1234&&privateCalls==unsigned(i+1),"Private queue replaced native output/dependency key");Check(unit==originalUnit&&areas==originalAreas&&discovered==originalDiscovered&&originalCache==cache,"Google map preparation contaminated native source or F5 cache");Check(std::abs(height-220.f/.75f)<.01f&&player[0]==100&&player[2]==300,"Far 3D zoom or native player position incorrect");native_fork_forget(sourceMap.data());Check(!fork_word(0x1578)&&!fork_word(0x1580)&&!native_map_device().layerCreated,"Save/vehicle teardown retained a private window");native_fork_update(source.data(),false);}
 Check(creates==20&&destroys==20&&freed==40,"Native windows/layers accumulated across reloads");
 // Exercise the actual private producer's selection/update/position path,
 // not only the arithmetic of the two touch buttons. Source mode stays live.
 {NativeMapDeviceScope scope(2);Check(native_fork_alias_register_current(),"Managed preset test alias missing");
  const auto priorSnapshot=mapViewCustomSnapshot;mapViewCustomSnapshot=+[](unsigned,unsigned mode){return MapViewSetting{1.,double(mode*10),double(mode*5)};};
  map_view_observe_mode(1);Check(map_view_step_mode(2,1)==2&&native_fork_prepare_frame(),"Direct native 2D selection failed");
  Check(*(unsigned*)(native_map_device().state.data()+0x12a4)==2&&height==900&&std::abs(camera[0]-120)<.001&&std::abs(camera[2]-310)<.001,"2D preset did not reach native camera or used another mode's offsets");
  Check(map_view_step_mode(2,1)==3&&native_fork_prepare_frame()&&height==4000&&*(unsigned*)(native_map_device().state.data()+0x12a4)==3,"Native far preset not applied");
  Check(map_view_step_mode(2,1)==3,"Native preset limit wrapped");
  mapPan.context(2,true,mapViewSourceStamp.load(),1,1);Check(native_fork_prepare_frame(),"Pan pose preparation failed");
  Check(mapPan.drag(2,mapViewSourceStamp.load(),1,1,GetTickCount64(),.4,.4,.5,.4,544./900),"Native pan pose was not published");
  Check(native_fork_prepare_frame()&&camera[0]<100,"Pan failed to shift final native camera");
  const float anchor=camera[0];poseX=125;Check(native_fork_prepare_frame()&&std::abs(camera[0]-anchor)<.01&&player[0]==125,"Native pan anchor/player following incorrect");
  const auto ownMap=(const uint8_t*)fork_word(0x1580);Check(*(float*)(ownMap+0x258)==125&&*(float*)(ownMap+0x1b4)==125,"Pan corrupted packed player/center");
  mapPan.recenter(2);poseX=100;Check(native_fork_prepare_frame()&&std::abs(camera[0]-130)<.01,"Native recenter did not restore saved offsets");mapPan.context(2,false,0,0,0);
  for(unsigned expected:{2u,1u,0u}){Check(map_view_step_mode(2,-1)==expected&&native_fork_prepare_frame(),"Reverse native preset selection failed");Check(*(unsigned*)(native_map_device().state.data()+0x12a4)==expected&&height==(expected==0?50:expected==1?220:900),"Native preset camera height changed or mode ignored");}
  Check(map_view_step_mode(2,-1)==0&&map_view_mode_for(0)==1&&map_view_mode_for(3)==1&&*(unsigned*)(source.data()+0x12a4)==1,"Preset selection contaminated source/other GPS");
  *(unsigned*)(source.data()+0x12a4)=2;native_fork_observe(source.data());Check(map_view_mode_for(2)==2&&native_fork_prepare_frame()&&height==900,"Real source-view observation did not supersede touch preset");
  *(unsigned*)(source.data()+0x12a4)=1;native_fork_observe(source.data());map_view_step_mode(2,1);native_fork_release_all();Check(map_view_mode_for(2)==1,"Map teardown retained touch preset");mapViewCustomSnapshot=priorSnapshot;
 }
 mapViewEditing=1;map_view_adjust(1,5);map_view_adjust(2,-6);map_view_adjust(0,1);native_map_device().queued=0;native_fork_queue(&result,source.data(),source.data()+16,&name,clear,0x67584923,sourceMap2.data());Check(std::abs(height-275.f)<.01f&&camera[0]==125&&camera[2]==270&&player[0]==100&&player[2]==300,"Camera offsets moved player or failed to move camera");Check(map_views_save(),"Camera config save failed");map_views_init(out);Check(map_view_snapshot(1).x==25&&map_view_snapshot(1).z==-30&&map_view_snapshot(1).zoom==.8,"Camera profiles lost after restart");
 // Project the player/center displacement back onto the actual view axes.
 // It must be independent of heading, including north-up 2D modes.
 const float realPose[]={100,2,300,0,1,0,0,0};
 for(unsigned mode=0;mode<4;++mode){mapViewCurrent=mode;{std::lock_guard<std::mutex> lock(mapViewMutex);mapViews[mode].x=25;mapViews[mode].z=-30;}
  for(unsigned heading=0;heading<360;heading+=3){cameraHeading=mode>=2?0:heading*3.141592653589793/180.;native_fork_position((void*)fork_word(0x1580),realPose,false,nullptr,0);
   const double dx=camera[0]-realPose[0],dz=camera[2]-realPose[2];
   Check(std::abs(dx*std::cos(cameraHeading)+dz*std::sin(cameraHeading)-25)<.0001&&std::abs(-dx*std::sin(cameraHeading)+dz*std::cos(cameraHeading)+30)<.0001,"Player drifted across the screen when turning with camera offsets");
   Check(player[0]==100&&player[2]==300&&memcmp(ownMap.data()+0x258,realPose,32)==0&&memcmp(ownMap.data()+0x1b4,realPose,16)==0,"Camera correction changed native player or packed center coordinates");
  }
 }
 // Independently project the unchanged player through the final native
 // camera. Reproduce the user's Zoom=.6 / X=115 / Z=30 and its reverse,
 // rather than testing only the offset arithmetic or a fake center argument.
 for(unsigned mode=0;mode<4;++mode)for(double zoom:{.25,.6,1.,4.})for(double sign:{-1.,0.,1.}){
  mapViewCurrent=mode;height=float(220/zoom);{std::lock_guard<std::mutex> lock(mapViewMutex);mapViews[mode].x=115*sign;mapViews[mode].z=30*sign;}
  std::array<double,2> first{};
  for(unsigned heading=0;heading<360;heading+=3){cameraHeading=mode>=2?0:heading*3.141592653589793/180.;native_fork_position((void*)fork_word(0x1580),realPose,false,nullptr,0);
   const auto* c=(const float*)(ownMap.data()+0x1a8);const double dx=realPose[0]-c[0],dy=realPose[1]-c[1],dz=realPose[2]-c[2];
   const double cs=std::cos(cameraHeading),sn=std::sin(cameraHeading),horizontal=dx*cs+dz*sn;
   std::array<double,2> screen{};
   if(mode>=2)screen={horizontal/height,dz/height};else{
    const double distance=std::hypot(100.,double(height)-2),a=100/distance,b=(height-2)/distance;
    const double depth=-(-dx*sn*a+dy*b+dz*cs*a),up=dx*sn*b+dy*a-dz*cs*b;
    Check(std::abs(depth)>.001,"Projection fixture crossed its near plane");screen={horizontal/depth,up/depth};
   }
   if(!heading)first=screen;
   Check(std::abs(screen[0]-first[0])<.00002&&std::abs(screen[1]-first[1])<.00002,"Player's projected screen anchor moved while turning at the user's offsets/zoom");
  }
 }
 {std::lock_guard<std::mutex> lock(mapViewMutex);mapViews[1].x=25;mapViews[1].z=-30;}
 // Sector boundaries and large map coordinates must not be flattened into
 // the center argument or corrupt the separately packed player placement.
 float packedPose[8]={12,2,-33,0,1,0,0,0};auto* sectors=(int16_t*)((uint8_t*)packedPose+12);sectors[0]=120;sectors[1]=-220;
 for(unsigned heading=0;heading<360;heading+=5){cameraHeading=heading*3.141592653589793/180.;mapViewCurrent=1;native_fork_position((void*)fork_word(0x1580),packedPose,false,nullptr,0);
  std::array<float,3> actual{};nativeFork.unpack(actual.data(),packedPose);const double dx=camera[0]-actual[0],dz=camera[2]-actual[2];
  Check(std::abs(dx*std::cos(cameraHeading)+dz*std::sin(cameraHeading)-25)<.02&&std::abs(-dx*std::sin(cameraHeading)+dz*std::cos(cameraHeading)+30)<.02&&memcmp(ownMap.data()+0x258,packedPose,32)==0,"Packed-sector camera drift or player corruption");
 }
 cameraHeading=0;mapViewCurrent=1;
 alignas(16) std::array<uint8_t,64> exportItems{};std::array<uintptr_t,4> exportTable{0,uintptr_t(exportItems.data()),1,4};*(uint16_t*)exportItems.data()=*(uint16_t*)(nativeForkSourceLayer+0x160);*(uint32_t*)(exportItems.data()+4)=1234;
 nativeFork.append=+[](void* header,size_t amount)->bool{auto* t=(uintptr_t*)header;Check(amount==1&&t[2]<t[3],"Export table append overflow");auto* item=(uint8_t*)(t[1]+16*t[2]++);memset(item,0,16);*(uint16_t*)item=0x3fff;*(uint16_t*)(item+8)=0xffff;return true;};
 Check(native_fork_export(exportTable.data())&&exportTable[2]==2,"Private job was queued but never exported to dashboard consumers");Check(*(uint16_t*)(exportItems.data()+16)==0x8010&&*(uint32_t*)(exportItems.data()+20)==9876&&*(uint16_t*)(exportItems.data()+24)==0xffff&&nativeForkStatus==4,"Private export lost texture handle/job key/default ownership");Check(native_fork_export(exportTable.data())&&exportTable[2]==2,"Multiple consumers duplicated the private export");exportTable[2]=1;native_fork_queue(&result,sourceMap2.data()+16,nullptr,&name,nullptr,0,exportTable.data());Check(result==1234&&exportTable[2]==2,"Consumer forwarding failed to wire the private map job");
 const auto before=privateCalls;native_fork_queue(&result,source.data(),source.data()+16,&name,clear,0x67584923,sourceMap2.data());Check(privateCalls==before+1,"Same-tick render frame reused an expired graph key");
 auto count=privateCalls;displayGlobalEnabled=false;native_map_device().queued=0;native_fork_queue(&result,source.data(),source.data()+16,&name,clear,0x67584923,sourceMap2.data());Check(privateCalls==count&&!native_map_device().keyValid&&!native_fork_export(exportTable.data()),"Global OFF retained a stale private render graph export");displayGlobalEnabled=true;displayManualPower=0;native_fork_queue(&result,source.data(),source.data()+16,&name,clear,0x67584923,sourceMap2.data());Check(privateCalls==count,"Screen Off still built a private map");displayManualPower=-1;displayPolicyReady=true;displayEnabledMask=1;displaySeenMask=0;native_fork_queue(&result,source.data(),source.data()+16,&name,clear,0x67584923,sourceMap2.data());Check(privateCalls==count,"Unequipped phones created a private map");displayPolicyReady=false;renderGameplayEnabled=true;renderGameplayActive=false;native_fork_queue(&result,source.data(),source.data()+16,&name,clear,0x67584923,sourceMap2.data());Check(privateCalls==count,"Pause rendered a private map");renderGameplayEnabled=false;
 displayPolicyReady=false;displayGlobalEnabled=true;displayManualPower=-1;renderGameplayEnabled=false;deferredGeometry=true;
 for(unsigned id=0;id<48;++id){displayStyleIndex=(id/2)%12;liveMapDark=id%2;native_map_device().queued=0;native_fork_queue(&result,source.data(),source.data()+16,&name,clear,0x67584923,sourceMap2.data());const auto beforeUnit=unit;const auto beforeAreas=areas,beforeDiscovered=discovered;NativeMapTheme queued;Check(native_fork_theme(uintptr_t(ownMap.data()),queued)&&queued.style==displayStyleIndex.load()&&queued.dark==liveMapDark,"Queued theme identity missing");displayStyleIndex=(displayStyleIndex.load()+1)%12;liveMapDark=!liveMapDark;native_fork_build_geometry(ownMap.data(),nullptr);native_fork_build_geometry(sourceMap.data(),nullptr);Check(unit==beforeUnit&&areas==beforeAreas&&discovered==beforeDiscovered,"Deferred build leaked theme into native source");}
 deferredGeometry=false;
 // No native map/window/layer exists before the first ignition. The dashboard
 // must nevertheless create, update and export its own map in this frame.
 native_fork_release();nativeForkSource=nativeForkSourceWindow=nativeForkSourceMap=nativeForkSourceLayer=0;
 *(uintptr_t*)(source.data()+0x1578)=0;*(uintptr_t*)(source.data()+0x1580)=0;*(uintptr_t*)(source.data()+0x1588)=0;
 gpsDisplays.resize(1);gpsDisplays[0].id="fixture_phone";gpsDisplays[0].layout="vertical";gpsDisplays[0].ui="/ui/dashboard/fixture_phone.sii";gpsDisplays[0].ready=true;gpsDisplays[0].sourceIndex=0;displaySeenMask=1;
 nativeForkOriginalDashboardCreate=+[](void*,void* output,const char**,const char**,const unsigned*,void*)->bool{auto* pair=(uintptr_t*)output;pair[0]=uintptr_t(phoneLayer.data());pair[1]=uintptr_t(phoneWindow.data());return true;};
 const char* phoneUi="/ui/dashboard/fixture_phone.sii";const char* phoneAlias="phone alias";unsigned phoneSize[]={256,512};
 Check(native_fork_dashboard_create(source.data(),phonePair.data(),&phoneUi,&phoneAlias,phoneSize,nullptr)&&nativePhoneJobCount==1,"Phone source not identified from its configured UI path before first ignition");
 nativeForkOriginalRender=+[](void*,bool active){++dashboardCalls;Check(active==dashboardActive,"Independent map changed native dashboard ignition");if(!nativeForkDashboardScope)return;
  alignas(16) std::array<uint8_t,64> items{};std::array<uintptr_t,4> table{0,uintptr_t(items.data()),0,4};uint32_t key=0;const char* name="native dashboard";
  native_fork_queue(&key,sourceMap2.data()+16,nullptr,&name,nullptr,0,table.data());
  const float suspended[4]={0,0,0,1};
  native_fork_queue(&key,phoneWindow.data(),phoneLayer.data(),&name,suspended,0x12345678,table.data());
  native_fork_queue(&key,phoneWindow.data()+16,phoneLayer.data(),&name,suspended,0x12345678,table.data());
 };
 displayStyleIndex=0;liveMapDark=false;displayIgnition=false;displayPowerMode=0;displayManualPower=-1;
 const auto independentStart=privateCalls;
 for(unsigned frame=0;frame<80;++frame){dashboardActive=(frame%4)==1;displayIgnition=dashboardActive;poseX=200.f+frame;
  nativeForkLastSurfaceUse=GetTickCount64();native_fork_render(source.data(),dashboardActive);
  Check(privateCalls==independentStart+frame+1&&player[0]==poseX,"Map stopped refreshing without ignition or native source queue");
  Check(!native_map_device().keyValid&&!nativeForkDashboardScope,"Frame-local graph key escaped the dashboard scope");
 }
 Check(phoneQueueCalls==160&&nativePhoneWakeRequests==80,"First-ignition phone UI composition was never woken");
 Check(originalCalls>0&&dashboardCalls==80,"Independent dashboard cycles incomplete");
 expectPhoneWake=false;displayPolicyReady=true;displayEnabledMask=0;native_fork_render(source.data(),dashboardActive);displayEnabledMask=1;displayPolicyReady=false;expectPhoneWake=true;
 auto independentCount=privateCalls;displayGlobalEnabled=false;native_fork_render(source.data(),dashboardActive);Check(privateCalls==independentCount,"Global off independently rendered map");displayGlobalEnabled=true;
 displayManualPower=0;native_fork_render(source.data(),dashboardActive);Check(privateCalls==independentCount,"Power off independently rendered map");displayManualPower=-1;
 renderGameplayEnabled=true;renderGameplayActive=false;native_fork_render(source.data(),dashboardActive);Check(privateCalls==independentCount,"Pause independently rendered map");renderGameplayEnabled=false;
 // Distinct native ui_maps and aliases must consume distinct camera profiles.
 gpsDisplays.resize(2);gpsDisplays[1].id="f150_infotainment";selectedDisplay=1;mapViewEditing=1;
 {std::lock_guard<std::mutex> lock(mapViewMutex);mapViews[1]={.6,15,30};mapF150Views=mapViewDefaults;mapF150Views[1]={.75,0,0};}
 map_view_adjust(0,5);map_view_adjust(1,10);map_view_adjust(2,-2);
 Check(map_view_snapshot(1).zoom==.6&&map_view_snapshot(1).x==15&&map_view_snapshot(1).z==30,"Ford menu modified the phone profile");
 Check(map_view_selected_snapshot(1).zoom==1&&map_view_selected_snapshot(1).x==50&&map_view_selected_snapshot(1).z==-10,"Ford menu did not edit the selected device");
 nativeForkLastSurfaceUse=GetTickCount64();nativeForkHorizontalLastUse=GetTickCount64();cameraHeading=0;
 {NativeMapDeviceScope phone(0);Check(native_fork_prepare_frame(),"Phone producer unavailable");}
 {NativeMapDeviceScope ford(1);Check(native_fork_prepare_frame()&&fork_word(0x1580)==uintptr_t(otherMap.data()),"Ford did not own a separate native ui_map");}
 native_fork_view(ownMap.data(),1,220,40,35,1,0,0,0);Check(std::abs(height-220/.6)<.001,"Phone zoom inherited Ford zoom");native_fork_view(otherMap.data(),1,220,40,35,1,0,0,0);Check(height==220,"Ford zoom inherited phone zoom");
 native_fork_position(ownMap.data(),realPose,false,nullptr,0);Check(camera[0]==115&&camera[2]==330,"Phone camera inherited Ford offsets");native_fork_position(otherMap.data(),realPose,false,nullptr,0);Check(camera[0]==150&&camera[2]==290,"Ford camera inherited phone offsets");
 // All device/theme/icon combinations, including conflicting global settings.
 for(size_t style=0;style<12;++style)for(bool dark:{false,true})for(unsigned mode=0;mode<4;++mode){
  {std::lock_guard<std::mutex> lock(nativeForkThemesMutex);nativeForkThemes[uintptr_t(ownMap.data())]={style,dark};nativeForkThemes[uintptr_t(otherMap.data())]={3,false};}
  displayStyleIndex=(style+1)%3;const bool truck=style==2||(style>=6&&style<9)||style==11;const float size=mode==0?15.f:mode==1?35.f:mode==2?120.f:500.f;
  for(unsigned repeat=0;repeat<3;++repeat){native_fork_view(ownMap.data(),mode,220,40,size,1,11,22,33);Check(std::abs(playerSize-size*(truck&&mode<2?1.33f:1.f))<.001f&&viewDetails==std::array<uint64_t,3>{11,22,33},"Truck mesh scale must be exactly +33% only in private 3D views, without accumulation or icon contamination");}
  native_fork_view(otherMap.data(),mode,220,40,size,1,11,22,33);Check(playerSize==size,"Truck scaling leaked into another device's Original arrow");
  native_fork_view(sourceMap.data(),mode,220,40,size,1,11,22,33);Check(playerSize==size&&height==220,"Truck scaling leaked into ATS native map");
 }
 alignas(16) std::array<uint8_t,64> bothJobs{};std::array<uintptr_t,4> bothTable{0,uintptr_t(bothJobs.data()),0,4};nativeForkDashboardScope=true;Check(native_fork_export(bothTable.data())&&bothTable[2]==2&&*(uint16_t*)bothJobs.data()==0x8010&&*(uint16_t*)(bothJobs.data()+16)==0x8011,"Dashboard did not export both independent producers");nativeForkDashboardScope=false;
 Check(map_views_save(),"Device cameras failed to save");map_views_init(out);Check(map_view_snapshot_for(0,1).x==15&&map_view_snapshot_for(1,1).x==50,"Device camera profiles did not survive reload");map_view_reset();Check(map_view_snapshot_for(1,1).x==mapF150Defaults[1].x&&map_view_snapshot_for(0,1).x==15,"Ford reset modified phone camera");selectedDisplay=0;
 native_fork_release_all();Check(!*(uintptr_t*)(nativeMapDevices[0].state.data()+0x1580)&&!*(uintptr_t*)(nativeMapDevices[1].state.data()+0x1580),"Vehicle/save teardown retained a device map");
 nativeForkOriginalRender=nullptr;nativeForkOriginalDashboardCreate=nullptr;nativePhoneJobs.clear();nativePhoneJobCount=0;
 native_fork_release();native_fork_release();Check(creates==destroys&&freed==2*creates,"Idempotent private-source teardown failed");native_fork_alias_release();Check(native_map_device().aliasHandle==0x3fff&&nativeMapDevices[1].aliasHandle==0x3fff&&nativeMapDevices[2].aliasHandle==0x3fff&&aliases==3,"Reloads recreated or leaked one of the three tested material aliases");liveMapReady=false;nativeFork={};liveMapUnits={};gpsDisplays.clear();std::cout<<"PASS: direct per-GPS native preset selection in both directions with hard limits, correct preset offsets, source-key synchronization and teardown reset; separate native UI window/layer/texture alias, unchanged native output and full palette restoration, 20 reload lifecycles, 80 independent dashboard frames and exact phone-only UI wake before first ignition, 6312 final-camera projections/packed-sector turning views, including the user's zoom and offsets, with untouched player/center, native camera zoom/X/Z with unchanged player, saved profiles, 48 deferred geometry jobs across all four themes and player-icon variants despite conflicting global UI state, native geometry restoration, and no private work for Original/global off/power off/pause/unequipped phones.\n";
}
}
