#pragma once
// Reuse the iPhone GPU compositor for closing the actual map + app UI, not
// the launch card. Only the selected International's existing map is consumed.
struct InternationalPageSurface {
 PhoneScreenPower power;
 ID3D11Texture2D* canvas=nullptr;ID3D11RenderTargetView* target=nullptr;
 uint64_t session=0,epoch=0,serial=0,revision=0,pageSerial=0;
 unsigned contentWidth=0,contentHeight=0;
 ~InternationalPageSurface(){SettingsMenuRenderer::drop(target);SettingsMenuRenderer::drop(canvas);}
 bool start(ID3D11Device* device,unsigned slot){
  D3D11_TEXTURE2D_DESC d{};d.Width=managed_marker_width(slot,0);d.Height=managed_marker_height(0);d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
  return SUCCEEDED(device->CreateTexture2D(&d,nullptr,&canvas))&&SUCCEEDED(device->CreateRenderTargetView(canvas,nullptr,&target))&&power.start(canvas);
 }
 void finish(ID3D11DeviceContext* context,unsigned slot,const ManagedDevice& d,const PageVisual& page,uint64_t now){
  auto& bridge=*g_managedBridges[slot*3];if(!bridge.liveView||!bridge.liveTexture||!managed_overlay_power_ready(slot,d))return;
  {auto& image=g_managedPrepared[slot];std::unique_lock<std::mutex> lock(image.mutex,std::try_to_lock);if(!lock||!image.pageReady||image.page.serial!=page.serial||image.uploaded!=image.version)return;}
  const auto nextSession=phoneSessionSerial.load(),nextEpoch=vehicleStart.epoch.load(),nextSerial=managedPower[slot].serial.load();
  if(session!=nextSession||epoch!=nextEpoch||serial!=nextSerial||revision!=d.revision){power.captured=power.pageClosing=power.outputReady=false;power.pageSerial=~uint64_t(0);session=nextSession;epoch=nextEpoch;serial=nextSerial;revision=d.revision;}
  const auto stored=managed_marker_content(slot,internationalWidth,internationalHeight);contentWidth=stored.first;contentHeight=stored.second;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> map;
  if(page.page!=DevicePage::Home){std::lock_guard<std::mutex> lock(g_phoneMapInputMutex);const auto& input=g_phoneMapInputs[slot];if(input.view&&input.generation==nativeForkSourceGeneration.load())map=input.view;if(!map)return;}
  const D3D11_BOX crop{0,0,0,contentWidth,contentHeight,1};
  const bool closing=page.transition<0&&page.elapsed<pageCloseMs&&power.begin_page_close(context,page.serial,&crop);
  PhoneInternalScope internal;ID3DDeviceContextState* saved=nullptr;ID3D11Predicate* predicate=nullptr;BOOL predValue=FALSE;auto* work=power.deferred?power.deferred:context;
  if(!power.deferred){context->GetPredication(&predicate,&predValue);power.isolatedContext->SwapDeviceContextState(power.isolatedState,&saved);}
  work->ClearState();work->SetPredication(nullptr,FALSE);const float clear[4]{};work->ClearRenderTargetView(target,clear);auto& r=power.renderer;
  D3D11_VIEWPORT viewport{0,0,float(power.width),float(power.height),0,1};work->OMSetRenderTargets(1,&target,nullptr);work->RSSetState(r.raster);work->RSSetViewports(1,&viewport);work->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);work->VSSetShader(r.vs,nullptr,0);work->PSSetShader(r.ps,nullptr,0);work->PSSetSamplers(0,1,&r.sampler);work->PSSetConstantBuffers(0,1,&r.constants);
  if(map){D3D11_VIEWPORT area{0,0,float(contentWidth),float(contentHeight),0,1};const float parameters[4]={1.f,float(internationalHeight)/internationalWidth,-3,0};work->RSSetViewports(1,&area);work->UpdateSubresource(r.constants,0,nullptr,parameters,0,0);auto* source=map.Get();work->PSSetShaderResources(0,1,&source);work->Draw(3,0);work->RSSetViewports(1,&viewport);}
  const float alpha[4]{};work->OMSetBlendState(r.blend,nullptr,~0u);work->UpdateSubresource(r.constants,0,nullptr,alpha,0,0);auto* layer=bridge.liveView;work->PSSetShaderResources(0,1,&layer);work->Draw(3,0);ID3D11ShaderResourceView* none=nullptr;work->PSSetShaderResources(0,1,&none);work->OMSetRenderTargets(0,nullptr,nullptr);
  bool ready=true;if(power.deferred){ID3D11CommandList* commands=nullptr;ready=SUCCEEDED(power.deferred->FinishCommandList(FALSE,&commands));if(ready){context->ExecuteCommandList(commands,TRUE);commands->Release();}}else{power.isolatedContext->SwapDeviceContextState(saved,nullptr);if(saved)saved->Release();context->SetPredication(predicate,predValue);if(predicate)predicate->Release();}
  if(!ready)return;power.pageClosing=false;power.capture_base(context,now);pageSerial=page.serial;
  if(closing){auto profile=d.overlay;if(managedSelected.load()==int(slot))profile=overlay_snapshot();const auto b=international_app_bounds(profile,1-double(page.elapsed)/pageCloseMs);const float sx=float(contentWidth)/internationalWidth,sy=float(contentHeight)/internationalHeight;const double zoom=d.globalScale;const float x=float((b.x-internationalWidth*.5)*zoom+internationalWidth*.5),y=float((b.y-internationalHeight*.5)*zoom+internationalHeight*.5);power.finish_page_close(context,x*sx,y*sy,float(b.w*zoom)*sx,float(b.h*zoom)*sy,float(14*double(page.elapsed)/pageCloseMs)*sx,now);}
 }
};
static std::array<std::unique_ptr<InternationalPageSurface>,managedDeviceCount> internationalPages;
static void international_gpu_stop(){for(auto& surface:internationalPages)surface.reset();}
static ID3D11ShaderResourceView* international_closing_view(unsigned slot,const ManagedDevice& d){
 const auto& surface=internationalPages[slot];const auto page=managed_page_visual(slot);if(!surface||!managed_is_international(d)||d.vehicle!=managed_vehicle()||d.style!=3||page.transition>=0||page.elapsed>=pageCloseMs||surface->session!=phoneSessionSerial||surface->epoch!=vehicleStart.epoch||surface->serial!=managedPower[slot].serial||surface->revision!=d.revision)return nullptr;
 if(surface->power.pageClosing&&surface->power.outputReady&&surface->pageSerial==page.serial)return surface->power.outputView;
 // The first Home upload can bind before Present composes its closing frame.
 // Keep the last actual app until then, never flash Home ahead of the shrink.
 return surface->power.captured?surface->power.snapshotView:nullptr;
}
static void international_pages_finish(ID3D11DeviceContext* context,uint64_t now){
 if(!render_gpu_allowed()||!displayGlobalEnabled||!managed_international_vehicle(managed_vehicle()))return;Microsoft::WRL::ComPtr<ID3D11Device> device;context->GetDevice(&device);
 for(unsigned slot=0;slot<managedDeviceCount;++slot){if(!managedDeviceActive[slot])continue;const auto d=managed_snapshot(slot);if(!d.used||!d.enabled||!managed_is_international(d)||d.style!=3||d.vehicle!=managed_vehicle()||!managed_power_target(slot)||managedPower[slot].phase<internationalBootMs)continue;auto& surface=internationalPages[slot];const auto page=managed_page_visual(slot);if(page.page==DevicePage::Home&&page.transition>=0){if(surface)surface->power.pageClosing=false;continue;}if(surface&&surface->power.renderer.device!=device.Get())surface.reset();if(!surface){PhoneInternalScope internal;auto created=std::make_unique<InternationalPageSurface>();if(!created->start(device.Get(),slot))continue;surface=std::move(created);}surface->finish(context,slot,d,page,now);}
}
