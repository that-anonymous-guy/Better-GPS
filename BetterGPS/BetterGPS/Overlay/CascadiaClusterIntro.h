#pragma once
#include "T680ClusterIntro.h"
namespace bettergps_overlay {
inline constexpr unsigned icc5ClusterWidth=1288,icc5ClusterHeight=480,icc5ClusterFrames=300;
inline constexpr int icc5ClusterMovieMs=(icc5ClusterFrames*1000+29)/30,icc5ClusterTotalMs=icc5ClusterMovieMs;
inline ClusterIntroClock icc5ClusterClock;
inline std::mutex icc5ClusterMutex;
inline std::atomic<bool> icc5ClusterAssetReady{false},icc5WesternClusterAssetReady{false},icc5ClusterQueued{false};
inline std::shared_ptr<const ClusterMovie> icc5ClusterAnimation,icc5WesternClusterAnimation;
inline std::string icc5ClusterVehicle;
inline bool icc5_cluster_ready(){return managed_vehicle()=="westernstar.57x"?icc5WesternClusterAssetReady.load():icc5ClusterAssetReady.load();}
inline bool icc5_cluster_guard_needed(){const auto v=managed_vehicle();return icc5_cluster_ready()&&displayGlobalEnabled&&(v.empty()||managed_icc5_vehicle(v));}
inline bool icc5_cluster_powered(){return icc5_cluster_ready()&&displayGlobalEnabled&&vehicleStart.eligible&&displayIgnition==1&&managed_icc5_vehicle(managed_vehicle());}
// Shared by preparation and the first-visible GPU bind, under the clock mutex.
inline int icc5_cluster_sample(uint64_t now){const auto vehicle=managed_vehicle();if(icc5ClusterVehicle!=vehicle){icc5ClusterVehicle=vehicle;icc5ClusterClock.on=false;icc5ClusterClock.shown=icc5ClusterClock.paused=0;++icc5ClusterClock.serial;}return icc5ClusterClock.sample(now,phoneSessionSerial,vehicleStart.epoch,icc5_cluster_powered());}
inline unsigned icc5_cluster_alpha(int phase){return phase<0||phase>=icc5ClusterTotalMs?0:255;}
inline void icc5_cluster_pause(uint64_t now){std::lock_guard<std::mutex> lock(icc5ClusterMutex);icc5ClusterClock.pause(now);}
inline bool icc5_boot_guard_needed(){if(!displayGlobalEnabled)return false;const auto v=managed_vehicle();if(!v.empty()&&!managed_icc5_vehicle(v))return false;std::lock_guard<std::mutex> lock(managedMutex);for(const auto& d:managedDevices)if(d.used&&managed_is_icc5(d)&&(v.empty()||d.vehicle==v)&&!d.enabled)return false;return true;}
}
