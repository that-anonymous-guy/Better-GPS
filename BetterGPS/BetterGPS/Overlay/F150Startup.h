#pragma once
#include "DisplayAssignments.h"
#include <atomic>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <mutex>
#include <string>
#include <algorithm>
#include <cstring>
#include <cctype>
#include "VideoPlayback.h"

namespace bettergps_overlay {
// Only the dedicated F-150 central UI marker can request this asset.
struct F150Animation { std::vector<uint8_t> blocks; std::vector<uint16_t> timeline; std::shared_ptr<VideoPlayback> video; unsigned count=0,width=1284,height=800; bool still=false; uint64_t revision=0; };
static constexpr uint32_t bgaLegacyMagic=0x31534742,bgaTimelineMagic=0x32534742;
static size_t animation_frame_bytes(const F150Animation& image){return size_t(image.width)*image.height/2;}
static unsigned animation_storage_frame(const F150Animation& image,unsigned frame){return image.timeline.empty()?frame:image.timeline[frame];}
static const uint8_t* animation_frame_data(const F150Animation& image,unsigned frame){const auto stored=animation_storage_frame(image,frame);const auto bytes=animation_frame_bytes(image);return frame<image.count&&size_t(stored+1)*bytes<=image.blocks.size()?image.blocks.data()+size_t(stored)*bytes:nullptr;}
static std::shared_ptr<F150Animation> animation_read(const std::filesystem::path& path,unsigned expectedWidth=0,unsigned expectedHeight=0,unsigned maxFrames=4096,uint64_t revision=0){
 std::ifstream in(path,std::ios::binary);uint32_t h[4]{};in.read((char*)h,sizeof(h));if(!in||(h[0]!=bgaLegacyMagic&&h[0]!=bgaTimelineMagic)||h[1]<4||h[1]>4096||h[1]%4||!h[2]||h[2]>4096||h[2]%4||!h[3]||h[3]>maxFrames||(expectedWidth&&h[1]!=expectedWidth)||(expectedHeight&&h[2]!=expectedHeight))return {};
 auto image=std::make_shared<F150Animation>();image->width=h[1];image->height=h[2];image->count=h[3];image->still=h[3]==1;image->revision=revision;unsigned stored=h[3];
 if(h[0]==bgaTimelineMagic){uint32_t unique=0;in.read((char*)&unique,sizeof(unique));if(!in||!unique||unique>h[3]||unique>maxFrames)return {};stored=unique;image->timeline.resize(h[3]);in.read((char*)image->timeline.data(),image->timeline.size()*sizeof(uint16_t));if(!in||std::any_of(image->timeline.begin(),image->timeline.end(),[&](uint16_t frame){return frame>=unique;}))return {};}
 const auto bytes=animation_frame_bytes(*image);if(uint64_t(stored)*bytes>512ull*1024*1024)return {};image->blocks.resize(size_t(stored)*bytes);in.read((char*)image->blocks.data(),image->blocks.size());if(!in||in.peek()!=std::char_traits<char>::eof())return {};return image;
}
static std::shared_ptr<F150Animation> animation_video(const std::filesystem::path& path,unsigned width,unsigned height,unsigned loops,uint64_t revision){auto image=std::make_shared<F150Animation>();image->width=width;image->height=height;image->count=1;image->revision=revision;image->video=std::make_shared<VideoPlayback>();image->video->configure(path,width,height,loops);return image;}
static bool animation_read_first(const std::filesystem::path& path,unsigned width,unsigned height,std::vector<uint8_t>& out){
 std::ifstream in(path,std::ios::binary);uint32_t h[4]{};in.read((char*)h,sizeof(h));if(!in||(h[0]!=bgaLegacyMagic&&h[0]!=bgaTimelineMagic)||h[1]!=width||h[2]!=height||!h[3])return false;uint32_t stored=0;uint64_t offset=sizeof(h);
 if(h[0]==bgaTimelineMagic){uint32_t unique=0;uint16_t first=0;in.read((char*)&unique,sizeof(unique));in.read((char*)&first,sizeof(first));if(!in||!unique||first>=unique)return false;stored=first;offset=sizeof(h)+sizeof(unique)+uint64_t(h[3])*sizeof(uint16_t);}const auto bytes=size_t(width)*height/2;in.seekg(std::streamoff(offset+uint64_t(stored)*bytes));out.resize(bytes);in.read((char*)out.data(),out.size());if(in)return true;out.clear();return false;
}
static constexpr unsigned f150Width=1284,f150Height=800;
static constexpr size_t f150FrameBytes=size_t(f150Width/4)*(f150Height/4)*8;
static std::shared_ptr<const F150Animation> f150Animation;
static std::vector<uint8_t> f150First;
static std::atomic<bool> f150Requested{false},f150Ignition{false};
static std::atomic<bool> f150LoadFinished{false};
static std::atomic<uint64_t> f150Serial{0};
static uint64_t f150SeenSerial=~uint64_t(0);
static std::atomic<uint64_t> f150Origin{0};
static int f150Uploaded=-99;
static bool f150Waiting=false;

struct CarSplashChoice {std::string name;std::filesystem::path path;};
inline std::mutex carMutex;
inline std::vector<CarSplashChoice> carSplashes,carInfoSplashes;
inline std::string carInfoSelected="animation.bga";
inline std::atomic<bool> carInfoEnabled{true},carSplashEnabled{true};
inline std::filesystem::path carInfoRoot;
inline std::string carSplashSelected="animation.bga",carVehicleId,carVehicleName="Waiting for vehicle...",carError;
inline std::filesystem::path carRoot,carConfig;
inline std::atomic<uint64_t> carRevision{0},carLoadRevision{0};
inline std::atomic<bool> carDirty{false},carIsF150{false},carIsBronco{false};
struct CarSplashProfile {std::string splash="animation.bga",info="animation.bga";};
inline std::array<CarSplashProfile,2> carProfiles;
inline std::filesystem::path carPlugin;
inline void car_apply_profile(bool bronco);
inline std::string car_preferred_splash(bool bronco,bool info){return bronco?(info?"welcome_cluster.mp4":"welcome_centerstack.mp4"):(info?"f150splash_cluster.mp4":"f150splash_75thanniversary.mp4");}
inline void car_capture_profile(){auto& p=carProfiles[carIsBronco?1:0];p.splash=carSplashEnabled?carSplashSelected:"__off__";p.info=carInfoEnabled?carInfoSelected:"__off__";}

inline std::string car_lower(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});return s;}
inline void car_vehicle(const std::string& id,const std::string& name){display_vehicle_changed(id);bool changed=false;{std::lock_guard<std::mutex> lock(carMutex);auto key=car_lower(id+" "+name);const bool bronco=key.find("bronco")!=std::string::npos;changed=bronco!=carIsBronco.load();if(changed)car_capture_profile();carIsBronco=bronco;carVehicleId=id;carVehicleName=name.empty()?(id.empty()?"No vehicle":id):name;carIsF150=key.find("f150")!=std::string::npos||key.find("f-150")!=std::string::npos||key.find("f_150")!=std::string::npos;++carRevision;}if(changed)car_apply_profile(carIsBronco);}


