#pragma once
// Exact role-162/163 marker identities; no private cluster UI or shader replay.
static const GUID kIcucClusterUpload={0xcac51623,0x8521,0x4504,{0x94,0x48,0x14,0xb2,0x88,0x61,0x53,0x21}};
static ID3D11ShaderResourceView* icuc_cluster_view(ID3D11DeviceContext* context,uint64_t now,int kind=0){
 uint64_t serial=0;int phase=-1;{std::lock_guard<std::mutex> lock(icucClusterMutex);phase=icuc_cluster_sample(now);serial=icucClusterClock.serial;}
 auto& bridge=kind==2?g_intlClusterBridge:kind==1?g_icuc5700ClusterBridge:g_icucClusterBridge;auto& uploadedVersion=kind==2?intlClusterUploaded:kind==1?icuc5700ClusterUploaded:icucClusterUploaded;const unsigned width=bridge.width,height=bridge.height;
 if(!icuc_cluster_guard_needed(kind)||phase>=icuc_cluster_total_ms(managed_vehicle()))return bridge.disabledView;
 if(phase<0)return managed_black_view(context,3);
 auto& image=kind==2?g_intlClusterPrepared:kind==1?g_icuc5700ClusterPrepared:g_icucClusterPrepared;std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0;
 {std::unique_lock<std::mutex> lock(image.mutex,std::try_to_lock);if(lock&&image.pixels&&image.powerState==serial){pixels=image.pixels;version=image.version;}}
 if(pixels&&pixels->size()==size_t(width)*height*4&&version!=uploadedVersion){
  D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(bridge.liveTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){
   for(unsigned y=0;y<height;++y)memcpy((uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch,pixels->data()+size_t(y)*width*4,width*4);
   context->Unmap(bridge.liveTexture,0);bridge.liveTexture->SetPrivateData(kIcucClusterUpload,sizeof(serial),&serial);uploadedVersion=version;
  }
 }
 uint64_t uploaded=0;UINT bytes=sizeof(uploaded);
 if(SUCCEEDED(bridge.liveTexture->GetPrivateData(kIcucClusterUpload,&bytes,&uploaded))&&uploaded==serial){std::lock_guard<std::mutex> lock(icucClusterMutex);if(icucClusterClock.present(serial,now))return bridge.liveView;}
 return managed_black_view(context,3);
}
