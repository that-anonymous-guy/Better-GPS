#pragma once
static const GUID kIcc5ClusterUpload={0xcac51c23,0x1aae,0x44ca,{0x9a,0x77,0x87,0x44,0x75,0x53,0x24,0x11}};
static ID3D11ShaderResourceView* icc5_cluster_view(ID3D11DeviceContext* context,uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(icc5ClusterMutex);phase=icc5_cluster_sample(now);serial=icc5ClusterClock.serial;}
 auto& bridge=g_icc5ClusterBridge;if(!icc5_cluster_guard_needed()||phase>=icc5ClusterTotalMs)return bridge.disabledView;if(phase<0)return managed_black_view(context,false);
 auto& image=g_icc5ClusterPrepared;std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0;{std::unique_lock<std::mutex> lock(image.mutex,std::try_to_lock);if(lock&&image.pixels&&image.powerState==serial){pixels=image.pixels;version=image.version;}}
 if(pixels&&pixels->size()==size_t(icc5ClusterWidth)*icc5ClusterHeight*4&&version!=icc5ClusterUploaded){D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(bridge.liveTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){for(unsigned y=0;y<icc5ClusterHeight;++y)memcpy((uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch,pixels->data()+size_t(y)*icc5ClusterWidth*4,icc5ClusterWidth*4);context->Unmap(bridge.liveTexture,0);bridge.liveTexture->SetPrivateData(kIcc5ClusterUpload,sizeof(serial),&serial);icc5ClusterUploaded=version;}}
 uint64_t uploaded=0;UINT bytes=sizeof(uploaded);if(SUCCEEDED(bridge.liveTexture->GetPrivateData(kIcc5ClusterUpload,&bytes,&uploaded))&&uploaded==serial){std::lock_guard<std::mutex> lock(icc5ClusterMutex);if(icc5ClusterClock.present(serial,now))return bridge.liveView;}return managed_black_view(context,false);
}
