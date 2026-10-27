#pragma once
// Native 16:10 CarPlay frame, with editable horizontal modules inside its map aperture.
static const std::vector<uint8_t>& horizontal_carplay_pixels(bool waze){auto& pixels=horizontalCarplayPixels[waze?1:0];return pixels.empty()?phone_module_pixels(horizontalCarplay[waze?1:0]):pixels;}
static void render_horizontal_static(std::vector<uint8_t>& pixels,const OverlaySettings& config,const bettergps_navigation::Display& display){
 const bool trip=g_tripPresent.load(),dark=presentation_dark(),waze=waze_style();const unsigned state=(dark?2:0)+(trip?0:1);
 pixels.assign(800*500*4,0);const auto* modules=waze?horizontalWaze:horizontalGoogle;
 auto paint=[&](const std::vector<uint8_t>& data,int w,int h,Placement out){overlay_blit(pixels,data,w,h,out,800,500);};
 auto local=[&](size_t item,double x,double y,double w,double h){const auto& p=config.items[item];double s=p.w/double((waze?horizontalWaze:horizontalGoogle)[item].width);return Placement{p.x+int(std::lround(x*s)),p.y+int(std::lround(y*s)),int(std::lround(w*s)),int(std::lround(h*s))};};
 auto image=[&](size_t item,unsigned resource,unsigned w,unsigned h){paint(phone_module_pixels(resource),w,h,config.items[item]);};
 for(size_t i=3;i<14;++i)if(modules[i].states&(1u<<state)){unsigned resource=modules[i].resource;if((i==4||i==5)&&navigationVoiceMuted)resource=horizontalExtras[(waze?2:0)+(i==5)];image(i,resource,modules[i].width,modules[i].height);}
 if(!waze&&!trip){auto item=dark?11:10;paint(phone_module_pixels(horizontalExtras[dark?5:4]),46,46,local(item,0,-250,46,46));}
 if(waze){paint(phone_module_pixels(dark?355:354),95,95,local(dark?5:4,50,160,70,70));if(!trip)paint(phone_module_pixels(dark?357:356),75,75,local(dark?11:10,0,80,75,75));}
 for(size_t i=0;i<3;++i)if(modules[i].states&(1u<<state))image(i,modules[i].resource,modules[i].width,modules[i].height);
 const size_t above=waze&&trip?1:dark?2:0;
 if(waze&&trip)paint(phone_module_pixels(horizontalGoogle[2].resource),205,35,local(above,530,0,205,35));
 static std::vector<uint8_t> clock;static std::string clockKey;const bool white=dark||(waze&&trip);auto ck=current_clock_text()+(white?"W":"B");if(ck!=clockKey||clock.empty()){render_clock_text_bgra(clock,192,64,current_clock_text());if(white)for(size_t i=0;i<clock.size();i+=4){clock[i]=255-clock[i];clock[i+1]=255-clock[i+1];clock[i+2]=255-clock[i+2];}clockKey=ck;}
 paint(clock,192,64,local(above,waze&&trip?533:3,1,76,25));
 static std::vector<uint8_t> guidance,speed,footer;static std::string guidanceKey,speedKey,footerKey;
 const std::string skin=std::to_string(unsigned(waze))+std::to_string(unsigned(dark));
 auto speedValue=current_speed_text()+current_speed_limit_text()+skin+(current_speed_is_over_limit()?"O":"N");
 if(speedValue!=speedKey||speed.empty()){if(waze){std::vector<uint8_t> full;render_waze_speed(full);speed.resize(92*98*4);for(unsigned y=0;y<98;++y)memcpy(speed.data()+y*92*4,full.data()+((y+510)*360+10)*4,92*4);}else render_speed_text_bgra(speed,192,352,current_speed_text());speedKey=speedValue;}
 paint(speed,waze?92:192,waze?98:352,config.items[overlayModuleCount]);
 if(trip){auto gk=display.instruction+display.text+display.status+display.currentRoad+display.targetRoad+display.destination+display.next+display.exit+std::to_string(shield_revision(bettergps_navigation::ShieldDirectory()))+skin;
  if(guidanceKey!=gk||guidance.empty()){if(waze){std::vector<uint8_t> full;render_waze_guidance(full,display);guidance.resize(360*128*4);for(unsigned y=0;y<128;++y)memcpy(guidance.data()+y*360*4,full.data()+(y+28)*360*4,360*4);}else render_guidance_text_bgra(guidance,960,480,display);guidanceKey=gk;}
  paint(guidance,waze?360:960,waze?128:480,local(3,waze?0:20,waze?8:13,waze?365:320,waze?128:160));
  const size_t below=dark?7:6;
  if(waze){auto key=current_time_text()+current_distance_text()+skin;
   if(key!=footerKey||footer.empty()){
    std::vector<uint8_t> full(360*740*4,0);const auto ink=dark?RGB(245,245,245):RGB(28,30,32);
    waze_text(full,current_arrival_text(false),{50,12,310,48},27,true,ink,false,FW_BOLD);
    auto distance=current_distance_text();distance=distance.substr(0,distance.find(" - "));
    waze_text(full,waze_time_text(),{56,45,171,70},18,true,ink);
    waze_text(full,distance,{189,45,304,70},18,true,ink);
    for(int y=55;y<63;++y)for(int x=176;x<184;++x){auto* d=full.data()+(y*360+x)*4;double a=std::clamp(3.1-std::hypot(x+.5-180,y+.5-59),0.0,1.0);d[0]=d[1]=d[2]=145;d[3]=uint8_t(a*255);}
    footer.resize(260*70*4);for(unsigned y=0;y<70;++y)memcpy(footer.data()+y*260*4,full.data()+(y*360+50)*4,260*4);footerKey=key;
   }paint(footer,260,70,local(below,220,20,260,70));}
  else{auto key=current_time_text()+current_distance_text()+skin;
   if(key!=footerKey||footer.empty()){
    std::vector<uint8_t> full(360*740*4,0);
    auto duration=waze_time_text();auto at=duration.find(" h ");if(at!=std::string::npos)duration.replace(at,3,"h ");
    waze_text(full,duration,{16,24,194,64},29,false,RGB(43,127,63),false,FW_MEDIUM);
    waze_text(full,current_distance_text(),{16,66,204,94},14,false,dark?RGB(200,200,200):RGB(75,75,75));
    footer.resize(320*110*4);for(unsigned y=0;y<110;++y)memcpy(footer.data()+y*320*4,full.data()+y*360*4,320*4);footerKey=key;
   }paint(footer,320,110,config.items[below]);}

 }

}

