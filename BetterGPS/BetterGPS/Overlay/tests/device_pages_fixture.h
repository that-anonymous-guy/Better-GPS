#pragma once
namespace device_pages_fixture {
using namespace bettergps_overlay;

inline std::array<uintptr_t,9> actorArguments{};
inline void __fastcall actor_original(uintptr_t a0,uintptr_t a1,uintptr_t a2,uintptr_t a3,uintptr_t a4,uintptr_t a5,uintptr_t a6,uintptr_t a7,uintptr_t a8){actorArguments={a0,a1,a2,a3,a4,a5,a6,a7,a8};}
inline void actor_observer(){
 std::vector<uintptr_t> controller(0x31b8/8);std::array<uintptr_t,4> actor{},preview{};uintptr_t truck=123;
 const uintptr_t control=uintptr_t(controller.data());controller[0x31b0/8]=uintptr_t(actor.data());actor[3]=uintptr_t(&truck);preview[3]=uintptr_t(&truck);
 playerControllerGlobal=uintptr_t(&control);playerActorOriginal=&actor_original;
 const auto epoch=vehicleStart.epoch.load();player_actor_initialized(uintptr_t(preview.data()),1,2,3,4,5,6,7,8);
 Check(!vehicleStart.waiting(),"Preview actor was treated as the player initialization");
 player_actor_initialized(uintptr_t(actor.data()),0,2,3,4,5,6,7,8);Check(!vehicleStart.waiting(),"Unsuccessful/null initialization armed Home");
 player_actor_initialized(uintptr_t(actor.data()),1,2,3,4,5,6,7,8);
 Check(actorArguments==std::array<uintptr_t,9>{uintptr_t(actor.data()),1,2,3,4,5,6,7,8},"Observer changed the native nine-argument ABI");
 Check(vehicleStart.waiting()&&vehicleStart.epoch==epoch,"Initialization was confirmed before fresh driving telemetry");
 Check(vehicleStart.confirm(uintptr_t(actor.data()),uintptr_t(&truck),true)&&vehicleStart.epoch==epoch+1,"Actual player actor/truck binding was rejected");
 playerActorOriginal=nullptr;playerControllerGlobal=0;
}

inline void gpu_close(const std::filesystem::path& out,UINT flags){
 ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;
 Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Page transition WARP unavailable");
 {
  IndependentPhoneSurface phone;Check(phone.start(device,0),"Page transition surface failed");
  D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=4;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  std::array<uint8_t,64> blue{};for(unsigned p=0;p<64;p+=4){blue[p]=20;blue[p+1]=40;blue[p+2]=220;blue[p+3]=255;}
  D3D11_SUBRESOURCE_DATA initial{blue.data(),16,0};ID3D11Texture2D* map=nullptr;ID3D11ShaderResourceView* view=nullptr;
  Check(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&map))&&SUCCEEDED(device->CreateShaderResourceView(map,nullptr,&view)),"Page transition map creation failed");
  std::vector<uint8_t> app(360*740*4,0);for(int y=30;y<90;++y)for(int x=10;x<90;++x)for(int c=0;c<4;++c)app[(y*360+x)*4+c]=255;
  const int phase=startup_total_ms();Check(phone.upload(context,app,360,1,1,phase),"App upload failed");phone.finish(context,nullptr,phase,1,1000,false,view);
  Check(phone.power.captured,"Actual app/map frame was not captured");
  std::vector<uint8_t> home(360*740*4,0);for(size_t p=0;p<home.size();p+=4){home[p+1]=220;home[p+3]=255;}
  Check(phone.upload(context,home,360,2,1,phase),"Home upload failed");
  D3D11_VIEWPORT sentinel{3,4,51,63,0,1};context->RSSetViewports(1,&sentinel);context->PSSetShaderResources(7,1,&view);
  phone.finish(context,nullptr,phase,1,1100,false,view,false,{DevicePage::Home,-1,175,100});
  Check(phone.power.pageClosing&&phone.view(phase,1)==phone.power.outputView,"Closing animation is not sampled by the physical phone");
  D3D11_VIEWPORT restored{};UINT count=1;context->RSGetViewports(&count,&restored);ID3D11ShaderResourceView* restoredView=nullptr;context->PSGetShaderResources(7,1,&restoredView);
  Check(memcmp(&sentinel,&restored,sizeof(sentinel))==0&&restoredView==view,"Page animation leaked engine viewport/resource bindings");SettingsMenuRenderer::drop(restoredView);
  D3D11_TEXTURE2D_DESC stageDesc{};phone.power.output->GetDesc(&stageDesc);stageDesc.BindFlags=stageDesc.MiscFlags=0;stageDesc.Usage=D3D11_USAGE_STAGING;stageDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* stage=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&stageDesc,nullptr,&stage)),"Closing readback allocation failed");
  context->CopyResource(stage,phone.power.output);D3D11_MAPPED_SUBRESOURCE read{};Check(SUCCEEDED(context->Map(stage,0,D3D11_MAP_READ,0,&read)),"Closing readback failed");std::vector<uint8_t> pixels(256*512*4);
  for(unsigned y=0;y<512;++y)memcpy(pixels.data()+size_t(y)*256*4,static_cast<uint8_t*>(read.pData)+size_t(y)*read.RowPitch,256*4);context->Unmap(stage,0);
  const auto* inside=pixels.data()+(256*256+128)*4;const auto* outside=pixels.data()+(256*256+1)*4;
  Check(inside[2]>180&&inside[1]<80&&outside[1]>180&&outside[2]<30,"Closing lost the real native map or did not reveal Home underneath");
  for(size_t p=0;p<pixels.size();p+=4)std::swap(pixels[p],pixels[p+2]);Save(out/(flags?"close-isolated.bmp":"close-deferred.bmp"),pixels,256,512);
  phone.finish(context,nullptr,phase,1,1140,false,nullptr,false,{DevicePage::Home,-1,200,100});phone.finish(context,nullptr,phase,1,1160,false,nullptr,false,{DevicePage::Home,-1,220,100});Check(phone.power.pageClosing&&phone.view(phase,1)==phone.power.outputView,"Closing flickered to Home while its map producer was suspended");
  phone.finish(context,nullptr,phase,1,1500,false,view,false,{DevicePage::Home,0,400,100});Check(!phone.power.pageClosing&&phone.view(phase,1)==phone.power.snapshotView,"Completed closing animation did not release the app view");
  context->ClearState();stage->Release();view->Release();map->Release();
 }
 context->Release();device->Release();
}

