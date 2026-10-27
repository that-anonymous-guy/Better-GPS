#pragma once
static const GUID kVolvo18ClusterUpload={0xa65e2c19,0x1aae,0x44ca,{0x9a,0x77,0x87,0x44,0x75,0x53,0x24,0x11}};
static ID3D11ShaderResourceView* volvo18_cluster_view(ID3D11DeviceContext* context,uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(volvo18ClusterMutex);phase=volvo18ClusterClock.sample(now,phoneSessionSerial,vehicleStart.epoch,volvo18_cluster_powered());serial=volvo18ClusterClock.serial;}
 auto& bridge=g_volvo18ClusterBridge;if(!volvo18_cluster_guard_needed()||phase>=volvo18ClusterTotalMs)return bridge.disabledView;if(phase<0)return managed_black_view(context,true);
 auto& image=g_volvo18ClusterPrepared;std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0;{std::unique_lock<std::mutex> lock(image.mutex,std::try_to_lock);if(lock&&image.pixels&&image.powerState==serial){pixels=image.pixels;version=image.version;}}
 if(pixels&&pixels->size()==size_t(volvo18ClusterWidth)*volvo18ClusterHeight*4&&version!=volvo18ClusterUploaded){D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(bridge.liveTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){for(unsigned y=0;y<volvo18ClusterHeight;++y)memcpy((uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch,pixels->data()+size_t(y)*volvo18ClusterWidth*4,volvo18ClusterWidth*4);context->Unmap(bridge.liveTexture,0);bridge.liveTexture->SetPrivateData(kVolvo18ClusterUpload,sizeof(serial),&serial);volvo18ClusterUploaded=version;}}
 uint64_t uploaded=0;UINT bytes=sizeof(uploaded);if(SUCCEEDED(bridge.liveTexture->GetPrivateData(kVolvo18ClusterUpload,&bytes,&uploaded))&&uploaded==serial){std::lock_guard<std::mutex> lock(volvo18ClusterMutex);if(volvo18ClusterClock.present(serial,now))return bridge.liveView;}return managed_black_view(context,true);
}
