#pragma once
#include <Windows.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
namespace bettergps_overlay {
inline std::atomic<bool> navigationVoiceMuted{false};
inline std::atomic<uint64_t> navigationVoiceRevision{0};
// Share one constructor scan with clock/audio policy; no additional startup
// image scan. Voice polling still consumes only its original three entries.
inline constexpr const char* voiceVariableNames[]={"g_voice_navigation","s_navigation_volume","s_navigation_mute","g_time_zones","s_master_volume","s_master_mute"};
inline constexpr size_t voiceVariableCount=sizeof(voiceVariableNames)/sizeof(*voiceVariableNames);
inline std::array<uintptr_t,voiceVariableCount> voiceVariables{};
// Resolve the engine's named static cvar constructors, never a heap scan or fixed RVA.
inline std::array<uintptr_t,voiceVariableCount> voice_resolve(const uint8_t* base,size_t size){
 std::array<uintptr_t,voiceVariableCount> result{};std::array<unsigned,voiceVariableCount> counts{};
 for(size_t at=0;at+19<=size;++at){const auto* p=base+at;if(p[0]!=0x48||p[1]!=0x8d||p[2]!=5||p[7]!=0xba||p[8]!=32||p[9]||p[10]||p[11]||p[12]!=0x4c||p[13]!=0x8d||p[14]!=5)continue;
  int32_t literal=0,name=0;memcpy(&literal,p+15,4);memcpy(&name,p+3,4);const int64_t text=int64_t(at)+19+literal,object=int64_t(at)+7+name-8;
  if(text<0||object<0||uint64_t(object)+0x130>size)continue;
  for(size_t i=0;i<voiceVariableCount;++i){const auto length=strlen(voiceVariableNames[i])+1;if(uint64_t(text)+length<=size&&!memcmp(base+text,voiceVariableNames[i],length)){result[i]=uintptr_t(base+object);++counts[i];}}
 }for(size_t i=0;i<voiceVariableCount;++i)if(counts[i]!=1)result[i]=0;return result;
}
inline bool voice_copy(void* destination,uintptr_t source,size_t size){
 if(!source||!size||source+size<source)return false;MEMORY_BASIC_INFORMATION region{};if(!VirtualQuery((void*)source,&region,sizeof(region))||region.State!=MEM_COMMIT||(region.Protect&(PAGE_NOACCESS|PAGE_GUARD))||source+size>uintptr_t(region.BaseAddress)+region.RegionSize)return false;
 // Profiles can replace cvar override nodes; tolerate a node disappearing.
 __try{memcpy(destination,(void*)source,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool voice_value(uintptr_t root,const char* name,double& value){
 char actual[32]{};if(!voice_copy(actual,root+8,sizeof(actual))||memcmp(actual,name,strlen(name)+1))return false;
 uintptr_t node=root;bool terminal=false;for(unsigned depth=0;depth<16;++depth){uintptr_t next=0;if(!voice_copy(&next,node+0x120,sizeof(next)))return false;if(!next){terminal=true;break;}if(next==node)return false;node=next;}
 if(!terminal)return false;uint8_t useDefault=0;if(!voice_copy(&useDefault,node+0xa1,1))return false;char text[102]{};if(!voice_copy(text,node+(useDefault?0x28:0xb1),101)||!memchr(text,0,101))return false;char* end=nullptr;value=strtod(text,&end);return end!=text&&!*end&&std::isfinite(value);
}
inline bool voice_muted_values(double enabled,double volume,double muted){return enabled<=0||volume<=0||muted>0;}
inline void voice_poll(uint64_t now){
 static uint64_t previous=0;if(previous&&now-previous<200)return;previous=now;
 double enabled=0,volume=0,muted=0;if(!voice_value(voiceVariables[0],voiceVariableNames[0],enabled)||!voice_value(voiceVariables[1],voiceVariableNames[1],volume)||!voice_value(voiceVariables[2],voiceVariableNames[2],muted))return;
 const bool next=voice_muted_values(enabled,volume,muted);if(navigationVoiceMuted.exchange(next)!=next)++navigationVoiceRevision;
}
}
