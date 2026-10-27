#pragma once
// CPU-only NavPlus artwork and text, sharing Garmin fonts/guidance resources.
static std::string navplus_cardinal(double heading){
 static const char* names[]={"N","NW","W","SW","S","SE","E","NE"};
 return std::isfinite(heading)?names[unsigned(std::floor(compass_turn(heading)*8+.5))%8]:"--";
}
static std::string navplus_clock(){
 if(!g_haveGameTime)return "--:--";const int64_t now=current_game_minutes();
 const int h=int(now%1440)/60;char text[24];std::snprintf(text,sizeof(text),"%d:%02d%s",h%12?h%12:12,int(now%60),h<12?"am":"pm");return text;
}
static std::string navplus_duration(){
 const double seconds=g_navTimeSec.load();if(!g_haveNavTime||!std::isfinite(seconds)||seconds<0||seconds/60.>2147483646.)return "--:--";
 const int minutes=int(std::ceil(seconds/60.));char text[32];std::snprintf(text,sizeof(text),"%d:%02d",minutes/60,minutes%60);return text;
}
static std::vector<uint8_t> navplus_arrive_in(COLORREF ink){
 // Different font sizes, but one actual GDI baseline for the whole h:mm group.
 constexpr int w=196,h=70;std::vector<uint8_t> result(w*h*4,0);auto duration=navplus_duration();const auto colon=duration.find(':');
 const std::array<std::wstring,3> runs{wide_text(duration.substr(0,colon)),L":",wide_text(duration.substr(colon+1))};
 BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w*2;bi.bmiHeader.biHeight=-h*2;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 auto dc=CreateCompatibleDC(nullptr);void* bits=nullptr;auto bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!dc||!bmp){if(bmp)DeleteObject(bmp);if(dc)DeleteDC(dc);return result;}
 auto old=SelectObject(dc,bmp);memset(bits,0,size_t(w)*h*16);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));SetTextAlign(dc,TA_LEFT|TA_BASELINE);
 std::array<HFONT,3> fonts{};std::array<int,3> widths{};double scale=1;int total=0;
 for(int attempt=0;attempt<2;++attempt){total=0;for(size_t i=0;i<3;++i){if(fonts[i])DeleteObject(fonts[i]);fonts[i]=CreateFontW(-int(std::lround((i?46:navplusPrimaryFont)*2*scale)),0,0,0,i?FW_BOLD:FW_HEAVY,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Arial");auto previous=SelectObject(dc,fonts[i]);SIZE extent{};GetTextExtentPoint32W(dc,runs[i].c_str(),int(runs[i].size()),&extent);SelectObject(dc,previous);widths[i]=extent.cx;total+=extent.cx;}if(total<=w*2-4)break;scale*=double(w*2-4)/total;}
 int x=(w*2-total)/2;for(size_t i=0;i<3;++i){auto previous=SelectObject(dc,fonts[i]);TextOutW(dc,x,120,runs[i].c_str(),int(runs[i].size()));if(i==0)TextOutW(dc,x+1,120,runs[i].c_str(),int(runs[i].size()));SelectObject(dc,previous);x+=widths[i];}GdiFlush();
 const auto* source=static_cast<const uint8_t*>(bits);for(int y=0;y<h;++y)for(int x=0;x<w;++x){unsigned alpha=0;for(int j=0;j<2;++j)for(int i=0;i<2;++i)alpha+=source[((y*2+j)*w*2+x*2+i)*4];auto* p=result.data()+(y*w+x)*4;p[0]=GetBValue(ink);p[1]=GetGValue(ink);p[2]=GetRValue(ink);p[3]=uint8_t((alpha+2)/4);}
 for(auto font:fonts)DeleteObject(font);SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);return result;
}
static void navplus_speed_text(std::vector<uint8_t>& values,COLORREF ink){
 // Reserve the full Arial line height so garmin_text does not auto-shrink it.
 garmin_text(values,900,112,current_speed_text(),{580,34,724,108},navplusPrimaryFont,ink,FW_HEAVY,false,false,true);
 garmin_text(values,900,112,"m",{730,53,750,79},20,ink,FW_BOLD);
 garmin_text(values,900,112,"h",{730,73,750,99},20,ink,FW_BOLD);
}
static std::vector<uint8_t> navplus_speed_limit(bool dark){
 std::vector<uint8_t> limit(65*83*4,255);for(int y=0;y<83;++y)for(int x=0;x<65;++x)if(x<3||x>=62||y<3||y>=80){auto* p=limit.data()+(y*65+x)*4;p[0]=p[1]=p[2]=0;}
 garmin_text(limit,65,83,"LIMIT",{4,4,61,31},22,RGB(0,0,0),FW_BOLD,true);garmin_text(limit,65,83,current_speed_limit_text(),{4,29,61,78},43,RGB(0,0,0),FW_BOLD,true);
 if(dark)for(size_t p=0;p<limit.size();p+=4)for(int c=0;c<3;++c)limit[p+c]=255-limit[p+c];return limit;
}
// The wider device separates the two footer halves instead of stretching its
// buttons, type or speed plate. The module's vertical scale is also 16/17.
static std::vector<uint8_t> navplus_wide_footer(const std::vector<uint8_t>& source,int sw,int sh){
 std::vector<uint8_t> base(900*112*4,0),result(base.size(),0),half(450*112*4);
 overlay_blit(base,source,sw,sh,{0,0,900,112},900,112);
 for(int side=0;side<2;++side){for(int y=0;y<112;++y)memcpy(half.data()+size_t(y)*450*4,base.data()+(size_t(y)*900+side*450)*4,450*4);overlay_blit(result,half,450,112,{side?476:0,0,424,112},900,112);}
 return result;
}
static void render_navplus(std::vector<uint8_t>& pixels,const OverlaySettings& config,const bettergps_navigation::Display& guidance=bettergps_navigation::Current()){
 const bool smartnav=config.navplus>=2,large579=config.navplus==3&&config.format==2;const auto size=navplus_canvas(config.format,smartnav);const int width=int(size.first),height=int(size.second);const bool wide=config.format==3;const std::string shape=wide?"5by3":"3by2",model=smartnav?"peterbilt_smartnav":"kenworth_navplus";pixels.assign(width*height*4,0);const bool dark=presentation_dark(),trip=g_tripPresent.load();
 auto art=[&](size_t item,const std::string& file){const auto& image=garmin_image(file,model);overlay_blit(pixels,image.pixels,image.width,image.height,config.items[item],width,height);};
 auto module=[&](size_t item,const std::vector<uint8_t>& data,int w,int h){overlay_blit(pixels,data,w,h,config.items[item],width,height);};
 std::vector<uint8_t> compass(58*43*4,0);garmin_text(compass,58,43,g_haveCompassHeading?navplus_cardinal(g_compassHeading.load()):"--",{0,0,58,43},38,RGB(255,255,255),FW_BOLD,false,false,true);
 auto compass_module=[&](size_t item){auto p=config.items[item];p.x-=int(std::lround(2.*p.w/58));p.y-=int(std::lround(4.*p.h/43));overlay_blit(pixels,compass,58,43,p,width,height);};
 if(presentation_home(trip)){
  art(1,"home_"+model+"_"+shape+".dds");std::vector<uint8_t> info(235*48*4,0);
  garmin_text(info,235,48,navplus_clock(),{3,0,166,48},34,RGB(255,255,255),FW_NORMAL,false,false,true);
  for(int y=5;y<43;++y)for(int x=169;x<171;++x){auto* p=info.data()+(y*235+x)*4;p[0]=p[1]=p[2]=150;p[3]=255;}
  garmin_text(info,235,48,navplusHaveTemperature?std::to_string(navplusTemperatureF.load()):"--",{171,0,226,48},32,RGB(255,255,255),FW_NORMAL,false,false,true);
  garmin_text(info,235,48,"f",{225,2,235,48},28,RGB(255,255,255),FW_NORMAL,false,false,true);
  module(8,info,235,48);compass_module(13);return;
 }
 // Reuse the original 3:2 header and zoom, without deforming any glyphs.
 if(trip){
 const auto& source=garmin_image(std::string("nav/garmin_465t_3by2_nav_above_")+(dark?"dark":"light")+".dds");
 if(source.width==1200&&source.height==240){
  const int hh=large579?153:wide?139:118;std::vector<uint8_t> header(900*hh*4,0),bar(1200*90*4),bubble(270*230*4,0);
  for(int y=0;y<90;++y)memcpy(bar.data()+size_t(y)*1200*4,source.pixels.data()+size_t(y)*1200*4,1200*4);
  for(int y=0;y<230;++y)memcpy(bubble.data()+size_t(y)*270*4,source.pixels.data()+size_t(y)*1200*4,270*4);
  overlay_blit(header,bar,1200,90,{0,0,900,wide?69:73},900,hh);overlay_blit(header,bubble,270,230,{0,0,large579?332:wide?300:255,hh},900,hh);module(dark?2:0,header,900,hh);
 }
 }
 const auto& zoom=garmin_image("nav/garmin_465t_anyres_nav_zoom.dds");module(4,zoom.pixels,zoom.width,zoom.height);
 if(trip){
 const auto p=guidance_presentation(guidance);std::vector<uint8_t> instructions(250*118*4,0);
 garmin_text(instructions,250,118,p.distance,{9,1,205,48},40,RGB(255,255,255),FW_BOLD,true,true);
 const auto icon=maneuver_icon(p.action);if(icon.alpha){std::vector<uint8_t> arrow(size_t(icon.size)*icon.size*4,255),edge(arrow.size(),0);for(size_t i=0;i<size_t(icon.size)*icon.size;++i){arrow[i*4+3]=icon.alpha[i];edge[i*4+3]=icon.alpha[i];}const Placement place{82,53,53,53};for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)if(x||y)overlay_blit(instructions,edge,icon.size,icon.size,{place.x+x,place.y+y,place.w,place.h},250,118);overlay_blit(instructions,arrow,icon.size,icon.size,place,250,118);}
 module(3,instructions,250,118);const int roadWidth=large579?543:wide?612:620;std::vector<uint8_t> road(roadWidth*73*4,0);garmin_text(road,roadWidth,73,p.message.empty()?p.road:p.message,{0,0,roadWidth-8,73},44,RGB(255,255,255));module(5,road,roadWidth,73);
 }
 const auto& footer=garmin_image(std::string(trip?"nav_1440_darkfooter_anyres_":"notrip_1440_darkfooter_anyres_")+(dark?"dark":"light")+".dds","Nuvi1440_collab");
 if(wide){const auto expanded=navplus_wide_footer(footer.pixels,footer.width,footer.height);module(dark?7:6,expanded,900,112);}else module(dark?7:6,footer.pixels,footer.width,footer.height);
 std::vector<uint8_t> values(900*112*4,0);const auto ink=dark?RGB(255,255,255):RGB(0,0,0);
 if(trip){const auto arrival=navplus_arrive_in(ink);overlay_blit(values,arrival,196,70,{111,32,196,70},900,112);}
 navplus_speed_text(values,ink);if(wide)values=navplus_wide_footer(values,900,112);module(9,values,900,112);
 const auto limit=navplus_speed_limit(dark);module(14,limit,65,83);
 art(12,model+"_"+shape+"_below_frame.dds");compass_module(11);
}
