#pragma once
// Existing preparation worker only; the native instrument producer stays live.
static std::shared_ptr<const std::vector<uint8_t>> icuc_cluster_load_art(const std::filesystem::path& path,unsigned contentWidth=450,unsigned canvasWidth=icucClusterWidth,unsigned canvasHeight=icucClusterHeight){
 HomeDecoder decoder;unsigned w=0,h=0;auto source=decoder.dds(path,&w,&h,4096);
 if(source.empty()||!w||!h)return {};
 std::vector<uint8_t> fitted;managed_resize(fitted,source,w,h,contentWidth,canvasHeight);
 auto pixels=std::make_shared<std::vector<uint8_t>>(size_t(canvasWidth)*canvasHeight*4,13);for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=255;
 for(unsigned y=0;y<canvasHeight;++y)memcpy(pixels->data()+size_t(y)*canvasWidth*4,fitted.data()+size_t(y)*contentWidth*4,contentWidth*4);
 // The authored intro is an opaque screen, even if its DDS carries alpha.
 for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=255;
 return pixels;
}
static void icuc_cluster_load(const std::filesystem::path& root){
 {HomeDecoder decoder;unsigned w=0,h=0;const auto logo=decoder.dds(root/"BetterGPS_assets/material/copilot/international/native_logo.dds",&w,&h,4096);if(w==303&&h==333&&!logo.empty()){auto image=std::make_shared<std::vector<uint8_t>>(size_t(intlClusterWidth)*intlClusterHeight*4,15);for(size_t i=3;i<image->size();i+=4)(*image)[i]=255;for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){const auto from=(size_t(y)*w+x)*4,to=(size_t(y+40)*intlClusterWidth+x+249)*4;for(unsigned c=0;c<3;++c)(*image)[to+c]=uint8_t((unsigned(logo[from+c])*logo[from+3]+15u*(255-logo[from+3])+127)/255);}intlClusterArtwork=std::move(image);}intlClusterAssetReady=bool(intlClusterArtwork);}
 icucClusterArtwork=icuc_cluster_load_art(root/"BetterGPS_assets/material/copilot/cascadia/icuc/cascadia_icuc_cluster_splash.dds");icucClusterAssetReady=bool(icucClusterArtwork);
 icuc49ClusterArtwork=icuc_cluster_load_art(root/"BetterGPS_assets/material/copilot/westernstar/western_star_49x_splash_screen.dds");icuc49ClusterAssetReady=bool(icuc49ClusterArtwork);
 icuc5700ClusterArtwork=icuc_cluster_load_art(root/"BetterGPS_assets/material/copilot/westernstar/western_star_5700xe_splash_screen.dds",1065,icuc5700ClusterWidth,icuc5700ClusterHeight);icuc5700ClusterAssetReady=bool(icuc5700ClusterArtwork);icucClusterQueued=false;
 log(SCS_LOG_TYPE_message,"[IcucCluster] Cascadia=%u 49X=%u (2s RGB13 + 5s still); 5700XE=%u (2s still): direct cut",unsigned(icucClusterAssetReady.load()),unsigned(icuc49ClusterAssetReady.load()),unsigned(icuc5700ClusterAssetReady.load()));
}
static void icuc_cluster_prepare(uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(icucClusterMutex);phase=icuc_cluster_sample(now);serial=icucClusterClock.serial;}
 const auto vehicle=managed_vehicle();const auto artwork=managed_international_vehicle(vehicle)?intlClusterArtwork:vehicle=="westernstar.5700xe"?icuc5700ClusterArtwork:vehicle=="westernstar.49x"?icuc49ClusterArtwork:icucClusterArtwork;const bool active=phase>=0&&phase<icuc_cluster_total_ms(vehicle);icucClusterQueued=active;if(!active||!artwork)return;
 const unsigned alpha=255,stage=phase<icuc_cluster_black_ms(vehicle)?0:1;static uint64_t previousSerial=~uint64_t(0);static unsigned previousAlpha=~0u,previousStage=~0u;
 if(previousSerial==serial&&previousAlpha==alpha&&previousStage==stage)return;
 auto pixels=std::make_shared<std::vector<uint8_t>>(*artwork);if(!stage)std::fill(pixels->begin(),pixels->end(),13);for(size_t i=3;i<pixels->size();i+=4)(*pixels)[i]=uint8_t(alpha);
 {auto& image=managed_international_vehicle(vehicle)?g_intlClusterPrepared:vehicle=="westernstar.5700xe"?g_icuc5700ClusterPrepared:g_icucClusterPrepared;std::lock_guard<std::mutex> lock(image.mutex);image.pixels=std::move(pixels);image.powerState=serial;++image.version;}
 previousSerial=serial;previousAlpha=alpha;previousStage=stage;
}
