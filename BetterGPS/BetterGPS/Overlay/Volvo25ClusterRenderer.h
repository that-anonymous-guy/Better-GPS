#pragma once
static void volvo25_cluster_load(const std::filesystem::path& root){volvo25ClusterAnimation=ClusterMovie::read(root/"BetterGPS_assets/material/sygic/vnl2025/vnl2025_cluster_intro.bga",volvo25ClusterWidth,volvo25ClusterHeight,volvo25ClusterFrames);volvo25ClusterAssetReady=bool(volvo25ClusterAnimation);if(!volvo25ClusterAssetReady)volvo25ClusterQueued=false;log(volvo25ClusterAssetReady?SCS_LOG_TYPE_message:SCS_LOG_TYPE_warning,"[Volvo25Cluster] %s",volvo25ClusterAssetReady?"150 lossless frames/30fps + 500ms fade to live instruments":"Invalid/missing movie; native instruments retained");}
static void volvo25_cluster_prepare(uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(volvo25ClusterMutex);phase=volvo25ClusterClock.sample(now,phoneSessionSerial,vehicleStart.epoch,volvo25_cluster_powered());serial=volvo25ClusterClock.serial;}
 const bool active=phase>=0&&phase<volvo25ClusterTotalMs;volvo25ClusterQueued=active;if(!active||!volvo25ClusterAnimation)return;
 const unsigned frame=std::min(volvo25ClusterFrames-1,unsigned(phase)*30/1000),alpha=volvo25_cluster_alpha(phase);static uint64_t previousSerial=~uint64_t(0);static unsigned previousFrame=~0u,previousAlpha=~0u;static std::vector<uint8_t> decoded;
 if(serial==previousSerial&&frame==previousFrame&&alpha==previousAlpha)return;
 if((serial!=previousSerial||frame!=previousFrame)&&!volvo25ClusterAnimation->decode(frame,decoded)){volvo25ClusterAssetReady=false;volvo25ClusterQueued=false;return;}
 auto pixels=std::make_shared<std::vector<uint8_t>>(decoded);for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=uint8_t(alpha);
 {std::lock_guard<std::mutex> lock(g_volvo25ClusterPrepared.mutex);g_volvo25ClusterPrepared.pixels=std::move(pixels);g_volvo25ClusterPrepared.powerState=serial;++g_volvo25ClusterPrepared.version;}
 previousSerial=serial;previousFrame=frame;previousAlpha=alpha;
}
