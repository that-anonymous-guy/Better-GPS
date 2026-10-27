#pragma once
// Exact role-151 marker identity; no private cluster UI or shader replay.
static const GUID kMackClusterUpload={0x9ef2f417,0x8521,0x4504,{0x94,0x48,0x14,0xb2,0x88,0x61,0x53,0x21}};
static ID3D11ShaderResourceView* mack_cluster_view(ID3D11DeviceContext* context,uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(mackClusterMutex);phase=mackClusterClock.sample(now,phoneSessionSerial,vehicleStart.epoch,mack_cluster_powered());serial=mackClusterClock.serial;}
 auto& bridge=g_mackClusterBridge;
 if(!mack_cluster_guard_needed()||phase>=mackClusterTotalMs)return bridge.disabledView;
 if(phase<0)return managed_black_view(context,true);
 auto& image=g_mackClusterPrepared;std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0;
 {std::unique_lock<std::mutex> lock(image.mutex,std::try_to_lock);if(lock&&image.pixels&&image.powerState==serial){pixels=image.pixels;version=image.version;}}
 if(pixels&&pixels->size()==size_t(mackClusterWidth)*mackClusterHeight*4&&version!=mackClusterUploaded){
  D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(bridge.liveTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){
   for(unsigned y=0;y<mackClusterHeight;++y)memcpy((uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch,pixels->data()+size_t(y)*mackClusterWidth*4,mackClusterWidth*4);
   context->Unmap(bridge.liveTexture,0);bridge.liveTexture->SetPrivateData(kMackClusterUpload,sizeof(serial),&serial);mackClusterUploaded=version;
  }
 }
 uint64_t uploaded=0;UINT bytes=sizeof(uploaded);
 if(SUCCEEDED(bridge.liveTexture->GetPrivateData(kMackClusterUpload,&bytes,&uploaded))&&uploaded==serial){std::lock_guard<std::mutex> lock(mackClusterMutex);if(mackClusterClock.present(serial,now))return bridge.liveView;}
 return managed_black_view(context,true);
}
