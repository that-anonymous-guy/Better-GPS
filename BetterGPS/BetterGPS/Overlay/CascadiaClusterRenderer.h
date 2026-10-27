#pragma once
static std::filesystem::path icc5ClusterPath,icc5WesternClusterPath;
static bool icc5_cluster_header(const std::filesystem::path& path){
 std::ifstream file(path,std::ios::binary|std::ios::ate);const auto length=file.tellg();uint32_t header[5]{};if(length>=20&&length<=80ll*1024*1024){file.seekg(0);file.read((char*)header,sizeof(header));}
 return bool(file)&&header[0]==0x33434742&&header[1]==icc5ClusterWidth&&header[2]==icc5ClusterHeight&&header[3]==icc5ClusterFrames&&header[4]>0&&header[4]<=icc5ClusterFrames;
}
static void icc5_cluster_load(const std::filesystem::path& root){
 // Read bounded headers only at initialization; decode the selected brand's
 // movie on the existing worker, not on DLL startup or the rendering thread.
 icc5ClusterPath=root/"BetterGPS_assets/material/tomtom/cascadia/icc5/cascadia_icc5_cluster_intro.bga";
 icc5WesternClusterPath=root/"BetterGPS_assets/material/tomtom/westernstar/westernstar_icc5_cluster_intro.bga";
 icc5ClusterAssetReady=icc5_cluster_header(icc5ClusterPath);icc5WesternClusterAssetReady=icc5_cluster_header(icc5WesternClusterPath);icc5ClusterQueued=false;
 log(SCS_LOG_TYPE_message,"[Icc5Cluster] Cascadia=%u 57X=%u; 300 lossless frames/30fps, deferred load, 10s direct cut",unsigned(icc5ClusterAssetReady.load()),unsigned(icc5WesternClusterAssetReady.load()));
}
static bool icc5_cluster_open(){const bool western=managed_vehicle()=="westernstar.57x";auto& animation=western?icc5WesternClusterAnimation:icc5ClusterAnimation;auto& ready=western?icc5WesternClusterAssetReady:icc5ClusterAssetReady;const auto& path=western?icc5WesternClusterPath:icc5ClusterPath;if(!animation&&ready){animation=ClusterMovie::read(path,icc5ClusterWidth,icc5ClusterHeight,icc5ClusterFrames);if(!animation){ready=false;icc5ClusterQueued=false;log(SCS_LOG_TYPE_warning,"[Icc5Cluster] Selected movie validation failed; native instruments retained");}}return bool(animation);}
static void icc5_cluster_prepare(uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(icc5ClusterMutex);phase=icc5_cluster_sample(now);serial=icc5ClusterClock.serial;}
 const bool active=phase>=0&&phase<icc5ClusterTotalMs;icc5ClusterQueued=active;if(!active||!icc5_cluster_open())return;
 const unsigned frame=std::min(icc5ClusterFrames-1,unsigned(phase)*30/1000),alpha=icc5_cluster_alpha(phase);static uint64_t previousSerial=~uint64_t(0);static unsigned previousFrame=~0u,previousAlpha=~0u;static std::vector<uint8_t> decoded;
 if(serial==previousSerial&&frame==previousFrame&&alpha==previousAlpha)return;
 const bool western=managed_vehicle()=="westernstar.57x";const auto& movie=western?icc5WesternClusterAnimation:icc5ClusterAnimation;
 if((serial!=previousSerial||frame!=previousFrame)&&!movie->decode(frame,decoded)){(western?icc5WesternClusterAssetReady:icc5ClusterAssetReady)=false;icc5ClusterQueued=false;return;}
 auto pixels=std::make_shared<std::vector<uint8_t>>(decoded);for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=uint8_t(alpha);
 {std::lock_guard<std::mutex> lock(g_icc5ClusterPrepared.mutex);g_icc5ClusterPrepared.pixels=std::move(pixels);g_icc5ClusterPrepared.powerState=serial;++g_icc5ClusterPrepared.version;}
 previousSerial=serial;previousFrame=frame;previousAlpha=alpha;
}
