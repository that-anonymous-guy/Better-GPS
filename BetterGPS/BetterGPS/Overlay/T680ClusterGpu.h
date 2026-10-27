#pragma once
// Included inside bettergps_overlay after DeviceGpu. The native UI shader blends
// this RGBA layer over LIVE instruments; no black pass or private UI export.
static const GUID kClusterIntroUpload={0xa751c185,0x5438,0x43e0,{0xb0,0x28,0x18,0x64,0x2c,0x31,0x19,0x50}};
static ID3D11ShaderResourceView* cluster_intro_guard(ID3D11Device* device,int tone){ID3D11DeviceContext* context=nullptr;device->GetImmediateContext(&context);auto* view=managed_black_view(context,tone);context->Release();return view;}
static ID3D11ShaderResourceView* cluster_intro_view(ID3D11DeviceContext* context,uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(clusterIntroMutex);phase=clusterIntroClock.sample(now,phoneSessionSerial,vehicleStart.epoch,cluster_intro_powered());serial=clusterIntroClock.serial;}
 auto& bridge=g_clusterIntroBridge;
 if(!cluster_intro_guard_needed()||phase>=clusterTotalMs)return bridge.disabledView;
 // Keep the native instruments covered BEFORE telemetry/initialization catches
 // up with ATS switching electrical power on. Waiting never starts the movie.
 if(phase<0)return managed_black_view(context,true);
 auto& image=g_clusterIntroPrepared;std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0;
 {std::unique_lock<std::mutex> lock(image.mutex,std::try_to_lock);if(lock&&image.pixels&&image.powerState==serial){pixels=image.pixels;version=image.version;}}
 if(pixels&&pixels->size()==size_t(clusterWidth)*clusterHeight*4&&version!=clusterIntroUploaded){
  D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(bridge.liveTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){
   for(unsigned y=0;y<clusterHeight;++y)memcpy((uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch,pixels->data()+size_t(y)*clusterWidth*4,clusterWidth*4);
   context->Unmap(bridge.liveTexture,0);bridge.liveTexture->SetPrivateData(kClusterIntroUpload,sizeof(serial),&serial);clusterIntroUploaded=version;
  }
 }
 uint64_t uploaded=0;UINT bytes=sizeof(uploaded);
 if(SUCCEEDED(bridge.liveTexture->GetPrivateData(kClusterIntroUpload,&bytes,&uploaded))&&uploaded==serial){
  std::lock_guard<std::mutex> lock(clusterIntroMutex);const bool first=!clusterIntroClock.shown;if(clusterIntroClock.present(serial,now)){if(first)log(SCS_LOG_TYPE_message,"[ClusterIntro] first visible frame serial=%llu",(unsigned long long)serial);return bridge.liveView;}
 }
 return managed_black_view(context,true);
}
