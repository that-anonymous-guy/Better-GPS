#pragma once
namespace t680_cluster_fixture {
using namespace bettergps_overlay;
using Microsoft::WRL::ComPtr;
inline bool valid_unit_name(const std::string& name){
 if(name.empty())return false;
 size_t first=name[0]=='.'?1:0;for(;;){const auto end=name.find('.',first),length=(end==std::string::npos?name.size():end)-first;if(!length||length>12)return false;for(size_t i=first;i<first+length;++i)if(!(name[i]>='a'&&name[i]<='z'||name[i]>='0'&&name[i]<='9'||name[i]=='_'))return false;if(end==std::string::npos)return true;first=end+1;}
}
inline void run(const std::filesystem::path& out){
 std::filesystem::create_directories(out);
 ClusterIntroClock clock;Check(clock.sample(1000,1,1,false)==-1,"Cluster plays before contact");
 Check(clock.sample(2000,1,1,true)==0&&clock.sample(9000,1,1,true)==0,"Cluster timer runs before first visible upload");
 auto serial=clock.serial;Check(!clock.present(serial-1,9000)&&clock.present(serial,9000),"Cluster accepts stale ignition pixels");
 Check(clock.sample(18000,1,1,true)==9000&&ClusterIntroClock::frame(9000)==269&&ClusterIntroClock::alpha(9000)==255&&ClusterIntroClock::alpha(9500)==127&&ClusterIntroClock::alpha(10000)==0,"Last second not replaced with a held-frame crossfade");
 clock.pause(18000);Check(clock.sample(28000,1,1,true)==9000,"Menu pause consumed cluster fade");
 Check(clock.sample(29000,1,1,true)==10000&&clock.sample(39000,1,1,true)==10000,"Completed cluster restarted while contact stayed on");
 clock.sample(40000,1,1,false);Check(clock.sample(41000,1,1,true)==0&&clock.serial!=serial,"Cluster did not restart at next contact");
 clock.present(clock.serial,41000);Check(clock.sample(42000,1,2,true)==0,"New vehicle initialization retained prior intro");
 const auto assets=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets");
 cluster_intro_load(assets.parent_path());Check(clusterIntroAssetReady&&clusterIntroAnimation->count==270,"Cluster movie missing/truncated");
 std::vector<uint8_t> decoded;for(unsigned frame=0;frame<270;++frame)Check(clusterIntroAnimation->decode(frame,decoded),"Cluster lost a retained lossless frame");
 uint64_t pixelHash=14695981039346656037ull;for(auto byte:decoded){pixelHash^=byte;pixelHash*=1099511628211ull;}Check(pixelHash==0x658e13b88f0e7959ull,"Cluster logo differs from the independently decoded source (RGB10 black)");Save(out/"lossless-frame-269.bmp",decoded,clusterWidth,clusterHeight);
 const auto uiBytes=display_bytes(assets/"ui/dashboard/kenworth_t680_2022.sii");std::string ui(uiBytes.begin(),uiBytes.end());ui.erase(std::remove(ui.begin(),ui.end(),'\r'),ui.end());
 Check(!valid_unit_name("_nameless.bettergps_cluster_intro")&&valid_unit_name("_nameless.bgcl_intro"),"Native SCS 12-character token regression not detected");
 {std::istringstream lines(ui);std::string line;unsigned count=0;while(std::getline(lines,line)){if(line.rfind("ui",0)!=0)continue;const auto colon=line.find(" : "),brace=line.find(" {",colon);if(colon==std::string::npos||brace==std::string::npos)continue;Check(valid_unit_name(line.substr(colon+3,brace-colon-3)),"Cluster unit name rejected by native ATS token limits");++count;}Check(count==131,"Cluster unit declaration count changed");}
 Check(ui.find("my_children: 6\n")!=std::string::npos&&ui.find("my_children[5]: _nameless.bgcl_intro")!=std::string::npos&&ui.find("layer: 127")!=std::string::npos&&ui.find("right=0.9961089494")!=std::string::npos,"Cluster additive UI/crop missing");
 // Removing the one additive child must recover every original instrument byte.
 auto original=ui;const auto child=std::string(" my_children[5]: _nameless.bgcl_intro\n");original.erase(original.find(child),child.size());original.replace(original.find("my_children: 6"),14,"my_children: 5");const auto node=original.find("\nui::text : _nameless.bgcl_intro");Check(node!=std::string::npos,"Cluster marker definition missing");original=original.substr(0,node)+"}\n";
 while(!original.empty()&&isspace((unsigned char)original.back()))original.pop_back();uint64_t fingerprint=14695981039346656037ull;for(unsigned char byte:original){fingerprint^=byte;fingerprint*=1099511628211ull;}Check(original.size()==82103&&fingerprint==0xd2bb206a598188d3ull,"Cluster intro changed the audited ATS 1.61 native instruments/pages");
 managed_init(out);gpsDisplays.clear();displayVehicle="kenworth.t680_2022";vehicleStart.eligible=false;vehicleStart.epoch=17;phoneSessionSerial=19;displayGlobalEnabled=true;displayIgnition=-1;renderStopping=false;renderGameplayEnabled=true;renderGameplayActive=false;g_assetsReady=g_cpuRunning=true;
 {std::lock_guard<std::mutex> lock(clusterIntroMutex);clusterIntroClock={};}
 Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Cluster GPU hooks unavailable");ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
 Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Cluster WARP unavailable");
 const auto marker=display_bytes(assets/"material/ui/dashboard/bettergps_overlaynew/t680_cluster_intro.dds");D3D11_TEXTURE2D_DESC desc{};desc.Width=clusterWidth;desc.Height=clusterHeight;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{marker.data()+128,clusterWidth*4,0};
 ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11ShaderResourceView> view;Check(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&view)),"Cluster marker unavailable");
 auto bound=[&](){auto* v=view.Get();context->PSSetShaderResources(0,1,&v);ComPtr<ID3D11ShaderResourceView> result;context->PSGetShaderResources(0,1,&result);return result;};
 Check(bound().Get()==g_managedBootBlack,"Native cluster leaked during loading before gameplay/telemetry");renderGameplayActive=true;displayIgnition=0;Check(bound().Get()==g_managedBootBlack,"Native cluster leaked before contact");displayIgnition=1;Check(bound().Get()==g_managedBootBlack&&clusterIntroClock.shown==0,"Unconfirmed player initialization exposed the instruments");vehicleStart.eligible=true;cluster_intro_prepare(GetTickCount64());Check(bound().Get()==g_clusterIntroBridge.liveView&&clusterIntroClock.shown>0,"First actual cluster frame did not start its own timer");
 const char* shader=R"(
