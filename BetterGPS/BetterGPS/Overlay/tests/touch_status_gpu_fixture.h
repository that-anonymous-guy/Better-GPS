#pragma once
namespace touch_status_gpu_fixture {
using namespace bettergps_overlay;
static void STDMETHODCALLTYPE bind(ID3D11DeviceContext* context,UINT first,UINT count,ID3D11ShaderResourceView* const* views){context->PSSetShaderResources(first,count,views);}
inline void run(const std::filesystem::path& out){
 std::filesystem::create_directories(out);ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;D3D_FEATURE_LEVEL level{};
 Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"Diagnostic HUD WARP device failed");
 ID3D11Texture2D* target=nullptr;ID3D11RenderTargetView* targetView=nullptr;D3D11_TEXTURE2D_DESC description{};description.Width=1280;description.Height=800;description.MipLevels=description.ArraySize=description.SampleDesc.Count=1;description.Format=DXGI_FORMAT_B8G8R8A8_UNORM;description.BindFlags=D3D11_BIND_RENDER_TARGET;
 Check(SUCCEEDED(device->CreateTexture2D(&description,nullptr,&target))&&SUCCEEDED(device->CreateRenderTargetView(target,nullptr,&targetView)),"Diagnostic HUD target failed");
 const float background[4]={.08f,.12f,.16f,1};context->ClearRenderTargetView(targetView,background);context->OMSetRenderTargets(1,&targetView,nullptr);
 menuDock.layout(1280,800);touchStatus.visible=true;TouchStatusContext status;status.menu=status.gameplay=status.foreground=status.eligible=true;status.width=1280;status.height=800;status.selected=3;status.name="Kenworth W900 NavPlus";status.page="HOME";status.state="splash (not interactive)";touchStatus.worker(out,status,GetTickCount64());
 SettingsMenuRenderer renderer;menuDockGpu.draw(context,target,renderer,&bind,1,1);Check(menuDockGpu.statusView&&menuDockGpu.uploaded,"Diagnostic HUD failed to upload through the actual menu renderer");
 ID3D11PixelShader* restored=nullptr;context->PSGetShader(&restored,nullptr,nullptr);Check(restored==nullptr,"Diagnostic HUD did not restore the pixel shader");ID3D11RenderTargetView* restoredView=nullptr;context->OMGetRenderTargets(1,&restoredView,nullptr);Check(restoredView==targetView,"Diagnostic HUD did not restore the render target");if(restoredView)restoredView->Release();
 description.BindFlags=0;description.Usage=D3D11_USAGE_STAGING;description.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* readback=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&description,nullptr,&readback)),"Diagnostic HUD readback allocation failed");context->CopyResource(readback,target);D3D11_MAPPED_SUBRESOURCE mapped{};Check(SUCCEEDED(context->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"Diagnostic HUD readback failed");std::vector<uint8_t> pixels(size_t(1280)*800*4);for(unsigned y=0;y<800;++y)memcpy(pixels.data()+size_t(y)*1280*4,(uint8_t*)mapped.pData+y*mapped.RowPitch,1280*4);context->Unmap(readback,0);Save(out/"touch-status-ingame-layout.bmp",pixels,1280,800);
 const auto rect=menuDock.diagnostics();const auto at=(size_t(rect.top+20)*1280+rect.left+20)*4;Check(pixels[at]!=pixels[0]||pixels[at+1]!=pixels[1]||pixels[at+2]!=pixels[2],"Diagnostic HUD target remained blank");
 touchStatus.visible=false;touchStatus.finish();menuDockGpu.stop();renderer.stop();readback->Release();targetView->Release();target->Release();context->Release();device->Release();
 std::cout<<"PASS: real diagnostic HUD texture upload, dock placement and Direct3D menu draw; pixel shader and render target restored. Isolated WARP test, not Snowymoon compatibility certification.\n";
}
}
