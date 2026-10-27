#pragma once
#include <d3d11.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <array>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>
namespace bettergps_overlay {
// Explicit, one-session diagnosis. No readback unless the .once file exists.
// GPU copies are bounded; Map never waits. File writing runs on the CPU worker.
struct MapGpuAudit {
 struct Image {ID3D11Texture2D* stage=nullptr;ID3D11Device* device=nullptr;UINT w=0,h=0;DXGI_FORMAT format{};std::vector<uint8_t> pixels;bool requested=false,written=false;};
 std::array<Image,6> images;
 const char* names[6]={"private-map-sample","phone-before-marker","phone-clean-snapshot","phone-final","f150-map-source","f150-final"};
 std::mutex mutex;std::atomic<bool> active{false};std::filesystem::path base,trigger;std::atomic<uint64_t> origin{0};unsigned traceCount=0;std::vector<std::string> trace;
 void init(const std::filesystem::path& plugin){trigger=plugin/"BetterGPSMapAudit.once";base=plugin/"data/map-render-audit";active=std::filesystem::is_regular_file(trigger);if(active){std::filesystem::create_directories(base);std::ofstream reset(base/"bindings.txt",std::ios::trunc);std::ofstream pending(base/"status.txt",std::ios::trunc);pending<<"Awaiting live GPU capture after ignition.\n";}}
 void note(const std::string& message){if(!active)return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(lock&&traceCount++<96)trace.push_back(message);}
 void capture(ID3D11DeviceContext* context,ID3D11Texture2D* texture,unsigned slot){
  if(!active||displayIgnition.load()!=1||!texture||slot>=images.size()||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
  const auto now=GetTickCount64();if(!origin){origin=now;return;}if(now-origin<7000)return;
  std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock)return;auto& image=images[slot];if(image.requested)return;
  D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);const auto f=d.Format;bool supported=f==DXGI_FORMAT_B8G8R8A8_UNORM||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  if(!supported||d.Width>2048||d.Height>2048||d.SampleDesc.Count!=1||d.ArraySize!=1)return;
  ID3D11Device* device=nullptr;context->GetDevice(&device);auto read=d;read.MipLevels=1;read.BindFlags=read.MiscFlags=0;read.Usage=D3D11_USAGE_STAGING;read.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  PhoneInternalScope internal;if(FAILED(device->CreateTexture2D(&read,nullptr,&image.stage))){device->Release();return;}
  image.w=d.Width;image.h=d.Height;image.format=f;image.device=device;image.requested=true;context->CopySubresourceRegion(image.stage,0,0,0,0,texture,0,nullptr);
 }
 void sample(ID3D11DeviceContext* context,ID3D11ShaderResourceView* view,unsigned slot){if(!active||!view)return;ID3D11Resource* resource=nullptr;view->GetResource(&resource);ID3D11Texture2D* texture=nullptr;if(resource&&SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture))){capture(context,texture,slot);texture->Release();}if(resource)resource->Release();}
 void target(ID3D11DeviceContext* context,unsigned slot){if(!active)return;ID3D11RenderTargetView* target=nullptr;context->OMGetRenderTargets(1,&target,nullptr);ID3D11Resource* resource=nullptr;if(target){target->GetResource(&resource);target->Release();}ID3D11Texture2D* texture=nullptr;if(resource&&SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture))){capture(context,texture,slot);texture->Release();}if(resource)resource->Release();}
 void poll(ID3D11DeviceContext* context){if(!active)return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock)return;ID3D11Device* device=nullptr;context->GetDevice(&device);PhoneInternalScope internal;
  for(auto& image:images)if(image.stage&&image.device==device){D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(image.stage,0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped))){image.pixels.resize(size_t(image.w)*image.h*4);const bool rgba=image.format==DXGI_FORMAT_R8G8B8A8_UNORM||image.format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;for(UINT y=0;y<image.h;++y){auto* dst=image.pixels.data()+size_t(y)*image.w*4;memcpy(dst,(uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch,image.w*4);if(rgba)for(UINT x=0;x<image.w;++x)std::swap(dst[x*4],dst[x*4+2]);}context->Unmap(image.stage,0);image.stage->Release();image.stage=nullptr;image.device->Release();image.device=nullptr;}}
  device->Release();
 }
 void flush(){if(!active)return;std::lock_guard<std::mutex> lock(mutex);if(trace.empty()&&std::none_of(images.begin(),images.end(),[](const auto& image){return !image.written&&!image.pixels.empty();})&&(!origin||GetTickCount64()-origin<=30000))return;std::filesystem::create_directories(base);for(size_t i=0;i<images.size();++i){auto& image=images[i];if(image.written||image.pixels.empty())continue;BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+DWORD(image.pixels.size());info.biSize=sizeof(info);info.biWidth=image.w;info.biHeight=-LONG(image.h);info.biPlanes=1;info.biBitCount=32;info.biSizeImage=DWORD(image.pixels.size());std::ofstream output(base/(std::string(names[i])+".bmp"),std::ios::binary);output.write((char*)&file,sizeof(file));output.write((char*)&info,sizeof(info));output.write((char*)image.pixels.data(),image.pixels.size());image.written=output.good();if(image.written)image.pixels.clear();}
  if(!trace.empty()){std::ofstream out(base/"bindings.txt",std::ios::app);for(auto& line:trace)out<<line<<'\n';trace.clear();}
  if(origin&&(GetTickCount64()-origin>30000||std::all_of(images.begin(),images.end(),[](const auto& image){return image.written;}))){std::ofstream out(base/"status.txt");for(size_t i=0;i<images.size();++i)out<<names[i]<<" requested="<<images[i].requested<<" saved="<<images[i].written<<" size="<<images[i].w<<'x'<<images[i].h<<'\n';std::error_code ec;std::filesystem::remove(trigger,ec);active=false;}
 }
 void stop(){std::lock_guard<std::mutex> lock(mutex);for(auto& image:images){if(image.stage)image.stage->Release();if(image.device)image.device->Release();image.stage=nullptr;image.device=nullptr;image.pixels.clear();}active=false;}
};
inline MapGpuAudit mapGpuAudit;
}
