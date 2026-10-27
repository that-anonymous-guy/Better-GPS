#pragma once
static const GUID kVnlClusterUpload={0x8c05bd21,0x1aae,0x44ca,{0x9a,0x77,0x87,0x44,0x75,0x53,0x24,0x11}};
static ID3D11ShaderResourceView* vnl_cluster_view(ID3D11DeviceContext* context,uint64_t now){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(vnlClusterMutex);phase=vnlClusterClock.sample(now,phoneSessionSerial,vehicleStart.epoch,vnl_cluster_powered());serial=vnlClusterClock.serial;}
 auto& bridge=g_vnlClusterBridge;if(!vnl_cluster_guard_needed()||phase>=vnlClusterTotalMs)return bridge.disabledView;if(phase<0)return managed_black_view(context,true);
 auto& image=g_vnlClusterPrepared;std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0;{std::unique_lock<std::mutex> lock(image.mutex,std::try_to_lock);if(lock&&image.pixels&&image.powerState==serial){pixels=image.pixels;version=image.version;}}
 if(pixels&&pixels->size()==size_t(vnlClusterWidth)*vnlClusterHeight*4&&version!=vnlClusterUploaded){D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(bridge.liveTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){for(unsigned y=0;y<vnlClusterHeight;++y)memcpy((uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch,pixels->data()+size_t(y)*vnlClusterWidth*4,vnlClusterWidth*4);context->Unmap(bridge.liveTexture,0);bridge.liveTexture->SetPrivateData(kVnlClusterUpload,sizeof(serial),&serial);vnlClusterUploaded=version;}}
 uint64_t uploaded=0;UINT bytes=sizeof(uploaded);if(SUCCEEDED(bridge.liveTexture->GetPrivateData(kVnlClusterUpload,&bytes,&uploaded))&&uploaded==serial){std::lock_guard<std::mutex> lock(vnlClusterMutex);if(vnlClusterClock.present(serial,now))return bridge.liveView;}return managed_black_view(context,true);
}
