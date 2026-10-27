#pragma once
#include "DeviceLimits.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <cmath>
namespace bettergps_overlay {
struct NavPlusImmersiveVisual {
 int mode=0;double hidden=0; // 1 Clock Night, 2 Gallery photo.
 int top()const{return -int(std::lround((mode==2?114:63)*hidden));}
 int bottom()const{return int(std::lround((mode==2?104:61)*hidden));}
};
class NavPlusImmersive {
 struct State {NavPlusImmersiveVisual visual;uint64_t epoch=0,power=0,page=0,lastActivity=0,origin=0;double from=0;bool target=false;};
 std::array<State,managedDeviceCount> states{};std::array<std::atomic<bool>,managedDeviceCount> enabled{};std::atomic<unsigned> count{0};std::mutex mutex;
 static double position(const State& s,uint64_t now){const double t=std::clamp(now>=s.origin?double(now-s.origin)/280:0.,0.,1.),ease=1-(1-t)*(1-t)*(1-t);return s.from+((s.target?1.:0.)-s.from)*ease;}
 static void target(State& s,bool hidden,uint64_t now){if(s.target==hidden)return;s.from=position(s,now);s.origin=now;s.target=hidden;}
public:
 bool active()const{return count.load()!=0;}
 void disable(unsigned slot){if(slot>=managedDeviceCount||!enabled[slot].load())return;std::lock_guard<std::mutex> lock(mutex);if(enabled[slot].exchange(false)){--count;states[slot]={};}}
 NavPlusImmersiveVisual sample(unsigned slot,int mode,uint64_t epoch,uint64_t power,uint64_t page,uint64_t now,bool keepVisible=false){
  if(slot>=managedDeviceCount)return {};if(!mode){disable(slot);return {};}
  std::lock_guard<std::mutex> lock(mutex);auto& s=states[slot];if(!enabled[slot].exchange(true))++count;
  if(s.visual.mode!=mode||s.epoch!=epoch||s.power!=power||s.page!=page){s={};s.visual.mode=mode;s.epoch=epoch;s.power=power;s.page=page;s.lastActivity=s.origin=now;}
  if(keepVisible)s.lastActivity=now;target(s,now>=s.lastActivity&&now-s.lastActivity>=2500,now);s.visual.hidden=position(s,now);return s.visual;
 }
 void activity(unsigned slot,uint64_t now){if(slot>=managedDeviceCount||!enabled[slot].load())return;std::lock_guard<std::mutex> lock(mutex);auto& s=states[slot];s.lastActivity=now;target(s,false,now);s.visual.hidden=position(s,now);}
 bool context(unsigned slot,uint64_t epoch,uint64_t power,uint64_t page){if(slot>=managedDeviceCount||!enabled[slot].load())return false;std::lock_guard<std::mutex> lock(mutex);const auto& s=states[slot];return s.epoch==epoch&&s.power==power&&s.page==page;}
 NavPlusImmersiveVisual visual(unsigned slot){if(slot>=managedDeviceCount||!enabled[slot].load())return {};std::lock_guard<std::mutex> lock(mutex);return states[slot].visual;}
};
inline NavPlusImmersive navPlusImmersive;
inline thread_local NavPlusImmersiveVisual presentationImmersive;
struct NavPlusImmersiveScope {NavPlusImmersiveVisual old=presentationImmersive;explicit NavPlusImmersiveScope(NavPlusImmersiveVisual v){presentationImmersive=v;}~NavPlusImmersiveScope(){presentationImmersive=old;}};
}
