#pragma once
// Included inside bettergps_overlay after DeviceRenderer, on the existing CPU worker.
static void cluster_intro_load(const std::filesystem::path& root){
 auto path=root/"BetterGPS_assets/material/copilot/t680/t680_clusterintro.bga";std::error_code ec;
 const auto size=std::filesystem::file_size(path,ec);std::shared_ptr<const ClusterMovie> animation;
 if(!ec&&size<=80ull*1024*1024)animation=ClusterMovie::read(path);
 if(animation&&animation->count==clusterFrames){clusterIntroAnimation=std::move(animation);clusterIntroAssetReady=true;log(SCS_LOG_TYPE_message,"[ClusterIntro] T680 2022 lossless BGRA ready: 270 frames/30fps, 9000ms movie + 1000ms fade to live instruments");}
 else{clusterIntroAnimation.reset();clusterIntroAssetReady=false;clusterIntroQueued=false;log(SCS_LOG_TYPE_warning,"[ClusterIntro] Missing/invalid movie; native instruments retained");}
}
static void cluster_intro_prepare(uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(clusterIntroMutex);phase=clusterIntroClock.sample(now,phoneSessionSerial,vehicleStart.epoch,cluster_intro_powered());serial=clusterIntroClock.serial;}
 const bool active=phase>=0&&phase<clusterTotalMs;const bool wasActive=clusterIntroQueued.exchange(active);if(!active){if(wasActive&&phase>=clusterTotalMs)log(SCS_LOG_TYPE_message,"[ClusterIntro] native instruments fully revealed");return;}
 const auto animation=clusterIntroAnimation;if(!animation)return;
 const auto frame=ClusterIntroClock::frame(phase);const auto alpha=ClusterIntroClock::alpha(phase);
 // At most 30 movie decodes/sec; keep the last image during the crossfade.
 static uint64_t previousSerial=~uint64_t(0);static unsigned previousFrame=~0u,previousAlpha=~0u;static std::vector<uint8_t> decoded;
 if(previousSerial==serial&&previousFrame==frame&&previousAlpha==alpha)return;
 if(previousSerial==serial&&previousAlpha==255&&alpha<255)log(SCS_LOG_TYPE_message,"[ClusterIntro] crossfade to live instruments (1000ms)");
 if((previousSerial!=serial||previousFrame!=frame)&&!animation->decode(frame,decoded)){clusterIntroAssetReady=false;clusterIntroQueued=false;log(SCS_LOG_TYPE_warning,"[ClusterIntro] Frame decompression failed; native instruments retained");return;}
 if(decoded.size()!=size_t(clusterWidth)*clusterHeight*4)return;
 auto pixels=std::make_shared<std::vector<uint8_t>>(decoded);
 for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=alpha;
 {std::lock_guard<std::mutex> lock(g_clusterIntroPrepared.mutex);g_clusterIntroPrepared.pixels=std::move(pixels);g_clusterIntroPrepared.powerState=serial;++g_clusterIntroPrepared.version;}
 previousSerial=serial;previousFrame=frame;previousAlpha=alpha;
}
