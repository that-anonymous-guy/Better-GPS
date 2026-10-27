#pragma once
#include <d3d11.h>
#include "DesignStyle.h"
#include <array>
#include <vector>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <algorithm>
namespace bettergps_overlay {
inline const GUID nativeIconTag={0xa751c180,0x5438,0x43e0,{0xb0,0x28,0x18,0x64,0x2c,0x31,0x19,0x50}};
inline const GUID nativeIconLight={0xa751c181,0x5438,0x43e0,{0xb0,0x28,0x18,0x64,0x2c,0x31,0x19,0x50}};
inline const GUID nativeIconDark={0xa751c182,0x5438,0x43e0,{0xb0,0x28,0x18,0x64,0x2c,0x31,0x19,0x50}};
struct NativeIconImage {unsigned width=0,height=0;DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;std::vector<uint8_t> pixels;};
struct NativeIconPair {std::array<NativeIconImage,11> images;bool player=false;};
inline NativeIconImage native_copilot_red(NativeIconImage image){const bool bgra=image.format==DXGI_FORMAT_B8G8R8A8_UNORM;for(size_t i=0;i<image.pixels.size();i+=4){auto* p=image.pixels.data()+i;auto& r=p[bgra?2:0];auto& b=p[bgra?0:2];if(int(b)>int(r)+8&&int(b)>int(p[1])+8){const auto low=std::min(r,p[1]),high=b;r=high;p[1]=b=low;}}return image;}
// Keep the native arrow's shading/antialiasing, but use the lighter cyan seen
// in the supplied TomTom reference. Gray/white outlines and alpha stay intact.
inline NativeIconImage native_tomtom_cyan(NativeIconImage image){const bool bgra=image.format==DXGI_FORMAT_B8G8R8A8_UNORM;for(size_t i=0;i<image.pixels.size();i+=4){auto* p=image.pixels.data()+i;auto& r=p[bgra?2:0];auto& b=p[bgra?0:2];if(int(b)>int(r)+8&&int(b)>int(p[1])+8){const auto shade=b;r=uint8_t((96u*shade+127)/255);p[1]=uint8_t((244u*shade+127)/255);b=shade;}}return image;}
inline NativeIconImage native_sygic_blue(NativeIconImage image){const bool bgra=image.format==DXGI_FORMAT_B8G8R8A8_UNORM;for(size_t i=0;i<image.pixels.size();i+=4){auto* p=image.pixels.data()+i;auto& r=p[bgra?2:0];auto& b=p[bgra?0:2];if(int(b)>int(r)+8&&int(b)>int(p[1])+8){const auto shade=b;r=uint8_t(17u*shade/255);p[1]=uint8_t(127u*shade/255);b=uint8_t(202u*shade/255);}}return image;}
inline std::vector<NativeIconPair> nativeIcons;
inline NativeIconImage native_icon_read(const std::filesystem::path& path){
 NativeIconImage result;std::ifstream file(path,std::ios::binary|std::ios::ate);const auto size=file.tellg();if(size<128||size>148+128*128*4*2)return result;std::vector<uint8_t> data(size_t(size),0);file.seekg(0);if(!file.read((char*)data.data(),data.size()))return result;
 auto get=[&](size_t at){uint32_t value;memcpy(&value,data.data()+at,4);return value;};
 if(get(0)!=0x20534444||get(4)!=124)return result;
 result.width=get(16);result.height=get(12);size_t offset=128;
 if(!result.width||result.width>128||!result.height||result.height>128)return {};
 if(get(84)==0x30315844&&data.size()>=148){offset=148;auto f=DXGI_FORMAT(get(128));if(f==DXGI_FORMAT_B8G8R8A8_UNORM||f==DXGI_FORMAT_B8G8R8X8_UNORM||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||f==DXGI_FORMAT_B8G8R8X8_UNORM_SRGB)result.format=DXGI_FORMAT_B8G8R8A8_UNORM;else if(f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)result.format=DXGI_FORMAT_R8G8B8A8_UNORM;}
 else if(get(88)==32&&get(92)==0xff0000&&get(96)==0xff00&&get(100)==0xff)result.format=DXGI_FORMAT_B8G8R8A8_UNORM;
 else if(get(88)==32&&get(92)==0xff&&get(96)==0xff00&&get(100)==0xff0000)result.format=DXGI_FORMAT_R8G8B8A8_UNORM;
 if(result.format==DXGI_FORMAT_UNKNOWN||data.size()<offset+result.width*result.height*4)return {};
 result.pixels.assign(data.begin()+offset,data.begin()+offset+result.width*result.height*4);return result;
}
inline bool native_icons_load(const std::filesystem::path& assets){
 nativeIcons.clear();const auto light=assets/"native_themes/light";const auto dark=assets/"native_themes/dark";
 for(const auto& item:std::filesystem::recursive_directory_iterator(light/"material")){if(!item.is_regular_file()||item.path().extension()!=L".dds")continue;
  NativeIconPair pair{{native_icon_read(item.path()),native_icon_read(dark/item.path().lexically_relative(light)),native_icon_read(assets/"native_themes/original"/item.path().lexically_relative(light))}};

  if(pair.images[0].pixels.empty()||pair.images[1].pixels.empty()||pair.images[0].width!=pair.images[1].width||pair.images[0].height!=pair.images[1].height||pair.images[0].format!=pair.images[1].format)return false;
  for(int mode=0;mode<2;++mode){auto image=native_icon_read(assets/"native_themes"/(mode?"waze_dark":"waze_light")/item.path().lexically_relative(light));pair.images[3+mode]=image.pixels.empty()?pair.images[mode]:std::move(image);}
  pair.player=item.path().filename()=="player.dds";
  auto truck=pair.player?native_icon_read(assets/"material/garmin/player_truck.dds"):NativeIconImage{};if(pair.player&&truck.pixels.empty())return false;
  pair.images[5]=pair.images[6]=pair.player?truck:pair.images[2];pair.images[7]=pair.player?std::move(truck):pair.images[2];
  pair.images[8]=pair.player?native_copilot_red(native_icon_read(assets/"material/ui/map/player.dds")):pair.images[2];if(pair.player&&pair.images[8].pixels.empty())return false;
  pair.images[9]=pair.player?native_tomtom_cyan(native_icon_read(assets/"material/ui/map/player.dds")):pair.images[2];if(pair.player&&pair.images[9].pixels.empty())return false;
  pair.images[10]=pair.player?native_sygic_blue(native_icon_read(assets/"material/ui/map/player.dds")):pair.images[2];if(pair.player&&pair.images[10].pixels.empty())return false;
  nativeIcons.push_back(std::move(pair));
 }return !nativeIcons.empty();
}
inline DXGI_FORMAT native_icon_format(DXGI_FORMAT format){if(format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||format==DXGI_FORMAT_B8G8R8A8_TYPELESS||format==DXGI_FORMAT_B8G8R8X8_UNORM||format==DXGI_FORMAT_B8G8R8X8_UNORM_SRGB)return DXGI_FORMAT_B8G8R8A8_UNORM;if(format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||format==DXGI_FORMAT_R8G8B8A8_TYPELESS)return DXGI_FORMAT_R8G8B8A8_UNORM;return format;}
inline void native_icon_identify(ID3D11Texture2D* texture,const D3D11_TEXTURE2D_DESC& desc,const void* data,UINT pitch){
 if(!data||!pitch||desc.ArraySize!=1||desc.SampleDesc.Count!=1||desc.Width>128||desc.Height>128||desc.Usage==D3D11_USAGE_DYNAMIC||desc.CPUAccessFlags||(desc.BindFlags&D3D11_BIND_RENDER_TARGET))return;
 for(size_t index=0;index<nativeIcons.size();++index)for(int mode=0;mode<5;++mode){const auto& image=nativeIcons[index].images[mode];if(desc.Width!=image.width||desc.Height!=image.height||native_icon_format(desc.Format)!=image.format||pitch<image.width*4)continue;
  bool same=true;for(unsigned y=0;y<image.height&&same;++y)same=memcmp((const uint8_t*)data+y*pitch,image.pixels.data()+y*image.width*4,image.width*4)==0;
  if(same){const unsigned tag=unsigned(index)+1;texture->SetPrivateData(nativeIconTag,sizeof(tag),&tag);return;}
 }
}
// Streaming may upload through Map/CopySubresourceRegion instead of supplying
// initial pixels. Inspect one bounded staging copy, asynchronously, and still
// require byte-exact identity. Never identify an icon by size alone.
inline const GUID nativeIconProbe={0xa751c183,0x5438,0x43e0,{0xb0,0x28,0x18,0x64,0x2c,0x31,0x19,0x50}};
struct NativeIconProbe {ID3D11Texture2D *source=nullptr,*stage=nullptr;D3D11_TEXTURE2D_DESC desc{};};
inline std::mutex nativeIconProbeMutex;
inline std::vector<NativeIconProbe> nativeIconProbes;
inline void native_icon_stop(){std::lock_guard<std::mutex> lock(nativeIconProbeMutex);for(auto& p:nativeIconProbes){p.source->Release();p.stage->Release();}nativeIconProbes.clear();}
inline void native_icon_probe(ID3D11DeviceContext* context,ID3D11ShaderResourceView* view,ID3D11Texture2D* source){
 bool attempted=false;UINT bytes=sizeof(attempted);if(SUCCEEDED(view->GetPrivateData(nativeIconProbe,&bytes,&attempted)))return;
 D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);if(desc.ArraySize!=1||desc.SampleDesc.Count!=1||desc.Width>128||desc.Height>128||desc.CPUAccessFlags||(desc.BindFlags&D3D11_BIND_RENDER_TARGET))return;
 if(std::none_of(nativeIcons.begin(),nativeIcons.end(),[&](const auto& p){return std::any_of(p.images.begin(),p.images.begin()+5,[&](const auto& i){return i.width==desc.Width&&i.height==desc.Height&&i.format==native_icon_format(desc.Format);});}))return;
 std::unique_lock<std::mutex> lock(nativeIconProbeMutex,std::try_to_lock);if(!lock||nativeIconProbes.size()>=64)return;
 auto stageDesc=desc;stageDesc.MipLevels=1;stageDesc.BindFlags=stageDesc.MiscFlags=0;stageDesc.Usage=D3D11_USAGE_STAGING;stageDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 ID3D11Device* device=nullptr;source->GetDevice(&device);ID3D11Texture2D* stage=nullptr;const auto hr=device->CreateTexture2D(&stageDesc,nullptr,&stage);device->Release();if(FAILED(hr))return;
 attempted=true;view->SetPrivateData(nativeIconProbe,sizeof(attempted),&attempted);source->AddRef();context->CopySubresourceRegion(stage,0,0,0,0,source,0,nullptr);nativeIconProbes.push_back({source,stage,desc});
}
inline void native_icon_poll(ID3D11DeviceContext* context,bool stop=false){
 std::unique_lock<std::mutex> lock(nativeIconProbeMutex,std::try_to_lock);if(!lock)return;
 for(auto it=nativeIconProbes.begin();it!=nativeIconProbes.end();){ID3D11Device *owner=nullptr,*device=nullptr;it->stage->GetDevice(&owner);context->GetDevice(&device);const bool same=owner==device;owner->Release();device->Release();D3D11_MAPPED_SUBRESOURCE mapped{};
  if(!stop&&same){const auto hr=context->Map(it->stage,0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);if(hr==DXGI_ERROR_WAS_STILL_DRAWING){++it;continue;}if(SUCCEEDED(hr)){native_icon_identify(it->source,it->desc,mapped.pData,mapped.RowPitch);context->Unmap(it->stage,0);}}
  it->source->Release();it->stage->Release();it=nativeIconProbes.erase(it);
 }
}
// Select the actual supplied texture, preserving its colors and alpha. The
// original SRV owns the two bounded cached variants, including device lifetime.
inline ID3D11ShaderResourceView* native_icon_select(ID3D11ShaderResourceView* original,unsigned tag,bool dark,size_t style=displayStyleIndex.load()){
 if(!tag||tag>nativeIcons.size())return nullptr;auto key=dark?nativeIconDark:nativeIconLight;key.Data1+=UINT(style)*10;ID3D11ShaderResourceView* view=nullptr;UINT size=sizeof(view);
 if(SUCCEEDED(original->GetPrivateData(key,&size,&view))&&view)return view;
 ID3D11Device* device=nullptr;original->GetDevice(&device);if(!device)return nullptr;const auto& pair=nativeIcons[tag-1];const size_t palette=native_style_palette(style),choice=native_style_icon(style);const size_t mode=pair.player&&choice==2?7:pair.player&&choice==1?2:palette==5?10:palette==4?9:palette==3?8:palette==2?5+unsigned(dark):palette==1?3+unsigned(dark):unsigned(dark);const auto& image=pair.images[mode];
 D3D11_SHADER_RESOURCE_VIEW_DESC source{};original->GetDesc(&source);D3D11_TEXTURE2D_DESC desc{};desc.Width=image.width;desc.Height=image.height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=(source.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||source.Format==DXGI_FORMAT_B8G8R8X8_UNORM_SRGB||source.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)?(image.format==DXGI_FORMAT_B8G8R8A8_UNORM?DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM_SRGB):image.format;source.Format=desc.Format;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA data{image.pixels.data(),image.width*4,0};ID3D11Texture2D* texture=nullptr;
 if(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture))){source.Texture2D.MostDetailedMip=0;source.Texture2D.MipLevels=1;device->CreateShaderResourceView(texture,&source,&view);texture->Release();}
 device->Release();if(view){original->SetPrivateDataInterface(key,view);view->Release();}return view;
}
}
