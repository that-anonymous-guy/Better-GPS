#pragma once
#include "T680ClusterIntro.h"
namespace bettergps_overlay {
inline constexpr unsigned mackClusterWidth=1284,mackClusterHeight=728;
inline constexpr int mackClusterStillMs=3000,mackClusterFadeMs=500,mackClusterTotalMs=3500;
// Same first-visible-upload, contact/session and pause semantics as the T680.
inline ClusterIntroClock mackClusterClock;
inline std::mutex mackClusterMutex;
inline std::atomic<bool> mackClusterAssetReady{false},mackClusterQueued{false};
inline std::shared_ptr<const std::vector<uint8_t>> mackClusterArtwork;
inline bool mack_cluster_vehicle(const std::string& vehicle){return vehicle=="mack.anthem"||vehicle=="mack.pinnacle";}
inline bool mack_cluster_guard_needed(){const auto vehicle=managed_vehicle();return mackClusterAssetReady&&displayGlobalEnabled&&(vehicle.empty()||mack_cluster_vehicle(vehicle));}
inline bool mack_cluster_powered(){return mackClusterAssetReady&&displayGlobalEnabled&&vehicleStart.eligible&&displayIgnition==1&&mack_cluster_vehicle(managed_vehicle());}
inline unsigned mack_cluster_alpha(int phase){return phase<0||phase>=mackClusterTotalMs?0:phase<mackClusterStillMs?255:unsigned(mackClusterTotalMs-phase)*255/mackClusterFadeMs;}
inline void mack_cluster_pause(uint64_t now){std::lock_guard<std::mutex> lock(mackClusterMutex);mackClusterClock.pause(now);}
}