inline void car_scan(){std::lock_guard<std::mutex> lock(carMutex);auto scan=[](const std::filesystem::path& root,std::vector<CarSplashChoice>& choices,std::string& selected,bool info){std::vector<CarSplashChoice> next;std::error_code ec;for(const auto& e:std::filesystem::directory_iterator(root,std::filesystem::directory_options::skip_permission_denied,ec))if(e.is_regular_file(ec)&&(car_lower(e.path().extension().u8string())==".bga"||car_lower(e.path().extension().u8string())==".mp4"))next.push_back({e.path().filename()=="animation.bga"?(info?"splash_info":"75th anniversary"):e.path().stem().u8string(),e.path()});std::sort(next.begin(),next.end(),[](auto& a,auto& b){if(a.path.extension()==".mp4")return b.path.extension()!=".mp4";if(b.path.extension()==".mp4")return false;return car_lower(a.name)<car_lower(b.name);});next.push_back({"OFF",root/"__off__"});choices=std::move(next);if(std::none_of(choices.begin(),choices.end(),[&](auto& c){return c.path.filename().u8string()==selected;}))selected="animation.bga";};scan(carRoot,carSplashes,carSplashSelected,false);scan(carInfoRoot,carInfoSplashes,carInfoSelected,true);++carRevision;}
static constexpr unsigned f150InfoWidth=1444,f150InfoHeight=540;
static constexpr size_t f150InfoFrameBytes=size_t(f150InfoWidth/4)*(f150InfoHeight/4)*8;
static std::shared_ptr<const F150Animation> f150InfoAnimation;
static std::vector<uint8_t> f150InfoFirst;
static std::atomic<bool> f150InfoRequested{false},f150InfoLoadFinished{false};
static std::atomic<uint64_t> f150InfoOrigin{0};
static uint64_t f150InfoSeenSerial=~uint64_t(0);
static int f150InfoUploaded=-99;
static bool f150InfoWaiting=false;
inline void car_apply_profile(bool bronco){
 {std::lock_guard<std::mutex> lock(carMutex);const auto& p=carProfiles[bronco?1:0];carRoot=carPlugin/(bronco?"BetterGPS_assets/material/ui/bronco_startup":"BetterGPS_assets/material/ui/f150_startup");carInfoRoot=carPlugin/(bronco?"BetterGPS_assets/material/ui/bronco_info_startup":"BetterGPS_assets/material/ui/f150_info_startup");carSplashSelected=p.splash;carInfoSelected=p.info;if(carSplashSelected=="animation.bga"&&std::filesystem::is_regular_file(carRoot/car_preferred_splash(bronco,false)))carSplashSelected=car_preferred_splash(bronco,false);if(carInfoSelected=="animation.bga"&&std::filesystem::is_regular_file(carInfoRoot/car_preferred_splash(bronco,true)))carInfoSelected=car_preferred_splash(bronco,true);carSplashEnabled=p.splash!="__off__";carInfoEnabled=p.info!="__off__";}
 car_scan();

 std::vector<uint8_t> firstFrame,firstInfo;animation_read_first(carRoot/std::filesystem::u8path(carSplashSelected=="__off__"?"animation.bga":carSplashSelected),f150Width,f150Height,firstFrame);animation_read_first(carInfoRoot/std::filesystem::u8path(carInfoSelected=="__off__"?"animation.bga":carInfoSelected),f150InfoWidth,f150InfoHeight,firstInfo);{std::lock_guard<std::mutex> lock(carMutex);f150First=std::move(firstFrame);f150InfoFirst=std::move(firstInfo);}
 std::atomic_store(&f150Animation,std::shared_ptr<const F150Animation>());std::atomic_store(&f150InfoAnimation,std::shared_ptr<const F150Animation>());f150LoadFinished=false;f150InfoLoadFinished=false;++f150Serial;
}
inline void car_init(const std::filesystem::path& plugin){
 carPlugin=plugin;carConfig=plugin/"BetterGPSCarSpecific.ini";carProfiles={};std::string line;unsigned section=0;std::ifstream input(carConfig,std::ios::binary);
 while(std::getline(input,line)){line=display_trim(line);if(line=="[Bronco]"){section=1;continue;}if(line=="[F150]"){section=0;continue;}auto& p=carProfiles[section];if(line.rfind("splash=",0)==0)p.splash=std::filesystem::u8path(line.substr(7)).filename().u8string();if(line.rfind("info_splash=",0)==0)p.info=std::filesystem::u8path(line.substr(12)).filename().u8string();if(line=="splash_enabled=0")p.splash="__off__";if(line=="info_enabled=0")p.info="__off__";}
 car_apply_profile(carIsBronco);carDirty=false;
}
inline bool car_save(){if(!carDirty.exchange(false))return true;std::filesystem::path config;std::array<CarSplashProfile,2> values;{std::lock_guard<std::mutex> lock(carMutex);car_capture_profile();config=carConfig;values=carProfiles;}if(config.empty())return true;auto tmp=config;tmp+=L".tmp";std::ofstream file(tmp,std::ios::binary);for(unsigned i=0;i<2;++i)file<<(i?"[Bronco]":"[F150]")<<"\nsplash="<<values[i].splash<<"\ninfo_splash="<<values[i].info<<"\n";file.close();if(file&&MoveFileExW(tmp.c_str(),config.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;carDirty=true;return false;}
inline void car_cycle(int step,bool info=false){std::lock_guard<std::mutex> lock(carMutex);auto& choices=info?carInfoSplashes:carSplashes;auto& selected=info?carInfoSelected:carSplashSelected;if(choices.empty())return;size_t i=0;for(size_t n=0;n<choices.size();++n)if(choices[n].path.filename().u8string()==selected)i=n;i=(i+choices.size()+step)%choices.size();selected=choices[i].path.filename().u8string();(info?carInfoEnabled:carSplashEnabled)=selected!="__off__";carDirty=true;++carRevision;}
inline void car_splash_enable(bool enabled){std::lock_guard<std::mutex> lock(carMutex);carSplashSelected=enabled?(carSplashSelected=="__off__"?"animation.bga":carSplashSelected):"__off__";carSplashEnabled=enabled;carDirty=true;++carRevision;}
inline void car_info_enable(bool enabled){std::lock_guard<std::mutex> lock(carMutex);carInfoSelected=enabled?(carInfoSelected=="__off__"?"animation.bga":carInfoSelected):"__off__";carInfoEnabled=enabled;carDirty=true;++carRevision;}
inline bool car_video_selected(bool info){std::lock_guard<std::mutex> lock(carMutex);const auto name=car_lower(info?carInfoSelected:carSplashSelected);return name.size()>=4&&name.rfind(".mp4")==name.size()-4;}
// Imported stills use the same bounded BC1 texture as the animation. Compression
// is performed once by the CPU worker, never by the rendering hook.
inline bool car_write_still(const std::filesystem::path& path,const std::vector<uint8_t>& pixels,unsigned width=1280,unsigned height=800){
 if(!width||!height||width%4||height%4||pixels.size()!=size_t(width)*height*4)return false;const unsigned paddedWidth=width+4;std::vector<uint8_t> blocks(size_t(paddedWidth/4)*(height/4)*8);
 for(unsigned by=0;by<height;by+=4)for(unsigned bx=0;bx<paddedWidth;bx+=4){unsigned lo[3]={255,255,255},hi[3]={0,0,0};unsigned rgb[16][3];for(unsigned n=0;n<16;++n){unsigned x=bx+n%4;const auto* p=pixels.data()+(size_t(by+n/4)*width+(x<4?0:x-4))*4;for(unsigned k=0;k<3;++k){rgb[n][k]=(unsigned(p[2-k])*p[3]+5*(255-p[3])+127)/255;lo[k]=std::min(lo[k],rgb[n][k]);hi[k]=std::max(hi[k],rgb[n][k]);}}
 auto pack=[](unsigned* c){return uint16_t(((c[0]>>3)<<11)|((c[1]>>2)<<5)|(c[2]>>3));};uint16_t a=pack(hi),b=pack(lo);if(lo[0]==5&&lo[1]==5&&lo[2]==5&&hi[0]==5&&hi[1]==5&&hi[2]==5){a=uint16_t((1<<11)|(2<<5)|1);b=0;}if(a<b)std::swap(a,b);if(a==b){if(a<65535)++a;else --b;}unsigned palette[4][3];for(unsigned i=0;i<2;++i){auto v=i?b:a;palette[i][0]=((v>>11)&31)*255/31;palette[i][1]=((v>>5)&63)*255/63;palette[i][2]=(v&31)*255/31;}for(unsigned k=0;k<3;++k){palette[2][k]=(2*palette[0][k]+palette[1][k])/3;palette[3][k]=(palette[0][k]+2*palette[1][k])/3;}uint32_t indices=0;for(unsigned n=0;n<16;++n){unsigned best=0,dist=~0u;for(unsigned i=0;i<4;++i){unsigned d=0;for(unsigned k=0;k<3;++k){int v=int(rgb[n][k])-int(palette[i][k]);d+=v*v;}if(d<dist){dist=d;best=i;}}indices|=best<<(2*n);}auto* out=blocks.data()+(size_t(by/4)*(paddedWidth/4)+bx/4)*8;memcpy(out,&a,2);memcpy(out+2,&b,2);memcpy(out+4,&indices,4);}
 std::ofstream file(path,std::ios::binary);uint32_t header[]={0x31534742,paddedWidth,height,1};file.write((const char*)header,sizeof(header));file.write((const char*)blocks.data(),blocks.size());return file.good();
}
static void f150_ignition(bool on){const bool changed=f150Ignition.exchange(on)!=on;if(changed&&on){if(auto a=std::atomic_load(&f150Animation);a&&a->video)a->video->restart();if(auto a=std::atomic_load(&f150InfoAnimation);a&&a->video)a->video->restart();}if(changed)++f150Serial;}
// -2: native GPS; -1: opaque black; otherwise the animation frame.
static int f150_frame(uint64_t elapsed,unsigned count,bool bronco=false){
 if(!count)return -2;
 const uint64_t delay=bronco?0:1000;if(elapsed<delay)return -1;
 if(count==1)return elapsed<(bronco?2000:4500)?0:-2;
 const uint64_t frame=(elapsed-delay)*30/1000,loops=bronco?1:2;
 return frame>=uint64_t(count)*loops?-2:int(frame%count);
}
static void f150_prepare(const std::filesystem::path& root){
 static uint64_t attempted=~uint64_t(0);const auto revision=carRevision.load();
 if(!f150Requested.load()||!carSplashEnabled||attempted==revision)return;
 attempted=revision;std::string selected;std::filesystem::path folder;{std::lock_guard<std::mutex> lock(carMutex);selected=carSplashSelected;folder=carRoot.empty()?root/"material/ui/f150_startup":carRoot;}static std::string preparedFile;const auto key=(folder/std::filesystem::u8path(selected)).u8string();if(key==preparedFile&&std::atomic_load(&f150Animation))return;preparedFile=key;f150LoadFinished=false;
 struct Finish {uint64_t revision; ~Finish(){if(revision==carRevision){f150LoadFinished=true;++carLoadRevision;}} } finish{revision};
 {std::lock_guard<std::mutex> lock(carMutex);carError="Cannot load selected splash; previous splash retained.";}
 const auto path=folder/std::filesystem::u8path(selected);auto image=car_lower(path.extension().u8string())==".mp4"?animation_video(path,f150Width,f150Height,carIsBronco?1:2,revision):animation_read(path,f150Width,f150Height,300,revision);if(!image)return;
 if(revision!=carRevision)return;
 {std::atomic_store(&f150Animation,std::shared_ptr<const F150Animation>(image));std::lock_guard<std::mutex> lock(carMutex);carError.clear();}
}
static uint64_t f150_info_end(unsigned count){return count==1?4000:(uint64_t(count)*1000+29)/30;}
// Fade overlaps the moving final frames; it never holds a black tail afterward.
static float f150_info_opacity(uint64_t elapsed,unsigned count){const auto end=f150_info_end(count),span=std::min(uint64_t(500),end),start=end-span;if(elapsed<=start)return 1;if(elapsed>=end)return 0;const float t=float(elapsed-start)/float(span);return 1-t*t*(3-2*t);}
static int f150_info_frame(uint64_t elapsed,unsigned count){if(!count||elapsed>=f150_info_end(count))return -2;if(count==1)return 0;return int(std::min(uint64_t(count)-1,elapsed*30/1000));}
static void f150_info_prepare(const std::filesystem::path& root){
 static uint64_t attempted=~uint64_t(0);const auto revision=carRevision.load();if(!f150InfoRequested||!carInfoEnabled||attempted==revision)return;attempted=revision;f150InfoLoadFinished=false;
 struct Finish {uint64_t revision;~Finish(){if(revision==carRevision){f150InfoLoadFinished=true;++carLoadRevision;}}} finish{revision};std::string selected;std::filesystem::path folder;{std::lock_guard<std::mutex> lock(carMutex);selected=carInfoSelected;folder=carInfoRoot.empty()?root/"material/ui/f150_info_startup":carInfoRoot;}
 static std::string preparedFile;const auto key=(folder/std::filesystem::u8path(selected)).u8string();if(key==preparedFile&&std::atomic_load(&f150InfoAnimation))return;preparedFile=key;
 const auto path=folder/std::filesystem::u8path(selected);auto image=car_lower(path.extension().u8string())==".mp4"?animation_video(path,f150InfoWidth,f150InfoHeight,1,revision):animation_read(path,f150InfoWidth,f150InfoHeight,300,revision);if(revision==carRevision&&image)std::atomic_store(&f150InfoAnimation,std::shared_ptr<const F150Animation>(image));
}

}
