#pragma once
#include <wrl/client.h>
#include <optional>
#include <fstream>
namespace bettergps_overlay {
// Explicit Touch diag only: one failed native RGB mapping per device/capture.
// The normal picking path still maps only its four-byte result. Failure images
// are asynchronously staged, then written by the existing preparation worker.
struct TouchCopyEvidence {
 template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
 struct Image {unsigned w=0,h=0;DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;unsigned alphaMin=255,alphaMax=0;std::vector<uint8_t> pixels;};
 struct Report {unsigned slot=0,mask=0;uint64_t trace=0,session=0;std::array<Image,2> image;};
 struct Readback {Report report;uint64_t at=0;std::array<Ptr<ID3D11Texture2D>,2> texture;Ptr<ID3D11Query> done;Ptr<ID3D11Device> device;};
 std::optional<Readback> pending;std::array<uint64_t,managedDeviceCount> saved{};std::mutex mutex;std::deque<Report> reports;
 void stop(){pending.reset();saved={};std::lock_guard<std::mutex> lock(mutex);reports.clear();}
 void capture(ID3D11Device* device,ID3D11DeviceContext* c,ID3D11Texture2D* source,ID3D11Texture2D* target,unsigned slot,uint64_t trace,unsigned mask){
  const auto session=touchStatus.sourceGeneration.load();if(!touchStatus.visible||!session||slot>=managedDeviceCount||saved[slot]==session||pending)return;
  Readback job;job.device=device;job.at=GetTickCount64();job.report.slot=slot;job.report.trace=trace;job.report.session=session;job.report.mask=mask;ID3D11Texture2D* inputs[]={source,target};
  for(unsigned i=0;i<2;++i){D3D11_TEXTURE2D_DESC d{};inputs[i]->GetDesc(&d);if(uint64_t(d.Width)*d.Height>1024*1024||d.SampleDesc.Count!=1||d.ArraySize!=1)return;auto& image=job.report.image[i];image.w=d.Width;image.h=d.Height;image.format=d.Format;d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;if(FAILED(device->CreateTexture2D(&d,nullptr,&job.texture[i])))return;c->CopyResource(job.texture[i].Get(),inputs[i]);}
  D3D11_QUERY_DESC q{D3D11_QUERY_EVENT,0};if(FAILED(device->CreateQuery(&q,&job.done)))return;c->End(job.done.Get());saved[slot]=session;pending=std::move(job);
 }
 void poll(ID3D11DeviceContext* c){
  if(!pending)return;auto& job=*pending;if(job.report.session!=touchStatus.sourceGeneration.load()||GetTickCount64()-job.at>300){pending.reset();return;}
  Ptr<ID3D11Device> device;c->GetDevice(&device);if(device.Get()!=job.device.Get()){pending.reset();return;}
  BOOL done=FALSE;const auto hr=c->GetData(job.done.Get(),&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);if(FAILED(hr)){pending.reset();return;}if(hr!=S_OK||!done)return;
  for(unsigned i=0;i<2;++i){auto& image=job.report.image[i];if(!image.pixels.empty())continue;D3D11_MAPPED_SUBRESOURCE map{};const auto read=c->Map(job.texture[i].Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map);if(read==DXGI_ERROR_WAS_STILL_DRAWING)return;if(FAILED(read)){pending.reset();return;}image.pixels.resize(size_t(image.w)*image.h*4);for(unsigned y=0;y<image.h;++y)memcpy(image.pixels.data()+size_t(y)*image.w*4,static_cast<const uint8_t*>(map.pData)+size_t(y)*map.RowPitch,image.w*4);c->Unmap(job.texture[i].Get(),0);
   const bool rgba=image.format==DXGI_FORMAT_R8G8B8A8_UNORM||image.format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||image.format==DXGI_FORMAT_R8G8B8A8_TYPELESS;for(size_t p=0;p<image.pixels.size();p+=4){if(rgba)std::swap(image.pixels[p],image.pixels[p+2]);image.alphaMin=std::min(image.alphaMin,unsigned(image.pixels[p+3]));image.alphaMax=std::max(image.alphaMax,unsigned(image.pixels[p+3]));image.pixels[p+3]=255;} // Preview RGB even if native alpha is zero; retain extrema in text.
  }
  {std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(lock&&reports.empty())reports.push_back(std::move(job.report));}pending.reset();
 }
 static bool save(const std::filesystem::path& file,const Image& image){BITMAPFILEHEADER f{};BITMAPINFOHEADER h{};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(h);f.bfSize=DWORD(f.bfOffBits+image.pixels.size());h.biSize=sizeof(h);h.biWidth=image.w;h.biHeight=-LONG(image.h);h.biPlanes=1;h.biBitCount=32;h.biCompression=BI_RGB;std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&h),sizeof(h));out.write(reinterpret_cast<const char*>(image.pixels.data()),image.pixels.size());return bool(out);}
 void flush(const std::filesystem::path& base){
  std::deque<Report> ready;{std::lock_guard<std::mutex> lock(mutex);ready.swap(reports);}for(const auto& report:ready){const auto root=base/"logs/touch";std::error_code ec;std::filesystem::create_directories(root,ec);const auto prefix="copy-slot-"+std::to_string(report.slot);const bool ok=save(root/(prefix+"-source.bmp"),report.image[0])&&save(root/(prefix+"-target.bmp"),report.image[1]);std::ofstream note(root/(prefix+".txt"));note<<"slot="<<report.slot<<" trace="<<report.trace<<" capture="<<report.session<<" rgba_mismatch="<<std::hex<<report.mask<<std::dec<<"\nRGB previews only; alpha is not changed in the game.\n";for(unsigned i=0;i<2;++i){const auto& image=report.image[i];note<<(i?"target":"source")<<"="<<image.w<<'x'<<image.h<<" format="<<unsigned(image.format)<<" alpha="<<image.alphaMin<<".."<<image.alphaMax<<'\n';}char line[200];snprintf(line,sizeof(line),"slot=%u trace=%llu saved=%u files=%s-source.bmp,%s-target.bmp",report.slot,report.trace,unsigned(ok&&bool(note)),prefix.c_str(),prefix.c_str());touchStatus.diagnostic_event(report.session,"copy-evidence",line);}
 }
};
inline TouchCopyEvidence touchCopyEvidence;
}
