#pragma once
// Compose the existing private map and overlay; only the app body slides.
// Shared Kenworth chrome stays fixed. No readback or new native map producer.
// End-marker observation only retains the final native target. Copy/draw is
// deferred until Present, after SCS has executed its UI command lists.
struct CopilotCameraTarget {
 std::mutex mutex;Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
 float left=0,top=0,width=1,height=1;uint64_t session=0,epoch=0,power=0,page=0;
};
static std::array<CopilotCameraTarget,managedDeviceCount> copilotCameraTargets;
static void copilot_camera_target(ID3D11Resource* resource,unsigned slot,float left,float top,float width,float height){
 if(!resource||slot>=managedDeviceCount)return;const auto d=managed_snapshot(slot);const auto page=managed_page_visual(slot);
 if(!managed_is_copilot(d)||d.style!=3||!copilot_camera_live(page)||!bettergps_camera::feed_ready(bettergps_camera::cameras.snapshot()))return;
 Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;if(FAILED(resource->QueryInterface(IID_PPV_ARGS(&texture))))return;
 auto& target=copilotCameraTargets[slot];std::lock_guard<std::mutex> lock(target.mutex);target.texture=texture;target.left=left;target.top=top;target.width=width;target.height=height;target.session=phoneSessionSerial;target.epoch=vehicleStart.epoch;target.power=managedPower[slot].serial;target.page=page.serial;
}
struct CopilotPageSurface {
 PhoneScreenPower power;
 ID3D11Texture2D* canvas=nullptr;ID3D11RenderTargetView* target=nullptr;
 Microsoft::WRL::ComPtr<ID3D11Texture2D> cameraCopy;Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cameraView;
 uint64_t session=0,epoch=0,serial=0,revision=0,pageSerial=0;
 unsigned contentWidth=0,contentHeight=0;
 ~CopilotPageSurface(){SettingsMenuRenderer::drop(target);SettingsMenuRenderer::drop(canvas);}
 bool start(ID3D11Device* device,unsigned slot){
  D3D11_TEXTURE2D_DESC d{};d.Width=managed_marker_width(slot,0);d.Height=managed_marker_height(0);d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
  return SUCCEEDED(device->CreateTexture2D(&d,nullptr,&canvas))&&SUCCEEDED(device->CreateRenderTargetView(canvas,nullptr,&target))&&power.start(canvas);
 }
 ID3D11ShaderResourceView* camera_frame(ID3D11DeviceContext* context,unsigned slot,const PageVisual& page){
  auto& source=copilotCameraTargets[slot];std::lock_guard<std::mutex> lock(source.mutex);
  if(!source.texture||source.session!=session||source.epoch!=epoch||source.power!=serial||source.page!=page.serial)return nullptr;
  D3D11_TEXTURE2D_DESC d{};source.texture->GetDesc(&d);if(d.SampleDesc.Count!=1||d.ArraySize!=1)return nullptr;
  const unsigned left=std::min(d.Width-1,unsigned(std::max(0l,std::lround(source.left*d.Width)))),top=std::min(d.Height-1,unsigned(std::max(0l,std::lround(source.top*d.Height))));
  const unsigned right=std::clamp(unsigned(std::max(0l,std::lround((source.left+source.width)*d.Width))),left+1,d.Width),bottom=std::clamp(unsigned(std::max(0l,std::lround((source.top+source.height)*d.Height))),top+1,d.Height);
  D3D11_TEXTURE2D_DESC previous{};if(cameraCopy)cameraCopy->GetDesc(&previous);
  if(!cameraCopy||previous.Width!=right-left||previous.Height!=bottom-top||previous.Format!=d.Format){
   cameraView.Reset();cameraCopy.Reset();d.Width=right-left;d.Height=bottom-top;d.MipLevels=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.CPUAccessFlags=d.MiscFlags=0;
   if(FAILED(power.renderer.device->CreateTexture2D(&d,nullptr,&cameraCopy)))return nullptr;
   D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;view.Texture2D.MipLevels=1;view.Format=d.Format;
   if(view.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS)view.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;else if(view.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS)view.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
   if(FAILED(power.renderer.device->CreateShaderResourceView(cameraCopy.Get(),&view,&cameraView))){cameraCopy.Reset();return nullptr;}
  }
  PhoneInternalScope internal;const D3D11_BOX crop{left,top,0,right,bottom,1};context->CopySubresourceRegion(cameraCopy.Get(),0,0,0,0,source.texture.Get(),0,&crop);return cameraView.Get();
 }
 void finish(ID3D11DeviceContext* context,unsigned slot,const ManagedDevice& d,const PageVisual& page,uint64_t now){
  auto& bridge=*g_managedBridges[slot*3];if(!bridge.liveView||!bridge.liveTexture||!managed_overlay_power_ready(slot,d))return;
  {auto& image=g_managedPrepared[slot];std::unique_lock<std::mutex> lock(image.mutex,std::try_to_lock);if(!lock||!image.pageReady||image.page.serial!=page.serial||image.page.transition!=page.transition||image.uploaded!=image.version)return;}
  const auto nextSession=phoneSessionSerial.load(),nextEpoch=vehicleStart.epoch.load(),nextSerial=managedPower[slot].serial.load();
  if(session!=nextSession||epoch!=nextEpoch||serial!=nextSerial||revision!=d.revision){power.captured=power.pageClosing=power.outputReady=false;power.pageSerial=~uint64_t(0);session=nextSession;epoch=nextEpoch;serial=nextSerial;revision=d.revision;}
  auto* black=managed_black_view(context,1);if(!black)return;
  const auto stored=managed_marker_content(slot,copilotWidth,copilotHeight);contentWidth=stored.first;contentHeight=stored.second;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> map;
  if(page.page==DevicePage::Nav||page.page==DevicePage::Notrip){std::lock_guard<std::mutex> lock(g_phoneMapInputMutex);const auto& input=g_phoneMapInputs[slot];if(input.view&&input.generation==nativeForkSourceGeneration.load())map=input.view;if(!map)return;}
  const auto bodyTop=unsigned(std::clamp(std::lround(((63-272)*d.globalScale+272)*contentHeight/544.),0l,long(contentHeight-1))),bodyBottom=unsigned(std::clamp(std::lround(((483-272)*d.globalScale+272)*contentHeight/544.),long(bodyTop+1),long(contentHeight)));
  const auto appTop=unsigned(std::clamp(std::lround(((copilotAppTop-272)*d.globalScale+272)*contentHeight/544.),0l,long(bodyBottom-1)));
  if(copilot_camera_live(page)&&!bettergps_camera::feed_ready(bettergps_camera::cameras.snapshot()))return;
  auto* camera=copilot_camera_live(page)?camera_frame(context,slot,page):nullptr;
  const D3D11_BOX crop{0,appTop,0,contentWidth,bodyBottom,1};
  const bool forward=copilot_forward_capture(page);const bool moving=(page.transition<0&&page.elapsed<copilotCloseDelayMs+copilotSlideMs||forward)&&power.begin_page_close(context,page.serial,&crop);
  PhoneInternalScope internal;ID3DDeviceContextState* saved=nullptr;ID3D11Predicate* predicate=nullptr;BOOL predValue=FALSE;auto* work=power.deferred?power.deferred:context;
  if(!power.deferred){context->GetPredication(&predicate,&predValue);power.isolatedContext->SwapDeviceContextState(power.isolatedState,&saved);}
  work->ClearState();work->SetPredication(nullptr,FALSE);const float clear[4]{};work->ClearRenderTargetView(target,clear);auto& r=power.renderer;
  D3D11_VIEWPORT viewport{0,0,float(power.width),float(power.height),0,1};work->OMSetRenderTargets(1,&target,nullptr);work->RSSetState(r.raster);work->RSSetViewports(1,&viewport);work->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);work->VSSetShader(r.vs,nullptr,0);work->PSSetShader(r.ps,nullptr,0);work->PSSetSamplers(0,1,&r.sampler);work->PSSetConstantBuffers(0,1,&r.constants);
  if((copilot_camera_live(page)&&!camera)||page.fromPage==DevicePage::Camera){D3D11_VIEWPORT area{0,0,float(contentWidth),float(contentHeight),0,1};const float parameters[4]{};work->RSSetViewports(1,&area);work->UpdateSubresource(r.constants,0,nullptr,parameters,0,0);work->PSSetShaderResources(0,1,&black);work->Draw(3,0);work->RSSetViewports(1,&viewport);}
  if(map){D3D11_VIEWPORT area{0,0,float(contentWidth),float(contentHeight),0,1};const float parameters[4]={1.f,float(copilotHeight)/copilotWidth,-3,0};work->RSSetViewports(1,&area);work->UpdateSubresource(r.constants,0,nullptr,parameters,0,0);auto* source=map.Get();work->PSSetShaderResources(0,1,&source);work->Draw(3,0);work->RSSetViewports(1,&viewport);}
  // Camera snapshots contain the completed SCS image, including its UI.
  // Stable playback binds the live overlay directly, so this never feeds back
  // a captured frame into the camera producer or freezes the live video.
  if(camera){D3D11_VIEWPORT area{0,0,float(contentWidth),float(contentHeight),0,1};const float parameters[4]={0,1,0,0};work->RSSetViewports(1,&area);work->UpdateSubresource(r.constants,0,nullptr,parameters,0,0);work->PSSetShaderResources(0,1,&camera);work->Draw(3,0);work->RSSetViewports(1,&viewport);}
  const float alpha[4]{};work->UpdateSubresource(r.constants,0,nullptr,alpha,0,0);work->PSSetShaderResources(0,1,&black);
  if(!camera)for(const auto area:std::array<D3D11_VIEWPORT,2>{{{0,0,float(contentWidth),float(bodyTop),0,1},{0,float(bodyBottom),float(contentWidth),float(contentHeight-bodyBottom),0,1}}})if(area.Height>0){work->RSSetViewports(1,&area);work->Draw(3,0);}work->RSSetViewports(1,&viewport);
  work->OMSetBlendState(r.blend,nullptr,~0u);work->UpdateSubresource(r.constants,0,nullptr,alpha,0,0);auto* layer=camera?black:bridge.liveView;work->PSSetShaderResources(0,1,&layer);if(!camera)work->Draw(3,0);ID3D11ShaderResourceView* none=nullptr;work->PSSetShaderResources(0,1,&none);work->OMSetRenderTargets(0,nullptr,nullptr);
  bool ready=true;if(power.deferred){ID3D11CommandList* commands=nullptr;ready=SUCCEEDED(power.deferred->FinishCommandList(FALSE,&commands));if(ready){context->ExecuteCommandList(commands,TRUE);commands->Release();}}else{power.isolatedContext->SwapDeviceContextState(saved,nullptr);if(saved)saved->Release();context->SetPredication(predicate,predValue);if(predicate)predicate->Release();}
  if(!ready)return;power.pageClosing=false;power.capture_base(context,now);pageSerial=page.serial;
  if(moving){const float progress=float(copilot_ease(double(copilot_motion_elapsed(page.elapsed,forward))/copilotSlideMs));power.finish_page_close(context,(forward?-1.f:1.f)*float(contentWidth)*progress,float(appTop),float(contentWidth),float(bodyBottom-appTop),0,now);}
 }
};
static std::array<std::unique_ptr<CopilotPageSurface>,managedDeviceCount> copilotPages;
static void copilot_gpu_stop(){for(auto& surface:copilotPages)surface.reset();for(auto& source:copilotCameraTargets){std::lock_guard<std::mutex> lock(source.mutex);source.texture.Reset();}}
static ID3D11ShaderResourceView* copilot_closing_view(unsigned slot,const ManagedDevice& d){
 const auto& surface=copilotPages[slot];const auto page=managed_page_visual(slot);
 if(copilot_camera_live(page)&&bettergps_camera::feed_ready(bettergps_camera::cameras.snapshot()))return nullptr;
 if(page.page==DevicePage::Home&&page.transition==0&&page.fromPage==DevicePage::Camera&&surface&&surface->session==phoneSessionSerial&&surface->epoch==vehicleStart.epoch&&surface->serial==managedPower[slot].serial&&surface->revision==d.revision){
  ManagedUploadState uploaded;auto* texture=g_managedBridges[slot*3]->liveTexture;UINT bytes=sizeof(uploaded);
  const bool homeUploaded=texture&&SUCCEEDED(texture->GetPrivateData(kManagedUploadState,&bytes,&uploaded))&&uploaded.pageSerial==page.serial&&uploaded.page==DevicePage::Home&&uploaded.transition==0;
  if(!homeUploaded||managedCameraLayoutActive[slot].load())return surface->power.outputReady?surface->power.outputView:surface->power.captured?surface->power.snapshotView:nullptr;
 }
 if(!surface||!managed_is_copilot(d)||d.vehicle!=managed_vehicle()||d.style!=3||(page.page==DevicePage::Home&&(page.transition>=0||page.elapsed>=copilotCloseDelayMs+copilotSlideMs))||surface->session!=phoneSessionSerial||surface->epoch!=vehicleStart.epoch||surface->serial!=managedPower[slot].serial||surface->revision!=d.revision)return nullptr;
 if(copilot_camera_live(page))return surface->power.captured?surface->power.snapshotView:nullptr;
 if(page.transition>=0&&!copilot_forward_capture(page))return surface->power.captured&&surface->pageSerial==page.serial?surface->power.snapshotView:nullptr;
 if(surface->power.pageClosing&&surface->power.outputReady&&surface->pageSerial==page.serial)return surface->power.outputView;
 // The first Home upload can bind before Present composes its closing frame.
 // Keep the last actual app until then, never flash Home ahead of the shrink.
 return surface->power.captured?surface->power.snapshotView:nullptr;
}
static void copilot_pages_finish(ID3D11DeviceContext* context,uint64_t now){
 if(!render_gpu_allowed()||!displayGlobalEnabled)return;Microsoft::WRL::ComPtr<ID3D11Device> device;context->GetDevice(&device);
 for(unsigned slot=0;slot<managedDeviceCount;++slot){if(!managedDeviceActive[slot])continue;const auto d=managed_snapshot(slot);if(!d.used||!d.enabled||!managed_is_copilot(d)||d.style!=3||d.vehicle!=managed_vehicle()||!managed_power_target(slot)||managedPower[slot].phase<(d.startup?copilotBootMs:0))continue;auto& surface=copilotPages[slot];const auto page=managed_page_visual(slot);if(page.page==DevicePage::Home&&page.transition>=0){if(surface)surface->power.pageClosing=false;continue;}if(surface&&surface->power.renderer.device!=device.Get())surface.reset();if(!surface){PhoneInternalScope internal;auto created=std::make_unique<CopilotPageSurface>();if(!created->start(device.Get(),slot))continue;surface=std::move(created);}surface->finish(context,slot,d,page,now);}
}
