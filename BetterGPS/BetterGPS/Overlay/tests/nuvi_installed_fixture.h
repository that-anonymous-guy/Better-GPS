#pragma once
namespace nuvi_installed_fixture {
using namespace bettergps_overlay;
inline void* queuedWindow=nullptr;inline void* queuedLayer=nullptr;inline const float* queuedClear=nullptr;inline uintptr_t queuedToken=0;inline void* queuedDependencies=nullptr;inline unsigned queueCalls=0;
inline uint32_t* __fastcall queue(uint32_t* result,void* window,void* layer,const char**,const float* clear,uintptr_t token,void* dependencies){queuedWindow=window;queuedLayer=layer;queuedClear=clear;queuedToken=token;queuedDependencies=dependencies;++queueCalls;*result=1234;return result;}
inline void run(const std::filesystem::path& out){
 const std::filesystem::path installed="H:/SteamLibrary/steamapps/common/American Truck Simulator/bin/win_x64/plugins/BetterGPS";
 std::filesystem::create_directories(out);std::filesystem::copy_file(installed/"BetterGPSDevices.ini",out/"BetterGPSDevices.ini");if(std::filesystem::exists(installed/"device-settings"))std::filesystem::copy(installed/"device-settings",out/"device-settings",std::filesystem::copy_options::recursive);
 const auto markers=out/"BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew";std::filesystem::create_directories(markers);for(const auto* name:{"phone_layout_marker.dds","phone_layout_marker.tobj"})std::filesystem::copy_file(installed/"BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew"/name,markers/name);
 g_assetRoot=(installed/"BetterGPS_assets").u8string();managed_init(out);gpsDisplays.clear();managedSelected=-1;overlayManagedSelected=false;managed_bridges_init();renderStopping=false;renderGameplayEnabled=false;vehicleStart.eligible=true;vehicleStart.epoch=900;displayIgnition=1;g_tripPresent=false;
 Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Nuvi hooks unavailable");ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Nuvi WARP unavailable");g_assetsReady=g_cpuRunning=true;
 unsigned tested=0;for(unsigned slot=0;slot<managedDeviceCount;++slot){auto d=managed_snapshot(slot);if(!d.used||d.ui!="/ui/dashboard/gps.dlc_toys.sii")continue;++tested;displayVehicle=d.vehicle;managedPower[slot].session=0;managedAppliedRevision[slot]=d.revision;managed_prepare_worker(GetTickCount64());
  const auto marker=display_bytes(out/"generated-displays"/(managed_prefix(slot,0).substr(1)+".dds"));D3D11_TEXTURE2D_DESC desc{};desc.Width=managed_marker_width(slot,0);desc.Height=managed_marker_height(0);desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{marker.data()+128,desc.Width*4,0};ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&view)),"Installed Nuvi marker unavailable");
  context->PSSetShaderResources(0,1,&view);ID3D11ShaderResourceView* bound=nullptr;context->PSGetShaderResources(0,1,&bound);Check(bound&&bound==g_managedBridges[slot*3]->liveView&&managedPower[slot].splashShownAt>0,"Actual installed high-slot Nuvi remains black at first splash upload");bound->Release();
  const auto size=managed_canvas(d);for(int phase:{0,3000,6000})for(DevicePage page:{DevicePage::Home,DevicePage::Notrip,DevicePage::Nav}){std::vector<uint8_t> canvas;managed_render(canvas,slot,d,phase,{page});Check(canvas.size()==size_t(size.first)*size.second*4,"Installed Nuvi canvas size mismatch");unsigned colored=0;for(size_t i=0;i<canvas.size();i+=4)if(canvas[i+3]&&(canvas[i]>15||canvas[i+1]>15||canvas[i+2]>15))++colored;Check(colored>500,"Installed Nuvi art is empty or black");if(phase==6000)Save(out/("slot-"+std::to_string(slot)+"-page-"+std::to_string(int(page))+".bmp"),canvas,size.first,size.second);}
  managed_prepare_worker(managedPower[slot].splashShownAt.load()+3100);context->PSSetShaderResources(0,1,&view);context->PSGetShaderResources(0,1,&bound);Check(bound==g_managedBridges[slot*3]->liveView&&managed_overlay_power_ready(slot,d),"Installed Nuvi stays black after splash");bound->Release();ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);view->Release();texture->Release();
  std::array<uint8_t,0x168> oldLayer{},newLayer{};std::array<uint8_t,64> oldWindow{},newWindow{};std::array<uintptr_t,2> pair{uintptr_t(newLayer.data()),uintptr_t(newWindow.data())};auto& state=managedNative[slot];state.applied=d.revision;state.vehicle=d.vehicle;state.replaced=true;state.pair=uintptr_t(pair.data());state.held={uintptr_t(oldLayer.data()),uintptr_t(oldWindow.data())};state.created=pair;managedDeviceActive[slot]=true;*(uint16_t*)(newLayer.data()+0x160)=166;
  void* w=oldWindow.data();void* l=oldLayer.data();Check(managed_native_route_queue(w,l)&&w==newWindow.data()&&l==newLayer.data(),"Cached original Nuvi job was not routed to its replacement");Check(!managed_native_route_queue(w,l),"Current replacement job routed twice");w=oldWindow.data();l=newLayer.data();Check(!managed_native_route_queue(w,l),"Unrelated mixed pair was redirected");w=oldWindow.data();l=oldLayer.data();pair[1]=0;Check(!managed_native_route_queue(w,l),"Expired/replaced owner was redirected");pair=state.created;managedDevices[slot].enabled=false;Check(!managed_native_route_queue(w,l),"Original/disabled Nuvi was redirected");managedDevices[slot]=d;
  const auto originalQueue=nativeForkOriginalQueue;const auto originalRoute=managedNativeRouteQueue;
  nativeForkOriginalQueue=&queue;managedNativeRouteQueue=&managed_native_route_queue;
  const float clear[4]{};uint32_t key=0;const char* label="Nuvi fixture";queueCalls=0;
  void* dependencies=oldLayer.data();
  native_fork_queue(&key,oldWindow.data(),oldLayer.data(),&label,clear,99,dependencies);
  Check(queueCalls==1&&queuedWindow==newWindow.data()&&queuedLayer==newLayer.data()&&queuedClear==clear&&queuedToken==99&&queuedDependencies==dependencies,"Native queue hook failed to forward the owned Nuvi pair or changed other arguments");
  managedDevices[slot].enabled=false;
  native_fork_queue(&key,oldWindow.data(),oldLayer.data(),&label,clear,99,dependencies);
  Check(queueCalls==2&&queuedWindow==oldWindow.data()&&queuedLayer==oldLayer.data(),"Native queue hook intercepted Original/disabled Nuvi");
  managedDevices[slot]=d;
  const auto oldAppend=nativeFork.append;const auto oldHandle=nativeFork.handle;
  std::array<uint8_t,64> entries{};std::array<uintptr_t,4> table{0,uintptr_t(entries.data()),0,4};
  nativeFork.append=+[](void* raw,size_t n){auto* t=(uintptr_t*)raw;if(n!=1||t[2]>=t[3])return false;++t[2];return true;};
  nativeFork.handle=+[](void* entry,uint16_t handle){*(uint16_t*)entry=handle;};
  const auto originalExport=managedNativeExport;const auto originalWake=managedNativeWake;
  const auto lastUse=nativeForkLastSurfaceUse.load();const auto horizontalUse=nativeForkHorizontalLastUse.load();
  const bool policy=displayPolicyReady.load(),anyActive=managedAnyActive.load();
  managedNativeExport=&managed_native_export;managedNativeWake=nullptr;displayPolicyReady=false;managedAnyActive=true;
  // Match the crash: ATS is already queuing this accessory's own UI while
  // ignition enables exports. Exporting that UI again adds it to its own
  // dependency table; a stub returning a valid key alone cannot validate it.
  for(unsigned frame=0;frame<3;++frame){
   ++managedNativeFrameSerial;nativeForkLastSurfaceUse=GetTickCount64();nativeForkHorizontalLastUse=GetTickCount64();
   Check(native_fork_wanted()&&render_gpu_allowed()&&managed_power_target(slot),"Nuvi crash regression did not exercise the powered native export path");
   const auto calls=queueCalls;const auto beforeEntries=entries;const auto beforeTable=table;
   native_fork_queue(&key,newWindow.data(),newLayer.data(),&label,clear,99,table.data());
   Check(queueCalls==calls+1,"Nuvi native consumer submitted its owned UI a second time during export");
   Check(table==beforeTable&&entries==beforeEntries,"Nuvi native consumer exported its own output as a dependency");
   Check(queuedWindow==newWindow.data()&&queuedLayer==newLayer.data()&&queuedClear==clear&&queuedToken==99&&queuedDependencies==table.data(),"Nuvi consumer arguments changed on powered export path");
   managed_native_export(table.data());
   Check(queueCalls==calls+1&&table==beforeTable&&entries==beforeEntries,"Nuvi UI was manually exported to an unrelated consumer");
  }
  // Keep the separate phone-producer contract: one manual job per frame,
  // exported once to its consumer, without changing the accessory policy.
  state.phone=true;state.replaced=false;++managedNativeFrameSerial;
  const auto phoneCalls=queueCalls;managed_native_export(table.data());
  Check(queueCalls==phoneCalls+1&&table[2]==1&&*(uint16_t*)entries.data()==166&&*(uint32_t*)(entries.data()+4)==1234,"Independent phone UI producer no longer exported");
  managed_native_export(table.data());
  Check(queueCalls==phoneCalls+1&&table[2]==1,"Independent phone UI producer queued/exported twice in one frame");
  nativeForkLastSurfaceUse=lastUse;nativeForkHorizontalLastUse=horizontalUse;displayPolicyReady=policy;managedAnyActive=anyActive;
  managedNativeExport=originalExport;managedNativeWake=originalWake;nativeFork.append=oldAppend;nativeFork.handle=oldHandle;
  nativeForkOriginalQueue=originalQueue;managedNativeRouteQueue=originalRoute;state={};
 }
 Check(tested>0,"No installed Nuvi profiles tested");g_cpuRunning=false;managed_gpu_stop();uninstall_d3d_hooks();MH_Uninitialize();context->Release();device->Release();std::cout<<"PASS: "<<tested<<" actual installed Nuvi profiles/assets, high-slot marker binding, splash readiness, Home/Notrip/Nav CPU images, powered native hook without duplicate UI jobs or self-dependencies; no installed inputs modified.\n";
}
}
