#pragma once
#include "TouchGeometryGpu.h"
namespace bettergps_overlay {
struct MenuDockGpu {
 TouchGeometryGpu geometry;
 ID3D11Device* device=nullptr;ID3D11ShaderResourceView* header=nullptr,*reset=nullptr,*diagnosticsOn=nullptr,*diagnosticsOff=nullptr,*statusView=nullptr;ID3D11Texture2D* statusTexture=nullptr;uint64_t uploaded=0;
 void stop(){geometry.stop();SettingsMenuRenderer::drop(header);SettingsMenuRenderer::drop(reset);SettingsMenuRenderer::drop(diagnosticsOn);SettingsMenuRenderer::drop(diagnosticsOff);SettingsMenuRenderer::drop(statusView);SettingsMenuRenderer::drop(statusTexture);SettingsMenuRenderer::drop(device);uploaded=0;}
 void image(ID3D11ShaderResourceView*& view,unsigned w,unsigned h,const wchar_t* text){
  if(view)return;const auto pixels=menu_tile(w,h,text);D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA data{pixels.data(),w*4,0};ID3D11Texture2D* texture=nullptr;PhoneInternalScope own;
  if(SUCCEEDED(device->CreateTexture2D(&d,&data,&texture))){device->CreateShaderResourceView(texture,nullptr,&view);texture->Release();}
 }
 void draw(ID3D11DeviceContext* context,ID3D11Texture2D* target,SettingsMenuRenderer& renderer,void(STDMETHODCALLTYPE* bind)(ID3D11DeviceContext*,UINT,UINT,ID3D11ShaderResourceView* const*),float ratioX,float ratioY){
  ID3D11Device* current=nullptr;context->GetDevice(&current);if(current!=device){stop();device=current;device->AddRef();}current->Release();image(header,540,84,L"Better GPS Menu");image(reset,116,52,L"Reset");
  geometry.draw(context,device,target,renderer,bind,ratioX,ratioY);
  auto paint=[&](ID3D11ShaderResourceView* view,RECT r){if(view)renderer.render(context,view,target,r.left*ratioX,r.top*ratioY,(r.right-r.left)*ratioX,(r.bottom-r.top)*ratioY,bind);};paint(header,menuDock.header());paint(reset,menuDock.reset());
  image(diagnosticsOn,336,52,L"Touch diag: ON");image(diagnosticsOff,336,52,L"Touch diag: OFF");paint(touchStatus.visible?diagnosticsOn:diagnosticsOff,menuDock.diagnostics_toggle());
  if(touchStatus.visible){
   std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0;{std::unique_lock<std::mutex> lock(touchStatus.imageMutex,std::try_to_lock);if(lock){pixels=touchStatus.image;version=touchStatus.imageVersion;}}
   if(pixels){PhoneInternalScope own;if(!statusTexture){D3D11_TEXTURE2D_DESC d{};d.Width=TouchStatus::imageWidth;d.Height=TouchStatus::imageHeight;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.Usage=D3D11_USAGE_DYNAMIC;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;if(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&statusTexture))&&FAILED(device->CreateShaderResourceView(statusTexture,nullptr,&statusView)))SettingsMenuRenderer::drop(statusTexture);}
    if(statusTexture&&version!=uploaded){D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(statusTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){for(unsigned y=0;y<TouchStatus::imageHeight;++y)memcpy((uint8_t*)mapped.pData+y*mapped.RowPitch,pixels->data()+size_t(y)*TouchStatus::imageWidth*4,TouchStatus::imageWidth*4);context->Unmap(statusTexture,0);uploaded=version;}}
    if(uploaded)paint(statusView,menuDock.diagnostics());
   }
  }
 }
};
inline MenuDockGpu menuDockGpu;
}
