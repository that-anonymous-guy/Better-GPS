#pragma once
namespace copilot_startup_gpu_fixture {
using namespace bettergps_overlay;
using Microsoft::WRL::ComPtr;
inline void run(const std::filesystem::path& out){
 std::filesystem::create_directories(out);managed_init(out);managed_bridges_init();gpsDisplays.clear();
 const auto markers=out/"BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew";std::filesystem::create_directories(markers);
 for(const auto* name:{"phone_layout_marker.dds","phone_layout_marker.tobj"})std::filesystem::copy_file(std::filesystem::path("BetterGPS/Overlay/BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew")/name,markers/name);
 ManagedDevice d;d.used=d.enabled=true;d.known=-1;d.style=3;d.startup=2;d.splash="copilot/t680";d.revision=1;d.prefs.power=1;
 managedDevices[0]=d;managedAppliedRevision[0]=1;managedPower[0].serial=1;managedPower[0].on=true;managedPower[0].phase=0;managedPower[0].session=phoneSessionSerial.load();managedPower[0].bootEpoch=vehicleStart.epoch.load();displayIgnition=1;
 renderStopping=false;renderGameplayEnabled=false;g_assetsReady=g_cpuRunning=true;
 Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Startup GPU hooks unavailable");
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
 Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Startup WARP device unavailable");
 const char* source=R"(
Texture2D image:register(t0);
float4 vs(uint id:SV_VertexID):SV_Position{return float4(id==1?3:-1,id==2?3:-1,0,1);}
float4 ps():SV_Target{return image.Load(int3(0,0,0));}
)";
 auto compile=[&](const char* entry,const char* profile){ComPtr<ID3DBlob> code,error;Check(SUCCEEDED(D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,entry,profile,0,0,&code,&error)),"Startup shader compile failed");return code;};
 auto vc=compile("vs","vs_5_0"),pc=compile("ps","ps_5_0");ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
 Check(SUCCEEDED(device->CreateVertexShader(vc->GetBufferPointer(),vc->GetBufferSize(),nullptr,&vs))&&SUCCEEDED(device->CreatePixelShader(pc->GetBufferPointer(),pc->GetBufferSize(),nullptr,&ps)),"Startup shaders unavailable");
 D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;ComPtr<ID3D11RasterizerState> raster;
 Check(SUCCEEDED(device->CreateRasterizerState(&rd,&raster)),"Startup rasterizer unavailable");
 D3D11_TEXTURE2D_DESC desc{};desc.Width=managed_marker_width(0,0);desc.Height=managed_marker_height(0);desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 managed_texture_file(managed_prefix(0,0),desc.Width,desc.Height,managed_marker_role(0,0));
 auto marker=display_bytes(out/"generated-displays"/(managed_prefix(0,0).substr(1)+".dds"));D3D11_SUBRESOURCE_DATA initial{marker.data()+128,desc.Width*4,0};
 ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11ShaderResourceView> view;
 Check(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&view)),"Startup marker unavailable");
 auto bound=[&](){auto* v=view.Get();context->PSSetShaderResources(0,1,&v);ComPtr<ID3D11ShaderResourceView> result;context->PSGetShaderResources(0,1,&result);Check(bool(result),"Startup binding missing");return result;};
 // Use the production binding before a worker image exists, then its first upload.
 auto waiting=bound();Check(waiting.Get()==g_managedBootBlack&&managedPower[0].splashShownAt==0,"Waiting frame consumed the visible startup clock");
 auto& prepared=g_managedPrepared[0];auto pixels=std::make_shared<std::vector<uint8_t>>(size_t(desc.Width)*desc.Height*4,10);for(size_t p=3;p<pixels->size();p+=4)(*pixels)[p]=255;
 prepared.pixels=pixels;prepared.version=1;prepared.powerState=1;prepared.powerSession=phoneSessionSerial.load();prepared.powerEpoch=vehicleStart.epoch.load();prepared.powerIgnition=1;prepared.powerPhase=0;g_managedPreparedRevision[0]=1;
 auto uploaded=bound();Check(uploaded.Get()==g_managedBridges[0]->liveView&&managedPower[0].splashShownAt>0,"First uploaded black did not start startup clock");
 auto render=[&](ID3D11DeviceContext* draw,ID3D11ShaderResourceView* image,DXGI_FORMAT format){
  D3D11_TEXTURE2D_DESC target{};target.Width=target.Height=target.MipLevels=target.ArraySize=target.SampleDesc.Count=1;target.Format=format;target.BindFlags=D3D11_BIND_RENDER_TARGET;
  ComPtr<ID3D11Texture2D> output,stage;ComPtr<ID3D11RenderTargetView> rtv;Check(SUCCEEDED(device->CreateTexture2D(&target,nullptr,&output))&&SUCCEEDED(device->CreateRenderTargetView(output.Get(),nullptr,&rtv)),"Startup target unavailable");
  target.BindFlags=0;target.Usage=D3D11_USAGE_STAGING;target.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Check(SUCCEEDED(device->CreateTexture2D(&target,nullptr,&stage)),"Startup readback unavailable");
  auto* rt=rtv.Get();D3D11_VIEWPORT viewport{0,0,1,1,0,1};draw->OMSetRenderTargets(1,&rt,nullptr);draw->RSSetViewports(1,&viewport);draw->RSSetState(raster.Get());draw->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);draw->VSSetShader(vs.Get(),nullptr,0);draw->PSSetShader(ps.Get(),nullptr,0);draw->PSSetShaderResources(0,1,&image);draw->Draw(3,0);draw->OMSetRenderTargets(0,nullptr,nullptr);draw->CopyResource(stage.Get(),output.Get());
  if(draw!=context.Get()){ComPtr<ID3D11CommandList> commands;Check(SUCCEEDED(draw->FinishCommandList(FALSE,&commands)),"Startup command list failed");context->ExecuteCommandList(commands.Get(),TRUE);}
  D3D11_MAPPED_SUBRESOURCE read{};Check(SUCCEEDED(context->Map(stage.Get(),0,D3D11_MAP_READ,0,&read)),"Startup GPU readback failed");std::array<uint8_t,4> color{};memcpy(color.data(),read.pData,4);context->Unmap(stage.Get(),0);return color;
 };
 ComPtr<ID3D11DeviceContext> deferred;Check(SUCCEEDED(device->CreateDeferredContext(0,&deferred)),"Startup deferred context unavailable");
 for(auto* draw:{context.Get(),deferred.Get()})for(auto format:{DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM}){
  const auto before=render(draw,waiting.Get(),format),after=render(draw,uploaded.Get(),format);
  std::cout<<"startup "<<(draw==context.Get()?"immediate":"deferred")<<" target="<<unsigned(format)<<" waiting="<<unsigned(before[0])<<" uploaded="<<unsigned(after[0])<<"\n";
  Check(before==after,"Gray flash: waiting black and first uploaded black differ after GPU sampling");
  if(format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)Check(before==std::array<uint8_t,4>{10,10,10,255},"Startup black is not RGB10 after sRGB rendering");
 }
 ++managedPower[0].serial;managedPower[0].splashShownAt=0;auto stale=bound();Check(stale.Get()==waiting.Get()&&managedPower[0].splashShownAt==0,"Stale image leaked at reignition");
 prepared.powerState=2;++prepared.version;auto ready=bound();Check(ready.Get()==uploaded.Get(),"Reignition black did not upload");
 std::promise<void> locked,release;auto unlock=release.get_future();auto worker=std::async(std::launch::async,[&]{std::lock_guard<std::mutex> lock(prepared.mutex);locked.set_value();unlock.wait();});locked.get_future().wait();auto contended=bound();release.set_value();worker.get();Check(contended.Get()==ready.Get(),"Worker contention replaced a valid black frame");
 const auto garmin=render(context.Get(),managed_black_view(context.Get()),DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);Check(garmin==std::array<uint8_t,4>{0,0,0,255},"Garmin fallback changed");
 context->ClearState();deferred->ClearState();waiting.Reset();uploaded.Reset();stale.Reset();ready.Reset();contended.Reset();managed_gpu_stop();release_live_locked(*g_managedBridges[0]);g_assetsReady=g_cpuRunning=false;uninstall_d3d_hooks();MH_Uninitialize();
 std::cout<<"PASS: real marker wait/upload, immediate/deferred GPU sampling, sRGB/linear targets, reignition, contention, visible clock and unchanged Garmin black.\n";
}
}
