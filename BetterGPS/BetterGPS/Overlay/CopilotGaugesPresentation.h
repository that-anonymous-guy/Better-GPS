#pragma once
#include "T680GaugeValues.h"
static constexpr double copilotNeedlePivotX=125.5,copilotNeedlePivotY=37.;
static constexpr double copilotGaugePivotX=164.5,copilotGaugePivotY=164.;
static std::vector<uint8_t> copilot_gauge_needle(const GarminImage& image,double angle){
 std::vector<uint8_t> pixels(329*286*4,0);if(image.pixels.empty())return pixels;
 const double radians=(angle-180)*3.14159265358979323846/180,c=std::cos(radians),s=std::sin(radians);
 for(int y=28;y<286;++y)for(int x=28;x<302;++x){
  const double dx=x-copilotGaugePivotX,dy=y-copilotGaugePivotY,sx=copilotNeedlePivotX+c*dx+s*dy,sy=copilotNeedlePivotY-s*dx+c*dy;
  const int ix=int(std::floor(sx)),iy=int(std::floor(sy));if(ix<0||iy<0||ix+1>=int(image.width)||iy+1>=int(image.height))continue;
  const double fx=sx-ix,fy=sy-iy;double alpha=0,rgb[3]{};
  for(int j=0;j<2;++j)for(int i=0;i<2;++i){const auto* p=image.pixels.data()+((iy+j)*image.width+ix+i)*4;const double a=p[3]*(i?fx:1-fx)*(j?fy:1-fy);alpha+=a;for(int k=0;k<3;++k)rgb[k]+=p[k]*a;}
  if(alpha<.5)continue;auto* p=pixels.data()+(y*329+x)*4;for(int k=0;k<3;++k)p[k]=uint8_t(std::clamp(std::lround(rgb[k]/alpha),0l,255l));p[3]=uint8_t(std::clamp(std::lround(alpha),0l,255l));
 }
 return pixels;
}
static const std::array<Placement,6> copilotGaugeBounds{{{88,71,224,194},{339,71,224,194},{591,71,224,194},{88,270,224,194},{339,270,224,194},{591,270,224,194}}};
static const Placement copilotGaugeDetailBounds{217,65,466,405};
static constexpr const char* copilotGaugeFaces[]={"oiltemp","trans","boost","dpf","mpg","torque"};
static constexpr unsigned copilot_gauge_page_count(){return unsigned((std::size(copilotGaugeFaces)+std::size(copilotGaugeBounds)-1)/std::size(copilotGaugeBounds));}
static Placement copilot_gauge_background_bounds(const GarminImage& image){
 // The replacement contains only the title, cropped from the original canvas.
 if(image.width==1324&&image.height==800)return {0,0,900,544};
 return {int(std::lround(900.*8/1324)),int(std::lround(544.*81/800)),int(std::lround(900.*169/1324)),int(std::lround(544.*48/800))};
}
static const GarminImage& copilot_gauge_face(unsigned index){return copilot_image(std::string("navplushd_gauge_")+copilotGaugeFaces[index]+".dds");}
static std::string copilot_mpg_text(const T680GaugeSnapshot& values){if(!(values.valid&16))return "--.-";char text[64];std::snprintf(text,sizeof(text),"%.1f",values.mpg);return text;}
static std::vector<uint8_t> copilot_gauge_red(std::vector<uint8_t> pixels){for(size_t i=0;i<pixels.size();i+=4){const auto bright=std::max({pixels[i],pixels[i+1],pixels[i+2]});pixels[i]=pixels[i+1]=0;pixels[i+2]=bright;}return pixels;}
static std::vector<uint8_t> copilot_gauges_layer(const T680GaugeSnapshot& values,int detail=-1){
 struct Base{std::string root;std::vector<uint8_t> pixels;unsigned hot=~0u;};static std::array<Base,7> bases;auto& base=bases[detail>=0&&detail<6?detail+1:0];const auto hot=t680_gauge_hot(values);
 if(base.pixels.empty()||base.root!=g_assetRoot||base.hot!=hot){base.root=g_assetRoot;base.hot=hot;base.pixels.assign(900*544*4,10);for(size_t i=3;i<base.pixels.size();i+=4)base.pixels[i]=255;
  if(detail<0){const auto& background=copilot_image("navplushd_gauges_background.dds");overlay_blit(base.pixels,background.pixels,background.width,background.height,copilot_gauge_background_bounds(background),900,544);}
  for(unsigned i=0;i<6;++i)if(detail<0||int(i)==detail){const auto& image=copilot_gauge_face(i);const auto red=(i<2&&(hot&(1u<<i)))?copilot_gauge_red(image.pixels):std::vector<uint8_t>{};overlay_blit(base.pixels,red.empty()?image.pixels:red,image.width,image.height,detail<0?copilotGaugeBounds[i]:copilotGaugeDetailBounds,900,544);}
 }
 static std::array<std::vector<uint8_t>,6> needles;static std::array<int,6> angles{{-1,-1,-1,-1,-1,-1}};static std::string needleRoot;const auto& needle=copilot_image("navplushd_needle.dds");if(needleRoot!=g_assetRoot){needleRoot=g_assetRoot;angles.fill(-1);}
 auto pixels=base.pixels;const auto positions=t680_gauge_angles(values);
 for(unsigned i=0;i<6;++i)if(detail<0||int(i)==detail){const int angle=int(std::lround(positions[i]*4));if(angles[i]!=angle){angles[i]=angle;needles[i]=copilot_gauge_needle(needle,angle/4.);}const auto r=detail<0?copilotGaugeBounds[i]:copilotGaugeDetailBounds;const auto red=(i<2&&(hot&(1u<<i)))?copilot_gauge_red(needles[i]):std::vector<uint8_t>{};overlay_blit(pixels,red.empty()?needles[i]:red,329,286,r,900,544);
  if(i==4){const auto text=copilot_mpg_text(values);const RECT bounds{r.x+LONG(std::lround(153.*r.w/325)),r.y+LONG(std::lround(211.*r.h/284)),r.x+LONG(std::lround(215.*r.w/325)),r.y+LONG(std::lround(241.*r.h/284))};garmin_text(pixels,900,544,text,bounds,int(std::lround((text.size()>4?20.:26.)*r.h/284)),RGB(255,255,255),FW_BOLD,true);}
 }
 if(detail<0)copilot_page_indicators(pixels,{0,0,900,544},copilot_gauge_page_count(),0);
 return pixels;
}
static std::string copilot_gauges_key(){const auto s=t680Gauges.get(vehicleStart.epoch.load());const auto angles=t680_gauge_angles(s);std::string key=std::to_string(s.valid)+":"+copilot_mpg_text(s)+":"+std::to_string(t680_gauge_hot(s));for(const auto a:angles)key+=":"+std::to_string(int(std::lround(a*4)));return key;}
