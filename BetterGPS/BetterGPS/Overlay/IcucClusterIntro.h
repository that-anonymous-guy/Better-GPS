#pragma once
#include "T680ClusterIntro.h"
namespace bettergps_overlay {
inline constexpr unsigned icucClusterWidth=456,icucClusterHeight=600,icuc5700ClusterWidth=1072,icuc5700ClusterHeight=300,intlClusterWidth=808,intlClusterHeight=390;
inline constexpr int icucClusterBlackMs=2000,icucClusterStillMs=5000,icucClusterTotalMs=7000;
// Same first-visible-upload, contact/session and pause semantics as the T680.
inline ClusterIntroClock icucClusterClock;
inline std::mutex icucClusterMutex;
inline std::atomic<bool> icucClusterAssetReady{false},icuc49ClusterAssetReady{false},icuc5700ClusterAssetReady{false},intlClusterAssetReady{false},icucClusterQueued{false};
inline std::shared_ptr<const std::vector<uint8_t>> icucClusterArtwork,icuc49ClusterArtwork,icuc5700ClusterArtwork,intlClusterArtwork;
inline std::string icucClusterVehicle;
inline const char* icuc_cluster_ui(const std::string& v){return managed_international_vehicle(v)?(v=="intnational.lt"?"/ui/dashboard/international_lt.sii":"/ui/dashboard/international_lonestar.sii"):v=="westernstar.5700xe"?"/ui/dashboard/westernstar_5700xe.sii":v=="westernstar.49x"?"/ui/dashboard/westernstar_49x.sii":v=="freightliner.cascadia2019"?"/ui/dashboard/freightliner_cascadia_2019.sii":"/ui/dashboard/freightliner_cascadia_2024_gauge.sii";}
inline bool icuc_cluster_ready(const std::string& v){return managed_international_vehicle(v)?intlClusterAssetReady.load():v=="westernstar.5700xe"?icuc5700ClusterAssetReady.load():v=="westernstar.49x"?icuc49ClusterAssetReady.load():icucClusterAssetReady.load();}
inline bool icuc_cluster_vehicle(const std::string& vehicle){return managed_international_vehicle(vehicle)||managed_zonar_vehicle(vehicle);}
inline int icuc_cluster_kind(const std::string& v){return managed_international_vehicle(v)?2:v=="westernstar.5700xe"?1:0;}
inline bool icuc_cluster_guard_needed(int kind=0){const auto vehicle=managed_vehicle();return (kind==2?intlClusterAssetReady.load():kind==1?icuc5700ClusterAssetReady.load():icuc_cluster_ready(vehicle))&&displayGlobalEnabled&&(vehicle.empty()||icuc_cluster_vehicle(vehicle)&&kind==icuc_cluster_kind(vehicle));}
inline int icuc_cluster_black_ms(const std::string& v){return v=="westernstar.5700xe"||managed_international_vehicle(v)?0:icucClusterBlackMs;}
inline int icuc_cluster_total_ms(const std::string& v){return managed_international_vehicle(v)?3000:v=="westernstar.5700xe"?2000:icucClusterTotalMs;}
inline bool icuc_cluster_powered(){return icuc_cluster_ready(managed_vehicle())&&displayGlobalEnabled&&vehicleStart.eligible&&displayIgnition==1&&icuc_cluster_vehicle(managed_vehicle());}
// Called under icucClusterMutex by both the renderer and first-bind guard.
inline int icuc_cluster_sample(uint64_t now){const auto vehicle=managed_vehicle();if(icucClusterVehicle!=vehicle){icucClusterVehicle=vehicle;icucClusterClock.on=false;icucClusterClock.shown=icucClusterClock.paused=0;++icucClusterClock.serial;}return icucClusterClock.sample(now,phoneSessionSerial,vehicleStart.epoch,icuc_cluster_powered());}
inline unsigned icuc_cluster_alpha(int phase){return phase<0||phase>=icucClusterTotalMs?0:255;}
inline void icuc_cluster_pause(uint64_t now){std::lock_guard<std::mutex> lock(icucClusterMutex);icucClusterClock.pause(now);}
}
