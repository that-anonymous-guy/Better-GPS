#pragma once
#include <array>
#include <algorithm>
#include <mutex>
#include <cmath>
#include <cstdint>
namespace bettergps_overlay {
inline constexpr double t680GaugeMpgMax=5.*2.3521458333333333; // Stock 5 km/l, US gallons.
struct T680GaugeSnapshot {double oil=100,trans=150,boost=0,torque=0,mpg=0;unsigned valid=0;};
inline unsigned t680_gauge_hot(const T680GaugeSnapshot& s){return ((s.valid&1)&&s.oil>250?1u:0u)|((s.valid&2)&&s.trans>280?2u:0u);}
struct T680GaugeAlert {
 uint64_t epoch=0,power=0;unsigned latched=0,pending=0;
 void sample(const T680GaugeSnapshot& s,uint64_t e,uint64_t p){if(epoch!=e||power!=p){epoch=e;power=p;latched=pending=0;}if(!e)return;
  const unsigned valid=s.valid&3,hot=t680_gauge_hot(s);latched&=~(valid&~hot);pending&=~(valid&~hot);pending|=hot&~latched;latched|=hot;
 }
 int take(const T680GaugeSnapshot& s){const auto ready=pending&t680_gauge_hot(s);if(!ready)return -1;pending&=~ready;return ready&1?0:1;}
};
struct T680GaugeState {
 std::mutex mutex;double oilC=0,litresPerKm=0,diffC=0,turbo=0;bool oilReady=false,mpgReady=false,nativeReady=false;uint64_t epoch=0;
 void reset(){std::lock_guard<std::mutex> lock(mutex);oilReady=mpgReady=nativeReady=false;epoch=0;}
 void sdk(bool oil,bool valid,double value){std::lock_guard<std::mutex> lock(mutex);if(oil){oilReady=valid;if(valid)oilC=value;}else if(valid&&value>0){mpgReady=true;litresPerKm=value;}}
 void native(uint64_t generation,bool valid,double diff=0,double boost=0){std::lock_guard<std::mutex> lock(mutex);epoch=generation;nativeReady=valid;if(valid){diffC=diff;turbo=boost;}}
 T680GaugeSnapshot get(uint64_t generation){std::lock_guard<std::mutex> lock(mutex);T680GaugeSnapshot s;if(!generation||epoch!=generation)return s;
  if(oilReady){s.oil=oilC*1.8+32;s.valid|=1;}if(mpgReady&&litresPerKm>0){s.mpg=2.3521458333333333/litresPerKm;s.valid|=16;}
  // Stock T680 turbo_anim uses its default 0..1 range for the 0..80 dial;
  // its native Torque bar instead uses 0..0.45. Keep those calibrations separate.
  if(nativeReady){s.trans=diffC*1.8+32;s.boost=turbo*80.;s.torque=turbo/.45*100.;s.valid|=6;}return s;
 }
};
inline T680GaugeState t680Gauges;
struct T680GaugeScale {double low,high,start,sweep;};
// Angles use image coordinates (+Y down), measured from +X. The supplied
// needle points left; its hub, not the texture midpoint, is the pivot.
inline constexpr std::array<T680GaugeScale,6> t680GaugeScales{{
 {100,300,158,224},{150,300,158,224},{0,80,180,180},
 {0,1,180,180},{0,t680GaugeMpgMax,180,180},{0,100,160,220}}};
inline double t680_gauge_angle(unsigned gauge,double value){const auto s=t680GaugeScales.at(gauge);if(!std::isfinite(value))value=s.low;return s.start+std::clamp((value-s.low)/(s.high-s.low),0.,1.)*s.sweep;}
inline std::array<double,6> t680_gauge_angles(const T680GaugeSnapshot& s){return {{t680_gauge_angle(0,s.oil),t680_gauge_angle(1,s.trans),t680_gauge_angle(2,s.boost),180,t680_gauge_angle(4,s.mpg),t680_gauge_angle(5,s.torque)}};}
}
