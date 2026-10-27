#pragma once
static const GarminImage& international_image(const char* name){return page_image(std::string("copilot/international/")+name);}
static std::shared_ptr<const ClusterMovie> internationalMovie;
static bool international_assets_ready(){for(const auto* name:{"lt8520_home.dds","copilot_v9_top.dds","copilot_v9_zoom.dds","copilot_v9_belowfooter.dds","copilot_v9_notrip.dds","copilot_launch.dds"})if(international_image(name).pixels.empty())return false;return true;}
static bool international_movie_open(){if(!internationalMovie)internationalMovie=ClusterMovie::read(std::filesystem::u8path(g_assetRoot)/"material/copilot/international/international_t8520_intro.bga",internationalMovieWidth,internationalHeight,internationalFrames);return bool(internationalMovie);}
static void international_clock(std::vector<uint8_t>& pixels,const OverlaySettings& settings){
 const auto box=settings.items[8];std::vector<uint8_t> layer(241*64*4,0);const auto now=copilot_minutes();const auto clock=copilot_clock();const auto suffix=now<0?"":now%1440<720?"AM":"PM";const int width=tomtom_measure(clock,44);
 garmin_text(layer,241,64,clock,{std::max(0,169-width),0,171,63},44,RGB(255,255,255),FW_BOLD,false,false,false,L"Arial");garmin_text(layer,241,64,suffix,{178,16,241,61},30,RGB(255,255,255),FW_BOLD);overlay_blit(pixels,layer,241,64,box,internationalWidth,internationalHeight);
}
static void international_home(std::vector<uint8_t>& pixels,const OverlaySettings& settings){pixels.assign(size_t(internationalWidth)*internationalHeight*4,0);const auto& image=international_image("lt8520_home.dds");overlay_blit(pixels,image.pixels,image.width,image.height,settings.items[1],internationalWidth,internationalHeight);international_clock(pixels,settings);}
static void international_startup(std::vector<uint8_t>& pixels,int phase){
 pixels.assign(size_t(internationalWidth)*internationalHeight*4,10);for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;if(!international_movie_open())return;
 const unsigned frame=std::min(internationalFrames-1,unsigned(std::max(0,phase))*30/1000);static unsigned previous=~0u;static std::vector<uint8_t> decoded;if(frame!=previous){if(!internationalMovie->decode(frame,decoded))return;previous=frame;}
 for(unsigned y=0;y<internationalHeight;++y)memcpy(pixels.data()+size_t(y)*internationalWidth*4,decoded.data()+size_t(y)*internationalMovieWidth*4,internationalWidth*4);
}
static void render_international(std::vector<uint8_t>& pixels,const OverlaySettings& settings,const bettergps_navigation::Display& guidance=bettergps_navigation::Current()){
 const bool trip=g_tripPresent.load(),dark=presentation_dark();if(presentation_home(trip)){international_home(pixels,settings);return;}pixels.assign(size_t(internationalWidth)*internationalHeight*4,0);
 auto image=[&](unsigned item,const char* name){const auto& source=international_image(name);overlay_blit(pixels,source.pixels,source.width,source.height,settings.items[item],internationalWidth,internationalHeight);};auto module=[&](unsigned item,const std::vector<uint8_t>& source,unsigned w,unsigned h){overlay_blit(pixels,source,w,h,settings.items[item],internationalWidth,internationalHeight);};
 image(0,"copilot_v9_top.dds");image(4,"copilot_v9_zoom.dds");image(trip?6:7,trip?"copilot_v9_belowfooter.dds":"copilot_v9_notrip.dds");international_clock(pixels,settings);module(14,copilot_speed_limit(dark),78,104);module(11,copilot_real_speed(dark),78,43);if(!trip)return;
 const auto p=guidance_presentation(guidance);module(3,copilot_maneuver(p),132,68);module(5,copilot_road(p,guidance),384,80);std::vector<uint8_t> distance(148*40*4,0),eta(144*40*4,0);garmin_text(distance,148,40,copilot_distance(),{4,0,144,40},34,copilotOrange,FW_BOLD,true);garmin_text(eta,144,40,copilot_eta(),{2,0,142,40},30,copilotOrange,FW_BOLD,true);module(9,distance,148,40);module(10,eta,144,40);
}
static Placement international_app_bounds(const OverlaySettings& settings,double progress){
 const double t=1-std::pow(1-std::clamp(progress,0.,1.),3);const auto icon=settings.items[13];return {int(std::lround(icon.x*(1-t))),int(std::lround(icon.y*(1-t))),int(std::lround(icon.w+(internationalWidth-icon.w)*t)),int(std::lround(icon.h+(internationalHeight-icon.h)*t))};
}
static void international_transition(std::vector<uint8_t>& pixels,const OverlaySettings& settings,const PageVisual& page){
 if(page.transition<=0)return;const auto& launch=international_image("copilot_launch.dds");if(launch.pixels.empty())return;
 if(page.elapsed<pageOpenMs){international_home(pixels,settings);const double progress=double(page.elapsed)/pageOpenMs;phone_app_blit(pixels,launch,international_app_bounds(settings,progress),progress,internationalWidth,internationalHeight);return;}
 auto layer=launch.pixels;const unsigned alpha=unsigned(std::lround(255*(1-std::clamp((double(page.elapsed)-pageOpenMs-pageLogoHoldMs)/pageFadeMs,0.,1.))));for(size_t i=3;i<layer.size();i+=4)layer[i]=uint8_t(unsigned(layer[i])*alpha/255);overlay_blit(pixels,layer,launch.width,launch.height,{0,0,int(internationalWidth),int(internationalHeight)},internationalWidth,internationalHeight);
}