inline void run(const std::filesystem::path& out){
 std::filesystem::create_directories(out);g_assetRoot=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets").u8string();renderStopping=false;renderGameplayEnabled=false;managedSelected=-1;overlayManagedSelected=false;g_haveGameTime=true;g_gameTimeMinutes=10*60+35;g_multiplayerTimeOffset=0;g_tripPresent=false;
 ManagedDevice phone;phone.used=phone.enabled=true;phone.horizontal=false;phone.ui="/ui/dashboard/rt_phone_gps.sii";phone.alias="/vehicle/truck/share/gps.tobj";phone.vehicle="test.pages";phone.prefs.shuffle=0;phone.prefs.firstDelay=0;phone.prefs.wallpaper[0]="iphone_lock";phone.overlay=default_overlay_settings(false);
 actor_observer();
 // No SDK/menu/power signal can manufacture an initialization token.
 vehicleStart.eligible=false;vehicleStart.epoch=0;
 auto page=managed_page_sample(0,phone,1000,false,true);
 Check(page.page==DevicePage::Notrip,"Home available without successful player initialization");
 VehicleStartTracker tracker;tracker.observed(0,20);Check(!tracker.waiting(),"Invalid actor armed a token");
 tracker.observed(10,20);Check(!tracker.confirm(10,20,false)&&tracker.waiting(),"Menu/preview consumed a pending initialization");
 Check(!tracker.confirm(11,20,true)&&!tracker.eligible,"A preview/different actor was accepted");
 tracker.observed(10,20);Check(tracker.confirm(10,20,true)&&tracker.epoch==1&&tracker.eligible,"Fresh matching player initialization rejected");
 Check(!tracker.confirm(10,20,true)&&tracker.epoch==1,"Repeated telemetry minted another generation");
 tracker.observed(10,20);Check(tracker.confirm(10,20,true)&&tracker.epoch==2,"Reinitializing the same actor/address did not rearm");
 auto reset=[&]{++vehicleStart.epoch;vehicleStart.eligible=true;};
 auto shown=[&](unsigned slot,uint64_t now){auto v=managed_page_visual(slot);managed_page_presented(slot,v.serial,now);};
 reset();page=managed_page_sample(0,phone,1000,true,false);
 Check(page.page==DevicePage::Home&&!page.transition,"Existing route skipped the mandatory startup Home");
 page=managed_page_sample(0,phone,100000,true,false);Check(page.page==DevicePage::Home,"Splash consumed startup Home");
 page=managed_page_sample(0,phone,100100,true,true);
 page=managed_page_sample(0,phone,200100,true,true);Check(page.page==DevicePage::Home,"Preparation before first displayed frame consumed Home");
 shown(0,200100);page=managed_page_sample(0,phone,202099,true,true);Check(page.page==DevicePage::Home,"Home did not remain visible for two seconds");
 page=managed_page_sample(0,phone,202100,true,true);Check(page.page==DevicePage::Nav&&page.transition==1,"Home did not open directly into the existing route");
 page=managed_page_sample(0,phone,202450,true,true);Check(page.elapsed==pageOpenMs,"App expansion timing changed");
 page=managed_page_sample(0,phone,203450,true,true);Check(page.elapsed==pageOpenMs+pageLogoHoldMs,"App logo hold timing changed");
 page=managed_page_sample(0,phone,203750,true,true);Check(!page.transition,"App opening never finished");
 ++devicePages[0].requests;page=managed_page_sample(0,phone,204000,true,true);Check(page.page==DevicePage::Home&&page.cause==PageCause::Shortcut&&devicePages[0].forcedHome,"Home shortcut lost to active guidance");
 page=managed_page_sample(0,phone,204500,true,true);Check(page.page==DevicePage::Home,"Active route reopened Nav after manual Home");
 ++devicePages[0].requests;page=managed_page_sample(0,phone,204600,true,true);Check(page.page==DevicePage::Nav&&!devicePages[0].forcedHome,"Shortcut failed to resume active navigation");
 page=managed_page_sample(0,phone,900000,false,true);Check(page.page==DevicePage::Notrip,"Stopping/removing a route returned Home automatically");
 ++phoneSessionSerial;page=managed_page_sample(0,phone,900100,false,false);page=managed_page_sample(0,phone,900200,false,true);
 Check(page.page==DevicePage::Notrip,"A timer/power-session restart rearmed Home without actor initialization");
 ++devicePages[0].requests;page=managed_page_sample(0,phone,900300,false,true);Check(page.page==DevicePage::Home&&page.transition==-1,"Manual Home toggle stopped working");
 page=managed_page_sample(0,phone,999999,false,true);Check(page.page==DevicePage::Home,"Manually selected Home inherited the startup timeout");
 ++devicePages[0].requests;page=managed_page_sample(0,phone,1000000,false,true);Check(page.page==DevicePage::Notrip&&page.transition==1,"Manual app launch failed");
 reset();managed_page_sample(0,phone,1000,false,true);shown(0,1000);managed_page_sample(0,phone,1500,false,true);managed_pages_pause();
 page=managed_page_sample(0,phone,100000,false,true);Check(page.page==DevicePage::Home,"Menu time consumed Home dwell");
 page=managed_page_sample(0,phone,101499,false,true);Check(page.page==DevicePage::Home,"Home resumed with a shortened dwell");
 page=managed_page_sample(0,phone,101500,false,true);Check(page.page==DevicePage::Notrip,"Home dwell did not resume after pause");
 reset();managed_page_sample(0,phone,1000,false,true);shown(0,1000);managed_page_sample(0,phone,1500,false,true);
 managed_page_sample(0,phone,2000,false,false);page=managed_page_sample(0,phone,200000,false,true);Check(page.page==DevicePage::Home,"Power-off consumed Home");
 page=managed_page_sample(0,phone,201500,false,true);Check(page.page==DevicePage::Notrip,"Power-on failed to finish the remaining dwell");
 reset();phone.prefs.bootHome=0;page=managed_page_sample(0,phone,1000,true,true);Check(page.page==DevicePage::Nav,"Disabled startup Home delayed a route");
 phone.prefs.bootHome=1;
 DevicePreferences legacy;device_preferences_read(legacy,"page_mode","1");device_preferences_read(legacy,"idle_seconds","60");Check(legacy.bootHome==1&&legacy.bootHomeSeconds==2,"Legacy idle policy overwrote first-start defaults");
 ManagedDevice ford=phone;ford.knownId="f150_infotainment";Check(managed_page_kind(ford)==PageDevice::None,"F150 page tree is affected");ford.knownId="bronco_infotainment";Check(managed_page_kind(ford)==PageDevice::None,"Bronco page tree is affected");
 phone.prefs.bootHome=0;phone.prefs.bootHomeSeconds=17;phone.prefs.pageKey=VK_F8;phone.prefs.pageMods=4;std::ostringstream saved;device_preferences_write(saved,phone.prefs);DevicePreferences restored;std::istringstream input(saved.str());std::string line;while(std::getline(input,line)){auto at=line.find('=');device_preferences_read(restored,line.substr(0,at),line.substr(at+1));}Check(restored.bootHome==0&&restored.bootHomeSeconds==17&&restored.pageKey==VK_F8&&restored.pageMods==4,"Per-device page preferences were not persisted");
 phone.prefs.pageKey=VK_F9;phone.prefs.pageMods=3;managedDevices={};managedDevices[0]=phone;managedDevices[1]=phone;displayVehicle=phone.vehicle;managedSelected=1;pageLatchedKey=0;const auto old0=devicePages[0].requests.load(),old1=devicePages[1].requests.load();
 vehicleStart.eligible=false;Check(managed_page_key_event(VK_F9,3,true,true)&&devicePages[1].requests==old1,"Unconfirmed vehicle allowed manual Home");managed_page_key_event(VK_F9,0,false,true);vehicleStart.eligible=true;
 Check(!managed_page_key_event(VK_F9,1,true,true)&&!managed_page_key_event(VK_F9,2,true,true)&&managed_page_key_event(VK_F9,3,true,true),"Page shortcut modifier matching is not exact");
 Check(devicePages[0].requests==old0&&devicePages[1].requests==old1+1,"Page shortcut affected another device");Check(managed_page_key_event(VK_F9,2,true,true)&&devicePages[1].requests==old1+1,"Releasing Shift while holding F9 retriggered another shortcut");managed_page_key_event(VK_F9,0,false,true);Check(!pageLatchedKey,"Page shortcut latch did not release");
 displayPageKey=VK_F9;displayPageMods=3;Check(!settings_menu_shortcut(VK_F9,3)&&!display_key_valid(VK_F9,3),"Menu/power shortcut can steal the page chord");managedSelected=-1;
 // Every page family must honor the explicit shortcut over an active route.
 for(int style:{0,1,2,3,4,5,6,7}){auto test=phone;test.style=style==7?0:style;test.prefs.bootHome=0;if(style==7){test.known=-1;test.horizontal=true;test.frameSkin="Crown_Victoria_Sony.dds";}reset();auto first=managed_page_sample(6,test,1100000,true,true);managed_page_sample(7,test,1100000,true,true);Check(first.page==DevicePage::Nav,"Family did not initialize to active route");++devicePages[6].requests;auto home=managed_page_sample(6,test,1100100,true,true);Check(home.page==DevicePage::Home&&home.cause==PageCause::Shortcut,"Family shortcut did not return Home");Check(managed_page_sample(7,test,1100200,true,true).page==DevicePage::Nav,"Shortcut changed another device");Check(managed_page_sample(6,test,1110000,true,true).page==DevicePage::Home,"Route/timer stole manual Home");++devicePages[6].requests;Check(managed_page_sample(6,test,1110100,true,true).page==DevicePage::Nav,"Family shortcut did not resume navigation");}
 // Real home artwork, independently selected wallpaper, no foreground layer.
 auto layers=std::make_shared<HomeLayers>();auto bg=std::make_shared<HomePixels>(360*740*4),top=std::make_shared<HomePixels>(360*740*4);for(size_t p=0;p<bg->size();p+=4){(*bg)[p]=90;(*bg)[p+1]=120;(*bg)[p+2]=180;(*bg)[p+3]=255;(*top)[p]=255;(*top)[p+3]=255;}layers->pixels[2]=bg;layers->pixels[3]=top;layers->revision=1234;
 {StartupVisualScope style(3,0,layers,true);std::vector<uint8_t> light,dark;{DisplayThemeScope theme(false);render_phone_home(light,phone,phone.overlay);} {DisplayThemeScope theme(true);render_phone_home(dark,phone,phone.overlay);}const size_t p=(400*360+180)*4;Check(light[p]==90&&light[p+2]==180&&dark[p]==60&&dark[p+2]==120,"Home changed wallpaper, included Toplayer, or used the wrong night dim");for(size_t a=3;a<light.size();a+=4)Check(light[a]==255,"Home exposes the native GPS");unsigned white=0;for(int y=2;y<28;++y)for(int x=14;x<94;++x){auto q=(y*360+x)*4;if(light[q]>235&&light[q+1]>235&&light[q+2]>235)++white;}Check(white>5,"Phone Home clock is not white");Save(out/"iphone-home.bmp",light,360,740);phone.prefs.wallpaper[0]="android_lock";std::vector<uint8_t> android;render_phone_home(android,phone,phone.overlay);Check(android==light,"Legacy Android selection changed the unified Home artwork");Save(out/"legacy-selection-home.bmp",android,360,740);phone.prefs.wallpaper[0]="iphone_lock";
  for(uint64_t elapsed:{0ull,175ull,350ull,1350ull,1500ull}){std::vector<uint8_t> app(360*740*4,0);render_phone_page(app,phone,phone.overlay,{DevicePage::Notrip,1,elapsed,20});if(elapsed==1500)Check(app[p+3]>100&&app[p+3]<160,"Logo transition does not crossfade over the real application/map");Save(out/("open-"+std::to_string(elapsed)+".bmp"),app,360,740);}
 }
 ManagedDevice sony=phone;sony.vehicle="ford.victoria_06";sony.ui="/ui/gps.sii";sony.horizontal=true;sony.skin=3;sony.frameSkin="Crown_Victoria_Sony.dds";sony.overlay=default_overlay_settings(true);Check(managed_page_kind(sony)==PageDevice::Sony,"Sony Home is not associated with the integrated Crown display");
 std::vector<uint8_t> am,pm,next;managed_render(am,2,sony,-1,{DevicePage::Home});g_gameTimeMinutes=22*60+35;managed_render(pm,2,sony,-1,{DevicePage::Home});g_gameTimeMinutes=22*60+36;managed_render(next,2,sony,-1,{DevicePage::Home});Check(am!=pm&&pm!=next,"Sony Home uses a fixed example instead of the live game time and AM/PM");Save(out/"sony-home-am.bmp",am,800,500);Save(out/"sony-home-pm.bmp",pm,800,500);
 g_tripPresent=false;g_haveTruckSpeed=g_haveSpeedLimit=true;g_truckSpeedMps=15;g_speedLimitMps=25;g_haveNavTime=true;g_navTimeSec=22500;bettergps_navigation::Display guidance{"TURN_RIGHT","turn right in 1.3 mi onto I-10 EAST","APPROACH",true};
 for(bool night:{false,true})for(int format:{1,2}){DisplayStyleScope style(2);DisplayThemeScope theme(night);DevicePageScope pageScope(DevicePage::Notrip);auto config=default_overlay_settings(true,format);std::vector<uint8_t> pixels;render_garmin(pixels,config,guidance);Check(pixels[(30*900+300)*4+3]==0,"Nuvi Notrip still contains navigation header/instructions");const auto values=garmin_footer(night,false);for(int y=0;y<140;++y)for(int x=600;x<870;++x)Check(!values[(y*870+x)*4+3],"Nuvi Notrip still paints arrival");Save(out/("nuvi-notrip-"+std::to_string(format)+(night?"-dark":"-light")+".bmp"),pixels,900,format==2?600:504);auto navplus=default_overlay_settings(true,2,true);render_navplus(pixels,navplus,guidance);Check(pixels[(30*900+300)*4+3]==0,"NavPlus Notrip still contains navigation header/instructions");Save(out/(std::string("navplus-notrip-")+(night?"dark":"light")+".bmp"),pixels,900,600);}
 // Durations and enable flags belong to each device, independently.
 reset();phone.prefs.bootHome=1;phone.prefs.bootHomeSeconds=3;auto other=phone;other.prefs.bootHomeSeconds=10;
 managed_page_sample(0,phone,1000,false,true);managed_page_sample(1,other,1000,false,true);shown(0,1000);shown(1,1000);
 Check(managed_page_sample(0,phone,4000,false,true).page==DevicePage::Notrip&&managed_page_sample(1,other,4000,false,true).page==DevicePage::Home,"One device's startup duration affected another");
 managedBase=out/"profiles";std::filesystem::create_directories(managedBase);managedDevices={};managedDevices[0]=phone;managedDevices[1]=other;managedDevices[1].ui="/ui/dashboard/cellphone_gps.dlc_toys.sii";managedDevices[0].prefs.bootHomeSeconds=17;managedDevices[0].prefs.bootHome=0;managedDevices[0].prefs.pageKey=VK_F8;managedDevices[0].prefs.pageMods=4;
 managedDirty=true;Check(managed_save(),"Page profiles did not save");managed_init(managedBase);Check(managedDevices[0].prefs.bootHomeSeconds==17&&managedDevices[0].prefs.bootHome==0&&managedDevices[0].prefs.pageKey==VK_F8&&managedDevices[1].prefs.bootHomeSeconds==10&&managedDevices[1].prefs.bootHome==1,"Reload merged per-device page preferences");
 managed_select(0);displayBootHomeSeconds=19;displayPageMods=2;displayPageKey=VK_F10;managed_flush_selection();Check(managedDevices[0].prefs.bootHomeSeconds==19&&managedDevices[1].prefs.bootHomeSeconds==10,"Page settings editor modified another device");managed_select(1);Check(displayBootHomeSeconds==10&&displayPageKey==VK_F9&&displayPageMods==3,"Page editor retained another device's shortcut/duration");managedSelected=-1;overlayManagedSelected=false;
 // A native candidate alone is NOT proof that a physical phone is installed.
 gpsDisplays.clear();GpsDisplay physical;physical.id="fixture_phone";physical.name="Fixture phone";physical.ui=phone.ui;physical.layout="vertical";physical.ready=physical.independent=true;physical.settings=phone.overlay;gpsDisplays.push_back(physical);
 managedDevices={};managedCandidates.clear();DeviceCandidate candidate;candidate.known=0;candidate.ui=phone.ui;candidate.alias=phone.alias;candidate.vehicle=phone.vehicle;candidate.width=360;candidate.height=740;candidate.seen=1;managedCandidates.push_back(candidate);displayVehicle=phone.vehicle;displayPolicyReady=false;displayGlobalEnabled=true;displayDeviceEnabled=nullptr;managedInitialPreferences=phone.prefs;phonePower=18503;phoneStartupReady=true;phoneFirstWaitPending=false;displaySeenMask=0;
 managed_ensure_phone_page_profiles(20000);Check(!managedDevices[0].used,"Absent accessory was adopted from a shared/native candidate");displaySeenMask=1;managed_ensure_phone_page_profiles(20000);Check(managedDevices[0].used&&managedDevices[0].known==0&&managed_power_sample(0,managedDevices[0],20000)==1500&&managedPower[0].serial==phonePower,"Adopting an observed phone restarted its lockscreen/swipe or invalidated its visible frame");managedDevices[0].enabled=false;managed_ensure_phone_page_profiles(20050);Check(!managedDevices[0].enabled&&!managedDevices[1].used,"Automatic page profile overrode Original or created a duplicate");
 gpu_close(out,0);gpu_close(out,D3D11_CREATE_DEVICE_SINGLETHREADED);
 std::cout<<"PASS: independent Home/Notrip/Nav, confirmed initialization/visible boot dwell/manual/pause, exact latched shortcut, saved preferences, real wallpaper/OS icons/dim, live Sony AM/PM, Garmin Notrip and real map+UI GPU closing in deferred/isolated contexts.\n";
}
}
