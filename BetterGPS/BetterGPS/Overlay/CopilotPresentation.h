#pragma once
// CPU presentation only. Native ATS still produces the map and route geometry.
static constexpr COLORREF copilotOrange=RGB(255,185,0);
static const GarminImage& copilot_image(const std::string& file){return page_image("copilot/t680/"+file);}
// Home content is a catalogue of real menu pages, independent of status chrome.
// Coordinates remain registered in the original 1324x800 source canvas.
struct CopilotHomePage {const char* texture;Placement source;};
static constexpr CopilotHomePage copilotHomePages[]={{"navplushd_homescreen_toplayer.dds",{73,126,1244,586}}};
static constexpr unsigned copilot_home_page_count(){return unsigned(sizeof(copilotHomePages)/sizeof(*copilotHomePages));}
static Placement copilot_home_foreground_bounds(const Placement& bounds,unsigned page=0){
 const auto& p=copilotHomePages[std::min(page,copilot_home_page_count()-1)].source;
 return {bounds.x+int(std::lround(bounds.w*p.x/1324.)),bounds.y+int(std::lround(bounds.h*p.y/800.)),int(std::lround(bounds.w*p.w/1324.)),int(std::lround(bounds.h*p.h/800.))};
}
static Placement copilot_page_indicator_bounds(const Placement& bounds,unsigned index,unsigned count){
 if(!count||index>=count)return {0,0,0,0};
 constexpr double diameter=34,pitch=44;const double width=count*diameter+(count-1)*(pitch-diameter);
 return {bounds.x+int(std::lround(bounds.w*((1324-width)*.5+index*pitch)/1324)),bounds.y+int(std::lround(bounds.h*84./800)),int(std::lround(bounds.w*diameter/1324)),int(std::lround(bounds.h*diameter/800))};
}
static void copilot_page_indicators(std::vector<uint8_t>& pixels,const Placement& bounds,unsigned count,unsigned current){
 if(!count)return;current=std::min(current,count-1);
 for(unsigned i=0;i<count;++i){const auto& dot=copilot_image(i==current?"navplushd_page_full.dds":"navplushd_page_empty.dds");overlay_blit(pixels,dot.pixels,dot.width,dot.height,copilot_page_indicator_bounds(bounds,i,count),900,544);}
}
static void copilot_home_artwork(std::vector<uint8_t>& pixels,const Placement& bounds,unsigned page=0){
 const auto& background=copilot_image("navplushd_gradient_background.dds");overlay_blit(pixels,background.pixels,background.width,background.height,bounds,900,544);
 page=std::min(page,copilot_home_page_count()-1);const auto& top=copilot_image(copilotHomePages[page].texture);
 overlay_blit(pixels,top.pixels,top.width,top.height,copilot_home_foreground_bounds(bounds,page),900,544);
 copilot_page_indicators(pixels,bounds,copilot_home_page_count(),page);
}
#include "CopilotGaugesPresentation.h"
static std::string copilot_weekday(int64_t minutes){static const char* days[]={"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};return days[(std::max<int64_t>(0,minutes)/1440)%7];}
static int64_t copilot_minutes(){return current_game_minutes();}
#include "CopilotClockPresentation.h"
#include "CalculatorPresentation.h"
#include "GalleryPresentation.h"
static std::string copilot_clock(){const auto now=copilot_minutes();if(now<0)return "--:--";const int h=int(now%1440)/60;char text[16];std::snprintf(text,sizeof(text),"%d:%02d",h%12?h%12:12,int(now%60));return text;}
static std::string copilot_eta(){auto arrival=garmin_arrival();if(!arrival.day.empty()){auto day=arrival.day;for(size_t i=1;i<day.size();++i)day[i]=char(std::tolower(static_cast<unsigned char>(day[i])));return day+" "+arrival.time;}return arrival.time;}
static std::string copilot_distance(){auto text=current_distance_text();const auto suffix=text.find(" - ");if(suffix!=std::string::npos)text.resize(suffix);return text;}
static void copilot_round(std::vector<uint8_t>& pixels,int width,int height,RECT rect,int radius,COLORREF color,unsigned opacity=255){
 for(int y=std::max(0L,rect.top);y<std::min(LONG(height),rect.bottom);++y)for(int x=std::max(0L,rect.left);x<std::min(LONG(width),rect.right);++x){
  const double cx=std::clamp(x+.5,double(rect.left+radius),double(rect.right-radius)),cy=std::clamp(y+.5,double(rect.top+radius),double(rect.bottom-radius));const double dx=x+.5-cx,dy=y+.5-cy;const unsigned a=radius<=0?opacity:unsigned(std::lround(opacity*std::clamp(radius+.5-std::sqrt(dx*dx+dy*dy),0.,1.)));if(!a)continue;
  auto* p=pixels.data()+(size_t(y)*width+x)*4;const unsigned old=p[3],sum=a+(old*(255-a)+127)/255;const unsigned rgb[]={GetBValue(color),GetGValue(color),GetRValue(color)};for(unsigned c=0;c<3;++c)p[c]=uint8_t((rgb[c]*a+(p[c]*old*(255-a)+127)/255+sum/2)/sum);p[3]=uint8_t(sum);
 }
}
static std::vector<uint8_t> copilot_speed_limit(bool dark){
 constexpr int w=78,h=104;std::vector<uint8_t> pixels(w*h*4,0);const COLORREF paper=dark?RGB(10,10,10):RGB(255,255,255),ink=dark?RGB(255,255,255):RGB(0,0,0);
 copilot_round(pixels,w,h,{0,0,w,h},9,RGB(65,65,65));copilot_round(pixels,w,h,{2,2,w-2,h-2},8,paper);copilot_round(pixels,w,h,{6,6,w-6,h-6},7,ink);copilot_round(pixels,w,h,{8,8,w-8,h-8},5,paper);
 garmin_text(pixels,w,h,"SPEED",{9,10,w-9,30},20,ink,FW_BOLD,true);garmin_text(pixels,w,h,"LIMIT",{9,30,w-9,50},20,ink,FW_BOLD,true);garmin_text(pixels,w,h,current_speed_limit_text(),{9,49,w-9,98},43,ink,FW_BOLD,true);return pixels;
}
static std::vector<uint8_t> copilot_real_speed(bool dark){
 std::vector<uint8_t> pixels(78*43*4,0);const bool over=current_speed_is_over_limit();const auto paper=over?RGB(201,0,15):dark?RGB(255,255,255):RGB(0,0,0);const auto ink=over||!dark?RGB(255,255,255):RGB(0,0,0);
 copilot_round(pixels,78,43,{0,0,78,43},5,paper,over?235:175);garmin_text(pixels,78,43,current_speed_text(),{4,1,74,42},34,ink,FW_NORMAL,true);return pixels;
}
static void copilot_crop(std::vector<uint8_t>& out,const GarminImage& image,int top,int bottom){out.clear();if(image.pixels.empty()||top<0||bottom>int(image.height)||top>=bottom)return;out.resize(size_t(image.width)*(bottom-top)*4);memcpy(out.data(),image.pixels.data()+size_t(top)*image.width*4,out.size());}
static const ShieldBitmap& copilot_shield(const std::string& identity,const std::wstring& directory){
 static std::map<std::string,ShieldBitmap> cache;static std::wstring previous;static uint64_t revision=0;const auto stamp=shield_revision(directory);
 if(previous!=directory||revision!=stamp){cache.clear();previous=directory;revision=stamp;}
 auto found=cache.find(identity);if(found!=cache.end())return found->second;
 auto image=load_route_shield(identity,directory);size_t colored=0,covered=0;
 for(size_t i=0;i<image.bgra.size();i+=4)if(image.bgra[i+3]>127){++covered;const auto* p=image.bgra.data()+i;const int high=std::max({p[0],p[1],p[2]}),low=std::min({p[0],p[1],p[2]});colored+=high-low>25;}
 const bool invert=covered&&colored*10>covered;
 for(size_t i=0;i<image.bgra.size();i+=4){auto* p=image.bgra.data()+i;int gray=(19*p[0]+183*p[1]+54*p[2]+128)/256;if(invert)gray=std::clamp((255-gray)*255/180,0,255);p[0]=p[1]=p[2]=uint8_t(gray);}
 return cache.emplace(identity,std::move(image)).first->second;
}
static std::vector<uint8_t> copilot_road(const GuidancePresentation& p,const bettergps_navigation::Display& guidance,const std::wstring& directory=bettergps_navigation::ShieldDirectory()){
 constexpr int width=384,height=80;std::vector<uint8_t> pixels(width*height*4,0);
 if(!p.message.empty()){garmin_text(pixels,width,height,p.message,{0,0,width,height},34,RGB(255,255,255),FW_NORMAL,true);return pixels;}
 // Tokenize roads into the same committed shield catalog as Google/Waze.
 // Measure every run; wrap at most twice, shrinking the font for longer roads.
 struct Run{std::string text;const ShieldBitmap* shield=nullptr;int width=0;};std::vector<Run> runs;
 const std::map<std::string,std::string> verbs{{"CONTINUE","Continue on"},{"LEFT","Turn left on"},{"RIGHT","Turn right on"},{"KEEP_LEFT","Keep left on"},{"KEEP_RIGHT","Keep right on"},{"MERGE_LEFT","Merge left on"},{"MERGE_RIGHT","Merge right on"},{"EXIT_LEFT","Exit left to"},{"EXIT_RIGHT","Exit right to"},{"UTURN","Make a U-turn"}};
 auto action=p.action;if(action.rfind("TURN_",0)==0)action.erase(0,5);auto verb=verbs.find(action);std::string prefix=verb==verbs.end()?"":verb->second;if(!guidance.exit.empty())prefix="Exit "+guidance.exit+" -";
 auto words=[&](const std::string& text){std::istringstream in(text);std::string word;while(in>>word)runs.push_back({word});};words(prefix);
 for(const auto& road:road_label_parts(p.road)){const auto* shield=road.numbered?&copilot_shield(road.shield.identity,directory):nullptr;if(shield&&!shield->bgra.empty()){runs.push_back({"",shield});words(road.shield.beside);}else words(road.text);}
 if(runs.empty())words(p.distance);int font=34,rows=1,shieldSize=38;HDC dc=CreateCompatibleDC(nullptr);if(!dc)return pixels;HFONT selected=nullptr;HGDIOBJ previous=nullptr;
 for(;font>=16;font-=2){selected=CreateFontW(-font,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Arial");previous=SelectObject(dc,selected);int x=0;rows=1;shieldSize=font+4;for(auto& run:runs){SIZE size{};const auto text=wide_text(run.text);GetTextExtentPoint32W(dc,text.c_str(),int(text.size()),&size);run.width=run.shield?shieldSize:std::min(width,int(size.cx));if(x&&x+run.width>width){++rows;x=0;}x+=run.width+7;}SelectObject(dc,previous);DeleteObject(selected);if(rows<=2)break;}
 DeleteDC(dc);font=std::max(font,16);int x=0,row=0;const int line=40,top=rows==1?20:0;
 for(const auto& run:runs){if(x&&x+run.width>width){++row;x=0;}if(row>=2)break;if(run.shield)paint_route_shield(pixels,width,height,*run.shield,x,top+row*line,shieldSize);else garmin_text(pixels,width,height,run.text,{x,top+row*line,std::min(width,x+run.width+1),top+(row+1)*line},font,RGB(255,255,255));x+=run.width+7;}
 return pixels;
}
static std::vector<uint8_t> copilot_maneuver(const GuidancePresentation& p){
 std::vector<uint8_t> maneuver(132*68*4,0);const auto icon=maneuver_icon(p.action);if(icon.alpha){std::vector<uint8_t> arrow(size_t(icon.size)*icon.size*4,0);for(size_t i=0;i<size_t(icon.size)*icon.size;++i){arrow[i*4]=GetBValue(copilotOrange);arrow[i*4+1]=GetGValue(copilotOrange);arrow[i*4+2]=GetRValue(copilotOrange);arrow[i*4+3]=icon.alpha[i];}overlay_blit(maneuver,arrow,icon.size,icon.size,{0,4,52,60},132,68);}
 auto distanceText=p.distance;const auto exitSuffix=distanceText.find(" \xC2\xB7");if(exitSuffix!=std::string::npos)distanceText.resize(exitSuffix);const auto unit=distanceText.find_last_of(' ');if(unit!=std::string::npos&&(distanceText.substr(unit+1)=="mi"||distanceText.substr(unit+1)=="ft"||distanceText.substr(unit+1)=="km"||distanceText.substr(unit+1)=="m")){garmin_text(maneuver,132,68,distanceText.substr(0,unit),{57,0,132,41},36,copilotOrange,FW_NORMAL,true);garmin_text(maneuver,132,68,distanceText.substr(unit+1),{57,39,132,68},26,copilotOrange,FW_NORMAL,true);}else garmin_text(maneuver,132,68,distanceText,{57,1,132,67},28,copilotOrange,FW_NORMAL,true);return maneuver;
}
static Placement copilot_camera_selector_bounds(){return {0,120,153,306};}
static std::vector<uint8_t> copilot_camera_layer(){
 const auto state=bettergps_camera::cameras.snapshot();std::vector<uint8_t> pixels(size_t(900)*544*4,0);
 if(!bettergps_camera::feed_ready(state)){for(size_t p=0;p<pixels.size();p+=4){pixels[p]=pixels[p+1]=pixels[p+2]=10;pixels[p+3]=255;}}
 const auto bounds=copilot_camera_selector_bounds();
 auto sprite=[&](const char* name,float x,float y,float w,float h,bool flipX=false,bool flipY=false){const auto& image=copilot_image(name);if(image.pixels.empty()||!image.width||!image.height)return;auto data=image.pixels;if(flipX||flipY)for(unsigned v=0;v<image.height;++v)for(unsigned u=0;u<image.width;++u){const auto from=(size_t(flipY?image.height-v-1:v)*image.width+(flipX?image.width-u-1:u))*4,to=(size_t(v)*image.width+u)*4;std::copy_n(image.pixels.data()+from,4,data.data()+to);}overlay_blit(pixels,data,image.width,image.height,{int(std::lround(bounds.x+x*bounds.w/300)),int(std::lround(bounds.y+y*bounds.h/600)),int(std::lround(w*bounds.w/300)),int(std::lround(h*bounds.h/600))},900,544);};
 if(state.available&bettergps_camera::bit(state.selected))switch(state.selected){
  case bettergps_camera::View::Forward:sprite("navplushd_camera_reverse_active.dds",0,0,300,128,true,true);break;
  case bettergps_camera::View::Left:sprite("navplushd_camera_left_active.dds",0,0,101,600);break;
  case bettergps_camera::View::Right:sprite("navplushd_camera_left_active.dds",199,0,101,600,true);break;
  case bettergps_camera::View::Reverse:sprite("navplushd_camera_reverse_active.dds",0,472,300,128);break;
 }
 sprite("navplushd_camera_icon.dds",0,0,300,600);
 const bool front=state.available&1,left=state.available&2,right=state.available&4,rear=state.available&8;
 if(front||left)sprite("navplushd_camera_bottom_right.dds",0,0,96,119,true,true);
 if(front||right)sprite("navplushd_camera_bottom_left.dds",204,0,96,119,true,true);
 if(rear||left)sprite("navplushd_camera_bottom_left.dds",0,481,96,119);
 if(rear||right)sprite("navplushd_camera_bottom_right.dds",204,481,96,119);
 return pixels;
}
static Placement copilot_camera_popup_bounds(){return {263,234,374,78};}
static std::vector<uint8_t> copilot_app_layer(DevicePage page,int gauge){return page==DevicePage::Gallery?copilot_gallery_layer():page==DevicePage::Calculator?copilot_calculator_layer():page==DevicePage::Clock?copilot_clock_layer():page==DevicePage::Camera?copilot_camera_layer():copilot_gauges_layer(t680Gauges.get(vehicleStart.epoch.load()),gauge);}
static void render_copilot(std::vector<uint8_t>& pixels,const OverlaySettings& config,const bettergps_navigation::Display& guidance=bettergps_navigation::Current(),bool recenter=false){
 constexpr int w=copilotWidth,h=copilotHeight;pixels.assign(w*h*4,0);const bool trip=g_tripPresent.load(),home=presentation_home(trip),dark=presentation_dark();
 auto module=[&](size_t slot,const std::vector<uint8_t>& source,int sw,int sh){auto p=config.items[slot];if(presentationImmersive.mode){if(slot==0||slot==8)p.y+=presentationImmersive.top();else if(slot==12)p.y+=presentationImmersive.bottom();}overlay_blit(pixels,source,sw,sh,p,w,h);};
 auto artwork=[&](unsigned slot,const char* name){const auto& image=copilot_image(name);module(slot,image.pixels,image.width,image.height);};
 const bool gauges=presentationPage==DevicePage::Gauges,camera=presentationPage==DevicePage::Camera,clockPage=presentationPage==DevicePage::Clock,calculatorPage=presentationPage==DevicePage::Calculator,galleryPage=presentationPage==DevicePage::Gallery;
 if(home)copilot_home_artwork(pixels,config.items[1]);
 else if(gauges)module(1,copilot_gauges_layer(t680Gauges.get(vehicleStart.epoch.load()),presentationGauge),900,544);
 else if(camera)module(1,copilot_camera_layer(),900,544);
 else if(galleryPage)module(1,copilot_gallery_layer(),900,544);
 else if(calculatorPage)module(1,copilot_calculator_layer(),900,544);
 else if(clockPage)module(1,copilot_clock_layer(),900,544);
 else{artwork(4,"copilot_v9_zoom.dds");artwork(trip?6:7,trip?"copilot_v9_belowfooter.dds":"copilot_v9_notrip.dds");}
 // Shared chrome is independent of the Home base and both native-map pages.
 artwork(0,"navplushd_top_5by3.dds");artwork(12,"navplushd_bottom_5by3.dds");
 std::vector<uint8_t> clock(235*42*4,0);copilot_round(clock,235,42,{0,0,90,42},8,RGB(255,255,255));garmin_text(clock,235,42,copilot_clock(),{2,0,88,42},34,RGB(90,90,90),FW_NORMAL,true);garmin_text(clock,235,42,copilot_minutes()<0?"---":copilot_weekday(copilot_minutes()),{96,0,235,42},36,RGB(255,255,255),FW_BOLD,true);module(8,clock,235,42);
 if(home||gauges||camera||clockPage||calculatorPage||galleryPage)return;
 if(recenter){std::vector<uint8_t> button(150*34*4,0);copilot_round(button,150,34,{0,0,150,34},4,RGB(255,255,255),128);garmin_text(button,150,34,"re-center",{4,0,146,34},20,RGB(0,0,0),FW_NORMAL,true);module(13,button,150,34);}
 module(14,copilot_speed_limit(dark),78,104);module(11,copilot_real_speed(dark),78,43);
 if(!trip)return;
 const auto p=guidance_presentation(guidance);module(3,copilot_maneuver(p),132,68);module(5,copilot_road(p,guidance),384,80);
 std::vector<uint8_t> distance(148*40*4,0),eta(144*40*4,0);garmin_text(distance,148,40,copilot_distance(),{4,0,144,40},34,copilotOrange,FW_BOLD,true);garmin_text(eta,144,40,copilot_eta(),{2,0,142,40},30,copilotOrange,FW_BOLD,true);module(9,distance,148,40);module(10,eta,144,40);
}
static Placement copilot_menu_bounds(const OverlaySettings& config,double progress){auto p=config.items[2];p.y+=int(std::lround((544-p.y)*(1-progress)));return p;}
static Placement copilot_logo_bounds(){const auto& image=copilot_image("copilot_v9_logo.dds");if(!image.width||!image.height)return {0,0,0,0};const double scale=std::min(450./image.width,272./image.height);const int w=int(std::lround(image.width*scale)),h=int(std::lround(image.height*scale));return {(900-w)/2,(544-h)/2,w,h};}
// Slots use the original 1324x92 status artwork coordinates. Additional
// resident app entries extend right with the same fixed eight-pixel gap.
static Placement copilot_background_icon_bounds(const OverlaySettings& config,unsigned slot){
 const auto& top=config.items[0];return {top.x+int(std::lround(top.w*(19.+55.*slot)/1324)),top.y+(presentationImmersive.mode?presentationImmersive.top():0)+int(std::lround(top.h*18./92)),int(std::lround(top.w*47./1324)),int(std::lround(top.h*46./92))};
}
static void copilot_background_icons(std::vector<uint8_t>& pixels,const OverlaySettings& config,uint32_t apps){
 static constexpr struct {uint32_t flag;const char* file;} icons[]={{CopilotRunning,"navplushd_icon_copilot.dds"},{ClockTimerRunning,"deskclock/stat_notify_timer.dds"},{ClockStopwatchRunning,"deskclock/stat_notify_stopwatch.dds"},{ClockAlarmEnabled,"deskclock/stat_notify_alarm.dds"}};
 unsigned slot=0;for(const auto& icon:icons)if(apps&icon.flag){const auto& image=copilot_image(icon.file);overlay_blit(pixels,image.pixels,image.width,image.height,copilot_background_icon_bounds(config,slot++),900,544);}
}
static void copilot_interaction(std::vector<uint8_t>& pixels,const OverlaySettings& config,const PageVisual& page,const CopilotMenuVisual& menu){
 copilot_background_icons(pixels,config,page.backgroundApps);
 if(page.transition<0&&(page.page==DevicePage::Nav||page.page==DevicePage::Notrip))return; // Native target map under the outgoing gauge snapshot.
 if(page.transition){
  std::vector<uint8_t> home(900*544*4,10);for(size_t i=3;i<home.size();i+=4)home[i]=255;if(page.subpage){const auto source=copilot_app_layer(page.transition>0?page.fromPage:page.page,page.transition>0?page.fromGauge:page.gauge);overlay_blit(home,source,900,544,config.items[1],900,544);}else if(!copilot_forward_capture(page))copilot_home_artwork(home,config.items[1]);
  const auto elapsed=copilot_motion_elapsed(page.elapsed,page.transition>0);const int shift=int(std::lround(900*(page.transition>0?-copilot_ease(double(elapsed)/copilotSlideMs):copilot_ease(double(elapsed)/copilotSlideMs)-1)));
  for(int y=copilotAppTop;y<483;++y)for(int x=0;x<900;++x){auto* dst=pixels.data()+(y*900+x)*4;const int sx=x-shift;if(sx>=0&&sx<900)memcpy(dst,home.data()+(y*900+sx)*4,4);else{dst[0]=dst[1]=dst[2]=10;dst[3]=255;}}
  std::vector<uint8_t> edge(900*copilotMapTop*4,0);const auto& top=copilot_image("navplushd_top_5by3.dds");overlay_blit(edge,top.pixels,top.width,top.height,config.items[0],900,copilotMapTop);std::vector<uint8_t> tail(edge.begin()+900*copilotAppTop*4,edge.end());overlay_blit(pixels,tail,900,copilotMapTop-copilotAppTop,{0,copilotAppTop,900,copilotMapTop-copilotAppTop},900,544);
  if(page.transition>0&&elapsed>=copilotSlideMs){if(page.page==DevicePage::Gauges||page.page==DevicePage::Camera||page.page==DevicePage::Clock){std::vector<uint8_t> loading(900*544*4,10);for(size_t i=3;i<loading.size();i+=4)loading[i]=255;const auto& background=copilot_image(page.page==DevicePage::Clock?"navplushd_gradient_background.dds":page.page==DevicePage::Camera?"navplushd_simple_background.dds":"navplushd_gauges_background.dds");overlay_blit(loading,background.pixels,background.width,background.height,config.items[1],900,544);memcpy(pixels.data()+900*copilotAppTop*4,loading.data()+900*copilotAppTop*4,900*(483-copilotAppTop)*4);overlay_blit(pixels,tail,900,copilotMapTop-copilotAppTop,{0,copilotAppTop,900,copilotMapTop-copilotAppTop},900,544);}else if(page.copilotLoading){const auto& logo=copilot_image("copilot_v9_logo.dds");overlay_blit(pixels,logo.pixels,logo.width,logo.height,copilot_logo_bounds(),900,544);}}
  // The status image has transparent pixels; do not expose the live map
  // through them while the app body moves underneath it.
  for(int y=0;y<544;++y)if(y<copilotAppTop||y>=483)for(int x=0;x<900;++x){const auto at=(y*900+x)*4;const auto old=pixels[at+3];const unsigned alpha=old;for(int c=0;c<3;++c)pixels[at+c]=uint8_t((pixels[at+c]*alpha+10*(255-alpha)+127)/255);pixels[at+3]=255;}
  return;
 }
 // Queued SCS camera draws can outlive the window swap by one frame. The
 // completed Home must cover them, including transparent artwork padding.
 if(page.page==DevicePage::Home&&page.fromPage==DevicePage::Camera){for(size_t at=0;at<pixels.size();at+=4){const unsigned a=pixels[at+3];for(int c=0;c<3;++c)pixels[at+c]=uint8_t((pixels[at+c]*a+10*(255-a)+127)/255);pixels[at+3]=255;}}
 if(page.page==DevicePage::Camera&&page.cameraPopup){const auto& image=copilot_image("navplushd_camera_noavailable.dds");overlay_blit(pixels,image.pixels,image.width,image.height,copilot_camera_popup_bounds(),900,544);return;}
 if((page.page!=DevicePage::Nav&&page.page!=DevicePage::Notrip)||!menu.blocked())return;
 const auto undimmed=pixels;const std::vector<uint8_t> shade{0,0,0,128};overlay_blit(pixels,shade,1,1,{0,63,900,420},900,544);
 const auto& image=copilot_image("copilot_v9_drivingmenu.dds");overlay_blit(pixels,image.pixels,image.width,image.height,copilot_menu_bounds(config,menu.progress),900,544);
 // The Kenworth chrome remains above the modal shade and sliding sheet.
 for(unsigned slot:{0u,12u}){const auto& p=config.items[slot];for(int y=std::max(0,p.y);y<std::min(544,p.y+p.h);++y)for(int x=std::max(0,p.x);x<std::min(900,p.x+p.w);++x){const auto at=(size_t(y)*900+x)*4;memcpy(pixels.data()+at,undimmed.data()+at,4);}}
}
