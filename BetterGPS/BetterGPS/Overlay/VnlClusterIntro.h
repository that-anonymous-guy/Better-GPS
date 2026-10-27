#pragma once
#include "T680ClusterIntro.h"
namespace bettergps_overlay {
inline constexpr unsigned vnlClusterWidth=1128,vnlClusterHeight=700,vnlClusterFrames=180;
inline constexpr int vnlClusterMovieMs=6000,vnlClusterFadeMs=500,vnlClusterTotalMs=6500;
inline ClusterIntroClock vnlClusterClock;
inline std::mutex vnlClusterMutex;
inline std::atomic<bool> vnlClusterAssetReady{false},vnlClusterQueued{false};
inline std::shared_ptr<const ClusterMovie> vnlClusterAnimation;
inline bool vnl_cluster_guard_needed(){const auto v=managed_vehicle();return vnlClusterAssetReady&&displayGlobalEnabled&&(v.empty()||v=="volvo.vnl");}
inline bool vnl_cluster_powered(){return vnlClusterAssetReady&&displayGlobalEnabled&&vehicleStart.eligible&&displayIgnition==1&&managed_vehicle()=="volvo.vnl";}
inline unsigned vnl_cluster_alpha(int phase){return phase<0||phase>=vnlClusterTotalMs?0:phase<vnlClusterMovieMs?255:unsigned(vnlClusterTotalMs-phase)*255/vnlClusterFadeMs;}
inline void vnl_cluster_pause(uint64_t now){std::lock_guard<std::mutex> lock(vnlClusterMutex);vnlClusterClock.pause(now);}
inline bool vnl_boot_guard_needed(){if(!displayGlobalEnabled)return false;const auto v=managed_vehicle();if(!v.empty()&&v!="volvo.vnl")return false;std::lock_guard<std::mutex> lock(managedMutex);for(const auto& d:managedDevices)if(d.used&&managed_is_vnl2014(d)&&(v.empty()||d.vehicle==v)&&!d.enabled)return false;return true;}
}
