#pragma once
#include "SettingsMenuRenderer.h"
#include <Windows.h>
#include <wincodec.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <atomic>
#include <array>
#include <map>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <random>
namespace bettergps_overlay {
using HomePixels=std::vector<uint8_t>;
struct HomeChoice {std::string name;std::filesystem::path path;};
struct HomeLayers {std::array<std::shared_ptr<const HomePixels>,4> pixels;uint64_t revision=0;};
inline std::mutex homeMutex;
inline std::array<std::vector<HomeChoice>,4> homeChoices;
inline std::array<std::string,4> homeSelected;
inline std::filesystem::path homeRoot,homeConfig;
inline std::string homeError;
inline std::atomic<uint64_t> homeRevision{0},homePublishedRevision{0};
inline std::atomic<int> homeDurationSeconds{3},homeShuffleMode{2},homeClockFont{0};
inline std::atomic<uint64_t> homePowerOnSerial{0};
inline uint64_t homeShuffleSeed=0,homeShuffleCounter=0,homeShuffleLastPower=~uint64_t(0);
inline int64_t homeShuffleLastDay=-1,homeShuffleDayOrigin=-1;
inline bool homeShufflePending=true;
inline uint64_t home_random_mix(uint64_t value){value+=0x9e3779b97f4a7c15ULL;value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;value=(value^(value>>27))*0x94d049bb133111ebULL;return value^(value>>31);}
inline std::atomic<bool> homeRescan{false};
inline std::shared_ptr<const HomeLayers> homeLayers;
inline uint64_t homeSavedRevision=~uint64_t(0);
inline const char* homeFolders[]={"icons","icons","background","toplayer"};
inline const char* homeSuffixes[]={"_normalmix","_overlaymix","_background","_toplayer"};
inline const char* homeKeys[]={"IconsNormal","IconsOverlay","Background","Toplayer"};
inline std::string home_lower(std::string text){for(auto& c:text)c=char(tolower((unsigned char)c));return text;}
// Legacy OS selections migrate only the icon layers, never clock typography,
// wallpaper, foreground, shuffle, power or another device's preferences.
inline std::string home_icon_selection(std::string name){const auto lower=home_lower(name);return lower=="android"||lower=="android_lock"?"iphone_lock":name;}
inline int home_clock_style_value(const std::string& value){const auto lower=home_lower(value);return lower=="style2"||lower=="style 2"||lower=="android"?1:0;}
inline void home_pair_foreground_locked(){if(homeSelected[3]=="__none__")return;homeSelected[3].clear();for(const auto& top:homeChoices[3])if(home_lower(top.name)==home_lower(homeSelected[2])){homeSelected[3]=top.name;return;}for(const auto& top:homeChoices[3])if(home_lower(top.name)=="none"){homeSelected[3]=top.name;return;}}
inline void home_scan(){
 std::lock_guard<std::mutex> lock(homeMutex);std::array<std::vector<HomeChoice>,4> choices;
 for(size_t i=0;i<4;++i){std::error_code error;const auto dir=homeRoot/homeFolders[i];if(!std::filesystem::is_directory(dir,error)||GetFileAttributesW(dir.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)continue;
  for(const auto& entry:std::filesystem::directory_iterator(dir,error)){if(!entry.is_regular_file(error)||GetFileAttributesW(entry.path().c_str())&FILE_ATTRIBUTE_REPARSE_POINT)continue;const auto ext=home_lower(entry.path().extension().u8string());if(ext!=".dds"&&ext!=".png"&&ext!=".bmp"&&ext!=".jpg"&&ext!=".jpeg"&&ext!=".tif"&&ext!=".tiff")continue;auto stem=entry.path().stem().u8string(),lower=home_lower(stem);const std::string suffix=homeSuffixes[i];if(lower.size()<=suffix.size()||lower.substr(lower.size()-suffix.size())!=suffix)continue;if(i<2&&lower.find("_home_")!=std::string::npos)continue;choices[i].push_back({stem.substr(0,stem.size()-suffix.size()),entry.path()});}
  std::sort(choices[i].begin(),choices[i].end(),[](const auto& a,const auto& b){auto x=home_lower(a.name),y=home_lower(b.name);return x==y?a.path<b.path:x<y;});
  choices[i].erase(std::unique(choices[i].begin(),choices[i].end(),[](const auto& a,const auto& b){return home_lower(a.name)==home_lower(b.name);}),choices[i].end());
  if(i<2){choices[i].erase(std::remove_if(choices[i].begin(),choices[i].end(),[](const auto& c){const auto name=home_lower(c.name);return name=="android"||name=="android_lock";}),choices[i].end());if(std::any_of(choices[i].begin(),choices[i].end(),[](const auto& c){return home_lower(c.name)=="iphone_lock";}))choices[i].erase(std::remove_if(choices[i].begin(),choices[i].end(),[](const auto& c){return home_lower(c.name)=="iphone";}),choices[i].end());homeSelected[i]=home_icon_selection(homeSelected[i]);}
 }
 bool changed=false;for(size_t i=2;i<4;++i){if(choices[i].size()!=homeChoices[i].size())changed=true;else for(size_t j=0;j<choices[i].size();++j)if(choices[i][j].path!=homeChoices[i][j].path)changed=true;}
 homeChoices=std::move(choices);for(unsigned i=0;i<2;++i){auto old=home_lower(homeSelected[i]);if(old=="iphone"||old=="android")for(auto& c:homeChoices[i])if(home_lower(c.name)==old+"_lock"){homeSelected[i]=c.name;break;}}for(size_t i=0;i<4;++i){auto& list=homeChoices[i];if(i==3&&homeSelected[i]=="__none__")continue;if(list.empty())continue;auto found=std::find_if(list.begin(),list.end(),[&](const auto& item){return home_lower(item.name)==home_lower(homeSelected[i]);});if(found==list.end()){const std::string preferred=i<2?"iphone_lock":"mustang_2";found=std::find_if(list.begin(),list.end(),[&](const auto& item){return home_lower(item.name)==preferred;});homeSelected[i]=(found==list.end()?list.front():*found).name;}}
 if(homeShuffleMode.load()>0)home_pair_foreground_locked();if(changed)homeShufflePending=true;++homeRevision;
}
inline void home_init(const std::filesystem::path& base){
 {std::lock_guard<std::mutex> lock(homeMutex);homeRoot=base/"BetterGPS_assets/material/ui/phone_lockscreen";homeConfig=base/"BetterGPSLockscreen.ini";homeSelected={};homeDurationSeconds=3;homeShuffleMode=2;homeClockFont=0;homeShufflePending=true;homeShuffleSeed=homeShuffleCounter=0;homeShuffleLastPower=~uint64_t(0);homeShuffleLastDay=homeShuffleDayOrigin=-1;homeError.clear();bool bagVersion=false;auto input=homeConfig;if(!std::filesystem::is_regular_file(input))input=base/"BetterGPSHomescreen.ini";std::ifstream f(input);std::string line;while(std::getline(f,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();const auto pos=line.find('=');if(pos==std::string::npos)continue;const auto key=line.substr(0,pos),value=line.substr(pos+1);if(key=="DurationSeconds"||key=="ShuffleMode"){try{size_t end=0;int n=std::stoi(value,&end);if(end==value.size()){if(key=="DurationSeconds")homeDurationSeconds=std::clamp(n,0,3600);else homeShuffleMode=std::clamp(n,0,2);}}catch(...){}}if(key=="ClockFontStyle")homeClockFont=home_clock_style_value(value);if(key=="ShuffleBagVersion")bagVersion=value=="1";if(key=="ShuffleDayOrigin"){try{size_t end=0;auto n=std::stoll(value,&end);if(end==value.size()&&n>=0)homeShuffleDayOrigin=n;}catch(...){}}if(key=="ShuffleSeed"||key=="ShuffleCounter"){try{size_t end=0;auto n=std::stoull(value,&end);if(end==value.size()&&!value.empty()&&value[0]!='-'){if(key=="ShuffleSeed")homeShuffleSeed=n;else homeShuffleCounter=n;}}catch(...){}}for(size_t i=0;i<4;++i)if(key==homeKeys[i])homeSelected[i]=value;}if(!bagVersion)homeShuffleCounter=0;if(!homeShuffleSeed){try{std::random_device random;homeShuffleSeed=(uint64_t(random())<<32)^random();}catch(...){}if(!homeShuffleSeed)homeShuffleSeed=home_random_mix(GetTickCount64()^GetCurrentProcessId());}homeSavedRevision=~uint64_t(0);}
 home_scan();
}
inline void home_cycle(size_t item,int direction){if(item>=4)return;std::lock_guard<std::mutex> lock(homeMutex);auto& list=homeChoices[item];if(list.empty()&&item!=3)return;auto found=std::find_if(list.begin(),list.end(),[&](const auto& c){return c.name==homeSelected[item];});const size_t count=list.size()+(item==3?1:0),index=found==list.end()?(item==3?list.size():0):size_t(found-list.begin());const auto next=(index+count+direction)%count;homeSelected[item]=next==list.size()?"__none__":list[next].name;if(item>=2)homeShuffleMode=0;++homeRevision;}
inline void home_shuffle_mode(int mode){std::lock_guard<std::mutex> lock(homeMutex);mode=std::clamp(mode,0,2);if(homeShuffleMode.load()==mode)return;homeShuffleMode=mode;homeShufflePending=true;++homeRevision;}
// Deterministic shuffled bags keep daily assignments stable across save rewinds.
// Work is limited to shuffle events; no growing history or per-frame allocations.
inline size_t home_shuffle_index(uint64_t draw,uint64_t seed=0){
 const size_t count=homeChoices[2].size();if(count<2)return 0;
 uint64_t catalog=seed?seed:homeShuffleSeed;for(const auto& item:homeChoices[2]){for(unsigned char c:home_lower(item.name))catalog=home_random_mix(catalog^c);catalog=home_random_mix(catalog^0xff);}
 const uint64_t cycle=draw/count;
 auto permutation=[&](uint64_t round){std::vector<size_t> order(count);for(size_t i=0;i<count;++i)order[i]=i;uint64_t random=home_random_mix(catalog^home_random_mix(round));for(size_t i=count-1;i>0;--i){random=home_random_mix(random);std::swap(order[i],order[random%(i+1)]);}return order;};
 // Two choices must alternate to avoid a repeat between bags.
 auto order=permutation(count==2?0:cycle);
 if(count>2&&cycle){const auto previous=permutation(cycle-1);if(order.front()==previous.back())std::swap(order[0],order[1]);}
 // The last entry remains unchanged, so checking the previous bag is bounded.
 return order[draw%count];
}
inline void home_shuffle_update(uint64_t powerSerial,int64_t day){
 if(!homeShuffleMode.load())return;std::lock_guard<std::mutex> lock(homeMutex);const int mode=homeShuffleMode.load();if(!mode||homeChoices[2].empty())return;
 if(mode==1&&!homeShufflePending&&powerSerial==homeShuffleLastPower)return;if(mode==2&&(day<0||(!homeShufflePending&&day==homeShuffleLastDay)))return;
 if(mode==2&&homeShuffleDayOrigin<0)homeShuffleDayOrigin=day;
 // Anchor the first daily bag at the first observed day, rather than entering
 // an arbitrary partial bag on installation. Persist the anchor across saves.
 const uint64_t count=homeChoices[2].size();
 const size_t index=home_shuffle_index(mode==2?uint64_t(day)+count-uint64_t(homeShuffleDayOrigin)%count:homeShuffleCounter++);
 homeSelected[2]=homeChoices[2][index].name;if(homeSelected[3]=="__none__")homeSelected[3].clear();home_pair_foreground_locked();
 homeShuffleLastPower=powerSerial;homeShuffleLastDay=day;homeShufflePending=false;++homeRevision;
}
inline void home_clock_style(int style){std::lock_guard<std::mutex> lock(homeMutex);style=std::clamp(style,0,1);if(homeClockFont.exchange(style)!=style)++homeRevision;}
inline void home_duration_adjust(int delta){std::lock_guard<std::mutex> lock(homeMutex);homeDurationSeconds=std::clamp(homeDurationSeconds.load()+delta,0,3600);++homeRevision;}
inline bool home_save(){std::lock_guard<std::mutex> lock(homeMutex);const auto rev=homeRevision.load();if(homeConfig.empty()||rev==homeSavedRevision)return true;auto temp=homeConfig;temp+=L".tmp";std::ofstream f(temp);if(!f)return false;f<<"; BetterGPS lockscreen selections; file suffixes are omitted\nDurationSeconds="<<homeDurationSeconds.load()<<"\nClockFontStyle="<<(homeClockFont.load()?"style2":"style1")<<"\n; Shuffle: 0=Off, 1=Power on/off, 2=Day\nShuffleMode="<<homeShuffleMode.load()<<"\nShuffleBagVersion=1\nShuffleSeed="<<homeShuffleSeed<<"\nShuffleDayOrigin="<<homeShuffleDayOrigin<<"\nShuffleCounter="<<homeShuffleCounter<<'\n';for(size_t i=0;i<4;++i)f<<homeKeys[i]<<'='<<homeSelected[i]<<'\n';f.close();if(!f||!MoveFileExW(temp.c_str(),homeConfig.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;homeSavedRevision=rev;return true;}
// Dedicated software device: decode compressed DDS once when a style changes.
// Never read back from, wait on, or alter the game's graphics device.
struct HomeDecoder {
 ID3D11Device* device{};ID3D11DeviceContext* context{};SettingsMenuRenderer renderer;
 ~HomeDecoder(){renderer.stop();SettingsMenuRenderer::drop(context);SettingsMenuRenderer::drop(device);}
 HomePixels dds(const std::filesystem::path& path,unsigned* decodedWidth=nullptr,unsigned* decodedHeight=nullptr,unsigned previewLimit=2048){
  std::ifstream f(path,std::ios::binary|std::ios::ate);const auto length=f.tellg();if(length<128||length>64*1024*1024)return {};std::vector<uint8_t> file(size_t(length),0);f.seekg(0);f.read((char*)file.data(),file.size());if(!f)return {};auto u=[&](size_t offset){uint32_t value=0;if(offset+4<=file.size())memcpy(&value,file.data()+offset,4);return value;};
  const unsigned sourceWidth=u(16),sourceHeight=u(12);if(u(0)!=0x20534444||u(4)!=124||!sourceWidth||!sourceHeight||sourceWidth>16384||sourceHeight>16384||(!decodedWidth&&(sourceWidth!=360||sourceHeight!=740)))return {};const double shrink=decodedWidth?std::min(1.0,double(previewLimit)/std::max(sourceWidth,sourceHeight)):1.0;const unsigned width=std::max(1u,unsigned(sourceWidth*shrink)),height=std::max(1u,unsigned(sourceHeight*shrink));DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;size_t offset=128;const auto fourcc=u(84);unsigned block=0,alphaMode=0;
  if(fourcc==0x30315844){if(file.size()<148||u(132)!=3||u(140)!=1||(u(136)&4))return {};format=DXGI_FORMAT(u(128));offset=148;alphaMode=u(144)&7;}
  else if(fourcc==0x31545844)format=DXGI_FORMAT_BC1_UNORM;else if(fourcc==0x33545844)format=DXGI_FORMAT_BC2_UNORM;else if(fourcc==0x35545844)format=DXGI_FORMAT_BC3_UNORM;
  else if((u(80)&0x40)&&u(88)==32){if(u(92)==0xff&&u(100)==0xff0000)format=DXGI_FORMAT_R8G8B8A8_UNORM;else if(u(92)==0xff0000&&u(100)==0xff)format=DXGI_FORMAT_B8G8R8A8_UNORM;if(!u(104))alphaMode=3;}
  // Read stored color values, including sRGB DDS, without applying a gamma transform.
  if(format==DXGI_FORMAT_BC1_UNORM_SRGB)format=DXGI_FORMAT_BC1_UNORM;if(format==DXGI_FORMAT_BC2_UNORM_SRGB)format=DXGI_FORMAT_BC2_UNORM;if(format==DXGI_FORMAT_BC3_UNORM_SRGB)format=DXGI_FORMAT_BC3_UNORM;if(format==DXGI_FORMAT_BC7_UNORM_SRGB)format=DXGI_FORMAT_BC7_UNORM;if(format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)format=DXGI_FORMAT_R8G8B8A8_UNORM;if(format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)format=DXGI_FORMAT_B8G8R8A8_UNORM;
  if(format==DXGI_FORMAT_BC1_UNORM)block=8;else if(format==DXGI_FORMAT_BC2_UNORM||format==DXGI_FORMAT_BC3_UNORM||format==DXGI_FORMAT_BC7_UNORM)block=16;else if(format!=DXGI_FORMAT_B8G8R8A8_UNORM&&format!=DXGI_FORMAT_R8G8B8A8_UNORM&&format!=DXGI_FORMAT_B8G8R8X8_UNORM)return {};
  const unsigned pitch=block?((sourceWidth+3)/4)*block:sourceWidth*4;const size_t bytes=block?size_t(pitch)*((sourceHeight+3)/4):size_t(pitch)*sourceHeight;if(offset+bytes>file.size())return {};
  PhoneInternalScope scope;if(!device&&FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return {};if(!renderer.start(device))return {};
  ID3D11Texture2D *source=nullptr,*target=nullptr,*stage=nullptr;ID3D11ShaderResourceView* view=nullptr;HomePixels result;
  D3D11_TEXTURE2D_DESC d{};d.Width=sourceWidth;d.Height=sourceHeight;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;d.Format=format;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA initial{file.data()+offset,pitch,0};
  bool ok=SUCCEEDED(device->CreateTexture2D(&d,&initial,&source))&&SUCCEEDED(device->CreateShaderResourceView(source,nullptr,&view));d.Width=width;d.Height=height;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;if(ok)ok=SUCCEEDED(device->CreateTexture2D(&d,nullptr,&target));d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;if(ok)ok=SUCCEEDED(device->CreateTexture2D(&d,nullptr,&stage));
  if(ok){auto blend=renderer.blend;renderer.blend=nullptr;ok=renderer.render(context,view,target,0,0,float(width),float(height),[](ID3D11DeviceContext* c,UINT slot,UINT count,ID3D11ShaderResourceView* const* views){c->PSSetShaderResources(slot,count,views);});renderer.blend=blend;}
  if(ok){context->CopyResource(stage,target);D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(stage,0,D3D11_MAP_READ,0,&mapped))){result.resize(size_t(width)*height*4);for(unsigned y=0;y<height;++y)memcpy(result.data()+size_t(y)*width*4,(uint8_t*)mapped.pData+y*mapped.RowPitch,width*4);context->Unmap(stage,0);}}
  if(alphaMode==3)for(size_t i=3;i<result.size();i+=4)result[i]=255;
  if(alphaMode==2)for(size_t i=0;i<result.size();i+=4)for(int c=0;c<3;++c)result[i+c]=result[i+3]?uint8_t(std::min(255u,(unsigned(result[i+c])*255+result[i+3]/2)/result[i+3])):0;
  SettingsMenuRenderer::drop(view);SettingsMenuRenderer::drop(source);SettingsMenuRenderer::drop(target);SettingsMenuRenderer::drop(stage);if(!result.empty()){if(decodedWidth)*decodedWidth=width;if(decodedHeight)*decodedHeight=height;}return result;
 }
 HomePixels image(const std::filesystem::path& path){if(home_lower(path.extension().u8string())==".dds")return dds(path);
  const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);IWICImagingFactory* factory=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;IWICFormatConverter* converter=nullptr;HomePixels result;
  bool ok=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))&&SUCCEEDED(decoder->GetFrame(0,&frame));UINT w=0,h=0;if(ok)ok=SUCCEEDED(frame->GetSize(&w,&h))&&w==360&&h==740;if(ok)ok=SUCCEEDED(factory->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(frame,GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));if(ok){result.resize(360*740*4);if(FAILED(converter->CopyPixels(nullptr,360*4,UINT(result.size()),result.data())))result.clear();}
  SettingsMenuRenderer::drop(converter);SettingsMenuRenderer::drop(frame);SettingsMenuRenderer::drop(decoder);SettingsMenuRenderer::drop(factory);if(SUCCEEDED(com))CoUninitialize();return result;
 }
};
inline void home_prepare(){
 if(homeRescan.exchange(false))home_scan();const auto requested=homeRevision.load();static uint64_t processed=~uint64_t(0);if(requested==processed)return;
 bool transparentTop=false;std::array<std::filesystem::path,4> paths;{std::lock_guard<std::mutex> lock(homeMutex);if(homeRoot.empty()){processed=requested;return;}for(size_t i=0;i<4;++i)for(const auto& choice:homeChoices[i])if(choice.name==homeSelected[i])paths[i]=choice.path;transparentTop=homeSelected[3]=="__none__"||(homeShuffleMode.load()>0&&homeSelected[3].empty());}
 struct Cached {std::shared_ptr<const HomePixels> pixels;uint64_t used;};static std::map<std::string,Cached> cache;static uint64_t serial=0;HomeDecoder decoder;
 auto layers=std::make_shared<HomeLayers>();std::string error;for(size_t i=0;i<4;++i){if(paths[i].empty())continue;std::error_code ec;auto time=std::filesystem::last_write_time(paths[i],ec);auto bytes=std::filesystem::file_size(paths[i],ec);auto key=paths[i].u8string()+":"+std::to_string(time.time_since_epoch().count())+":"+std::to_string(bytes);auto found=cache.find(key);if(found==cache.end()){auto pixels=decoder.image(paths[i]);auto image=pixels.empty()?std::shared_ptr<const HomePixels>{}:std::make_shared<const HomePixels>(std::move(pixels));found=cache.emplace(key,Cached{image,++serial}).first;}found->second.used=++serial;layers->pixels[i]=found->second.pixels;if(!layers->pixels[i])error="Cannot load "+paths[i].filename().u8string()+" (360 x 740 required).";}
 while(cache.size()>16){auto oldest=std::min_element(cache.begin(),cache.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});cache.erase(oldest);}
 if(transparentTop)layers->pixels[3]=std::make_shared<const HomePixels>(360*740*4,0);
 {std::lock_guard<std::mutex> lock(homeMutex);homeError=error;}if(error.empty()){layers->revision=homePublishedRevision.load()+1;std::atomic_store(&homeLayers,std::shared_ptr<const HomeLayers>(layers));homePublishedRevision=layers->revision;}processed=requested;
}
}
