#pragma once
#include "TouchGeometryView.h"
namespace bettergps_overlay {
// Snapshot the native scene before reuse, not when its delayed CPU projection
// happens to finish. Copies only; never query/replace/replay native shaders.
inline DXGI_FORMAT touch_depth_storage(DXGI_FORMAT f){switch(f){
 case DXGI_FORMAT_D16_UNORM:case DXGI_FORMAT_R16_TYPELESS:return DXGI_FORMAT_R16_TYPELESS;
 case DXGI_FORMAT_D24_UNORM_S8_UINT:case DXGI_FORMAT_R24G8_TYPELESS:return DXGI_FORMAT_R24G8_TYPELESS;
 case DXGI_FORMAT_D32_FLOAT:case DXGI_FORMAT_R32_TYPELESS:return DXGI_FORMAT_R32_TYPELESS;
 case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:case DXGI_FORMAT_R32G8X24_TYPELESS:return DXGI_FORMAT_R32G8X24_TYPELESS;
 default:return DXGI_FORMAT_UNKNOWN;}}
inline float touch_depth_decode(const void* data,DXGI_FORMAT viewFormat){
 if(viewFormat==DXGI_FORMAT_D16_UNORM){uint16_t n;memcpy(&n,data,2);return n/65535.f;}
 if(viewFormat==DXGI_FORMAT_D24_UNORM_S8_UINT){uint32_t n;memcpy(&n,data,4);return float(n&0xffffff)/16777215.f;}
 float n=-1;if(viewFormat==DXGI_FORMAT_D32_FLOAT||viewFormat==DXGI_FORMAT_D32_FLOAT_S8X24_UINT)memcpy(&n,data,4);return n;
}
inline void touch_depth_seal(ID3D11DeviceContext* context,TouchGeometryDepth& d,const char* boundary){
 if(!d.usable||d.sealed||!d.trace)return;d.sealed=true;d.boundary=boundary;PhoneInternalScope own;
 Microsoft::WRL::ComPtr<ID3D11Device> device;context->GetDevice(&device);auto desc=d.desc;const auto storage=touch_depth_storage(d.viewFormat);
 if(storage==DXGI_FORMAT_UNKNOWN||storage!=touch_depth_storage(desc.Format)||desc.SampleDesc.Count!=1||desc.ArraySize!=1||desc.MipLevels!=1||desc.Width>3840||desc.Height>2160||d.x>=desc.Width||d.y>=desc.Height){d.usable=false;d.reason="Depth format/view/sample/dimensions unsupported";return;}
 const size_t needed=size_t(desc.Width)*desc.Height*(storage==DXGI_FORMAT_R16_TYPELESS?2:storage==DXGI_FORMAT_R32G8X24_TYPELESS?8:4);size_t used=touchDepthBytes.load();do{if(used+needed>64*1024*1024){d.usable=false;d.reason="Depth snapshot 64 MiB shared budget exceeded";return;}}while(!touchDepthBytes.compare_exchange_weak(used,used+needed));d.reserved=needed;
 desc.Format=storage;desc.BindFlags=desc.MiscFlags=desc.CPUAccessFlags=0;desc.Usage=D3D11_USAGE_DEFAULT;
 auto hr=device->CreateTexture2D(&desc,nullptr,&d.frozen);desc.Width=desc.Height=1;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;if(SUCCEEDED(hr))hr=device->CreateTexture2D(&desc,nullptr,&d.pixel);
 D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};if(SUCCEEDED(hr))hr=device->CreateQuery(&query,&d.completed);
 if(FAILED(hr)){d.usable=false;char why[128];snprintf(why,sizeof(why),"Depth snapshot allocation hr=0x%08lx",static_cast<unsigned long>(hr));d.reason=why;return;}
 context->CopyResource(d.frozen.Get(),d.texture.Get());const D3D11_BOX box{d.x,d.y,0,d.x+1,d.y+1,1};context->CopySubresourceRegion(d.pixel.Get(),0,0,0,0,d.frozen.Get(),0,&box);context->End(d.completed.Get());
}
inline bool touch_depth_poll(ID3D11DeviceContext* context,TouchGeometryDepth& d){
 if(!d.usable||!d.trace||d.ready)return true;if(!d.sealed)return false;
 BOOL done=FALSE;auto hr=context->GetData(d.completed.Get(),&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);if(hr==S_FALSE||SUCCEEDED(hr)&&!done)return false;
 D3D11_MAPPED_SUBRESOURCE data{};if(SUCCEEDED(hr))hr=context->Map(d.pixel.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&data);if(hr==DXGI_ERROR_WAS_STILL_DRAWING)return false;
 if(FAILED(hr)){d.usable=false;char why[128];snprintf(why,sizeof(why),"Depth snapshot query/map hr=0x%08lx",static_cast<unsigned long>(hr));d.reason=why;return true;}
 const unsigned bytes=d.viewFormat==DXGI_FORMAT_D16_UNORM?2:d.viewFormat==DXGI_FORMAT_D32_FLOAT_S8X24_UINT?8:4;memcpy(d.raw.data(),data.pData,bytes);d.actual=touch_depth_decode(d.raw.data(),d.viewFormat);context->Unmap(d.pixel.Get(),0);d.ready=true;
 d.pixel.Reset();d.frozen.Reset();d.completed.Reset();d.texture.Reset();
 if(d.reserved){touchDepthBytes.fetch_sub(d.reserved);d.reserved=0;}
 if(!std::isfinite(d.actual)||d.actual<0||d.actual>1){d.usable=false;d.reason="Frozen depth texel outside [0,1] (raw bytes retained)";}return true;
}
}
