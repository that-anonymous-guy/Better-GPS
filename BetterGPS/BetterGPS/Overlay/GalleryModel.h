#pragma once
// SPDX-License-Identifier: Apache-2.0
// Gallery2 AOSP 4.2.2 AlbumPage/PhotoPage/PhotoView behaviour adapted to NavPlus.
// Native file catalog and bounded thumbnail cache replace Android MediaStore/GL.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace bettergps_gallery {
namespace fs=std::filesystem;
struct Item {fs::path path;int64_t stamp=0;uintmax_t bytes=0;};
struct Image {unsigned width=0,height=0,originalWidth=0,originalHeight=0;std::vector<uint8_t> pixels;};
using Picture=std::shared_ptr<const Image>;
using Items=std::shared_ptr<const std::vector<Item>>;
inline std::string identity(const Item& i,unsigned limit){return i.path.u8string()+":"+std::to_string(i.stamp)+":"+std::to_string(i.bytes)+":"+std::to_string(limit);}
inline std::vector<Item> scan(const std::vector<fs::path>& folders){
 std::vector<Item> result;std::error_code ec;
 for(const auto& folder:folders){for(fs::directory_iterator it(folder,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){
   const auto& e=*it;if(!e.is_regular_file(ec)||ec){ec.clear();continue;}auto ext=e.path().extension().u8string();for(auto& c:ext)c=char(std::tolower(static_cast<unsigned char>(c)));if(ext!=".jpg"&&ext!=".jpeg"&&ext!=".png"&&ext!=".bmp")continue;
   const auto bytes=e.file_size(ec);if(ec||!bytes||bytes>512ull*1024*1024){ec.clear();continue;}const auto stamp=e.last_write_time(ec);if(ec){ec.clear();continue;}result.push_back({e.path(),int64_t(stamp.time_since_epoch().count()),bytes});if(result.size()>=20000)break;
  }ec.clear();if(result.size()>=20000)break;}
 std::sort(result.begin(),result.end(),[](const Item& a,const Item& b){return a.stamp!=b.stamp?a.stamp>b.stamp:a.path.native()>b.path.native();});result.erase(std::unique(result.begin(),result.end(),[](const Item& a,const Item& b){return a.path==b.path;}),result.end());return result;
}
struct View {
 Items items=std::make_shared<const std::vector<Item>>();std::array<Picture,10> thumbs{};Picture image;
 int first=0,selected=-1,zoom=1;double panX=0,panY=0;bool fullscreen=false,controls=true,menu=false,details=false,slideshow=false,scanning=false,loaded=false,imageReady=false;
 uint64_t revision=1,now=0,openedAt=0;int sourceSlot=0;
};
class Model {
 struct Cached {Picture image;uint64_t used=0;};struct Job {Item item;unsigned limit=256;std::string key;};
 mutable std::mutex mutex;std::condition_variable wake;std::thread worker;bool stopping=false,scanRequested=false;
 View state;std::map<std::string,Cached> cache;std::deque<Job> jobs;std::string inflight;uint64_t used=0,nextScan=0,nextSlide=0,lastTap=0;double tapX=0,tapY=0;
 std::function<std::vector<fs::path>()> discover;std::function<Picture(const fs::path&,unsigned)> decode;
 int max_first()const{return std::max(0,(int(state.items->size())-10+4)/5)*5;}
 void schedule(){jobs.clear();auto request=[&](int index,unsigned limit){if(index<0||index>=int(state.items->size()))return;const auto& i=(*state.items)[index];auto key=identity(i,limit);if(cache.find(key)==cache.end()&&key!=inflight)jobs.push_back({i,limit,std::move(key)});};
  if(state.selected>=0){request(state.selected,2048);request(state.selected+1,2048);request(state.selected-1,2048);}else for(int i=0;i<10;++i)request(state.first+i,256);wake.notify_one();}
 void loop(){
  for(;;){Job job;bool rescan=false;{std::unique_lock<std::mutex> lock(mutex);wake.wait(lock,[&]{return stopping||scanRequested||!jobs.empty();});if(stopping)return;if(scanRequested){scanRequested=false;rescan=true;}else{job=std::move(jobs.front());jobs.pop_front();inflight=job.key;}}
   if(rescan){std::vector<Item> found;try{found=scan(discover());}catch(const std::exception&){}std::lock_guard<std::mutex> lock(mutex);if(stopping)return;
    bool changed=found.size()!=state.items->size();if(!changed)for(size_t i=0;i<found.size();++i)if(identity(found[i],256)!=identity((*state.items)[i],256)){changed=true;break;}
    if(changed){fs::path selected;if(state.selected>=0&&state.selected<int(state.items->size()))selected=(*state.items)[state.selected].path;state.items=std::make_shared<const std::vector<Item>>(std::move(found));if(!selected.empty()){state.selected=-1;for(size_t i=0;i<state.items->size();++i)if((*state.items)[i].path==selected){state.selected=int(i);break;}if(state.selected<0){state.zoom=1;state.slideshow=false;}}state.first=std::min(state.first,max_first());}
    state.scanning=false;state.loaded=true;++state.revision;schedule();
   }else{Picture image;try{image=decode(job.item.path,job.limit);}catch(const std::exception&){}std::lock_guard<std::mutex> lock(mutex);if(stopping)return;inflight.clear();cache[job.key]={std::move(image),++used};
    // At most three full-size pictures plus twenty thumbnail/failed entries.
    for(unsigned kind:{256u,2048u}){for(;;){size_t count=0;auto oldest=cache.end();const auto suffix=":"+std::to_string(kind);for(auto it=cache.begin();it!=cache.end();++it)if(it->first.size()>=suffix.size()&&it->first.compare(it->first.size()-suffix.size(),suffix.size(),suffix)==0){++count;bool pinned=false;const int start=kind==2048?state.selected-1:state.first,end=kind==2048?state.selected+2:state.first+10;for(int n=start;n<end;++n)if(n>=0&&n<int(state.items->size())&&(kind!=2048||state.selected>=0)&&it->first==identity((*state.items)[n],kind)){pinned=true;break;}if(!pinned&&(oldest==cache.end()||it->second.used<oldest->second.used))oldest=it;}if(count<=(kind==2048?3u:20u)||oldest==cache.end())break;cache.erase(oldest);}}
    ++state.revision;
   }
  }
 }
 void select_locked(int index,uint64_t now,bool animate){if(state.items->empty())return;state.selected=std::clamp(index,0,int(state.items->size())-1);state.sourceSlot=std::clamp(state.selected-state.first,0,9);state.zoom=1;state.panX=state.panY=0;state.controls=true;state.menu=state.details=false;state.openedAt=animate?now:0;++state.revision;schedule();}
public:
 ~Model(){stop();}
 void configure(std::function<std::vector<fs::path>()> folders,std::function<Picture(const fs::path&,unsigned)> loader){std::lock_guard<std::mutex> lock(mutex);if(!worker.joinable()&&!discover){discover=std::move(folders);decode=std::move(loader);}}
 void activate(uint64_t now){std::lock_guard<std::mutex> lock(mutex);state.now=now;if(!decode||!discover)return;if(!worker.joinable()){stopping=false;worker=std::thread([this]{loop();});}
  if(!nextScan||now>=nextScan){scanRequested=state.scanning=true;nextScan=now+5000;++state.revision;wake.notify_one();}
  if(state.slideshow&&now>=nextSlide&&!state.items->empty()){select_locked((state.selected+1)%int(state.items->size()),now,false);state.controls=false;nextSlide=now+3000;}
 }
 void pause(){std::lock_guard<std::mutex> lock(mutex);if(state.slideshow){state.slideshow=false;state.controls=true;++state.revision;}}
 void stop(){ {std::lock_guard<std::mutex> lock(mutex);stopping=true;jobs.clear();wake.notify_all();}if(worker.joinable())worker.join();std::lock_guard<std::mutex> lock(mutex);nextScan=0;inflight.clear();state.scanning=false;}
 View view(){std::lock_guard<std::mutex> lock(mutex);auto v=state;auto picture=[&](int index,unsigned limit,bool* ready=nullptr)->Picture{if(index<0||index>=int(state.items->size()))return {};auto it=cache.find(identity((*state.items)[index],limit));if(ready)*ready=it!=cache.end();if(it==cache.end())return {};it->second.used=++used;return it->second.image;};if(v.selected>=0)v.image=picture(v.selected,2048,&v.imageReady);for(int i=0;i<10;++i)v.thumbs[i]=picture(v.first+i,256);return v;}
 std::string key(uint64_t now){std::lock_guard<std::mutex> lock(mutex);return std::to_string(state.revision)+":"+std::to_string(state.openedAt&&now>=state.openedAt&&now-state.openedAt<200?now/16:0);}
 void open(int index,uint64_t now){std::lock_guard<std::mutex> lock(mutex);select_locked(index,now,true);lastTap=0;}
 void move(int delta,uint64_t now){std::lock_guard<std::mutex> lock(mutex);if(state.selected>=0)select_locked(state.selected+delta,now,false);}
 void scroll(int rows){std::lock_guard<std::mutex> lock(mutex);if(state.selected<0){state.first=std::clamp(state.first+rows*5,0,max_first());++state.revision;schedule();}}
 bool back(){std::lock_guard<std::mutex> lock(mutex);if(state.menu||state.details){state.menu=state.details=false;++state.revision;return true;}if(state.selected<0)return false;if(state.slideshow){state.slideshow=false;state.controls=true;}else{state.first=std::min(state.selected/5*5,max_first());state.selected=-1;state.zoom=1;state.openedAt=0;state.panX=state.panY=0;schedule();}++state.revision;return true;}
 void fullscreen(bool enabled){std::lock_guard<std::mutex> lock(mutex);if(state.fullscreen!=enabled){state.fullscreen=enabled;state.panX=state.panY=0;++state.revision;}}
 void zoom(){std::lock_guard<std::mutex> lock(mutex);if(state.selected>=0){state.zoom=state.zoom==1?2:1;state.panX=state.panY=0;++state.revision;}}
 void pan(double x,double y){std::lock_guard<std::mutex> lock(mutex);if(state.selected>=0&&state.zoom>1&&!state.menu&&!state.details){const auto found=cache.find(identity((*state.items)[state.selected],2048));if(found==cache.end()||!found->second.image)return;const auto& image=*found->second.image;const double scale=std::min(900./image.width,(state.fullscreen?544.:420.)/image.height)*state.zoom,mx=std::max(0.,(image.width*scale-900)/2),my=std::max(0.,(image.height*scale-(state.fullscreen?544:420))/2);state.panX=std::clamp(state.panX+x,-mx,mx);state.panY=std::clamp(state.panY+y,-my,my);++state.revision;}}
 void photo_tap(double x,double y,uint64_t now){std::lock_guard<std::mutex> lock(mutex);if(state.selected<0)return;if(lastTap&&now>=lastTap&&now-lastTap<=350&&std::hypot(x-tapX,y-tapY)<45){state.zoom=state.zoom==1?2:1;state.panX=state.panY=0;lastTap=0;}else{state.controls=!state.controls;lastTap=now;tapX=x;tapY=y;}state.slideshow=false;++state.revision;}
 void menu(){std::lock_guard<std::mutex> lock(mutex);state.menu=!state.menu;++state.revision;}
 void details(){std::lock_guard<std::mutex> lock(mutex);state.menu=false;state.details=state.selected>=0;++state.revision;}
 void refresh(){std::lock_guard<std::mutex> lock(mutex);cache.clear();state.menu=false;scanRequested=state.scanning=true;++state.revision;wake.notify_one();}
 void slideshow(uint64_t now){std::lock_guard<std::mutex> lock(mutex);if(state.items->empty())return;if(state.selected<0)select_locked(state.first,now,false);state.slideshow=true;state.controls=state.menu=false;nextSlide=now+3000;++state.revision;}
};
inline Model gallery;
}
