#pragma once
#include <Windows.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include "TouchDiagnostics.h"
namespace bettergps_overlay {
inline std::atomic<bool> settingsExpanded{false};
inline std::atomic<int> menuCursorX{0},menuCursorY{0};
inline void (*settingsTouchPointer)(POINT,bool,uint64_t)=nullptr;
inline void (*settingsTouchCancel)()=nullptr;
struct MenuDock {
 std::mutex mutex;std::filesystem::path path;
 float x=.012f,y=.018f,width=1280,height=720,scale=1,diagnosticX=1,diagnosticY=.01f;
 bool held=false,dragged=false;int owner=0;POINT press{};float originX=0,originY=0;
 std::atomic<bool> dirty{false};
 void init(const std::filesystem::path& root){
  std::lock_guard<std::mutex> lock(mutex);path=root/"BetterGPSMenu.ini";
  std::ifstream in(path);std::string name;float value;
  while(in>>name>>value)if(std::isfinite(value)){if(name=="header_x")x=std::clamp(value,0.f,1.f);if(name=="header_y")y=std::clamp(value,0.f,1.f);}
 }
 bool save(){
  if(!dirty.exchange(false))return true;std::lock_guard<std::mutex> lock(mutex);
  if(path.empty())return true;std::ofstream out(path);out<<"header_x "<<x<<"\nheader_y "<<y<<'\n';
  if(!out){dirty=true;return false;}return true;
 }
 void layout(float w,float h){
  std::lock_guard<std::mutex> lock(mutex);width=std::max(1.f,w);height=std::max(1.f,h);
  scale=std::max(.1f,std::min(width/1040.f,height/820.f)*.92f);
  const float maxX=std::max(0.f,width-270*scale),maxY=std::max(0.f,height-42*scale);
  x=std::clamp(x*width,0.f,maxX)/width;y=std::clamp(y*height,0.f,maxY)/height;
 }
 RECT header(){std::lock_guard<std::mutex> lock(mutex);return {LONG(x*width),LONG(y*height),LONG(x*width+270*scale),LONG(y*height+42*scale)};}
 RECT panel(){std::lock_guard<std::mutex> lock(mutex);const float px=std::clamp(x*width+18*scale,0.f,std::max(0.f,width-960*scale)),py=std::clamp(y*height+46*scale,0.f,std::max(0.f,height-720*scale));return {LONG(px),LONG(py),LONG(px+960*scale),LONG(py+720*scale)};}
 RECT reset(){std::lock_guard<std::mutex> lock(mutex);return {LONG(width-66*scale),LONG(height-34*scale),LONG(width-8*scale),LONG(height-8*scale)};}
 RECT diagnostics_toggle(){std::lock_guard<std::mutex> lock(mutex);return {LONG(std::max(0.f,width-244*scale)),LONG(height-34*scale),LONG(width-76*scale),LONG(height-8*scale)};}
 RECT diagnostics(){std::lock_guard<std::mutex> lock(mutex);float s=std::min(scale,std::min(width/736.f,height/452.f));float px=std::clamp(diagnosticX*width,0.f,std::max(0.f,width-728*s)),py=std::clamp(diagnosticY*height,0.f,std::max(0.f,height-428*s));return {LONG(px),LONG(py),LONG(px+720*s),LONG(py+420*s)};}
 RECT diagnostics_header(){auto r=diagnostics();r.bottom=r.top+LONG((r.bottom-r.top)*32.f/TouchStatus::imageHeight);return r;}
 RECT diagnostics_capture(){auto r=diagnostics();const float sx=float(r.right-r.left)/TouchStatus::imageWidth,sy=float(r.bottom-r.top)/TouchStatus::imageHeight;return {LONG(r.left+535*sx),LONG(r.top+374*sy),LONG(r.left+708*sx),LONG(r.top+410*sy)};}
 static bool inside(RECT r,POINT p){return p.x>=r.left&&p.x<r.right&&p.y>=r.top&&p.y<r.bottom;}
 bool blocks_world(POINT p){return inside(header(),p)||inside(reset(),p)||inside(diagnostics_toggle(),p)||(settingsExpanded&&inside(panel(),p))||(touchStatus.visible&&inside(diagnostics(),p));}
 // Return true only for pointers owned by the expanded settings panel. A
 // gesture keeps its owner until release: moving across a panel cannot leak.
 bool pointer(POINT p,bool down){
  menuCursorX=p.x;menuCursorY=p.y;
  const bool was=held;
  if(down&&!was){press=p;dragged=false;owner=inside(diagnostics_toggle(),p)?6:touchStatus.visible&&inside(diagnostics_header(),p)?7:touchStatus.visible&&inside(diagnostics(),p)?5:inside(header(),p)?1:inside(reset(),p)?3:settingsExpanded&&inside(panel(),p)?2:4;originX=owner==7?float(diagnostics().left)/width:x;originY=owner==7?float(diagnostics().top)/height:y;if(owner!=4)touchDiagnostics.begin(p,owner);}
  if(owner==1&&down){if(std::abs(p.x-press.x)+std::abs(p.y-press.y)>5)dragged=true;if(dragged){std::lock_guard<std::mutex> lock(mutex);x=std::clamp(originX+(p.x-press.x)/width,0.f,std::max(0.f,1-270*scale/width));y=std::clamp(originY+(p.y-press.y)/height,0.f,std::max(0.f,1-42*scale/height));dirty=true;}}
  if(owner==7&&down){if(std::abs(p.x-press.x)+std::abs(p.y-press.y)>5)dragged=true;if(dragged){std::lock_guard<std::mutex> lock(mutex);diagnosticX=std::clamp(originX+(p.x-press.x)/width,0.f,1.f);diagnosticY=std::clamp(originY+(p.y-press.y)/height,0.f,1.f);}}
  if(!down&&was){if(owner==1&&!dragged&&inside(header(),p))settingsExpanded=!settingsExpanded.load();if(owner==3&&inside(reset(),p)){std::lock_guard<std::mutex> lock(mutex);x=.012f;y=.018f;dirty=true;}}
  if(!down&&was&&owner==6&&inside(diagnostics_toggle(),p)){touchStatus.cancel();touchStatus.visible=!touchStatus.visible.load();if(settingsTouchCancel)settingsTouchCancel();}
  if(!down&&was&&owner==5&&inside(diagnostics_capture(),press)&&inside(diagnostics_capture(),p)){touchStatus.restart=true;if(settingsTouchCancel)settingsTouchCancel();}
  const int current=owner;held=down;
  if(current==4&&settingsTouchPointer)settingsTouchPointer(p,down,GetTickCount64());
  touchStatus.pointer(p,down,unsigned(current),touchDiagnostics.current.load());
  if(!down)owner=0;
  return current==2||(current==0&&settingsExpanded&&inside(panel(),p));
 }
 void cancel(){held=dragged=false;owner=0;touchStatus.cancel();if(settingsTouchCancel)settingsTouchCancel();}
};
inline MenuDock menuDock;
inline std::vector<uint8_t> menu_tile(unsigned w,unsigned h,const wchar_t* label){
 std::vector<uint8_t> pixels(size_t(w)*h*4,0);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-LONG(h);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 HDC dc=CreateCompatibleDC(nullptr);void* bits=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);if(!bitmap){DeleteDC(dc);return pixels;}
 auto old=SelectObject(dc,bitmap);RECT box{0,0,LONG(w),LONG(h)};HBRUSH brush=CreateSolidBrush(RGB(27,32,35));FillRect(dc,&box,brush);DeleteObject(brush);
 auto font=CreateFontW(-int(h*.46),0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,165,30));DrawTextW(dc,label,-1,&box,DT_CENTER|DT_VCENTER|DT_SINGLELINE);GdiFlush();memcpy(pixels.data(),bits,pixels.size());for(size_t i=3;i<pixels.size();i+=4)pixels[i]=240;SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return pixels;
}
}
