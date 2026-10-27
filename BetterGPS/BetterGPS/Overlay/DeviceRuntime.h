#pragma once
namespace bettergps_overlay {
struct DevicePowerState {
 std::atomic<int> phase{0};std::atomic<bool> on{false};std::atomic<uint64_t> serial{0};
 std::atomic<uint64_t> splashShownAt{0};
 std::atomic<uint64_t> unlockRequest{0};
 uint64_t unlockAt=0;
 std::atomic<uint64_t> session{0},bootEpoch{0};
 uint64_t wait=0,origin=0,off=0,pause=0;bool first=true;int startup=-1;
};
inline std::array<DevicePowerState,managedDeviceCount> managedPower;
inline bool managed_power_target(unsigned i){return i<managedDeviceCount&&managedPower[i].on.load();}
inline bool managed_any_power(){for(auto& p:managedPower)if(p.on)return true;return false;}
inline int managed_power_sample(unsigned i,const ManagedDevice& d,uint64_t now,int ignition=displayIgnition.load()){
 auto& p=managedPower[i];const auto epoch=managed_page_kind(d)!=PageDevice::None?vehicleStart.epoch.load():0;if(p.session!=phoneSessionSerial||p.bootEpoch!=epoch){p.session=phoneSessionSerial.load();p.bootEpoch=epoch;p.on=false;p.wait=p.origin=p.off=p.pause=0;p.first=true;p.startup=-1;p.unlockAt=0;p.unlockRequest=0;}
 if(p.pause){const auto dt=now>p.pause?now-p.pause:0;if(p.wait)p.wait+=dt;if(p.origin)p.origin+=dt;if(p.off)p.off+=dt;if(p.splashShownAt)p.splashShownAt+=dt;if(p.unlockAt)p.unlockAt+=dt;p.pause=0;}
 bool target=d.enabled&&(d.prefs.manual>=0?d.prefs.manual!=0:d.prefs.power==0||(d.prefs.power==1&&ignition!=0));
 const bool ford=d.known>=0&&size_t(d.known)<gpsDisplays.size()&&ford_console_id(gpsDisplays[d.known].id);if(ford&&ignition!=1)target=false;
 if((managed_is_international(d)||managed_is_navplus(d)||managed_is_copilot(d)||managed_is_zonar(d)||managed_is_icc5(d)||managed_is_mack(d)||managed_is_vnl2014(d)||(managed_is_volvo18(d)||managed_is_volvo25(d)))&&d.prefs.manual<0&&d.prefs.power==1&&ignition!=1)target=false;
 const bool previous=p.on.load();if(target&&(!previous||p.startup!=d.startup)){p.wait=now+1;p.origin=p.off=0;p.splashShownAt=0;p.unlockAt=0;p.unlockRequest=0;p.phase=0;++p.serial;}p.on=target;
 p.startup=d.startup;int phase=-2;
 if(target){if(p.first&&p.wait&&now-(p.wait-1)<uint64_t(d.prefs.firstDelay)*1000)phase=0;else{p.first=false;if(!p.origin)p.origin=now+1;phase=d.startup?int(std::min<uint64_t>(now-(p.origin-1),INT_MAX)):startup_total_ms();}}
 else{if(previous)p.off=now+1;if(p.off){const auto dt=now-(p.off-1);if(dt<shutdownHoldMs+shutdownFadeMs)phase=-3-int(dt);}}
 // Count each Garmin splash from its first uploaded frame, not from a
 // worker tick that may precede creation of the native display by seconds.
 if(target&&d.known<0&&managed_visible_splash(d)){const auto shown=p.splashShownAt.load();phase=shown&&now>=shown?int(std::min<uint64_t>(now-shown,INT_MAX)):0;}
 if(target&&d.startup==1&&managed_page_kind(d)!=PageDevice::None&&!d.prefs.autoUnlock&&phase>=startupBlackMs){
  const int swipe=startupBlackMs+400+d.prefs.duration*1000;
  if(!p.unlockAt&&p.unlockRequest.load()==p.serial.load())p.unlockAt=now+1;
  phase=p.unlockAt?std::min(startup_total_ms(),swipe+int(now-(p.unlockAt-1))):std::min(phase,swipe-1);
 }
 p.phase=phase;return phase;
}
inline void managed_power_pause(uint64_t now){managed_pages_pause();for(auto& p:managedPower)if(p.session&&!p.pause)p.pause=now;}
inline void managed_power_input(bool foreground){
 static std::array<bool,managedDeviceCount> held{};const auto vehicle=managed_vehicle();std::set<std::pair<int,int>> consumed;std::array<unsigned,managedDeviceCount> order{};for(unsigned i=0;i<order.size();++i)order[i]=i;const int selected=managedSelected>=0?managedSelected.load():managed_for_known(selectedDisplay.load());if(selected>=0)std::swap(order[0],order[unsigned(selected)]);
 for(unsigned i:order){auto d=managed_snapshot(i);if(!d.used||!d.enabled||d.vehicle!=vehicle||!d.prefs.key)continue;const bool down=(GetAsyncKeyState(d.prefs.key)&0x8000)&&display_key_modifiers()==d.prefs.mods;
  if(down&&!held[i]&&foreground&&consumed.insert({d.prefs.key,d.prefs.mods}).second){std::lock_guard<std::mutex> lock(managedMutex);auto& m=managedDevices[i];m.prefs.manual=managedPower[i].on?0:1;++m.prefsRevision;if(int(i)==managedSelected)displayManualPower=m.prefs.manual;++managedRevision;}
  held[i]=down;
 }
}
}
