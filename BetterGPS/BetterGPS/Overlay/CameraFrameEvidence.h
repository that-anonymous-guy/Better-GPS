#pragma once
// Temporary opt-in evidence only. Never substitutes a texture or changes a draw.
struct CameraFrameEvidence {
 template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
 struct Surface {Ptr<ID3D11Texture2D> native,overlay,source,physical;};
 struct Image {Ptr<ID3D11Texture2D> stage;D3D11_TEXTURE2D_DESC desc{};std::string name;};
 struct Job {std::vector<Image> images;Ptr<ID3D11Query> done;uint64_t at=0,index=0;std::string note;};
 std::mutex mutex;std::array<Surface,managedDeviceCount> surfaces;std::deque<Job> jobs;
 uint64_t configAfter=0,until=0,next=0,index=0,lastSerial=0,lastSelection=0,lastEpoch=0;int lastTransition=99,lastPage=-99;std::atomic<bool> enabled{false};std::filesystem::path folder;
 uintptr_t lastNative=0,lastOverlay=0,lastSource=0;bool lastReady=false,lastLayout=false,closingOnly=false;
 bool active(){return enabled;}
 void consumed(ID3D11ShaderResourceView* view){if(!enabled||!view)return;Ptr<ID3D11Resource> resource;view->GetResource(&resource);TouchSurface tag;if(!touch_surface_read(resource.Get(),tag)||!(tag.proof>>63)||tag.epoch!=vehicleStart.epoch.load())return;const auto d=managed_snapshot(tag.slot);if(d.vehicle.find("t680_2022")==std::string::npos||!managed_is_copilot(d))return;std::lock_guard<std::mutex> lock(mutex);surfaces[tag.slot].physical=texture(resource.Get());}
 static Ptr<ID3D11Texture2D> texture(ID3D11Resource* r){Ptr<ID3D11Texture2D> t;if(r)r->QueryInterface(IID_PPV_ARGS(&t));return t;}
 void target(ID3D11Resource* r,unsigned slot){if(!enabled||slot>=managedDeviceCount)return;std::lock_guard<std::mutex> lock(mutex);surfaces[slot].native=texture(r);}
 void binding(ID3D11DeviceContext* c,unsigned slot,ID3D11ShaderResourceView* selected){
  if(!enabled||slot>=managedDeviceCount)return;Ptr<ID3D11Resource> r;Ptr<ID3D11Texture2D> overlay,source;
  if(selected){selected->GetResource(&r);overlay=texture(r.Get());r.Reset();}
  if(c->GetType()==D3D11_DEVICE_CONTEXT_IMMEDIATE){Ptr<ID3D11ShaderResourceView> previous;c->PSGetShaderResources(0,1,&previous);if(previous){previous->GetResource(&r);source=texture(r.Get());}}
  std::lock_guard<std::mutex> lock(mutex);surfaces[slot].overlay=overlay;surfaces[slot].source=source;
 }
 static bool readable(DXGI_FORMAT f){return f==DXGI_FORMAT_B8G8R8A8_UNORM||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||f==DXGI_FORMAT_B8G8R8A8_TYPELESS||f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||f==DXGI_FORMAT_R8G8B8A8_TYPELESS;}
 static bool save(ID3D11DeviceContext* c,const Image& image,const std::filesystem::path& file){
  D3D11_MAPPED_SUBRESOURCE map{};if(FAILED(c->Map(image.stage.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map)))return false;
  const auto w=std::min(450u,image.desc.Width),h=std::max(1u,unsigned(uint64_t(image.desc.Height)*w/image.desc.Width));std::vector<uint8_t> pixels(size_t(w)*h*4);
  const bool rgba=image.desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM||image.desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||image.desc.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS;
  for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){auto* dst=pixels.data()+(size_t(y)*w+x)*4;const auto* src=(const uint8_t*)map.pData+size_t(uint64_t(y)*image.desc.Height/h)*map.RowPitch+unsigned(uint64_t(x)*image.desc.Width/w)*4;memcpy(dst,src,4);if(rgba)std::swap(dst[0],dst[2]);dst[3]=255;}c->Unmap(image.stage.Get(),0);
  BITMAPFILEHEADER f{};BITMAPINFOHEADER b{};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(b);f.bfSize=DWORD(f.bfOffBits+pixels.size());b.biSize=sizeof(b);b.biWidth=w;b.biHeight=-LONG(h);b.biPlanes=1;b.biBitCount=32;b.biCompression=BI_RGB;std::ofstream out(file,std::ios::binary);out.write((const char*)&f,sizeof(f));out.write((const char*)&b,sizeof(b));out.write((const char*)pixels.data(),pixels.size());return bool(out);
 }
 void present(ID3D11DeviceContext* c,uint64_t now){
  if(now>=configAfter){configAfter=now+1000;const int mode=GetPrivateProfileIntW(L"Camera",L"CaptureFrames",0,(managedBase/L"BetterGPSCamera.ini").c_str());enabled=mode!=0;closingOnly=mode==2;}
  if(!enabled){jobs.clear();return;}
  PhoneInternalScope internal;
  while(!jobs.empty()){auto& job=jobs.front();BOOL done=FALSE;const auto hr=c->GetData(job.done.Get(),&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);if(hr==S_FALSE&&!done&&now-job.at<1000)break;if(FAILED(hr)||!done){jobs.pop_front();continue;}
   const auto prefix=std::to_string(job.index);std::ofstream note(folder/(prefix+".txt"));note<<job.note;for(const auto& image:job.images)note<<image.name<<" saved="<<save(c,image,folder/(prefix+"-"+image.name+".bmp"))<<'\n';jobs.pop_front();
  }
  if(index>=120||jobs.size()>=3||!render_gpu_allowed())return;
  for(unsigned slot=0;slot<managedDeviceCount;++slot){const auto d=managed_snapshot(slot);if(!d.used||!d.enabled||d.style!=3||!managed_is_copilot(d)||d.vehicle!=managed_vehicle()||d.vehicle.find("t680_2022")==std::string::npos)continue;
   const auto p=managed_page_visual(slot);const auto state=bettergps_camera::cameras.snapshot();const bool relevant=closingOnly?(p.page==DevicePage::Home&&p.fromPage==DevicePage::Camera):(p.page==DevicePage::Camera||p.fromPage==DevicePage::Camera);
   if(!relevant)continue;
   Surface surface;{std::lock_guard<std::mutex> lock(mutex);surface=surfaces[slot];}if(!surface.native)return;
   const bool ready=bettergps_camera::feed_ready(state),layout=managedCameraLayoutActive[slot].load();
   const bool imageChanged=lastReady!=ready||lastLayout!=layout; // Native ping-pong buffers must not consume the capture budget.
   const bool changed=imageChanged||lastSerial!=p.serial||lastSelection!=state.selection||lastEpoch!=state.epoch||lastPage!=int(p.page)||lastTransition!=p.transition;
   if(changed){lastNative=uintptr_t(surface.native.Get());lastOverlay=uintptr_t(surface.overlay.Get());lastSource=uintptr_t(surface.source.Get());lastReady=ready;lastLayout=layout;lastSerial=p.serial;lastSelection=state.selection;lastEpoch=state.epoch;lastPage=int(p.page);lastTransition=p.transition;until=now+1200;next=now;}
   if(now>until||now<next)return;next=closingOnly&&p.elapsed>=350&&p.elapsed<=1000?now:now+50;
   if(folder.empty()){folder=managedBase/"logs/camera-frames"/std::to_string(now);std::error_code ec;std::filesystem::create_directories(folder,ec);log(SCS_LOG_TYPE_message,"[CameraFrames] read-only capture=%s budget=120",folder.string().c_str());}

   Ptr<ID3D11Device> device;c->GetDevice(&device);Job job;job.index=++index;job.at=now;
   ManagedUploadState upload;UINT bytes=sizeof(upload);auto* live=g_managedBridges[slot*3]->liveTexture;if(live)live->GetPrivateData(kManagedUploadState,&bytes,&upload);
   job.note="tick="+std::to_string(now)+" slot="+std::to_string(slot)+" page="+std::to_string(int(p.page))+" from="+std::to_string(int(p.fromPage))+" transition="+std::to_string(p.transition)+" elapsed="+std::to_string(p.elapsed)+" serial="+std::to_string(p.serial)+" selection="+std::to_string(state.selection)+" view="+std::to_string(unsigned(state.selected))+" feedReady="+std::to_string(bettergps_camera::feed_ready(state))+" nativeCameraLayout="+std::to_string(managedCameraLayoutActive[slot].load())+" uploadedPage="+std::to_string(int(upload.page))+" uploadedTransition="+std::to_string(upload.transition)+" uploadedSerial="+std::to_string(upload.pageSerial)+"\n";
   const char* names[]={"native-final","selected-overlay","previous-ui-source","physical-screen"};ID3D11Texture2D* inputs[]={surface.native.Get(),surface.overlay.Get(),surface.source.Get(),surface.physical.Get()};
   for(unsigned n=0;n<4;++n){if(!inputs[n]){job.note+=std::string(names[n])+" absent\n";continue;}D3D11_TEXTURE2D_DESC desc{};inputs[n]->GetDesc(&desc);job.note+=std::string(names[n])+" ptr="+std::to_string(uintptr_t(inputs[n]))+" size="+std::to_string(desc.Width)+"x"+std::to_string(desc.Height)+" format="+std::to_string(unsigned(desc.Format))+" bind="+std::to_string(desc.BindFlags)+"\n";
    if(!readable(desc.Format)||desc.SampleDesc.Count!=1||desc.ArraySize!=1||uint64_t(desc.Width)*desc.Height>2048*2048)continue;Image image;image.name=names[n];image.desc=desc;desc.Usage=D3D11_USAGE_STAGING;desc.MipLevels=1;desc.BindFlags=desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;if(FAILED(device->CreateTexture2D(&desc,nullptr,&image.stage)))continue;const D3D11_BOX box{0,0,0,desc.Width,desc.Height,1};c->CopySubresourceRegion(image.stage.Get(),0,0,0,0,inputs[n],0,&box);job.images.push_back(std::move(image));
   }
   D3D11_QUERY_DESC q{D3D11_QUERY_EVENT,0};if(SUCCEEDED(device->CreateQuery(&q,&job.done))){c->End(job.done.Get());jobs.push_back(std::move(job));}return;
  }
 }
};
static CameraFrameEvidence cameraFrameEvidence;
static void camera_evidence_target(ID3D11Resource* r,unsigned slot){cameraFrameEvidence.target(r,slot);}
