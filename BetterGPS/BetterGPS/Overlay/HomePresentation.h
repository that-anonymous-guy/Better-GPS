#pragma once
// CPU-only artwork and text. Decoding is cached and confined to preparation.
static const GarminImage& page_image(const std::string& relative){
 struct Entry {GarminImage image;uint64_t check=0;std::filesystem::file_time_type time{};};
 static std::map<std::string,Entry> images;auto& item=images[g_assetRoot+relative];const auto now=GetTickCount64();
 if(now<item.check)return item.image;item.check=now+1000;std::error_code ec;auto path=std::filesystem::u8path(g_assetRoot)/"material"/relative;
 const auto time=std::filesystem::last_write_time(path,ec);if(ec||time==item.time&&!item.image.pixels.empty())return item.image;
 HomeDecoder decoder;auto pixels=decoder.dds(path,&item.image.width,&item.image.height,4096);if(!pixels.empty()){item.image.pixels=std::move(pixels);item.time=time;}return item.image;
}
static Placement phone_home_app_bounds(const ManagedDevice&,bool waze){return Placement{waze?277:194,63,57,57};}
static PhoneHomeTarget phone_home_target(const ManagedDevice& d,bool waze){
 const auto p=phone_home_app_bounds(d,waze);float x=float(p.x),y=float(p.y),w=float(p.w),h=float(p.h);
 const auto fit=[&](float sw,float sh,float dw,float dh){const double s=std::min(double(dw)/sw,double(dh)/sh);const float fw=float(std::lround(sw*s)),fh=float(std::lround(sh*s));x=x*fw/sw+int(dw-fw)/2;y=y*fh/sh+int(dh-fh)/2;w*=fw/sw;h*=fh/sh;};
 if(d.horizontal)fit(360,740,800,500);const auto size=managed_canvas(d);if(managed_format(d)==2)fit(d.horizontal?800.f:360.f,d.horizontal?500.f:740.f,float(size.first),float(size.second));
 const auto zoom=[&](float v,unsigned length){if(d.globalScale<=1.0001)return v/length;const unsigned crop=std::max(1u,unsigned(std::lround(length/std::clamp(d.globalScale,1.,2.))));return (v-int(length-crop)/2)/crop;};return {zoom(x,size.first),zoom(y,size.second),zoom(x+w,size.first),zoom(y+h,size.second)};
}
static void render_phone_home(std::vector<uint8_t>& pixels,const ManagedDevice& d,const OverlaySettings& config){
 const auto layers=startupLayersScoped?startupLayersOverride:std::atomic_load(&homeLayers);const auto& fallback=startup_images()[0];
 pixels=layers&&layers->pixels[2]?*layers->pixels[2]:fallback;if(pixels.size()!=360*740*4)pixels.assign(360*740*4,18);
 const bool dark=presentation_dark();for(size_t p=0;p<pixels.size();p+=4){if(dark)for(int c=0;c<3;++c)pixels[p+c]=pixels[p+c]*2/3;pixels[p+3]=255;}
 const auto& overlay=page_image("ui/phone_home/icons/iphone_home_overlaymix.dds");
 if(overlay.pixels.size()==pixels.size())for(size_t p=0;p<pixels.size();p+=4)for(int c=0;c<3;++c){const unsigned a=overlay.pixels[p+3],color=pixels[p+c];pixels[p+c]=uint8_t((startup_overlay(color,overlay.pixels[p+c])*a+color*(255-a)+127)/255);}
 const auto& normal=page_image("ui/phone_home/icons/iphone_home_normalmix.dds");overlay_blit(pixels,normal.pixels,normal.width,normal.height,{0,0,360,740});
 std::vector<uint8_t> clock;render_clock_text_bgra(clock,192,64,current_clock_text());for(size_t p=0;p<clock.size();p+=4)for(int c=0;c<3;++c)clock[p+c]=255;
 overlay_blit(pixels,clock,192,64,attached_placement(config,dark?2:0,{14,2,80,26}));
}
static Placement phone_app_bounds(double progress,bool waze){
 progress=std::clamp(progress,0.,1.);const double t=1-std::pow(1-progress,3);
 const int x=waze?277:194;return {int(std::lround(x*(1-t))),int(std::lround(63*(1-t))),int(std::lround(57+(360-57)*t)),int(std::lround(57+(740-57)*t))};
}
static void managed_fit(std::vector<uint8_t>&,const std::vector<uint8_t>&,unsigned,unsigned,unsigned,unsigned,bool);
static void phone_app_blit(std::vector<uint8_t>& canvas,const GarminImage& image,Placement bounds,double progress,int width,int height){
 std::vector<uint8_t> tile(size_t(bounds.w)*bounds.h*4,0);overlay_blit(tile,image.pixels,image.width,image.height,{0,0,bounds.w,bounds.h},bounds.w,bounds.h);
 const double radius=14*(1-progress);if(radius>.1)for(int y=0;y<bounds.h;++y)for(int x=0;x<bounds.w;++x){double dx=std::max(0.,std::max(radius-x-.5,x+.5-(bounds.w-radius))),dy=std::max(0.,std::max(radius-y-.5,y+.5-(bounds.h-radius)));if(dx&&dy){const auto a=std::clamp(radius+.5-std::sqrt(dx*dx+dy*dy),0.,1.);tile[(size_t(y)*bounds.w+x)*4+3]=uint8_t(std::lround(tile[(size_t(y)*bounds.w+x)*4+3]*a));}}
 overlay_blit(canvas,tile,bounds.w,bounds.h,bounds,width,height);
}
static void render_phone_page(std::vector<uint8_t>& app,const ManagedDevice& d,const OverlaySettings& config,const PageVisual& page){
 if(page.page==DevicePage::Home){render_phone_home(app,d,config);return;}
 if(page.transition<=0)return;
 const auto& transition=page_image(std::string("ui/phone_home/transitions/")+(d.style==1?"waze":"googlemaps")+"_transition.dds");
 if(transition.pixels.empty())return;
 const int width=d.horizontal?800:360,height=d.horizontal?500:740;
 if(page.elapsed<pageOpenMs){render_phone_home(app,d,config);auto bounds=phone_app_bounds(double(page.elapsed)/pageOpenMs,d.style==1);if(d.horizontal){std::vector<uint8_t> fitted;managed_fit(fitted,app,360,740,width,height,true);app=std::move(fitted);bounds.x=int(bounds.x*width/360.);bounds.y=int(bounds.y*height/740.);bounds.w=int(bounds.w*width/360.);bounds.h=int(bounds.h*height/740.);}phone_app_blit(app,transition,bounds,double(page.elapsed)/pageOpenMs,width,height);return;}
 const auto fade=page.elapsed>pageOpenMs+pageLogoHoldMs?page.elapsed-pageOpenMs-pageLogoHoldMs:0;
 auto logo=transition.pixels;const unsigned alpha=unsigned(std::lround(255*(1-std::clamp(double(fade)/pageFadeMs,0.,1.))));
 for(size_t p=3;p<logo.size();p+=4)logo[p]=uint8_t((unsigned(logo[p])*alpha+127)/255);
 overlay_blit(app,logo,transition.width,transition.height,{0,0,width,height},width,height);
}
static void render_sony_home(std::vector<uint8_t>& pixels,unsigned width,unsigned height){
 const auto& image=page_image("sony/crown_victoria/sonygpshome.dds");std::vector<uint8_t> source(1200*800*4,0);
 overlay_blit(source,image.pixels,image.width,image.height,{0,0,1200,800},1200,800);
 const auto minutes=current_game_minutes();
 const int hour=minutes>=0?int(minutes%1440)/60:0;char clock[24];if(minutes<0)std::snprintf(clock,sizeof(clock),"--:--");else std::snprintf(clock,sizeof(clock),"%d:%02d",hour%12?hour%12:12,int(minutes%60));
 load_phone_fonts();
 garmin_text(source,1200,800,clock,{83,128,700,408},230,RGB(255,255,255),FW_LIGHT,false,false,false,L"Roboto Light");
 garmin_text(source,1200,800,minutes<0?"":hour<12?"AM":"PM",{644,310,714,367},40,RGB(255,255,255),FW_BOLD);
 const double scale=std::min(width/1200.,height/800.);const int w=int(std::lround(1200*scale)),h=int(std::lround(800*scale));pixels.assign(size_t(width)*height*4,0);for(size_t p=3;p<pixels.size();p+=4)pixels[p]=255;overlay_blit(pixels,source,1200,800,{(int(width)-w)/2,(int(height)-h)/2,w,h},width,height);
}
