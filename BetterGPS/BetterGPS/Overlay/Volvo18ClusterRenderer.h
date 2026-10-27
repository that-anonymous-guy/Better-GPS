#pragma once
static void volvo18_cluster_load(const std::filesystem::path& root){volvo18ClusterAnimation=ClusterMovie::read(root/"BetterGPS_assets/material/tomtom/volvo_infotainment/vnl2018_cluster_intro.bga",volvo18ClusterWidth,volvo18ClusterHeight,volvo18ClusterFrames);volvo18ClusterAssetReady=bool(volvo18ClusterAnimation);if(!volvo18ClusterAssetReady)volvo18ClusterQueued=false;log(volvo18ClusterAssetReady?SCS_LOG_TYPE_message:SCS_LOG_TYPE_warning,"[Volvo18Cluster] %s",volvo18ClusterAssetReady?"241 lossless frames/30fps then direct cut to native instruments":"Invalid/missing movie; native instruments retained");}
static void volvo18_cluster_prepare(uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(volvo18ClusterMutex);phase=volvo18ClusterClock.sample(now,phoneSessionSerial,vehicleStart.epoch,volvo18_cluster_powered());serial=volvo18ClusterClock.serial;}
 const bool active=phase>=0&&phase<volvo18ClusterTotalMs;volvo18ClusterQueued=active;if(!active||!volvo18ClusterAnimation)return;
 const unsigned frame=std::min(volvo18ClusterFrames-1,unsigned(phase)*30/1000),alpha=volvo18_cluster_alpha(phase);static uint64_t previousSerial=~uint64_t(0);static unsigned previousFrame=~0u,previousAlpha=~0u;static std::vector<uint8_t> decoded;
 if(serial==previousSerial&&frame==previousFrame&&alpha==previousAlpha)return;
 if((serial!=previousSerial||frame!=previousFrame)&&!volvo18ClusterAnimation->decode(frame,decoded)){volvo18ClusterAssetReady=false;volvo18ClusterQueued=false;return;}
 auto pixels=std::make_shared<std::vector<uint8_t>>(decoded);for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=uint8_t(alpha);
 {std::lock_guard<std::mutex> lock(g_volvo18ClusterPrepared.mutex);g_volvo18ClusterPrepared.pixels=std::move(pixels);g_volvo18ClusterPrepared.powerState=serial;++g_volvo18ClusterPrepared.version;}
 previousSerial=serial;previousFrame=frame;previousAlpha=alpha;
}
