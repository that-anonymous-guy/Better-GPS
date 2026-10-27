#pragma once
#include <Windows.h>
#include <MinHook.h>
#include "HookBatch.h"
#include <algorithm>
#include <cstring>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <atomic>
#include <array>
namespace bettergps_overlay {
inline bool settings_capture_mouse_input();
inline bool settings_mouse_foreground();
// ATS polls DirectInput before our WndProc. Capture that mouse-only path rather
// than modifying camera transforms, tracking state, VR poses or game controls.
using MenuMouseState=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,LPVOID);
using MenuMouseData=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
inline std::array<void*,2> menuMouseStateAddresses{},menuMouseDataAddresses{};
inline std::array<MenuMouseState,2> menuMouseStates{};
inline std::array<MenuMouseData,2> menuMouseData{};
inline HMODULE menuMouseModule=nullptr;
inline std::atomic<bool> menuMouseDrainState{false},menuMouseDrainData{false};
inline bool menu_mouse_device(IDirectInputDevice8W* device){if(!device)return false;DIDEVCAPS caps{};caps.dwSize=sizeof(caps);return SUCCEEDED(device->GetCapabilities(&caps))&&GET_DIDEVICE_TYPE(caps.dwDevType)==DI8DEVTYPE_MOUSE;}
inline void menu_mouse_release(){menuMouseDrainState=true;menuMouseDrainData=true;}
template<size_t I>HRESULT STDMETHODCALLTYPE menu_mouse_state(IDirectInputDevice8W* device,DWORD size,LPVOID data){
 const auto result=menuMouseStates[I](device,size,data);
 if(FAILED(result)||!data||(size!=sizeof(DIMOUSESTATE)&&size!=sizeof(DIMOUSESTATE2)))return result;
 const bool active=settings_capture_mouse_input();if(!active&&!menuMouseDrainState.load(std::memory_order_relaxed))return result;
 if(!settings_mouse_foreground()||!menu_mouse_device(device))return result;
 if(active){menuMouseDrainState=true;memset(data,0,size);}else if(menuMouseDrainState.exchange(false))memset(data,0,size);
 return result;
}
template<size_t I>HRESULT STDMETHODCALLTYPE menu_mouse_data(IDirectInputDevice8W* device,DWORD size,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags){
 const auto result=menuMouseData[I](device,size,data,count,flags);
 if(FAILED(result)||!count)return result;
 const bool active=settings_capture_mouse_input();if(!active&&!menuMouseDrainData.load(std::memory_order_relaxed))return result;
 if(!settings_mouse_foreground()||!menu_mouse_device(device))return result;
 if(active)menuMouseDrainData=true;else if(!menuMouseDrainData.exchange(false))return result;
 // Drain even peeked or truncated queues so closing cannot replay old motion.
 DWORD discard=INFINITE;menuMouseData[I](device,size,nullptr,&discard,0);*count=0;return result;
}
inline void menu_mouse_uninstall(){for(auto address:menuMouseStateAddresses)if(address){MH_DisableHook(address);MH_RemoveHook(address);}for(auto address:menuMouseDataAddresses)if(address){MH_DisableHook(address);MH_RemoveHook(address);}menuMouseStateAddresses={};menuMouseDataAddresses={};menuMouseStates={};menuMouseData={};if(menuMouseModule){FreeLibrary(menuMouseModule);menuMouseModule=nullptr;}menuMouseDrainState=menuMouseDrainData=false;}
// An owning installer may collect this complete, disabled cohort and enable it
// with its cursor hooks. Standalone callers still receive active hooks.
inline bool menu_mouse_install(std::vector<void*>* pending=nullptr){
 if(menuMouseModule)return true;
 // Retain an already-loaded input provider; otherwise use the Windows DLL.
 if(!GetModuleHandleExW(0,L"dinput8.dll",&menuMouseModule))menuMouseModule=LoadLibraryExW(L"dinput8.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!menuMouseModule)return false;
 using Factory=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);auto factory=reinterpret_cast<Factory>(GetProcAddress(menuMouseModule,"DirectInput8Create"));if(!factory){menu_mouse_uninstall();return false;}
 size_t states=0,buffered=0;bool ok=true;std::vector<void*> addresses;
 auto probe=[&](REFIID iid){
  IDirectInput8W* input=nullptr;if(FAILED(factory(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,iid,reinterpret_cast<void**>(&input),nullptr)))return;
  // The ANSI/Unicode interfaces share these non-string method signatures. Both
  // implementations are probed because ATS may have created its device earlier.
  IDirectInputDevice8W* mouse=nullptr;if(SUCCEEDED(input->CreateDevice(GUID_SysMouse,&mouse,nullptr))){auto** vtable=*reinterpret_cast<void***>(mouse);
   auto state=vtable[9],data=vtable[10];
   if(std::find(menuMouseStateAddresses.begin(),menuMouseStateAddresses.end(),state)==menuMouseStateAddresses.end()&&states<2){size_t i=states++;menuMouseStateAddresses[i]=state;auto hook=i?&menu_mouse_state<1>:&menu_mouse_state<0>;if(MH_CreateHook(state,reinterpret_cast<void*>(hook),reinterpret_cast<void**>(&menuMouseStates[i]))!=MH_OK){menuMouseStateAddresses[i]=nullptr;ok=false;}else addresses.push_back(state);}
   if(std::find(menuMouseDataAddresses.begin(),menuMouseDataAddresses.end(),data)==menuMouseDataAddresses.end()&&buffered<2){size_t i=buffered++;menuMouseDataAddresses[i]=data;auto hook=i?&menu_mouse_data<1>:&menu_mouse_data<0>;if(MH_CreateHook(data,reinterpret_cast<void*>(hook),reinterpret_cast<void**>(&menuMouseData[i]))!=MH_OK){menuMouseDataAddresses[i]=nullptr;ok=false;}else addresses.push_back(data);}
   mouse->Release();}input->Release();
 };
 probe(IID_IDirectInput8W);probe(IID_IDirectInput8A);
 if(!ok||!states||!buffered){menu_mouse_uninstall();return false;}
 if(pending)pending->insert(pending->end(),addresses.begin(),addresses.end());
 else if(enable_hook_batch(addresses)!=MH_OK){menu_mouse_uninstall();return false;}
 return true;
}
}