Texture2D image:register(t0);SamplerState samp:register(s0);
struct P{float4 p:SV_Position;float2 uv:TEXCOORD0;};
P vs(uint id:SV_VertexID){P o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv.x*2-1,1-o.uv.y*2,0,1);return o;}
float4 ps(P p):SV_Target{return image.Sample(samp,p.uv*float2(1024./1028.,1));}
)";
 auto compile=[&](const char* entry,const char* profile){ComPtr<ID3DBlob> code,error;Check(SUCCEEDED(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,entry,profile,0,0,&code,&error)),"Cluster shader compile failed");return code;};auto vc=compile("vs","vs_5_0"),pc=compile("ps","ps_5_0");ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;device->CreateVertexShader(vc->GetBufferPointer(),vc->GetBufferSize(),nullptr,&vs);device->CreatePixelShader(pc->GetBufferPointer(),pc->GetBufferSize(),nullptr,&ps);
 D3D11_BLEND_DESC blend{};blend.RenderTarget[0]={TRUE,D3D11_BLEND_SRC_ALPHA,D3D11_BLEND_INV_SRC_ALPHA,D3D11_BLEND_OP_ADD,D3D11_BLEND_ONE,D3D11_BLEND_INV_SRC_ALPHA,D3D11_BLEND_OP_ADD,D3D11_COLOR_WRITE_ENABLE_ALL};ComPtr<ID3D11BlendState> bs;Check(SUCCEEDED(device->CreateBlendState(&blend,&bs)),"Cluster native alpha blend unavailable");
 D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=D3D11_FLOAT32_MAX;ComPtr<ID3D11SamplerState> ss;device->CreateSamplerState(&sampler,&ss);
 D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;ComPtr<ID3D11RasterizerState> rs;device->CreateRasterizerState(&raster,&rs);
 auto targetDesc=desc;targetDesc.Width=128;targetDesc.Height=48;targetDesc.BindFlags=D3D11_BIND_RENDER_TARGET;ComPtr<ID3D11Texture2D> target,stage;ComPtr<ID3D11RenderTargetView> rtv;device->CreateTexture2D(&targetDesc,nullptr,&target);device->CreateRenderTargetView(target.Get(),nullptr,&rtv);targetDesc.BindFlags=0;targetDesc.Usage=D3D11_USAGE_STAGING;targetDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;device->CreateTexture2D(&targetDesc,nullptr,&stage);
 auto draw=[&](ID3D11DeviceContext* draw,const std::array<float,4>& nativeColor){
  auto* rt=rtv.Get();draw->ClearRenderTargetView(rt,nativeColor.data());draw->OMSetRenderTargets(1,&rt,nullptr);draw->OMSetBlendState(bs.Get(),nullptr,~0u);D3D11_VIEWPORT viewport{0,0,128,48,0,1};draw->RSSetViewports(1,&viewport);draw->RSSetState(rs.Get());draw->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);draw->VSSetShader(vs.Get(),nullptr,0);draw->PSSetShader(ps.Get(),nullptr,0);auto* samp=ss.Get();draw->PSSetSamplers(0,1,&samp);auto* v=view.Get();draw->PSSetShaderResources(0,1,&v);draw->Draw(3,0);draw->OMSetRenderTargets(0,nullptr,nullptr);draw->CopyResource(stage.Get(),target.Get());
  if(draw!=context.Get()){ComPtr<ID3D11CommandList> commands;Check(SUCCEEDED(draw->FinishCommandList(FALSE,&commands)),"Cluster deferred native UI failed");context->ExecuteCommandList(commands.Get(),TRUE);}
  D3D11_MAPPED_SUBRESOURCE read{};Check(SUCCEEDED(context->Map(stage.Get(),0,D3D11_MAP_READ,0,&read)),"Cluster readback failed");std::vector<uint8_t> pixels(128*48*4);for(unsigned y=0;y<48;++y)memcpy(pixels.data()+y*128*4,(uint8_t*)read.pData+y*read.RowPitch,128*4);context->Unmap(stage.Get(),0);return pixels;
 };
 ComPtr<ID3D11DeviceContext> deferred;device->CreateDeferredContext(0,&deferred);
 for(auto* drawContext:{context.Get(),deferred.Get()})for(int phase:{-2,8900,9500,10000}){
  vehicleStart.eligible=phase>=0;auto now=GetTickCount64();{std::lock_guard<std::mutex> lock(clusterIntroMutex);clusterIntroClock.sample(now,phoneSessionSerial,vehicleStart.epoch,cluster_intro_powered());if(phase>=0)clusterIntroClock.shown=now-phase;}
  cluster_intro_prepare(now);const auto a=draw(drawContext,{.1f,.3f,.6f,1}),b=draw(drawContext,{.6f,.1f,.3f,1});
  Check((a!=b)==(phase>=9000),"Cluster fades to black instead of revealing LIVE instruments");
  if(phase<0)for(size_t pixel=0;pixel<a.size();pixel+=4)Check(a[pixel]==10&&a[pixel+1]==10&&a[pixel+2]==10&&a[pixel+3]==255,"Waiting/initialization guard is not exact opaque RGB10 after GPU rendering");
  if(phase==10000)Check(a[0]>190&&a[1]>140&&a[2]>80,"Final instruments popped through a black intermediate frame");
  Save(out/(std::string(drawContext==context.Get()?"immediate-":"deferred-")+std::to_string(phase)+".bmp"),a,128,48);
 }
 clusterIntroQueued=true;managedWindowPaths[123]="/ui/dashboard/kenworth_t680_2022.sii";managedWindowPaths[124]="/ui/dashboard/kenworth_t680_2022_gps.sii";Check(managed_native_wake((void*)123)&&!managed_native_wake((void*)124),"Cluster intro woke the GPS or a different native window");clusterIntroQueued=false;
 renderGameplayActive=false;Check(bound().Get()==g_clusterIntroBridge.disabledView,"Returning to a menu restarted the completed intro guard");renderGameplayActive=true;
 displayIgnition=0;Check(bound().Get()==g_managedBootBlack,"Cutting contact leaves a gap before the next intro");displayIgnition=1;Check(bound().Get()==g_managedBootBlack,"Reignition exposed previous movie/fade pixels");cluster_intro_prepare(GetTickCount64());Check(bound().Get()==g_clusterIntroBridge.liveView,"Reignition failed to restart first frame");
 displayVehicle="kenworth.w900";Check(bound().Get()==g_clusterIntroBridge.disabledView,"Cluster intro leaked to another truck");
 context->ClearState();deferred->ClearState();g_assetsReady=g_cpuRunning=false;uninstall_d3d_hooks();MH_Uninitialize();std::cout<<"PASS: additive byte-preserved native instruments, 270 frames/30fps + 1s held-frame fade, real immediate/deferred marker binding and alpha over changing native instruments, contact/off/reignition, pause, vehicle/session isolation, exact cluster-only wake.\n";
}
}
