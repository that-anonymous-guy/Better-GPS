#pragma once
#include <mutex>
#include "ClusterMovie.h"
namespace bettergps_overlay {
inline constexpr unsigned clusterWidth=1028,clusterHeight=512,clusterFrames=270;
inline constexpr int clusterMovieMs=9000,clusterFadeMs=1000,clusterTotalMs=10000;
struct ClusterIntroClock {
 uint64_t session=0,epoch=0,serial=0,shown=0,paused=0;bool on=false;
 int sample(uint64_t now,uint64_t nextSession,uint64_t nextEpoch,bool powered){
  if(session!=nextSession||epoch!=nextEpoch){session=nextSession;epoch=nextEpoch;on=false;shown=paused=0;++serial;}
  if(paused){if(shown)shown+=now>paused?now-paused:0;paused=0;}
  if(on!=powered){on=powered;shown=0;++serial;}
  return !on?-1:shown&&now>=shown?int(std::min<uint64_t>(now-shown,clusterTotalMs)):0;
 }
 bool present(uint64_t imageSerial,uint64_t now){if(!on||imageSerial!=serial)return false;if(!shown)shown=now;return true;}
 void pause(uint64_t now){if(on&&!paused)paused=now;}
 static unsigned frame(int phase){return unsigned(std::clamp(phase,0,clusterMovieMs-1))*30/1000;}
 static uint8_t alpha(int phase){return uint8_t(phase<0||phase>=clusterTotalMs?0:phase<clusterMovieMs?255:(clusterTotalMs-phase)*255/clusterFadeMs);}
};
inline ClusterIntroClock clusterIntroClock;
inline std::mutex clusterIntroMutex;
inline std::atomic<bool> clusterIntroAssetReady{false},clusterIntroQueued{false};
inline std::shared_ptr<const ClusterMovie> clusterIntroAnimation;
inline bool cluster_intro_guard_needed(){const auto vehicle=managed_vehicle();return clusterIntroAssetReady&&displayGlobalEnabled&&(vehicle.empty()||vehicle=="kenworth.t680_2022");}
inline bool cluster_intro_powered(){return clusterIntroAssetReady&&displayGlobalEnabled&&vehicleStart.eligible&&displayIgnition==1&&managed_vehicle()=="kenworth.t680_2022";}
inline void cluster_intro_pause(uint64_t now){std::lock_guard<std::mutex> lock(clusterIntroMutex);clusterIntroClock.pause(now);}
}
