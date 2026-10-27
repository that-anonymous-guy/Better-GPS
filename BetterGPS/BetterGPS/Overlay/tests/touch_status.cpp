#include <unknwn.h>
#include "../MenuDock.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace bettergps_overlay;
static void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static unsigned worldCalls=0;
static void world(POINT,bool,uint64_t){++worldCalls;}
static void save_image(const std::filesystem::path& path,const std::vector<uint8_t>& pixels){
 BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=TouchStatus::imageWidth;info.biHeight=-LONG(TouchStatus::imageHeight);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=DWORD(file.bfOffBits+pixels.size());std::ofstream out(path,std::ios::binary);out.write((const char*)&file,sizeof(file));out.write((const char*)&info,sizeof(info));out.write((const char*)pixels.data(),pixels.size());check(bool(out),"Diagnostic preview write failed");
}
int main(int argc,char** argv){try{
 check(argc==2,"Output directory required");static_assert(!touch_gpu_backend_allowed(),"Production diagnostic fixture must not enable experimental GPU replay");
 const auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);
 TouchStatus status;TouchStatusContext context;context.menu=context.foreground=context.gameplay=context.eligible=true;context.selected=3;context.name="Kenworth W900 NavPlus";context.page="HOME";context.state="power target ON";context.width=1920;context.height=1080;context.epoch=4;context.power=9;context.pageSerial=12;
 status.worker(out,context,1000);check(!std::filesystem::exists(out/"logs/touch/touch-status.log"),"Disabled diagnostics wrote a log");
 status.visible=true;status.worker(out,context,1100);check(status.snapshot().logOk&&status.snapshot().capture==1,"Capture did not start");
 const auto firstSession=status.sourceGeneration.load();status.diagnostic_event(firstSession,"geometry","hover-only observation before any new pointer press");
 status.pointer({481,530},true,4,1);status.pointer({481,390},true,4,1);status.pointer({481,390},false,4,1);status.worker(out,context,1200);
 auto snapshot=status.snapshot();check(snapshot.context.selected==3&&!snapshot.valid&&snapshot.owner==4&&!snapshot.down,"Selected device was fabricated as hovered, or pointer state lost");check(snapshot.result.find("Awaiting device surface + final depth")!=std::string::npos&&snapshot.projection.find("unavailable")!=std::string::npos,"Pending depth/projection state was hidden");
 status.hit(0,3,.2f,.3f,true,{481,390},1150);check(!status.snapshot().valid,"Stale sample from another gesture accepted");status.hit(1,3,std::numeric_limits<float>::quiet_NaN(),.3f,true,{481,390},1150);check(!status.snapshot().valid,"Invalid UV was shown as a hit");
 status.hit(1,3,.2f,.3f,true,{481,390},1150);check(status.snapshot().valid,"Measured sample rejected");status.pointer({100,100},true,4,2);check(!status.snapshot().valid,"A previous gesture's hit survived a new press");status.cancel();check(!status.snapshot().down,"Cancel retained pointer capture");
 save_image(out/"touch-status.bmp",status.raster(status.snapshot(),1300));
 status.restart=true;status.worker(out,context,1400);check(status.snapshot().capture==2&&!status.snapshot().valid,"Fresh capture did not reset status");status.hit(2,3,.2f,.3f,true,{100,100},1400);check(!status.snapshot().valid,"Previous capture sample was accepted");
 status.diagnostic_event(firstSession,"geometry","stale-session observation");check(status.snapshot().recent[0].find("stale-session")==std::string::npos,"Old capture diagnostic event crossed session boundary");
 check(std::filesystem::exists(out/"logs/touch/touch-status.previous.log"),"New capture failed to preserve previous log");
 for(unsigned i=0;i<2000;++i){status.pointer({LONG(i),5},i%2==0,4,3+i);if(i%20==0)status.worker(out,context,1500+i);}status.worker(out,context,4000);check(status.snapshot().full&&status.pending.size()<=TouchStatus::maxQueue,"Capture was not bounded");status.finish();
 std::ifstream file(out/"logs/touch/touch-status.previous.log");std::string text((std::istreambuf_iterator<char>(file)),{});check(text.find("backend=blocked-gpu-replay")!=std::string::npos&&text.find("stage=pointer-down")!=std::string::npos&&text.find("stage=pointer-up")!=std::string::npos&&text.find("selected=3")!=std::string::npos,"Log omitted backend, pointer edges or selected device");
 check(text.find("hover-only observation before any new pointer press")!=std::string::npos,"Hover-only acquisition was silently omitted from the capture log");
 settingsTouchPointer=&world;menuDock.init(out);menuDock.layout(1920,1080);settingsExpanded=false;touchStatus.visible=false;
 auto toggle=menuDock.diagnostics_toggle();POINT p{toggle.left+5,toggle.top+5};menuDock.pointer(p,true);menuDock.pointer(p,false);check(touchStatus.visible&&worldCalls==0&&!settingsExpanded,"Diagnostic toggle leaked a world click or expanded settings");
 auto oldPanel=menuDock.diagnostics();p={oldPanel.left+10,oldPanel.top+10};menuDock.pointer(p,true);menuDock.pointer({p.x-100,p.y+100},true);menuDock.pointer({p.x-100,p.y+100},false);check(menuDock.diagnostics().left<oldPanel.left-90&&worldCalls==0,"Diagnostic header drag failed or leaked a world click");
 auto panel=menuDock.diagnostics();p={panel.left+10,panel.top+70};menuDock.pointer(p,true);menuDock.pointer({20,600},true);menuDock.pointer({20,600},false);check(worldCalls==0,"Diagnostic panel drag leaked to world");
 auto capture=menuDock.diagnostics_capture();p={capture.left+4,capture.top+4};menuDock.pointer(p,true);menuDock.pointer(p,false);check(touchStatus.restart&&worldCalls==0,"Fresh capture leaked a world click");
 menuDock.pointer({20,600},true);menuDock.pointer({20,600},false);check(worldCalls==2,"World input was lost outside diagnostics");
 touchStatus.visible=false;settingsTouchPointer=nullptr;
 touchStatus.visible=true;touchStatus.worker(out/"restartable",context,4500);TouchDiagnostics legacy;
 for(unsigned i=0;i<70;++i){const auto trace=legacy.begin({10,20});touchStatus.pointer({10,20},true,4,trace);legacy.note(TouchStage::ActionRejected,"after-legacy-cap");touchStatus.pointer({10,20},false,4,trace);}
 check(touchStatus.snapshot().result.find("after-legacy-cap")!=std::string::npos,"Restartable capture stopped at the old 64-gesture diagnostic limit");touchStatus.visible=false;touchStatus.finish();
 TouchStatus failed;failed.visible=true;failed.worker(out/"touch-status.bmp",context,5000);check(!failed.snapshot().logOk,"Unwritable capture path was reported successful");failed.finish();
 std::cout<<"PASS: production GPU replay blocked; opt-in logs, real pointer ownership, explicit missing projection, stale/invalid samples, bounded capture rotation, diagnostic HUD raster, menu click isolation and log failure reporting.\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
