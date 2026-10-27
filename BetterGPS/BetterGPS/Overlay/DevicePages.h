#pragma once
#include "RenderActivity.h"
#include "CameraState.h"
#include "VehicleStart.h"
#include "CopilotInteraction.h"
#include "T680GaugeValues.h"
#include "DeskClockModel.h"
#include "DeskClockCities.h"
#include "CalculatorModel.h"
#include "GalleryModel.h"
#include "NavPlusImmersive.h"
#include <mmsystem.h>
namespace bettergps_overlay {
// Only the existing preparation worker owns the timelines. Input/telemetry
// publish bounded atomic signals; no timer thread or per-frame I/O is added.
enum class DevicePage {Legacy=-1,Home,Notrip,Nav,Gauges,Camera,Clock,Calculator,Gallery};
enum class PageDevice {None,Phone,Sony,Garmin};
enum class PageCause {None,VehicleInit,BootTimer,TouchHome,TouchMap,Unlock,Shortcut,Route,NotEligible};
inline const char* page_cause_name(PageCause cause){switch(cause){case PageCause::VehicleInit:return "vehicle-init";case PageCause::BootTimer:return "boot-home-timer";case PageCause::TouchHome:return "touch-home";case PageCause::TouchMap:return "touch-map";case PageCause::Unlock:return "unlock";case PageCause::Shortcut:return "shortcut";case PageCause::Route:return "route";case PageCause::NotEligible:return "not-eligible";default:return "none";}}
inline PageDevice managed_page_kind(const ManagedDevice& d){
 if(ford_console_id(d.knownId)||(d.known>=0&&size_t(d.known)<gpsDisplays.size()&&ford_console_id(gpsDisplays[d.known].id)))return PageDevice::None;
 if(d.style>=2)return PageDevice::Garmin;
 if(d.known>=0&&size_t(d.known)<gpsDisplays.size()&&gpsDisplays[d.known].independent)return PageDevice::Phone;
 if(home_lower(d.frameSkin)=="crown_victoria_sony.dds"||d.ui.find("sonygps")!=std::string::npos||(d.ui=="/ui/gps.sii"&&(d.vehicle=="ford.victoria_06"||d.vehicle=="ford.crown_victoria")))return PageDevice::Sony;
 if(!d.horizontal||(d.known>=0&&size_t(d.known)<gpsDisplays.size()&&gpsDisplays[d.known].independent))return PageDevice::Phone;
 return PageDevice::None;
}
enum CopilotBackgroundApp : uint32_t {CopilotRunning=1u<<0,ClockTimerRunning=1u<<1,ClockStopwatchRunning=1u<<2,ClockAlarmEnabled=1u<<3};
struct PageVisual {
 DevicePage page=DevicePage::Legacy;int transition=0;uint64_t elapsed=0,serial=0;
 PageCause cause=PageCause::None;
 int gauge=-1,fromGauge=-1;bool subpage=false;
 DevicePage fromPage=DevicePage::Legacy;
 bool cameraPopup=false;
 uint32_t backgroundApps=0;bool copilotLoading=false;
 // transition: +1 opens from Home, -1 closes the actual app, 0 is stable.
};
inline bool copilot_camera_live(const PageVisual& page){return page.page==DevicePage::Camera&&page.transition==0;}
inline bool copilot_forward_capture(const PageVisual& page){return page.transition>0&&page.page==DevicePage::Gauges&&(page.fromPage==DevicePage::Nav||page.fromPage==DevicePage::Notrip);}
struct DevicePageRuntime {
 std::atomic<uint64_t> requests{0};
 std::atomic<int> destination{0}; // 1 Home, 2 Map, 3 unlock-to-Home, 4 Gauges, 5 Back, 6..11 gauge detail, 12 Camera, 13 Clock, 14 Calculator, 15 Gallery.
 std::atomic<uint64_t> shownSerial{0},shownAt{0};
 std::atomic<uint64_t> cameraPopupDismissed{0};
 uint64_t epoch=0,lastTick=0,clock=0,homeElapsed=0,handled=0,origin=0,serial=0;
 bool initialized=false,visible=false,home=false,bootPending=false,forcedHome=false,gauges=false,camera=false,clockPage=false,calculatorPage=false,galleryPage=false,subpage=false;int transition=0,gauge=-1,fromGauge=-1;
 std::array<int,8> history{};unsigned historyDepth=0;
 DevicePage fromPage=DevicePage::Legacy;
 PageCause cause=PageCause::None;
 bool cameraPopup=false,cameraChecked=false;
 uint32_t backgroundApps=0;bool copilotLoading=false;
 PageVisual published;
};
inline std::array<DevicePageRuntime,managedDeviceCount> devicePages;
inline std::array<std::atomic<bool>,managedDeviceCount> managedCameraLayoutActive;
inline std::mutex pageVisualMutex;
inline constexpr uint64_t pageOpenMs=350,pageLogoHoldMs=1000,pageFadeMs=300,pageCloseMs=350;
inline thread_local DevicePage presentationPage=DevicePage::Legacy;
inline thread_local int presentationGauge=-1;
static Placement phone_app_bounds(double progress,bool waze);
struct PhoneHomeTarget {float left,top,right,bottom;};
static PhoneHomeTarget phone_home_target(const ManagedDevice&,bool);
struct DevicePageScope {DevicePage old=presentationPage;int oldGauge=presentationGauge;explicit DevicePageScope(DevicePage page,int gauge=-1){presentationPage=page;presentationGauge=gauge;}~DevicePageScope(){presentationPage=old;presentationGauge=oldGauge;}};
inline bool presentation_home(bool trip){return presentationPage==DevicePage::Home||(!trip&&presentationPage==DevicePage::Legacy);}
inline PageVisual managed_page_visual(unsigned slot){std::lock_guard<std::mutex> lock(pageVisualMutex);return devicePages[slot].published;}
inline void managed_page_presented(unsigned slot,uint64_t serial,uint64_t now){
 if(slot>=managedDeviceCount)return;
 std::lock_guard<std::mutex> lock(pageVisualMutex);auto& p=devicePages[slot];
 if(p.published.page==DevicePage::Home&&p.published.serial==serial&&p.shownSerial!=serial){p.shownAt=now;p.shownSerial=serial;}
}
inline void managed_pages_pause(){for(auto& p:devicePages)p.lastTick=0;}
inline bool managed_page_destination(unsigned slot,bool home){
 if(slot>=managedDeviceCount||!vehicleStart.eligible||!vehicleStart.epoch)return false;
 devicePages[slot].destination=home?1:2;return true;
}
inline bool managed_page_copilot_destination(unsigned slot,int destination){
 if(slot>=managedDeviceCount||!vehicleStart.eligible||!vehicleStart.epoch)return false;
 const auto d=managed_snapshot(slot);if(!managed_is_copilot(d)||d.style!=3)return false;
 devicePages[slot].destination=destination;return true;
}
inline bool managed_page_gauges(unsigned slot){return managed_page_copilot_destination(slot,4);}
inline bool managed_page_camera(unsigned slot){return managed_page_copilot_destination(slot,12);}
inline bool managed_page_clock(unsigned slot){return managed_page_copilot_destination(slot,13);}
inline bool managed_page_calculator(unsigned slot){return managed_page_copilot_destination(slot,14);}
inline bool managed_page_gallery(unsigned slot){return managed_page_copilot_destination(slot,15);}
inline void managed_camera_popup_ok(unsigned slot,uint64_t serial){if(slot<managedDeviceCount)devicePages[slot].cameraPopupDismissed=serial;}
inline bool managed_page_gauge(unsigned slot,unsigned gauge){return gauge<6&&managed_page_copilot_destination(slot,6+int(gauge));}
inline bool managed_page_back(unsigned slot){return managed_page_copilot_destination(slot,5);}
inline PageVisual managed_page_sample(unsigned slot,const ManagedDevice& d,uint64_t now,bool trip,bool visible,bool covered=false){
 auto& p=devicePages[slot];const auto kind=managed_page_kind(d);if(kind==PageDevice::None)return {};
 const auto epoch=vehicleStart.epoch.load(),requests=p.requests.load();const bool eligible=epoch&&vehicleStart.eligible.load();
 if(!p.initialized||p.epoch!=epoch){p.epoch=epoch;p.lastTick=now;p.clock=p.homeElapsed=0;p.handled=requests;p.destination=0;p.forcedHome=false;p.initialized=true;p.visible=false;p.bootPending=eligible&&d.prefs.bootHome&&d.prefs.bootHomeSeconds>0;p.home=p.bootPending;p.gauges=p.camera=p.clockPage=p.calculatorPage=p.galleryPage=p.cameraPopup=false;p.cameraPopupDismissed=0;p.cameraChecked=false;p.gauge=p.fromGauge=-1;p.subpage=false;p.historyDepth=0;p.transition=0;p.backgroundApps=0;p.copilotLoading=false;p.cause=PageCause::VehicleInit;++p.serial;}
 // Only time after a matching Home frame was submitted to the real display
 // counts. Splash, loading, pause and power-off cannot consume the dwell.
 if(p.bootPending&&p.home&&p.visible&&visible&&!covered&&p.lastTick&&p.shownSerial.load()==p.serial){const auto begin=std::max(p.lastTick,p.shownAt.load());if(now>begin)p.homeElapsed+=now-begin;}
 if(p.lastTick&&now>=p.lastTick)p.clock+=now-p.lastTick;p.lastTick=now;
 bool home=p.home;const int previousTarget=p.home?1:p.galleryPage?15:p.calculatorPage?14:p.clockPage?13:p.camera?12:p.gauges?(p.gauge<0?4:6+p.gauge):2,previousGauge=p.gauge;const auto previousPage=p.home?DevicePage::Home:p.galleryPage?DevicePage::Gallery:p.calculatorPage?DevicePage::Calculator:p.clockPage?DevicePage::Clock:p.camera?DevicePage::Camera:p.gauges?DevicePage::Gauges:trip?DevicePage::Nav:DevicePage::Notrip;
 int destination=p.destination.exchange(0);const bool goingBack=destination==5;if(goingBack)destination=p.historyDepth?p.history[--p.historyDepth]:1;
 if(destination&&destination!=previousTarget&&destination!=1&&!goingBack&&managed_is_copilot(d)&&d.style==3){if(p.historyDepth==p.history.size()){std::move(p.history.begin()+1,p.history.end(),p.history.begin());--p.historyDepth;}p.history[p.historyDepth++]=previousTarget;}if(destination==1)p.historyDepth=0;
 if(!eligible){home=false;p.gauges=p.camera=p.clockPage=p.calculatorPage=p.galleryPage=false;p.gauge=-1;p.historyDepth=0;p.bootPending=p.forcedHome=false;p.cause=PageCause::NotEligible;}
 else if(destination){p.galleryPage=destination==15&&managed_is_copilot(d)&&d.style==3;p.calculatorPage=destination==14&&managed_is_copilot(d)&&d.style==3;p.clockPage=destination==13&&managed_is_copilot(d)&&d.style==3;p.camera=destination==12&&managed_is_copilot(d)&&d.style==3;p.gauges=(destination==4||(destination>=6&&destination<12))&&managed_is_copilot(d)&&d.style==3;p.gauge=p.gauges&&destination>=6?destination-6:-1;if(destination!=3||!p.bootPending){p.bootPending=false;p.forcedHome=destination!=2;}home=destination!=2&&!p.gauges&&!p.camera&&!p.clockPage&&!p.calculatorPage&&!p.galleryPage;p.cause=destination==3?PageCause::Unlock:destination==1?PageCause::TouchHome:PageCause::TouchMap;}
 // An explicit Home shortcut has the same priority and persistence as touch.
 // Otherwise active guidance consumed the request, or reopened Nav next tick.
 else if(requests!=p.handled&&visible&&!p.cameraPopup){home=!home;p.bootPending=false;p.forcedHome=home;p.cause=PageCause::Shortcut;}
 else if(p.bootPending){if(!d.prefs.bootHome||p.homeElapsed>=uint64_t(std::max(0,d.prefs.bootHomeSeconds))*1000){p.bootPending=false;home=false;p.cause=PageCause::BootTimer;}}
 else if(trip&&!p.forcedHome){if(home)p.cause=PageCause::Route;home=false;}
 p.handled=requests;
 if(home){p.gauges=p.camera=p.clockPage=p.calculatorPage=p.galleryPage=false;p.gauge=-1;p.historyDepth=0;}
 const int nextTarget=home?1:p.galleryPage?15:p.calculatorPage?14:p.clockPage?13:p.camera?12:p.gauges?(p.gauge<0?4:6+p.gauge):2;
 if(nextTarget!=previousTarget){p.fromPage=previousPage;p.fromGauge=previousGauge;p.subpage=previousTarget>=4&&nextTarget>=4;if(nextTarget==12&&!goingBack){p.cameraPopup=false;p.cameraChecked=false;}else if(nextTarget!=12)p.cameraPopup=false;p.home=home;p.origin=p.clock;++p.serial;p.transition=kind==PageDevice::Phone&&p.visible&&visible?(home?-1:1):(managed_is_international(d)||managed_is_copilot(d))&&d.style==3&&p.visible&&visible?(home||goingBack?-1:1):0;}
 // Only a fresh vehicle initialization clears resident applications. Ignition,
 // screen power and navigation between pages leave this mask intact.
 const bool copilot=managed_is_copilot(d)&&d.style==3;
 if(nextTarget!=2||!eligible||!visible)p.copilotLoading=false;
 if(copilot&&eligible&&visible&&!covered&&nextTarget==2&&!(p.backgroundApps&CopilotRunning)&&!p.copilotLoading&&p.transition>=0){
  p.copilotLoading=true;if(!p.transition){p.origin=p.clock;p.fromPage=DevicePage::Home;p.subpage=false;++p.serial;p.transition=1;}
 }
 if(!visible){p.transition=0;p.visible=false;}else p.visible=true;
 const uint64_t elapsed=p.clock-p.origin;const uint64_t end=copilot?(p.transition>0?copilotOpenDelayMs+copilotSlideMs+(p.copilotLoading?copilotMapWaitMs:0):copilotCloseDelayMs+copilotSlideMs):(p.transition>0?pageOpenMs+pageLogoHoldMs+pageFadeMs:pageCloseMs);
 if(p.transition&&elapsed>=end&&(!p.copilotLoading||!covered)){if(p.copilotLoading&&visible){p.backgroundApps|=CopilotRunning;p.copilotLoading=false;}p.transition=0;}
 if(p.camera&&!p.cameraChecked){const auto camera=bettergps_camera::cameras.snapshot();if(camera.epoch==epoch&&camera.known){p.cameraPopup=camera.available==0;p.cameraChecked=true;}}
 if(p.cameraPopupDismissed.load()==p.serial)p.cameraPopup=false;PageVisual result{p.home?DevicePage::Home:p.galleryPage?DevicePage::Gallery:p.calculatorPage?DevicePage::Calculator:p.clockPage?DevicePage::Clock:p.camera?DevicePage::Camera:p.gauges?DevicePage::Gauges:trip?DevicePage::Nav:DevicePage::Notrip,p.transition,elapsed,p.serial,p.cause,p.gauge,p.fromGauge,p.subpage,p.fromPage,p.cameraPopup,p.backgroundApps|(managed_is_copilot(d)?bettergps_deskclock::clock.background():0u),p.copilotLoading};
 {std::lock_guard<std::mutex> lock(pageVisualMutex);p.published=result;}return result;
}
inline std::atomic<int> pageLatchedKey{0};
inline bool managed_page_shortcut_conflict(int key,int mods){std::lock_guard<std::mutex> lock(managedMutex);for(const auto& d:managedDevices)if(d.used&&d.prefs.pageKey==key&&d.prefs.pageMods==mods)return true;return false;}
inline bool managed_page_shortcut_valid(int key,int mods){
 if(!shortcut_key_valid(key,mods)||cursor_shortcut_matches(key,mods)||(key==settingsMenuKey&&mods==settingsMenuMods)||(key==displayPowerKey&&mods==displayPowerMods))return false;
 std::lock_guard<std::mutex> lock(managedMutex);for(const auto& d:managedDevices)if(d.used&&d.prefs.key==key&&d.prefs.mods==mods)return false;return true;
}
inline bool managed_page_key_event(int key,int mods,bool down,bool foreground){
 const int latched=pageLatchedKey.load();if(latched==key){if(!down)pageLatchedKey=0;return true;}if(!down||!foreground||!render_gameplay_allowed())return false;
 const auto vehicle=managed_vehicle();int chosen=-1;
 for(unsigned i=0;i<managedDeviceCount;++i){auto d=managed_snapshot(i);if(d.used&&d.enabled&&d.vehicle==vehicle&&managed_page_kind(d)!=PageDevice::None&&d.prefs.pageKey==key&&d.prefs.pageMods==mods){if(chosen<0||int(i)==managedSelected)chosen=int(i);}}
 if(chosen<0)return false;pageLatchedKey=key;if(vehicleStart.eligible.load()&&vehicleStart.epoch.load())devicePages[chosen].requests.fetch_add(1);return true;
}
inline void managed_page_input(bool foreground){
 const int latched=pageLatchedKey.load();if(latched){if(!(GetAsyncKeyState(latched)&0x8000))pageLatchedKey=0;else return;}
 const int mods=display_key_modifiers();for(unsigned i=0;i<managedDeviceCount;++i){auto d=managed_snapshot(i);if(d.used&&d.prefs.pageKey&&(GetAsyncKeyState(d.prefs.pageKey)&0x8000)&&managed_page_key_event(d.prefs.pageKey,mods,true,foreground))return;}
}
}
