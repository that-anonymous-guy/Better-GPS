#pragma once
#include <Windows.h>
#include <atomic>
#include <array>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
namespace bettergps_overlay {
// Production never re-enables the shader replay that crashed with Snowymoon.
// The existing isolated GPU fixtures must opt in at compile time, not via UI.
inline constexpr bool touch_gpu_backend_allowed(){
#if defined(BETTERGPS_TOUCH_TEST) && !defined(BETTERGPS_TOUCH_CPU_ONLY)
 return true;
#else
 return false;
#endif
}
struct TouchStatusContext {
 bool menu=false,foreground=false,gameplay=false,eligible=false;
 int selected=-1,width=0,height=0;uint64_t epoch=0,power=0,pageSerial=0;
 std::string name="None",page="Unavailable",state="No selected device";
 bool operator==(const TouchStatusContext& b)const{return menu==b.menu&&foreground==b.foreground&&gameplay==b.gameplay&&eligible==b.eligible&&selected==b.selected&&width==b.width&&height==b.height&&epoch==b.epoch&&power==b.power&&pageSerial==b.pageSerial&&name==b.name&&page==b.page&&state==b.state;}
};
struct TouchStatusSnapshot {
 TouchStatusContext context;POINT cursor{},sampleCursor{};bool down=false,valid=false,full=false,logOk=false;
 unsigned owner=0,surface=~0u,dropped=0;float u=0,v=0;uint64_t trace=0,sampleTrace=0,sampleAt=0,capture=0;
 POINT candidateCursor{};uint64_t candidateAt=0;
 std::array<std::string,4> recent{};std::string result="No gesture yet",projection="unavailable - no guessed hit",probe="Hovered device / local UV / hitbox: UNAVAILABLE";
};
struct TouchStatus {
 static constexpr unsigned imageWidth=720,imageHeight=420,maxRecords=1024,maxQueue=128;
 std::atomic<bool> visible{false},restart{false};std::atomic<unsigned> contentionDrops{0};
 std::atomic<uint64_t> sourceGeneration{0};
 std::mutex mutex,imageMutex;TouchStatusSnapshot state;std::deque<std::string> pending;
 std::shared_ptr<const std::vector<uint8_t>> image;uint64_t imageVersion=0;
 std::ofstream output;std::filesystem::path root;uint64_t nextCapture=0,lastPaint=0,revision=0,painted=~uint64_t(0),captureTrace=0;unsigned records=0;bool wasVisible=false,contextLogged=false;
 static const char* owner_name(unsigned n){return n==1?"header":n==2?"settings":n==3?"reset":n==5?"diagnostics":n==6?"diagnostics-toggle":n==7?"diagnostics-header":"world";}
 void enqueue_locked(std::string line){
  if(records>=maxRecords){state.full=true;return;}if(pending.size()>=maxQueue){++state.dropped;return;}
  ++records;pending.push_back(std::move(line));
 }
 void event_locked(const char* stage,const char* detail){
  char prefix[112];snprintf(prefix,sizeof(prefix),"tick=%llu trace=%llu stage=%s ",GetTickCount64(),state.trace,stage);
  std::string line=prefix;line+=detail;std::replace(line.begin(),line.end(),'\n',' ');std::replace(line.begin(),line.end(),'\r',' ');
  enqueue_locked(line);for(unsigned i=3;i>0;--i)state.recent[i]=state.recent[i-1];state.recent[0]=std::string(stage)+": "+detail;++revision;
 }
 void pointer(POINT p,bool held,unsigned owner,uint64_t trace){
  if(!visible)return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock){++contentionDrops;return;}
  const bool edge=state.down!=held;if(p.x!=state.cursor.x||p.y!=state.cursor.y||edge||owner!=state.owner)++revision;
  state.cursor=p;state.owner=owner;state.trace=trace;state.down=held;
  if(edge){char line[160];snprintf(line,sizeof(line),"client=%ld,%ld owner=%s",p.x,p.y,owner_name(owner));if(held){state.valid=false;state.sampleAt=0;state.result=owner==4?"Awaiting device surface + final depth sample":"Pointer owned by menu controls";}event_locked(held?"pointer-down":"pointer-up",line);}
 }
 void note(uint64_t trace,const char* stage,const char* detail,bool outcome=false){
  if(!visible)return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock){++contentionDrops;return;}if(trace!=state.trace||trace<=captureTrace)return;
  event_locked(stage,detail);if(outcome)state.result=std::string(stage)+": "+detail;
 }
 void diagnostic_event(uint64_t session,const char* stage,const char* detail){
  if(!visible||session!=sourceGeneration.load())return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock){++contentionDrops;return;}
  if(session!=sourceGeneration.load())return;event_locked(stage,detail);
 }
 void hit(uint64_t trace,unsigned slot,float u,float v,bool valid,POINT cursor,uint64_t at){
  if(!visible)return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock){++contentionDrops;return;}if(trace!=state.trace||trace<=captureTrace)return;
  state.valid=valid&&std::isfinite(u)&&std::isfinite(v)&&u>=0&&u<=1&&v>=0&&v<=1;state.surface=slot;state.u=u;state.v=v;state.sampleCursor=cursor;state.sampleAt=at;state.sampleTrace=trace;++revision;
 }
 void cancel(){if(!visible)return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock){++contentionDrops;return;}if(state.down||state.valid){state.down=false;state.valid=false;event_locked("cancel","pointer capture cancelled");}}
 TouchStatusSnapshot snapshot(){std::lock_guard<std::mutex> lock(mutex);auto result=state;result.dropped+=contentionDrops.load();return result;}
 void candidate(POINT cursor,uint64_t at){if(!visible)return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(lock){state.candidateCursor=cursor;state.candidateAt=at;}}
 void geometry(std::string projection,std::string probe){std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock)return;if(state.projection!=projection||state.probe!=probe){state.projection=std::move(projection);state.probe=std::move(probe);++revision;}}
 void begin_capture(const std::filesystem::path& base){
  ++sourceGeneration;
  // Worker only. Sequence never resets; old asynchronous samples cannot become current.
  std::deque<std::string> previousLines;{std::lock_guard<std::mutex> lock(mutex);previousLines.swap(pending);}
  if(output.is_open()){for(const auto& line:previousLines)output<<line<<'\n';output<<"capture-end\n";output.flush();}output.close();output.clear();root=base/"logs/touch";
  std::error_code ec;std::filesystem::create_directories(root,ec);
  const auto file=root/"touch-status.log",previous=root/"touch-status.previous.log";
  if(!ec&&std::filesystem::exists(file,ec))std::filesystem::copy_file(file,previous,std::filesystem::copy_options::overwrite_existing,ec);
  if(!ec)output.open(file,std::ios::trunc);
  std::lock_guard<std::mutex> lock(mutex);pending.clear();records=0;contextLogged=false;captureTrace=state.trace;state.capture=++nextCapture;state.full=false;state.dropped=0;contentionDrops=0;state.valid=false;state.sampleAt=0;state.recent={};state.result="No gesture in this capture";state.logOk=output.is_open()&&output.good();++revision;
  if(output.is_open()){SYSTEMTIME utc{};GetSystemTime(&utc);output<<"BetterGPS touch status v1 capture="<<state.capture<<" utc="<<utc.wYear<<'-'<<utc.wMonth<<'-'<<utc.wDay<<'T'<<utc.wHour<<':'<<utc.wMinute<<':'<<utc.wSecond<<"Z pid="<<GetCurrentProcessId()<<" backend="<<(touch_gpu_backend_allowed()?"isolated-test-gpu":"blocked-gpu-replay")<<" geometry_backend=read-only-cpu projection=unverified-candidates max_records="<<maxRecords<<"\n";output.flush();state.logOk=output.good();}
 }
 std::vector<uint8_t> raster(const TouchStatusSnapshot& s,uint64_t now)const{
  std::vector<uint8_t> pixels(size_t(imageWidth)*imageHeight*4,0);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=imageWidth;info.bmiHeader.biHeight=-LONG(imageHeight);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
  HDC dc=CreateCompatibleDC(nullptr);void* bits=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);if(!bitmap){DeleteDC(dc);return pixels;}auto old=SelectObject(dc,bitmap);RECT all{0,0,LONG(imageWidth),LONG(imageHeight)};auto brush=CreateSolidBrush(RGB(20,27,31));FillRect(dc,&all,brush);DeleteObject(brush);
  auto font=CreateFontW(-19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Consolas");auto oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
  auto text=[&](const std::string& value,int y,COLORREF color=RGB(230,235,238)){SetTextColor(dc,color);RECT row{12,y,LONG(imageWidth)-12,y+23};DrawTextA(dc,value.c_str(),int(value.size()),&row,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);};
  text("BETTERGPS / TOUCH DIAGNOSTICS  (drag header)",9,RGB(255,174,48));
  text(touch_gpu_backend_allowed()?"Backend: isolated GPU test (NOT live compatibility proof)":"Backend: CPU geometry + copy mapping (no replay)",36,RGB(255,177,108));
  text("Geometry: "+s.projection,59,RGB(255,177,108));
  char line[192];snprintf(line,sizeof(line),"Mode=%s  focus=%s  gameplay=%s  init=%s",s.context.menu?"CTRL F9":"closed",s.context.foreground?"yes":"no",s.context.gameplay?"yes":"no",s.context.eligible?"yes":"no");text(line,86);
  snprintf(line,sizeof(line),"Mouse: %ld,%ld  %s  owner=%s",s.cursor.x,s.cursor.y,s.down?"DOWN":"UP",owner_name(s.owner));text(line,109);
  text("Selected (NOT hovered): "+std::to_string(s.context.selected)+" / "+s.context.name,132);
  text("Selected state: "+s.context.page+" / "+s.context.state,155);
  snprintf(line,sizeof(line),"Client=%dx%d epoch=%llu power=%llu page=%llu",s.context.width,s.context.height,s.context.epoch,s.context.power,s.context.pageSerial);text(line,178);
  if(s.valid&&s.sampleAt){snprintf(line,sizeof(line),"LAST sample: slot=%u uv=%.4f,%.4f age=%llums",s.surface,s.u,s.v,now>=s.sampleAt?now-s.sampleAt:0);text(line,201);}
  else text(s.probe,201,RGB(255,177,108));
  text("Result: "+s.result,224);
  for(unsigned i=0;i<4;++i)text(s.recent[i],249+int(i)*23,RGB(167,189,201));
  snprintf(line,sizeof(line),"Capture #%llu  log=%s  dropped=%u %s",s.capture,s.logOk?"OK":"UNAVAILABLE",s.dropped,s.full?"FULL - start new capture":"");text(line,348,s.logOk?RGB(164,214,174):RGB(255,130,100));
  text("Device: copy + depth / pink=candidate green=hit",375,RGB(167,189,201));
  SetTextColor(dc,RGB(255,174,48));RECT button{535,374,708,410};DrawTextA(dc,"New capture",-1,&button,DT_CENTER|DT_VCENTER|DT_SINGLELINE);FrameRect(dc,&button,(HBRUSH)GetStockObject(GRAY_BRUSH));GdiFlush();memcpy(pixels.data(),bits,pixels.size());for(size_t i=3;i<pixels.size();i+=4)pixels[i]=244;
  SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return pixels;
 }
 void worker(const std::filesystem::path& base,const TouchStatusContext& context,uint64_t now){
  const bool on=visible.load();if(on&&(!wasVisible||restart.exchange(false)))begin_capture(base);
  if(!on){if(wasVisible)finish();return;}wasVisible=true;
  std::deque<std::string> lines;uint64_t currentRevision=0;TouchStatusSnapshot copy;
  {std::lock_guard<std::mutex> lock(mutex);if(!contextLogged||!(state.context==context)){contextLogged=true;state.context=context;char detail[360];snprintf(detail,sizeof(detail),"selected=%d name=%s page=%s state=%s mode=%u focus=%u gameplay=%u init=%u client=%dx%d epoch=%llu power=%llu page_serial=%llu",context.selected,context.name.c_str(),context.page.c_str(),context.state.c_str(),unsigned(context.menu),unsigned(context.foreground),unsigned(context.gameplay),unsigned(context.eligible),context.width,context.height,context.epoch,context.power,context.pageSerial);event_locked("context",detail);}lines.swap(pending);copy=state;copy.dropped+=contentionDrops.load();currentRevision=revision;}
  if(output.is_open()&&!lines.empty()){for(const auto& line:lines)output<<line<<'\n';output.flush();if(!output){std::lock_guard<std::mutex> lock(mutex);state.logOk=false;++revision;}}
  if(now-lastPaint<100||currentRevision==painted&&now-lastPaint<1000)return;
  auto pixels=std::make_shared<const std::vector<uint8_t>>(raster(copy,now));{std::lock_guard<std::mutex> lock(imageMutex);image=std::move(pixels);++imageVersion;}lastPaint=now;painted=currentRevision;
 }
 void finish(){++sourceGeneration;std::deque<std::string> lines;{std::lock_guard<std::mutex> lock(mutex);lines.swap(pending);}if(output.is_open()){for(const auto& line:lines)output<<line<<'\n';output<<"capture-end\n";output.flush();}output.close();wasVisible=false;}
};
inline TouchStatus touchStatus;
}
