#pragma once
#include "PhoneStartup.h"
#include <filesystem>
#include <fstream>
#include <mutex>
namespace bettergps_overlay {
// Global policy; the temporary hotkey override deliberately does not survive a session.
inline std::atomic<int> displayPowerMode{0},displayPowerKey{VK_F9},displayPowerMods{1};
inline std::atomic<int> settingsMenuKey{VK_F9},settingsMenuMods{0};
inline std::atomic<int> displayBootHome{1},displayBootHomeSeconds{2},displayPageKey{VK_F9},displayPageMods{2};
inline std::atomic<int> cursorOnlyKey{VK_RBUTTON},cursorOnlyMods{0},cursorOnlyToggle{0},cursorOnlyDoubleTap{1};
inline bool cursor_shortcut_matches(int key,int mods){return key&&key==cursorOnlyKey&&mods==cursorOnlyMods;}
inline std::atomic<int> displayAutoUnlock{1};
inline bool (*devicePageShortcutConflict)(int,int)=nullptr;
inline std::atomic<int> displayIgnition{-1},displayManualPower{-1};
inline std::atomic<bool> displayPowerEditingProfile{false};
inline std::array<int,4> displayPowerDefaults{0,VK_F9,1,5};
inline std::atomic<uint64_t> displayPowerRevision{0};
inline std::mutex displayPowerConfigMutex;
inline std::filesystem::path displayPowerConfigPath;
inline uint64_t displayPowerSavedRevision=~uint64_t(0);
inline bool shortcut_key_valid(int key,int mods){return key>=VK_SPACE&&key<=254&&key!=VK_LWIN&&key!=VK_RWIN&&key!=VK_APPS&&key!=VK_LSHIFT&&key!=VK_RSHIFT&&key!=VK_LCONTROL&&key!=VK_RCONTROL&&key!=VK_LMENU&&key!=VK_RMENU&&mods>=0&&mods<=7;}
inline bool display_key_valid(int key,int mods){return shortcut_key_valid(key,mods)&&!cursor_shortcut_matches(key,mods)&&!(key==settingsMenuKey&&mods==settingsMenuMods)&&!(key==displayPageKey&&mods==displayPageMods)&&(!devicePageShortcutConflict||!devicePageShortcutConflict(key,mods));}
inline bool settings_menu_shortcut(int key,int mods){std::lock_guard<std::mutex> lock(displayPowerConfigMutex);if(!shortcut_key_valid(key,mods)||cursor_shortcut_matches(key,mods)||(key==displayPowerKey&&mods==displayPowerMods)||(key==displayPageKey&&mods==displayPageMods)||(devicePageShortcutConflict&&devicePageShortcutConflict(key,mods)))return false;settingsMenuKey=key;settingsMenuMods=mods;++displayPowerRevision;return true;}
inline bool cursor_key_valid(int key,int mods){return shortcut_key_valid(key,mods)||((key==VK_RBUTTON||key==VK_MBUTTON||key==VK_XBUTTON1||key==VK_XBUTTON2)&&mods>=0&&mods<=7);}
inline bool cursor_shortcut_valid(int key,int mods){return cursor_key_valid(key,mods)&&!(key==settingsMenuKey&&mods==settingsMenuMods)&&!(key==displayPowerKey&&mods==displayPowerMods)&&!(key==displayPageKey&&mods==displayPageMods)&&(!devicePageShortcutConflict||!devicePageShortcutConflict(key,mods));}
inline bool cursor_shortcut_set(int key,int mods){std::lock_guard<std::mutex> lock(displayPowerConfigMutex);if(key&&!cursor_shortcut_valid(key,mods))return false;cursorOnlyKey=key;cursorOnlyMods=key?mods:0;++displayPowerRevision;return true;}
inline bool display_power_target(){const int manual=displayManualPower.load();return manual>=0?manual!=0:displayPowerMode==0||(displayPowerMode==1&&displayIgnition!=0);}
inline void display_power_refresh(uint64_t now=GetTickCount64()){if(!displayPowerEditingProfile)phone_power(display_power_target(),now);}
inline void display_ignition(bool on,uint64_t now=GetTickCount64()){displayIgnition=on?1:0;phoneStartupReady=true;display_power_refresh(now);}
inline void display_power_mode(int mode,uint64_t now=GetTickCount64()){
 {std::lock_guard<std::mutex> lock(displayPowerConfigMutex);displayPowerMode=std::clamp(mode,0,2);displayManualPower=-1;++displayPowerRevision;}
 display_power_refresh(now);
}
inline void display_power_toggle(uint64_t now=GetTickCount64()){displayManualPower=display_power_target()?0:1;++displayPowerRevision;display_power_refresh(now);}
inline void display_first_delay_adjust(int delta){phoneFirstDelaySeconds=std::clamp(phoneFirstDelaySeconds.load()+delta,0,120);++displayPowerRevision;}
inline void display_power_shortcut(int key,int mods){std::lock_guard<std::mutex> lock(displayPowerConfigMutex);if(key&& !display_key_valid(key,mods))return;displayPowerKey=key;displayPowerMods=key?mods:0;++displayPowerRevision;}
inline void display_power_load(const std::filesystem::path& path){
 {std::lock_guard<std::mutex> lock(displayPowerConfigMutex);displayPowerConfigPath=path;displayPowerEditingProfile=false;displayPowerMode=0;displayPowerKey=VK_F9;displayPowerMods=1;settingsMenuKey=VK_F9;settingsMenuMods=0;displayManualPower=-1;phoneFirstDelaySeconds=5;phoneStartupReady=displayIgnition.load()>=0;cursorOnlyKey=VK_RBUTTON;cursorOnlyMods=0;cursorOnlyToggle=0;cursorOnlyDoubleTap=1;int shortcutVersion=0;
  std::ifstream f(path);std::string line;while(std::getline(f,line)){auto pos=line.find('=');if(pos==std::string::npos)continue;auto key=line.substr(0,pos),value=line.substr(pos+1);try{size_t end=0;const int n=std::stoi(value,&end);if(end!=value.size()&&value.substr(end)!="\r")continue;if(key=="FirstStartupDelaySeconds"&&n>=0&&n<=120)phoneFirstDelaySeconds=n;else if(key=="Mode"&&n>=0&&n<=2)displayPowerMode=n;else if(key=="ToggleKey")displayPowerKey=n;else if(key=="ToggleModifiers")displayPowerMods=n;else if(key=="MenuKey")settingsMenuKey=n;else if(key=="MenuModifiers")settingsMenuMods=n;else if(key=="CursorKey")cursorOnlyKey=n;else if(key=="CursorModifiers")cursorOnlyMods=n;else if(key=="CursorToggle")cursorOnlyToggle=std::clamp(n,0,1);else if(key=="CursorDoubleTap")cursorOnlyDoubleTap=std::clamp(n,0,1);else if(key=="ShortcutDefaultsVersion")shortcutVersion=n;}catch(...){}}
  if(shortcutVersion<2&&settingsMenuKey==VK_F9&&settingsMenuMods==2)settingsMenuMods=0;
  if(!shortcut_key_valid(settingsMenuKey,settingsMenuMods)){settingsMenuKey=VK_F9;settingsMenuMods=0;}
  if(displayPowerKey!=0&&!display_key_valid(displayPowerKey,displayPowerMods)){displayPowerKey=VK_F9;displayPowerMods=1;if(!display_key_valid(displayPowerKey,displayPowerMods))displayPowerKey=0;}if(!displayPowerKey)displayPowerMods=0;if(cursorOnlyKey&&!cursor_shortcut_valid(cursorOnlyKey,cursorOnlyMods)){cursorOnlyKey=VK_RBUTTON;cursorOnlyMods=0;}if(!cursorOnlyKey)cursorOnlyMods=0;++displayPowerRevision;displayPowerSavedRevision=~uint64_t(0);
 displayPowerDefaults={displayPowerMode.load(),displayPowerKey.load(),displayPowerMods.load(),phoneFirstDelaySeconds.load()};}display_power_refresh();
}
inline bool display_power_save(){std::lock_guard<std::mutex> lock(displayPowerConfigMutex);const auto revision=displayPowerRevision.load();if(displayPowerConfigPath.empty()||revision==displayPowerSavedRevision)return true;auto temp=displayPowerConfigPath;temp+=L".tmp";std::ofstream f(temp);if(!f)return false;f<<"; BetterGPS screen power: 0=Always active, 1=Ignition only, 2=Off\nMode="<<(displayPowerEditingProfile?displayPowerDefaults[0]:displayPowerMode.load())<<"\n; First phone startup after loading, seconds (0..120)\nFirstStartupDelaySeconds="<<(displayPowerEditingProfile?displayPowerDefaults[3]:phoneFirstDelaySeconds.load())<<"\n; Windows virtual key; modifiers: 1=Shift, 2=Ctrl, 4=Alt; key 0 disables shortcut\nToggleKey="<<(displayPowerEditingProfile?displayPowerDefaults[1]:displayPowerKey.load())<<"\nToggleModifiers="<<(displayPowerEditingProfile?displayPowerDefaults[2]:displayPowerMods.load())<<"\n; Menu shortcut (required); default F9 = key 120, modifiers 0\nMenuKey="<<settingsMenuKey.load()<<"\nMenuModifiers="<<settingsMenuMods.load()<<"\nShortcutDefaultsVersion=2\n; Cursor only: Toggle 0=hold, 1=toggle; default right mouse button\nCursorKey="<<cursorOnlyKey.load()<<"\nCursorModifiers="<<cursorOnlyMods.load()<<"\nCursorToggle="<<cursorOnlyToggle.load()<<"\n; Double press in Hold mode locks cursor until next press; default enabled\nCursorDoubleTap="<<cursorOnlyDoubleTap.load()<<'\n';f.close();if(!f||!MoveFileExW(temp.c_str(),displayPowerConfigPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;displayPowerSavedRevision=revision;return true;}
#ifdef BETTERGPS_TOUCH_TEST
inline int displayFixtureModifiers=-1;
#endif
inline int display_key_modifiers(){
#ifdef BETTERGPS_TOUCH_TEST
 if(displayFixtureModifiers>=0)return displayFixtureModifiers;
#endif
 return ((GetAsyncKeyState(VK_SHIFT)&0x8000)?1:0)|((GetAsyncKeyState(VK_CONTROL)&0x8000)?2:0)|((GetAsyncKeyState(VK_MENU)&0x8000)?4:0);}
inline std::string display_key_name(int key=displayPowerKey.load(),int mods=displayPowerMods.load()){if(!key)return "Disabled";std::string name;if(mods&2)name+="Ctrl+";if(mods&1)name+="Shift+";if(mods&4)name+="Alt+";if(key==VK_RBUTTON)return name+"Right mouse";if(key==VK_MBUTTON)return name+"Middle mouse";if(key==VK_XBUTTON1)return name+"Mouse X1";if(key==VK_XBUTTON2)return name+"Mouse X2";char text[64]{};UINT scan=MapVirtualKeyA(key,MAPVK_VK_TO_VSC);if(key==VK_INSERT||key==VK_DELETE||key==VK_HOME||key==VK_END||key==VK_PRIOR||key==VK_NEXT||(key>=VK_LEFT&&key<=VK_DOWN)||key==VK_DIVIDE||key==VK_NUMLOCK)scan|=0x100;GetKeyNameTextA(LONG(scan<<16),text,sizeof(text));return name+(text[0]?std::string(text):"Key "+std::to_string(key));}
}
