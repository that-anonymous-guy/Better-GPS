#pragma once
// Included with the CPU presentation renderers. Assets and GDI work are confined
// to the preparation thread; only the resulting pixels reach the render hook.
struct GarminImage {unsigned width=0,height=0;std::vector<uint8_t> pixels;};
static const GarminImage& garmin_image(const std::string& name,const std::string& model="nuvi465t"){
 struct Entry{GarminImage image;std::filesystem::file_time_type time{};uint64_t next=0;};
 static std::map<std::string,Entry> images;auto& entry=images[model+"/"+name];const auto now=GetTickCount64();if(now<entry.next)return entry.image;entry.next=now+1000;
 const auto path=std::filesystem::u8path(g_assetRoot)/"material/garmin"/model/name;std::error_code ec;auto time=std::filesystem::last_write_time(path,ec);if(ec||time==entry.time&&!entry.image.pixels.empty())return entry.image;
 HomeDecoder decoder;auto data=decoder.dds(path,&entry.image.width,&entry.image.height,4096);if(!data.empty()){entry.image.pixels=std::move(data);entry.time=time;}return entry.image;
}
static void garmin_text(std::vector<uint8_t>& pixels,int width,int height,const std::string& value,RECT rect,int size,COLORREF color,int weight=FW_NORMAL,bool center=false,bool outline=false,bool right=false,const wchar_t* face=L"Arial"){
 auto text=wide_text(value);const int w=(rect.right-rect.left)*2,h=(rect.bottom-rect.top)*2;if(text.empty()||w<=0||h<=0)return;
 BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 auto dc=CreateCompatibleDC(nullptr);void* bits=nullptr;auto bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!dc||!bmp){if(bmp)DeleteObject(bmp);if(dc)DeleteDC(dc);return;}auto old=SelectObject(dc,bmp);memset(bits,0,size_t(w)*h*4);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
 HFONT font=nullptr;HGDIOBJ oldFont=nullptr;for(int n=size;n>=8;--n){font=CreateFontW(-n*2,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,face);oldFont=SelectObject(dc,font);SIZE extent{};GetTextExtentPoint32W(dc,text.data(),int(text.size()),&extent);TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);if(extent.cx<=w&&metrics.tmHeight<=h||n==8)break;SelectObject(dc,oldFont);DeleteObject(font);}
 RECT box{0,0,w,h};DrawTextW(dc,text.c_str(),-1,&box,DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER|(right?DT_RIGHT:center?DT_CENTER:DT_LEFT));if(weight>=FW_HEAVY){box.left++;box.right++;DrawTextW(dc,text.c_str(),-1,&box,DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER|(right?DT_RIGHT:center?DT_CENTER:DT_LEFT));}GdiFlush();const auto* src=static_cast<uint8_t*>(bits);
 std::vector<uint8_t> glyph(size_t(w/2)*(h/2)*4,0);for(int y=0;y<h/2;++y)for(int x=0;x<w/2;++x){unsigned alpha=0;for(int j=0;j<2;++j)for(int i=0;i<2;++i)alpha+=src[((y*2+j)*w+x*2+i)*4];auto* d=glyph.data()+(size_t(y)*(w/2)+x)*4;d[0]=GetBValue(color);d[1]=GetGValue(color);d[2]=GetRValue(color);d[3]=uint8_t((alpha+2)/4);}
 if(outline){auto edge=glyph;for(size_t p=0;p<edge.size();p+=4)edge[p]=edge[p+1]=edge[p+2]=0;for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x)if(x||y)overlay_blit(pixels,edge,w/2,h/2,{rect.left+x,rect.top+y,w/2,h/2},width,height);}
 overlay_blit(pixels,glyph,w/2,h/2,{rect.left,rect.top,w/2,h/2},width,height);
 SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);
}
struct GarminArrival {std::string time="--:--",period="",day="";};
static GarminArrival garmin_arrival(){
 GarminArrival result;if(!g_haveGameTime||!g_haveNavTime)return result;const double seconds=g_navTimeSec.load();if(!std::isfinite(seconds)||seconds<0)return result;
 const int64_t now=current_game_minutes(),arrival=now+int64_t(std::ceil(seconds/60.));const int minute=int(arrival%1440),hour=minute/60;char text[16];std::snprintf(text,sizeof(text),"%d:%02d",hour%12?hour%12:12,minute%60);result.time=text;result.period=hour<12?"AM":"PM";
 static const char* days[]={"MON","TUE","WED","THU","FRI","SAT","SUN"};if(arrival/1440!=now/1440)result.day=days[arrival/1440%7];return result;
}
static std::vector<uint8_t> garmin_footer(bool dark,bool trip=true){
 std::vector<uint8_t> pixels(870*140*4,0);const COLORREF ink=dark?RGB(255,255,255):RGB(0,0,0);
 // Numeric boxes have hard boundaries; 1/2/3-digit speeds never cover m/h.
 garmin_text(pixels,870,140,current_speed_text(),{10,47,188,133},83,ink,FW_BOLD,false,false,true);
 garmin_text(pixels,870,140,"m",{194,73,224,98},26,ink);garmin_text(pixels,870,140,"h",{194,96,224,121},26,ink);
 if(!trip)return pixels;
 const auto eta=garmin_arrival();const bool later=!eta.day.empty();const int left=later?634:617;
 garmin_text(pixels,870,140,eta.time,{left,47,792,133},later?65:83,ink,FW_BOLD,true);
 garmin_text(pixels,870,140,eta.period.substr(0,1),{794,73,828,98},24,ink);garmin_text(pixels,870,140,eta.period.substr(1),{794,96,828,121},24,ink);
 if(later)for(int i=0;i<3;++i)garmin_text(pixels,870,140,eta.day.substr(i,1),{606,51+i*23,634,77+i*23},28,ink,FW_HEAVY,true);
 return pixels;
}
static std::vector<uint8_t> garmin_limit(bool dark=false){
 constexpr int w=110,h=130;std::vector<uint8_t> pixels(w*h*4,255);for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(x<5||x>=w-5||y<5||y>=h-5){const auto value=uint8_t(90*(1-std::abs(2.*y/(h-1)-1)));auto* p=pixels.data()+(y*w+x)*4;p[0]=p[1]=p[2]=value;}
 garmin_text(pixels,w,h,"SPEED",{8,6,102,28},22,RGB(0,0,0),FW_NORMAL,true);garmin_text(pixels,w,h,"LIMIT",{8,27,102,49},22,RGB(0,0,0),FW_NORMAL,true);
 garmin_text(pixels,w,h,current_speed_limit_text(),{8,47,102,103},60,RGB(0,0,0),FW_BOLD,true);garmin_text(pixels,w,h,"mph",{8,101,102,125},24,RGB(0,0,0),FW_NORMAL,true);if(dark)for(size_t p=0;p<pixels.size();p+=4)for(int c=0;c<3;++c)pixels[p+c]=255-pixels[p+c];return pixels;
}
static void render_garmin(std::vector<uint8_t>& pixels,const OverlaySettings& config,const bettergps_navigation::Display& guidance=bettergps_navigation::Current()){
 const auto size=garmin_canvas(config.format);const int width=int(size.first),height=int(size.second);pixels.assign(size_t(width)*height*4,0);
 const bool dark=presentation_dark(),trip=g_tripPresent.load(),three=config.format==2;const std::string shape=three?"3by2":"dlctoys",mode=dark?"dark":"light";
 auto paint=[&](size_t item,const std::string& file){const auto& image=garmin_image(file);overlay_blit(pixels,image.pixels,int(image.width),int(image.height),config.items[item],width,height);};
 if(presentation_home(trip)){paint(dark?11:10,"home/garmin_465t_"+shape+"_home_background_"+mode+".dds");paint(1,"home/garmin_465t_"+shape+"_home_above.dds");paint(dark?9:8,"home/garmin_465t_anyres_home_middle_"+mode+".dds");paint(dark?13:12,"home/garmin_465t_"+shape+"_home_below_"+mode+".dds");return;}
 // Enlarge the bar and bubble independently. Stretching the entire header
 // would leave the bubble too narrow for its enlarged instruction module.
 if(trip){
 const auto& source=garmin_image("nav/garmin_465t_"+shape+"_nav_above_"+mode+".dds");
 if(source.width==1200&&source.height==unsigned(three?240:210)&&source.pixels.size()==size_t(source.width)*source.height*4){
  const int bh=three?240:210,hh=int(std::lround(bh*1.3));std::vector<uint8_t> header(size_t(1200)*hh*4,0),bar(1200*73*4),bubble(250*bh*4);
  for(int y=0;y<73;++y)memcpy(bar.data()+size_t(y)*1200*4,source.pixels.data()+size_t(y)*source.width*4,1200*4);
  for(int y=0;y<bh;++y)memcpy(bubble.data()+size_t(y)*250*4,source.pixels.data()+size_t(y)*source.width*4,250*4);
  overlay_blit(header,bar,1200,73,{0,0,1200,88},1200,hh);overlay_blit(header,bubble,250,bh,{0,0,325,int(std::lround(bh*1.3))},1200,hh);
  overlay_blit(pixels,header,1200,hh,config.items[dark?2:0],width,height);
 }
 }
 paint(4,"nav/garmin_465t_anyres_nav_zoom.dds");
 // The supplied footer filenames are reversed: _dark is silver, _light black.
 const size_t footer=dark?7:6;paint(footer,std::string(trip?"nav/garmin_465t_anyres_nav_below_":"notrip/garmin_465t_anyres_notrip_below_")+(trip?(dark?"light":"dark"):(dark?"dark":"light"))+".dds");
 auto values=garmin_footer(dark,trip);overlay_blit(pixels,values,870,140,config.items[footer],width,height);
 auto limit=garmin_limit(dark);overlay_blit(pixels,limit,110,130,config.items[14],width,height);
 if(!trip)return;
 const auto p=guidance_presentation(guidance);const int bubbleHeight=three?240:210;std::vector<uint8_t> bubble(size_t(250)*bubbleHeight*4,0);
 garmin_text(bubble,250,bubbleHeight,p.distance,{10,2,190,74},65,RGB(255,255,255),FW_BOLD,true,true);
 const auto icon=maneuver_icon(p.action);if(icon.alpha){std::vector<uint8_t> arrow(size_t(icon.size)*icon.size*4,255),edge(arrow.size(),0);for(size_t i=0;i<size_t(icon.size)*icon.size;++i){arrow[i*4+3]=icon.alpha[i];edge[i*4+3]=icon.alpha[i];}const Placement place{58,three?108:80,85,85};for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x)if(x||y)overlay_blit(bubble,edge,icon.size,icon.size,{place.x+x,place.y+y,place.w,place.h},250,bubbleHeight);overlay_blit(bubble,arrow,icon.size,icon.size,place,250,bubbleHeight);}
 overlay_blit(pixels,bubble,250,bubbleHeight,config.items[3],width,height);
 // Road tokens remain text, including Interstate/US/state route identifiers;
 // no road_label_parts or shield painter is used in the Nuvi presentation.
 std::vector<uint8_t> road(907*73*4,0);garmin_text(road,907,73,p.message.empty()?p.road:p.message,{0,0,900,73},56,RGB(255,255,255));overlay_blit(pixels,road,907,73,config.items[5],width,height);
}
