#pragma once
// CPU worker only; the native map remains visible through the overlay.
static void render_icc5(std::vector<uint8_t>&,const OverlaySettings&,const bettergps_navigation::Display&);
static unsigned tomtom_width(const OverlaySettings& s){return s.navplus==9?icc5Width:tomtomWidth;}
static unsigned tomtom_height(const OverlaySettings& s){return s.navplus==9?icc5Height:tomtomHeight;}
static constexpr COLORREF tomtomCyan=RGB(1,234,249);
static constexpr const wchar_t* tomtomFont=L"Segoe UI";
static constexpr double tomtomFinalMeters=10*1609.344;
static void tomtom_text(std::vector<uint8_t>& pixels,int w,int h,const std::string& text,RECT rect,int size,COLORREF color,int weight=FW_NORMAL,bool center=false,bool outline=false,bool right=false){garmin_text(pixels,w,h,text,rect,size,color,weight,center,outline,right,tomtomFont);}
static const GarminImage& tomtom_image(const std::string& name){return page_image("tomtom/"+name);}
static std::string tomtom_home_time(){if(!g_haveGameTime)return "--:--";return current_clock_text()+(copilot_minutes()%1440<720?" AM":" PM");}
static void tomtom_home_clock(std::vector<uint8_t>& pixels,const OverlaySettings& settings){
 const auto p=settings.items[1];const bool volvo=settings.navplus==7;std::vector<uint8_t> clock(size_t(p.w)*p.h*4,0);
 const auto text=tomtom_home_time();const RECT rect=volvo?RECT{int(p.w*.32),3,int(p.w*.68),int(p.h*.65)}:RECT{int(p.w*.835),2,int(p.w*.995),int(p.h*.92)};
 tomtom_text(clock,p.w,p.h,text,rect,volvo?25:23,RGB(255,255,255),FW_NORMAL,volvo,false,!volvo);overlay_blit(pixels,clock,p.w,p.h,p,tomtomWidth,tomtomHeight);
}
static void tomtom_home(std::vector<uint8_t>& pixels,const OverlaySettings& settings,int reveal=3){
 pixels.assign(tomtomWidth*tomtomHeight*4,10);for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;
 if(settings.navplus==7){
  const char* files[]={"volvo2018_home_background.dds","volvo2018_home_top.dds","volvo2018_home_menu.dds","volvo2018_home_cases.dds"};
  const auto& bar=tomtom_image("volvo_infotainment/volvo_backtohome_left.dds");overlay_blit(pixels,bar.pixels,bar.width,bar.height,settings.items[4],tomtomWidth,tomtomHeight);
  for(int i=0;i<=std::min(3,reveal);++i){const auto& a=tomtom_image(std::string("volvo_infotainment/home_screen/")+files[i]);if(i==2){const unsigned start=124;if(a.height>start){std::vector<uint8_t> crop(a.pixels.begin()+size_t(start)*a.width*4,a.pixels.end());overlay_blit(pixels,crop,a.width,a.height-start,settings.items[i],tomtomWidth,tomtomHeight);}}else overlay_blit(pixels,a.pixels,a.width,a.height,settings.items[i],tomtomWidth,tomtomHeight);}
  if(reveal>=1)tomtom_home_clock(pixels,settings);return;
 }
 const char* files[]={"mack_home_background.dds","mack_home_top.dds","mack_home_menus.dds","mack_home_cases.dds"};
 for(int i=0;i<=std::min(3,reveal);++i){const auto& a=tomtom_image(std::string("mack_infotainment/home_screen/")+files[i]);overlay_blit(pixels,a.pixels,a.width,a.height,settings.items[i],tomtomWidth,tomtomHeight);}
 if(reveal>=1)tomtom_home_clock(pixels,settings);
}
static int tomtom_measure(const std::string& text,int size,int weight=FW_NORMAL){
 if(text.empty())return 0;auto dc=CreateCompatibleDC(nullptr);if(!dc)return int(text.size())*size/2;auto font=CreateFontW(-size,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,tomtomFont);auto old=SelectObject(dc,font);SIZE extent{};auto w=wide_text(text);GetTextExtentPoint32W(dc,w.c_str(),int(w.size()),&extent);SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);return extent.cx;
}
struct TomtomRoadRun {std::string text;const ShieldBitmap* shield=nullptr;int width=0;};
static std::vector<TomtomRoadRun> tomtom_road_runs(const std::string& road,int font){
 std::vector<TomtomRoadRun> runs;for(const auto& part:road_label_parts(clean_guidance_text(road))){const auto* shield=part.numbered?&copilot_shield(part.shield.identity,bettergps_navigation::ShieldDirectory()):nullptr;if(shield&&!shield->bgra.empty()){runs.push_back({"",shield,font+5});if(!part.shield.beside.empty())runs.push_back({part.shield.beside,nullptr,tomtom_measure(part.shield.beside,font)});}else if(!part.text.empty())runs.push_back({part.text,nullptr,tomtom_measure(part.text,font)});}return runs;
}
static int tomtom_runs_width(const std::vector<TomtomRoadRun>& runs){int width=0;for(const auto& run:runs)width+=run.width+7;return width?width-7:0;}
static void tomtom_road(std::vector<uint8_t>& pixels,int w,int h,const std::vector<TomtomRoadRun>& runs,int x,int y,int font,bool outline=false){
 for(const auto& run:runs){if(run.shield)paint_route_shield(pixels,w,h,*run.shield,x,y+(h-y-font-5)/2,font+5);else tomtom_text(pixels,w,h,run.text,{x,y,x+run.width+1,h},font,RGB(255,255,255),FW_NORMAL,false,outline);x+=run.width+7;}
}
static void tomtom_arrow(std::vector<uint8_t>& pixels,int w,int h,const std::string& action,Placement box){
 const auto icon=maneuver_icon(action);if(!icon.alpha)return;std::vector<uint8_t> arrow(size_t(icon.size)*icon.size*4,0);for(size_t i=0;i<size_t(icon.size)*icon.size;++i){arrow[i*4]=249;arrow[i*4+1]=234;arrow[i*4+2]=1;arrow[i*4+3]=icon.alpha[i];}overlay_blit(pixels,arrow,icon.size,icon.size,box,w,h);
}
static void tomtom_instructions(std::vector<uint8_t>& pixels,const OverlaySettings& settings,const bettergps_navigation::Display& guidance){
 const auto p=guidance_presentation(guidance);const int maxWidth=570,height=70;auto road=p.road;auto distance=p.distance;const auto suffix=distance.find(" \xC2\xB7");if(suffix!=std::string::npos)distance.resize(suffix);
 int font=29;auto runs=tomtom_road_runs(road,font);for(;font>16&&tomtom_runs_width(runs)>maxWidth-186;--font)runs=tomtom_road_runs(road,font-1);
 const int roadWidth=std::min(maxWidth-186,tomtom_runs_width(runs)),width=std::clamp(175+roadWidth,210,maxWidth);std::vector<uint8_t> layer(width*height*4,0);copilot_round(layer,width,height,{0,0,width,height},35,RGB(0,0,0),128);
 tomtom_arrow(layer,width,height,p.action,{13,13,42,44});
 const auto unit=distance.find_last_of(' ');if(unit!=std::string::npos){tomtom_text(layer,width,height,distance.substr(0,unit),{63,5,124,65},38,RGB(255,255,255),FW_BOLD,true);tomtom_text(layer,width,height,distance.substr(unit+1),{126,24,152,63},21,RGB(255,255,255));}else tomtom_text(layer,width,height,distance,{61,7,153,64},26,RGB(255,255,255),FW_BOLD,true);
 if(roadWidth){tomtom_road(layer,width,height,runs,165,0,font);copilot_round(layer,width,height,{165,54,width-12,57},0,tomtomCyan);}
 else if(!p.message.empty())tomtom_text(layer,width,height,p.message,{160,5,width-8,65},22,RGB(255,255,255));
 auto place=settings.items[7];const double scale=double(place.w)/maxWidth;place.x+=int(std::lround((maxWidth-width)*scale/2));place.w=int(std::lround(width*scale));overlay_blit(pixels,layer,width,height,place,tomtom_width(settings),tomtom_height(settings));
}
static void tomtom_speed(std::vector<uint8_t>& pixels,const OverlaySettings& settings,const std::string& road){
 constexpr int maxWidth=570,h=52;int font=28;auto runs=tomtom_road_runs(road,font);for(;font>16&&tomtom_runs_width(runs)>maxWidth-150;--font)runs=tomtom_road_runs(road,font-1);
 const int width=std::clamp(150+tomtom_runs_width(runs),150,maxWidth);std::vector<uint8_t> layer(width*h*4,0);copilot_round(layer,width,h,{0,0,width,h},8,RGB(0,0,0));
 tomtom_text(layer,width,h,current_speed_text(),{61,0,107,h},39,RGB(255,255,255),FW_BOLD,false,true);tomtom_text(layer,width,h,"mph",{106,12,145,h},18,RGB(255,255,255),FW_NORMAL,false,true);tomtom_road(layer,width,h,runs,151,0,font,true);
 for(size_t i=3;i<layer.size();i+=4)layer[i]=uint8_t((unsigned(layer[i])+1)/2);
 // The speed-limit sign alone stays fully opaque.
 copilot_round(layer,width,h,{0,0,53,52},7,RGB(255,255,255));copilot_round(layer,width,h,{2,2,51,50},6,RGB(0,0,0));copilot_round(layer,width,h,{5,5,48,47},4,RGB(255,255,255));tomtom_text(layer,width,h,"LIMIT",{5,5,48,19},13,RGB(0,0,0),FW_BOLD,true);tomtom_text(layer,width,h,current_speed_limit_text(),{5,17,48,48},31,RGB(0,0,0),FW_BOLD,true);
 auto place=settings.items[8];const double scale=double(place.w)/maxWidth;place.x+=int(std::lround((maxWidth-width)*scale/2));place.w=int(std::lround(width*scale));overlay_blit(pixels,layer,width,h,place,tomtom_width(settings),tomtom_height(settings));
}
static int64_t tomtom_remaining_minutes(){if(!g_haveNavTime)return -1;const double seconds=g_navTimeSec.load();return !std::isfinite(seconds)||seconds<0?-1:int64_t(std::ceil(seconds/60));}
static std::string tomtom_remaining_time(){const auto minutes=tomtom_remaining_minutes();if(minutes<0)return "-- min";return minutes<60?std::to_string(minutes)+" min":std::to_string(minutes/60)+"h "+std::to_string(minutes%60)+"m";}
static int tomtom_duration_size(int64_t minutes){return minutes>=0&&minutes<60?42:32;}
static void tomtom_duration(std::vector<uint8_t>& pixels,int w,int h){
 const auto minutes=tomtom_remaining_minutes();if(minutes<0){tomtom_text(pixels,w,h,"-- min",{23,43,w-4,87},30,RGB(255,255,255),FW_BOLD,false,false,true);return;}
 struct Run{std::string text;bool number;};std::vector<Run> runs=minutes<60?std::vector<Run>{{std::to_string(minutes),true},{" min",false}}:std::vector<Run>{{std::to_string(minutes/60),true},{"h ",false},{std::to_string(minutes%60),true},{"m",false}};
 int size=tomtom_duration_size(minutes),width=0;auto measure=[&]{int n=0;for(const auto& run:runs)n+=tomtom_measure(run.text,run.number?size:std::max(15,size*2/3),run.number?FW_BOLD:FW_NORMAL);return n;};while((width=measure())>w-27&&size>18)--size;
 int x=w-4-width;for(const auto& run:runs){const int font=run.number?size:std::max(15,size*2/3),weight=run.number?FW_BOLD:FW_NORMAL,rw=tomtom_measure(run.text,font,weight);tomtom_text(pixels,w,h,run.text,{x,43+(size-font),x+rw+1,87},font,RGB(255,255,255),weight);x+=rw;}
}
static std::string tomtom_eta(){if(!g_haveGameTime||!g_haveNavTime)return "--:--";const double seconds=g_navTimeSec.load();if(!std::isfinite(seconds)||seconds<0)return "--:--";const int64_t now=copilot_minutes(),eta=now+int64_t(std::ceil(seconds/60));char text[16];std::snprintf(text,sizeof(text),"%02d:%02d",int(eta%1440)/60,int(eta%60));return std::string(eta/1440!=now/1440?copilot_weekday(eta)+" ":"")+text;}
static double tomtom_finish_progress(){if(!g_haveNavDistance)return 1.;const double distance=g_navDistanceM.load();return std::isfinite(distance)?std::clamp(distance/tomtomFinalMeters,0.,1.):1.;}
static void tomtom_column(std::vector<uint8_t>& pixels,int w,int h,int top,int bottom){for(int y=top;y<bottom;++y){const double t=std::clamp((y-top)/60.,0.,1.);copilot_round(pixels,w,h,{13,y,25,y+1},0,RGB(0,0,0),unsigned(std::lround(85*t*t*(3-2*t))));}}
static void tomtom_trip(std::vector<uint8_t>& pixels,const OverlaySettings& settings){
 constexpr int w=117,h=544;std::vector<uint8_t> layer(w*h*4,0);const auto& base=tomtom_image("trucker6000/trip_tomtomttrucker.dds");overlay_blit(layer,base.pixels,base.width,base.height,{0,0,w,h},w,h);
 tomtom_text(layer,w,h,tomtom_eta(),{3,0,113,47},43,RGB(255,255,255),FW_BOLD,false,false,true);
 tomtom_duration(layer,w,h);
 auto icon=[&](const char* file,Placement p,unsigned opacity=255){const auto& image=tomtom_image(std::string("trucker6000/icons/")+file);if(opacity==255)overlay_blit(layer,image.pixels,image.width,image.height,p,w,h);else{auto faded=image.pixels;for(size_t i=3;i<faded.size();i+=4)faded[i]=uint8_t(unsigned(faded[i])*opacity/255);overlay_blit(layer,faded,image.width,image.height,p,w,h);}};
 icon("tomtom_finishline_white_50transparent.dds",{3,57,18,18});
 const double progress=tomtom_finish_progress();const int top=100,bottom=511,finish=top+int(std::lround((1-progress)*(478-top)));const bool isNear=g_haveNavDistance&&g_navDistanceM.load()<tomtomFinalMeters;const int front=isNear?finish:top;
 // Two 33%-opaque columns. The rear one never moves; the front fades at its top.
 tomtom_column(layer,w,h,top,bottom);tomtom_column(layer,w,h,front,bottom);
 if(isNear)icon("tomtom_finishline_white.dds",{4,finish-12,30,30},unsigned(std::lround(255*std::clamp((1-progress)*10,0.,1.))));
 icon("tomtom_player_icon.dds",{5,499,29,26});
 auto distance=copilot_distance();const auto split=distance.find_last_of(' ');if(split!=std::string::npos){tomtom_text(layer,w,h,distance.substr(0,split),{34,485,113,528},40,RGB(255,255,255),FW_BOLD,false,false,true);tomtom_text(layer,w,h,distance.substr(split+1),{76,519,113,544},24,RGB(255,255,255),FW_NORMAL,false,false,true);}else tomtom_text(layer,w,h,distance,{34,490,113,540},36,RGB(255,255,255),FW_BOLD,false,false,true);
 overlay_blit(pixels,layer,w,h,settings.items[9],tomtomWidth,tomtomHeight);
}
static void tomtom_compass(std::vector<uint8_t>& pixels,const OverlaySettings& settings){
 constexpr int w=51,h=74;std::vector<uint8_t> layer(w*h*4,0);const auto& image=tomtom_image("trucker6000/icons/tomtom_player_icon.dds");
 if(!image.pixels.empty()){std::vector<uint8_t> rotated(image.pixels.size(),0);const double turn=g_haveCompassHeading?compass_turn(g_compassHeading.load()):0.,angle=-turn*6.2831853071795864769,c=std::cos(angle),s=std::sin(angle);const double cx=(image.width-1)/2.,cy=(image.height-1)/2.;for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){const double dx=x-cx,dy=y-cy;const int sx=int(std::lround(cx+c*dx+s*dy)),sy=int(std::lround(cy-s*dx+c*dy));if(sx>=0&&sy>=0&&sx<int(image.width)&&sy<int(image.height))memcpy(rotated.data()+(size_t(y)*image.width+x)*4,image.pixels.data()+(size_t(sy)*image.width+sx)*4,4);}overlay_blit(layer,rotated,image.width,image.height,{11,10,29,27},w,h);}
 tomtom_text(layer,w,h,g_haveCompassHeading?navplus_cardinal(g_compassHeading.load()):"--",{2,35,49,61},23,RGB(255,255,255),FW_BOLD,true);overlay_blit(pixels,layer,w,h,settings.items[6],tomtom_width(settings),tomtom_height(settings));
}
static void render_tomtom(std::vector<uint8_t>& pixels,const OverlaySettings& settings,const bettergps_navigation::Display& guidance=bettergps_navigation::Current()){
 if(settings.navplus==9){render_icc5(pixels,settings,guidance);return;}
 if(presentation_home(g_tripPresent.load())){tomtom_home(pixels,settings);return;}pixels.assign(tomtomWidth*tomtomHeight*4,0);
 const auto& bar=tomtom_image(settings.navplus==7?"volvo_infotainment/volvo_backtohome_left.dds":"mack_infotainment/mack_backtohome_left.dds");overlay_blit(pixels,bar.pixels,bar.width,bar.height,settings.items[4],tomtomWidth,tomtomHeight);const auto& left=tomtom_image("trucker6000/left_tomtomttrucker.dds");overlay_blit(pixels,left.pixels,left.width,left.height,settings.items[5],tomtomWidth,tomtomHeight);tomtom_compass(pixels,settings);
 if(!g_tripPresent)return;tomtom_instructions(pixels,settings,guidance);tomtom_speed(pixels,settings,guidance.currentRoad);tomtom_trip(pixels,settings);
}
