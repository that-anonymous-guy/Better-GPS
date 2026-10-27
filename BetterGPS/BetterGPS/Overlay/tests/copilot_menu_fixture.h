#pragma once
namespace copilot_menu_fixture {
using namespace bettergps_overlay;
inline unsigned mapRequests=0,clearRequests=0;
inline bool open_map(){++mapRequests;return true;}
inline bool clear_route(){++clearRequests;return true;}
inline unsigned nativeOrder=0;
inline void __fastcall clear_controller(void* object){Check(nativeOrder==0,"Controller must clear before GPS");++nativeOrder;((uintptr_t*)object)[1]=0;}
inline void __fastcall reset_gps(void* gps){Check(nativeOrder==1,"GPS reset missed stored destinations");++nativeOrder;*(uintptr_t*)((uint8_t*)gps+0x28)=0;}
inline void native_clear_test(){
 const auto base=uintptr_t(GetModuleHandleW(nullptr));const auto* nt=(const IMAGE_NT_HEADERS64*)(base+((const IMAGE_DOS_HEADER*)base)->e_lfanew);const MapImage image{(uint8_t*)base,nt->OptionalHeader.SizeOfImage};std::vector<uint8_t> game(0x4128+0x5c8);static const struct {uintptr_t reserved[49];CopilotGpsReset clear;} table={{},&clear_controller};Check(!map_memory(uintptr_t(&table),sizeof(table)),"Fixture must use a read-only engine-style vtable");std::array<uintptr_t,2> controller{uintptr_t(&table),1};*(uintptr_t*)(game.data()+0x2b30)=uintptr_t(controller.data());
 for(unsigned mode:{2u,8u}){nativeOrder=0;controller[1]=1;*(unsigned*)(game.data()+0x4128+0x1c8)=mode;*(uintptr_t*)(game.data()+0x4128+0x28)=1;Check(copilot_clear_native_gps(uintptr_t(game.data()),&reset_gps,image)&&nativeOrder==2&&controller[1]==0,"Manual destination controller was rejected or retained waypoints");}
 for(unsigned mode:{6u,7u}){nativeOrder=0;*(unsigned*)(game.data()+0x4128+0x1c8)=mode;*(uintptr_t*)(game.data()+0x4128+0x28)=1;Check(!copilot_clear_native_gps(uintptr_t(game.data()),&reset_gps,image)&&nativeOrder==0,"Unsupported native destination mode was cleared");}
 zonarJobActive=true;Check(!copilot_clear_native_gps(uintptr_t(game.data()),&reset_gps,image),"Native reset cleared a mission");zonarJobActive=false;
}
inline void run(const std::filesystem::path& out){
 std::filesystem::create_directories(out);native_clear_test();managed_init(out);g_assetRoot=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets").u8string();managedSelected=-1;overlayManagedSelected=false;displayVehicle="kenworth.t680_2022";
 ManagedDevice d;d.used=d.enabled=d.horizontal=true;d.vehicle=displayVehicle;d.ui="/ui/dashboard/kenworth_t680_2022_gps.sii";d.alias="/vehicle/truck/share/gps.tobj";d.style=3;d.format=3;d.startup=2;d.splash="copilot/t680";d.prefs.power=0;d.prefs.bootHome=0;{DisplayStyleScope style(3);d.overlay=default_overlay_settings(true,3,4);}managedDevices[0]=d;managedPower[0].on=true;managedPower[0].phase=copilotBootMs+100;managedPower[0].serial=901;vehicleStart.epoch=902;vehicleStart.eligible=true;renderStopping=false;renderGameplayEnabled=false;settingsOpen=true;touchFixtureForeground=true;g_tripPresent=false;zonarJobActive=false;uint64_t clock=100000;
 auto page=[&](unsigned delta=5000){const auto p=managed_page_sample(0,managed_snapshot(0),clock+=delta,g_tripPresent,true);copilotMenu.sample(0,!p.transition&&p.page!=DevicePage::Home,vehicleStart.epoch.load(),managedPower[0].serial.load(),GetTickCount64());return p;};
 auto settle=[&]{return copilotMenu.sample(0,true,vehicleStart.epoch.load(),managedPower[0].serial.load(),GetTickCount64()+copilotMenuMs+1);};
 auto tap=[&](unsigned item,float u,float v){const auto config=managed_snapshot(0);const auto p=config.overlay.items[item];TouchHit hit;hit.valid=true;hit.trace=17;hit.u=.5f+((p.x+p.w*u)/900.f-.5f)*float(config.globalScale);hit.v=.5f+((p.y+p.h*v)/544.f-.5f)*float(config.globalScale);hit.surface={0,0,0,1,1,vehicleStart.epoch.load(),managedPower[0].serial.load(),managed_page_visual(0).serial};{std::lock_guard<std::mutex> lock(touchInput.mutex);touchInput.actions.push_back({hit,hit,false});}touch_actions_worker();};
 page();managed_page_destination(0,true);page();page();tap(1,.38f,.3f);const auto opening=page(1);Check(opening.transition==1,"CoPilot application open animation missing");
 std::vector<uint8_t> pixels,home;{DisplayStyleScope style(3);DevicePageScope scope(DevicePage::Home);render_copilot(home,d.overlay);}
 const auto bounds=copilot_logo_bounds();Check(bounds.x==(900-bounds.w)/2&&bounds.y==(544-bounds.h)/2&&bounds.w<=450&&bounds.h<=272,"Loading logo is not centred in a half-screen box with its aspect preserved");
 std::vector<uint8_t> loading(900*544*4,10);for(size_t i=3;i<loading.size();i+=4)loading[i]=255;const auto& logo=copilot_image("copilot_v9_logo.dds");overlay_blit(loading,logo.pixels,logo.width,logo.height,bounds,900,544);
 for(unsigned elapsed:{0u,499u,500u,600u,819u,820u,1820u,3819u}){
  DisplayStyleScope style(3);DevicePageScope scope(DevicePage::Notrip);render_copilot(pixels,d.overlay);const auto chrome=pixels;auto cold=opening;cold.elapsed=elapsed;copilot_interaction(pixels,d.overlay,cold,{});
  for(int y=0;y<544;++y)if(y<copilotAppTop||y>=483)for(int x=0;x<900;++x){const auto at=(y*900+x)*4;Check(pixels[at+3]==255,"Native map leaks through transparent chrome during slide");for(int c=0;c<3;++c)Check(pixels[at+c]==(chrome[at+c]*chrome[at+3]+10*(255-chrome[at+3])+127)/255,"Application slide changed fixed chrome");}
  if(elapsed<500)Check(std::equal(pixels.begin()+900*copilotAppTop*4,pixels.begin()+900*483*4,home.begin()+900*copilotAppTop*4),"Application moved before the 500ms click delay");
  if(elapsed==600)Check(pixels[(300*900+899)*4]==10&&pixels[(300*900+899)*4+3]==255,"Opening still uses the previous direction");
  if(elapsed>=820)Check(std::equal(pixels.begin()+900*63*4,pixels.begin()+900*483*4,loading.begin()+900*63*4),"Loading logo does not cut in for the full 3-second hold");
  else Check(!std::equal(pixels.begin()+900*63*4,pixels.begin()+900*483*4,loading.begin()+900*63*4),"Loading logo appeared before the swipe ended");
  Save(out/("open-"+std::to_string(elapsed)+".bmp"),pixels,900,544);
 }
 // The supplied Home dots overlap the transparent tail of the top texture.
 // During close latency they must be off-screen with Home, not frozen/cut.
 int dot=-1;for(int y=55;y<63&&dot<0;++y)for(int x=350;x<550;++x){const auto at=(y*900+x)*4;if(home[at]>170&&home[at+1]>170&&home[at+2]>170){dot=at;break;}}Check(dot>=0,"Source Home dots no longer overlap status padding; recheck crop fixture");
 {DisplayStyleScope style(3);DevicePageScope scope(DevicePage::Home);render_copilot(pixels,d.overlay);copilot_interaction(pixels,d.overlay,{DevicePage::Home,-1,100,opening.serial},{});Check(pixels[dot]<100&&pixels[dot+1]<100&&pixels[dot+2]<100,"Upper half of Home dots remains stuck outside close animation");Save(out/"close-delay-home-body.bmp",pixels,900,544);render_copilot(pixels,d.overlay);copilot_interaction(pixels,d.overlay,{DevicePage::Home,-1,copilotCloseDelayMs+copilotSlideMs,opening.serial},{});Check(std::equal(pixels.begin()+900*copilotAppTop*4,pixels.begin()+900*483*4,home.begin()+900*copilotAppTop*4),"Close animation does not restore the complete Home artwork");}
 Check(page(copilotOpenDelayMs+copilotSlideMs+copilotMapWaitMs-1).transition==1&&page(1).transition==0,"Application black wait timing wrong");
 const auto oldMap=copilotGameMapRequest,oldClear=copilotRouteExecute;copilotGameMapRequest=&open_map;copilotRouteExecute=&clear_route;
 for(bool trip:{false,true}){
  g_tripPresent=trip;page();auto open=[&]{tap(trip?6:7,trip?.08f:.5f,trip?.25f:.5f);Check(copilotMenu.get(0).target,"Apps did not open Driving Menu");settle();};open();
  {DisplayStyleScope style(3);DevicePageScope scope(trip?DevicePage::Nav:DevicePage::Notrip);render_copilot(pixels,d.overlay);auto base=pixels;copilot_background_icons(base,d.overlay,managed_page_visual(0).backgroundApps);copilot_interaction(pixels,d.overlay,managed_page_visual(0),copilotMenu.get(0));Check(pixels[(200*900+30)*4+3]==128,"Driving Menu did not dim exposed map by 50 percent");Check(!memcmp(pixels.data(),base.data(),900*63*4)&&!memcmp(pixels.data()+900*483*4,base.data()+900*483*4,900*61*4),"Driving Menu changed shared chrome or clock");Save(out/(trip?"driving-menu-nav.bmp":"driving-menu-notrip.bmp"),pixels,900,544);}
  TouchHit h;h.valid=true;h.u=.5f;h.v=.4f;h.surface={0,0,0,1,1,vehicleStart.epoch.load(),managedPower[0].serial.load(),managed_page_visual(0).serial};Check(!touch_copilot_pan_eligible(h),"Modal Driving Menu allows map pan");const auto zoom=map_view_mode_for(2);tap(4,.98f,.5f);Check(map_view_mode_for(2)==zoom,"Driving Menu click leaked into zoom");
  tap(2,.95f,.08f);Check(!copilotMenu.get(0).target&&copilotMenu.get(0).blocked(),"Close did not start slide-out");Check(!settle().blocked(),"Driving Menu did not finish slide-out");
  for(float y:{.3f,.65f})for(float x:{.18f,.5f,.8f}){open();const auto before=mapRequests;tap(2,x,y);Check(mapRequests==before+1&&!copilotMenu.get(0).target,"Blue Driving Menu cells did not open World Map");settle();}
  open();const auto before=clearRequests;zonarJobActive=true;tap(2,.25f,.88f);copilot_clear_route_frame();Check(clearRequests==before&&copilotMenu.get(0).target,"Clear Destination changed an active mission");zonarJobActive=false;
  if(trip){tap(2,.25f,.88f);zonarJobActive=true;copilot_clear_route_frame();Check(clearRequests==before,"Queued clear ignored new mission");zonarJobActive=false;tap(2,.25f,.88f);copilot_clear_route_frame();Check(clearRequests==before+1&&!copilotMenu.get(0).target,"Clear Destination not executed on SDK frame");settle();open();}
  else{tap(2,.25f,.88f);copilot_clear_route_frame();Check(clearRequests==before,"Clear Destination acted without a route");}
  tap(2,.75f,.88f);const auto closing=page(1);Check(closing.page==DevicePage::Home&&closing.transition==-1,"Driving Menu Main Menu did not close application");Check(page(copilotCloseDelayMs+copilotSlideMs).transition==0,"Application close waits or never ends");tap(1,.38f,.3f);page(1);page();open();tap(12,.5f,.5f);Check(page(1).page==DevicePage::Home,"Bottom Home was swallowed by Driving Menu");page();tap(1,.38f,.3f);page(1);page();
 }
 // Edited/scaled menu targets invert the same global UI zoom as the artwork.
 managedDevices[0].globalScale=1.1;managedDevices[0].overlay.items[2].x+=14;overlay_apply_scale(managedDevices[0].overlay,2,.85);tap(6,.08f,.25f);settle();const auto before=mapRequests;tap(2,.5f,.3f);Check(mapRequests==before+1,"Edited/global-zoom Driving Menu missed");settle();copilotGameMapRequest=oldMap;copilotRouteExecute=oldClear;settingsOpen=false;touchFixtureForeground=false;
 std::cout<<"PASS: T680 500ms launch hold, 3s centred logo cut, 250ms return delay, slides with fixed chrome, Nav/Notrip modal sheet, 50 percent dim, all six World Map cells, close/Home animations, mission/no-route/frame guards, scaled targets and pan/zoom exclusion.\n";
}
}
