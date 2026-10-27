#pragma once
#include "GalleryRuntime.h"
static Placement gallery_cell(unsigned i){return {18+int(i%5)*174,120+int(i/5)*176,168,168};}
static void gallery_icon(std::vector<uint8_t>& pixels,const char* file,Placement bounds){const auto& icon=copilot_image(std::string("gallery/")+file);overlay_blit(pixels,icon.pixels,icon.width,icon.height,bounds,900,544);}
static void gallery_photo(std::vector<uint8_t>& pixels,const bettergps_gallery::Picture& image,Placement bounds){if(!image||!image->width||!image->height)return;const int top=presentationImmersive.mode?0:63,height=presentationImmersive.mode?544:420;std::vector<uint8_t> body(900*height*4,0);overlay_blit(body,image->pixels,image->width,image->height,{bounds.x,bounds.y-top,bounds.w,bounds.h},900,height);overlay_blit(pixels,body,900,height,{0,top,900,height},900,544);}
static void gallery_grid(std::vector<uint8_t>& pixels,const bettergps_gallery::View& v){
 DeskClockCanvas c{&pixels,{}};c.fill({0,63,900,420},RGB(25,25,25));
 if(v.items->empty()){c.text(v.loaded?"No Steam screenshots for ATS":"Loading screenshots...",{70,225,760,46},29,FW_NORMAL);if(v.loaded)c.text("Your Steam screenshots will appear here.",{70,277,760,31},19,FW_NORMAL,RGB(170,170,170));return;}
 for(unsigned i=0;i<10&&v.first+int(i)<int(v.items->size());++i){const auto r=gallery_cell(i);c.fill(r,RGB(43,43,43));const auto& image=v.thumbs[i];if(!image)continue;const double scale=std::max(double(r.w)/image->width,double(r.h)/image->height);const int w=int(std::lround(image->width*scale)),h=int(std::lround(image->height*scale));std::vector<uint8_t> tile(size_t(r.w)*r.h*4,0);overlay_blit(tile,image->pixels,image->width,image->height,{(r.w-w)/2,(r.h-h)/2,w,h},r.w,r.h);overlay_blit(pixels,tile,r.w,r.h,r,900,544);}
}
static Placement gallery_photo_bounds(const bettergps_gallery::View& v){
 if(!v.image||!v.image->width||!v.image->height)return {0,63,900,420};const auto& im=*v.image;const int top=(v.fullscreen||presentationImmersive.mode)?0:63,height=(v.fullscreen||presentationImmersive.mode)?544:420;const double scale=std::min(900./im.width,double(height)/im.height)*v.zoom;const int w=int(std::lround(im.width*scale)),h=int(std::lround(im.height*scale));const double dx=std::clamp(v.panX,-std::max(0.,(w-900)/2.),std::max(0.,(w-900)/2.)),dy=std::clamp(v.panY,-std::max(0.,(h-height)/2.),std::max(0.,(h-height)/2.));return {(900-w)/2+int(dx),top+(height-h)/2+int(dy),w,h};
}
static std::vector<uint8_t> gallery_layout(bettergps_gallery::View v){
 std::vector<uint8_t> pixels(900*544*4,10);for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;DeskClockCanvas c{&pixels,{}};const bool photo=v.selected>=0&&v.selected<int(v.items->size());const int topShift=presentationImmersive.mode?presentationImmersive.top():0,bottomShift=presentationImmersive.mode?presentationImmersive.bottom():0;if(presentationImmersive.mode){v.fullscreen=true;v.controls=true;c.clipTop=0;c.clipBottom=544;}
 if(!photo)gallery_grid(pixels,v);else{
  auto r=gallery_photo_bounds(v);if(v.openedAt&&v.now>=v.openedAt&&v.now-v.openedAt<200){gallery_grid(pixels,v);const double t=copilot_ease(double(v.now-v.openedAt)/200);c.fill({0,63,900,420},RGB(10,10,10),unsigned(t*255));const auto from=gallery_cell(unsigned(v.sourceSlot));r={int(from.x+(r.x-from.x)*t),int(from.y+(r.y-from.y)*t),int(from.w+(r.w-from.w)*t),int(from.h+(r.h-from.h)*t)};}
  gallery_photo(pixels,v.image,r);if(!v.image)c.text(v.imageReady?"Cannot load this image":"Loading image...",{80,238,740,46},28,FW_NORMAL);
 }
 if(!photo||v.controls){c.fill({0,63+topShift,900,51},RGB(25,25,25),photo?210:255);c.text("<",{0,68+topShift,26,41},27);gallery_icon(pixels,"ic_launcher_gallery.dds",{27,72+topShift,34,34});
  c.text(photo?(*v.items)[v.selected].path.filename().u8string():"Steam screenshots",{73,66+topShift,730,27},21,FW_MEDIUM,RGB(255,255,255),false);
  c.text(photo?"Photo "+std::to_string(v.selected+1)+" of "+std::to_string(v.items->size()):"Grid view  |  "+std::to_string(v.items->size())+" photos",{73,92+topShift,730,17},14,FW_NORMAL,RGB(180,180,180),false);
  for(int i=0;i<3;++i)c.fill({864,74+i*10+topShift,4,4},RGB(220,220,220));
  if(photo){c.fill({0,440+bottomShift,900,43},RGB(25,25,25),210);c.text("<",{10,440+bottomShift,110,43},28);c.text(v.zoom==1?"Fit":"2x",{380,440+bottomShift,140,43},20);c.text(">",{780,440+bottomShift,110,43},28);}else if(v.items->size()>10){c.text(v.first?"<":"",{690,70,42,35},25);c.text(v.first+10<int(v.items->size())?">":"",{796,70,42,35},25);}
 }
 if(v.menu){const bool any=!v.items->empty();c.fill({592,114,296,photo?147:98},RGB(45,45,45));c.text("Slideshow",{607,117,266,43},21,FW_NORMAL,any?RGB(255,255,255):RGB(130,130,130),false);if(photo)c.text("Details",{607,166,266,43},21,FW_NORMAL,RGB(255,255,255),false);c.text("Refresh",{607,photo?215:166,266,43},21,FW_NORMAL,RGB(255,255,255),false);}
 if(v.details&&photo){c.fill({130,153,640,250},RGB(43,43,43));c.text("Details",{152,169,596,35},25,FW_MEDIUM,RGB(255,255,255),false);c.text((*v.items)[v.selected].path.filename().u8string(),{152,217,596,34},23,FW_NORMAL,RGB(255,255,255),false);c.text(v.image?std::to_string(v.image->originalWidth)+" x "+std::to_string(v.image->originalHeight):"Resolution unavailable",{152,262,596,29},20,FW_NORMAL,RGB(210,210,210),false);c.text(std::to_string((*v.items)[v.selected].bytes/1024)+" KB  |  Steam / American Truck Simulator",{152,304,596,29},19,FW_NORMAL,RGB(210,210,210),false);c.text("OK",{590,352,160,40},22);}
 return pixels;
}
static std::vector<uint8_t> copilot_gallery_layer(){return gallery_layout(bettergps_gallery::gallery.view());}
static bool copilot_gallery_hit(float x,float y,uint64_t now){
 auto v=bettergps_gallery::gallery.view();if(presentationImmersive.mode)v.controls=true;const bool photo=v.selected>=0;
 if(v.details){if(x>=130&&x<770&&y>=153&&y<403){if(y>=345)bettergps_gallery::gallery.back();}else bettergps_gallery::gallery.back();return true;}
 if(v.menu){if(x>=592&&x<888&&y>=114&&y<114+(photo?147:98)){const int row=int(y-114)/49;if(row==0)bettergps_gallery::gallery.slideshow(now);else if(photo&&row==1)bettergps_gallery::gallery.details();else bettergps_gallery::gallery.refresh();}else bettergps_gallery::gallery.menu();return true;}
 if((!photo||v.controls)&&y>=63&&y<114){if(x<70)return false;if(x>=840){bettergps_gallery::gallery.menu();return true;}if(!photo&&x>=690&&x<732){bettergps_gallery::gallery.scroll(-2);return true;}if(!photo&&x>=796&&x<838){bettergps_gallery::gallery.scroll(2);return true;}return true;}
 if(photo){if(v.controls&&y>=440&&y<483){if(x<140)bettergps_gallery::gallery.move(-1,now);else if(x>760)bettergps_gallery::gallery.move(1,now);else if(x>=380&&x<520)bettergps_gallery::gallery.zoom();return true;}if(y>=63&&y<483){bettergps_gallery::gallery.photo_tap(x,y,now);return true;}}
 else for(unsigned i=0;i<10&&v.first+int(i)<int(v.items->size());++i){const auto r=gallery_cell(i);if(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h){bettergps_gallery::gallery.open(v.first+int(i),now);return true;}}
 return false;
}
