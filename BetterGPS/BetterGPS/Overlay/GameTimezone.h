#pragma once
#include "VoiceNavigation.h"
namespace bettergps_overlay {
struct GameTimezoneBindings {uintptr_t controller=0,session=0;};
inline GameTimezoneBindings gameTimezoneBindings;
// One coherent snapshot for all CPU presentations. Bit 0 is validity, bits
// 1..2 the effective mode, bits 8..23 the signed local-minus-economy offset.
inline std::atomic<uint32_t> gameTimezoneState{0};
// Native economy reference zone; world-clock conversion never changes the displayed game time.
inline std::atomic<int> gameTimezoneReferenceMinutes{-480};
inline uint32_t game_timezone_state(bool valid,int mode,int offset){return (valid?1u:0u)|(uint32_t(mode&3)<<1)|(uint32_t(uint16_t(int16_t(offset)))<<8);}
inline int game_timezone_offset(uint32_t state){return (state&1)&&((state>>1)&3)?int(int16_t(uint16_t(state>>8))):0;}
inline int64_t game_display_minutes(int64_t economy,int64_t multiplayer,uint32_t state){
 int64_t local=economy+multiplayer+game_timezone_offset(state);
 // SCS pads timezone-enabled date formatting with a week before taking the
 // weekday. Preserve Sunday at the very beginning of a first game week too.
 if(local<0)local+=((-local+10079)/10080)*10080;return local;
}
inline bool game_timezone_resolve(const MapImage& image){
 gameTimezoneBindings={};gameTimezoneState=0;const auto base=uintptr_t(image.base);
 // Audited ATS 1.61.3.18: the actual local-date formatter reads controller
 // time_zone (+138) minus default_time_zone (+170). The Convoy predicate uses
 // the session's settings word (+25e), otherwise the named g_time_zones cvar.
 const std::pair<size_t,std::vector<uint8_t>> checks[]={
  {0x12e162b,{0x48,0x8b,0x05}},
  {0x12e1654,{0x44,0x8b,0x88,0x38,0x01,0,0,0x48,0x8d,0x90,0x48,0x01,0,0,0x44,0x2b,0x88,0x70,0x01,0,0}},
  {0x5308b0,{0x48,0x83,0xec,0x28,0x48,0x8b,0x0d}},
  {0x5308c0,{0x0f,0xb6,0x41,0x0b,0x2c,0x08,0x3c,0x01}},
  {0x5308ca,{0x48,0x8b,0x41,0x10,0x48,0x85,0xc0}},
  {0x5308d3,{0x80,0xb8,0xc8,0,0,0,0x05}},
  {0x5308dc,{0x48,0x8b,0x51,0x70}},
  {0x5308e7,{0x66,0x39,0x82,0x5e,0x02,0,0}}};
 for(const auto& check:checks)if(!image.contains(base+check.first,check.second.size())||!image.executable(base+check.first)||memcmp(image.base+check.first,check.second.data(),check.second.size()))return false;
 auto slot=[&](size_t at){int32_t displacement=0;memcpy(&displacement,image.base+at+3,4);return base+at+7+displacement;};
 const auto controller=slot(0x12e162b),session=slot(0x5308b4);
 if(!image.contains(controller,8)||!image.contains(session,8))return false;gameTimezoneBindings={controller,session};return true;
}
inline bool game_timezone_read(GameTimezoneBindings bindings,uintptr_t variable,int& mode,int& current,int& reference){
 mode=current=reference=0;double setting=0;
 if(!voice_value(variable,"g_time_zones",setting)||setting!=std::floor(setting)||setting<0||setting>2)return false;mode=int(setting);
 uintptr_t session=0;if(!voice_copy(&session,bindings.session,8))return false;
 if(session){uint8_t state=0;if(!voice_copy(&state,session+0xb,1))return false;
  if(state==8||state==9){uintptr_t config=0;uint8_t kind=0;if(!voice_copy(&config,session+0x10,8))return false;
   if(config){if(!voice_copy(&kind,config+0xc8,1))return false;if(kind==5){uintptr_t settings=0;if(!voice_copy(&settings,session+0x70,8))return false;uint16_t overrideMode=0;if(settings&&!voice_copy(&overrideMode,settings+0x25e,2))return false;if(overrideMode>2)return false;mode=overrideMode;}}
  }
 }
 uintptr_t controller=0;if(!voice_copy(&controller,bindings.controller,8)||!controller||!voice_copy(&current,controller+0x138,4)||!voice_copy(&reference,controller+0x170,4))return false;
 return current>=-1440&&current<=1440&&reference>=-1440&&reference<=1440;
}
}
