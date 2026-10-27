#pragma once
#include <string>
#include "LockscreenStyle.h"
namespace bettergps_overlay {
// Worker-owned file cache. Stat at most once a second while this display is
// active; decode only changed files on a separate software D3D device.
static std::array<std::vector<uint8_t>,2> horizontalCarplayPixels;
static uint64_t horizontalCarplayRevision=0;
static std::map<std::string,std::vector<uint8_t>> managedFramePixels;
static uint64_t managedFrameRevision=0;
static std::vector<uint8_t> frame_wic(const std::filesystem::path& path,unsigned& width,unsigned& height){const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);IWICImagingFactory* factory=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;IWICFormatConverter* converter=nullptr;std::vector<uint8_t> result;UINT w=0,h=0;bool ok=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))&&SUCCEEDED(decoder->GetFrame(0,&frame))&&SUCCEEDED(frame->GetSize(&w,&h))&&w&&h&&w<=4096&&h<=4096&&SUCCEEDED(factory->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(frame,GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));if(ok){result.resize(size_t(w)*h*4);if(FAILED(converter->CopyPixels(nullptr,w*4,UINT(result.size()),result.data())))result.clear();}SettingsMenuRenderer::drop(converter);SettingsMenuRenderer::drop(frame);SettingsMenuRenderer::drop(decoder);SettingsMenuRenderer::drop(factory);if(SUCCEEDED(com))CoUninitialize();if(!result.empty()){width=w;height=h;}return result;}
static void managed_frame_prepare(const std::filesystem::path& root,uint64_t now){static std::map<std::string,std::string> keys;static uint64_t next=0;if(now<next)return;next=now+1000;HomeDecoder decoder;std::error_code ec;for(const auto& entry:std::filesystem::directory_iterator(root,std::filesystem::directory_options::skip_permission_denied,ec)){if(!entry.is_regular_file(ec))continue;auto ext=home_lower(entry.path().extension().u8string());if(ext!=".dds"&&ext!=".png"&&ext!=".bmp")continue;auto time=std::filesystem::last_write_time(entry.path(),ec);auto bytes=std::filesystem::file_size(entry.path(),ec);if(ec)continue;auto name=entry.path().filename().u8string(),key=std::to_string(time.time_since_epoch().count())+":"+std::to_string(bytes);if(keys[name]==key)continue;unsigned w=0,h=0;auto pixels=ext==".dds"?decoder.dds(entry.path(),&w,&h,4096):frame_wic(entry.path(),w,h);if(pixels.empty())continue;std::vector<uint8_t> fitted(800*500*4,0);const double scale=std::min(800.0/w,500.0/h);const int fw=std::max(1,int(std::lround(w*scale))),fh=std::max(1,int(std::lround(h*scale)));overlay_blit(fitted,pixels,int(w),int(h),{(800-fw)/2,(500-fh)/2,fw,fh},800,500);managedFramePixels[name]=std::move(fitted);keys[name]=key;++managedFrameRevision;}}
static const std::vector<uint8_t>* managed_frame_pixels(const std::string& name){auto it=managedFramePixels.find(name);return it==managedFramePixels.end()?nullptr:&it->second;}
static void horizontal_carplay_prepare(const std::filesystem::path& root,uint64_t now){
 static std::array<std::string,2> keys;static std::filesystem::path previousRoot;static uint64_t next=0;
 if(root==previousRoot&&now<next)return;previousRoot=root;next=now+1000;
 // Release the software graphics device here, on the worker, never from
 // a static destructor under FreeLibrary/Windows loader lock.
 HomeDecoder decoder;
 for(size_t i=0;i<2;++i){auto path=root/"material/ford/f150/carplay"/(i?"F150_Carplay_Waze.dds":"F150_Carplay_Google.dds");
  std::error_code ec;auto time=std::filesystem::last_write_time(path,ec);if(ec)continue;auto bytes=std::filesystem::file_size(path,ec);if(ec)continue;
  auto key=path.u8string()+":"+std::to_string(time.time_since_epoch().count())+":"+std::to_string(bytes);if(keys[i]==key)continue;keys[i]=key;
  unsigned w=0,h=0;auto pixels=decoder.dds(path,&w,&h,800);
  if(w==800&&h==500&&pixels.size()==800*500*4){horizontalCarplayPixels[i]=std::move(pixels);++horizontalCarplayRevision;}
 }
}
}
