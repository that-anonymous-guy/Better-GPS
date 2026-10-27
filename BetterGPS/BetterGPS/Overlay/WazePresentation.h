#pragma once
inline constexpr COLORREF wazeBlue=RGB(109,193,249);
// Waze uses the same validated guidance data, with its own compact presentation.
static void waze_text(std::vector<uint8_t>& pixels,const std::string& value,RECT rect,int size,bool center=false,COLORREF color=RGB(255,255,255),bool wrap=false,int weight=FW_MEDIUM,int minimum=12){
 auto text=wide_text(value);if(text.empty())return;const int w=(rect.right-rect.left)*2,h=(rect.bottom-rect.top)*2;if(w<=0||h<=0)return;
 BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 auto dc=CreateCompatibleDC(nullptr);void* bits=nullptr;auto bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!dc||!bmp){if(bmp)DeleteObject(bmp);if(dc)DeleteDC(dc);return;}auto old=SelectObject(dc,bmp);memset(bits,0,size_t(w)*h*4);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
 HFONT font=nullptr;HGDIOBJ oldFont=nullptr;for(int n=size;n>=minimum;--n){font=CreateFontW(-n*2,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Google Sans");oldFont=SelectObject(dc,font);SIZE extent{};GetTextExtentPoint32W(dc,text.data(),int(text.size()),&extent);RECT measured{0,0,w,h};if(wrap)DrawTextW(dc,text.c_str(),-1,&measured,DT_NOPREFIX|DT_WORDBREAK|DT_CALCRECT);if((wrap?measured.bottom<=h:extent.cx<=w)||n==minimum)break;SelectObject(dc,oldFont);DeleteObject(font);}
 RECT box{0,0,w,h};DrawTextW(dc,text.c_str(),-1,&box,DT_NOPREFIX|DT_END_ELLIPSIS|(center?DT_CENTER:DT_LEFT)|(wrap?DT_WORDBREAK:DT_SINGLELINE|DT_VCENTER));GdiFlush();const auto* src=static_cast<uint8_t*>(bits);
 for(int y=0;y<h/2;++y)for(int x=0;x<w/2;++x){unsigned alpha=0;for(int j=0;j<2;++j)for(int i=0;i<2;++i)alpha+=src[((y*2+j)*w+x*2+i)*4];alpha=(alpha+2)/4;int px=rect.left+x,py=rect.top+y;if(!alpha||px<0||px>=360||py<0||py>=740)continue;auto* d=pixels.data()+(py*360+px)*4;const unsigned sum=alpha+(d[3]*(255-alpha)+127)/255;const unsigned c[]={GetBValue(color),GetGValue(color),GetRValue(color)};for(int k=0;k<3;++k)d[k]=uint8_t((c[k]*alpha+(d[k]*d[3]*(255-alpha)+127)/255+sum/2)/sum);d[3]=uint8_t(sum);}
 SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);
}
static void waze_icon(std::vector<uint8_t>& pixels,const std::string& action,int x,int y,int size){auto icon=maneuver_icon(action);if(!icon.alpha)return;std::vector<uint8_t> image(icon.size*icon.size*4);for(size_t i=0;i<size_t(icon.size)*icon.size;++i){image[i*4]=image[i*4+1]=image[i*4+2]=255;image[i*4+3]=icon.alpha[i];}overlay_blit(pixels,image,icon.size,icon.size,{x,y,size,size});}
static int waze_street_width(const std::string& value){auto text=wide_text(value);auto dc=CreateCompatibleDC(nullptr);auto font=CreateFontW(-54,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Google Sans");auto old=SelectObject(dc,font);SIZE extent{};GetTextExtentPoint32W(dc,text.data(),int(text.size()),&extent);SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);return (extent.cx+1)/2+3;}
static void render_waze_guidance(std::vector<uint8_t>& pixels,const bettergps_navigation::Display& state,const std::wstring& directory=bettergps_navigation::ShieldDirectory()){
 pixels.assign(360*740*4,0);load_phone_fonts();const auto p=guidance_presentation(state);
 if(!p.message.empty()){waze_text(pixels,p.message,{12,35,348,112},30,true,RGB(255,255,255),true);return;}
 waze_icon(pixels,p.action,14,43,66);waze_text(pixels,p.distance,{97,32,346,73},37);
 auto labels=road_label_parts(p.road);bool shield=false;for(auto& label:labels)if(label.numbered&&!load_route_shield(label.shield.identity,directory).bgra.empty())shield=true;
 if(!shield)waze_text(pixels,p.road,{94,74,348,117},27,false,wazeBlue,false,FW_SEMIBOLD,25);
 else{int x=94;for(const auto& label:labels){if(x>=328)break;bool numbered=label.numbered&&!load_route_shield(label.shield.identity,directory).bgra.empty();if(numbered){paint_route_shield(pixels,360,740,load_route_shield(label.shield.identity,directory),x,77,37);x+=43;}auto text=numbered?label.shield.beside:label.text;if(!text.empty()){const int end=std::min(348,x+std::max(48,waze_street_width(text)));waze_text(pixels,text,{x,74,end,117},27,false,wazeBlue,false,FW_SEMIBOLD,25);x=end+10;}}}
 if(!p.next.empty()){waze_text(pixels,"and then",{10,119,90,154},20,false,wazeBlue);waze_icon(pixels,p.next,94,123,25);}
 if(!p.detail.empty())waze_text(pixels,p.detail,{128,119,348,154},20,false,wazeBlue);
}
static std::string waze_time_text(){
 const float seconds=g_navTimeSec.load();if(!g_haveNavTime||!std::isfinite(seconds)||seconds<0)return "-- min";const int minutes=int(std::ceil(seconds/60));if(minutes<60)return std::to_string(minutes)+" min";return std::to_string(minutes/60)+" h "+std::to_string(minutes%60)+" min";
}
static void render_waze_footer(std::vector<uint8_t>& pixels,bool dark){
 pixels.assign(360*740*4,0);const auto ink=dark?RGB(245,245,245):RGB(28,30,32);
 auto arrival=current_arrival_text(false);waze_text(pixels,arrival,{67,664,293,701},27,true,ink,false,FW_BOLD);
 auto distance=current_distance_text();auto at=distance.find(" - ");auto miles=distance.substr(0,at);
 waze_text(pixels,waze_time_text(),{56,698,171,728},18,true,ink);waze_text(pixels,miles,{189,698,304,728},18,true,ink);
 // Separate neutral dot: it does not inherit the text's light/dark tint.
 for(int y=709;y<717;++y)for(int x=176;x<184;++x){double a=std::clamp(3.1-std::hypot(x+.5-180,y+.5-713),0.0,1.0);auto* d=pixels.data()+(y*360+x)*4;d[0]=d[1]=d[2]=145;d[3]=uint8_t(a*255);}
}
static void render_waze_speed(std::vector<uint8_t>& pixels){
 pixels.assign(360*740*4,0);
 // Dark dial in both themes; the small US limit sign overlaps its upper right.
 std::vector<uint8_t> dial(76*76*4);for(int y=0;y<76;++y)for(int x=0;x<76;++x){const double dx=x+.5-38,dy=y+.5-38,r=std::hypot(dx,dy);auto* d=&dial[(y*76+x)*4];const double edge=std::clamp(36-r,0.0,1.0);if(!edge)continue;
  unsigned red=31,green=34,blue=44;const double angle=std::atan2(dy,dx);const bool dot=r>31&&r<33&&std::cos(angle*48)>.1;const bool rim=r>34.4;
  if(dot){red=90;green=94;blue=108;}else if(rim){red=51;green=54;blue=65;}d[0]=uint8_t(blue);d[1]=uint8_t(green);d[2]=uint8_t(red);d[3]=uint8_t(edge*255);
 }
 overlay_blit(pixels,dial,76,76,{10,532,76,76});const auto ink=current_speed_is_over_limit()?RGB(242,82,82):RGB(255,255,255);
 waze_text(pixels,current_speed_text(),{19,548,77,585},33,true,ink);waze_text(pixels,"mph",{27,578,69,598},13,true,RGB(255,255,255),false,FW_BOLD);
 overlay_blit(pixels,phone_module_pixels(358),128,128,{59,511,43,43});waze_text(pixels,current_speed_limit_text(),{63,518,98,553},24,true,RGB(25,25,28),false,FW_BOLD);
}
struct WazeCachedLayer {std::vector<uint8_t> pixels;Placement bounds;};
static void waze_cache_layer(WazeCachedLayer& cache,std::vector<uint8_t>& full){
 int l=360,t=740,r=0,b=0;for(int y=0;y<740;++y)for(int x=0;x<360;++x)if(full[(y*360+x)*4+3]){l=std::min(l,x);r=std::max(r,x+1);t=std::min(t,y);b=std::max(b,y+1);}cache.pixels.clear();if(r<=l||b<=t){cache.bounds={};return;}cache.bounds={l,t,r-l,b-t};cache.pixels.resize(size_t(r-l)*(b-t)*4);for(int y=t;y<b;++y)memcpy(cache.pixels.data()+size_t(y-t)*(r-l)*4,full.data()+(y*360+l)*4,size_t(r-l)*4);
}
static void render_waze_contents(std::vector<uint8_t>& pixels,const OverlaySettings& config,bool trip,bool dark){
 auto attach=[&](size_t item,const WazeCachedLayer& layer){if(!layer.pixels.empty())overlay_blit(pixels,layer.pixels,layer.bounds.w,layer.bounds.h,attached_placement(config,item,layer.bounds));};
 static std::string guidanceKey,footerKey,speedKey;static WazeCachedLayer guidance,footer,speed;static std::vector<uint8_t> work;
 auto display=bettergps_navigation::Current();if(trip){auto key=display.instruction+display.text+display.status+display.currentRoad+display.targetRoad+display.destination+display.next+display.exit+std::to_string(shield_revision(bettergps_navigation::ShieldDirectory()));if(key!=guidanceKey||guidance.pixels.empty()){render_waze_guidance(work,display);waze_cache_layer(guidance,work);guidanceKey=key;}attach(3,guidance);
 auto key2=current_time_text()+current_distance_text()+(dark?"D":"L");if(key2!=footerKey||footer.pixels.empty()){render_waze_footer(work,dark);waze_cache_layer(footer,work);footerKey=key2;}attach(dark?7:6,footer);}
 auto key=current_speed_text()+current_speed_limit_text()+(dark?"D":"L")+(current_speed_is_over_limit()?"O":"N");if(key!=speedKey||speed.pixels.empty()){render_waze_speed(work);waze_cache_layer(speed,work);speedKey=key;}attach(overlayModuleCount,speed);
 if(g_haveCompassHeading.load()){static std::vector<uint8_t> needle;render_compass_needle(needle,g_compassHeading.load());auto item=trip?(dark?9:8):(dark?11:10);overlay_blit(pixels,needle,36,36,attached_placement(config,item,{22,trip?179:98,36,36}));}
}
