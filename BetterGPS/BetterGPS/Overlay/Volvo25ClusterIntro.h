#pragma once
#include "T680ClusterIntro.h"
namespace bettergps_overlay {
inline constexpr unsigned volvo25ClusterWidth=1604,volvo25ClusterHeight=600,volvo25ClusterFrames=150;
inline constexpr int volvo25ClusterMovieMs=5000,volvo25ClusterFadeMs=500,volvo25ClusterTotalMs=5500;
inline ClusterIntroClock volvo25ClusterClock;
inline std::mutex volvo25ClusterMutex;
inline std::atomic<bool> volvo25ClusterAssetReady{false},volvo25ClusterQueued{false};
inline std::shared_ptr<const ClusterMovie> volvo25ClusterAnimation;
inline bool volvo25_cluster_guard_needed(){const auto v=managed_vehicle();return volvo25ClusterAssetReady&&displayGlobalEnabled&&(v.empty()||v=="volvo.vnl2025");}
inline bool volvo25_cluster_powered(){return volvo25ClusterAssetReady&&displayGlobalEnabled&&vehicleStart.eligible&&displayIgnition==1&&managed_vehicle()=="volvo.vnl2025";}
inline unsigned volvo25_cluster_alpha(int phase){return phase<0||phase>=volvo25ClusterTotalMs?0:phase<volvo25ClusterMovieMs?255:unsigned(volvo25ClusterTotalMs-phase)*255/volvo25ClusterFadeMs;}
inline void volvo25_cluster_pause(uint64_t now){std::lock_guard<std::mutex> lock(volvo25ClusterMutex);volvo25ClusterClock.pause(now);}
inline bool volvo25_boot_guard_needed(){if(!displayGlobalEnabled)return false;const auto v=managed_vehicle();if(!v.empty()&&v!="volvo.vnl2025")return false;std::lock_guard<std::mutex> lock(managedMutex);for(const auto& d:managedDevices)if(d.used&&managed_is_volvo25(d)&&(v.empty()||d.vehicle==v)&&!d.enabled)return false;return true;}
}
