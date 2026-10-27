#pragma once
static void vnl_cluster_load(const std::filesystem::path& root){vnlClusterAnimation=ClusterMovie::read(root/"BetterGPS_assets/material/garmin/volvodynafleet/vnl2014_cluster.bga",vnlClusterWidth,vnlClusterHeight,vnlClusterFrames);vnlClusterAssetReady=bool(vnlClusterAnimation);if(!vnlClusterAssetReady)vnlClusterQueued=false;log(vnlClusterAssetReady?SCS_LOG_TYPE_message:SCS_LOG_TYPE_warning,"[VnlCluster] %s",vnlClusterAssetReady?"180 lossless frames/30fps + 500ms fade to live instruments":"Invalid/missing movie; native instruments retained");}
static void vnl_cluster_prepare(uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(vnlClusterMutex);phase=vnlClusterClock.sample(now,phoneSessionSerial,vehicleStart.epoch,vnl_cluster_powered());serial=vnlClusterClock.serial;}
 const bool active=phase>=0&&phase<vnlClusterTotalMs;vnlClusterQueued=active;if(!active||!vnlClusterAnimation)return;
 const unsigned frame=std::min(vnlClusterFrames-1,unsigned(phase)*30/1000),alpha=vnl_cluster_alpha(phase);static uint64_t previousSerial=~uint64_t(0);static unsigned previousFrame=~0u,previousAlpha=~0u;static std::vector<uint8_t> decoded;
 if(serial==previousSerial&&frame==previousFrame&&alpha==previousAlpha)return;
 if((serial!=previousSerial||frame!=previousFrame)&&!vnlClusterAnimation->decode(frame,decoded)){vnlClusterAssetReady=false;vnlClusterQueued=false;return;}
 auto pixels=std::make_shared<std::vector<uint8_t>>(decoded);for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=uint8_t(alpha);
 {std::lock_guard<std::mutex> lock(g_vnlClusterPrepared.mutex);g_vnlClusterPrepared.pixels=std::move(pixels);g_vnlClusterPrepared.powerState=serial;++g_vnlClusterPrepared.version;}
 previousSerial=serial;previousFrame=frame;previousAlpha=alpha;
}
