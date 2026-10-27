#pragma once
#include "VehicleStart.h"
namespace bettergps_overlay {
// ATS 1.61 Windows x64. All nine integer/pointer ABI slots are forwarded
// unchanged. The observed routine binds/places the player's actor and emits
// [actor] Initialized at its successful completion. No engine data is written.
using PlayerActorInitialize=void(__fastcall*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
inline PlayerActorInitialize playerActorOriginal=nullptr;
inline uintptr_t playerActorAddress=0,playerControllerGlobal=0;
inline bool player_actor_binding(uintptr_t& actor,uintptr_t& truck){
 actor=truck=0;
 if(!map_memory(playerControllerGlobal,8))return false;
 const auto controller=*reinterpret_cast<uintptr_t*>(playerControllerGlobal);
 if(!map_memory(controller,0x31b8))return false;
 actor=*reinterpret_cast<uintptr_t*>(controller+0x31b0);
 if(!map_memory(actor,0x20))return false;
 truck=*reinterpret_cast<uintptr_t*>(actor+0x18);
 return map_memory(truck,8);
}
inline void __fastcall player_actor_initialized(uintptr_t actor,uintptr_t a1,uintptr_t a2,uintptr_t a3,uintptr_t a4,uintptr_t a5,uintptr_t a6,uintptr_t a7,uintptr_t a8){
 playerActorOriginal(actor,a1,a2,a3,a4,a5,a6,a7,a8);
 uintptr_t current=0,truck=0;
 if(a1&&player_actor_binding(current,truck)&&current==actor)vehicleStart.observed(actor,truck);
}
inline bool player_actor_resolve(const MapImage& image){
 playerActorAddress=playerControllerGlobal=0;
 const auto base=uintptr_t(image.base);
 const uint8_t entry[]={0x48,0x8b,0xc4,0x4c,0x89,0x48,0x20,0x55,0x57,0x41,0x55,0x41,0x56,0x48,0x8d,0x68,0xc9};
 const uint8_t binding[]={0x4d,0x8d,0xb7,0xb0,0x31,0,0,0x49,0x83,0x3e,0};
 const uint8_t call[]={0xe8,0x70,0x22,0x10,0};
 if(!image.executable(base+0x5d8b90)||!image.contains(base+0x5d8b90,sizeof(entry))||memcmp(image.base+0x5d8b90,entry,sizeof(entry))||
    !image.contains(base+0x4d6838,sizeof(binding))||memcmp(image.base+0x4d6838,binding,sizeof(binding))||
    !image.contains(base+0x4d691b,sizeof(call))||memcmp(image.base+0x4d691b,call,sizeof(call))||
    !image.contains(base+0x2270698,21)||memcmp(image.base+0x2270698,"[actor] Initialized at",21)||!image.contains(base+0x36bad90,8))return false;
 playerActorAddress=base+0x5d8b90;playerControllerGlobal=base+0x36bad90;return true;
}
inline bool player_actor_install(){
 if(!playerActorAddress||MH_CreateHook(reinterpret_cast<void*>(playerActorAddress),reinterpret_cast<void*>(&player_actor_initialized),reinterpret_cast<void**>(&playerActorOriginal))!=MH_OK)return false;
 if(MH_EnableHook(reinterpret_cast<void*>(playerActorAddress))==MH_OK)return true;
 MH_RemoveHook(reinterpret_cast<void*>(playerActorAddress));playerActorOriginal=nullptr;return false;
}
inline void player_actor_stop(){
 if(playerActorOriginal){MH_DisableHook(reinterpret_cast<void*>(playerActorAddress));MH_RemoveHook(reinterpret_cast<void*>(playerActorAddress));}
 playerActorOriginal=nullptr;playerActorAddress=playerControllerGlobal=0;vehicleStart.eligible=false;
}
}
