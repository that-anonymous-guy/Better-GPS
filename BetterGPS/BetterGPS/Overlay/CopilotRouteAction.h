#pragma once
// The same GPS reset routine reached by world_map_hdl's button 360. Resolve
// only on a click, with the existing supported-build gate and exact call-site
// checks. The preparation worker queues; the SDK frame executes on game thread.
struct CopilotRouteRequest {unsigned slot=~0u;uint64_t epoch=0,power=0,page=0,trace=0,at=0;};
inline std::mutex copilotRouteMutex;
inline CopilotRouteRequest copilotRoutePending;
using CopilotGpsReset=void(__fastcall*)(void*);
struct CopilotRouteDiagnostic {const char* reason="not-called";unsigned mode=~0u;uintptr_t game=0,task=0,controller=0,method=0;};
inline thread_local CopilotRouteDiagnostic copilotRouteDiagnostic;
static bool copilot_clear_native_gps(uintptr_t game,CopilotGpsReset reset,const MapImage& image){
 auto& diagnostic=copilotRouteDiagnostic;diagnostic.game=game;
 if(zonarJobActive){diagnostic.reason="job-active";return false;}if(!reset){diagnostic.reason="reset-unresolved";return false;}if(!map_memory(game,0x4128+0x5c8)){diagnostic.reason="controller-unreadable";return false;}
 const auto gps=game+0x4128;const auto mode=*(unsigned*)(gps+0x1c8);
 diagnostic.mode=mode;diagnostic.task=*(uintptr_t*)(gps+0x28);
 // Match the stock Reset Navigation restriction (6/7), rather than rejecting
 // every mode above five or treating any navigation controller as a mission.
 if(mode==6||mode==7){diagnostic.reason="native-mode-protected";return false;}if(!diagnostic.task){diagnostic.reason="route-task-empty";return false;}
 const auto controller=*(uintptr_t*)(game+0x2b30);
 diagnostic.controller=controller;
 if(controller){if(!map_memory(controller,sizeof(uintptr_t))){diagnostic.reason="destination-controller-unreadable";return false;}const auto table=*(uintptr_t*)controller;if(!image.contains(table,0x190)){diagnostic.reason="destination-vtable-outside-image";return false;}const auto clear=image.get<uintptr_t>(table+0x188);diagnostic.method=clear;if(!image.executable(clear)){diagnostic.reason="destination-method-outside-image";return false;}
  // Clear the controller's destinations first, exactly like world_map_hdl.
  // Resetting only the computed route lets the stored waypoints recreate it.
  ((CopilotGpsReset)clear)((void*)controller);
 }
 reset((void*)gps);diagnostic.reason="native-reset-called";return true;
}
static bool copilot_native_clear_route(){
 copilotRouteDiagnostic={};auto& diagnostic=copilotRouteDiagnostic;
 if(!nativeFork.supported||!nativeFork.base){diagnostic.reason="unsupported-native-build";return false;}if(zonarJobActive){diagnostic.reason="job-active";return false;}
 const auto base=nativeFork.base;const auto* nt=(const IMAGE_NT_HEADERS64*)(base+((const IMAGE_DOS_HEADER*)base)->e_lfanew);const MapImage image{(uint8_t*)base,nt->OptionalHeader.SizeOfImage};
 static const uint8_t prefix[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x50};
 if(!image.contains(base+0x4fc070,sizeof(prefix))||memcmp((void*)(base+0x4fc070),prefix,sizeof(prefix))){diagnostic.reason="reset-prologue-mismatch";return false;}if(image.get<uint8_t>(base+0x101563d)!=0xe8||base+0x1015642+image.get<int32_t>(base+0x101563e)!=base+0x4fc070){diagnostic.reason="world-map-call-mismatch";return false;}
 // The stock reset call takes game_ctrl.gps_manager at +0x4128. The stock
 // world-map branch obtains game_ctrl from this RIP-relative global.
 if((image.get<uint32_t>(base+0x101561f)&0xffffff)!=0x058b48){diagnostic.reason="controller-global-mismatch";return false;}if(image.get<uint8_t>(base+0x49cb22)!=0xe8||base+0x49cb27+image.get<int32_t>(base+0x49cb23)!=base+0x4fc070||image.get<uint32_t>(base+0x49cb1e)!=0x4128){diagnostic.reason="gps-call-offset-mismatch";return false;}
 const auto global=base+0x1015626+image.get<int32_t>(base+0x1015622),game=image.get<uintptr_t>(global);
 return copilot_clear_native_gps(game,(CopilotGpsReset)(base+0x4fc070),image);
}
inline bool(*copilotRouteExecute)()=&copilot_native_clear_route;
static void copilot_request_clear_route(unsigned slot,uint64_t trace){
 if(zonarJobActive||!g_tripPresent){touchDiagnostics.note_for(trace,TouchStage::ActionRejected,"Clear Destination unavailable: job destination or no route");return;}
 const auto page=managed_page_visual(slot);std::lock_guard<std::mutex> lock(copilotRouteMutex);copilotRoutePending={slot,vehicleStart.epoch.load(),managedPower[slot].serial.load(),page.serial,trace,GetTickCount64()};
}
static void copilot_clear_route_frame(){
 CopilotRouteRequest request;{std::lock_guard<std::mutex> lock(copilotRouteMutex);request=copilotRoutePending;copilotRoutePending={};}if(request.slot>=managedDeviceCount)return;
 const auto slot=request.slot;const auto d=managed_snapshot(slot);const auto page=managed_page_visual(slot);const auto now=GetTickCount64();
 const bool valid=touch_active()&&vehicleStart.eligible&&request.epoch==vehicleStart.epoch&&request.power==managedPower[slot].serial&&request.page==page.serial&&!page.transition&&page.page!=DevicePage::Home&&d.used&&d.enabled&&managed_is_copilot(d)&&d.style==3&&d.vehicle==managed_vehicle()&&managed_power_target(slot)&&!zonarJobActive&&g_tripPresent&&now>=request.at&&now-request.at<=1500&&copilotMenu.get(slot).blocked();
 const bool cleared=valid&&copilotRouteExecute&&copilotRouteExecute();const auto& diagnostic=copilotRouteDiagnostic;
 touchDiagnostics.note_for(request.trace,cleared?TouchStage::Action:TouchStage::ActionRejected,"Clear Destination valid=%u reason=%s mode=%u task=%p owner=%p method=%p",unsigned(valid),valid?diagnostic.reason:"context-changed",diagnostic.mode,(void*)diagnostic.task,(void*)diagnostic.controller,(void*)diagnostic.method);
 log(SCS_LOG_TYPE_message,"[ClearRoute] valid=%u reason=%s active=%u epoch=%u power=%u page=%u transition=%d job=%u trip=%u menu=%u age=%llu native-mode=%u game=%p task=%p owner=%p method=%p",unsigned(valid),valid?diagnostic.reason:"context-changed",unsigned(touch_active()),unsigned(request.epoch==vehicleStart.epoch),unsigned(request.power==managedPower[slot].serial),unsigned(request.page==page.serial),page.transition,unsigned(zonarJobActive.load()),unsigned(g_tripPresent.load()),unsigned(copilotMenu.get(slot).blocked()),(unsigned long long)(now-request.at),diagnostic.mode,(void*)diagnostic.game,(void*)diagnostic.task,(void*)diagnostic.controller,(void*)diagnostic.method);if(cleared)copilotMenu.request(slot,false,now);
}
