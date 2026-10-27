#pragma once
// Clock uses game minutes, including the multiplayer offset. No wall-clock timer.
struct CopilotClockText {std::string hour,minute,period,day;};
static CopilotClockText copilot_clock_text(int64_t minutes){
 if(minutes<0)return {"--","--","--","---, WEEK -"};
 const int hour=int(minutes%1440)/60;char number[8];std::snprintf(number,sizeof(number),"%02d",int(minutes%60));
 auto day=copilot_weekday(minutes);for(auto& c:day)c=char(std::toupper(static_cast<unsigned char>(c)));
 return {std::to_string(hour%12?hour%12:12),number,hour<12?"AM":"PM",day+", WEEK "+std::to_string(minutes/10080+1)};
}
struct CopilotClockGlyph {int width=0,height=0,advance=0,left=0,top=0;std::vector<uint8_t> pixels;};
static CopilotClockGlyph copilot_clock_glyph(const std::string& value,int size,int weight,int tracking=0){
 // Supersample the actual Roboto weights. Tracking is in thousandths of an em.
 constexpr int scale=3;CopilotClockGlyph glyph;const auto text=wide_text(value);if(text.empty())return glyph;
 HDC dc=CreateCompatibleDC(nullptr);if(!dc)return glyph;
 // Static Roboto releases expose separate legacy GDI families per weight.
 const wchar_t* face=weight==FW_THIN?L"Roboto Thin":weight==FW_EXTRABOLD?L"Roboto ExtraBold":L"Roboto Medium";
 HFONT font=CreateFontW(-size*scale,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,face);
 const auto oldFont=SelectObject(dc,font);SetTextCharacterExtra(dc,int(std::lround(size*scale*tracking/1000.)));SIZE extent{};GetTextExtentPoint32W(dc,text.data(),int(text.size()),&extent);
 const int w=((extent.cx+scale*4+scale-1)/scale)*scale,h=size*scale*2;glyph.advance=(extent.cx+scale/2)/scale;
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 void* bits=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
 if(bitmap){const auto oldBitmap=SelectObject(dc,bitmap);memset(bits,0,size_t(w)*h*4);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));TextOutW(dc,0,0,text.data(),int(text.size()));GdiFlush();
  const auto* source=static_cast<const uint8_t*>(bits);int left=w/scale,top=h/scale,right=0,bottom=0;
  std::vector<uint8_t> alpha(size_t(w/scale)*(h/scale));for(int y=0;y<h/scale;++y)for(int x=0;x<w/scale;++x){unsigned sum=0;for(int j=0;j<scale;++j)for(int i=0;i<scale;++i)sum+=source[((y*scale+j)*w+x*scale+i)*4];const auto a=uint8_t((sum+4)/9);alpha[size_t(y)*(w/scale)+x]=a;if(a){left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+1);bottom=std::max(bottom,y+1);}}
  if(right>left&&bottom>top){glyph.left=left;glyph.top=top;glyph.width=right-left;glyph.height=bottom-top;glyph.pixels.resize(size_t(glyph.width)*glyph.height*4,255);for(int y=0;y<glyph.height;++y)for(int x=0;x<glyph.width;++x)glyph.pixels[(size_t(y)*glyph.width+x)*4+3]=alpha[size_t(y+top)*(w/scale)+x+left];}
  SelectObject(dc,oldBitmap);DeleteObject(bitmap);
 }
 SelectObject(dc,oldFont);DeleteObject(font);DeleteDC(dc);return glyph;
}
#include "DeskClockPresentation.h"