static void render_configured_horizontal(std::vector<uint8_t>& pixels,const OverlaySettings& config,size_t tag=0,const bettergps_navigation::Display& guidance=bettergps_navigation::Current(),bool carplay=true){
 struct Cached {std::string key;std::vector<uint8_t> pixels;};static std::array<Cached,34+managedDeviceCount> caches;auto& cache=caches[std::min(tag,caches.size()-1)];
 auto key=std::to_string(config.revision)+":"+std::to_string(waze_style())+std::to_string(presentation_dark())+std::to_string(g_tripPresent.load())+std::to_string(navigationVoiceMuted.load())+current_clock_text()+current_speed_text()+current_speed_limit_text()+std::to_string(current_speed_is_over_limit())+current_time_text()+current_distance_text()+guidance.instruction+guidance.text+guidance.status+guidance.currentRoad+guidance.targetRoad+guidance.destination+guidance.next+guidance.exit+std::to_string(shield_revision(bettergps_navigation::ShieldDirectory()));
 if(cache.key!=key||cache.pixels.empty()){render_horizontal_static(cache.pixels,config,guidance);cache.key=std::move(key);}pixels=cache.pixels;
 const bool trip=g_tripPresent.load(),dark=presentation_dark(),waze=waze_style();
 auto local=[&](size_t item,double x,double y,double w,double h){const auto& p=config.items[item];double s=p.w/double((waze?horizontalWaze:horizontalGoogle)[item].width);return Placement{p.x+int(std::lround(x*s)),p.y+int(std::lround(y*s)),int(std::lround(w*s)),int(std::lround(h*s))};};
 auto paint=[&](const std::vector<uint8_t>& data,int w,int h,Placement out){overlay_blit(pixels,data,w,h,out,800,500);};static std::vector<uint8_t> needle;
 if(g_haveCompassHeading.load()){render_compass_needle(needle,g_compassHeading.load());if(waze){size_t item=trip?(dark?9:8):(dark?11:10);paint(needle,36,36,local(item,19,trip?19:99,36,36));}else{size_t item=trip?(dark?5:4):(dark?11:10);paint(needle,36,36,local(item,trip?87:10,trip?77:10,36,36));}}
 if(carplay)paint(horizontal_carplay_pixels(waze),800,500,{0,0,800,500});
}
