#pragma once
#include "PhoneScreenPower.h"
namespace bettergps_overlay {
// A compositor for the F150's first native page. The game still owns its page
// cycle, ignition and perspective. No phone startup/power state is applied.
struct HorizontalSurface {
 PhoneScreenPower isolation;ID3D11Texture2D* canvas{};ID3D11RenderTargetView* target{};uint64_t composed=~uint64_t(0),seen=0;
 ~HorizontalSurface(){SettingsMenuRenderer::drop(target);SettingsMenuRenderer::drop(canvas);}
 bool start(ID3D11Device* device){if(isolation.renderer.device==device)return true;
  composed=~uint64_t(0);SettingsMenuRenderer::drop(target);SettingsMenuRenderer::drop(canvas);D3D11_TEXTURE2D_DESC d{};d.Width=800;d.Height=500;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
  std::vector<uint8_t> black(800*500*4,0);for(size_t i=3;i<black.size();i+=4)black[i]=255;D3D11_SUBRESOURCE_DATA initial{black.data(),800*4,0};
  return SUCCEEDED(device->CreateTexture2D(&d,&initial,&canvas))&&SUCCEEDED(device->CreateRenderTargetView(canvas,nullptr,&target))&&isolation.start(canvas);
 }
 ID3D11ShaderResourceView* compose(ID3D11DeviceContext* context,ID3D11ShaderResourceView* map,ID3D11ShaderResourceView* overlay,unsigned stride,uint64_t now,bool dark){
  ID3D11Device* device=nullptr;context->GetDevice(&device);const bool ok=start(device);device->Release();if(!ok||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return nullptr;
  if(composed==now)return isolation.nativeView;
  if(map){ID3D11Device* source=nullptr;map->GetDevice(&source);const bool same=source==isolation.renderer.device;source->Release();if(!same)map=nullptr;}
  PhoneInternalScope internal;ID3DDeviceContextState* saved=nullptr;ID3D11Predicate* predicate=nullptr;BOOL predValue=FALSE;auto* work=isolation.deferred?isolation.deferred:context;
  if(!isolation.deferred){context->GetPredication(&predicate,&predValue);isolation.isolatedContext->SwapDeviceContextState(isolation.isolatedState,&saved);}
  work->ClearState();work->SetPredication(nullptr,FALSE);const float bg[]={dark?.0185f:.75f,dark?.0295f:.75f,dark?.045f:.75f,1};work->ClearRenderTargetView(target,bg);
  auto& r=isolation.renderer;D3D11_VIEWPORT viewport{0,0,800,500,0,1};work->OMSetRenderTargets(1,&target,nullptr);work->RSSetState(r.raster);work->RSSetViewports(1,&viewport);work->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);work->VSSetShader(r.vs,nullptr,0);work->PSSetShader(r.ps,nullptr,0);work->PSSetSamplers(0,1,&r.sampler);work->PSSetConstantBuffers(0,1,&r.constants);
  const float mapParameters[]={340.625f/712.5f,0,-1,0},overlayParameters[]={800.f/stride,0,-2,0};
  if(map){D3D11_VIEWPORT mapViewport{87.5f,90.625f,712.5f,340.625f,0,1};work->RSSetViewports(1,&mapViewport);work->UpdateSubresource(r.constants,0,nullptr,mapParameters,0,0);work->PSSetShaderResources(0,1,&map);work->Draw(3,0);}
  work->RSSetViewports(1,&viewport);work->OMSetBlendState(r.blend,nullptr,0xffffffff);work->UpdateSubresource(r.constants,0,nullptr,overlayParameters,0,0);work->PSSetShaderResources(0,1,&overlay);work->Draw(3,0);ID3D11ShaderResourceView* none=nullptr;work->PSSetShaderResources(0,1,&none);work->OMSetRenderTargets(0,nullptr,nullptr);
  bool complete=true;if(isolation.deferred){ID3D11CommandList* commands=nullptr;complete=SUCCEEDED(isolation.deferred->FinishCommandList(FALSE,&commands));if(complete){context->ExecuteCommandList(commands,TRUE);commands->Release();}}
  else{isolation.isolatedContext->SwapDeviceContextState(saved,nullptr);if(saved)saved->Release();context->SetPredication(predicate,predValue);if(predicate)predicate->Release();}
  if(complete)composed=now;return complete?isolation.nativeView:nullptr;
 }
};
}
