#pragma once
// Prepared on the existing CPU worker. No new timer, decoder or render thread.
static const GarminImage& sygic_image(const std::string& name){return page_image("sygic/"+name);}
static void sygic_text(std::vector<uint8_t>& pixels,int w,int h,const std::string& text,RECT r,int size,COLORREF ink,int weight=FW_NORMAL,bool center=false){garmin_text(pixels,w,h,text,r,size,ink,weight,center,false,false,L"Segoe UI");}
static void sygic_paint(std::vector<uint8_t>& pixels,const std::string& name,Placement p){const auto& a=sygic_image(name);overlay_blit(pixels,a.pixels,a.width,a.height,p,sygicWidth,sygicHeight);}
static void sygic_clock(std::vector<uint8_t>& pixels,const OverlaySettings& settings){
 const auto p=settings.items[10];std::vector<uint8_t> layer(size_t(p.w)*p.h*4,0);
 sygic_text(layer,p.w,p.h,copilot_clock()+(copilot_minutes()%1440<720?" AM":" PM"),{0,0,p.w,p.h},25,RGB(255,255,255),FW_NORMAL,true);
 overlay_blit(pixels,layer,p.w,p.h,p,sygicWidth,sygicHeight);
}
static void sygic_home(std::vector<uint8_t>& pixels,const OverlaySettings& settings,int reveal=3){
 pixels.assign(sygicWidth*sygicHeight*4,10);for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;
 sygic_paint(pixels,"vnl2025/vnl2025_background.dds",settings.items[0]);
 if(reveal>=1){sygic_paint(pixels,"vnl2025/vnl2025_bottom.dds",settings.items[1]);sygic_clock(pixels,settings);}
 if(reveal>=2)sygic_paint(pixels,"vnl2025/vnl2025_menu.dds",settings.items[2]);
 if(reveal>=3)sygic_paint(pixels,"vnl2025/vnl2025_cases.dds",settings.items[3]);
}
struct SygicTripProgress {
 uint64_t epoch=0,session=0;bool active=false;double total=0,progress=0;std::string destination;
 void sample(bool trip,bool valid,double remaining,uint64_t nextEpoch,uint64_t nextSession,const std::string& nextDestination){
  if(!trip){active=false;total=progress=0;destination.clear();return;}
  if(!valid||!std::isfinite(remaining)||remaining<0)return;
  const bool changed=active&&!destination.empty()&&!nextDestination.empty()&&destination!=nextDestination;
  if(!active||epoch!=nextEpoch||session!=nextSession||changed){total=remaining;progress=0;active=true;epoch=nextEpoch;session=nextSession;destination=nextDestination;}
  if(destination.empty())destination=nextDestination;
  // A recalculation may lengthen the route: extend its total instead of resetting
  // on each maneuver, Home visit, pause, ignition cycle or day/night switch.
  total=std::max(total,remaining);progress=total>0?std::clamp(1-remaining/total,0.,1.):1.;
 }
};
static SygicTripProgress sygicTrip;
static void sygic_trip_sample(){if(managed_vehicle()=="volvo.vnl2025")sygicTrip.sample(g_tripPresent.load(),g_haveNavDistance.load(),g_navDistanceM.load(),vehicleStart.epoch.load(),phoneSessionSerial.load(),bettergps_navigation::Current().destination);}
struct SygicRun {std::string text;int weight;};
static void sygic_runs(std::vector<uint8_t>& pixels,int w,int h,const std::vector<SygicRun>& runs,int top,int bottom,int size,COLORREF ink){
 int width=0;for(const auto& r:runs)width+=tomtom_measure(r.text,size,r.weight);
 for(;width>w-150&&size>16;--size){width=0;for(const auto& r:runs)width+=tomtom_measure(r.text,size-1,r.weight);}
 int x=(w-width)/2;for(const auto& r:runs){const int n=tomtom_measure(r.text,size,r.weight);sygic_text(pixels,w,h,r.text,{x,top,x+n+2,bottom},size,ink,r.weight);x+=n;}
}
static void sygic_footer(std::vector<uint8_t>& pixels,const OverlaySettings& settings,bool dark){
 constexpr int w=638,h=157;const auto& a=sygic_image(dark?"nav/sygic_nav_footer_dark.dds":"nav/sygic_nav_footer_light.dds");std::vector<uint8_t> layer=a.pixels;
 if(layer.size()!=w*h*4)return;
 const auto ink=dark?RGB(255,255,255):RGB(25,25,27),durationInk=dark?ink:RGB(66,113,18);const auto minutes=tomtom_remaining_minutes();
 std::vector<SygicRun> duration=minutes<0?std::vector<SygicRun>{{"--",FW_BOLD},{" min",FW_NORMAL}}:minutes<60?std::vector<SygicRun>{{std::to_string(minutes),FW_BOLD},{" min",FW_NORMAL}}:std::vector<SygicRun>{{std::to_string(minutes/60),FW_BOLD},{" h ",FW_NORMAL},{std::to_string(minutes%60),FW_BOLD},{" min",FW_NORMAL}};
 sygic_runs(layer,w,h,duration,24,75,minutes>=60?42:46,durationInk);
 const auto distance=copilot_distance();const auto split=distance.find_last_of(' ');const auto eta=garmin_arrival();
 sygic_runs(layer,w,h,{{split==std::string::npos?distance:distance.substr(0,split),FW_BOLD},{split==std::string::npos?"":" "+distance.substr(split+1),FW_NORMAL},{"  \xC2\xB7  ",FW_NORMAL},{eta.time,FW_BOLD},{" "+eta.period,FW_NORMAL}},77,124,29,ink);
 constexpr int left=18,right=619,y=145;const int player=left+int(std::lround((right-left)*sygicTrip.progress));
 copilot_round(layer,w,h,{left-4,y-4,right+4,y+4},4,RGB(167,163,170));
 copilot_round(layer,w,h,{player-4,y-4,right+4,y+4},4,RGB(147,107,245));
 const auto& arrival=sygic_image(dark?"nav/icons/sygic_arrival_dark.dds":"nav/icons/sygic_arrival_light.dds");
 const auto& marker=sygic_image("nav/icons/sygic_player.dds");
 overlay_blit(layer,arrival.pixels,arrival.width,arrival.height,{right-11,y-11,23,23},w,h);
 overlay_blit(layer,marker.pixels,marker.width,marker.height,{player-10,y-10,20,21},w,h);
 overlay_blit(pixels,layer,w,h,settings.items[7],sygicWidth,sygicHeight);
}
static void sygic_arrow(std::vector<uint8_t>& pixels,int w,int h,const std::string& action,Placement p){
 const auto icon=maneuver_icon(action);if(!icon.alpha)return;std::vector<uint8_t> image(size_t(icon.size)*icon.size*4,255);for(size_t i=0;i<size_t(icon.size)*icon.size;++i)image[i*4+3]=icon.alpha[i];overlay_blit(pixels,image,icon.size,icon.size,p,w,h);
}
static std::string sygic_road(const std::string& road){
 auto text=clean_guidance_text(road);static const std::regex interstate(R"(\bI\s*[- ]\s*([0-9]+))",std::regex::icase),us(R"(\bUS\s*[- ]\s*([0-9]+))",std::regex::icase);
 return std::regex_replace(std::regex_replace(text,interstate,"Interstate $1"),us,"US Route $1");
}
static void sygic_instructions(std::vector<uint8_t>& pixels,const OverlaySettings& settings,const bettergps_navigation::Display& state){
 constexpr int w=638,h=190;const auto& a=sygic_image("nav/sygic_nav_instructions.dds");auto layer=a.pixels;if(layer.size()!=w*h*4)return;
 const auto p=guidance_presentation(state);sygic_arrow(layer,w,h,p.action,{24,48,101,115});
 auto distance=p.distance;const auto suffix=distance.find(" \xC2\xB7");if(suffix!=std::string::npos)distance.resize(suffix);
 const auto split=distance.find_last_of(' ');const auto number=split==std::string::npos?distance:distance.substr(0,split),unit=split==std::string::npos?"":distance.substr(split+1);
 sygic_text(layer,w,h,number,{142,4,365,90},64,RGB(255,255,255));const int n=tomtom_measure(number,64);sygic_text(layer,w,h,unit,{145+n,23,466,88},31,RGB(255,255,255));
 auto road=sygic_road(p.road);if(road.empty())road=p.message;int font=31;while(font>20&&tomtom_measure(road,font,FW_BOLD)>730)--font;
 // Keep two readable lines rather than squeezing long road names into one.
 std::string first=road,second;const int max=400;while(tomtom_measure(first,font,FW_BOLD)>max){auto at=first.find_last_of(' ');if(at==std::string::npos)break;second=first.substr(at+1)+(second.empty()?"":" "+second);first.resize(at);}
 if(tomtom_measure(first,font,FW_BOLD)>max)while(first.size()>3&&tomtom_measure(first+"...",font,FW_BOLD)>max)first.pop_back();
 if(tomtom_measure(second,font,FW_BOLD)>max){while(second.size()>3&&tomtom_measure(second+"...",font,FW_BOLD)>max)second.pop_back();second+="...";}
 sygic_text(layer,w,h,first,{143,98,548,142},font,RGB(255,255,255),FW_BOLD);sygic_text(layer,w,h,second,{143,139,548,183},font,RGB(255,255,255),FW_BOLD);
 std::string next=state.next.empty()?p.next:state.next;if(!next.empty()){const auto end=next.find_first_of(" |;[");if(end!=std::string::npos)next.resize(end);sygic_arrow(layer,w,h,next,{573,115,51,61});}
 int x=w-14;auto parts=road_label_parts(p.road);for(auto it=parts.rbegin();it!=parts.rend();++it)if(it->numbered){const auto& shield=load_route_shield(it->shield.identity,bettergps_navigation::ShieldDirectory());if(shield.bgra.empty())continue;const int sh=32,sw=int(std::lround(double(sh)*shield.width/shield.height));x-=sw;if(x<395)break;paint_route_shield(layer,w,h,shield,x,24,sh);x-=7;}
 overlay_blit(pixels,layer,w,h,settings.items[6],sygicWidth,sygicHeight);
}
static void sygic_speed(std::vector<uint8_t>& pixels,const OverlaySettings& settings,bool dark){
 constexpr int w=100,h=100;std::vector<uint8_t> layer(w*h*4,0);const auto ink=dark?RGB(255,255,255):RGB(0,0,0);copilot_round(layer,w,h,{0,0,w,h},50,dark?RGB(0,0,0):RGB(255,255,255));
 sygic_text(layer,w,h,current_speed_text(),{0,2,w,63},50,ink,FW_BOLD,true);sygic_text(layer,w,h,"mph",{0,63,w,91},24,ink,FW_NORMAL,true);overlay_blit(pixels,layer,w,h,settings.items[8],sygicWidth,sygicHeight);
 layer.assign(100*118*4,0);copilot_round(layer,100,118,{0,0,100,118},8,RGB(0,0,0));copilot_round(layer,100,118,{5,5,95,113},6,RGB(255,255,255));copilot_round(layer,100,118,{8,8,92,110},5,RGB(0,0,0));copilot_round(layer,100,118,{10,10,90,108},4,RGB(255,255,255));
 sygic_text(layer,100,118,"LIMIT",{9,12,91,49},27,RGB(0,0,0),FW_BOLD,true);sygic_text(layer,100,118,current_speed_limit_text(),{9,45,91,109},57,RGB(0,0,0),FW_BOLD,true);overlay_blit(pixels,layer,100,118,settings.items[9],sygicWidth,sygicHeight);
}
static void render_sygic(std::vector<uint8_t>& pixels,const OverlaySettings& settings,const bettergps_navigation::Display& state=bettergps_navigation::Current()){
 if(presentation_home(g_tripPresent.load())){sygic_home(pixels,settings);return;}
 pixels.assign(sygicWidth*sygicHeight*4,0);const bool dark=presentation_dark();
 sygic_paint(pixels,"vnl2025/vnl2025_bottom_nav.dds",settings.items[1]);sygic_clock(pixels,settings);
 sygic_paint(pixels,dark?"sygic_zoom_dark.dds":"sygic_zoom_light.dds",settings.items[5]);
 if(g_tripPresent.load()){sygic_instructions(pixels,settings,state);sygic_footer(pixels,settings,dark);sygic_speed(pixels,settings,dark);}
 else sygic_paint(pixels,dark?"notrip/sygic_notrip_search_dark.dds":"notrip/sygic_notrip_search_light.dds",settings.items[4]);
}
