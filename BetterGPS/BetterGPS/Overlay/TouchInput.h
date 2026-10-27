#pragma once
#include <deque>
namespace bettergps_overlay {
struct TouchSurface {
 unsigned slot=~0u;float left=0,top=0,width=1,height=1;
 uint64_t epoch=0,power=0,page=0;
 // A composed UI copy is not clickable until its same-frame GPU proof passes.
 uint64_t proof=0;
};
inline const GUID kTouchSurface={0x4765ce71,0x13cb,0x4a76,{0x86,0x62,0x23,0x29,0x8c,0x87,0x1b,0x61}};
struct TouchHit {
 TouchSurface surface;float u=0,v=0;POINT cursor{};uint64_t time=0;
 uint64_t trace=0;
 bool valid=false;
};
struct TouchGesture {TouchHit first,last;bool swipe=false,pan=false;};
inline std::atomic<bool(*)(const TouchHit&)> touchPanEligible{nullptr},touchClockGestureEligible{nullptr};
#ifdef BETTERGPS_TOUCH_TEST
inline bool touchFixtureForeground=false;
#endif
inline bool touch_active(){
 if(!settingsOpen.load()&&!cursorOnlyActive.load())return false;
 bool foreground=settings_mouse_foreground();
#ifdef BETTERGPS_TOUCH_TEST
 foreground=foreground||touchFixtureForeground;
#endif
 return (settingsOpen||cursorOnlyActive)&&foreground&&render_gpu_allowed();
}
// Passive hover uses the same image-copy, projection and depth proofs as a click.
// It can reveal chrome only: it never enters the gesture/action queue.
struct ImmersiveHoverProbe {
 std::mutex mutex;POINT cursor{};bool initialized=false,pending=false;uint64_t movedAt=0,trace=0;
 void pointer(POINT p,bool held,uint64_t now){std::lock_guard<std::mutex> lock(mutex);if(held||!navPlusImmersive.active()){pending=false;cursor=p;initialized=true;return;}if(!initialized||p.x!=cursor.x||p.y!=cursor.y){cursor=p;initialized=pending=true;movedAt=now;trace=(uint64_t(1)<<63)|((trace+1)&~(uint64_t(1)<<63));}}
 bool request(POINT& p,uint64_t& id,uint64_t& began){if(!navPlusImmersive.active())return false;std::lock_guard<std::mutex> lock(mutex);const auto now=GetTickCount64();if(!pending||now<movedAt||now-movedAt>300)return false;p=cursor;id=trace;began=movedAt;return true;}
 void hit(const TouchHit& h,uint64_t now){if(!h.valid||!navPlusImmersive.context(h.surface.slot,h.surface.epoch,h.surface.power,h.surface.page))return;{std::lock_guard<std::mutex> lock(mutex);if(!pending||now<movedAt||now-movedAt>300||h.trace!=trace||std::abs(cursor.x-h.cursor.x)>4||std::abs(cursor.y-h.cursor.y)>4)return;pending=false;}navPlusImmersive.activity(h.surface.slot,now);}
 void cancel(){std::lock_guard<std::mutex> lock(mutex);initialized=pending=false;}
};
inline ImmersiveHoverProbe immersiveHoverProbe;
struct TouchInput {
 std::mutex mutex;bool down=false,tracking=false,released=false;POINT press{},current{};uint64_t began=0;
 TouchHit first,last;std::deque<TouchGesture> actions;uint64_t trace=0;bool panCandidate=false,panning=false;
 uint64_t timeout()const{return panCandidate?60000:2500;}
 struct Request {POINT cursor{};uint64_t trace=0,began=0;unsigned slot=~0u;bool hover=false;};
 bool request(Request& out){std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock)return false;if(!tracking){out={};out.hover=true;return immersiveHoverProbe.request(out.cursor,out.trace,out.began);}if(GetTickCount64()-began>timeout()){tracking=false;touchDiagnostics.note(TouchStage::Timeout,"no complete CPU screen gesture after 2500 ms");return false;}out={first.valid?current:press,trace,began,first.valid?first.surface.slot:~0u};return true;}
 bool gesture_requested(){Request out;return request(out)&&!out.hover;}
 void cancel(){std::lock_guard<std::mutex> lock(mutex);if(tracking)touchDiagnostics.note(TouchStage::Cancelled,"mode/focus/power changed");down=tracking=released=panCandidate=panning=false;first=last={};actions.clear();}
 static bool close_point(POINT a,POINT b,int tolerance=4){return std::abs(a.x-b.x)<=tolerance&&std::abs(a.y-b.y)<=tolerance;}
 void pointer(POINT p,bool held,uint64_t now){
  std::lock_guard<std::mutex> lock(mutex);current=p;
  if(held&&!down){trace=touchDiagnostics.begin(p);press=p;began=now;tracking=true;released=panCandidate=panning=false;first=last={};}
  if(!held&&down){released=true;touchDiagnostics.note(TouchStage::Release,"client=%ld,%ld",p.x,p.y);}down=held;
 }
 POINT cursor(POINT fallback){std::lock_guard<std::mutex> lock(mutex);return tracking&&!first.valid?press:tracking?current:fallback;}
 bool requested(){std::lock_guard<std::mutex> lock(mutex);if(!tracking){POINT p{};uint64_t id=0,at=0;return immersiveHoverProbe.request(p,id,at);}if(tracking&&GetTickCount64()-began>timeout()){touchDiagnostics.note(TouchStage::Timeout,"no complete screen gesture after 2500 ms");tracking=false;}return tracking;}
 void hit(const TouchHit& value){
  if(!touch_active())return;
  std::lock_guard<std::mutex> lock(mutex);if(!tracking||value.trace!=trace||value.time<began)return;
  touchStatus.hit(value.trace,value.surface.slot,value.u,value.v,value.valid,value.cursor,value.time);
  if(value.time>began+timeout()){touchDiagnostics.note(TouchStage::Timeout,"late hit");tracking=false;return;}
  const auto previous=last;
  if(!first.valid){if(!close_point(value.cursor,press))return;if(!value.valid){touchDiagnostics.note(TouchStage::PixelMiss,"no valid surface at press position");tracking=false;return;}first=last=value;const auto eligible=touchPanEligible.load();panCandidate=eligible&&eligible(value);}
  else if(close_point(value.cursor,current)){
   if(!value.valid||value.surface.slot!=first.surface.slot||value.surface.power!=first.surface.power||value.surface.epoch!=first.surface.epoch||value.surface.page!=first.surface.page){touchDiagnostics.note_for(trace,TouchStage::Cancelled,"surface lost or generation changed during gesture");tracking=released=false;return;}if(value.time<last.time)return;last=value;
  }
  if(panCandidate&&close_point(last.cursor,current)&&(panning||!close_point(press,current,6))){
   const auto from=panning&&previous.valid?previous:first;
   if(!actions.empty()&&actions.back().pan&&actions.back().first.trace==trace)actions.back().last=last;
   else if(actions.size()<16)actions.push_back({from,last,false,true});
   panning=true;if(released)tracking=released=false;return;
  }
  if(!released||!close_point(last.cursor,current))return;
  const float dx=last.u-first.u,dy=last.v-first.v;
  const auto appGesture=touchClockGestureEligible.load();const bool clockSwipe=appGesture&&appGesture(first)&&(std::fabs(dx)>.12f||std::fabs(dy)>.12f);
  const bool swipe=clockSwipe||(first.v>=.78f&&dy<=-.18f&&std::fabs(dx)<.32f);
  const bool tap=std::fabs(dx)<.035f&&std::fabs(dy)<.035f&&close_point(press,current,12);
  if((tap||swipe)&&actions.size()<16){actions.push_back({first,last,swipe});touchDiagnostics.note_for(first.trace,TouchStage::GestureQueued,"slot=%u kind=%s uv=%.5f,%.5f",first.surface.slot,swipe?"swipe":"tap",last.u,last.v);}
  else touchDiagnostics.note(TouchStage::GestureRejected,"tap=%u swipe=%u queue=%zu delta=%.5f,%.5f",unsigned(tap),unsigned(swipe),actions.size(),dx,dy);
  tracking=released=false;
 }
 std::deque<TouchGesture> take(){std::lock_guard<std::mutex> lock(mutex);std::deque<TouchGesture> result;result.swap(actions);return result;}
};
inline TouchInput touchInput;
inline void touch_pointer(POINT p,bool held,uint64_t now){if(touch_active()){immersiveHoverProbe.pointer(p,held,now);touchInput.pointer(p,held,now);}else{immersiveHoverProbe.cancel();touchInput.cancel();}}
inline void touch_cancel(){immersiveHoverProbe.cancel();touchInput.cancel();}
inline bool touch_surface_read(ID3D11Resource* resource,TouchSurface& value){UINT bytes=sizeof(value);return resource&&SUCCEEDED(resource->GetPrivateData(kTouchSurface,&bytes,&value))&&bytes==sizeof(value)&&value.slot<managedDeviceCount;}
inline void touch_tag_texture(ID3D11Resource* resource,unsigned slot,float left=0,float top=0,float width=1,float height=1){
 if(!resource||slot>=managedDeviceCount)return;const auto d=managed_snapshot(slot);if(managed_page_kind(d)==PageDevice::None||!d.enabled)return;
 TouchSurface data{slot,left,top,width,height,vehicleStart.epoch.load(),managedPower[slot].serial.load(),managed_page_visual(slot).serial};resource->SetPrivateData(kTouchSurface,sizeof(data),&data);
}
inline void touch_copy_tag(ID3D11Resource* destination,ID3D11Resource* source){
 if(!destination||!source)return;TouchSurface data;if(touch_surface_read(source,data))destination->SetPrivateData(kTouchSurface,sizeof(data),&data);else if(touch_surface_read(destination,data))destination->SetPrivateData(kTouchSurface,0,nullptr);
}
inline void touch_copy_region_tag(ID3D11Resource* destination,UINT destinationSub,UINT x,UINT y,UINT z,ID3D11Resource* source,UINT sourceSub,const D3D11_BOX* box){
 // Preserve identity only for a complete base-level copy with identical extents.
 // A full explicit box is just as valid as nullptr; partial/scaled/offset copies
 // cannot establish the same UV mapping and must not acquire a device tag.
 if(!destination||!source||destinationSub||sourceSub||x||y||z)return;
 TouchSurface associated;if(!touch_surface_read(source,associated)&&!touch_surface_read(destination,associated))return;
 ID3D11Texture2D *dst=nullptr,*src=nullptr;destination->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&dst);source->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&src);
 if(dst&&src){D3D11_TEXTURE2D_DESC dd{},sd{};dst->GetDesc(&dd);src->GetDesc(&sd);if(dd.Width==sd.Width&&dd.Height==sd.Height&&(!box||(!box->left&&!box->top&&!box->front&&box->right==sd.Width&&box->bottom==sd.Height&&box->back==1)))touch_copy_tag(destination,source);}
 if(dst)dst->Release();if(src)src->Release();
}
inline void touch_tag_view(ID3D11ShaderResourceView* view,unsigned slot){if(!view)return;ID3D11Resource* resource=nullptr;view->GetResource(&resource);touch_tag_texture(resource,slot);if(resource)resource->Release();}
}
