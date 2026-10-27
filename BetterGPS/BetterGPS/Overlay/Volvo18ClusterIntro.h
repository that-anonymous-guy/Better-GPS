#pragma once
#include "T680ClusterIntro.h"
namespace bettergps_overlay {
inline constexpr unsigned volvo18ClusterWidth=1328,volvo18ClusterHeight=800,volvo18ClusterFrames=241;
inline constexpr int volvo18ClusterMovieMs=(volvo18ClusterFrames*1000+29)/30,volvo18ClusterTotalMs=volvo18ClusterMovieMs;
inline ClusterIntroClock volvo18ClusterClock;
inline std::mutex volvo18ClusterMutex;
inline std::atomic<bool> volvo18ClusterAssetReady{false},volvo18ClusterQueued{false};
inline std::shared_ptr<const ClusterMovie> volvo18ClusterAnimation;
inline bool volvo18_cluster_guard_needed(){const auto v=managed_vehicle();return volvo18ClusterAssetReady&&displayGlobalEnabled&&(v.empty()||managed_volvo18_vehicle(v));}
inline bool volvo18_cluster_powered(){return volvo18ClusterAssetReady&&displayGlobalEnabled&&vehicleStart.eligible&&displayIgnition==1&&managed_volvo18_vehicle(managed_vehicle());}
inline unsigned volvo18_cluster_alpha(int phase){return phase<0||phase>=volvo18ClusterTotalMs?0:255;}
inline void volvo18_cluster_pause(uint64_t now){std::lock_guard<std::mutex> lock(volvo18ClusterMutex);volvo18ClusterClock.pause(now);}
inline bool volvo18_boot_guard_needed(){if(!displayGlobalEnabled)return false;const auto v=managed_vehicle();if(!v.empty()&&!managed_volvo18_vehicle(v))return false;std::lock_guard<std::mutex> lock(managedMutex);for(const auto& d:managedDevices)if(d.used&&managed_is_volvo18(d)&&(v.empty()||d.vehicle==v)&&!d.enabled)return false;return true;}
}
