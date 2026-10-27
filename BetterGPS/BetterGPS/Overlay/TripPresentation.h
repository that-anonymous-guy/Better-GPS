#pragma once
#include <cmath>
#include <cstring>
#include <vector>
#include <array>
#include "NavigationRuntime.h"
#include "VoiceNavigation.h"
#include "generated/PhoneModuleLayout.h"
#include "OverlaySettings.h"
namespace bettergps_overlay {
inline bool trip_present(float distance,bool haveDistance,float time,bool haveTime,const bettergps_navigation::Display& guidance) {
    // SDK distance remains available while semantic guidance is preparing,
    // unavailable or paused. Zero is the native indication of no itinerary.
    if(haveDistance&&std::isfinite(distance)&&distance>=0)return distance>0;
    if(haveTime&&std::isfinite(time)&&time>=0)return time>0;
    return guidance.live&&!guidance.instruction.empty();
}
inline const std::vector<uint8_t>& phone_module_pixels(unsigned id){
 static const auto resources=[](){std::array<std::vector<uint8_t>,512> all;HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&phone_module_pixels),&module);
 for(unsigned id=270;id<512;++id){if((id>=284&&id<340)||(id>=364&&id<400))continue;auto r=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);if(!r)continue;auto size=SizeofResource(module,r);auto bytes=static_cast<const uint8_t*>(LockResource(LoadResource(module,r)));uint32_t w=0,h=0;if(size>=8){memcpy(&w,bytes,4);memcpy(&h,bytes+4,4);}if(w&&h&&size==8+size_t(w)*h*4)all[id].assign(bytes+8,bytes+size);}
 return all;}();return resources[std::min(id,511u)];
}
inline void render_phone_layout(std::vector<uint8_t>& pixels,UINT width,UINT height,bool trip,bool dark=false,const OverlaySettings* settings=nullptr) {
 pixels.assign(size_t(width)*height*4,0);if(width!=360||height!=740)return;
 const auto config=settings?*settings:default_overlay_settings();const unsigned state=(dark?2:0)+(trip?0:1);const auto* modules=waze_style()?wazeModules:phoneModules;
 auto paint=[&](size_t i,Placement placement){const auto& m=modules[i];auto resource=m.resource;if(navigationVoiceMuted&&(i==4||i==5)&&(waze_style()||trip))resource=(waze_style()?362:360)+unsigned(i-4);overlay_blit(pixels,phone_module_pixels(resource),m.width,m.height,placement);};
 for(size_t i=3;i<overlayModuleCount;++i){const auto& m=modules[i];if(!(m.states&(1u<<state)))continue;auto placement=config.items[i];if(waze_style()&&(i==4||i==5)&&!trip)placement=attached_placement(config,i,{282,27,75,140});paint(i,placement);}
 if(waze_style()){
  // Report follows the right-hand controls; menu and compass share their left anchor.
  overlay_blit(pixels,phone_module_pixels(dark?355:354),95,95,attached_placement(config,dark?5:4,{266,trip?550:560,95,95}));
  if(!trip)overlay_blit(pixels,phone_module_pixels(dark?357:356),75,75,attached_placement(config,dark?11:10,{3,79,75,75}));
 }
 // The shared status bar stays above both navigation styles.
 for(size_t i=0;i<3;++i)if(modules[i].states&(1u<<state))paint(i,config.items[i]);
}
inline void render_phone_skin(std::vector<uint8_t>& pixels,UINT width,UINT height,bool trip,bool dark=false){render_phone_layout(pixels,width,height,trip,dark);}
}
