#pragma once
#include "DeviceLimits.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
namespace bettergps_overlay {
// Temporary exploration, never serialized into the user's camera profiles.
// UI-space drag deltas are projected onto the native map's horizontal plane.
struct MapPanRuntime {
 struct State {bool allowed=false,active=false,pose=false;uint64_t epoch=0,power=0,source=0,at=0;double targetX=0,targetZ=0,height=0,back=0,rx=1,rz=0,bx=0,bz=1,anchorX=0,anchorZ=0,x=0,z=0;};
 std::mutex mutex;std::array<State,2+managedDeviceCount> states{};std::array<std::atomic<bool>,2+managedDeviceCount> enabled{};
 void reset_all(){std::lock_guard<std::mutex> lock(mutex);for(size_t i=0;i<states.size();++i){states[i]={};enabled[i]=false;}}
 void context(unsigned device,bool allowed,uint64_t source,uint64_t epoch,uint64_t power){
  if(device>=states.size()||!allowed&&!enabled[device].load())return;
  std::lock_guard<std::mutex> lock(mutex);auto& s=states[device];if(!allowed||s.source!=source||s.epoch!=epoch||s.power!=power)s={};s.allowed=allowed;s.source=source;s.epoch=epoch;s.power=power;enabled[device]=allowed;
 }
 bool active(unsigned device,uint64_t source,uint64_t epoch,uint64_t power){
  if(device>=states.size()||!enabled[device])return false;std::lock_guard<std::mutex> lock(mutex);const auto& s=states[device];return s.allowed&&s.active&&s.source==source&&s.epoch==epoch&&s.power==power;
 }
 bool recenter(unsigned device){if(device>=states.size()||!enabled[device])return false;std::lock_guard<std::mutex> lock(mutex);auto& s=states[device];const bool changed=s.active;s.active=false;s.x=s.z=0;return changed;}
 // Called on the native map update, after the saved camera offsets. Keep the
 // stock player/packed-center data intact; translate only the final camera.
 bool frame(unsigned device,uint64_t source,uint64_t now,const std::array<float,3>& target,float* camera,double rx,double rz,double bx,double bz,double back){
  if(device>=states.size()||!enabled[device])return false;
  const double height=double(camera[1])-target[1];if(!std::isfinite(height)||height<=.001||!std::isfinite(back))return false;
  std::lock_guard<std::mutex> lock(mutex);auto& s=states[device];if(!s.allowed)return false;if(s.source!=source){s.active=false;s.x=s.z=0;s.source=source;}
  s.pose=true;s.at=now;s.targetX=target[0];s.targetZ=target[2];s.height=height;s.back=back;s.rx=rx;s.rz=rz;s.bx=bx;s.bz=bz;
  if(!s.active)return false;camera[0]+=float(s.anchorX-s.targetX+s.x);camera[2]+=float(s.anchorZ-s.targetZ+s.z);return true;
 }
 static bool plane(const State& s,double u,double v,double aspect,double& x,double& z){
  if(!std::isfinite(u)||!std::isfinite(v)||!std::isfinite(aspect)||aspect<=0)return false;
  const double length=std::hypot(s.height,s.back);if(length<.001)return false;const double sn=s.height/length,cs=s.back/length,sx=(u-.5)*2,sy=(.5-v)*2*aspect,dy=sy*cs-sn;
  if(dy>=-.08)return false;const double distance=s.height/-dy;if(!std::isfinite(distance)||distance>s.height*20)return false;
  const double along=sy*sn-cs;x=distance*(s.rx*sx+s.bx*along);z=distance*(s.rz*sx+s.bz*along);return std::isfinite(x)&&std::isfinite(z);
 }
 bool drag(unsigned device,uint64_t source,uint64_t epoch,uint64_t power,uint64_t now,double u0,double v0,double u1,double v1,double aspect){
  if(device>=states.size()||!enabled[device])return false;std::lock_guard<std::mutex> lock(mutex);auto& s=states[device];if(!s.allowed||!s.pose||s.source!=source||s.epoch!=epoch||s.power!=power||now<s.at||now-s.at>500)return false;
  double x0,z0,x1,z1;if(!plane(s,u0,v0,aspect,x0,z0)||!plane(s,u1,v1,aspect,x1,z1))return false;
  const double dx=x0-x1,dz=z0-z1;if(std::hypot(dx,dz)<.00001)return false;
  if(!s.active){s.anchorX=s.targetX;s.anchorZ=s.targetZ;s.x=s.z=0;s.active=true;}
  // Exploration remains bounded to the existing camera-offset envelope.
  s.x=std::clamp(s.x+dx,-5000.,5000.);s.z=std::clamp(s.z+dz,-5000.,5000.);return true;
 }
};
inline MapPanRuntime mapPan;
}
