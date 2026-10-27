#pragma once
namespace t680_camera_fixture {
using namespace bettergps_overlay;
using ProbePose=CameraNativeProbe::Pose32;
struct alignas(16) ProbeNode {unsigned char prefix[0x30]{};ProbePose parent;};
inline ProbeNode probeNode,probeFrame,probeChild;inline unsigned directCalls=0,recursiveCalls=0;
inline bool directMatch=true,childMatch=false;
inline bool __fastcall fake_scene_locate(void* node,ProbePose* out,const uint64_t* name){++directCalls;if(*name!=CameraNativeProbe::token("bgps_v_f")||!((directMatch&&node==&probeNode)||(childMatch&&node==&probeChild)))return false;*out={};out->position[0]=.125f;return true;}
inline void* __fastcall fake_recursive_locate(void*,ProbePose* out,const uint64_t* name){++recursiveCalls;if(*name!=CameraNativeProbe::token("bgps_v_f"))return nullptr;*out={};out->position[2]=-.25f;return &probeNode;}
inline ProbePose* __fastcall fake_pose_compose(const ProbePose* parent,ProbePose* out,const ProbePose* local){*out=*parent;for(unsigned i=0;i<3;++i)out->position[i]+=local->position[i];return out;}
inline void probe_search_checks(){
 CameraNativeProbe probe;probe.base=0;probe.size=SIZE_MAX;probe.compose=&fake_pose_compose;probe.locateScene=&fake_scene_locate;
 alignas(16) std::array<unsigned char,0x7d0> truck{};alignas(16) std::array<unsigned char,0x30> entries{};std::array<uintptr_t,36> table{};
 const uintptr_t list=uintptr_t(entries.data()),count=1,node=uintptr_t(&probeNode),vtable=uintptr_t(table.data());
 memcpy(truck.data()+0x798,&list,8);memcpy(truck.data()+0x7a0,&count,8);memcpy(entries.data()+0x10,&node,8);const uintptr_t frameNode=uintptr_t(&probeFrame);memcpy(entries.data()+0x18,&frameNode,8);probeFrame={};memcpy(truck.data(),&vtable,8);table[0x118/8]=uintptr_t(&fake_recursive_locate);
 probeNode={};probeNode.parent.position[0]=2;ProbePose result{};directCalls=recursiveCalls=0;directMatch=true;
 Check(probe.locate(uintptr_t(truck.data()),"bgps_v_f",result)&&result.position[0]==2.125f&&directCalls==1&&recursiveCalls==0,"Installed camera in direct scene array was skipped in favour of recursive lookup");
 // Native scene frames can start at 8 modulo 16 (observed in the crash).
 // Our local/native-call poses remain aligned, but engine memory is copied
 // through bytes before invoking functions that use aligned SIMD loads.
 alignas(16) std::array<uint8_t,sizeof(ProbeNode)+16> unalignedStorage{};const auto unalignedFrame=uintptr_t(unalignedStorage.data()+8);
 ProbeNode externalFrame{};externalFrame.parent.position[1]=3;memcpy((void*)unalignedFrame,&externalFrame,sizeof(externalFrame));
 memcpy(entries.data()+0x18,&unalignedFrame,8);ProbePose attachment{};
 Check(probe.locate(uintptr_t(truck.data()),"bgps_v_f",result,&attachment)&&result.position[0]==2.125f&&result.position[1]==3&&attachment.position[1]==3,"8-byte aligned native frame crashes or corrupts the camera attachment pose");
 memcpy(entries.data()+0x18,&frameNode,8);
 directMatch=false;Check(probe.locate(uintptr_t(truck.data()),"bgps_v_f",result)&&result.position[2]==-.25f&&recursiveCalls==1,"Missing direct locator does not use native recursive fallback");
 alignas(16) std::array<unsigned char,0x18> children{};const uintptr_t childList=uintptr_t(children.data()),childNode=uintptr_t(&probeChild),bumperKey=CameraNativeProbe::token("bumper");
 memcpy(truck.data()+0x7c0,&childList,8);memcpy(truck.data()+0x7c8,&count,8);memcpy(children.data(),&bumperKey,8);memcpy(children.data()+8,&childNode,8);memcpy(entries.data(),&bumperKey,8);probeChild={};probeChild.parent.position[0]=-.25f;childMatch=true;const auto recursiveBefore=recursiveCalls;
 Check(probe.locate(uintptr_t(truck.data()),"bgps_v_f",result)&&result.position[0]==1.875f&&recursiveCalls==recursiveBefore,"Hookup optical locator did not compose through its actual bumper parent");
 childMatch=false;const uintptr_t noChildren=0;memcpy(truck.data()+0x7c8,&noChildren,8);
 Check(!probe.locate(0,"bgps_v_f",result),"Native locator search accepts unreadable truck");
 probeNode.parent.rotation[0]=0;Check(!probe.locate(uintptr_t(truck.data()),"bgps_v_f",result),"Native locator search accepts invalid parent pose");probeNode.parent.rotation[0]=1;
 const uintptr_t badCount=SIZE_MAX;memcpy(truck.data()+0x7a0,&badCount,8);directCalls=0;
 Check(probe.locate(uintptr_t(truck.data()),"bgps_v_f",result)&&directCalls==0,"Invalid direct scene count is traversed");
 const uintptr_t zero=0;memcpy(truck.data(),&zero,8);Check(!probe.locate(uintptr_t(truck.data()),"bgps_v_f",result),"Missing direct scenes dereference invalid recursive vtable");
 std::cout<<"PASS: native camera direct-scene priority, recursive fallback and bounded/invalid pose guards.\n";
}
inline uint32_t __fastcall mask_baseline(void*,unsigned){return 0x185;}
inline void final_pose_checks(){
 for(unsigned view=0;view<4;++view){alignas(16) std::array<uint8_t,0x548> source{};std::fill(source.begin(),source.end(),0xa7);const auto before=source;CameraNativeProbe::Pose32 desired{};desired.position[0]=.25f*view;desired.position[1]=2.7f;desired.position[2]=.53f;desired.sectors[0]=-200;desired.sectors[1]=34;bettergps_camera::camera_rotation(bettergps_camera::yawDegrees[view],bettergps_camera::downDegrees[view],desired.rotation);CameraPoseAudit::Target target{7,view,view,desired,{},view==3};
  const bool changed=CameraNativeBackend::applySource(source.data(),target,7);Check(changed==(view!=3),"Source hook alters native Rear or fails to set another camera");
  if(view==3){Check(source==before,"Validated native rear input was changed");continue;}
  for(unsigned i=0;i<source.size();++i)if(i<0x40||i>=0x60)Check(source[i]==before[i],"Source correction changes native calibration, cache or projection");
  CameraNativeProbe::Pose32 readback{};memcpy(&readback,source.data()+0x40,sizeof(readback));Check(!memcmp(&readback,&desired,sizeof(desired)),"Source camera/frustum does not receive the full optical pose");
  // SCS export is q -> q*(0,0,0,-1), verified from its instruction stream.
  // Both reversed texture axes undo that native half turn for this UI.
  auto render=readback;const float exportBasis[]={0,0,0,-1};bettergps_camera::quaternion_multiply(readback.rotation,exportBasis,render.rotation);const auto visible=bettergps_camera::camera_image_basis(render);const auto actual=bettergps_camera::view_metrics(visible),wanted=bettergps_camera::view_metrics(desired);Check(bettergps_camera::direction_error(actual,wanted)<.001&&std::abs(actual.roll)<.001,"Native source/export/image axes disagree");
  const auto stable=source;Check(!CameraNativeBackend::applySource(source.data(),target,8)&&source==stable,"Source hook applies an obsolete vehicle epoch");
 }
 Check(!CameraNativeBackend::applySource(nullptr,{},7),"Source hook accepts a null camera");
}
inline void audit_checks(){
 final_pose_checks();
 CameraPoseAudit audit;Check(audit.due(1,1,100)&&!audit.due(1,1,1099)&&audit.due(1,1,1100),"Camera audit frequency is not limited to one sample/s");
 Check(audit.due(1,2,1101),"Camera source change is not logged immediately");audit.enabled=false;Check(!audit.due(1,3,2200),"Disabled camera audit still logs");
 CameraPoseAudit observer;observer.configAfter=~uint64_t(0);CameraNativeProbe::Pose32 measured{},target{};bettergps_camera::camera_rotation(180,0,measured.rotation);
 alignas(16) std::array<uint8_t,0x100> output{};memcpy(output.data()+0x24,&measured,sizeof(measured));const auto before=output;
 observer.publish(0x1234,{7,2,0,target,{}});observer.observe((void*)0x4321,output.data(),100);Check(observer.samples==0,"Camera observer samples unrelated native cameras");
 observer.observe((void*)0x1234,output.data(),100);Check(observer.samples==1&&output==before,"Final camera pose audit modifies renderer data or misses its source");
 observer.observe((void*)0x1234,output.data(),101);Check(observer.samples==1,"Final camera audit ignores its sampling limit");
 for(unsigned view=0;view<4;++view){bettergps_camera::PackedPose p{};bettergps_camera::camera_rotation(bettergps_camera::yawDegrees[view],bettergps_camera::downDegrees[view],p.rotation);const auto m=bettergps_camera::view_metrics(p);Check(std::abs(m.down-bettergps_camera::downDegrees[view])<.001&&std::abs(m.roll)<.001,"Camera diagnostics misread a flat, pitched view");double yaw=std::abs(m.yaw-bettergps_camera::yawDegrees[view]);Check(std::min(yaw,360-yaw)<.001,"Camera diagnostics misread yaw");Check(bettergps_camera::direction_error(m,m)<.001,"Identical camera poses report direction error");}
 bettergps_camera::PackedPose inverted{};const float roll[]={0,0,0,1};std::copy_n(roll,4,inverted.rotation);Check(std::abs(std::abs(bettergps_camera::view_metrics(inverted).roll)-180)<.001,"Camera diagnostics miss an upside-down image");
}
inline void backend_checks(){
 audit_checks();
 renderStopping=false;renderGameplayEnabled=false;renderActivityEnabled=false;
 auto& b=cameraNativeBackend;b.original=&mask_baseline;b.filterOwner=0x1234;b.wanted=false;
 Check(camera_native_mask((void*)0x4321,2)==0x185,"Native camera gate changes an unrelated owner");
 Check(camera_native_mask((void*)0x1234,2)==0x105,"Camera keeps rendering outside its application");
 b.wanted=true;Check(camera_native_mask((void*)0x1234,2)==0x185,"Camera app does not activate native producer 7");
 renderStopping=true;Check(camera_native_mask((void*)0x1234,2)==0x105,"Native camera stays active during shutdown");renderStopping=false;
 b.original=nullptr;b.filterOwner=0;b.wanted=false;
 CameraNativeBackend restore;alignas(16) std::array<uint8_t,0x1400> owner{};alignas(16) std::array<uint8_t,0x548> unit{};std::array<uintptr_t,9> units{};
 units[7]=uintptr_t(unit.data());const auto array=uintptr_t(units.data());const uintptr_t count=9,table=0x22cb9a8;
 memcpy(owner.data()+0x13b0,&array,8);memcpy(owner.data()+0x13b8,&count,8);memcpy(unit.data(),&table,8);
 restore.savedOwner=uintptr_t(owner.data());restore.savedUnit=uintptr_t(unit.data());restore.savedFov=77;
 const auto previous=unit;restore.release(true);float fov=0;memcpy(&fov,unit.data()+0x20,4);Check(fov==77,"Native camera FOV is not restored");
 for(unsigned i=0;i<unit.size();++i)if(i<0x20||i>=0x24)Check(unit[i]==previous[i],"Release rewrites native poses it no longer owns");
 Check(!restore.savedOwner&&!restore.wanted,"Camera native ownership persists after release");
}

inline void run(const std::filesystem::path& out){
 PageVisual liveCamera{DevicePage::Camera,0,820,1};Check(copilot_camera_live(liveCamera),"Stable camera is not eligible for live video");
 for(int direction:{-1,1})for(unsigned elapsed:{0u,249u,499u,650u,819u}){auto moving=liveCamera;moving.transition=direction;moving.elapsed=elapsed;Check(!copilot_camera_live(moving),"Camera producer starts before its swipe completes");}
 Check(!copilot_camera_live({DevicePage::Home,0,0,2}),"Home enables the camera producer");
 bettergps_camera::VideoFrames frames;Check(!frames.present(1,1,false)&&!frames.present(1,1,true),"Pose preparation exposes PARK before video submission");Check(!frames.present(1,1,false)&&frames.present(1,1,true),"Two submitted native frames do not unlock video");Check(!frames.present(1,2,true)&&frames.present(1,2,true),"Changing view reuses old producer readiness");Check(!frames.present(2,2,true),"A new truck reuses old camera readiness");
 backend_checks();
 probe_search_checks();
 std::filesystem::create_directories(out);managed_init(out);g_assetRoot=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets").u8string();managedSelected=-1;overlayManagedSelected=false;displayVehicle="kenworth.t680_2022";vehicleStart.eligible=true;vehicleStart.epoch=1101;bettergps_camera::cameras.reset(1101);bettergps_camera::cameras.hardware(1101,0,{});
 ManagedDevice d;d.used=d.enabled=d.horizontal=true;d.vehicle=displayVehicle;d.ui="/ui/dashboard/kenworth_t680_2022_gps.sii";d.alias="/vehicle/truck/share/gps.tobj";d.style=3;d.format=3;d.prefs.power=0;d.prefs.bootHome=0;{DisplayStyleScope style(3);d.overlay=default_overlay_settings(true,3,4);}managedDevices[0]=d;managedPower[0].on=true;managedPower[0].phase=copilotBootMs+100;managedPower[0].serial=1102;
 renderStopping=false;renderGameplayEnabled=false;settingsOpen=true;touchFixtureForeground=true;g_tripPresent=false;uint64_t now=10000;
 auto page=[&](unsigned elapsed){return managed_page_sample(0,d,now+=elapsed,g_tripPresent,true);};
 auto tap=[&](unsigned item,float u,float v){const auto p=d.overlay.items[item];TouchHit h;h.valid=true;h.trace=31;h.u=(p.x+p.w*u)/900.f;h.v=(p.y+p.h*v)/544.f;h.surface={0,0,0,1,1,vehicleStart.epoch.load(),managedPower[0].serial.load(),managed_page_visual(0).serial};{std::lock_guard<std::mutex> lock(touchInput.mutex);touchInput.actions.push_back({h,h,false});}touch_actions_worker();};
 page(1);managed_page_destination(0,true);page(1);page(5000);tap(1,505.f/1324,580.f/800);const auto opening=page(1);Check(opening.page==DevicePage::Camera&&opening.transition==1,"Camera Views home tile did not open its own page");
 std::vector<uint8_t> camera,home;{DisplayStyleScope style(3);DevicePageScope scope(DevicePage::Home);render_copilot(home,d.overlay);}{DisplayStyleScope style(3);DevicePageScope scope(DevicePage::Camera);render_copilot(camera,d.overlay);}
 auto restoredHome=home;restoredHome[(200*900+600)*4+3]=0;PageVisual settledHome{DevicePage::Home,0,570,3};settledHome.fromPage=DevicePage::Camera;copilot_interaction(restoredHome,d.overlay,settledHome,{});for(size_t at=3;at<restoredHome.size();at+=4)Check(restoredHome[at]==255,"Home padding exposes a queued native PARK draw after the swipe");
 Save(out/"camera.bmp",camera,900,544);unsigned iconPixels=0;for(int y=63;y<483;++y)for(int x=0;x<900;++x){const auto at=(y*900+x)*4;Check(camera[at+3]==255,"Camera page has transparency exposing the live map");if(x>=153)Check(camera[at]==10&&camera[at+1]==10&&camera[at+2]==10,"Camera background is not exact RGB(10,10,10) or contains navigation UI");else if(camera[at]>100)++iconPixels;}Check(iconPixels>3500,"Camera truck selector is missing from the left edge");
 for(unsigned elapsed:{0u,499u,600u,819u}){auto pixels=camera;auto visual=opening;visual.elapsed=elapsed;copilot_interaction(pixels,d.overlay,visual,{});Save(out/("opening-"+std::to_string(elapsed)+".bmp"),pixels,900,544);for(int y=63;y<483;++y)for(int x=0;x<900;++x){const auto at=(y*900+x)*4;Check(pixels[at+3]==255,"Camera swipe exposes live map");if(elapsed<500)Check(std::equal(pixels.begin()+at,pixels.begin()+at+4,home.begin()+at),"Camera opens before standard click delay");}}
 Check(page(819).transition==1&&page(1).transition==0,"Camera app has an unexpected loading hold");g_tripPresent=true;Check(page(5000).page==DevicePage::Camera&&managed_page_visual(0).cameraPopup,"Camera popup expired or route replaced its page");
 const auto modal=managed_page_visual(0);auto popup=camera;copilot_interaction(popup,d.overlay,modal,{});Save(out/"camera-popup.bmp",popup,900,544);Check(popup!=camera,"Camera popup artwork is not rendered");for(int y=0;y<544;++y)for(int x=0;x<900;++x)if(x<263||x>=637||y<234||y>=312){const auto at=(y*900+x)*4;Check(std::equal(popup.begin()+at,popup.begin()+at+4,camera.begin()+at),"Camera popup changes pixels outside screenshot-sized card");}
 tap(12,.5f,.5f);tap(12,.324f,.5f);tap(1,450.f/900,250.f/544);devicePages[0].requests.fetch_add(1);Check(page(1).page==DevicePage::Camera&&managed_page_visual(0).cameraPopup,"Popup dismissed by Home, Return, message body or shortcut");tap(1,450.f/900,299.f/544);Check(!page(1).cameraPopup&&managed_page_visual(0).serial==modal.serial,"OK does not dismiss only the popup");page(30000);Check(!managed_page_visual(0).cameraPopup,"Acknowledged popup reappears without reopening Camera");
 TouchHit hit;hit.valid=true;hit.surface={0,0,0,1,1,vehicleStart.epoch.load(),1102,managed_page_visual(0).serial};Check(!touch_copilot_pan_eligible(hit),"Camera accepts map dragging");tap(7,.5f,.5f);Check(page(1).page==DevicePage::Camera&&!copilotMenu.get(0).blocked(),"Camera selector opens map Driving Menu");
 managed_page_gauge(0,0);const auto warning=page(1);Check(warning.subpage&&warning.fromPage==DevicePage::Camera,"Gauge interrupt lost Camera source");std::vector<uint8_t> interrupted;{DisplayStyleScope style(3);DevicePageScope scope(DevicePage::Gauges,0);render_copilot(interrupted,d.overlay);}copilot_interaction(interrupted,d.overlay,warning,{});Check(std::equal(interrupted.begin()+900*63*4,interrupted.begin()+900*483*4,camera.begin()+900*63*4),"Gauge interrupt flashes Home instead of Camera");page(820);tap(12,.324f,.5f);Check(page(1).page==DevicePage::Camera,"Return after gauge interrupt does not restore Camera");page(570);
 tap(12,460.f/1324,.5f);const auto back=page(1);Check(back.page==DevicePage::Home&&back.transition==-1,"Camera Return does not close to Home");Check(page(569).transition==-1&&page(1).transition==0,"Camera close lost standard click delay");tap(1,505.f/1324,580.f/800);page(1);page(820);tap(1,450.f/900,299.f/544);page(1);tap(12,.5f,.5f);Check(page(1).page==DevicePage::Home,"Camera Home button does not close to Home");page(570);tap(1,505.f/1324,580.f/800);page(1);page(820);tap(1,450.f/900,299.f/544);page(1);devicePages[0].requests.fetch_add(1);Check(page(1).page==DevicePage::Home,"Camera traps Home shortcut");
 page(570);bettergps_camera::cameras.hardware(1101,5,{});tap(1,505.f/1324,580.f/800);page(1);page(820);Check(!managed_page_visual(0).cameraPopup,"Installed cameras still show no-camera popup");bettergps_camera::cameras.sample(true,false,false,false);
 tap(1,130.f/900,275.f/544);Check(bettergps_camera::cameras.snapshot().selected==bettergps_camera::View::Right,"Right camera touch region failed");tap(1,20.f/900,275.f/544);Check(bettergps_camera::cameras.snapshot().selected==bettergps_camera::View::Right,"Missing left camera accepts a manual selection");tap(1,75.f/900,160.f/544);Check(bettergps_camera::cameras.snapshot().selected==bettergps_camera::View::Forward,"Forward camera touch region failed");
 const auto ready=bettergps_camera::cameras.snapshot();bettergps_camera::renderEpoch=ready.epoch;bettergps_camera::renderSelection=ready.selection;
 auto visible=copilot_camera_layer();Check(visible[(200*900+600)*4+3]==0,"Ready camera body is not transparent for the native SCS image");
 bettergps_camera::renderSelection=ready.selection+1;visible=copilot_camera_layer();Check(visible[(200*900+600)*4+3]==255,"Stale camera selection exposes a previous view");
 bettergps_camera::renderEpoch=0;bettergps_camera::renderSelection=0;
 const auto markerFolder=out/"BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew";std::filesystem::create_directories(markerFolder);
 for(const auto* name:{"phone_layout_marker.dds","phone_layout_marker.tobj"})std::filesystem::copy_file(std::filesystem::path(g_assetRoot)/"material/ui/dashboard/bettergps_overlaynew"/name,markerFolder/name,std::filesystem::copy_options::overwrite_existing);
 managed_build_ui(0,d);auto readUi=[&](bool native){const auto bytes=display_bytes(out/"generated-displays"/managed_ui_name(0,d.revision,false,native).substr(1));return std::string(bytes.begin(),bytes.end());};
 const auto normalUi=readUi(false),cameraUi=readUi(true);
 Check(normalUi.find(managed_map_name(0)+".mat")!=std::string::npos&&normalUi.find(managed_prefix(0,1)+".mat")!=std::string::npos,"Normal NavPlus UI lost its private map/primer");
 Check(normalUi.find("park_1x1")==std::string::npos&&normalUi.find("id: 910")==std::string::npos,"Home/navigation still contains a native parking-camera consumer");
 Check(cameraUi.find(managed_map_name(0)+".mat")==std::string::npos&&cameraUi.find(managed_prefix(0,1)+".mat")==std::string::npos,"Native camera UI captures its image as navigation map");
 Check(cameraUi.find("park_1x1_ui.mat left=1 right=0 top=0.733333333333 bottom=0.266666666667")!=std::string::npos&&cameraUi.find("id: 910")!=std::string::npos,"Native camera material, vertical flip/crop or SCS registration missing");
 const auto rearBytes=display_bytes(out/"generated-displays"/managed_ui_name(0,d.revision,false,true,true).substr(1));const std::string rearUi(rearBytes.begin(),rearBytes.end());
 Check(rearUi.find("park_1x1_ui.mat left=0 right=1 top=1 bottom=0")!=std::string::npos&&rearUi.find(managed_map_name(0)+".mat")==std::string::npos,"Native rear no longer uses the validated SCS material orientation");
 Check(cameraUi.find("<color value=00000000><img src=/material/environment/mirror/park_1x1_ui.mat")!=std::string::npos,"Hidden camera registration draws native PARK fallback artwork");
 Check(cameraUi.find("<color value=FF0A0A0A><img src=/material/ui/white.mat")!=std::string::npos,"Native camera backing is not RGB10");
 bettergps_camera::cameras.reset(0);
 settingsOpen=false;touchFixtureForeground=false;g_tripPresent=false;
 std::cout<<"PASS: Camera tile, opaque RGB10 background, left selector, standard app delays, Return/Home/shortcut, route/pan/menu isolation and gauge interrupt history.\n";
}
}
