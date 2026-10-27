#pragma once
// Existing preparation worker only; the native instrument producer stays live.
static void mack_cluster_load(const std::filesystem::path& root){
 HomeDecoder decoder;unsigned w=0,h=0;auto source=decoder.dds(root/"BetterGPS_assets/material/tomtom/mack_infotainment/mack_cluster_intro.dds",&w,&h,4096);
 if(source.empty()||!w||!h){mackClusterArtwork.reset();mackClusterAssetReady=false;mackClusterQueued=false;log(SCS_LOG_TYPE_warning,"[MackCluster] Missing/invalid artwork; native instruments retained");return;}
 std::vector<uint8_t> fitted;managed_resize(fitted,source,w,h,mackClusterWidth-4,mackClusterHeight);
 auto pixels=std::make_shared<std::vector<uint8_t>>(size_t(mackClusterWidth)*mackClusterHeight*4,10);for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=255;
 for(unsigned y=0;y<mackClusterHeight;++y)memcpy(pixels->data()+size_t(y)*mackClusterWidth*4,fitted.data()+size_t(y)*(mackClusterWidth-4)*4,(mackClusterWidth-4)*4);
 // The authored intro is an opaque screen, even if its DDS carries alpha.
 for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=255;
 mackClusterArtwork=std::move(pixels);mackClusterAssetReady=true;log(SCS_LOG_TYPE_message,"[MackCluster] Anthem/Pinnacle ready: 3000ms still + 500ms fade to LIVE native instruments");
}
static void mack_cluster_prepare(uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(mackClusterMutex);phase=mackClusterClock.sample(now,phoneSessionSerial,vehicleStart.epoch,mack_cluster_powered());serial=mackClusterClock.serial;}
 const bool active=phase>=0&&phase<mackClusterTotalMs;mackClusterQueued=active;if(!active||!mackClusterArtwork)return;
 const unsigned alpha=mack_cluster_alpha(phase);static uint64_t previousSerial=~uint64_t(0);static unsigned previousAlpha=~0u;
 if(previousSerial==serial&&previousAlpha==alpha)return;
 auto pixels=std::make_shared<std::vector<uint8_t>>(*mackClusterArtwork);for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=uint8_t(alpha);
 {std::lock_guard<std::mutex> lock(g_mackClusterPrepared.mutex);g_mackClusterPrepared.pixels=std::move(pixels);g_mackClusterPrepared.powerState=serial;++g_mackClusterPrepared.version;}
 previousSerial=serial;previousAlpha=alpha;
}
