#pragma once
// Included inside SettingsMenu's namespace. Reuse its mouse hooks without
// opening the settings UI, a dock or diagnostic panels.
inline std::atomic<bool> cursorOnlyActive{false},cursorOnlyHeld{false};
inline bool cursorOnlyEngaged=false;
inline RECT cursorOnlyOldClip{};inline POINT cursorOnlyOldPosition{};
// Polling also happens from the mouse hooks: serialize edge/timing state so
// one physical press cannot be counted twice by different callers.
inline std::mutex cursorOnlyGestureMutex;
inline bool cursorOnlyLatched=false,cursorOnlyTapPending=false,cursorOnlySuppressPress=false;
inline uint64_t cursorOnlyTapAt=0;
inline std::array<int,4> cursorOnlyGestureConfig{-1,-1,-1,-1};
inline void cursor_only_clear_gesture(){
 cursorOnlyLatched=cursorOnlyTapPending=cursorOnlySuppressPress=false;cursorOnlyTapAt=0;
}
#ifdef BETTERGPS_TOUCH_TEST
inline int cursorOnlyFixtureDown=-1;
#endif
inline bool cursor_only_sample(bool down,bool available,uint64_t now=GetTickCount64()){
 std::lock_guard<std::mutex> lock(cursorOnlyGestureMutex);
 const bool previous=cursorOnlyHeld.exchange(down);
 const std::array<int,4> config{cursorOnlyKey.load(),cursorOnlyMods.load(),cursorOnlyToggle.load(),cursorOnlyDoubleTap.load()};
 if(config!=cursorOnlyGestureConfig){cursorOnlyGestureConfig=config;cursor_only_clear_gesture();cursorOnlyActive=false;}
 if(!available){cursor_only_clear_gesture();cursorOnlyActive=false;return false;}
 if(!config[2]&&config[3]){
  if(down&&!previous){
   if(cursorOnlyLatched){cursorOnlyLatched=false;cursorOnlySuppressPress=true;cursorOnlyTapPending=false;}
   else if(cursorOnlyTapPending&&now>=cursorOnlyTapAt&&now-cursorOnlyTapAt<=GetDoubleClickTime()){
    cursorOnlyLatched=true;cursorOnlyTapPending=false;
   }else{cursorOnlyTapAt=now;cursorOnlyTapPending=true;}
  }
  if(!down)cursorOnlySuppressPress=false;
  cursorOnlyActive=cursorOnlyLatched||(down&&!cursorOnlySuppressPress);
 }else if(!config[2])cursorOnlyActive=down;
 else if(down&&!previous){bool old=cursorOnlyActive.load();while(!cursorOnlyActive.compare_exchange_weak(old,!old)){}}
 return cursorOnlyActive;
}
inline bool cursor_only_poll(){
 const int key=cursorOnlyKey.load();bool down=key&&(GetAsyncKeyState(key)&0x8000)&&display_key_modifiers()==cursorOnlyMods;
#ifdef BETTERGPS_TOUCH_TEST
 if(cursorOnlyFixtureDown>=0)down=cursorOnlyFixtureDown!=0;
#endif
 return cursor_only_sample(down,key&&!settingsOpen&&!settingsCaptureCursorKey&&settings_mouse_foreground()&&render_gpu_allowed());
}
inline void cursor_only_release(bool restorePosition=true,bool clearGesture=true){
 {std::lock_guard<std::mutex> lock(cursorOnlyGestureMutex);cursorOnlyActive=false;if(clearGesture)cursor_only_clear_gesture();}
 if(!cursorOnlyEngaged)return;cursorOnlyEngaged=false;
 if(settingsTouchCancel)settingsTouchCancel();menu_mouse_release();
 settingsOriginalClipCursor(restorePosition?&cursorOnlyOldClip:nullptr);
 if(restorePosition)settingsOriginalSetCursorPos(cursorOnlyOldPosition.x,cursorOnlyOldPosition.y);
 menu_mouse_release();
}
inline void cursor_only_apply(){
 // A normal release between the two taps must retain its timing candidate.
 if(!cursorOnlyActive){cursor_only_release(settings_mouse_foreground(),false);return;}
 if(!cursorOnlyEngaged){GetClipCursor(&cursorOnlyOldClip);GetCursorPos(&cursorOnlyOldPosition);cursorOnlyEngaged=true;settingsOriginalClipCursor(nullptr);menu_mouse_release();}
 POINT point{};if(GetCursorPos(&point)&&ScreenToClient(settingsWindow,&point)){
  menuCursorX=point.x;menuCursorY=point.y;
  if(settingsTouchPointer)settingsTouchPointer(point,(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0,GetTickCount64());
 }
}
inline int cursor_only_message_key(UINT message,WPARAM value){
 if(message==WM_RBUTTONDOWN||message==WM_RBUTTONUP||message==WM_RBUTTONDBLCLK)return VK_RBUTTON;
 if(message==WM_MBUTTONDOWN||message==WM_MBUTTONUP||message==WM_MBUTTONDBLCLK)return VK_MBUTTON;
 if(message==WM_XBUTTONDOWN||message==WM_XBUTTONUP||message==WM_XBUTTONDBLCLK)return GET_XBUTTON_WPARAM(value)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2;
 if(message==WM_KEYDOWN||message==WM_SYSKEYDOWN||message==WM_KEYUP||message==WM_SYSKEYUP)return int(value);
 return 0;
}
inline bool cursor_only_raw_mouse(LPARAM value){
 RAWINPUTHEADER header{};UINT size=sizeof(header);
 return GetRawInputData(reinterpret_cast<HRAWINPUT>(value),RID_HEADER,&header,&size,sizeof(header))!=UINT(-1)&&header.dwType==RIM_TYPEMOUSE;
}
