#pragma once
#include "DisplayPowerSettings.h"
namespace bettergps_overlay {
// The globals used by the menu are an editor for the selected display only.
// Renderers consume these value snapshots, never that mutable editor.
struct DevicePreferences {
 int power=0,key=VK_F9,mods=1,manual=-1,firstDelay=5,duration=3,font=0,shuffle=2;
 int bootHome=1,bootHomeSeconds=2,autoUnlock=1,pageKey=VK_F9,pageMods=2;
 std::array<std::string,4> wallpaper;
 uint64_t seed=0,counter=0,lastPower=~uint64_t(0);
 int64_t dayOrigin=-1,lastDay=-1;
 bool pending=true;
};
inline DevicePreferences device_preferences_from_editor(){
 DevicePreferences p;p.autoUnlock=displayAutoUnlock;p.bootHome=displayBootHome;p.bootHomeSeconds=displayBootHomeSeconds;p.pageKey=displayPageKey;p.pageMods=displayPageMods;p.power=displayPowerMode;p.key=displayPowerKey;p.mods=displayPowerMods;p.manual=displayManualPower;p.firstDelay=phoneFirstDelaySeconds;
 std::lock_guard<std::mutex> lock(homeMutex);p.duration=homeDurationSeconds;p.font=homeClockFont;p.shuffle=homeShuffleMode;p.wallpaper=homeSelected;p.seed=homeShuffleSeed;p.counter=homeShuffleCounter;p.dayOrigin=homeShuffleDayOrigin;p.lastDay=homeShuffleLastDay;p.lastPower=homeShuffleLastPower;p.pending=homeShufflePending;return p;
}
inline void device_preferences_to_editor(const DevicePreferences& p,const std::filesystem::path& path){
 displayAutoUnlock=p.autoUnlock;displayPowerEditingProfile=true;displayBootHome=p.bootHome;displayBootHomeSeconds=p.bootHomeSeconds;displayPageKey=p.pageKey;displayPageMods=p.pageMods;displayPowerMode=p.power;displayPowerKey=p.key;displayPowerMods=p.mods;displayManualPower=p.manual;phoneFirstDelaySeconds=p.firstDelay;++displayPowerRevision;
 std::lock_guard<std::mutex> lock(homeMutex);homeConfig.clear();homeDurationSeconds=p.duration;homeClockFont=p.font;homeShuffleMode=p.shuffle;homeSelected=p.wallpaper;homeShuffleSeed=p.seed;homeShuffleCounter=p.counter;homeShuffleDayOrigin=p.dayOrigin;homeShuffleLastDay=p.lastDay;homeShuffleLastPower=p.lastPower;homeShufflePending=p.pending;++homeRevision;
}
inline bool device_preferences_equal(const DevicePreferences& a,const DevicePreferences& b){return a.autoUnlock==b.autoUnlock&&a.bootHome==b.bootHome&&a.bootHomeSeconds==b.bootHomeSeconds&&a.pageKey==b.pageKey&&a.pageMods==b.pageMods&&a.power==b.power&&a.key==b.key&&a.mods==b.mods&&a.manual==b.manual&&a.firstDelay==b.firstDelay&&a.duration==b.duration&&a.font==b.font&&a.shuffle==b.shuffle&&a.wallpaper==b.wallpaper&&a.seed==b.seed&&a.counter==b.counter&&a.dayOrigin==b.dayOrigin&&a.lastDay==b.lastDay&&a.lastPower==b.lastPower&&a.pending==b.pending;}
inline void device_preferences_write(std::ostream& f,const DevicePreferences& p){f<<"auto_unlock="<<p.autoUnlock<<"\nboot_home="<<p.bootHome<<"\nboot_home_seconds="<<p.bootHomeSeconds<<"\npage_key="<<p.pageKey<<"\npage_modifiers="<<p.pageMods<<"\nprefs_version=2\npower="<<p.power<<"\npower_key="<<p.key<<"\npower_modifiers="<<p.mods<<"\nfirst_delay="<<p.firstDelay<<"\nlock_duration="<<p.duration<<"\nlock_font="<<p.font<<"\nshuffle="<<p.shuffle<<"\nshuffle_seed="<<p.seed<<"\nshuffle_counter="<<p.counter<<"\nshuffle_day_origin="<<p.dayOrigin<<'\n';for(unsigned i=0;i<4;++i)f<<"wallpaper."<<i<<'='<<p.wallpaper[i]<<'\n';}
inline bool device_preferences_read(DevicePreferences& p,const std::string& k,const std::string& v){
 if(k=="auto_unlock")p.autoUnlock=std::clamp(std::stoi(v),0,1);else if(k=="boot_home")p.bootHome=std::clamp(std::stoi(v),0,1);else if(k=="boot_home_seconds")p.bootHomeSeconds=std::clamp(std::stoi(v),0,3600);else if(k=="page_mode"||k=="idle_seconds")return true;else if(k=="page_key")p.pageKey=std::clamp(std::stoi(v),0,254);else if(k=="page_modifiers")p.pageMods=std::clamp(std::stoi(v),0,7);else if(k=="power")p.power=std::clamp(std::stoi(v),0,2);else if(k=="power_key")p.key=std::clamp(std::stoi(v),0,254);else if(k=="power_modifiers")p.mods=std::clamp(std::stoi(v),0,7);else if(k=="first_delay")p.firstDelay=std::clamp(std::stoi(v),0,120);else if(k=="lock_duration")p.duration=std::clamp(std::stoi(v),0,3600);else if(k=="lock_font")p.font=std::clamp(std::stoi(v),0,1);else if(k=="shuffle")p.shuffle=std::clamp(std::stoi(v),0,2);else if(k=="shuffle_seed")p.seed=std::stoull(v);else if(k=="shuffle_counter")p.counter=std::stoull(v);else if(k=="shuffle_day_origin")p.dayOrigin=std::stoll(v);else if(k.rfind("wallpaper.",0)==0){unsigned i=unsigned(std::stoi(k.substr(10)));if(i<4)p.wallpaper[i]=i<2?home_icon_selection(v):v;}else return false;return true;
}
}
