#pragma once
// ICC5 uses the shared TomTom data, controls and navigation modules, on a
// wide canvas. All placements are also the inverse-touch source of truth.
static const GarminImage& icc5_image(const char* name){if(managed_vehicle()=="westernstar.57x"){if(!strcmp(name,"icc5_home.dds"))return tomtom_image("westernstar/westernstar_icc5_home.dds");if(!strcmp(name,"icc5_splash_runsmart.dds"))return tomtom_image("westernstar/icc5_splash_westernstar.dds");}return tomtom_image(std::string("cascadia/icc5/")+name);}
static std::string icc5_clock(){if(!g_haveGameTime)return "--:--";const auto t=copilot_minutes()%1440;char text[16];std::snprintf(text,sizeof(text),"%d:%02d",int(t/60),int(t%60));return text;}
static void icc5_blit(std::vector<uint8_t>& pixels,const char* name,Placement p,unsigned alpha=255){
 const auto& image=icc5_image(name);if(alpha==255)overlay_blit(pixels,image.pixels,image.width,image.height,p,icc5Width,icc5Height);
 else{auto layer=image.pixels;for(size_t i=3;i<layer.size();i+=4)layer[i]=uint8_t(unsigned(layer[i])*alpha/255);overlay_blit(pixels,layer,image.width,image.height,p,icc5Width,icc5Height);}
}
static void icc5_text(std::vector<uint8_t>& pixels,const std::string& text,Placement p,int size,COLORREF color,int weight=FW_NORMAL,bool right=false,bool center=false){
 std::vector<uint8_t> layer(size_t(p.w)*p.h*4,0);tomtom_text(layer,p.w,p.h,text,{0,0,p.w,p.h},size,color,weight,center,false,right);overlay_blit(pixels,layer,p.w,p.h,p,icc5Width,icc5Height);
}
static Placement icc5_temperature_place(const OverlaySettings& settings){auto p=settings.items[2];p.x-=int(std::lround(12*settings.scales[2]));p.y-=int(std::lround(10*settings.scales[2]));return p;}
static Placement icc5_home_clock_place(const OverlaySettings& settings){auto p=settings.items[1];p.x-=int(std::lround(4*settings.scales[1]));return p;}
static OverlaySettings icc5_navigation_size(OverlaySettings settings){for(unsigned i:{7u,8u}){auto& p=settings.items[i];const int w=int(std::lround(p.w*1.3)),h=int(std::lround(p.h*1.3));p.x-=(w-p.w)/2;if(i==8)p.y-=h-p.h;p.w=w;p.h=h;}return settings;}
static void icc5_overlay(std::vector<uint8_t>& pixels,const OverlaySettings& settings){
 icc5_blit(pixels,"icc5_overlay.dds",settings.items[4]);
 icc5_text(pixels,(navplusHaveTemperature?std::to_string(navplusTemperatureF.load()):"--")+"\xC2\xB0" "F",icc5_temperature_place(settings),34,RGB(238,241,247),FW_BOLD,false,true);
}
static void icc5_home(std::vector<uint8_t>& pixels,const OverlaySettings& settings){
 pixels.assign(size_t(icc5Width)*icc5Height*4,0);icc5_blit(pixels,"icc5_home.dds",settings.items[0]);icc5_overlay(pixels,settings);
 icc5_text(pixels,icc5_clock(),icc5_home_clock_place(settings),36,RGB(218,223,233),FW_NORMAL,false,true);
}
static bool icc5_assets_ready(){
 for(const char* f:{"icc5_home.dds","icc5_overlay.dds","icc5_splash_background.dds","icc5_splash_runsmart.dds","icc5_splash_detroit.dds","trip_revisisted.dds"})if(icc5_image(f).pixels.empty())return false;
 for(const char* f:{"left_tomtomttrucker.dds","icons/tomtom_player_icon.dds","icons/tomtom_finishline_white.dds","icons/tomtom_finishline_white_50transparent.dds"})if(tomtom_image(std::string("trucker6000/")+f).pixels.empty())return false;return true;
}
static unsigned icc5_logo_alpha(int phase,int start){const int local=phase-start;return local<0||local>=4000?0:local<1000?unsigned(local*255/1000):local<3000?255:unsigned((4000-local)*255/1000);}
static Placement icc5_logo_place(const GarminImage& image,bool detroit){const int height=detroit?200:320,width=image.height?int(std::lround(double(height)*image.width/image.height)):0;return {(int(icc5Width)-width)/2,(int(icc5Height)-height)/2,width,height};}
static void icc5_startup(std::vector<uint8_t>& pixels,const OverlaySettings& settings,int phase){
 pixels.assign(size_t(icc5Width)*icc5Height*4,0);for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;
 icc5_blit(pixels,"icc5_splash_background.dds",{0,0,int(icc5Width),int(icc5Height)},unsigned(std::clamp(phase,0,500)*255/500));
 if(phase>=750)icc5_overlay(pixels,settings);
 for(const auto& cue:std::array<std::pair<const char*,int>,2>{{{"icc5_splash_runsmart.dds",icc5RunsmartMs},{"icc5_splash_detroit.dds",icc5DetroitMs}}}){
  const unsigned alpha=icc5_logo_alpha(phase,cue.second);if(!alpha)continue;const auto& image=icc5_image(cue.first);if(!image.height)continue;
  icc5_blit(pixels,cue.first,icc5_logo_place(image,cue.second==icc5DetroitMs),alpha);
 }
}
static void icc5_trip_duration(std::vector<uint8_t>& layer,int w,int h){
 const auto minutes=tomtom_remaining_minutes();struct Run{std::string text;bool number;};const std::vector<Run> runs=minutes<0?std::vector<Run>{{"--",true},{" min",false}}:minutes<60?std::vector<Run>{{std::to_string(minutes),true},{" min",false}}:std::vector<Run>{{std::to_string(minutes/60),true},{"h ",false},{std::to_string(minutes%60),true},{"m",false}};
 int size=minutes>=0&&minutes<60?46:40,width=0;auto measure=[&]{int n=0;for(const auto& run:runs)n+=tomtom_measure(run.text,run.number?size:std::max(20,size*2/3),run.number?FW_BOLD:FW_NORMAL);return n;};while((width=measure())>131&&size>24)--size;
 int x=320-width;for(const auto& run:runs){const int font=run.number?size:std::max(20,size*2/3),weight=run.number?FW_BOLD:FW_NORMAL,rw=tomtom_measure(run.text,font,weight),bottom=run.number?98:95;tomtom_text(layer,w,h,run.text,{x,bottom-font-6,x+rw+1,bottom},font,RGB(255,255,255),weight);x+=rw;}
}
static void icc5_trip(std::vector<uint8_t>& pixels,const OverlaySettings& settings){
 constexpr int w=325,h=600;std::vector<uint8_t> layer(w*h*4,0);const auto& base=icc5_image("trip_revisisted.dds");overlay_blit(layer,base.pixels,base.width,base.height,{0,0,w,h},w,h);
 tomtom_text(layer,w,h,tomtom_eta(),{5,0,320,54},47,RGB(255,255,255),FW_BOLD,false,false,true);
 // Same ETA/duration/distance data as Mack; only their arrangement differs.
 auto distance=copilot_distance();const auto split=distance.find_last_of(' ');const auto number=split==std::string::npos?distance:distance.substr(0,split),unit=split==std::string::npos?"":distance.substr(split+1);
 int distanceSize=36;while(distanceSize>20&&tomtom_measure(number,distanceSize,FW_BOLD)+tomtom_measure(unit,22)+5>110)--distanceSize;const int digits=tomtom_measure(number,distanceSize,FW_BOLD),unitWidth=tomtom_measure(unit,22),x=185-digits-unitWidth-5;tomtom_text(layer,w,h,number,{x,98-distanceSize-6,185-unitWidth-5,98},distanceSize,RGB(255,255,255),FW_BOLD);tomtom_text(layer,w,h,unit,{181-unitWidth,67,183,95},22,RGB(255,255,255));
 icc5_trip_duration(layer,w,h);
 auto icon=[&](const char* file,Placement place,unsigned alpha=255){const auto& image=tomtom_image(std::string("trucker6000/icons/")+file);auto rgba=image.pixels;if(alpha!=255)for(size_t i=3;i<rgba.size();i+=4)rgba[i]=uint8_t(unsigned(rgba[i])*alpha/255);overlay_blit(layer,rgba,image.width,image.height,place,w,h);};
 constexpr int top=120,bottom=466;const double progress=tomtom_finish_progress();const bool isNear=g_haveNavDistance&&g_navDistanceM.load()<tomtomFinalMeters;const int finish=top+int(std::lround((1-progress)*(440-top)));
 auto column=[&](int start){for(int y=start;y<bottom;++y){const double t=std::clamp((y-start)/60.,0.,1.);copilot_round(layer,w,h,{27,y,39,y+1},0,RGB(0,0,0),unsigned(std::lround(85*t*t*(3-2*t))));}};
 icon("tomtom_finishline_white_50transparent.dds",{40,65,24,24});column(top);column(isNear?finish:top);
 if(isNear)icon("tomtom_finishline_white.dds",{18,finish-12,30,30},unsigned(std::lround(255*std::clamp((1-progress)*10,0.,1.))));
 icon("tomtom_player_icon.dds",{19,451,29,27});tomtom_text(layer,w,h,icc5_clock(),{135,548,320,595},43,RGB(255,255,255),FW_NORMAL,false,false,true);
 overlay_blit(pixels,layer,w,h,settings.items[9],icc5Width,icc5Height);
}
static void render_icc5(std::vector<uint8_t>& pixels,const OverlaySettings& settings,const bettergps_navigation::Display& guidance){
 if(presentation_home(g_tripPresent.load())){icc5_home(pixels,settings);return;}
 pixels.assign(size_t(icc5Width)*icc5Height*4,0);copilot_round(pixels,icc5Width,icc5Height,{0,0,148,600},0,RGB(0,0,0));copilot_round(pixels,icc5Width,icc5Height,{1448,0,1596,600},0,RGB(0,0,0));
 const auto& left=tomtom_image("trucker6000/left_tomtomttrucker.dds");overlay_blit(pixels,left.pixels,left.width,left.height,settings.items[5],icc5Width,icc5Height);tomtom_compass(pixels,settings);
 if(g_tripPresent){const auto sized=icc5_navigation_size(settings);tomtom_instructions(pixels,sized,guidance);tomtom_speed(pixels,sized,guidance.currentRoad);icc5_trip(pixels,settings);}
 icc5_overlay(pixels,settings);
}
