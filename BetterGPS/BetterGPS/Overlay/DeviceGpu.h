#pragma once
// Included inside bettergps_overlay, after phone_screen and the bridge declarations.
static void international_gpu_stop();
static void copilot_gpu_stop();
static ID3D11ShaderResourceView* copilot_closing_view(unsigned,const ManagedDevice&);
static void copilot_camera_target(ID3D11Resource*,unsigned,float,float,float,float);
static void camera_evidence_target(ID3D11Resource*,unsigned);
static ID3D11ShaderResourceView* international_closing_view(unsigned,const ManagedDevice&);
struct ManagedOutput {ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;bool pending=false;};
static std::array<ManagedOutput,managedDeviceCount> g_managedOutput;
static ID3D11Texture2D* g_managedStartupTexture=nullptr;
static ID3D11ShaderResourceView* g_managedStartupView=nullptr;
static uint64_t g_managedStartupUploaded=0;
static unsigned g_managedStartupSlot=~0u;
static uint64_t g_managedStartupSerial=~uint64_t(0),g_managedStartupRevision=0;
static ID3D11ShaderResourceView* g_managedOrange=nullptr;
static ID3D11ShaderResourceView* g_managedBlack=nullptr;
static ID3D11ShaderResourceView* g_managedBootBlack=nullptr;
static ID3D11ShaderResourceView* g_managedBootWhite=nullptr;
static ID3D11ShaderResourceView* g_managedBoot13=nullptr;
static ID3D11ShaderResourceView* g_managedBoot20=nullptr;
static ID3D11Device* g_managedOrangeDevice=nullptr;
static void managed_gpu_stop(){international_gpu_stop();copilot_gpu_stop();native_icon_stop();{std::lock_guard<std::mutex> lock(g_phoneMapInputMutex);for(auto& map:g_phoneMapInputs){SettingsMenuRenderer::drop(map.view);map.generation=0;}}SettingsMenuRenderer::drop(g_managedStartupView);SettingsMenuRenderer::drop(g_managedStartupTexture);g_managedStartupUploaded=0;for(auto& out:g_managedOutput){SettingsMenuRenderer::drop(out.view);SettingsMenuRenderer::drop(out.texture);out.pending=false;}SettingsMenuRenderer::drop(g_managedOrange);SettingsMenuRenderer::drop(g_managedBlack);SettingsMenuRenderer::drop(g_managedBootBlack);SettingsMenuRenderer::drop(g_managedBootWhite);SettingsMenuRenderer::drop(g_managedBoot13);SettingsMenuRenderer::drop(g_managedBoot20);SettingsMenuRenderer::drop(g_managedOrangeDevice);}
static ID3D11ShaderResourceView* managed_orange_view(ID3D11DeviceContext* context){
 ID3D11Device* device=nullptr;context->GetDevice(&device);if(device!=g_managedOrangeDevice){SettingsMenuRenderer::drop(g_managedOrange);SettingsMenuRenderer::drop(g_managedBlack);SettingsMenuRenderer::drop(g_managedBootBlack);SettingsMenuRenderer::drop(g_managedBootWhite);SettingsMenuRenderer::drop(g_managedBoot13);SettingsMenuRenderer::drop(g_managedBoot20);SettingsMenuRenderer::drop(g_managedOrangeDevice);g_managedOrangeDevice=device;device->AddRef();}
 if(!g_managedOrange){D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;const uint8_t color[]={0,147,255,255};D3D11_SUBRESOURCE_DATA initial{color,4,0};ID3D11Texture2D* texture=nullptr;PhoneInternalScope internal;if(SUCCEEDED(g_origCreateTexture2D(device,&d,&initial,&texture))){g_origCreateSRV(device,texture,nullptr,&g_managedOrange);texture->Release();}}
 device->Release();return g_managedOrange;
}
static ID3D11ShaderResourceView* managed_black_view(ID3D11DeviceContext* context,int dim=0){
 managed_orange_view(context);auto& view=dim==4?g_managedBoot20:dim==3?g_managedBoot13:dim==2?g_managedBootWhite:dim?g_managedBootBlack:g_managedBlack;if(view)return view;
 // CoPilot's RGB10 is encoded like the worker's sRGB overlay. Sampling it
 // as linear UNORM makes it RGB56 after the native sRGB output conversion.
 D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=dim?DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:DXGI_FORMAT_B8G8R8A8_UNORM;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;const uint8_t v=startup_guard_gray(dim);const uint8_t black[]={v,v,v,255};D3D11_SUBRESOURCE_DATA data{black,4,0};ID3D11Texture2D* texture=nullptr;PhoneInternalScope internal;
 if(SUCCEEDED(g_origCreateTexture2D(g_managedOrangeDevice,&desc,&data,&texture))){g_origCreateSRV(g_managedOrangeDevice,texture,nullptr,&view);texture->Release();}return view;
}
struct ManagedUploadState {uint64_t serial=~uint64_t(0);int ignition=-1,phase=-99999;uint64_t pageSerial=0;bool pageReady=false;uint64_t session=~uint64_t(0),epoch=~uint64_t(0);DevicePage page=DevicePage::Legacy;int transition=0;};
static const GUID kManagedUploadState={0xa751c184,0x5438,0x43e0,{0xb0,0x28,0x18,0x64,0x2c,0x31,0x19,0x50}};
static bool managed_overlay_power_ready(unsigned slot,const ManagedDevice& config){
 ManagedUploadState uploaded;auto* texture=g_managedBridges[slot*3]->liveTexture;UINT bytes=sizeof(uploaded);if(!texture||FAILED(texture->GetPrivateData(kManagedUploadState,&bytes,&uploaded)))return false;auto& power=managedPower[slot];
 // The GPU can bind before the worker observes a new vehicle/session. Equal
 // ignition and serial alone must not authorize the previous trip's pixels.
 if(power.session.load()!=phoneSessionSerial.load()||power.bootEpoch.load()!=(managed_page_kind(config)!=PageDevice::None?vehicleStart.epoch.load():0))return false;
 if(uploaded.session!=phoneSessionSerial.load()||uploaded.epoch!=(managed_page_kind(config)!=PageDevice::None?vehicleStart.epoch.load():0))return false;
 if(uploaded.serial!=power.serial.load())return false;
 if(config.prefs.manual<0&&config.prefs.power==1&&uploaded.ignition!=displayIgnition.load())return false;
 const bool target=config.prefs.manual>=0?config.prefs.manual!=0:config.prefs.power==0||displayIgnition!=0;
 if(!target)return false;
 const int phase=power.phase.load(),duration=managed_visible_splash_ms(config);const bool ready=phase>=duration?uploaded.phase>=0:phase>=0&&uploaded.phase>=0&&uploaded.phase<duration;if(ready&&phase<duration){uint64_t expected=0;power.splashShownAt.compare_exchange_strong(expected,GetTickCount64());}return ready;
}
static ID3D11ShaderResourceView* managed_guarded_overlay_view(ID3D11DeviceContext* context,OverlayBridge& bridge,unsigned slot){
 const auto config=managed_snapshot(slot);
 const auto page=managed_page_visual(slot);
 if(managed_is_copilot(config)&&config.style==3&&displayGlobalEnabled&&config.enabled&&managed_overlay_power_ready(slot,config)){
  // A transparent camera overlay must never reach the old native PARK image
  // while SCS is rebuilding a view or restoring the normal Home UI.
  if(auto* held=copilot_closing_view(slot,config))return held;
  if(copilot_camera_live(page)&&!bettergps_camera::feed_ready(bettergps_camera::cameras.snapshot()))return managed_black_view(context,1);
 }
 if(!managed_visible_splash(config)){if(auto* closing=copilot_closing_view(slot,config))return closing;return bridge.liveView;}
 if(!displayGlobalEnabled||!config.enabled)return bridge.disabledView;
 if(!managed_overlay_power_ready(slot,config))return managed_black_view(context,managed_boot_guard_tone(config));if(auto* closing=international_closing_view(slot,config))return closing;if(auto* closing=copilot_closing_view(slot,config))return closing;return bridge.liveView;
}
static ID3D11ShaderResourceView* managed_f150_startup_view(ID3D11DeviceContext* context,unsigned slot){
 const auto config=managed_snapshot(slot);const bool bronco=config.splash.rfind("bronco/",0)==0;const int phase=managedPower[slot].phase.load();if(phase==-2||!bronco&&phase>=0&&phase<startupBlackMs)return g_f150BlackView;if(!config.startup&&phase>=0)return g_f150ClearView;
 const auto serial=managedPower[slot].serial.load();auto* previous=g_managedStartupView&&g_managedStartupSlot==slot&&g_managedStartupSerial==serial&&g_managedStartupRevision==config.revision?g_managedStartupView:(bronco?nullptr:g_f150BlackView);
 auto& image=g_managedPrepared[slot];std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0;{std::unique_lock<std::mutex> l(image.mutex,std::try_to_lock);if(!l)return previous;if(!image.pixels||image.powerState!=serial||g_managedPreparedRevision[slot]!=config.revision||g_managedPreparedRevision[slot]>managedAppliedRevision[slot].load())return previous;pixels=image.pixels;version=image.version;}
 if(pixels->size()!=size_t(f150Width)*f150Height*4)return nullptr;ID3D11Device* device=nullptr;context->GetDevice(&device);if(g_managedStartupTexture){ID3D11Device* previous=nullptr;g_managedStartupTexture->GetDevice(&previous);if(previous!=device){SettingsMenuRenderer::drop(g_managedStartupView);SettingsMenuRenderer::drop(g_managedStartupTexture);g_managedStartupUploaded=0;}previous->Release();}
 if(!g_managedStartupTexture){D3D11_TEXTURE2D_DESC d{};d.Width=f150Width;d.Height=f150Height;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.Usage=D3D11_USAGE_DYNAMIC;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;PhoneInternalScope internal;if(SUCCEEDED(g_origCreateTexture2D(device,&d,nullptr,&g_managedStartupTexture)))g_origCreateSRV(device,g_managedStartupTexture,nullptr,&g_managedStartupView);}
 device->Release();if(!g_managedStartupView)return previous;if(version!=g_managedStartupUploaded||g_managedStartupSerial!=serial||g_managedStartupSlot!=slot){D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(g_managedStartupTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){for(unsigned y=0;y<f150Height;++y)memcpy((uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch,pixels->data()+size_t(y)*f150Width*4,f150Width*4);context->Unmap(g_managedStartupTexture,0);g_managedStartupUploaded=version;g_managedStartupSerial=serial;g_managedStartupSlot=slot;g_managedStartupRevision=config.revision;}}return g_managedStartupUploaded&&g_managedStartupSlot==slot&&g_managedStartupSerial==serial?g_managedStartupView:previous;
}
static bool managed_highlight_known(size_t known){if(!managedScan)return false;int i=managedCandidate.load();std::lock_guard<std::mutex> l(managedMutex);return i>=0&&size_t(i)<managedCandidates.size()&&managedCandidates[i].known==int(known);}
static void managed_upload(ID3D11DeviceContext* context,OverlayBridge& bridge,unsigned slot){
 auto& image=g_managedPrepared[slot];std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0;ManagedUploadState uploaded;{std::unique_lock<std::mutex> l(image.mutex,std::try_to_lock);if(!l||!image.pixels||image.version==image.uploaded||g_managedPreparedRevision[slot]>managedAppliedRevision[slot].load())return;pixels=image.pixels;version=image.version;uploaded={image.powerState,image.powerIgnition,image.powerPhase,image.page.serial,image.pageReady,image.powerSession,image.powerEpoch,image.page.page,image.page.transition};}
 D3D11_MAPPED_SUBRESOURCE mapped{};if(pixels->size()!=size_t(bridge.width)*bridge.height*4||FAILED(context->Map(bridge.liveTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;for(unsigned y=0;y<bridge.height;++y){auto* dst=(uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch;auto* src=pixels->data()+size_t(y)*bridge.width*4;if(bridge.liveFormat==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||bridge.liveFormat==DXGI_FORMAT_B8G8R8A8_UNORM)memcpy(dst,src,bridge.width*4);else for(unsigned x=0;x<bridge.width;++x){dst[x*4]=src[x*4+2];dst[x*4+1]=src[x*4+1];dst[x*4+2]=src[x*4];dst[x*4+3]=src[x*4+3];}}
 context->Unmap(bridge.liveTexture,0);bridge.liveTexture->SetPrivateData(kManagedUploadState,sizeof(uploaded),&uploaded);if(uploaded.pageReady)managed_page_presented(slot,uploaded.pageSerial,GetTickCount64());{std::lock_guard<std::mutex> l(image.mutex);image.uploaded=version;}
}
static void managed_capture_target(ID3D11DeviceContext* context,unsigned slot){
 // The end marker is drawn into this device's final native UI target. Tag
 // that target, not a shared GPS/map source or a dimension-based candidate.
 {const auto d=managed_snapshot(slot);if(managed_page_kind(d)!=PageDevice::None&&!managed_is_vnl2014(d)){
  auto canvas=managed_canvas(d);const float w=float(canvas.first),h=float(canvas.second);const bool known=d.known>=0&&size_t(d.known)<gpsDisplays.size();auto renderSize=known?std::make_pair(gpsDisplays[d.known].layout=="horizontal"?800u:360u,gpsDisplays[d.known].layout=="horizontal"?500u:740u):managed_render_dimensions(d.nativeWidth?d.nativeWidth:canvas.first,d.nativeHeight?d.nativeHeight:canvas.second);const float vw=float(renderSize.first),vh=float(renderSize.second);
  float fw=0,fh=0,fx=0,fy=0;
  if(d.style==2&&d.ui=="/ui/dashboard/gps.dlc_toys.sii"){const double physical=1200./672.,content=w/h;fw=float(std::lround(vw*std::min(1.,content/physical)));fh=float(std::lround(vh*std::min(1.,physical/content)));}
  else{const double fit=std::min(vw/w,vh/h);fw=float(std::lround(w*fit));fh=float(std::lround(h*fit));}fx=(vw-fw)/2;fy=(vh-fh)/2;
  if(managed_is_international(d)||managed_is_navplus(d)||managed_is_copilot(d)||managed_is_zonar(d)||managed_is_icc5(d)||managed_is_mack(d)||(managed_is_volvo18(d)||managed_is_volvo25(d))||managed_is_vnl2014(d)){const auto p=managed_navplus_rect(d,unsigned(vw),unsigned(vh),unsigned(w),unsigned(h));fx=float(p.x);fy=float(p.y);fw=float(p.w);fh=float(p.h);}
  // Generated SCS UI uses coords_b/coords_t (bottom-origin), whereas texture
  // pixels and the certified native-copy mapping use top-origin. Symmetric
  // crops hid this mismatch; the 5700XE's off-centre crop makes it decisive.
  const float textureTop=1.f-(fy+fh)/vh;
  ID3D11RenderTargetView* target=native_bound_target(context);if(target){ID3D11Resource* resource=nullptr;target->GetResource(&resource);target->Release();touch_tag_texture(resource,slot,fx/vw,textureTop,fw/vw,fh/vh);camera_evidence_target(resource,slot);copilot_camera_target(resource,slot,fx/vw,textureTop,fw/vw,fh/vh);if(resource)resource->Release();}
 }}
 auto d=managed_snapshot(slot);if(d.known<0||size_t(d.known)>=gpsDisplays.size()||!gpsDisplays[d.known].independent )return;
 std::unique_lock<std::mutex> screenLock(g_phoneScreenMutex,std::try_to_lock);if(!screenLock)return;
 ID3D11RenderTargetView* target=nullptr;if(context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED){UINT bytes=sizeof(target);context->GetPrivateData(kManagedDeferredTarget,&bytes,&target);}else context->OMGetRenderTargets(1,&target,nullptr);if(!target)return;D3D11_RENDER_TARGET_VIEW_DESC targetView{};target->GetDesc(&targetView);ID3D11Resource* resource=nullptr;target->GetResource(&resource);target->Release();ID3D11Texture2D* texture=nullptr;if(resource)resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture);if(resource)resource->Release();if(!texture)return;
 auto& out=g_managedOutput[slot];if(out.texture!=texture){
  g_phoneScreens.erase(std::remove_if(g_phoneScreens.begin(),g_phoneScreens.end(),[&](auto& s){return s->native==out.texture;}),g_phoneScreens.end());SettingsMenuRenderer::drop(out.view);SettingsMenuRenderer::drop(out.texture);
  ID3D11Device* device=nullptr;texture->GetDevice(&device);D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;view.Format=targetView.Format;view.Texture2D.MipLevels=1;PhoneInternalScope internal;if(SUCCEEDED(g_origCreateSRV(device,texture,&view,&out.view))){out.texture=texture;out.texture->AddRef();D3D11_TEXTURE2D_DESC dimensions{};texture->GetDesc(&dimensions);log(SCS_LOG_TYPE_message,"[DeviceOutput] slot=%u known=%d target=%ux%u context=%s",slot,d.known,dimensions.Width,dimensions.Height,context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED?"deferred":"immediate");}device->Release();
 }
 auto* power=phone_screen(texture);if(out.view&&!power){auto created=std::make_unique<PhoneScreenPower>();PhoneInternalScope internal;if(created->start(texture)){created->displayIndex=size_t(d.known);created->managedOutput=true;power=created.get();g_phoneScreens.push_back(std::move(created));}}
 if(power)out.pending=true;texture->Release();
}
// Called only at Present, after the engine has executed its UI command lists.
static void managed_finish_targets(ID3D11DeviceContext* context,int phase,uint64_t now){for(unsigned i=0;i<managedDeviceCount;++i){auto& out=g_managedOutput[i];if(!out.pending||!out.texture)continue;out.pending=false;auto d=managed_snapshot(i);if(!d.enabled||d.known<0||!display_active(size_t(d.known))||!managed_power_target(i)||managedPower[i].phase<startupBlackMs)continue;if(auto* power=phone_screen(out.texture)){ID3D11Device* device=nullptr;context->GetDevice(&device);bool same=device==power->renderer.device;device->Release();if(same){power->pageClosing=false;const auto page=managed_page_visual(i);const bool closing=managed_page_kind(d)==PageDevice::Phone&&page.transition<0&&page.elapsed<pageCloseMs&&power->begin_page_close(context,page.serial);power->capture_base(context,now);if(closing){const auto bounds=phone_app_bounds(1-double(page.elapsed)/pageCloseMs,d.style==1);const float sx=power->width/360.f,sy=power->height/740.f;power->finish_page_close(context,bounds.x*sx,bounds.y*sy,bounds.w*sx,bounds.h*sy,float(14*double(page.elapsed)/pageCloseMs)*sx,now);}}}}}
static ID3D11ShaderResourceView* managed_phone_view(ID3D11DeviceContext* context,size_t known,int phase,ID3D11ShaderResourceView* waiting=nullptr){int slot=managed_for_known(known);if(slot<0)return nullptr;auto d=managed_snapshot(unsigned(slot));if(!d.enabled||!displayGlobalEnabled||!display_active(known))return nullptr;std::unique_lock<std::mutex> l(g_phoneScreenMutex,std::try_to_lock);if(l){auto& out=g_managedOutput[slot];if(auto* screen=out.texture?phone_screen(out.texture):nullptr){auto* view=screen->owned_view(managedPower[slot].phase.load());if(view){view->AddRef();return view;}}}if(waiting)waiting->AddRef();return waiting;}
