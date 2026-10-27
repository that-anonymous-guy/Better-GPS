// Test the actual renderer and GPU substitution without a running game.
#include "../BetterGPSOverlay.cpp"
#include <stdexcept>
#include <iostream>
#include <future>
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void Save(const std::filesystem::path& path,const std::vector<uint8_t>& pixels,UINT w,UINT h){
    BITMAPFILEHEADER file{};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+static_cast<DWORD>(pixels.size());
    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=w;info.biHeight=-static_cast<LONG>(h);info.biPlanes=1;info.biBitCount=32;info.biSizeImage=static_cast<DWORD>(pixels.size());
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());Check(out.good(),"Preview write failed");
}
#include "native_map_fork_fixture.h"
#include "f150_horizontal_fixture.h"
#include "managed_device_fixture.h"
#include "device_capacity_fixture.h"
#include "managed_phone_controls_fixture.h"
#include "phone_direct_fixture.h"
#include "bronco_fixture.h"
#include "garmin_fixture.h"
#include "navplus_fixture.h"
#include "navplus_t680_fixture.h"
#include "smartnav_fixture.h"
#include "t680_pan_fixture.h"
#include "copilot_fixture.h"
#include "copilot_menu_fixture.h"
#include "copilot_slide_fixture.h"
#include "t680_gauges_fixture.h"
#include "t680_camera_fixture.h"
#include "t680_clock_fixture.h"
#include "calculator_fixture.h"
#include "gallery_fixture.h"
#include "immersive_fixture.h"
#include "deskclock_fixture.h"
#include "game_audio_fixture.h"
#include "copilot_background_apps_fixture.h"
#include "cursor_only_fixture.h"
#include "game_timezone_fixture.h"
#include "copilot_startup_gpu_fixture.h"
#include "t680_cluster_fixture.h"
#include "mack_cluster_fixture.h"
#include "mack_fixture.h"
#include "vnl2014_fixture.h"
#include "volvo18_fixture.h"
#include "sygic_fixture.h"
#include "icc5_fixture.h"
#include "zonar_fixture.h"
#include "international_fixture.h"
#include "startup_guards_fixture.h"
#include "startup_performance_fixture.h"
#include "phone_style_cleanup_fixture.h"
#include "nuvi_installed_fixture.h"
#include "garmin_quirks_fixture.h"
#include "native_player_material_fixture.h"
#include "installed_layouts_fixture.h"
#include "device_pages_fixture.h"
#include "touchscreen_fixture.h"
#include "touch_status_gpu_fixture.h"
#include "touch_geometry_fixture.h"
int main(int argc,char** argv){try{
    if(argc==3&&std::string(argv[2])=="--device-capacity"){device_capacity_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--international"){international_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--zonar"){zonar_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--startup-performance"){startup_performance_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--startup-guards"){startup_guards_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--icc5"){icc5_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--sygic"){sygic_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--volvo18"){volvo18_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--vnl2014"){vnl2014_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--mack"){mack_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--mack-cluster"){mack_cluster_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--t680-cluster"){t680_cluster_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--copilot-startup-gpu"){copilot_startup_gpu_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--phone-style-cleanup"){phone_style_cleanup_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--touch-geometry"){touch_geometry_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--touch-status"){touch_status_gpu_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--touchscreen"){touchscreen_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--touch-actions"){touchscreen_fixture::actions(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--device-pages"){device_pages_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--navplus"){navplus_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--navplus-t680"){navplus_t680_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--smartnav"){smartnav_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--copilot-slide"){copilot_slide_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--t680-gauges"){t680_gauges_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--copilot-background-apps"){copilot_background_apps_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--game-audio"){game_audio_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--deskclock"){deskclock_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--t680-clock"){t680_clock_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--immersive"){immersive_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--gallery"){gallery_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--gallery-live"){gallery_fixture::run(std::filesystem::absolute(argv[1]),true);return 0;}
    if(argc==3&&std::string(argv[2])=="--calculator"){calculator_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--t680-camera"){t680_camera_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--copilot-menu"){copilot_menu_fixture::run(std::filesystem::absolute(argv[1]));copilot_slide_fixture::run(std::filesystem::absolute(argv[1])/"gpu");return 0;}
    if(argc==3&&std::string(argv[2])=="--copilot"){copilot_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--nuvi-installed"){nuvi_installed_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--installed-layouts"){installed_layouts_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--native-player-material"){native_player_material_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--garmin-quirks"){garmin_quirks_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--garmin"){garmin_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--video-timing"){
        struct VideoCase{const char* path;unsigned width,height,frames;double seconds;};
        const VideoCase cases[]={
            {"BetterGPS/Overlay/BetterGPS_assets/material/ui/f150_startup/f150splash_75thanniversary.mp4",1280,800,104,3.466667},
            {"BetterGPS/Overlay/BetterGPS_assets/material/ui/f150_info_startup/f150splash_cluster.mp4",1440,540,92,3.066667},
            {"BetterGPS/Overlay/BetterGPS_assets/material/ui/bronco_startup/welcome_centerstack.mp4",1280,800,270,9.0},
            {"BetterGPS/Overlay/BetterGPS_assets/material/ui/bronco_info_startup/welcome_cluster.mp4",1440,540,272,9.066667}
        };
        for(const auto& test:cases){VideoPlayback video;video.configure(std::filesystem::absolute(test.path),test.width,test.height,1);const auto start=std::chrono::steady_clock::now();video.restart();VideoFrame frame;do{std::this_thread::sleep_for(std::chrono::milliseconds(5));frame=video.snapshot();Check(std::chrono::steady_clock::now()-start<std::chrono::seconds(15),"Video playback did not reach EOS on time");}while(!frame.done);const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();Check(frame.serial+2>=test.frames,"Video playback dropped too many decoded frames");Check(std::abs(elapsed-test.seconds)<1.0,"Video playback cadence differs from media timestamps");std::cout<<test.path<<" frames="<<frame.serial<<" elapsed="<<elapsed<<"s\n";}
        std::cout<<"PASS: all F150 and Bronco MP4 files follow their media timestamps and stop at EOS.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--bronco"){bronco_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    bettergps_overlay::homeDurationSeconds=1;bettergps_overlay::phoneStartupHoldMs=1000; // Legacy timing fixtures; defaults are checked via home_init.






    if(argc==3&&std::string(argv[2])=="--device-camera-migration"){
        using namespace bettergps_overlay;const auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);
        {std::ofstream f(out/"BetterGPSMapViews.ini");f<<"[view.1]\nZoom=0.6\nOffsetX=10\nOffsetZ=-120\n";}
        map_views_init(out);Check(map_view_snapshot_for(0,1).zoom==.6&&map_view_snapshot_for(1,1).x==10&&map_view_snapshot_for(1,1).z==-120,"Legacy camera migration changed existing framing");
        gpsDisplays.resize(2);gpsDisplays[0].id="road_trip_phone";gpsDisplays[1].id="f150_infotainment";selectedDisplay=1;mapViewEditing=1;
        map_view_adjust(1,5);Check(map_view_selected_snapshot(1).x==35&&map_view_snapshot_for(0,1).x==10,"Device selection did not isolate adjustment");Check(map_views_save(),"Device camera migration failed to save");map_views_init(out);
        Check(map_view_snapshot_for(0,1).x==10&&map_view_snapshot_for(1,1).x==35&&map_view_snapshot_for(1,1).zoom==.6,"Device camera reload lost migrated values");
        selectedDisplay=0;map_view_reset();Check(map_view_snapshot_for(0,1).zoom==mapViewDefaults[1].zoom&&map_view_snapshot_for(1,1).x==35,"Reset leaked into another device");
        std::cout<<"PASS: legacy camera framing migrated without visual reset; selected-device adjustment, persistence, reload and reset isolated.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--f150-hardware-capture"){f150_horizontal_fixture::run(std::filesystem::absolute(argv[1]),true,true);return 0;}
    if(argc==3&&std::string(argv[2])=="--f150-real-capture"){f150_horizontal_fixture::run(std::filesystem::absolute(argv[1]),true);return 0;}
    if(argc==3&&std::string(argv[2])=="--f150-horizontal"){f150_horizontal_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--voice-icons"){
        using namespace bettergps_overlay;const auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);
        std::ifstream exe("H:/SteamLibrary/steamapps/common/American Truck Simulator/bin/win_x64/amtrucks.exe",std::ios::binary|std::ios::ate);Check(bool(exe),"ATS exe missing");std::vector<uint8_t> raw(size_t(exe.tellg()));exe.seekg(0);exe.read((char*)raw.data(),raw.size());auto* nt=(IMAGE_NT_HEADERS64*)(raw.data()+((IMAGE_DOS_HEADER*)raw.data())->e_lfanew);std::vector<uint8_t> image(nt->OptionalHeader.SizeOfImage);memcpy(image.data(),raw.data(),nt->OptionalHeader.SizeOfHeaders);for(auto* sec=IMAGE_FIRST_SECTION(nt);sec<IMAGE_FIRST_SECTION(nt)+nt->FileHeader.NumberOfSections;++sec)memcpy(image.data()+sec->VirtualAddress,raw.data()+sec->PointerToRawData,sec->SizeOfRawData);
        const auto resolved=voice_resolve(image.data(),image.size());for(auto at:resolved)Check(at>=uintptr_t(image.data())&&at+0x130<=uintptr_t(image.data()+image.size()),"Actual ATS named voice cvar constructor unresolved");
        std::array<std::array<uint8_t,0x130>,3> roots{},overrideNodes{};for(size_t i=0;i<3;++i){memcpy(roots[i].data()+8,voiceVariableNames[i],strlen(voiceVariableNames[i])+1);voiceVariables[i]=uintptr_t(roots[i].data());}auto set=[&](size_t i,const char* value,bool overridden=false){auto* node=overridden?overrideNodes[i].data():roots[i].data();memset(node+0xb1,0,101);memcpy(node+0xb1,value,strlen(value)+1);node[0xa1]=0;uintptr_t link=overridden?uintptr_t(node):0;memcpy(roots[i].data()+0x120,&link,8);};
        memcpy(roots[0].data()+0x28,"0",2);memcpy(roots[1].data()+0x28,"0.5",4);memcpy(roots[2].data()+0x28,"0",2);set(0,"1");set(1,"0.5");set(2,"0");voice_poll(1000);Check(!navigationVoiceMuted,"Audible navigation was muted");set(1,"0",true);voice_poll(1200);Check(navigationVoiceMuted,"Zero voice volume not detected");set(1,"0.5");set(0,"0",true);voice_poll(1400);Check(navigationVoiceMuted,"Disabled voice not detected");set(0,"1");set(2,"1",true);voice_poll(1600);Check(navigationVoiceMuted,"Navigation mute not detected");set(2,"0");voice_poll(1800);Check(!navigationVoiceMuted,"Unmute did not restore live voice icon");auto before=navigationVoiceRevision.load();uintptr_t invalid=1;memcpy(roots[1].data()+0x120,&invalid,8);voice_poll(2000);Check(!navigationVoiceMuted&&navigationVoiceRevision==before,"Transient invalid profile node changed icon");set(1,"0.75");voice_poll(2200);Check(!navigationVoiceMuted,"Profile reload did not recover");roots[0][0xa1]=1;voice_poll(2400);Check(navigationVoiceMuted,"Reset-to-default flag ignored");roots[0][0xa1]=0;voice_poll(2600);Check(!navigationVoiceMuted,"Live value did not override disabled default");
        g_haveGameTime=g_haveNavTime=true;g_multiplayerTimeOffset=0;g_gameTimeMinutes=2*1440+11*60+50;g_navTimeSec=16*60;Check(current_arrival_text()=="Wed 12:06"&&current_arrival_text(false)=="12:06","Noon ETA format incorrect");g_gameTimeMinutes=2*1440+23*60+50;Check(current_arrival_text()=="Thu 12:06"&&current_arrival_text(false)=="Thu 12:06","Next-day ETA lacks weekday");g_gameTimeMinutes=6*1440+23*60+50;Check(current_arrival_text(false)=="Mon 12:06","ETA week rollover incorrect");g_gameTimeMinutes=2*1440+12*60;g_navTimeSec=3600;Check(current_arrival_text(false)=="1:00","ETA lost 12-hour clock");

        for(size_t style:{size_t(0),size_t(1)})for(bool dark:{false,true})for(bool trip:{false,true}){displayStyleIndex=style;auto settings=default_overlay_settings();std::vector<uint8_t> normal,muted;navigationVoiceMuted=false;render_phone_layout(normal,360,740,trip,dark,&settings);navigationVoiceMuted=true;render_phone_layout(muted,360,740,trip,dark,&settings);Check((normal!=muted)==(style==1||trip),"Wrong mute variant or unexpected no-trip change");if(style==1&&trip){auto* p=normal.data()+(20*360+2)*4;Check(p[0]==0&&p[1]==0&&p[2]==0&&p[3]==255,"Waze nav status bar not opaque black");}Save(out/(std::to_string(style)+(trip?"-nav-":"-notrip-")+(dark?"dark":"light")+"-mute.bmp"),muted,360,740);}
        std::cout<<"PASS: actual ATS named constructor resolution, live override chains, disabled/zero-volume/mute/unmute, invalid/reloaded profile nodes, Google/Waze day/night mute resources and black Waze nav status bar.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--waze"){
        using namespace bettergps_overlay;auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);Check(load_phone_fonts(),"Fonts missing");
        displayBase=out/"settings";std::filesystem::create_directories(displayBase/"display-settings");overlay_load(displayBase/"BetterGPSOverlay.cfg");GpsDisplay d;d.id="test_phone";d.settings=overlay_snapshot();gpsDisplays.push_back(d);overlaySettingsPath=display_settings_path(0);overlay_adjust(3,0,7);Check(overlay_save(),"Google placement save failed");
        display_style_cycle(1);Check(waze_style()&&overlay_snapshot().items[3].x==0,"Waze inherited Google placements");overlay_adjust(3,0,9);Check(overlay_save(),"Waze placement save failed");display_style_cycle(-1);Check(!waze_style()&&overlay_snapshot().items[3].x==7,"Google placement lost");display_style_cycle(1);Check(overlay_snapshot().items[3].x==9,"Waze placement lost");
        g_haveTruckSpeed=g_haveSpeedLimit=g_haveNavDistance=g_haveNavTime=g_haveGameTime=true;g_truckSpeedMps=23;g_speedLimitMps=24.5872f;g_navDistanceM=22000;g_navTimeSec=1800;g_gameTimeMinutes=14*60+10;
        const bettergps_navigation::Display cases[]={{"TURN_RIGHT","turn right in 450 ft onto Navy Dr; then left","APPROACH",true},{"MERGE_LEFT","merge left in 1.2 mi onto I-5 NORTH toward Sacramento (exit 32); then right","APPROACH",true},{"KEEP_RIGHT","Continue for 44.7 mi on CA-99 NORTH | keep right in 44.7 mi on CA-99 NORTH; then stay","CRUISE",true}};
        for(int dark=0;dark<2;++dark)for(int trip=0;trip<2;++trip){g_darkMode=dark;auto settings=default_overlay_settings();std::vector<uint8_t> pixels;render_phone_layout(pixels,360,740,trip,dark,&settings);auto place=[&](size_t item,const auto& layer){overlay_blit(pixels,layer,360,740,attached_placement(settings,item,{0,0,360,740}));};std::vector<uint8_t> layer;
          if(trip){render_waze_guidance(layer,cases[0]);place(3,layer);render_waze_footer(layer,dark);place(dark?7:6,layer);}render_waze_speed(layer);place(overlayModuleCount,layer);std::vector<uint8_t> background(360*740*4);for(size_t k=0;k<background.size();k+=4){background[k]=dark?59:242;background[k+1]=dark?48:241;background[k+2]=dark?38:236;background[k+3]=255;}overlay_blit(background,pixels,360,740,{0,0,360,740});Save(out/(std::string(trip?"nav-":"notrip-")+(dark?"dark":"light")+".bmp"),background,360,740);}
        for(int i=0;i<3;++i){std::vector<uint8_t> pixels;render_waze_guidance(pixels,cases[i],std::filesystem::absolute("validation/gps-asset-cache/phone-shields").wstring());Save(out/("instruction-"+std::to_string(i)+".bmp"),pixels,360,740);}
        for(int dark=0;dark<2;++dark){auto palette=read_native_palette(std::filesystem::path("BetterGPS/Overlay/BetterGPS_assets/native_themes")/(dark?"waze_dark":"waze_light")/"colour_data.sui");Check(palette.colors.count("navigation_current_color")&&palette.areas.size()==palette.discovered.size()&&!palette.areas.empty(),"Waze native palette invalid");}
        Check(native_icons_load("BetterGPS/Overlay/BetterGPS_assets"),"Map icon variants unavailable");for(const auto& pair:nativeIcons)Check(!pair.images[3].pixels.empty()&&!pair.images[4].pixels.empty(),"Waze icon variant missing");
        ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Waze GPU device failed");
        for(size_t index=0;index<nativeIcons.size();++index){const auto& original=nativeIcons[index].images[0];D3D11_TEXTURE2D_DESC desc{};desc.Width=original.width;desc.Height=original.height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=original.format;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{original.pixels.data(),original.width*4,0};ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&view)),"Waze GPU source failed");
          for(size_t style:{size_t(0),size_t(1),size_t(0),size_t(1)})for(bool dark:{false,true}){displayStyleIndex=style;auto* chosen=native_icon_select(view,unsigned(index+1),dark);Check(chosen,"Waze GPU variant failed");const auto& expected=nativeIcons[index].images[(style?3:0)+(dark?1:0)];ID3D11Resource* resource=nullptr;chosen->GetResource(&resource);auto staged=desc;staged.Width=expected.width;staged.Height=expected.height;staged.Format=expected.format;staged.Usage=D3D11_USAGE_STAGING;staged.BindFlags=0;staged.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* stage=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&staged,nullptr,&stage)),"Waze staging failed");context->CopyResource(stage,resource);D3D11_MAPPED_SUBRESOURCE read{};Check(SUCCEEDED(context->Map(stage,0,D3D11_MAP_READ,0,&read)),"Waze readback failed");for(unsigned y=0;y<expected.height;++y)Check(memcmp((uint8_t*)read.pData+y*read.RowPitch,expected.pixels.data()+y*expected.width*4,expected.width*4)==0,"Waze GPU RGBA/alpha differs from supplied asset");context->Unmap(stage,0);stage->Release();resource->Release();}view->Release();texture->Release();}
        context->Release();device->Release();
        std::cout<<"PASS: Waze style, separate persistent placements, supplied modular assets in four states, instructions/shields/speed/ETA rendering, native palettes and map icon variants.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--waze-fork"){bettergps_overlay::displayStyleIndex=1;fork_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--lockscreen-font-preview"){
        using bettergps_overlay::startup_style2_clock;
        Check(startup_style2_clock(16*60+27)==std::array<std::wstring,2>{L"04",L"27"},"Style 2 afternoon format incorrect");
        Check(startup_style2_clock(0)==std::array<std::wstring,2>{L"12",L"00"},"Style 2 midnight format incorrect");
        Check(startup_style2_clock(12*60+5)==std::array<std::wstring,2>{L"12",L"05"},"Style 2 noon format incorrect");
        Check(startup_style2_clock(60+7)==std::array<std::wstring,2>{L"01",L"07"},"Style 2 leading zero missing");

        using namespace bettergps_overlay;auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);Check(load_phone_fonts(),"Bundled clock fonts did not register");
        for(auto* face:{L"Chonburi",L"Zen Tokyo Zoo",L"Roboto Light"}){auto dc=CreateCompatibleDC(nullptr);auto font=CreateFontW(-80,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,face);auto old=SelectObject(dc,font);wchar_t actual[128]{};GetTextFaceW(dc,128,actual);Check(wcscmp(face,actual)==0,"Clock font substituted by Windows");SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);}
        home_init(out);{std::lock_guard<std::mutex> guard(homeMutex);homeRoot=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets/material/ui/phone_lockscreen");homeShuffleMode=0;}home_scan();std::ofstream metrics(out/"metrics.txt");
        for(int style=0;style<2;++style){home_clock_style(style);{std::lock_guard<std::mutex> guard(homeMutex);homeSelected={"iphone_lock","iphone_lock","mustang_2","mustang_2"};++homeRevision;}home_prepare();for(int variant=style?0:1;variant<=(style?2:1);++variant){std::vector<uint8_t> pixels(360*740*4,0);for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;render_phone_startup(pixels,1800,2*1440+16*60+27,false,variant);auto name=style?"style-2-"+std::to_string(variant):std::string("style-1-reference");Save(out/(name+".bmp"),pixels,360,740);std::vector<uint8_t> mask(360*740);auto type=startup_typography(style,variant);startup_clock_mask(mask,2*1440+16*60+27,style,variant);int left=360,top=740,right=0,bottom=0;for(int y=0;y<740;++y)for(int x=0;x<360;++x)if(mask[y*360+x]){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}Check(right>left&&bottom>top,"Clock glyph coverage is empty");metrics<<name<<" ink="<<right-left+1<<'x'<<bottom-top+1<<" at="<<left<<','<<top<<" horizontalScale="<<type.horizontalScale<<'\n';}}
        home_clock_style(1);std::vector<uint8_t> longDate(360*740*4,0);for(size_t i=3;i<longDate.size();i+=4)longDate[i]=255;render_phone_startup(longDate,1800,9000LL*7*1440+67,false);Save(out/"style-2-week9000plus.bmp",longDate,360,740);Check(home_save(),"Lockscreen clock style save failed");home_init(out);Check(homeClockFont==1,"Style 2 clock style lost after reload");settingsOpen=true;settings_hit(600,670);settingsSearch=L"Clock font style";settingsFirst=0;settings_hit(450,190);Check(homeClockFont==0,"Style 1 font button missed");settings_hit(750,190);Check(homeClockFont==1,"Style 2 font button missed");std::vector<uint8_t> menu;settingsSearch.clear();settings_render(menu,2,false);Save(out/"font-settings.bmp",menu,1920,1440);settingsOpen=false;std::cout<<"PASS: real embedded fonts selected without substitution, both clock styles rendered, three Style 2 sizes, long centered week, persisted font selection and live menu buttons.\n";return 0;
    }

    if(argc==3&&std::string(argv[2])=="--map-gpu-audit"){
        using namespace bettergps_overlay;auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);mapGpuAudit.base=out;mapGpuAudit.trigger=out/"capture.once";{std::ofstream file(mapGpuAudit.trigger);file<<"one session";}mapGpuAudit.active=true;mapGpuAudit.origin=GetTickCount64()-8000;displayIgnition=1;
        ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Audit GPU unavailable");D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=16;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;std::array<uint8_t,16*16*4> pixels{};for(size_t i=0;i<pixels.size();i+=4){pixels[i]=210;pixels[i+1]=50;pixels[i+2]=17;pixels[i+3]=255;}D3D11_SUBRESOURCE_DATA data{pixels.data(),16*4,0};ID3D11Texture2D* texture=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)),"Audit source unavailable");mapGpuAudit.capture(context,texture,0);Check(mapGpuAudit.images[0].requested,"Audit did not request the GPU readback");context->Flush();for(int i=0;i<2000&&mapGpuAudit.images[0].stage;++i){mapGpuAudit.poll(context);Sleep(1);}Check(!mapGpuAudit.images[0].stage,"Audit GPU readback did not become ready");mapGpuAudit.flush();auto bytes=display_bytes(out/"private-map-sample.bmp");Check(bytes.size()==54+pixels.size()&&bytes[54]==17&&bytes[55]==50&&bytes[56]==210&&bytes[57]==255,"Audit BMP did not retain actual GPU colour/alpha");mapGpuAudit.origin=GetTickCount64()-31000;mapGpuAudit.flush();Check(!mapGpuAudit.active&&!std::filesystem::exists(mapGpuAudit.trigger),"Audit did not stop after one capture session");mapGpuAudit.stop();texture->Release();context->Release();device->Release();std::cout<<"PASS: actual WARP texture copied through nonblocking staging, GPU pixels and alpha preserved in BMP, one-session timeout and trigger removal. This validates the capture tool, not the ATS map.\n";return 0;
    }

    if(argc==3&&std::string(argv[2])=="--scoped-map-icons"){
        using namespace bettergps_overlay;std::filesystem::create_directories(argv[1]);Check(native_icons_load(std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets")),"Private icon catalog missing");Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Scoped icon hooks failed");ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Scoped icon WARP failed");g_assetsReady=true;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=1024;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;ID3D11Texture2D *privateTexture=nullptr,*nativeTexture=nullptr;ID3D11RenderTargetView *privateTarget=nullptr,*nativeTarget=nullptr;ID3D11ShaderResourceView* privateView=nullptr;device->CreateTexture2D(&desc,nullptr,&privateTexture);device->CreateTexture2D(&desc,nullptr,&nativeTexture);device->CreateRenderTargetView(privateTexture,nullptr,&privateTarget);device->CreateRenderTargetView(nativeTexture,nullptr,&nativeTarget);device->CreateShaderResourceView(privateTexture,nullptr,&privateView);Check(privateTarget&&nativeTarget&&privateView,"Scoped map targets missing");
        Check(!google_map_target(privateTarget),"Texture dimensions alone claimed a native map");expect_google_map_source(context);tag_google_map_source(context,privateView);Check(google_map_target(privateTarget)&&!google_map_target(nativeTarget),"Semantic source marker did not isolate private map identity");
        auto bind=[&](ID3D11ShaderResourceView* input,ID3D11ShaderResourceView* expected){context->PSSetShaderResources(0,1,&input);ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==expected,"Native/private icon binding crossed map targets");if(actual)actual->Release();};
        const float day[]={.75f,.75f,.75f,1},night[]={.0103f,.0194f,.0369f,1};
        for(unsigned index=0;index<nativeIcons.size();++index){const auto& image=nativeIcons[index].images[2];Check(!image.pixels.empty(),"Stock map icon source not retained");D3D11_TEXTURE2D_DESC d{};d.Width=image.width;d.Height=image.height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=image.format;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{image.pixels.data(),image.width*4,0};ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;device->CreateTexture2D(&d,&data,&texture);device->CreateShaderResourceView(texture,nullptr,&view);Check(view,"Stock icon view missing");unsigned tag=0;UINT bytes=sizeof(tag);texture->GetPrivateData(nativeIconTag,&bytes,&tag);Check(tag>0&&tag<=nativeIcons.size()&&nativeIcons[tag-1].images[2].pixels==image.pixels,"Stock icon identity did not resolve to its exact native pixels");
            context->OMSetRenderTargets(1,&nativeTarget,nullptr);bind(view,view);ID3D11ShaderResourceView* cached=nullptr;bytes=sizeof(cached);Check(FAILED(view->GetPrivateData(nativeIconLight,&bytes,&cached)),"Native map allocated an unused Google icon variant");
            context->OMSetRenderTargets(1,&privateTarget,nullptr);context->ClearRenderTargetView(privateTarget,day);g_darkMode=true;bind(view,native_icon_select(view,tag,false));context->ClearRenderTargetView(privateTarget,night);g_darkMode=false;bind(view,native_icon_select(view,tag,true));
            const float clears[4][4]={{.75f,.75f,.75f,1},{.0103f,.0194f,.0369f,1},{.84f,.88f,.90f,1},{.0185f,.0295f,.045f,1}};
            for(unsigned theme=0;theme<18;++theme){context->OMSetRenderTargets(1,&privateTarget,nullptr);const auto clear=native_map_clear(theme);context->ClearRenderTargetView(privateTarget,clear.data());const size_t capturedStyle=theme/2;const bool capturedDark=theme%2;displayStyleIndex=(capturedStyle+1)%3;g_darkMode=!capturedDark;bind(view,native_icon_select(view,tag,capturedDark,capturedStyle));}
            displayStyleIndex=0;
            context->OMSetRenderTargets(1,&nativeTarget,nullptr);bind(view,view);view->Release();texture->Release();
        }
        ++nativeForkSourceGeneration;Check(!google_map_target(privateTarget),"Reload retained private identity on a recycled map texture");expect_google_map_source(context);tag_google_map_source(context,privateView);Check(google_map_target(privateTarget),"Reload could not tag the new private generation");
        context->ClearState();privateView->Release();privateTarget->Release();nativeTarget->Release();privateTexture->Release();nativeTexture->Release();g_assetsReady=false;uninstall_d3d_hooks();MH_Uninitialize();context->Release();device->Release();std::cout<<"PASS: all stock icon identities preserved on native maps, no native Google allocations, exact private render-target tagging, and independent light/dark icon selection from target generation rather than global phone mode.\n";return 0;
    }

    if(argc==3&&std::string(argv[2])=="--managed-phone-controls"){phone_direct_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--managed-device"){managed_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--native-fork"){fork_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--display-assignments"){
        using namespace bettergps_overlay;auto out=std::filesystem::absolute(argv[1]),base=out/"plugin";std::filesystem::create_directories(base/"layouts/vertical");auto source=std::filesystem::absolute("BetterGPSOverlay");
        for(auto name:{"phone.sii","native-phone.sii"})std::filesystem::copy_file(source/"layouts/vertical"/name,base/"layouts/vertical"/name,std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy(source/"layouts/vertical/surfaces",base/"layouts/vertical/surfaces",std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy(source/"layouts/horizontal",base/"layouts/horizontal",std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
        auto material=std::filesystem::path("BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew");std::filesystem::create_directories(base/material);for(auto name:{"phone_layout_marker.dds","phone_layout_marker.tobj"})std::filesystem::copy_file(source/material/name,base/material/name,std::filesystem::copy_options::overwrite_existing);
        {auto bytes=display_bytes(source/"config/BetterGPSDisplays.ini");std::string text(bytes.begin(),bytes.end());auto horizontal=text.find("[display.f150_infotainment]");if(horizontal!=std::string::npos)text.resize(horizontal);std::ofstream file(base/"BetterGPSDisplays.ini");file<<text;}
        {std::ofstream f(base/"BetterGPSDisplays.ini",std::ios::app);f<<"\n[display.mod_phone]\nname=Mod phone\nui_path=/ui/custom/test.sii\nlayout=vertical\n";}
        home_init(base);display_read_registry(base);display_generate();display_activate_first();display_policy_init(base);
        Check(gpsDisplays.size()==5&&gpsDisplays[0].nativeFallback&&gpsDisplays[1].nativeFallback&&gpsDisplays[2].nativeFallback&&display_source_index(2)==0,"Physical phone split/source/native template failed");
        auto native=display_bytes(base/"generated-displays/ui/dashboard/rt_phone_gps.sii");std::string ui(native.begin(),native.end());Check(ui.find("_nameless.bettergps.nativebase")!=std::string::npos&&ui.find("_nameless._.speedlimit")!=std::string::npos,"Native interface was not retained");
        auto models=base/"generated-displays/vehicle/car/upgrade/interior_decors/set_lglass";for(auto name:{"rt_glass_phoneholder.pmd","rt_ac_phoneholder.pmd","rt_ac_phoneholder3.pmd"}){auto bytes=display_bytes(models/name);std::string binary(bytes.begin(),bytes.end());Check(binary.find(std::string(name).find("rt_ac_")==0?"road_trip_console_surface.mat":"road_trip_phone_surface.mat")!=std::string::npos,"Physical slot material misassigned");}
        car_vehicle("vehicle.ford.mustang_67","Ford Mustang");Check(display_current(false)==std::vector<size_t>({0,2})&&display_current(true).empty(),"Unknown/unequipped displays leaked into current vehicle");display_note_seen(0);display_note_seen(2);Check(display_current(true)==std::vector<size_t>({0,2}),"Observed phone slots missing");
        settingsOpen=true;settingsSearch.clear();settings_hit(830,670);Check(settings_matches()==std::vector<int>({18,100,102}),"Car specific physical controls missing");settings_hit(750,220);Check(!display_active(0)&&display_active(2)&&display_current(true)==std::vector<size_t>({2})&&display_source_active(0),"Windshield switch disabled console/native producer");settings_hit(750,270);Check(display_current(true).empty()&&!display_source_active(0),"Both phones cannot be disabled");
        settings_hit(150,670);Check(settings_matches()==std::vector<int>({21,27,0,1}),"Hidden inactive modules remained editable");settings_hit(830,670);settings_hit(450,270);Check(display_current(true)==std::vector<size_t>({2}),"Console independent enable failed");Check(display_policy_save(),"Vehicle assignments not saved");
        car_vehicle("ford.victoria_06","Crown Victoria");display_note_seen(0);display_note_seen(2);Check(display_active(0)&&display_active(2),"Mustang preferences leaked into another vehicle");car_vehicle("ford.mustang_67","Mustang");Check(display_current(true).empty()&&!display_active(0)&&display_active(2),"Vehicle restore lost preferences or retained old screen detections");display_note_seen(2);display_policy_init(base);Check(!display_active(0)&&display_active(2),"Assignments lost after reload");car_vehicle("ford.mustang_67","Mustang");Check(display_current(true).empty(),"Same-vehicle save/config reload retained equipped slots");
        car_vehicle("kenworth.w900","Kenworth");Check(display_current(false)==std::vector<size_t>({1}),"Truck registry filtering failed");display_note_seen(1);display_note_seen(3);Check(display_current(true)==std::vector<size_t>({1,3}),"Actually detected custom display was excluded");
        car_vehicle("ford.mustang_67","Ford Mustang");display_note_seen(0);display_note_seen(2);settings_hit(830,670);std::vector<uint8_t> pixels;settings_render(pixels,2);Save(out/"car-specific.bmp",pixels,1920,1440);settings_hit(150,670);settings_render(pixels,2);Save(out/"display.bmp",pixels,1920,1440);settingsOpen=false;

        map_views_init(out);settingsOpen=true;settings_hit(150,670);settingsSearch=L"GPS camera view";settingsFirst=0;Check(settings_matches()==std::vector<int>({22}),"Camera selector missing from search");settings_hit(870,190);Check(mapViewEditing==2,"Camera selector did not change editing profile");settings_hit(410,190);Check(mapViewEditing==1,"Camera selector did not restore far 3D profile");settingsSearch=L"magnification";settingsFirst=0;Check(settings_matches()==std::vector<int>({23}),"Camera zoom search missing");settings_hit(870,190);Check(map_view_snapshot(1).zoom==.8,"Camera zoom UI did not apply");settingsSearch=L"Offset X";settings_hit(870,190);Check(map_view_snapshot(1).x==5,"Camera X UI did not apply");settingsSearch=L"Offset Z";settings_hit(410,190);Check(map_view_snapshot(1).z==-5,"Camera Z UI did not apply");settingsSearch=L"Reset this view";settings_hit(600,190);Check(map_view_snapshot(1).zoom==.75&&map_view_snapshot(1).x==0&&map_view_snapshot(1).z==0,"Camera reset UI changed wrong profile");settingsSearch.clear();settingsFirst=0;
        settingsOpen=true;settings_hit(150,670);Check(settings_matches().front()==21,"Global switch is not the first Display row");settings_hit(750,190);Check(!displayGlobalEnabled&&!display_active(2)&&!display_source_active(0)&&display_current(true).empty(),"Global OFF did not bypass all phone surfaces");Check(display_policy_save(),"Global switch not saved");display_policy_init(base);Check(!displayGlobalEnabled,"Global OFF lost after reload");settings_hit(450,190);Check(displayGlobalEnabled&&!display_active(0)&&display_active(2),"Global ON overwrote individual screen preferences");Check(display_policy_save(),"Global ON save failed");settingsOpen=false;
        std::cout<<"PASS: distinct physical phone models, shared native producer, preserved native interface, independent per-vehicle switches, filtered actual displays, custom mod detection, persisted preferences, and same-vehicle save/config reload.\n";return 0;
    }

    if(argc==3&&std::string(argv[2])=="--menu-mouse"){
        using namespace bettergps_overlay;displayFixtureModifiers=0;cursorOnlyFixtureDown=0;std::filesystem::create_directories(argv[1]);
        struct Device {void** vtable;DWORD type=DI8DEVTYPE_MOUSE;int caps=0,flushes=0;HRESULT result=DI_OK;};
        std::array<void*,4> table{};table[3]=reinterpret_cast<void*>(+[](IDirectInputDevice8W* p,LPDIDEVCAPS caps)->HRESULT{auto* d=reinterpret_cast<Device*>(p);++d->caps;caps->dwDevType=d->type;return DI_OK;});Device mouse{table.data()};
        auto state=+[](IDirectInputDevice8W* p,DWORD bytes,LPVOID data)->HRESULT{auto* d=reinterpret_cast<Device*>(p);if(FAILED(d->result))return d->result;memset(data,0x3f,bytes);if(bytes>=sizeof(DIMOUSESTATE)){auto* m=static_cast<DIMOUSESTATE*>(data);m->lX=17;m->lY=-31;m->lZ=120;m->rgbButtons[0]=0x80;}return d->result;};
        auto buffered=+[](IDirectInputDevice8W* p,DWORD size,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags)->HRESULT{auto* d=reinterpret_cast<Device*>(p);if(FAILED(d->result))return d->result;if(!data){if(!(flags&DIGDD_PEEK))++d->flushes;*count=0;return d->result;}*count=std::min(*count,DWORD(2));for(DWORD i=0;i<*count;++i){auto* record=reinterpret_cast<DIDEVICEOBJECTDATA*>((uint8_t*)data+size*i);*record={};record->dwOfs=i?DIMOFS_Y:DIMOFS_X;record->dwData=17;}return d->result;};
        menuMouseStates[0]=state;menuMouseData[0]=buffered;settingsWindow=GetForegroundWindow();Check(settingsWindow!=nullptr,"No foreground window for mouse ownership fixture");settingsOpen=true;renderStopping=false;auto* device=reinterpret_cast<IDirectInputDevice8W*>(&mouse);
        DIMOUSESTATE2 value{};Check(menu_mouse_state<0>(device,sizeof(value),&value)==DI_OK&&value.lX==0&&value.lY==0&&value.lZ==0&&value.rgbButtons[0]==0,"Mouse deltas/buttons leaked while menu owned cursor");
        settingsOpen=false;cursorOnlyFixtureDown=1;menu_mouse_state<0>(device,sizeof(value),&value);Check(cursorOnlyActive&&value.lX==0&&value.lY==0&&value.rgbButtons[0]==0,"Cursor-only mode leaked mouse input to camera");cursorOnlyFixtureDown=0;cursorOnlyActive=false;settingsOpen=true;
        for(DWORD type:{DWORD(DI8DEVTYPE_JOYSTICK),DWORD(DI8DEVTYPE_KEYBOARD),DWORD(DI8DEVTYPE_DEVICE)}){mouse.type=type;menu_mouse_state<0>(device,sizeof(value),&value);Check(value.lX==17&&value.lY==-31&&value.rgbButtons[0]==0x80,"Non-mouse wheel/keyboard/HID tracking input was filtered");}
        mouse.type=DI8DEVTYPE_MOUSE;std::array<uint8_t,256> keyboard{};const auto caps=mouse.caps;menu_mouse_state<0>(device,DWORD(keyboard.size()),keyboard.data());Check(mouse.caps==caps&&keyboard[200]==0x3f,"Keyboard-sized state was inspected or filtered");
        DIDEVICEOBJECTDATA events[2]{};DWORD count=2;Check(menu_mouse_data<0>(device,sizeof(events[0]),events,&count,DIGDD_PEEK)==DI_OK&&count==0&&mouse.flushes==1,"Buffered mouse peek leaked events or retained old movement");
        mouse.type=DI8DEVTYPE_JOYSTICK;count=2;menu_mouse_data<0>(device,sizeof(events[0]),events,&count,0);Check(count==2&&events[0].dwData==17,"Buffered joystick/tracking input was filtered");mouse.type=DI8DEVTYPE_MOUSE;
        settingsOpen=false;menu_mouse_release();menu_mouse_state<0>(device,sizeof(value),&value);Check(value.lX==0,"Menu close replayed recenter motion");menu_mouse_state<0>(device,sizeof(value),&value);Check(value.lX==17&&value.lY==-31,"Camera mouse input did not resume");count=2;menu_mouse_data<0>(device,sizeof(events[0]),events,&count,0);Check(count==0,"Close replayed buffered motion");count=2;menu_mouse_data<0>(device,sizeof(events[0]),events,&count,0);Check(count==2,"Buffered input did not resume");
        const auto idleCaps=mouse.caps;for(int i=0;i<10000;++i)menu_mouse_state<0>(device,sizeof(value),&value);Check(mouse.caps==idleCaps,"Closed menu caused per-poll device inspection");settingsOpen=true;settingsWindow=nullptr;menu_mouse_state<0>(device,sizeof(value),&value);Check(value.lX==17,"Background menu captured input");settingsWindow=GetForegroundWindow();mouse.result=DIERR_INPUTLOST;value.lX=42;Check(menu_mouse_state<0>(device,sizeof(value),&value)==DIERR_INPUTLOST&&value.lX==42,"Original DirectInput error/state changed");
        settingsOpen=false;menuMouseStates={};menuMouseData={};menuMouseDrainState=menuMouseDrainData=false;
        Check(MH_Initialize()==MH_OK,"Input hook runtime initialization failed");
        for(int n=0;n<5;++n){Check(menu_mouse_install(),"Actual DirectInput mouse hook installation failed");using Factory=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);auto factory=reinterpret_cast<Factory>(GetProcAddress(menuMouseModule,"DirectInput8Create"));for(const auto* iid:{&IID_IDirectInput8W,&IID_IDirectInput8A}){IDirectInput8W* input=nullptr;Check(SUCCEEDED(factory(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,*iid,reinterpret_cast<void**>(&input),nullptr)),"Real ANSI/Wide DirectInput factory failed");IDirectInputDevice8W* actual=nullptr;Check(SUCCEEDED(input->CreateDevice(GUID_SysMouse,&actual,nullptr)),"Real mouse probe failed");Check(FAILED(actual->GetDeviceState(sizeof(value),&value)),"Unacquired native mouse error was overridden");actual->Release();input->Release();}menu_mouse_uninstall();Check(!menuMouseModule&&!menuMouseStateAddresses[0]&&!menuMouseDataAddresses[0],"Input hook lifecycle leaked resources");}
        for(int cycle=0;cycle<3;++cycle){Check(settings_install_mouse_hooks(),"Combined cursor/DirectInput batch failed");Check(settingsSetCursorAddress&&settingsClipCursorAddress&&menuMouseModule&&menuMouseStateAddresses[0]&&menuMouseDataAddresses[0],"Combined batch lost cursor or DirectInput hooks");Check(!settings_capture_mouse_input(),"Closed menu captured mouse during batched install");settings_uninstall_mouse_hooks();Check(!settingsSetCursorAddress&&!settingsClipCursorAddress&&!menuMouseModule,"Combined batch leaked hooks");}
        MH_Uninitialize();settingsWindow=nullptr;std::cout<<"PASS: menu-only DirectInput mouse capture, unchanged wheel/keyboard/HID data, immediate and buffered/peek queue handling, clean close/resume/focus/error behavior, no idle device queries, five standalone ANSI/Wide plus three combined cursor/input installation/teardown cycles without acquiring the mouse.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--f150-info"){
        using namespace bettergps_overlay;auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);auto plugin=out/"plugin",assets=plugin/"BetterGPS_assets";std::filesystem::create_directories(assets/"material/ui/f150_info_startup");std::filesystem::create_directories(assets/"material/ui/f150_startup");
        auto source=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets");for(auto folder:{"f150_info_startup","f150_startup"})std::filesystem::copy_file(source/"material/ui"/folder/"animation.bga",assets/"material/ui"/folder/"animation.bga",std::filesystem::copy_options::overwrite_existing);home_init(plugin);car_init(plugin);car_vehicle("ford.f150_23","Ford F150");Check(carInfoEnabled&&carSplashEnabled,"Splash defaults not enabled");Check(f150InfoFirst.size()==f150InfoFrameBytes&&std::any_of(f150InfoFirst.begin(),f150InfoFirst.end(),[](uint8_t x){return x!=0;}),"Cold fallback lost first animation image");
        Check(f150_info_frame(0,92)==0&&f150_info_frame(33,92)==0&&f150_info_frame(34,92)==1&&f150_info_frame(3066,92)==91&&f150_info_frame(3067,92)==-2&&std::abs(f150_info_opacity(2817,92)-.5f)<.001&&f150_info_frame(2817,92)>f150_info_frame(2567,92)&&f150_info_frame(3999,1)==0&&f150_info_frame(4000,1)==-2,"Info startup not one loop at 30 fps without delay");
        settingsOpen=true;settingsSearch.clear();settings_hit(650,670);settings_hit(870,290);Check(!carSplashEnabled&&carInfoEnabled,"Central splash Off affected info splash");settings_hit(870,340);Check(!carInfoEnabled,"Info Off control missed");Check(car_save(),"Splash switches not saved");car_init(plugin);Check(!carSplashEnabled&&!carInfoEnabled,"Splash switches not restored");settings_hit(870,290);settings_hit(870,340);Check(carSplashEnabled&&carInfoEnabled,"Independent On controls missed");std::vector<uint8_t> menu;settings_render(menu,2);Save(out/"settings.bmp",menu,1920,1440);
        {std::lock_guard<std::mutex> lock(homeImport.mutex);homeImport.folder=source/"material/ui/f150_startup";}settings_hit(915,340);Check(homeImportActive&&homeImport.layer==5,"Info image import plus missed");home_import_prepare();{std::lock_guard<std::mutex> lock(homeImport.mutex);homeImport.address=(source/"material/ui/f150_startup/Ford_simple.dds").wstring();homeImport.edit=true;}import_key(WM_KEYDOWN,VK_RETURN);home_import_prepare();Check(homeImport.page==ImportPage::Crop,"Info cropper failed");auto x=.5,y=.5;auto crop=import_crop(*homeImport.image,x,y,1,1440,540);Check(std::abs(crop.width/crop.height-1440.0/540)<.001,"Info crop is not full-cluster aspect");import_hit(350,575);home_import_prepare();Check(!homeImportActive&&carInfoSelected=="Ford_simple.bga"&&carSplashSelected=="animation.bga","Info import affected central selection");Check(car_save(),"Info selection save failed");car_init(plugin);Check(carInfoSelected=="Ford_simple.bga","Info selection not restored");car_cycle(1,true);Check(carInfoSelected=="__off__"&&!carInfoEnabled,"Info OFF choice missing");car_cycle(1,true);Check(carInfoSelected=="animation.bga","Info animation selector failed");settingsOpen=false;
        Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Info hooks failed");ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Info WARP failed");
        std::vector<uint8_t> marker(size_t(f150InfoWidth)*f150InfoHeight*4);for(unsigned i=0;i<4;++i){marker[4*i]=0xa7;marker[4*i+1]=0x5b;marker[4*i+2]=0x31+11+i;marker[4*i+3]=255;}
        D3D11_TEXTURE2D_DESC desc{};desc.Width=f150InfoWidth;desc.Height=f150InfoHeight;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)),"Info marker creation failed");g_origUpdateSubresource(context,texture,0,nullptr,marker.data(),f150InfoWidth*4,0);Check(SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&view)),"Info marker SRV failed");
        auto bind=[&](bool clear,bool faded=false){context->PSSetShaderResources(0,1,&view);ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==(clear?g_f150InfoClearView:faded?g_f150InfoFade.view:g_f150InfoBridge.liveView),"Info cold binding exposed native marker or wrong SRV");if(actual)actual->Release();};
        bind(false);Check(g_f150InfoBridge.liveView,"Cold info marker was not recovered in its first bind");
        {D3D11_TEXTURE2D_DESC d{};g_f150InfoBridge.liveTexture->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* stage=nullptr;device->CreateTexture2D(&d,nullptr,&stage);context->CopyResource(stage,g_f150InfoBridge.liveTexture);D3D11_MAPPED_SUBRESOURCE m{};Check(SUCCEEDED(context->Map(stage,0,D3D11_MAP_READ,0,&m)),"Initial info frame readback failed");for(unsigned row=0;row<f150InfoHeight/4;++row)Check(memcmp((uint8_t*)m.pData+row*m.RowPitch,f150InfoFirst.data()+row*(f150InfoWidth/4)*8,(f150InfoWidth/4)*8)==0,"Initial GPU image is black/uninitialized instead of frame zero");context->Unmap(stage,0);stage->Release();}
        f150_ignition(true);bind(false);f150InfoOrigin=GetTickCount64()-5000;bind(false);Check(!f150InfoLoadFinished,"Cold fixture already loaded");f150_info_prepare(assets);auto animation=std::atomic_load(&f150InfoAnimation);Check(animation&&animation->count==89,"Info animation not loaded");bind(false);Check(f150InfoUploaded==0,"Late preparation skipped first info frames");
        for(int n=0;n<20;++n){f150_ignition(false);bind(false);f150_ignition(true);bind(false);Check(f150InfoUploaded==0,"Ignition did not start info immediately");f150InfoOrigin=GetTickCount64()-1000;bind(false);Check(f150InfoUploaded>=30&&f150InfoUploaded<33,"Info animation frame incorrect");f150InfoOrigin=GetTickCount64()-(f150_info_end(animation->count)-250);bind(false,true);
            {D3D11_TEXTURE2D_DESC d{};g_f150InfoFade.target->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* stage=nullptr;device->CreateTexture2D(&d,nullptr,&stage);context->CopyResource(stage,g_f150InfoFade.target);D3D11_MAPPED_SUBRESOURCE m{};Check(SUCCEEDED(context->Map(stage,0,D3D11_MAP_READ,0,&m)),"Fade readback failed");auto alpha=((uint8_t*)m.pData)[3];Check(alpha>=100&&alpha<=155,"Info fade alpha not gradual transparency");context->Unmap(stage,0);stage->Release();}
            f150InfoOrigin=GetTickCount64()-f150_info_end(animation->count);bind(true);Check(f150InfoUploaded==-2,"Info loop did not finish once");bind(true);}
        g_origPSSetSRV(context,7,1,&g_f150InfoFade.view);f150InfoOrigin=GetTickCount64()-(f150_info_end(animation->count)-250);bind(false,true);ID3D11ShaderResourceView* preserved=nullptr;context->PSGetShaderResources(7,1,&preserved);Check(preserved==g_f150InfoFade.view,"Fade altered unrelated UI texture slot");preserved->Release();
        car_info_enable(false);bind(true);car_info_enable(true);bind(false);Check(f150InfoUploaded==0,"Re-enabled info splash did not restart cleanly");car_splash_enable(false);Check(carInfoEnabled,"Central Off disabled info");
        // Exact dimensions alone cannot claim unrelated dashboard textures.
        marker.assign(marker.size(),255);D3D11_SUBRESOURCE_DATA data{marker.data(),f150InfoWidth*4,0};ID3D11Texture2D* other=nullptr;ID3D11ShaderResourceView* otherView=nullptr;device->CreateTexture2D(&desc,&data,&other);device->CreateShaderResourceView(other,nullptr,&otherView);context->PSSetShaderResources(0,1,&otherView);ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==otherView,"Info hook modified an unrelated texture");actual->Release();context->ClearState();otherView->Release();other->Release();view->Release();texture->Release();uninstall_d3d_hooks();MH_Uninitialize();context->Release();device->Release();

        {std::ofstream old(carConfig);old<<"[F150]\nsplash=animation.bga\ninfo_splash=animation.bga\nsplash_enabled=0\ninfo_enabled=0\n";}car_init(plugin);Check(!carSplashEnabled&&!carInfoEnabled&&carSplashSelected=="__off__"&&carInfoSelected=="__off__","Legacy disabled switches did not migrate to OFF choices");carDirty=true;Check(car_save(),"Migrated OFF choices not saved");car_init(plugin);Check(carSplashSelected=="__off__"&&carInfoSelected=="__off__","OFF choices lost after migrated save");
        std::cout<<"PASS: independent persisted OFF selector choices, info selector/import 1440x540 crop, cold first-bind frame zero, late worker recovery, single 30fps playback across 20 ignition cycles, no repeat, 500ms overlapping alpha fade without black tail, native handoff and unrelated GPU state/textures preserved.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--car-specific"){
        using namespace bettergps_overlay;auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);auto plugin=out/"plugin";auto assets=plugin/"BetterGPS_assets/material/ui/f150_startup";std::filesystem::create_directories(assets);
        auto source=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets/material/ui/f150_startup");for(auto name:{"animation.bga","Ford_simple.bga"})std::filesystem::copy_file(source/name,assets/name,std::filesystem::copy_options::overwrite_existing);home_init(plugin);car_init(plugin);Check(carSplashes.size()==3&&carSplashes[0].name=="75th anniversary"&&carSplashSelected=="animation.bga","Splash defaults/catalog failed");
        scs_named_value_t attrs[4]{};attrs[0].name="id";attrs[0].value.type=SCS_VALUE_TYPE_string;attrs[0].value.value_string.value="ford.f150_23";attrs[1].name="name";attrs[1].value.type=SCS_VALUE_TYPE_string;attrs[1].value.value_string.value="F-150 Lariat";attrs[2].name="brand";attrs[2].value.type=SCS_VALUE_TYPE_string;attrs[2].value.value_string.value="Ford";scs_telemetry_configuration_t cfg{"truck",attrs};on_theme_session(SCS_TELEMETRY_EVENT_configuration,&cfg,nullptr);Check(carIsF150&&carVehicleName=="Ford F-150 Lariat","Actual configuration event did not detect F150");
        settingsOpen=true;settingsSearch.clear();settings_hit(650,670);Check(settingsCategory==3,"Car specific tab missed");settings_hit(870,290);Check(carSplashSelected=="Ford_simple.bga","Visible splash selector missed");Check(car_save(),"Car settings save failed");car_init(plugin);Check(carSplashSelected=="Ford_simple.bga","Selected splash lost on restart");f150Requested=true;f150_prepare(plugin/"BetterGPS_assets");auto splash=std::atomic_load(&f150Animation);Check(splash&&splash->count==1&&splash->blocks.size()==f150FrameBytes,"Selected still failed to prepare");Check(f150_frame(999,1)==-1&&f150_frame(1000,1)==0&&f150_frame(4499,1)==0&&f150_frame(4500,1)==-2,"Still black/hold/native timing failed");std::vector<uint8_t> pixels;settings_render(pixels,2);Save(out/"car-specific.bmp",pixels,1920,1440);
        {std::lock_guard<std::mutex> lock(homeImport.mutex);homeImport.folder=source;}settings_hit(915,290);Check(homeImportActive&&homeImport.layer==4,"Splash plus missed");home_import_prepare();{std::lock_guard<std::mutex> lock(homeImport.mutex);homeImport.address=(source/"Ford_simple.dds").wstring();homeImport.edit=true;}import_key(WM_KEYDOWN,VK_RETURN);home_import_prepare();Check(homeImport.page==ImportPage::Crop,"Splash image did not open cropper");auto cx=.5,cy=.5;auto crop=import_crop(*homeImport.image,cx,cy,1,1280,800);Check(std::abs(crop.width/crop.height-1.6)<.00001,"Splash crop used phone aspect");settings_render(pixels,2);Save(out/"splash-crop.bmp",pixels,1920,1440);auto homeBefore=homeSelected;import_hit(350,575);home_import_prepare();Check(!homeImportActive&&homeImport.error.empty()&&carSplashSelected=="Ford_simple_2.bga","Custom splash import/collision failed");Check(homeBefore==homeSelected,"Splash import changed lockscreen selections");f150_prepare(plugin/"BetterGPS_assets");splash=std::atomic_load(&f150Animation);Check(splash&&splash->count==1,"Imported splash did not prepare");
        car_cycle(1);Check(carSplashSelected=="__off__"&&!carSplashEnabled,"Central OFF choice missing");car_cycle(1);Check(carSplashSelected=="animation.bga","Cycle did not return to original animation");f150_prepare(plugin/"BetterGPS_assets");Check(std::atomic_load(&f150Animation)->count==52,"Original animation lost");
        attrs[0].value.value_string.value="ford.bronco_24";attrs[1].value.value_string.value="Bronco";on_theme_session(SCS_TELEMETRY_EVENT_configuration,&cfg,nullptr);Check(!carIsF150,"Vehicle switch retained F150");auto rows=settings_matches();Check(carIsBronco&&std::find(rows.begin(),rows.end(),19)!=rows.end()&&std::find(rows.begin(),rows.end(),20)!=rows.end(),"Bronco car-specific customization missing");settings_render(pixels,2);Save(out/"other-car.bmp",pixels,1920,1440);settingsOpen=false;
        std::cout<<"PASS: actual truck configuration detection/swap, car tab, visible splash select/import, default anniversary, still timing, PNG crop to 1280x800, BC1 encoding, collision/source safety, persistence and original animation retained.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--settings-ui"){
        using namespace bettergps_overlay;auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);home_init(out/"runtime");displayBase=out/"runtime";std::filesystem::create_directories(displayBase/"display-settings");GpsDisplay fixture;fixture.id="test_phone";fixture.name="Test phone";fixture.ready=true;fixture.settings=overlay_snapshot();gpsDisplays.push_back(fixture);settingsOpen=true;homeImportActive=false;
        settingsCategory=0;settingsFirst=0;settingsSearch.clear();std::vector<uint8_t> screen;auto visibleClick=[&](int id,int x){auto rows=settings_matches();auto at=std::find(rows.begin(),rows.end(),id);Check(at!=rows.end(),"Requested setting is not visible in category");int index=int(at-rows.begin());settingsFirst=std::clamp(index-3,0,std::max(0,int(rows.size())-8));settings_hit(x,190+(index-settingsFirst)*50);};
        settings_render(screen,2);Save(out/"map.bmp",screen,1920,1440);
        settings_hit(770,290);Check(overlay_snapshot().theme==2,"Visible theme control missed");
        settings_hit(150,670);Check(settingsCategory==0,"Bottom display category missed");int oldX=overlay_snapshot().items[settingsSelected].x;settings_hit(100,95);for(wchar_t c:std::wstring(L"Horizontal position"))settings_search_key(WM_CHAR,c);settings_hit(870,190);settings_hit(880,95);Check(overlay_snapshot().items[settingsSelected].x==oldX+1,"Visible position control missed");
        settings_render(screen,2);Save(out/"display.bmp",screen,1920,1440);
        settings_hit(350,670);Check(settingsCategory==1,"Bottom shortcuts category missed");visibleClick(10,500);Check(settingsCaptureMenuKey,"Visible menu shortcut control missed");settingsCaptureMenuKey=false;
        settings_hit(470,670);Check(settingsCategory==2,"Bottom phone category missed");visibleClick(8,600);Check(displayPowerMode==1,"Visible power control missed");int duration=homeDurationSeconds;visibleClick(15,870);Check(homeDurationSeconds==duration+1,"Visible duration control missed");settings_render(screen,2);Save(out/"lockscreen.bmp",screen,1920,1440);
        settings_hit(100,95);for(wchar_t c:std::wstring(L"theme"))settings_search_key(WM_CHAR,c);Check(settings_matches().size()==1&&settingsRows[settings_matches()[0]].category==0,"Search did not find another category");settings_hit(600,190);Check(overlay_snapshot().theme==1,"Search result control missed");settings_render(screen,2);Save(out/"search.bmp",screen,1920,1440);
        settings_hit(880,95);Check(settingsSearch.empty(),"Clear search missed");for(wchar_t c:std::wstring(L"not found"))settings_search_key(WM_CHAR,c);Check(settings_matches().empty(),"Empty search retained results");settings_hit(870,190);Check(overlay_snapshot().theme==1,"Empty result activated stale setting");settings_search_key(WM_KEYDOWN,VK_ESCAPE);Check(settingsSearch.empty()&&!settingsSearchEdit,"Escape did not clear search");
        settings_hit(470,670);visibleClick(13,915);Check(homeImportActive&&homeImport.layer==2,"Visible background import plus missed");import_close();settingsOpen=false;
        std::cout<<"PASS: bottom categories, actual visible theme/position/power/duration/shortcut/import controls, global live search, cross-category editing, empty results, clear and Escape.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--home-search"){
        using namespace bettergps_overlay;const auto out=std::filesystem::absolute(argv[1]),folder=out/"images";std::filesystem::create_directories(folder/"Truck garage");std::filesystem::create_directories(folder/"Other");home_init(out/"runtime");
        HomePixels pixels(360*740*4,255);for(int i=0;i<22;++i){char name[32];std::snprintf(name,sizeof(name),"truck_%02d.png",i);Check(import_png(folder/name,pixels),"Search fixture write failed");}Check(import_png(folder/std::filesystem::path(L"\u00c9t\u00e9_Paris.png"),pixels),"Unicode search fixture failed");
        settingsOpen=true;settingsLockscreen=true;{std::lock_guard<std::mutex> lock(homeImport.mutex);homeImport.folder=folder;}import_open(2);home_import_prepare();Check(homeImport.entries.size()==25&&homeImport.filtered.size()==25,"Unfiltered browser catalog lost entries");const auto names=homeImport.searchNames;
        import_hit(100,95);Check(homeImport.searchEdit&&!homeImport.edit,"Search click missed");for(wchar_t c:std::wstring(L"TRUCK"))import_key(WM_CHAR,c);Check(homeImport.job==ImportJob::Filter,"Typing did not schedule live search");home_import_prepare();Check(homeImport.filtered.size()==23&&homeImport.searchNames==names,"Case-insensitive search rescanned catalog or lost matches");for(auto i:homeImport.filtered)Check(import_matches(homeImport.entries[i].path.filename().wstring(),L"truck"),"Filtered result mismatched");
        import_hit(250,600);Check(homeImport.first==importPageSize,"Search pagination ignored filtered results");import_wheel(WHEEL_DELTA);Check(homeImport.first==0,"Search scrolling failed");
        settings_escape();Check(homeImportActive&&homeImport.search.empty()&&!homeImport.searchEdit,"Escape closed importer instead of clearing search");home_import_prepare();Check(homeImport.filtered.size()==25,"Clearing search lost catalog");
        import_hit(100,95);for(wchar_t c:std::wstring(L"ete"))import_key(WM_CHAR,c);home_import_prepare();Check(homeImport.filtered.size()==1&&homeImport.entries[homeImport.filtered[0]].path.filename()==std::filesystem::path(L"\u00c9t\u00e9_Paris.png"),"Unicode/accent-insensitive search failed");
        std::vector<uint8_t> screen;settings_render(screen,2);Save(out/"search.bmp",screen,1920,1440);
        import_key(WM_CHAR,8);home_import_prepare();Check(homeImport.search==L"et","Search Backspace failed");import_hit(875,95);home_import_prepare();Check(homeImport.search.empty()&&homeImport.filtered.size()==25,"Search clear button failed");
        for(wchar_t c:std::wstring(L"no such image"))import_key(WM_CHAR,c);home_import_prepare();Check(homeImport.filtered.empty(),"Empty search results failed");import_hit(100,250);Check(homeImport.page==ImportPage::Browse,"Empty result row selected stale file");
        import_hit(875,95);home_import_prepare();for(wchar_t c:std::wstring(L"truck_12"))import_key(WM_CHAR,c);home_import_prepare();Check(homeImport.filtered.size()==1,"Exact filename filter failed");auto expected=homeImport.entries[homeImport.filtered[0]].path;import_hit(100,250);Check(homeImport.file==expected&&homeImport.job==ImportJob::Decode,"Click used raw row rather than filtered result");home_import_prepare();Check(homeImport.page==ImportPage::Crop,"Filtered file failed to open");
        import_close();Check(!homeImport.searchNames&&homeImport.filtered.empty(),"Search catalog retained after close");settingsOpen=false;
        std::cout<<"PASS: live Unicode/case/accent-insensitive file and folder search, coalesced filtering without disk rescan, pagination/scroll, clear/Escape, empty results and filtered row selection.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--home-import"){
        using namespace bettergps_overlay;const auto out=std::filesystem::absolute(argv[1]),fixtures=std::filesystem::absolute("validation/lockscreen-import/fixtures");std::filesystem::create_directories(out);const auto base=out/"runtime";
        home_init(base);for(const auto* folder:homeFolders)std::filesystem::create_directories(homeRoot/folder);
        HomePixels opaque(360*740*4,255);Check(import_png(homeRoot/"toplayer/old_toplayer.png",opaque),"PNG encoder failed");{std::ofstream file(homeRoot/"background/mytruck_background.dds");file<<"Existing user asset";}
        home_scan();homeSelected[3]="old";homeShuffleMode=2;homeShufflePending=false;homeShuffleLastDay=1000;homeShuffleLastPower=42;
        for(const char* name:{"mytruck.png","mytruck.jpg","mytruck.bmp","mytruck.tiff","mytruck.dds","rotated.jpg","large.png"}){auto image=import_decode(fixtures/name);Check(image&&!image->pixels.empty()&&std::max(image->width,image->height)<=2048,"Accepted image decoder failed");if(std::string(name)=="rotated.jpg")Check(image->originalWidth==900&&image->originalHeight==1600,"EXIF orientation ignored");if(std::string(name)=="large.png")Check(image->originalWidth==5000&&image->width==2048,"Large image preview was not bounded");}
        Check(!import_extension("unsupported.gif")&&!import_extension("unsupported.webp"),"Browser accepted unsupported extension");
        settingsOpen=true;settingsLockscreen=true;{std::lock_guard<std::mutex> lock(homeImport.mutex);homeImport.folder=fixtures;}
        settings_action(915,300);Check(homeImportActive&&homeImport.layer==2,"Background plus did not open importer");home_import_prepare();Check(homeImport.page==ImportPage::Browse,"Directory job failed");for(const auto& entry:homeImport.entries)Check(entry.path.filename()!="unsupported.gif","Unsupported file shown");
        std::vector<uint8_t> pixels;settings_render(pixels,2);Save(out/"browser.bmp",pixels,1920,1440);
        auto choose=[&](const std::filesystem::path& path){{std::lock_guard<std::mutex> lock(homeImport.mutex);homeImport.address=L"  \""+path.wstring()+L"\"  ";homeImport.edit=true;homeImport.page=ImportPage::Browse;}Check(import_key(WM_KEYDOWN,VK_RETURN),"Address enter missed");home_import_prepare();};
        const auto source=display_bytes(fixtures/"mytruck.png");choose(fixtures/"mytruck.png");Check(homeImport.page==ImportPage::Crop&&homeImport.image,"Image selection did not open cropper");
        auto cx=homeImport.centerX,cy=homeImport.centerY;auto crop=import_crop(*homeImport.image,cx,cy,1);Check(std::abs(crop.width/crop.height-360.0/740)<.000001,"Crop aspect distorted");
        import_hit(700,300);import_pointer(660,280,true);import_pointer(660,280,false);Check(homeImport.centerX!=.5,"Crop drag did not pan");import_wheel(WHEEL_DELTA);Check(homeImport.zoom>1,"Crop wheel did not zoom");settings_render(pixels,2);Save(out/"cropper.bmp",pixels,1920,1440);
        import_hit(350,575);Check(homeImport.page==ImportPage::Saving,"Import click missed");home_import_prepare();Check(!homeImportActive&&homeImport.error.empty(),"Import failed");
        const auto imported=homeRoot/"background/mytruck_2_background.png";Check(std::filesystem::is_regular_file(imported),"Cross-format filename collision overwrote an asset");Check(display_bytes(fixtures/"mytruck.png")==source,"Import modified source image");
        HomeDecoder decoder;auto saved=decoder.image(imported);Check(saved.size()==360*740*4,"Imported PNG is not 360x740");Check(homeSelected[2]=="mytruck_2"&&homeSelected[3]=="__none__","Background import did not select image and clear toplayer");home_shuffle_update(42,1000);Check(homeSelected[2]=="mytruck_2"&&homeSelected[3]=="__none__","Import immediately reshuffled itself");home_scan();Check(homeSelected[3]=="__none__","Refresh restored old toplayer");home_prepare();auto layers=std::atomic_load(&homeLayers);Check(layers&&layers->pixels[3]&&std::all_of(layers->pixels[3]->begin(),layers->pixels[3]->end(),[](uint8_t x){return x==0;}),"None did not clear published toplayer");homeShuffleMode=0;Check(home_save(),"Import selection not saved");home_init(base);Check(homeSelected[3]=="__none__","None lost on config reload");
        import_open(3);home_import_prepare();choose(fixtures/"transparent_toplayer.png");Check(homeImport.page==ImportPage::Crop,"Transparent image rejected");import_hit(350,575);home_import_prepare();auto transparent=decoder.image(homeRoot/"toplayer/transparent_toplayer.png");bool zero=false,partial=false;for(size_t i=3;i<transparent.size();i+=4){zero|=transparent[i]==0;partial|=transparent[i]>0&&transparent[i]<255;}Check(zero&&partial,"PNG import lost alpha");
        const auto selected=homeSelected;import_open(0);home_import_prepare();choose(fixtures/"broken.png");Check(homeImport.page==ImportPage::Browse&&!homeImport.error.empty()&&homeSelected==selected,"Decode failure changed active style");
        import_open(1);import_close();home_import_prepare();Check(!homeImportActive&&homeImport.job==ImportJob::None,"Cancelled import continued");
        settingsLockscreen=true;settings_render(pixels,2);Save(out/"layers.bmp",pixels,1920,1440);settingsOpen=false;
        std::cout<<"PASS: all supported formats, EXIF, bounded preview, in-game browser/pan/zoom, crop aspect, high-resolution PNG export, cross-format collisions, source/alpha preservation, background clears toplayer, persistence and cancellation.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--f150-startup"){
        using namespace bettergps_overlay;
        Check(f150_frame(0,52)==-1&&f150_frame(999,52)==-1,"F150 black delay changed");
        Check(f150_frame(1000,52)==0&&f150_frame(1033,52)==0&&f150_frame(1034,52)==1,"F150 not 30 fps");
        Check(f150_frame(2733,52)==51&&f150_frame(2734,52)==0,"F150 second loop wrong");
        Check(f150_frame(4466,52)==51&&f150_frame(4467,52)==-2,"F150 native handoff wrong");
        f150Requested=true;f150_prepare(std::filesystem::current_path()/"BetterGPS/Overlay/BetterGPS_assets");
        const auto animation=std::atomic_load(&f150Animation);Check(animation&&animation->count==52&&animation->blocks.size()==52*f150FrameBytes,"F150 animation missing/invalid");
        Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"F150 hooks failed");
        ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;
        Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"F150 WARP failed");
        std::vector<uint8_t> marker(size_t(f150Width)*f150Height*4);
        for(unsigned i=0;i<4;++i){marker[4*i]=0xa7;marker[4*i+1]=0x5b;marker[4*i+2]=0x31+10+i;marker[4*i+3]=255;}
        D3D11_TEXTURE2D_DESC desc{};desc.Width=f150Width;desc.Height=f150Height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{marker.data(),f150Width*4,0};ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;
        Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)),"F150 cold texture failed");
        g_origUpdateSubresource(context,texture,0,nullptr,marker.data(),f150Width*4,0);
        Check(SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&view))&&!g_f150Bridge.liveView,"F150 cold marker was already recognized");
        auto binding=[&](bool replaced){context->PSSetShaderResources(0,1,&view);ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==(replaced?(f150Uploaded==-1?g_f150BlackView:f150Uploaded==-2?g_f150ClearView:g_f150Bridge.liveView):view),"F150 binding/handoff incorrect");if(actual)actual->Release();};
        binding(true);Check(g_f150Bridge.liveView&&f150Uploaded==-1,"First cold bind leaked GPS before ignition telemetry");car_splash_enable(false);binding(true);Check(f150Uploaded==-2,"Central splash Off did not immediately reveal native display");car_splash_enable(true);binding(true);Check(f150Uploaded==-1,"Re-enabled central splash did not restore ignition gate");
        {ID3D11Resource* black=nullptr;g_f150BlackView->GetResource(&black);D3D11_TEXTURE2D_DESC read{};read.Width=read.Height=read.MipLevels=read.ArraySize=read.SampleDesc.Count=1;read.Format=DXGI_FORMAT_B8G8R8A8_UNORM;read.Usage=D3D11_USAGE_STAGING;read.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* stage=nullptr;
         Check(SUCCEEDED(device->CreateTexture2D(&read,nullptr,&stage)),"F150 gray readback failed");context->CopyResource(stage,black);D3D11_MAPPED_SUBRESOURCE mapped{};Check(SUCCEEDED(context->Map(stage,0,D3D11_MAP_READ,0,&mapped)),"F150 gray mapping failed");auto* pixel=static_cast<const uint8_t*>(mapped.pData);Check(pixel[0]==5&&pixel[1]==5&&pixel[2]==5&&pixel[3]==255,"F150 delay not exact RGB(5,5,5)");context->Unmap(stage,0);stage->Release();black->Release();}
        for(int i=0;i<20;++i){f150_ignition(true);auto serial=f150Serial.load();f150_ignition(true);Check(f150Serial==serial,"Repeated ignition sample restarted splash");binding(true);Check(f150Uploaded==-1,"F150 initial frame not black");
            f150Origin=GetTickCount64()-1600;binding(true);Check(f150Uploaded>=0&&f150Uploaded<52,"F150 frame upload failed");
            D3D11_TEXTURE2D_DESC live{};g_f150Bridge.liveTexture->GetDesc(&live);Check(live.Format==DXGI_FORMAT_BC1_UNORM&&live.Usage==D3D11_USAGE_DEFAULT,"F150 compressed texture lost");
            f150Origin=GetTickCount64()-5000;binding(true);Check(f150Uploaded==-2,"Completed splash did not reveal GPS");f150_ignition(false);binding(true);
        }
        std::atomic_store(&f150Animation,std::shared_ptr<const F150Animation>());f150LoadFinished=false;
        f150_ignition(true);binding(true);f150Origin=GetTickCount64()-5000;binding(true);Check(f150Uploaded==-1,"Slow preparation leaked GPS");
        std::atomic_store(&f150Animation,animation);f150LoadFinished=true;binding(true);Check(f150Uploaded==0,"Late animation skipped its first frames");
        // Same-sized unmarked game resources must remain completely native.
        marker.assign(marker.size(),255);ID3D11Texture2D* other=nullptr;ID3D11ShaderResourceView* otherView=nullptr;data.pSysMem=marker.data();device->CreateTexture2D(&desc,&data,&other);device->CreateShaderResourceView(other,nullptr,&otherView);
        context->PSSetShaderResources(0,1,&otherView);ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==otherView,"F150 matched an unrelated screen");actual->Release();
        renderGameplayEnabled=true;renderGameplayActive=false;binding(false);renderGameplayEnabled=false;
        context->ClearState();otherView->Release();other->Release();view->Release();texture->Release();uninstall_d3d_hooks();MH_Uninitialize();context->Release();device->Release();
        std::cout<<"F150 cold first bind, late telemetry/asset preparation, exact RGB(5,5,5), two loops, 20 ignition cycles and native handoff passed\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--gameplay"){
        using namespace bettergps_overlay;std::filesystem::create_directories(argv[1]);g_shutdownPath=std::filesystem::path(argv[1])/"shutdown.log";Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Gameplay hooks failed");renderGameplayEnabled=true;renderGameplayActive=false;
        ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Gameplay WARP failed");
        std::vector<uint8_t> marker(360*740*4);for(unsigned i=0;i<4;++i){marker[i*4]=0xa7;marker[i*4+1]=0x5b;marker[i*4+2]=uint8_t(0x31+6+i);marker[i*4+3]=255;}
        D3D11_TEXTURE2D_DESC desc{};desc.Width=360;desc.Height=740;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{marker.data(),1440,0};ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;device->CreateTexture2D(&desc,&data,&texture);device->CreateShaderResourceView(texture,nullptr,&view);Check(view&&!g_skinBridge.liveView&&!g_cpuRunning,"Main menu created phone GPU resources/worker");
        auto nativeBinding=[&]{context->PSSetShaderResources(0,1,&view);ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==view,"Menu changed a native shader binding");actual->Release();};nativeBinding();
        const auto original=g_origPresent;static unsigned presents=0;g_origPresent=+[](IDXGISwapChain*,UINT interval,UINT flags)->HRESULT{Check(interval==2&&flags==DXGI_PRESENT_TEST,"Menu modified native Present");++presents;return S_OK;};hook_Present(nullptr,2,DXGI_PRESENT_TEST);Check(presents==1,"Menu suppressed native Present");g_origPresent=original;
        scs_value_t speed{},pose{};speed.type=SCS_VALUE_TYPE_float;speed.value_float.value=0;pose.type=SCS_VALUE_TYPE_dplacement;pose.value_dplacement.orientation.heading=0.75f;pose.value_dplacement.position.x=50;
        auto channels=[&]{truck_speed_cb(nullptr,0,&speed,nullptr);world_position_cb(nullptr,0,&pose,nullptr);};channels();Check(!update_gameplay()&&!g_cpuRunning,"Preview telemetry without STARTED enabled gameplay");
        on_theme_session(SCS_TELEMETRY_EVENT_started,nullptr,nullptr);Check(!update_gameplay(),"STARTED reused old truck data");truck_speed_cb(nullptr,0,&speed,nullptr);Check(!update_gameplay(),"Speed alone enabled gameplay");world_position_cb(nullptr,0,&pose,nullptr);Check(update_gameplay()&&g_cpuRunning&&render_gpu_allowed(),"Stationary valid truck did not activate gameplay");context->PSSetShaderResources(0,1,&view);Check(g_skinBridge.liveView,"Gameplay failed to recover streamed marker");
        phonePower=GetTickCount64()+3;const auto serial=homePowerOnSerial.load();
        for(int cycle=0;cycle<5;++cycle){auto cached=std::make_shared<const std::vector<uint8_t>>(16,255);{std::lock_guard<std::mutex> lock(g_prepared[32].mutex);g_prepared[32].pixels=cached;g_prepared[32].version=777;}const auto before=startup_elapsed(GetTickCount64());on_theme_session(SCS_TELEMETRY_EVENT_paused,nullptr,nullptr);Check(!render_gpu_allowed()&&!g_cpuRunning,"Warm pause retained rendering in menu");nativeBinding();on_theme_session(SCS_TELEMETRY_EVENT_started,nullptr,nullptr);Check(render_gpu_allowed()&&g_cpuRunning,"Same-session STARTED waited for another telemetry frame");{std::lock_guard<std::mutex> lock(g_prepared[32].mutex);Check(g_prepared[32].pixels==cached&&g_prepared[32].version==777,"Warm resume discarded its prepared image before first cabin draw");}Check(std::abs(startup_elapsed(GetTickCount64())-before)<50&&homePowerOnSerial==serial,"Warm resume restarted/consumed the startup animation");}
        for(int cycle=0;cycle<20;++cycle){const auto before=startup_elapsed(GetTickCount64());on_theme_session(SCS_TELEMETRY_EVENT_paused,nullptr,nullptr);Check(!g_cpuRunning&&!g_cpuWorker.joinable()&&!render_gpu_allowed(),"Pause retained phone worker/rendering");nativeBinding();on_theme_session(SCS_TELEMETRY_EVENT_configuration,nullptr,nullptr);channels();Check(!update_gameplay(),"Configurator preview enabled gameplay");Sleep(10);on_theme_session(SCS_TELEMETRY_EVENT_started,nullptr,nullptr);Check(!update_gameplay(),"Save/resume reused stale telemetry");channels();Check(update_gameplay()&&g_cpuRunning,"Resume failed");Check(std::abs(startup_elapsed(GetTickCount64())-before)<50&&homePowerOnSerial==serial,"Menu consumed/replayed phone animation");}
        world_position_cb(nullptr,0,nullptr,nullptr);Check(!update_gameplay()&&!g_cpuRunning,"Invalid pose kept rendering active");channels();Check(update_gameplay(),"Fresh valid channels failed to restore driving");
        renderActivityEnabled=true;renderGameWindow=reinterpret_cast<HWND>(1);renderGameplaySuspend=+[]{g_drivingStarted=false;suspend_gameplay(GetTickCount64());};render_window_message(renderGameWindow,WM_CLOSE,0);Check(!g_cpuRunning&&!render_gpu_allowed(),"Window close retained phone worker");renderActivityEnabled=false;renderGameWindow=nullptr;
        ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);view->Release();texture->Release();g_shutdownStarted=false;scs_telemetry_shutdown();context->Release();device->Release();
        std::cout<<"PASS: no worker/rendering before driving or in pause/configurator, unchanged native calls, stationary activation after fresh pose/speed, marker recovery, 20 menu/save resume cycles with animation time frozen, and shutdown from menu.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--shutdown"){
        using namespace bettergps_overlay;std::filesystem::create_directories(argv[1]);g_shutdownPath=std::filesystem::path(argv[1])/"shutdown.log";
        static std::atomic<int> messages{0};WNDCLASSW windowClass{};windowClass.hInstance=GetModuleHandleW(nullptr);windowClass.lpszClassName=L"BetterGPS shutdown regression";windowClass.lpfnWndProc=+[](HWND w,UINT m,WPARAM a,LPARAM b)->LRESULT{if(m==WM_APP+99){++messages;return 87;}return DefWindowProcW(w,m,a,b);};Check(RegisterClassW(&windowClass),"Shutdown test class failed");
        HWND window=CreateWindowExW(0,windowClass.lpszClassName,L"",0,0,0,160,160,nullptr,nullptr,windowClass.hInstance,nullptr);Check(window,"Shutdown test window failed");
        for(int cycle=0;cycle<20;++cycle){auto entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);g_cpuStop=false;g_cpuRunning=true;g_cpuWorker=std::thread([&]{SetEvent(entered);Check(SendMessageW(window,WM_APP+99,0,0)==87,"Worker teardown message failed");});WaitForSingleObject(entered,1000);Sleep(2);
            Check(WaitForSingleObject(g_cpuWorker.native_handle(),0)==WAIT_TIMEOUT,"Worker was not awaiting the UI thread");if(!cycle)PostQuitMessage(42);auto start=GetTickCount64();stop_cpu_worker();Check(GetTickCount64()-start<1000&&!g_cpuWorker.joinable()&&!g_cpuRunning,"UI-aware join stalled");CloseHandle(entered);
            if(!cycle){MSG quit{};Check(PeekMessageW(&quit,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE)&&quit.wParam==42,"Shutdown swallowed WM_QUIT");}}
        Check(messages==20,"Not all teardown messages were dispatched");DestroyWindow(window);UnregisterClassW(windowClass.lpszClassName,windowClass.hInstance);
        Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Shutdown hooks failed");bettergps_navigation::Start();start_cpu_worker(false);g_shutdownStarted=false;auto start=GetTickCount64();scs_telemetry_shutdown();Check(GetTickCount64()-start<2000&&renderStopping&&!render_gpu_allowed()&&!g_cpuWorker.joinable()&&!g_hooksReady,"Actual SDK shutdown did not quiesce workers/hooks");scs_telemetry_shutdown();
        std::ifstream journal(g_shutdownPath);std::string notes((std::istreambuf_iterator<char>(journal)),{});Check(notes.find("worker-stopped")<notes.find("host-stopped")&&notes.find("done")!=std::string::npos,"Shutdown ordering or completion not recorded");std::cout<<"PASS: 20 UI-dependent worker teardowns, WM_QUIT preserved, actual SDK stop ordering, stopped GPU gate, hook cleanup and idempotent shutdown.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--timezones"){game_timezone_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--cursor-only"){cursor_only_fixture::run(std::filesystem::absolute(argv[1]));return 0;}
    if(argc==3&&std::string(argv[2])=="--menu-shortcut"){
        using namespace bettergps_overlay;displayFixtureModifiers=0;cursorOnlyFixtureDown=0;std::filesystem::create_directories(argv[1]);const auto file=std::filesystem::path(argv[1])/"shortcuts.ini";{std::ofstream f(file);f<<"Mode=1\nToggleKey=120\nToggleModifiers=1\n";}display_power_load(file);
        Check(settingsMenuKey==VK_F9&&settingsMenuMods==0,"Legacy config lost default menu shortcut");
        Check(!settings_menu_shortcut(VK_F9,1)&&settingsMenuMods==0,"Menu accepted display toggle conflict");
        Check(!settings_menu_shortcut(0,0)&&!settings_menu_shortcut(VK_LSHIFT,0),"Menu could be disabled or modifier-only");
        settingsOpen=true;settings_action(500,300);Check(settingsCaptureMenuKey&&!settingsCapturePowerKey,"Menu shortcut button did not arm capture");
        settings_wndproc(nullptr,WM_KEYDOWN,'G',0);Check(settingsMenuKey=='G'&&settingsMenuMods==0&&!settingsCaptureMenuKey,"Menu shortcut capture failed");
        settings_menu_key_input(true,true);Check(settingsOpen,"Rebinding immediately closed the menu");
        const auto clip=settingsOriginalClipCursor;const auto cursor=settingsOriginalSetCursorPos;settingsOriginalClipCursor=+[](const RECT*)->BOOL{return TRUE;};settingsOriginalSetCursorPos=+[](int,int)->BOOL{return TRUE;};
        settings_menu_key_input(false,true);settings_menu_key_input(true,true);Check(!settingsOpen,"New shortcut did not close menu");settings_menu_key_input(true,true);Check(!settingsOpen,"Held shortcut repeated");settings_menu_key_input(false,true);settings_menu_key_input(true,false);Check(!settingsOpen,"Menu shortcut acted out of focus");
        settings_menu_key_input(false,true);settings_menu_key_input(true,true);Check(settingsOpen,"New shortcut did not reopen menu");
        settings_action(500,300);settings_wndproc(nullptr,WM_KEYDOWN,VK_ESCAPE,0);Check(!settingsCaptureMenuKey&&settingsOpen&&settingsMenuKey=='G',"Escape did not cancel capture");
        Check(display_key_valid(VK_F9,0),"Old menu shortcut was still reserved");Check(display_power_save(),"Shortcut save failed");display_power_load(file);Check(settingsMenuKey=='G'&&settingsMenuMods==0&&displayPowerMode==1,"Menu shortcut did not persist");
        settings_action(820,300);Check(settingsMenuKey==VK_F9&&settingsMenuMods==0,"Reset did not restore F9");
        {std::ofstream f(file);f<<"MenuKey=0\nMenuModifiers=99\nToggleKey=120\nToggleModifiers=2\n";}display_power_load(file);Check(settingsMenuKey==VK_F9&&settingsMenuMods==0&&displayPowerKey==VK_F9&&displayPowerMods==1,"Invalid/conflicting file shortcuts were not sanitized");
        std::vector<uint8_t> image;settings_render(image,2,false);Save(std::filesystem::path(argv[1])/"menu.bmp",image,1920,1440);settings_close();settingsOriginalClipCursor=clip;settingsOriginalSetCursorPos=cursor;
        std::cout<<"PASS: default/migrated/file menu shortcuts, capture, immediate use without repeat, focus, Escape, conflict protection, reset and persistence.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--composition-benchmark"){
        using namespace bettergps_overlay;const auto out=std::filesystem::path(argv[1]);std::filesystem::create_directories(out);Check(load_phone_fonts(),"Benchmark font resources missing");phoneStartupHoldMs=3000;
        auto style=std::make_shared<HomeLayers>();style->revision=1;
        for(size_t layer=0;layer<4;++layer){auto image=std::make_shared<HomePixels>(360*740*4);for(size_t i=0;i<image->size();++i)(*image)[i]=uint8_t((i*17+layer*41)%256);style->pixels[layer]=image;}
        std::atomic_store(&homeLayers,std::shared_ptr<const HomeLayers>(style));
        std::vector<uint8_t> pixels(360*740*4),base(pixels.size());for(size_t i=0;i<base.size();++i)base[i]=uint8_t(i*7%256);
        std::ofstream hashes(out/"pixels.txt");double homeMs=0,overlayMs=0;
        auto digest=[&]{uint64_t hash=1469598103934665603ull;for(auto byte:pixels){hash^=byte;hash*=1099511628211ull;}hashes<<hash<<'\n';};
        for(int i=0;i<120;++i){pixels=base;const auto start=std::chrono::steady_clock::now();render_phone_startup(pixels,1000+(i%60)*68,2886+i/30,i>=60);homeMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();digest();}
        overlay_reset(-1);g_haveGameTime=g_haveTruckSpeed=g_haveNavDistance=g_haveNavTime=g_haveCompassHeading=true;g_gameTimeMinutes=700;g_navDistanceM=100000;g_navTimeSec=3600;
        for(int i=0;i<120;++i){g_tripPresent=i<60;g_darkMode=(i/30)%2;g_compassHeading=double(i)/119;auto start=std::chrono::steady_clock::now();render_configured_phone(pixels);overlayMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();digest();}
        for(int phase:{-1303,-1003,-3,-2,-1,0,999,1000,1020,1400,2200,4399,4400,4401,4900,5198,5200}){
            auto profile=overlay_snapshot();std::vector<uint8_t> reference,actual;render_configured_phone(reference,&profile,1);render_phone_startup(reference,phase,g_gameTimeMinutes.load(),g_darkMode.load());render_prepared_phone(actual,profile,1,phase);Check(actual==reference,"Covered lockscreen skipped visible GPS content");
        }
        std::ofstream report(out/"timing.json");report<<"{\"framesPerSection\":120,\"homeMs\":"<<homeMs<<",\"overlayMs\":"<<overlayMs<<"}\n";std::cout<<"home_ms="<<homeMs<<" overlay_ms="<<overlayMs<<" (120 frames each)\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--render-resume"){
        using namespace bettergps_overlay;std::filesystem::create_directories(argv[1]);Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Resume test hooks failed");
        ID3D11Device *device=nullptr,*secondDevice=nullptr;ID3D11DeviceContext *context=nullptr,*secondContext=nullptr;
        Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Resume WARP device failed");
        HWND window=CreateWindowExW(0,L"STATIC",L"BetterGPS hidden resume test",WS_POPUP,0,0,160,160,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);Check(window!=nullptr,"Resume test window failed");
        auto chainFor=[&](ID3D11Device* source){IDXGIDevice* dxgi=nullptr;IDXGIAdapter* adapter=nullptr;IDXGIFactory* factory=nullptr;IDXGISwapChain* chain=nullptr;source->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi);dxgi->GetAdapter(&adapter);adapter->GetParent(__uuidof(IDXGIFactory),(void**)&factory);DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=desc.BufferDesc.Height=160;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=window;desc.Windowed=TRUE;Check(SUCCEEDED(factory->CreateSwapChain(source,&desc,&chain)),"Resume swapchain failed");factory->Release();adapter->Release();dxgi->Release();return chain;};
        auto* chain=chainFor(device);renderActivityEnabled=true;renderGameWindow=window;renderMainChain=chain;render_set_foreground(false);renderOccluded=renderDeviceLost=false;
        Check(render_gpu_allowed(),"Alt-Tab suppressed a visible gameplay display");render_window_message(window,WM_ACTIVATEAPP,TRUE);Check(render_gpu_allowed(),"Activation message did not wake plugin");render_window_message(window,WM_SIZE,SIZE_MINIMIZED);Check(!render_gpu_allowed(),"Minimize allowed plugin GPU work");
        render_set_foreground(true);render_set_visible(true);render_present_result(DXGI_STATUS_OCCLUDED);Check(!render_gpu_allowed(),"Occluded window allowed plugin GPU work");render_present_result(S_OK);Check(render_gpu_allowed(),"Successful visibility probe did not clear occlusion");render_present_result(DXGI_ERROR_DEVICE_REMOVED);Check(!render_gpu_allowed(),"Removed device allowed plugin GPU work");Check(prepare_present_device(chain)&&render_gpu_allowed(),"Healthy device recovery failed");
        // A marker can be streamed while the app is inactive. Do not create a
        // live texture until its first resumed binding; retain its identity.
        render_set_foreground(false);render_set_visible(false);std::vector<uint8_t> pixels(360*740*4,0);for(unsigned i=0;i<4;++i){pixels[i*4]=0xa7;pixels[i*4+1]=0x5b;pixels[i*4+2]=uint8_t(0x31+6+i);pixels[i*4+3]=255;}
        D3D11_TEXTURE2D_DESC desc{};desc.Width=360;desc.Height=740;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{pixels.data(),1440,0};ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&view)),"Inactive marker allocation failed");Check(!g_skinBridge.liveView,"Inactive texture streaming created plugin GPU resources");
        auto nativeSerial=homePowerOnSerial.load();for(int cycle=0;cycle<250;++cycle){context->PSSetShaderResources(0,1,&view);ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==view&&!g_skinBridge.liveView,"Inactive binding performed plugin work");actual->Release();}Check(homePowerOnSerial==nativeSerial,"Inactive bindings restarted lockscreen");
        // Original Present always executes, including probes and hidden frames.
        const auto originalPresent=g_origPresent;static unsigned calls=0,seenFlags=0,seenInterval=0;static HRESULT result=S_OK;g_origPresent=+[](IDXGISwapChain*,UINT interval,UINT flags)->HRESULT{++calls;seenFlags=flags;seenInterval=interval;return result;};
        std::unique_lock<std::mutex> phoneLock(g_phoneScreenMutex),bridgeLock(g_d3dMutex);auto future=std::async(std::launch::async,[&]{return hook_Present(chain,2,0);});const bool hiddenFast=future.wait_for(std::chrono::milliseconds(500))==std::future_status::ready;phoneLock.unlock();bridgeLock.unlock();Check(SUCCEEDED(future.get())&&hiddenFast&&calls==1&&seenInterval==2&&!seenFlags,"Hidden Present waited for plugin locks or changed engine arguments");
        result=DXGI_STATUS_OCCLUDED;Check(hook_Present(chain,3,DXGI_PRESENT_TEST)==DXGI_STATUS_OCCLUDED&&calls==2&&seenFlags==DXGI_PRESENT_TEST&&seenInterval==3&&!g_skinBridge.liveView,"Visibility probe touched plugin resources or changed native call");result=S_OK;hook_Present(chain,0,DXGI_PRESENT_TEST);Check(!renderOccluded,"Resumed native probe remained occluded");g_origPresent=originalPresent;
        // GPU fixtures intentionally have no visible/focused OS window. Disable
        // only visibility policy to exercise the genuine resumed bind path.
        renderActivityEnabled=false;context->PSSetShaderResources(0,1,&view);Check(g_skinBridge.liveView!=nullptr,"Resumed marker stayed permanently black");ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==g_skinBridge.liveView,"Resumed marker did not recover its live view");actual->Release();
        renderActivityEnabled=true;render_set_visible(true);render_set_foreground(false);render_window_message(window,WM_KILLFOCUS,0);context->PSSetShaderResources(0,1,&view);actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==g_skinBridge.liveView&&render_gpu_allowed(),"Visible unfocused gameplay reverted to native/blank display");actual->Release();renderActivityEnabled=false;
        phoneLock.lock();auto busy=std::async(std::launch::async,[&]{finish_phone_frame(context);});const bool busyFast=busy.wait_for(std::chrono::milliseconds(500))==std::future_status::ready;phoneLock.unlock();busy.get();Check(busyFast,"Phone Present work blocked on another D3D callback");
        auto surface=std::make_unique<IndependentPhoneSurface>();Check(surface->start(device,32),"Old-device surface failed");g_independentSurfaces.push_back(std::move(surface));auto screen=std::make_unique<PhoneScreenPower>();Check(screen->start(texture),"Old-device source failed");g_phoneScreens.push_back(std::move(screen));
        Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&secondDevice,nullptr,&secondContext)),"Second recovery device failed");auto* secondChain=chainFor(secondDevice);Check(prepare_present_device(secondChain)&&g_phoneScreens.empty()&&g_independentSurfaces.empty()&&!g_skinBridge.liveView,"Recovered device retained incompatible phone resources");
        ID3D11Texture2D* nextTexture=nullptr;ID3D11ShaderResourceView* nextView=nullptr;Check(SUCCEEDED(secondDevice->CreateTexture2D(&desc,&data,&nextTexture))&&SUCCEEDED(secondDevice->CreateShaderResourceView(nextTexture,nullptr,&nextView)),"Recovered device marker failed");secondContext->PSSetShaderResources(0,1,&nextView);ID3D11Device* owner=nullptr;g_skinBridge.liveTexture->GetDevice(&owner);Check(owner==secondDevice,"Recovered bridge reused an old-device texture");owner->Release();
        g_graphicsWatchPath=std::filesystem::path(argv[1])/"watch.log";{RenderCall trace(RenderStage::Binding);currentRenderTrace->since=GetTickCount64()-6000;graphics_watchdog();Check(std::filesystem::file_size(g_graphicsWatchPath)>0,"Stall breadcrumbs not recorded");const auto bytes=std::filesystem::file_size(g_graphicsWatchPath);graphics_watchdog();Check(std::filesystem::file_size(g_graphicsWatchPath)==bytes,"Watchdog repeatedly logged the same stall");}
        ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);secondContext->PSSetShaderResources(0,1,&empty);view->Release();texture->Release();nextView->Release();nextTexture->Release();chain->Release();secondChain->Release();uninstall_d3d_hooks();context->Release();device->Release();secondContext->Release();secondDevice->Release();DestroyWindow(window);MH_Uninitialize();
        std::cout<<"PASS: focus/minimize/occlusion/device gate, 250 inactive binds with no GPU creation or lockscreen restart, unchanged native Present/probe forwarding under contended locks, resumed marker recovery, nonblocking phone lock, replacement-device cleanup/rebinding and bounded stall breadcrumbs.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--native-live-theme"){
        using namespace bettergps_overlay;std::filesystem::create_directories(argv[1]);
        // Map the actual installed executable as inert data, including ASLR
        // relocations. Never execute its entry point or require a running game.
        std::ifstream file("H:/SteamLibrary/steamapps/common/American Truck Simulator/bin/win_x64/amtrucks.exe",std::ios::binary|std::ios::ate);Check(bool(file),"ATS image unavailable");std::vector<uint8_t> raw(size_t(file.tellg()));file.seekg(0);file.read((char*)raw.data(),raw.size());
        auto* nt=(IMAGE_NT_HEADERS64*)(raw.data()+((IMAGE_DOS_HEADER*)raw.data())->e_lfanew);std::vector<uint8_t> image(nt->OptionalHeader.SizeOfImage,0);memcpy(image.data(),raw.data(),nt->OptionalHeader.SizeOfHeaders);
        for(auto* section=IMAGE_FIRST_SECTION(nt);section<IMAGE_FIRST_SECTION(nt)+nt->FileHeader.NumberOfSections;++section)memcpy(image.data()+section->VirtualAddress,raw.data()+section->PointerToRawData,section->SizeOfRawData);
        const auto delta=uintptr_t(image.data())-nt->OptionalHeader.ImageBase;const auto reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];size_t at=reloc.VirtualAddress;
        while(at<reloc.VirtualAddress+reloc.Size){auto* block=(IMAGE_BASE_RELOCATION*)(image.data()+at);if(block->SizeOfBlock<8)break;auto* entries=(uint16_t*)(block+1);for(size_t i=0;i<(block->SizeOfBlock-8)/2;++i)if((entries[i]>>12)==IMAGE_REL_BASED_DIR64)*(uintptr_t*)(image.data()+block->VirtualAddress+(entries[i]&4095))+=delta;at+=block->SizeOfBlock;}
        Check(native_fork_resolve({image.data(),image.size()}),"Installed ATS private map ABI failed its code/string guards");
        const auto resolveStart=std::chrono::steady_clock::now();
        const auto layout=resolve_map_unit({image.data(),image.size()});Check(layout.vtable&&layout.cacheInit&&layout.cacheDestroy&&layout.cacheReset&&layout.cacheColors.size()==14&&layout.cacheBytes==0xdd8&&layout.colors.size()==14&&layout.bytes==0x310,"Actual ATS reflection / unit lifecycle resolution failed");
        std::cout<<"MAP_RESOLVE_BENCH us="<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-resolveStart).count()<<'\n';
        {std::ofstream fingerprint(std::filesystem::path(argv[1])/"map-layout.txt");for(auto pointer:{layout.owner,layout.vtable,layout.loaded,layout.destroy,layout.cacheInit,layout.cacheDestroy,layout.cacheReset})fingerprint<<pointer-uintptr_t(image.data())<<'\n';fingerprint<<layout.bytes<<' '<<layout.areas<<' '<<layout.discovered<<' '<<layout.cacheBytes<<'\n';for(const auto* fields:{&layout.colors,&layout.cacheColors})for(const auto& field:*fields)fingerprint<<field.first<<' '<<field.second<<'\n';}
        Check(layout.colors.at("road_color")==0x1a0&&layout.areas==0x1e8&&layout.discovered==0x210,"Actual ATS property offsets incorrectly resolved");
        const auto assets=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets");std::array<NativePalette,2> palettes{read_native_palette(assets/"native_themes/light/colour_data.sui"),read_native_palette(assets/"native_themes/dark/colour_data.sui")};
        std::vector<uint8_t> unit(layout.bytes,0x6a);*(uintptr_t*)unit.data()=layout.vtable;std::vector<uint32_t> areas(palettes[0].areas.size()),discovered(palettes[0].discovered.size());
        auto configure=[&](){*(uintptr_t*)(unit.data()+layout.areas+8)=uintptr_t(areas.data());*(size_t*)(unit.data()+layout.areas+16)=areas.size();*(uintptr_t*)(unit.data()+layout.discovered+8)=uintptr_t(discovered.data());*(size_t*)(unit.data()+layout.discovered+16)=discovered.size();};configure();auto untouched=unit;
        for(int cycle=0;cycle<300;++cycle){const auto& palette=palettes[cycle%2];Check(map_unit_write(layout,uintptr_t(unit.data()),palette),"Native palette transaction failed");for(auto& field:layout.colors){Check(*(uint32_t*)(unit.data()+field.second)==palette.colors.at(field.first),"Native road / prefab color stale");memcpy(untouched.data()+field.second,unit.data()+field.second,4);}Check(unit==untouched&&areas==palette.areas&&discovered==palette.discovered,"Native theme modified unrelated unit fields");}
        auto previous=unit;auto previousAreas=areas;*(size_t*)(unit.data()+layout.discovered+16)=9999;auto invalid=unit;Check(!map_unit_write(layout,uintptr_t(unit.data()),palettes[0])&&unit==invalid&&areas==previousAreas,"Invalid array caused a partial theme write");configure();
        liveMapLayout=layout;liveMapPalettes=palettes;liveMapUnits={};liveMapDark=false;liveMapOriginalLoaded=+[](void*){return true;};liveMapOriginalDestroy=+[](void* p,unsigned)->void*{return p;};liveMapReady=true;
        std::vector<uint8_t> cache(layout.cacheBytes,0x39);*(uintptr_t*)cache.data()=layout.vtable;for(auto& field:layout.cacheColors)*(uint32_t*)(cache.data()+field.second)=palettes[0].colors.at(field.first);
        liveMapOriginalCacheInit=+[](void* p)->void*{return p;};liveMapOriginalCacheDestroy=liveMapOriginalCacheReset=liveMapOriginalCacheInit;
        live_map_cache_init(cache.data());
        live_map_loaded(unit.data());Check(live_map_apply(true)&&areas==previousAreas,"Loaded unit did not adopt requested native theme");live_map_destroy(unit.data(),0);Check(!live_map_apply(false),"Destroyed save unit remained referenced");
        live_map_loaded(unit.data());Check(live_map_apply(false)&&areas==previousAreas,"Replacement save unit did not rebind");
        for(int cycle=0;cycle<100;++cycle){const bool dark=cycle%2;Check(live_map_apply(dark),"Cached map palette apply failed");for(auto& field:layout.cacheColors)Check(*(uint32_t*)(cache.data()+field.second)==palettes[0].colors.at(field.first),"Native ui_map cache was overwritten");}
        *(uint32_t*)(cache.data()+layout.cacheColors.at("road_color"))=0xff707070;
        Check(live_map_apply(!liveMapDark)&&*(uint32_t*)(cache.data()+layout.cacheColors.at("road_color"))==0xff707070,"Derived world/job map override was replaced by phone road color");
        *(uint32_t*)(cache.data()+layout.cacheColors.at("road_color"))=palettes[0].colors.at("road_color");
        live_map_cache_reset(cache.data());Check(std::none_of(liveMapCaches.begin(),liveMapCaches.end(),[](auto c){return c.pointer!=0;}),"Save reset retained a UI palette cache");live_map_cache_init(cache.data());
        auto savedAreas=areas;auto savedCache=cache;*(size_t*)(unit.data()+layout.discovered+16)=9999;Check(!live_map_apply(!liveMapDark)&&areas==savedAreas&&cache==savedCache,"Failed multi-target preflight partially updated UI cache");configure();
        g_assetsReady=true;overlay_reset(-1);g_haveGameTime=true;g_haveWorldPosition=true;g_worldX=0;g_worldZ=0;g_multiplayerTimeOffset=0;
        for(int cycle=0;cycle<30;++cycle){g_gameTimeMinutes=12*60;on_theme_session(0,nullptr,nullptr);on_theme_frame(0,nullptr,nullptr);Check(!g_darkMode&&areas==previousAreas,"Day save reload left mixed native palette");g_gameTimeMinutes=23*60;on_theme_session(0,nullptr,nullptr);on_theme_frame(0,nullptr,nullptr);Check(g_darkMode&&areas==previousAreas,"Night save reload left mixed native palette");}
        // Forced light/dark is per display now. The shared solar state must
        // remain automatic; test the same effective selector the renderers use.
        overlay_theme(1);on_theme_frame(0,nullptr,nullptr);Check(!selected_dark(overlay_snapshot().theme,g_darkMode)&&areas==previousAreas,"Display's forced-light mode was overridden");overlay_theme(2);g_gameTimeMinutes=12*60;on_theme_frame(0,nullptr,nullptr);Check(selected_dark(overlay_snapshot().theme,g_darkMode)&&areas==previousAreas,"Solar time overrode display's forced dark");overlay_reset(-1);live_map_cache_destroy(cache.data());Check(std::none_of(liveMapCaches.begin(),liveMapCaches.end(),[](auto c){return c.pointer!=0;}),"Destroyed UI map retained its palette cache");liveMapReady=false;g_assetsReady=false;liveMapUnits={};
        Check(native_icons_load(assets)&&nativeIcons.size()>=20,"Native day/night icon pairs incomplete");
        ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Icon test WARP device failed");
        Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Native icon binding hook install failed");
        for(unsigned index=0;index<nativeIcons.size();++index)for(int sourceMode=0;sourceMode<2;++sourceMode){const auto& source=nativeIcons[index].images[sourceMode];D3D11_TEXTURE2D_DESC desc{};desc.Width=source.width;desc.Height=source.height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=source.format;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{source.pixels.data(),source.width*4,0};ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&view)),"Native icon test creation failed");native_icon_identify(texture,desc,data.pSysMem,data.SysMemPitch);unsigned tag=0;UINT size=sizeof(tag);texture->GetPrivateData(nativeIconTag,&size,&tag);Check(tag==index+1,"Exact native icon identification failed");
            for(bool dark:{false,true,false,true}){set_dark_presentation(dark);context->PSSetShaderResources(0,1,&view);ID3D11ShaderResourceView* bound=nullptr;context->PSGetShaderResources(0,1,&bound);auto* chosen=native_icon_select(view,tag,dark);Check(bound==chosen,"Native PS binding did not select the matching theme texture");bound->Release();Check(chosen,"Native icon texture swap failed");ID3D11Resource* selected=nullptr;chosen->GetResource(&selected);auto stageDesc=desc;stageDesc.BindFlags=0;stageDesc.Usage=D3D11_USAGE_STAGING;stageDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* stage=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&stageDesc,nullptr,&stage)),"Icon staging failed");context->CopyResource(stage,selected);D3D11_MAPPED_SUBRESOURCE mapped{};Check(SUCCEEDED(context->Map(stage,0,D3D11_MAP_READ,0,&mapped)),"Icon readback failed");const auto& expected=nativeIcons[index].images[dark?1:0];for(unsigned y=0;y<expected.height;++y)Check(memcmp((uint8_t*)mapped.pData+y*mapped.RowPitch,expected.pixels.data()+y*expected.width*4,expected.width*4)==0,"Swapped icon RGBA / alpha differs from supplied native asset");context->Unmap(stage,0);stage->Release();selected->Release();}view->Release();texture->Release();
        }ID3D11ShaderResourceView* noIcon=nullptr;context->PSSetShaderResources(0,1,&noIcon);context->Release();device->Release();uninstall_d3d_hooks();
        std::cout<<"PASS: actual ATS reflection + lifecycle resolved; 300 native palette transitions, 100 UI cache transitions/reset/destruction, invalid multi-target transaction, destroyed/reloaded unit rebinding, 30 day/night save reload pairs, forced modes, and all 20 native icon pairs in both startup themes with byte-exact GPU RGBA/alpha. No flattened-map filter.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--lockscreen-shuffle"){
        using namespace bettergps_overlay;const auto out=std::filesystem::absolute(argv[1]);const auto fixture=out/"fixture";const auto assets=fixture/"BetterGPS_assets/material/ui/phone_lockscreen";std::filesystem::create_directories(assets);std::filesystem::copy("BetterGPS/Overlay/BetterGPS_assets/material/ui/phone_lockscreen",assets,std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
        home_init(fixture);Check(homeDurationSeconds==3&&homeShuffleMode==2,"Defaults must be 3 seconds and Day shuffle");{std::lock_guard<std::mutex> lock(homeMutex);homeShuffleSeed=123456789;}
        const auto icons=std::array<std::string,2>{homeSelected[0],homeSelected[1]};auto pair=[&](){Check(home_lower(homeSelected[2])==home_lower(homeSelected[3]),"Shuffle background/foreground mismatch");Check(homeSelected[0]==icons[0]&&homeSelected[1]==icons[1],"Shuffle altered icon style");};
        home_shuffle_update(1,-1);Check(homeShuffleLastDay==-1,"Unknown game day used wall-clock day");home_shuffle_update(1,42);pair();const auto first=homeSelected;const auto revision=homeRevision.load();for(uint64_t on=2;on<20;++on)home_shuffle_update(on,42);Check(homeSelected==first&&homeRevision==revision,"Day shuffle rerolled on phone power / polling");Check(home_save(),"Day shuffle preferences failed to save");const auto seed=homeShuffleSeed;home_init(fixture);home_shuffle_update(50,42);Check(homeShuffleSeed==seed&&homeSelected==first,"Day assignment did not survive restart");home_shuffle_update(50,43);pair();home_shuffle_update(50,42);Check(homeSelected==first,"Savegame rewind changed already assigned day");
        home_shuffle_mode(1);home_shuffle_update(100,42);pair();auto counter=homeShuffleCounter;for(uint64_t on=101;on<151;++on){auto previous=homeSelected[2];home_shuffle_update(on,42);pair();Check(homeSelected[2]!=previous,"Power shuffle immediately repeated its previous background");const auto current=homeSelected;home_shuffle_update(on,99);Check(homeSelected==current,"Power mode shuffled on day change / duplicate event");}Check(homeShuffleCounter==counter+50,"Power shuffle counter incorrect");
        const auto beforeMenu=homeSelected;counter=homeShuffleCounter;home_scan();home_shuffle_update(150,99);Check(homeSelected==beforeMenu&&homeShuffleCounter==counter,"Opening/refreshing catalog triggered a power shuffle");
        Check(home_save(),"Power shuffle state save failed");counter=homeShuffleCounter;home_init(fixture);Check(homeShuffleMode==1&&homeShuffleCounter==counter,"Power mode / counter not restored");home_shuffle_update(1,99);Check(homeSelected[2]!=beforeMenu[2],"Restart power shuffle repeated last displayed image");
        // Every full bag must contain each wallpaper exactly once, including
        // boundaries, small catalogs, and different installation seeds.
        const auto catalog=homeChoices[2];const auto originalSeed=homeShuffleSeed;
        for(size_t count=1;count<=32;++count){homeChoices[2].clear();for(size_t i=0;i<count;++i)homeChoices[2].push_back({"wallpaper_"+std::to_string(i),{}});
            for(uint64_t seedCase=1;seedCase<=20;++seedCase){homeShuffleSeed=seedCase;size_t previous=count;for(uint64_t bag=0;bag<40;++bag){std::vector<bool> seen(count,false);for(size_t pos=0;pos<count;++pos){const auto index=home_shuffle_index(bag*count+pos);Check(index<count&&!seen[index],"Wallpaper repeated before bag exhausted");Check(count==1||index!=previous,"Wallpaper repeated across bag boundary");seen[index]=true;previous=index;}}}}
        homeChoices[2]=catalog;homeShuffleSeed=originalSeed;
        home_shuffle_mode(2);homeShuffleDayOrigin=-1;std::vector<std::string> daily;
        for(int64_t day=0;day<int64_t(catalog.size()*12);++day){home_shuffle_update(1,day+47);pair();if(day%catalog.size()==0)daily.clear();Check(std::find(daily.begin(),daily.end(),homeSelected[2])==daily.end(),"Day mode repeated within its bag");daily.push_back(homeSelected[2]);const auto chosen=homeSelected;Check(home_save(),"Daily bag save failed");home_init(fixture);home_shuffle_update(1,day+47);Check(homeSelected==chosen&&homeShuffleDayOrigin==47,"Daily bag restart changed assignment or anchor");}
        home_shuffle_mode(1);homeShuffleCounter=0;std::vector<std::string> power;
        for(uint64_t draw=0;draw<catalog.size()*12;++draw){if(draw%catalog.size()==0)power.clear();home_shuffle_update(draw+1000,100);pair();Check(std::find(power.begin(),power.end(),homeSelected[2])==power.end(),"Power mode repeated within its bag");power.push_back(homeSelected[2]);Check(home_save(),"Bag progress save failed");home_init(fixture);Check(homeShuffleCounter==draw+1,"Restart lost shuffle bag progress");}
        // Missing matching foreground uses none, then a transparent layer if absent.
        homeChoices[2].resize(1);const auto name=homeChoices[2][0].name;auto& top=homeChoices[3];top.erase(std::remove_if(top.begin(),top.end(),[&](const auto& c){return home_lower(c.name)==home_lower(name);}),top.end());home_shuffle_mode(1);home_shuffle_update(2,99);Check(homeSelected[3]=="none","Missing foreground did not select none");top.erase(std::remove_if(top.begin(),top.end(),[](const auto& c){return c.name=="none";}),top.end());home_shuffle_mode(1);home_shuffle_update(3,99);home_prepare();auto active=std::atomic_load(&homeLayers);Check(active&&active->pixels[3]&&std::all_of(active->pixels[3]->begin(),active->pixels[3]->end(),[](uint8_t n){return n==0;}),"Missing foreground did not become transparent");home_scan();
        home_shuffle_mode(0);auto manual=homeSelected;home_shuffle_update(999,999);Check(homeSelected==manual,"Off mode shuffled");home_shuffle_mode(2);home_cycle(2,1);Check(homeShuffleMode==0,"Manual background choice did not disable shuffle");
        settingsLockscreen=true;settings_action(600,500);Check(homeShuffleMode==1,"Power shuffle button missed");settings_action(800,500);Check(homeShuffleMode==2,"Day shuffle button missed");settings_action(420,500);Check(homeShuffleMode==0,"Off shuffle button missed");
        phonePower=1;phoneStartupReady=false;const auto serial=homePowerOnSerial.load();phone_power(true,10000);startup_elapsed(10000);Check(homePowerOnSerial==serial,"Loading preview emitted a power-on shuffle event");phoneStartupReady=true;startup_elapsed(11000);startup_elapsed(11100);phone_power(true,11200);Check(homePowerOnSerial==serial+1&&startup_total_ms()==5200,"Visible power-on not counted once / default duration incorrect");
        std::vector<uint8_t> menu;settings_render(menu,2,false);Save(out/"shuffle-menu.bmp",menu,1920,1440);std::cout<<"PASS: 3s/Day defaults, stable game-day assignment across restart/rewind, paired foregrounds, 50 nonrepeating power shuffles, exhaustive bags for 32 catalog sizes / 20 seeds / 40 cycles, daily and power bag exhaustion with restart after every draw, no menu reroll, persistence, manual/off behavior, missing foreground fallback and one event per visible power-on.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--lockscreen-duration"){
        using namespace bettergps_overlay;const auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);home_init(out);Check(homeDurationSeconds==3,"Default home hold must be three seconds");home_duration_adjust(-2);
        settingsLockscreen=true;settings_action(870,435);Check(homeDurationSeconds==2,"Duration + button missed");settings_action(420,435);Check(homeDurationSeconds==1,"Duration - button missed");settings_action(810,435);settings_action(475,435);Check(homeDurationSeconds==1,"Duration 10-second buttons missed");home_duration_adjust(9);Check(homeDurationSeconds==10&&home_save(),"Duration did not save");homeDurationSeconds=1;home_init(out);Check(homeDurationSeconds==10,"Duration did not persist");
        phonePower=1;phoneStartupReady=true;phone_power(true,10000);Check(startup_elapsed(10000)==0&&startup_total_ms()==12200,"Ten second duration did not extend total startup");
        load_phone_fonts();std::vector<uint8_t> original(360*740*4,0);for(size_t i=3;i<original.size();i+=4)original[i]=255;auto frame=[&](int elapsed){auto image=original;render_phone_startup(image,elapsed,2*1440+12*60+6);return image;};
        const auto home=frame(2400);Check(home!=original&&frame(11399)==home&&frame(11400)==home,"Ten second hold ended early");Check(frame(11800)!=home&&frame(12200)==original,"Swipe duration / completion changed");Check(startup_frame(2400)==startup_frame(11000),"Long static home hold kept redrawing at 30fps");
        home_duration_adjust(-9);Check(startup_total_ms()==12200,"Editing duration interrupted the current startup");phone_power(false,25000);phone_power(true,26000);startup_elapsed(26000);Check(startup_total_ms()==3200,"Next startup did not take new duration");Check(frame(2399)==frame(2400)&&frame(2800)!=frame(2400)&&frame(3200)==original,"One second hold not exact");
        home_duration_adjust(-1);phone_power(false,31000);phone_power(true,32000);startup_elapsed(32000);Check(startup_total_ms()==2200&&frame(2200)==original,"Zero hold did not retain fade/swipe");home_duration_adjust(99999);Check(homeDurationSeconds==3600,"Excessive duration not bounded");home_duration_adjust(-99999);Check(homeDurationSeconds==0,"Negative duration not bounded");
        std::vector<uint8_t> menu;settings_render(menu,2,false);Save(out/"duration-menu.bmp",menu,1920,1440);std::cout<<"PASS: 1s / 10s exact home hold, unchanged black/fade/swipe, persistence, next-startup application, menu +/- controls, static hold caching and safe bounds.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--lockscreen-style"){
        using namespace bettergps_overlay;const auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);const auto fixture=out/"fixture";const auto assets=fixture/"BetterGPS_assets/material/ui/phone_lockscreen";std::filesystem::create_directories(assets);
        std::filesystem::copy("BetterGPS/Overlay/BetterGPS_assets/material/ui/phone_lockscreen",assets,std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
        home_init(fixture);Check(homeChoices[0].size()==1&&homeChoices[1].size()==1&&homeChoices[2].size()>=23&&homeChoices[3].size()>=24,"Home catalog counts / suffix matching wrong");Check(homeSelected[0]=="iphone_lock"&&homeSelected[1]=="iphone_lock"&&homeSelected[2]=="mustang_2"&&homeSelected[3]=="mustang_2","Home defaults did not retain Mustang style");
        const auto originalBackgroundCount=homeChoices[2].size();HomeDecoder decoder;for(const auto& list:homeChoices)for(const auto& choice:list){auto pixels=decoder.image(choice.path);Check(pixels.size()==360*740*4,"Supplied DDS failed to decode");std::ofstream raw(out/(choice.path.stem().u8string()+".bgra"),std::ios::binary);raw.write((char*)pixels.data(),pixels.size());}
        const auto originalPngPixels=decoder.image(assets/"background/bronco_1_background.dds");Check(import_png(out/"probe_background.png",originalPngPixels),"Local PNG fixture encoding failed");auto png=decoder.image(out/"probe_background.png");Check(png==originalPngPixels,"WIC PNG decoding differs from original DDS pixels");
        home_prepare();auto active=std::atomic_load(&homeLayers);Check(active&&std::all_of(active->pixels.begin(),active->pixels.end(),[](const auto& p){return p&&p->size()==360*740*4;}),"Selected home layers not published");load_phone_fonts();std::vector<uint8_t> original(360*740*4),pixels;for(size_t i=3;i<original.size();i+=4)original[i]=255;pixels=original;render_phone_startup(pixels,1400,2*1440+12*60+6);Save(out/"home-default.bmp",pixels,360,740);pixels=original;render_phone_startup(pixels,1400,2*1440+12*60+6,true);Save(out/"home-night.bmp",pixels,360,740);
        settingsLockscreen=false;settings_action(600,40);Check(settingsLockscreen,"Lockscreen submenu not opened");settings_render(pixels,2,false);Save(out/"menu.bmp",pixels,1920,1440);settings_action(870,300);Check(homeSelected[2]!="mustang_2","Background selector did not advance");const auto selected=homeSelected;Check(home_save(),"Home preferences save failed");home_init(fixture);Check(homeSelected==selected,"Home preferences not restored");home_prepare();
        const auto before=std::atomic_load(&homeLayers);const auto bad=assets/"background/broken_background.dds";{std::ofstream f(bad);f<<"broken";}home_scan();{std::lock_guard<std::mutex> lock(homeMutex);homeSelected[2]="broken";++homeRevision;}home_prepare();Check(!homeError.empty()&&std::atomic_load(&homeLayers)==before,"Bad texture replaced a valid style / error hidden");std::filesystem::remove(bad);home_scan();home_prepare();
        const auto added=assets/"background/MixedCase_BACKGROUND.DDS";std::filesystem::copy_file(assets/"background/bronco_1_background.dds",added,std::filesystem::copy_options::overwrite_existing);homeRescan=true;home_prepare();Check(homeChoices[2].size()==originalBackgroundCount+1&&std::any_of(homeChoices[2].begin(),homeChoices[2].end(),[](const auto& c){return c.name=="MixedCase";}),"Runtime texture discovery / case-insensitive suffix failed");std::filesystem::remove(added);home_scan();
        settings_action(600,550);Check(!settingsLockscreen,"Home menu Back failed");
        // Exact source-over and Overlay order, including icons above the lock.
        auto solid=[](uint8_t b,uint8_t g,uint8_t r,uint8_t a){auto image=std::make_shared<HomePixels>(360*740*4);for(size_t i=0;i<image->size();i+=4){(*image)[i]=b;(*image)[i+1]=g;(*image)[i+2]=r;(*image)[i+3]=a;}return image;};
        auto synthetic=std::make_shared<HomeLayers>();synthetic->pixels={solid(15,160,240,128),solid(220,100,20,128),solid(40,80,120,255),solid(200,30,10,128)};synthetic->revision=homePublishedRevision+100;std::atomic_store(&homeLayers,std::shared_ptr<const HomeLayers>(synthetic));pixels=original;render_phone_startup(pixels,1400,-1);const size_t point=(650*360+10)*4;
        for(int c=0;c<3;++c){unsigned value=((*synthetic->pixels[3])[c]*128+(*synthetic->pixels[2])[c]*127+127)/255;value=(startup_overlay(value,(*synthetic->pixels[1])[c])*128+value*127+127)/255;value=((*synthetic->pixels[0])[c]*128+value*127+127)/255;Check(pixels[point+c]==value,"Home layer / normal / Overlay order incorrect");}
        synthetic->pixels[0]=solid(12,34,56,255);++synthetic->revision;pixels=original;render_phone_startup(pixels,1400,-1,true);const size_t lock=(55*360+180)*4;Check(pixels[lock]==12&&pixels[lock+1]==34&&pixels[lock+2]==56,"Normal icons not above lock or incorrectly dimmed at night");
        std::cout<<"PASS: Supplied BC7 DDS including unified icons decoded, suffix catalog/defaults, independent selectors, submenu, persistence, live rescan, invalid-file fallback, four-layer normal/Overlay order and night icon brightness.\n";return 0;
    }
    if(argc==3&&(std::string(argv[2])=="--cold-surface"||std::string(argv[2])=="--cold-surface-single"||std::string(argv[2])=="--cold-style")){
        using namespace bettergps_overlay;std::filesystem::create_directories(argv[1]);
        if(std::string(argv[2])=="--cold-style")home_init("validation/lockscreen-style/test/fixture");
        const bool single=std::string(argv[2])=="--cold-surface-single";
        Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Cold surface hooks failed");
        GpsDisplay entry;entry.id="road_trip_phone";entry.ready=entry.independent=true;entry.markerWidth=364;entry.surfaceWidth=37;entry.settings=overlay_snapshot();gpsDisplays.push_back(entry);g_surfaceBridges.push_back(std::make_unique<OverlayBridge>(OverlayBridge{gpsDisplays[0].id.c_str(),37,41}));
        ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,single?D3D11_CREATE_DEVICE_SINGLETHREADED:0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Cold surface device failed");
        HWND window=CreateWindowExW(0,L"STATIC",L"BetterGPS hidden Present test",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);Check(window!=nullptr,"Hidden Present window failed");
        IDXGIDevice* dxgi=nullptr;IDXGIAdapter* adapter=nullptr;IDXGIFactory* factory=nullptr;IDXGISwapChain* chain=nullptr;device->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi);dxgi->GetAdapter(&adapter);adapter->GetParent(__uuidof(IDXGIFactory),(void**)&factory);DXGI_SWAP_CHAIN_DESC swap{};swap.BufferDesc.Width=swap.BufferDesc.Height=64;swap.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;swap.SampleDesc.Count=1;swap.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;swap.BufferCount=1;swap.OutputWindow=window;swap.Windowed=TRUE;Check(SUCCEEDED(factory->CreateSwapChain(device,&swap,&chain)),"Real Present swapchain failed");factory->Release();adapter->Release();dxgi->Release();
        auto present=[&](){Check(SUCCEEDED(hook_Present(chain,0,0)),"Actual hooked Present failed");};
        std::vector<uint8_t> marker(37*41*4);for(size_t j=3;j<marker.size();j+=4)marker[j]=255;for(unsigned j=0;j<4;++j){marker[j*4]=0xA7;marker[j*4+1]=0x5B;marker[j*4+2]=0x3A+j;}
        D3D11_TEXTURE2D_DESC d{};d.Width=37;d.Height=41;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial{marker.data(),37*4,0};ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&d,&initial,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&view)),"Cold marker failed");
        auto bind=[&](){context->PSSetShaderResources(0,1,&view);};
        auto read=[&](){bind();ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);ID3D11Resource* resource=nullptr;actual->GetResource(&resource);actual->Release();ID3D11Texture2D* image=nullptr;resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&image);resource->Release();D3D11_TEXTURE2D_DESC copy{};image->GetDesc(&copy);copy.Usage=D3D11_USAGE_STAGING;copy.BindFlags=copy.MiscFlags=0;copy.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* stage=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&copy,nullptr,&stage)),"Cold readback failed");context->CopyResource(stage,image);D3D11_MAPPED_SUBRESOURCE mapped{};Check(SUCCEEDED(context->Map(stage,0,D3D11_MAP_READ,0,&mapped)),"Cold map failed");std::vector<uint8_t> result(size_t(copy.Width)*copy.Height*4);for(unsigned y=0;y<copy.Height;++y)memcpy(result.data()+size_t(y)*copy.Width*4,(uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch,copy.Width*4);context->Unmap(stage,0);stage->Release();image->Release();return result;};
        phonePower=0;displayPowerMode=0;displayManualPower=-1;phoneStartupReady=false;display_power_refresh();bind();Check(g_independentSurfaces.size()==1&&g_phoneScreens.empty(),"First off-contact display still requires native UI");present();Check(phonePower==2&&startup_elapsed(GetTickCount64()+90000)==0,"Loading-preview surface started animation before vehicle readiness");
        auto black=read();for(size_t i=0;i<black.size();i+=4)Check(black[i]==0&&black[i+1]==0&&black[i+2]==0&&black[i+3]==255,"Cold first frame flashed");
        display_ignition(false);bind();Check(startup_elapsed(GetTickCount64())<100,"First off telemetry did not start the full animation");
        // The real worker must awaken from the visible surface, with zero native UI draws.
        start_cpu_worker(false);phonePower=GetTickCount64()-1400+3;bind();auto deadline=GetTickCount64()+3000;
        while(!g_independentSurfaces[0]->power.captured&&GetTickCount64()<deadline){bind();present();std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        Check(g_independentSurfaces[0]->power.captured&&g_phoneScreens.empty(),"Background startup did not reach display without ignition");stop_cpu_worker();load_phone_fonts();
        uint64_t version=10000;
        auto frame=[&](int elapsed){phonePower=GetTickCount64()-elapsed+3;std::vector<uint8_t> canvas;render_configured_phone(canvas,&gpsDisplays[0].settings,1);render_phone_startup(canvas,elapsed,2*1440+12*60+6,false);auto pixels=std::make_shared<std::vector<uint8_t>>(364*740*4,0);for(unsigned y=0;y<740;++y)memcpy(pixels->data()+size_t(y)*364*4,canvas.data()+size_t(y)*360*4,360*4);{std::lock_guard<std::mutex> lock(g_prepared[0].mutex);g_prepared[0].pixels=pixels;g_prepared[0].version=++version;g_prepared[0].powerState=phonePower;g_prepared[0].powerPhase=elapsed;}bind();present();return read();};
        for(int cycle=0;cycle<3;++cycle){
            auto fade=frame(1100),home=frame(1400),swipe=frame(startupBlackMs+400+phoneStartupHoldMs.load()+400),gps=frame(startup_total_ms());
            Check(fade!=home&&home!=swipe&&swipe!=gps,"Off-contact startup lost fade-in or swipe frames");
            if(!cycle){auto save=[&](const char* name,std::vector<uint8_t> pixels){for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);Save(std::filesystem::path(argv[1])/name,pixels,256,512);};save("cold-fade.bmp",fade);save("cold-home.bmp",home);save("cold-swipe.bmp",swipe);save("cold-gps.bmp",gps);}
            phone_power(false);auto held=read();Check(held==gps,"Off-contact shutdown lost retained display");
            phonePower=phoneShutdownBit|(GetTickCount64()-1200);finish_phone_frame(context);auto dim=read();Check(dim!=held,"Off-contact fade-out stopped");
            phonePower=phoneShutdownBit|(GetTickCount64()-1500);auto off=read();for(size_t i=0;i<off.size();i+=4)Check(off[i]==0&&off[i+1]==0&&off[i+2]==0&&off[i+3]==255,"Cold shutdown not black");
        }
        // No image update means no compositor or upload on idle frames.
        auto& surface=*g_independentSurfaces[0];phonePower=surface.composedPowerState;const auto copied=surface.power.lastCopy;std::this_thread::sleep_for(std::chrono::milliseconds(20));bind();present();Check(surface.power.lastCopy==copied,"Idle display performed another composition");
        // Deliberately leave the preceding GPS and prepared layer available.
        // Cross the black interval without allowing the worker to publish a home frame.
        phone_power(false);phone_power(true);phonePower=GetTickCount64()-1100+3;bind();present();auto delayed=read();for(size_t i=0;i<delayed.size();i+=4)Check(delayed[i]==0&&delayed[i+1]==0&&delayed[i+2]==0&&delayed[i+3]==255,"Previous GPS flashed while first home frame was late");
        Check(g_phoneScreens.empty()&&g_independentSurfaces.size()==1,"Off-contact cycles manufactured native producers or accumulated surfaces");
        view->Release();texture->Release();uninstall_d3d_hooks();MH_Uninitialize();chain->Release();DestroyWindow(window);context->Release();device->Release();
        std::cout<<"PASS: cold first load with ignition always off, actual worker awakened by front surface, opaque initial black, home fade-in/swipe/GPS overlay, repeated off-contact restart and shutdown, no native UI producer.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--first-start-delay"){
        using namespace bettergps_overlay;phone_startup_session();phoneStartupReady=false;phone_power(true,1000);
        Check(startup_elapsed(1000)==0&&startup_elapsed(90000)==0&&phoneFirstWaitStart==0,"Loading preview consumed initial wait");
        phoneStartupReady=true;const auto serial=homePowerOnSerial.load();
        Check(startup_elapsed(100000)==0&&phonePower==2,"First phone did not stay armed during initial wait");
        Check(startup_elapsed(104999)==0&&phonePower==2&&homePowerOnSerial==serial,"Initial wait revealed startup too soon");
        Check(startup_elapsed(105000)==0&&phonePower>=3&&!phoneFirstWaitPending&&homePowerOnSerial==serial+1,"Startup did not begin after exactly five seconds");
        Check(startup_elapsed(106400)==1400,"Lockscreen fade timeline was shortened by the initial wait");
        phone_power(false,112000);startup_elapsed(114000);phone_power(true,115000);Check(startup_elapsed(115000)==0&&phonePower>=3&&!phoneFirstWaitPending,"Repeated power-on repeated five-second initial wait");
        phone_startup_session();phone_power(true,200000);startup_elapsed(200000);phone_startup_resume_wait(201000,211000);
        Check(startup_elapsed(214999)==0&&phonePower==2,"Pause consumed the session's initial wait");
        Check(startup_elapsed(215000)==0&&phonePower>=3,"Initial wait did not resume after pause");
        phoneFirstWaitPending=false;std::cout<<"PASS: exact five-second first phone startup after readiness, no loading-preview consumption, original fade/swipe timing, no repeat on later power cycles, rearmed on session load and frozen during pause.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--power-settings"){
        using namespace bettergps_overlay;std::filesystem::create_directories(argv[1]);
        const auto file=std::filesystem::path(argv[1])/"power.ini";
        {std::ofstream fresh(file);fresh<<"; empty config uses defaults\n";}
        phonePower=0;displayIgnition=-1;display_power_load(file);
        Check(displayPowerMode==0&&displayPowerKey==VK_F9&&displayPowerMods==1,"Power defaults differ from Always active / Shift+F9");
        Check(startup_elapsed(100)==0&&startup_elapsed(90000)==0&&phonePower==2,"Loading preview consumed startup before vehicle telemetry was ready");
        display_ignition(false,10000);Check(display_power_target()&&startup_elapsed(10000)==0,"Always active followed the ignition off");
        display_ignition(true,11000);display_ignition(false,12000);Check(startup_elapsed(15000)==startup_total_ms(),"Ignition edges restarted Always active");
        display_power_mode(1,16000);Check(!display_power_target()&&startup_elapsed(16000)==-3,"Ignition only did not start shutdown");
        Check(startup_elapsed(16999)==-1002&&startup_elapsed(17400)==-2,"Policy did not preserve shutdown hold/fade");
        display_ignition(true,18000);Check(startup_elapsed(18000)==0,"Ignition only did not start animation");
        display_ignition(true,18500);Check(startup_elapsed(18500)==500,"Repeated ignition restarted animation");
        display_power_mode(2,22000);Check(!display_power_target(),"Off policy still active");display_ignition(true,22100);Check(!display_power_target(),"Ignition bypassed Off");
        settingsOpen=false;settingsPowerKeyHeld=false;settings_power_key_input(true,true);Check(display_power_target(),"Shortcut did not override Off");
        auto state=phonePower.load();settings_power_key_input(true,true);Check(phonePower==state&&display_power_target(),"Held shortcut repeated");
        settings_power_key_input(false,true);settings_power_key_input(true,false);Check(display_power_target(),"Shortcut acted outside foreground game");
        settings_power_key_input(false,true);settingsOpen=true;settings_power_key_input(true,true);Check(display_power_target(),"Shortcut acted inside menu");settingsOpen=false;
        settings_power_key_input(false,true);settings_power_key_input(true,true);Check(!display_power_target(),"Second shortcut did not switch off");
        display_ignition(false);display_ignition(true);Check(!display_power_target(),"Ignition discarded manual override");
        settings_action(420,500);Check(displayPowerMode==0&&displayManualPower==-1&&display_power_target(),"Mode menu did not reset manual override");
        settings_action(600,500);Check(displayPowerMode==1,"Ignition only button missed");settings_action(800,500);Check(displayPowerMode==2,"Off button missed");
        settings_action(500,545);Check(settingsCapturePowerKey,"Shortcut capture button missed");
        Check(!display_key_valid(VK_F9,2)&&display_key_valid(VK_F9,3)&&!display_key_valid(VK_LSHIFT,0)&&display_key_valid('G',5),"Shortcut validation allowed Ctrl+F9 or rejected valid chord");
        display_power_shortcut('G',5);settingsCapturePowerKey=false;Check(display_key_name().find("Shift+Alt+")==0,"Shortcut label missing modifiers");
        Check(display_power_save(),"Power settings save failed");display_power_mode(0);display_power_shortcut(VK_F9,1);display_power_load(file);
        Check(displayPowerMode==2&&displayPowerKey=='G'&&displayPowerMods==5&&displayManualPower==-1&&!display_power_target(),"Power settings did not persist or manual override leaked across session");
        settings_action(870,545);Check(displayPowerKey==0&&display_power_save(),"Shortcut disable failed");display_power_load(file);Check(displayPowerKey==0,"Disabled shortcut did not persist");
        {std::ofstream corrupt(file);corrupt<<"Mode=99\nToggleKey=120\nToggleModifiers=2\n";}display_power_load(file);Check(displayPowerMode==0&&displayPowerKey==VK_F9&&displayPowerMods==1,"Invalid config not sanitized");
        std::vector<uint8_t> menu;settings_render(menu,2,false);Save(std::filesystem::path(argv[1])/"power-menu.bmp",menu,1920,1440);
        std::cout<<"PASS: power modes, default Always active, ignition isolation, hold/fade/startup, manual override, hotkey edge/focus/menu guards, reserved key validation, menu actions and persistent configuration.\n";return 0;
    }
    if(argc==3&&(std::string(argv[2])=="--power-gpu"||std::string(argv[2])=="--power-gpu-srgb"||std::string(argv[2])=="--power-gpu-srgb-single"||std::string(argv[2])=="--surface-gpu")){
        const bool independent=std::string(argv[2])=="--surface-gpu";const bool singleThread=std::string(argv[2])=="--power-gpu-srgb-single";const bool nativeSrgb=independent||singleThread||std::string(argv[2])=="--power-gpu-srgb";
        using namespace bettergps_overlay;Check(MH_Initialize()==MH_OK&&install_d3d_hooks(),"Power GPU hook setup failed");
        if(independent){GpsDisplay display;display.id="road_trip_phone";display.ready=display.independent=true;display.markerWidth=364;display.surfaceWidth=37;display.settings=overlay_snapshot();gpsDisplays.push_back(display);g_displayBridges.push_back(std::make_unique<OverlayBridge>(OverlayBridge{gpsDisplays[0].id.c_str(),364,740}));g_surfaceBridges.push_back(std::make_unique<OverlayBridge>(OverlayBridge{gpsDisplays[0].id.c_str(),37,41}));}
        auto* uiBridge=independent?g_displayBridges[0].get():&g_skinBridge;const unsigned uiWidth=uiBridge->width;
        ID3D11Texture2D* surfaceMarker=nullptr;ID3D11ShaderResourceView* surfaceMarkerView=nullptr;ID3D11Texture2D* marker=nullptr;ID3D11ShaderResourceView* markerView=nullptr;
        ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;D3D_FEATURE_LEVEL level{};
        Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,singleThread?D3D11_CREATE_DEVICE_SINGLETHREADED:0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"Power WARP device failed");
        D3D11_TEXTURE2D_DESC d{};d.Width=256;d.Height=512;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        if(nativeSrgb){d.MipLevels=10;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;d.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;}
        ID3D11Texture2D* native=nullptr;ID3D11ShaderResourceView* view=nullptr;ID3D11RenderTargetView* target=nullptr;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&native))&&SUCCEEDED(device->CreateShaderResourceView(native,nullptr,&view))&&SUCCEEDED(device->CreateRenderTargetView(native,nullptr,&target)),"Native drawable allocation failed");
        ID3D11Texture2D* geometryTarget=nullptr;ID3D11RenderTargetView* geometryView=nullptr;
        auto geometryDesc=d;geometryDesc.Width=320;geometryDesc.Height=640;geometryDesc.MipLevels=1;geometryDesc.MiscFlags=0;
        Check(SUCCEEDED(device->CreateTexture2D(&geometryDesc,nullptr,&geometryTarget))&&SUCCEEDED(device->CreateRenderTargetView(geometryTarget,nullptr,&geometryView)),"Consumer target allocation failed");
        ID3D11Texture2D* sameSizeTarget=nullptr;ID3D11RenderTargetView* sameSizeView=nullptr;auto sameSizeDesc=geometryDesc;sameSizeDesc.Width=256;sameSizeDesc.Height=512;
        Check(SUCCEEDED(device->CreateTexture2D(&sameSizeDesc,nullptr,&sameSizeTarget))&&SUCCEEDED(device->CreateRenderTargetView(sameSizeTarget,nullptr,&sameSizeView)),"Same-sized consumer allocation failed");
        auto* mainGeometryView=geometryView;
        bool finalGeometry=false;PhoneScreenPower* ownedScreen=nullptr;
        auto bind=[&](ID3D11ShaderResourceView* source){if(independent&&finalGeometry&&source&&startup_elapsed(GetTickCount64())>=startupBlackMs){context->OMSetRenderTargets(1,&target,nullptr);context->PSSetShaderResources(0,1,&markerView);context->OMSetRenderTargets(1,&geometryView,nullptr);}if(finalGeometry)context->OMSetRenderTargets(1,&geometryView,nullptr);context->PSSetShaderResources(0,1,&source);
            if(independent&&finalGeometry&&source){ID3D11ShaderResourceView* nativeBound=nullptr;context->PSGetShaderResources(0,1,&nativeBound);Check(nativeBound==source,"Independent display modified its native producer binding");nativeBound->Release();context->PSSetShaderResources(0,1,&surfaceMarkerView);Check(!g_independentSurfaces.empty(),"Own display not initialized");ownedScreen=&g_independentSurfaces[0]->power;if(startup_elapsed(GetTickCount64())>=startupBlackMs){{std::lock_guard<std::mutex> lock(g_prepared[0].mutex);if(g_prepared[0].powerState!=phonePower){g_prepared[0].powerState=phonePower;g_prepared[0].powerPhase=startup_elapsed(GetTickCount64());++g_prepared[0].version;}}finish_phone_frame(context);context->PSSetShaderResources(0,1,&surfaceMarkerView);}}
            ID3D11ShaderResourceView* bound=nullptr;context->PSGetShaderResources(0,1,&bound);return bound;};
        auto pixel=[&](ID3D11ShaderResourceView* source,UINT mip=0,UINT x=0,UINT y=0){ID3D11Resource* resource=nullptr;source->GetResource(&resource);ID3D11Texture2D* texture=nullptr;resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture);resource->Release();D3D11_TEXTURE2D_DESC copy{};texture->GetDesc(&copy);copy.Usage=D3D11_USAGE_STAGING;copy.CPUAccessFlags=D3D11_CPU_ACCESS_READ;copy.BindFlags=copy.MiscFlags=0;ID3D11Texture2D* stage=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&copy,nullptr,&stage)),"Power readback allocation failed");context->CopyResource(stage,texture);D3D11_MAPPED_SUBRESOURCE mapped{};Check(SUCCEEDED(context->Map(stage,mip,D3D11_MAP_READ,0,&mapped)),"Power readback failed");std::array<uint8_t,4> color;memcpy(color.data(),static_cast<uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch+x*4,4);context->Unmap(stage,mip);stage->Release();texture->Release();return color;};
        phonePower=1;auto* unrelated=bind(view);Check(unrelated==view,"Power control captured unrelated 256x512 texture");unrelated->Release();bind(nullptr);
        std::vector<uint8_t> bytes(size_t(uiWidth)*740*4);for(unsigned j=0;j<4;++j){bytes[j*4]=0xA7;bytes[j*4+1]=0x5B;bytes[j*4+2]=0x37+j;bytes[j*4+3]=255;}
        d.Width=uiWidth;d.Height=740;d.MipLevels=1;d.MiscFlags=0;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{};data.pSysMem=bytes.data();data.SysMemPitch=uiWidth*4;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&marker))&&SUCCEEDED(device->CreateShaderResourceView(marker,nullptr,&markerView)),"Phone marker allocation failed");
        Check(!uiBridge->liveView,"Empty streamed marker was accepted without its signature");
        g_origUpdateSubresource(context,marker,0,nullptr,bytes.data(),uiWidth*4,0);start_cpu_worker(false);
        context->OMSetRenderTargets(1,&target,nullptr);auto* markerBound=bind(markerView);markerBound->Release();context->OMSetRenderTargets(0,nullptr,nullptr);bind(nullptr);
        stop_cpu_worker();if(independent){std::lock_guard<std::mutex> lock(g_prepared[0].mutex);g_prepared[0].pixels=std::make_shared<std::vector<uint8_t>>(364*740*4,0);g_prepared[0].version=99;}Check(g_phoneScreens.size()==1,"First streamed marker binding did not identify the drawable in the same frame");if(singleThread)Check(g_phoneScreens[0]->isolatedContext&&!g_phoneScreens[0]->deferred,"Single-threaded compatibility path was not exercised");
        Check(pixel(uiBridge->liveView)==std::array<uint8_t,4>{0,0,0,255},"New overlay exposes uninitialized pixels");finalGeometry=true;
        if(independent){D3D11_TEXTURE2D_DESC sd=d;sd.Width=37;sd.Height=41;std::vector<uint8_t> pixels(37*41*4);for(size_t j=3;j<pixels.size();j+=4)pixels[j]=255;for(unsigned j=0;j<4;++j){pixels[j*4]=0xA7;pixels[j*4+1]=0x5B;pixels[j*4+2]=0x3A+j;}D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=pixels.data();initial.SysMemPitch=37*4;Check(SUCCEEDED(device->CreateTexture2D(&sd,&initial,&surfaceMarker))&&SUCCEEDED(device->CreateShaderResourceView(surfaceMarker,nullptr,&surfaceMarkerView)),"Independent surface marker failed");}
        const float color[4]={0.8f,0.4f,0.2f,1};context->ClearRenderTargetView(target,color);
        std::vector<uint8_t> corners(256*512*4);
        for(UINT y=0;y<512;++y)for(UINT x=0;x<256;++x){auto* p=corners.data()+(y*256+x)*4;p[0]=x<128?64:192;p[1]=y<256?96:224;p[2]=(x<128)==(y<256)?160:48;p[3]=255;}
        context->UpdateSubresource(native,0,nullptr,corners.data(),256*4,0);
        if(nativeSrgb)context->GenerateMips(view);
        D3D11_TEXTURE2D_DESC frontDesc{};native->GetDesc(&frontDesc);frontDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;frontDesc.MiscFlags=0;if(nativeSrgb)frontDesc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
        ID3D11Texture2D* front=nullptr;ID3D11ShaderResourceView* frontView=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&frontDesc,nullptr,&front)),"Native front buffer allocation failed");
        D3D11_SHADER_RESOURCE_VIEW_DESC frontSrv{};frontSrv.Format=nativeSrgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_B8G8R8A8_UNORM;frontSrv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;frontSrv.Texture2D.MipLevels=frontDesc.MipLevels;
        Check(SUCCEEDED(device->CreateShaderResourceView(front,&frontSrv,&frontView)),"Native front view allocation failed");context->CopyResource(front,native);Check(phone_screen(front),"Native drawable copy was not followed");
        std::vector<std::array<uint8_t,4>> mipColors;for(UINT mip=0;mip<(nativeSrgb?10u:1u);++mip)mipColors.push_back(pixel(frontView,mip));
        if(nativeSrgb)for(UINT mip=1;mip<10;++mip){auto w=std::max(1u,256u>>mip),h=std::max(1u,512u>>mip);std::vector<uint8_t> invalid(size_t(w)*h*4);for(size_t i=0;i<invalid.size();i+=4){invalid[i+1]=invalid[i+3]=255;}context->UpdateSubresource(front,mip,nullptr,invalid.data(),w*4,0);}

        view->Release();view=frontView;if(!independent)ownedScreen=phone_screen(front);
        phone_power(true);auto* black=bind(view);Check(black!=view&&pixel(black)==std::array<uint8_t,4>{0,0,0,255},"Initial native GPS flash was not blocked");if(nativeSrgb&&!independent)for(UINT mip=0;mip<10;++mip)Check(pixel(black,mip)==std::array<uint8_t,4>{0,0,0,255},"Startup exposed uninitialized mip level");black->Release();bind(nullptr);
        geometryView=sameSizeView;auto* sameSizeBlack=bind(view);Check(sameSizeBlack!=view&&pixel(sameSizeBlack)==std::array<uint8_t,4>{0,0,0,255},"Same-sized consumer bypassed startup control (v5 regression)");sameSizeBlack->Release();bind(nullptr);geometryView=mainGeometryView;
        if(nativeSrgb){D3D11_SHADER_RESOURCE_VIEW_DESC mipViewDesc{};mipViewDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;mipViewDesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;mipViewDesc.Texture2D.MostDetailedMip=1;mipViewDesc.Texture2D.MipLevels=9;ID3D11ShaderResourceView* mipView=nullptr;Check(SUCCEEDED(device->CreateShaderResourceView(native,&mipViewDesc,&mipView)),"Producer mip SRV allocation failed");context->OMSetRenderTargets(1,&target,nullptr);context->PSSetShaderResources(0,1,&mipView);ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==mipView,"Power control modified the native mip producer");actual->Release();mipView->Release();ID3D11ShaderResourceView* noView=nullptr;context->PSSetShaderResources(0,1,&noView);context->OMSetRenderTargets(0,nullptr,nullptr);}
        D3D11_QUERY_DESC capturePredicateDesc{D3D11_QUERY_OCCLUSION_PREDICATE,0};ID3D11Predicate* capturePredicate=nullptr;Check(SUCCEEDED(device->CreatePredicate(&capturePredicateDesc,&capturePredicate)),"Capture predicate allocation failed");context->Begin(capturePredicate);context->End(capturePredicate);context->SetPredication(capturePredicate,TRUE);
        phonePower=GetTickCount64()-10000+3;auto* on=bind(view);
        ID3D11Predicate* retainedPredicate=nullptr;BOOL retainedValue=FALSE;context->GetPredication(&retainedPredicate,&retainedValue);Check(retainedPredicate==capturePredicate&&retainedValue==TRUE,"Snapshot changed native predication");retainedPredicate->Release();context->SetPredication(nullptr,FALSE);capturePredicate->Release();Check(independent?on==ownedScreen->snapshotView:on==view,"Independent material did not display its owned image");auto original=pixel(on);on->Release();bind(nullptr);finish_phone_frame(context);
        const std::array<std::pair<UINT,UINT>,4> points={{{8,8},{247,8},{8,503},{247,503}}};
        auto checkCorners=[&](ID3D11ShaderResourceView* image,bool faded){for(auto [x,y]:points){auto actual=pixel(image,0,x,y);auto* expected=corners.data()+(y*256+x)*4;for(int c=0;c<3;++c){double v=expected[c]/255.0;if(faded){if(nativeSrgb)v=v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);v*=.5;if(nativeSrgb)v=v<=.0031308?12.92*v:1.055*std::pow(v,1/2.4)-.055;}Check(std::abs(int(actual[c])-int(std::lround(v*255)))<=4,"Screen orientation/color changed at an asymmetric corner");}Check(actual[3]==255,"Phone output became transparent");}};
        if(independent){
            renderGameplayEnabled=true;renderGameplayActive=true;g_drivingStarted=true;
            const auto surfaceCount=g_independentSurfaces.size(),screenCount=g_phoneScreens.size();
            const auto composed=g_independentSurfaces[0]->composed,prepared=g_prepared[0].version;
            on_theme_session(SCS_TELEMETRY_EVENT_paused,nullptr,nullptr);
            Check(!render_gpu_allowed()&&!g_cpuRunning&&!g_cpuWorker.joinable(),"Driving panel restarted rendering/worker");
            auto pausedBind=[&](ID3D11ShaderResourceView* source,ID3D11ShaderResourceView* expected){context->PSSetShaderResources(0,1,&source);ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==expected,"Paused cabin screen disappeared or changed unrelated binding");if(actual)actual->Release();};
            for(unsigned i=0;i<64;++i){pausedBind(surfaceMarkerView,ownedScreen->snapshotView);pausedBind(view,view);}
            checkCorners(ownedScreen->snapshotView,false);
            Check(g_independentSurfaces.size()==surfaceCount&&g_phoneScreens.size()==screenCount&&g_independentSurfaces[0]->composed==composed&&g_prepared[0].version==prepared,"Paused display recomposed or allocated resources");
            renderActivityEnabled=true;renderVisible=false;pausedBind(surfaceMarkerView,surfaceMarkerView);renderVisible=true;
            renderOccluded=true;pausedBind(surfaceMarkerView,surfaceMarkerView);renderOccluded=false;
            renderDeviceLost=true;pausedBind(surfaceMarkerView,surfaceMarkerView);renderDeviceLost=false;
            renderStopping=true;pausedBind(surfaceMarkerView,surfaceMarkerView);renderStopping=false;
            renderActivityEnabled=false;renderGameplayEnabled=false;
            ID3D11ShaderResourceView* reloadedSurface=nullptr;Check(SUCCEEDED(device->CreateShaderResourceView(surfaceMarker,nullptr,&reloadedSurface)),"Reloaded surface view failed");
            surfaceMarkerView->Release();surfaceMarkerView=reloadedSurface;
            context->PSSetShaderResources(0,1,&surfaceMarkerView);
            renderGameplayEnabled=true;renderGameplayActive=false;pausedBind(surfaceMarkerView,ownedScreen->snapshotView);renderGameplayEnabled=false;

            displayPolicyReady=true;displaySeenMask=1;displayEnabledMask=0;displayIgnition=1;
            auto* nativeOnly=bind(view);Check(pixel(nativeOnly)==std::array<uint8_t,4>{0,0,0,0},"Original mode left an opaque front over the native display");
            SettingsMenuRenderer nativePresenter;Check(nativePresenter.start(device),"Native handoff presenter failed");auto shader=display_bytes(std::filesystem::path(__FILE__).parent_path().parent_path()/"generated/test-native-surface.cso");nativePresenter.ps->Release();nativePresenter.ps=nullptr;Check(SUCCEEDED(device->CreatePixelShader(shader.data(),shader.size(),nullptr,&nativePresenter.ps)),"Alpha-test shader failed");
            ID3D11ShaderResourceView* resultView=nullptr;Check(SUCCEEDED(device->CreateShaderResourceView(geometryTarget,nullptr,&resultView)),"Native handoff readback failed");
            for(int n=0;n<16;++n){
                Check(nativePresenter.render(context,view,geometryTarget,0,0,320,640,g_origPSSetSRV),"Native backing did not draw");auto before=pixel(resultView,0,100,300);
                Check(nativePresenter.render(context,nativeOnly,geometryTarget,0,0,320,640,g_origPSSetSRV),"Original front draw failed");Check(pixel(resultView,0,100,300)==before&&before[3]==255&&before[2]!=0,"Original mode changed or blacked out the actual native backing");
                Check(nativePresenter.render(context,ownedScreen->blackView,geometryTarget,0,0,320,640,g_origPSSetSRV),"Enabled front draw failed");Check(pixel(resultView,0,100,300)==std::array<uint8_t,4>{0,0,0,255},"Opaque BetterGPS front stopped covering the native plane");
            }resultView->Release();nativePresenter.stop();nativeOnly->Release();
            const auto idleCompose=g_independentSurfaces[0]->composed;for(int n=0;n<12;++n)finish_phone_frame(context);Check(g_independentSurfaces[0]->composed==idleCompose,"Disabled display still composed GPU images");
            displayIgnition=0;auto* nativeOff=bind(view);Check(pixel(nativeOff)==std::array<uint8_t,4>{0,0,0,0},"Original off mode hid the native renderer");nativeOff->Release();displayIgnition=1;
            nativeOnly=bind(view);renderGameplayEnabled=true;renderGameplayActive=false;pausedBind(surfaceMarkerView,nativeOnly);renderGameplayEnabled=false;nativeOnly->Release();
            displayPolicyReady=false;displayGlobalEnabled=false;nativeOnly=bind(view);Check(pixel(nativeOnly)[3]==0,"Global OFF did not bypass the independent front");nativeOnly->Release();displayGlobalEnabled=true;
            displayPolicyReady=false;displayEnabledMask=~uint32_t(0);displayIgnition=-1;
            gpsDisplays[0].nativeFallback=true;uiBridge->cleared=false;std::atomic<bool> clearDirty{true};update_bridge_on_render_thread(context,*uiBridge,clearDirty,"",0);Check(pixel(uiBridge->liveView)==std::array<uint8_t,4>{0,0,0,0},"Native producer marker hid the preserved native interface");
            std::cout<<"PASS: paused cabin retains exact phone image over 64 panel bindings, with stopped worker/no composition; hidden, occluded, lost-device and shutdown remain native.\n";
        }
        // ATS can recycle its native image after drawing it but before Present.
        // Only the image copied at consumption must survive this late overwrite.
        {const float scratch[4]={0,1,0,1};context->ClearRenderTargetView(target,scratch);context->CopyResource(front,native);finish_phone_frame(context);}
        phone_power(false);const float empty[4]={0,0,0,1};context->ClearRenderTargetView(target,empty);
        context->CopySubresourceRegion(front,0,0,0,0,native,0,nullptr);
        auto* held=bind(view);Check(held!=view&&pixel(held)==original,"Native power-off erased retained full phone image");Check(ownedScreen->snapshot!=nullptr,"Retained base image missing");checkCorners(held,false);held->Release();bind(nullptr);
        // Exercise the real Draw hook and the game's UV/perspective path,
        // rather than only inspecting which SRV the scope chooses.
        SettingsMenuRenderer presenter;Check(presenter.render(context,independent?surfaceMarkerView:view,geometryTarget,0,0,320,640,hook_PSSetShaderResources),"Final geometry draw failed");
        ID3D11ShaderResourceView* geometrySrv=nullptr;Check(SUCCEEDED(device->CreateShaderResourceView(geometryTarget,nullptr,&geometrySrv)),"Geometry SRV allocation failed");
        for(auto [x,y]:points){auto actual=pixel(geometrySrv,0,x*320/256,y*640/512);auto* expected=corners.data()+(y*256+x)*4;for(int c=0;c<4;++c)Check(std::abs(int(actual[c])-int(expected[c]))<=2,"Actual draw changed retained screen orientation");}Check(presenter.render(context,independent?surfaceMarkerView:view,geometryTarget,0,0,64,128,hook_PSSetShaderResources),"Minified held screen draw failed");for(auto [x,y]:points){auto actual=pixel(geometrySrv,0,x*64/256,y*128/512);auto* expected=corners.data()+(y*256+x)*4;for(int c=0;c<4;++c)Check(std::abs(int(actual[c])-int(expected[c]))<=2,"Untrusted native mip leaked into held phone");}geometrySrv->Release();presenter.stop();
        D3D11_VIEWPORT engineViewport{11,17,201,302,0,1};context->RSSetViewports(1,&engineViewport);
        ID3D11RenderTargetView* engineTarget=nullptr;context->OMGetRenderTargets(1,&engineTarget,nullptr);
        auto* engineSlot=ownedScreen->outputView;context->PSSetShaderResources(7,1,&engineSlot);context->CSSetShaderResources(3,1,&engineSlot);
        phonePower=phoneShutdownBit|(GetTickCount64()-1200);
        D3D11_QUERY_DESC predicateDesc{D3D11_QUERY_OCCLUSION_PREDICATE,0};ID3D11Predicate* enginePredicate=nullptr;Check(SUCCEEDED(device->CreatePredicate(&predicateDesc,&enginePredicate)),"Engine predicate allocation failed");context->Begin(enginePredicate);context->End(enginePredicate);context->SetPredication(enginePredicate,TRUE);
        finish_phone_frame(context);
        D3D11_VIEWPORT retainedViewport{};UINT viewportCount=1;context->RSGetViewports(&viewportCount,&retainedViewport);Check(viewportCount==1&&memcmp(&retainedViewport,&engineViewport,sizeof(engineViewport))==0,"Phone fade changed engine viewport");ID3D11RenderTargetView* retainedTarget=nullptr;context->OMGetRenderTargets(1,&retainedTarget,nullptr);Check(retainedTarget==engineTarget,"Phone fade changed engine render target");if(retainedTarget)retainedTarget->Release();if(engineTarget)engineTarget->Release();
        ID3D11Predicate* actualPredicate=nullptr;BOOL predicateValue=FALSE;context->GetPredication(&actualPredicate,&predicateValue);Check(actualPredicate==enginePredicate&&predicateValue==TRUE,"Phone fade changed engine predication");actualPredicate->Release();context->SetPredication(nullptr,FALSE);enginePredicate->Release();auto* fading=bind(view);
        ID3D11ShaderResourceView* savedSlot=nullptr;context->PSGetShaderResources(7,1,&savedSlot);Check(savedSlot==engineSlot,"Phone fade cleared engine PS slot 7 (environment flicker regression)");savedSlot->Release();context->CSGetShaderResources(3,1,&savedSlot);Check(savedSlot==engineSlot,"Phone fade cleared engine CS slot 3");savedSlot->Release();
        ID3D11ShaderResourceView* emptySlot=nullptr;context->PSSetShaderResources(7,1,&emptySlot);context->CSSetShaderResources(3,1,&emptySlot);auto middle=pixel(fading);checkCorners(fading,true);fading->Release();bind(nullptr);
        const double encoded=original[2]/255.0;const double linear=encoded<=0.04045?encoded/12.92:std::pow((encoded+0.055)/1.055,2.4);const double dimmed=linear/2;
        const int half=nativeSrgb?int(std::lround(255*(dimmed<=0.0031308?12.92*dimmed:1.055*std::pow(dimmed,1/2.4)-0.055))):int(original[2])/2;
        Check(middle[3]==255&&std::abs(int(middle[2])-half)<=4,"Final drawable fade is not opaque half brightness");
        if(nativeSrgb&&!independent)for(UINT mip=0;mip<10;++mip){double v=mipColors[mip][2]/255.0;v=v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);v*=.5;v=v<=.0031308?12.92*v:1.055*std::pow(v,1/2.4)-.055;Check(std::abs(int(pixel(ownedScreen->outputView,mip)[2])-int(std::lround(v*255)))<=4,"Fade did not regenerate native mip chain");}
        phonePower=phoneShutdownBit|(GetTickCount64()-1500);auto* off=bind(view);Check(pixel(off)==std::array<uint8_t,4>{0,0,0,255},"Final native drawable not fully black");if(nativeSrgb&&!independent)for(UINT mip=0;mip<10;++mip)Check(pixel(off,mip)==std::array<uint8_t,4>{0,0,0,255},"Off screen retained visible mip content");off->Release();bind(nullptr);
        // More cycles than the diagnostic budget and resource-registration cap.
        // Missing telemetry cannot reset the latched electrical state.
        for(unsigned cycle=0;cycle<64;++cycle){geometryView=cycle%2?sameSizeView:mainGeometryView;context->UpdateSubresource(native,0,nullptr,corners.data(),256*4,0);if(nativeSrgb)context->GenerateMips(phone_screen(native)->nativeView);context->CopyResource(front,native);
            phonePower=GetTickCount64()-10000+3;ownedScreen->lastCopy=0;auto* live=bind(view);Check(independent?live==ownedScreen->snapshotView:live==view,"Repeated ignition lost native GPS");live->Release();bind(nullptr);ownedScreen->lastCopy=0;finish_phone_frame(context);
            phone_power(false);const auto latched=phonePower.load();electric_enabled_cb(nullptr,0,nullptr,nullptr);Check(phonePower==latched,"Transient telemetry canceled shutdown");context->ClearRenderTargetView(target,empty);context->CopySubresourceRegion(front,0,0,0,0,native,0,nullptr);
            auto* keep=bind(view);checkCorners(keep,false);keep->Release();bind(nullptr);
            phonePower=phoneShutdownBit|(GetTickCount64()-1200);finish_phone_frame(context);auto* fade=bind(view);checkCorners(fade,true);fade->Release();bind(nullptr);
            phonePower=phoneShutdownBit|(GetTickCount64()-1500);auto* end=bind(view);Check(pixel(end)==std::array<uint8_t,4>{0,0,0,255},"Repeated shutdown lost black screen");end->Release();bind(nullptr);
        }
        Check(g_phoneScreens.size()==2,"Repeated ignition accumulated GPU screens");
        // A drawable can be replaced/reallocated at the power edge. Use the
        // most recent retained image for this registered display in that case.
        phonePower=phoneShutdownBit|(GetTickCount64()-100);
        D3D11_TEXTURE2D_DESC replacementDesc{};native->GetDesc(&replacementDesc);ID3D11Texture2D* replacementTexture=nullptr;ID3D11ShaderResourceView* replacementView=nullptr;ID3D11RenderTargetView* replacementTarget=nullptr;
        Check(SUCCEEDED(device->CreateTexture2D(&replacementDesc,nullptr,&replacementTexture))&&SUCCEEDED(device->CreateShaderResourceView(replacementTexture,nullptr,&replacementView))&&SUCCEEDED(device->CreateRenderTargetView(replacementTexture,nullptr,&replacementTarget)),"Reallocated native drawable failed");
        context->ClearRenderTargetView(replacementTarget,empty);context->OMSetRenderTargets(1,&replacementTarget,nullptr);context->PSSetShaderResources(0,1,&markerView);
        Check(phone_screen(replacementTexture)&&!phone_screen(replacementTexture)->captured,"Replacement unexpectedly has a snapshot");
        auto* replacementHeld=bind(replacementView);checkCorners(replacementHeld,false);replacementHeld->Release();bind(nullptr);
        replacementTarget->Release();replacementView->Release();replacementTexture->Release();
        if(surfaceMarkerView)surfaceMarkerView->Release();if(surfaceMarker)surfaceMarker->Release();sameSizeView->Release();sameSizeTarget->Release();mainGeometryView->Release();geometryTarget->Release();markerView->Release();marker->Release();target->Release();view->Release();front->Release();native->Release();uninstall_d3d_hooks();MH_Uninitialize();context->Release();device->Release();phonePower=0;
        std::cout<<"PASS: initialized black, verified drawable identity, blocked native startup flash, native clear bypass, retained asymmetric screen, unchanged orientation, isolated end-of-frame fade, engine PS/CS slots and predication preserved, native base image retained before reuse, 64 electrical cycles.\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--startup"){
        using namespace bettergps_overlay;std::filesystem::create_directories(argv[1]);
        Check(startup_images()[0].size()==360*740*4&&startup_images()[1].size()==360*740*4,"Startup resources missing");
        Check(startup_unlock().pixels&&startup_unlock().count==22&&startup_unlock().width==24&&startup_unlock().height==24,"Unlock resources missing");
        Check(startup_unlock_frame(0,22)==0&&startup_unlock_frame(499,22)==0&&startup_unlock_frame(500,22)==1&&startup_unlock_frame(533,22)==1&&startup_unlock_frame(534,22)==2,"Unlock did not hold 0.5 seconds or play at 30fps");
        Check(startup_unlock_frame(1167,22)==21&&startup_unlock_frame(2199,22)==21,"Final unlock frame was not held");
        phonePower=0;Check(startup_elapsed(10000)==-1,"Missing telemetry hid GPS");
        phone_power(false);Check(startup_elapsed(10000)==-2,"Ignition off not black");
        phone_power(true);Check(phonePower==2,"Ignition failed to arm");
        Check(startup_elapsed(20000)==0,"Startup elapsed before screen visible");
        phone_power(true);Check(startup_elapsed(20400)==400,"Repeated electric value restarted animation");
        Check(startup_elapsed(90000)==startup_total_ms()&&startup_frame(startup_total_ms())==startup_frame(99000),"Completed animation continues invalidating texture");
        phone_power(false);phone_power(true);Check(startup_elapsed(91000)==0,"Second ignition did not restart");
        std::vector<uint8_t> original;render_configured_phone(original);
        phonePower=0;phone_power(true,10000);Check(startup_elapsed(10000)==0,"Power-on did not start");
        phone_power(false,15000);const auto shutdownState=phonePower.load();phone_power(false,15500);Check(phonePower==shutdownState,"Repeated off telemetry restarted shutdown");
        auto retained=original;render_phone_startup(retained,startup_elapsed(15999));Check(retained==original,"Shutdown did not retain display for a full second");
        auto fading=original;render_phone_startup(fading,startup_elapsed(16200));
        Check(fading!=original&&fading[3]>=127,"Shutdown fade failed to cover underlying map");
        auto off=original;render_phone_startup(off,startup_elapsed(16400));for(size_t i=0;i<off.size();i+=4)Check(off[i]==0&&off[i+1]==0&&off[i+2]==0&&off[i+3]==255,"Shutdown left content behind black");
        Check(startup_frame(startup_elapsed(15000))==startup_frame(startup_elapsed(15999)),"Shutdown hold refreshes needlessly");
        phone_power(true,16500);Check(phonePower==2&&startup_elapsed(16500)==0,"Reignition during shutdown did not restart startup");
        phone_power(false,16600);Check(phoneShutdownVisual==100,"Power cut lost the startup phase");phonePower=0;
        Check(load_phone_fonts()&&g_phoneFonts[2],"Embedded clock font missing");
        Check(startup_clock(8999LL*7*1440).date==L"Mon \x00b7 Week 9000"&&startup_clock(9000LL*7*1440).date==L"Mon \x00b7 Week 9000+"&&startup_clock(50000LL*7*1440).date==L"Mon \x00b7 Week 9000+","Week display cap incorrect");
        for(auto minute:{0LL,98LL*7*1440,8999LL*7*1440,9000LL*7*1440}){
            std::vector<uint8_t> mask(360*740);startup_text(mask,startup_clock(minute).date,L"Google Sans",{60,82,300,99},1.0,FW_NORMAL,0.8);
            int left=360,right=-1;for(int y=82;y<99;++y)for(int x=0;x<360;++x)if(mask[y*360+x]){left=std::min(left,x);right=std::max(right,x);}
            Check(right>left&&std::abs((left+right)/2.0-179.5)<=1,"Week line is not centered");
        }
        const int64_t gameMinutes=2*1440+12*60+6;
        Check(startup_clock(gameMinutes).time==L"12:06"&&startup_clock(gameMinutes).period==L"PM"&&startup_clock(gameMinutes).date==L"Wed \x00b7 Week 1","Game day/period incorrect");
        Check(startup_clock(0).time==L"12:00"&&startup_clock(0).period==L"AM"&&startup_clock(7*1440).date==L"Mon \x00b7 Week 2","Midnight/week transition incorrect");
        Check(startup_clock(11*60+59).period==L"AM"&&startup_clock(12*60).period==L"PM"&&startup_clock(-1).date.empty(),"Noon/unknown calendar incorrect");
        Check(startup_overlay(64,255)==128&&startup_overlay(192,255)==255&&startup_overlay(64,0)==0,"Overlay blend formula incorrect");
        auto render=[&](int t,const char* name){auto p=original;render_phone_startup(p,t+startupBlackMs,gameMinutes);Save(std::filesystem::path(argv[1])/name,p,360,740);return p;};
        auto black=render(0,"startup-0000.bmp");for(size_t i=0;i<black.size();i+=4)Check(!black[i]&&!black[i+1]&&!black[i+2]&&black[i+3]==255,"Fade does not start black");
        for(int elapsed:{0,500,999}){auto delayed=original;render_phone_startup(delayed,elapsed,gameMinutes);Check(delayed==black,"Initial black screen did not last a full second");}
        Check(startup_frame(0)==startup_frame(999)&&startup_frame(1000)!=startup_frame(999),"Black hold refreshes unnecessarily or misses the fade transition");
        render(200,"startup-0200.bmp");auto hold=render(400,"startup-0400.bmp");Check(hold==render(499,"startup-0499.bmp"),"Closed unlock frame changed before 0.5 seconds");
        auto night=original;render_phone_startup(night,400+startupBlackMs,gameMinutes,true);Save(std::filesystem::path(argv[1])/"startup-night.bmp",night,360,740);
        for(int c=0;c<3;++c)Check(night[(300*360)*4+c]==hold[(300*360)*4+c]/2,"Night background was not dimmed by 50 percent");
        for(size_t i=0;i<360*740;++i)if(startup_images()[1][i*4+3]==255)for(int c=0;c<3;++c)Check(night[i*4+c]==hold[i*4+c]/2,"Night top layer was not dimmed by 50 percent");
        Check(render(400,"startup-day-restored.bmp")==hold,"Night/day change reused the wrong home cache");
        auto open=render(1200,"startup-1200.bmp");Check(open!=hold&&open==render(1399,"startup-1399.bmp"),"Unlock did not finish or its last frame disappeared");
        for(int y=0;y<740;++y)for(int x=0;x<360;++x)if(x<170||x>=194||y<45||y>=69)for(int c=0;c<4;++c)Check(hold[(y*360+x)*4+c]==open[(y*360+x)*4+c],"Unlock changed unrelated home pixels");
        render(800,"startup-0800.bmp");render(1410,"startup-1410.bmp");
        unsigned textPixels=0,occluded=0;for(size_t i=0;i<360*740;++i){const auto& bg=startup_images()[0];const auto& top=startup_images()[1];if(top[i*4+3]==255){for(int c=0;c<3;++c)Check(hold[i*4+c]==top[i*4+c],"Clock painted in front of top layer");++occluded;}else if(hold[i*4]!=bg[i*4])++textPixels;}
        Check(textPixels>2000&&occluded>2000,"Clock text/top layer absent");
        auto swipe=render(1800,"startup-1800.bmp");Check(std::equal(swipe.begin()+360*400*4,swipe.end(),original.begin()+360*400*4),"Swipe failed to reveal lower GPS");
        Check(render(2200,"startup-2200.bmp")==original,"Startup overlay remained after completion");
        for(auto minute:{0,9*60+41,23*60+59}){auto p=original;render_phone_startup(p,400+startupBlackMs,minute);Save(std::filesystem::path(argv[1])/("clock-"+std::to_string(minute)+".bmp"),p,360,740);}
        phonePower=0;std::cout<<"Startup resource, electrical edge, timing, fade, reveal and completion checks passed\n";return 0;
    }
    using namespace bettergps_overlay;Check(argc==2,"Expected preview output directory");std::filesystem::create_directories(argv[1]);
    std::vector<uint8_t> pixels;
    {std::vector<uint8_t> first,second;settingsMouseX=100;settings_render(first,2,false);settingsMouseX=600;settings_render(second,2,false);Check(first==second,"Mouse motion changed the cached menu background");}
    {std::ifstream f(std::filesystem::path(__FILE__).parent_path()/"../BetterGPS_assets/def/map_data/colour_data.sui");std::string text((std::istreambuf_iterator<char>(f)),{});Check(std::regex_search(text,std::regex("navigation_current_color:[ \\t]*0xFFF48542")),"ATS 1.61 current-route source colour missing");}

    const auto assets=std::filesystem::path(__FILE__).parent_path()/"../BetterGPS_assets";
    const auto nativeTest=std::filesystem::path(argv[1])/"native-assets";
    std::filesystem::create_directories(nativeTest);
    std::filesystem::copy(assets/"native_themes",nativeTest/"native_themes",std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
    for(bool dark:{false,true,false}){
        Check(native_map_prepare(nativeTest,dark),"Native profile preparation failed");
        auto profile=nativeTest/"native_themes"/(dark?"dark":"light");
        for(auto& entry:std::filesystem::recursive_directory_iterator(profile))if(entry.is_regular_file()){
            auto target=entry.path().filename()=="colour_data.sui"?nativeTest/"def/map_data/colour_data.sui":nativeTest/entry.path().lexically_relative(profile);
            std::ifstream a(entry.path(),std::ios::binary),b(target,std::ios::binary);
            Check(std::vector<char>((std::istreambuf_iterator<char>(a)),{})==std::vector<char>((std::istreambuf_iterator<char>(b)),{}),"Native palette or icon differs from supplied profile");
        }
    }
    auto state=std::filesystem::path(argv[1])/"native-theme.state";
    Check(native_map_save(state,true)&&native_map_dark(0,state)&&!native_map_dark(1,state)&&native_map_dark(2,state),"Native startup theme selection failed");
    Check(native_map_save(state,false)&&!native_map_dark(0,state),"Automatic native theme persistence failed");
    Check(!native_map_prepare(nativeTest/"missing",false),"Missing native theme accepted");

    SolarSettings summer;Check(std::abs(latitude_at(0,0,summer)-39)<.001,"Lambert origin latitude incorrect");
    Check(solar_elevation(13*60,0,0,summer)>40&&solar_elevation(1*60,0,0,summer)<0,"Day/night solar calculation incorrect");
    SolarSettings winter=summer;winter.day=355;winter.summerTime=0;
    Check(solar_elevation(17*60,0,0,summer)>5&&solar_elevation(17*60,0,0,winter)<5,"Seasonal environment did not move the night transition");
    Check(!solar_night(false,true,4.9,5)&&solar_night(true,true,5.1,5)&&solar_night(false,true,4.4,5)&&!solar_night(true,true,5.6,5),"Twilight hysteresis flickers");
    Check(!trip_present(0,true,0,true,{"TURN_RIGHT","old guidance","APPROACH",true}),"Removed route retained stale guidance");
    Check(trip_present(1000,true,0,true,{"","Preparing guidance","PREPARING_ROUTE",true}),"Preparing route switched to no-trip");
    Check(trip_present(1000,true,0,true,{"","Unavailable","ROUTE_REJECTED",true}),"Guidance rejection hid an existing route");
    Check(!trip_present(0,false,0,false,{}),"Startup without telemetry invented a trip");
    render_phone_skin(pixels,360,740,true);const auto normalSkin=pixels;Check(pixels.size()==360*740*4,"Normal skin resource missing");Save(std::filesystem::path(argv[1])/"phone-normal.bmp",pixels,360,740);
    render_phone_skin(pixels,360,740,false);Check(pixels!=normalSkin&&pixels.size()==normalSkin.size(),"No-trip skin did not change");Save(std::filesystem::path(argv[1])/"phone-no-trip.bmp",pixels,360,740);
    render_phone_skin(pixels,360,740,true,true);Check(pixels!=normalSkin,"Dark trip skin missing");Save(std::filesystem::path(argv[1])/"phone-dark.bmp",pixels,360,740);
    render_phone_skin(pixels,360,740,false,true);Save(std::filesystem::path(argv[1])/"phone-no-trip-dark.bmp",pixels,360,740);
    const char* referenceFiles[]={"phone-normal.bgra","phone-no-trip.bgra","phone-dark.bgra","phone-no-trip-dark.bgra"};
    for(unsigned state=0;state<4;++state){
        std::ifstream reference(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"validation/phone-module-migration/legacy-reference"/referenceFiles[state],std::ios::binary);reference.seekg(8);std::vector<uint8_t> old(360*740*4);reference.read(reinterpret_cast<char*>(old.data()),old.size());Check(reference.good(),"Migration comparison reference missing");
        render_phone_layout(pixels,360,740,state%2==0,state>=2);double error=0;unsigned large=0;
        const unsigned background[]={state>=2?0x36u:0xecu,state>=2?0x26u:0xecu,state>=2?0x1au:0xecu};
        for(size_t offset=0;offset<old.size();offset+=4){unsigned delta=0;for(unsigned c=0;c<3;++c){const int before=(old[offset+c]*old[offset+3]+background[c]*(255-old[offset+3])+127)/255;const int after=(pixels[offset+c]*pixels[offset+3]+background[c]*(255-pixels[offset+3])+127)/255;error+=std::abs(before-after);delta=std::max(delta,static_cast<unsigned>(std::abs(before-after)));}if(delta>20)++large;}
        Check(error/(360*740*3)<1.0&&large<360*740/100,"Module layout differs beyond DDS edge recompression tolerance");
        if(state%2==0){auto center=[](const std::vector<uint8_t>& image){double x=0,y=0,count=0;for(int row=650;row<740;++row)for(int col=260;col<355;++col){const auto* p=image.data()+(row*360+col)*4;if(p[2]>180&&p[2]>p[1]*2&&p[2]>p[0]*2&&p[3]>150){x+=col;y+=row;++count;}}Check(count>100,"Exit button missing");return std::pair<double,double>{x/count,y/count};};const auto before=center(old),after=center(pixels);Check(std::abs(before.first-after.first)<.2&&std::abs(before.second-after.second)<.2,"Module migration moved Exit button");}
    }
    const auto shieldDirectory=std::filesystem::absolute("validation/gps-asset-cache/phone-shields").wstring();
    auto reference=route_shields("Interstate 95 North");Check(reference.size()==1&&reference[0].identity=="I-95"&&reference[0].beside=="NORTH","Interstate reference/direction parsed incorrectly");
    Check(route_shields("95th Street").empty()&&route_shields("I Street").empty()&&route_shields("I-95 BUSINESS").empty(),"Street or special route got a misleading shield");
    Check(route_shields("US-16A WEST [provisoire]")[0].identity=="US-16A","Letter suffix lost");
    Check(route_shields("I-5 NORTH / US-95 SOUTH").size()==2,"Concurrent references lost");
    const auto& realShield=load_route_shield("I-95",shieldDirectory);Check(realShield.width==64&&realShield.height==64&&!realShield.bgra.empty(),"Actual ATS I-95 texture not decoded");
    {std::ifstream source(std::filesystem::path(shieldDirectory)/L"I-95.dds",std::ios::binary);source.seekg(148);std::vector<uint8_t> bytes(realShield.bgra.size());source.read(reinterpret_cast<char*>(bytes.data()),bytes.size());Check(bytes==realShield.bgra,"ATS texture colors/pixels modified");}
    Check(load_route_shield("I-9999",shieldDirectory).bgra.empty(),"Missing ATS texture replaced by invented badge");
    Check(route_shields("Nevada State Route 160 northbound")[0].identity=="NV-160"&&route_shields("US Highway 101 WB")[0].beside=="WEST","Named state/highway or bound direction not parsed");
    Check(route_shields("I-5/I-10").size()==2&&road_label_parts("I-10 EAST / Milliken Ave")[0].numbered&&!road_label_parts("I-10 EAST / Milliken Ave")[1].numbered,"Mixed route/street discards available badge");
    {std::ifstream manifest(std::filesystem::path(shieldDirectory)/L"manifest.json");std::string catalog((std::istreambuf_iterator<char>(manifest)),{});std::regex entry("\"identity\":\"([^\"]+)\"");unsigned count=0;for(std::sregex_iterator it(catalog.begin(),catalog.end(),entry),end;it!=end;++it){Check(!load_route_shield((*it)[1].str(),shieldDirectory).bgra.empty(),"Committed ATS shield cannot be decoded");++count;}Check(count>=390,"Shield catalog regression too small");}
    Check(load_phone_fonts(),"Embedded Google Sans resources failed to load");
    {HDC dc=CreateCompatibleDC(nullptr);HFONT font=CreateFontW(-48,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Google Sans");auto old=SelectObject(dc,font);wchar_t face[128]{};GetTextFaceW(dc,128,face);Check(std::wstring(face)==L"Google Sans","GDI silently substituted another font");SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);}
    const bettergps_navigation::Display states[]={
        {"TURN_RIGHT","turn right in 450 ft onto Navy Dr; then left","APPROACH",true},
        {"KEEP_RIGHT","Continue for 44.7 mi on CA-99 NORTH | keep right in 44.7 mi on CA-99 NORTH; then stay","CRUISE",true},
        {"MERGE_LEFT","merge left in 1.2 mi onto I-5 NORTH toward Sacramento (exit 32); then right","APPROACH",true},
        {"","Waiting for live guidance...","OFFLINE",false},
        {"TURN_LEFT","turn left in 500 ft onto Rue de la R\xC3\xA9publique and Washington Street [provisional] toward Stockton; then right","APPROACH",true}
    };
    const char* actions[]={"TURN_LEFT","TURN_RIGHT","SHARP_LEFT","SHARP_RIGHT","KEEP_LEFT","KEEP_RIGHT","CONTINUE","STAY","MERGE","MERGE_LEFT","MERGE_RIGHT","EXIT_LEFT","EXIT_RIGHT","U_TURN","U_TURN_LEFT","U_TURN_RIGHT","ROUNDABOUT","SLIGHT_LEFT","SLIGHT_RIGHT","CONTINUE_AT_FORK","FORK_STRAIGHT","USE_SELECTED_LANES"};
    for(auto action:actions)Check(maneuver_icon(action).alpha!=nullptr,"Supported engine maneuver has no Google icon");
    Check(maneuver_icon("FERRY_TRAIN").alpha==nullptr,"Unsupported maneuver invented");
    Check(maneuver_icon("TURN_RIGHT").alpha!=maneuver_icon("KEEP_RIGHT").alpha,"Keep and turn icons collapsed");
    Check(maneuver_icon("U_TURN_LEFT").alpha!=maneuver_icon("U_TURN_RIGHT").alpha,"U-turn orientation lost");
    auto right=guidance_presentation(states[0]);Check(right.distance=="450 ft"&&right.road=="Navy Dr"&&right.next=="left","Distance, road or Then changed");
    auto cruise=guidance_presentation(states[1]);Check(cruise.action=="CONTINUE"&&cruise.road=="CA-99 NORTH"&&cruise.next=="KEEP_RIGHT","Cruise shows wrong next action");
    auto merge=guidance_presentation(states[2]);Check(merge.road=="I-5 NORTH"&&merge.detail=="toward Sacramento"&&merge.distance.find("Exit 32")!=std::string::npos,"Sign details lost");
    auto roundabout=guidance_presentation({"ROUNDABOUT","at the roundabout, take exit 3 in 500 ft onto Main St; then right","APPROACH",true});Check(roundabout.action=="ROUNDABOUT_EXIT"&&roundabout.detail=="Exit 3"&&roundabout.road=="Main St","Roundabout exit ordinal lost");
    auto partial=guidance_presentation({"TURN_RIGHT","turn right in 450 ft onto Navy Dr; then left [guidage partiel]","APPROACH",true});Check(partial.next=="left"&&partial.detail.empty(),"Technical annotation leaked into phone UI");
    auto entry=guidance_presentation({"CONTINUE","Join the planned route toward I-5 NORTH [provisoire]","APPROACHING_ROUTE",true});Check(entry.action=="CONTINUE"&&entry.distance=="Head toward"&&entry.road=="I-5 NORTH"&&entry.detail.empty(),"Entry guidance retained generic join/log text");
    auto ready=guidance_presentation({"TURN_RIGHT","Join the planned route | turn right in 450 ft onto Navy Dr; then left","APPROACHING_ROUTE",true});Check(ready.action=="TURN_RIGHT"&&ready.distance=="450 ft"&&ready.road=="Navy Dr","Real itinerary instruction was replaced by join message");
    auto waiting=guidance_presentation({"TURN_RIGHT","turn right when ready","WAITING_AT_MANEUVER",true});Check(waiting.distance=="When ready"&&waiting.road.empty(),"When ready or action repeated in road line");
    auto waitingRoad=guidance_presentation({"TURN_RIGHT","turn right when ready onto Navy Dr [provisoire] toward Stockton (provisional); then left [provisoires]","WAITING_AT_MANEUVER",true});Check(waitingRoad.distance=="When ready"&&waitingRoad.road=="Navy Dr"&&waitingRoad.detail=="toward Stockton"&&waitingRoad.next=="left","Waiting instruction lost road or retained provisional label");
    Check(clean_guidance_text("CA-99 NORTH [PROVISOIRE] / I-5 (provisional) provisoires")=="CA-99 NORTH / I-5","Provisional label remains visible");
    Check(guidance_presentation({"","[ROUTE_REJECTED] Unresolved live PPD branch.","ROUTE_REJECTED",true}).message=="Guidance unavailable","Rejected route exposed internal reason");
    render_guidance_text_bgra(pixels,960,480,{"TURN_RIGHT","turn right when ready","WAITING_AT_MANEUVER",true},shieldDirectory);Save(std::filesystem::path(argv[1])/"guidance-when-ready.bmp",pixels,960,480);
    render_guidance_text_bgra(pixels,960,480,{"","[ROUTE_REJECTED] Unresolved live PPD branch.","ROUTE_REJECTED",true},shieldDirectory);Save(std::filesystem::path(argv[1])/"guidance-unavailable.bmp",pixels,960,480);
    for(const auto& state:{bettergps_navigation::Display{"","[DEBUG] native uid=0123 loading corridor proofs etc","PREPARING_ROUTE",true},bettergps_navigation::Display{"","Navigation paused","PAUSED",true}}){
        auto presentation=guidance_presentation(state);Check(presentation.message==(state.status=="PAUSED"?"":"Calculating Route..."),"Internal startup details escaped UI message");if(state.status=="PAUSED"){Check(presentation.action.empty()&&presentation.road.empty(),"Paused fallback leaked status text");continue;}
        render_guidance_text_bgra(pixels,960,480,state,shieldDirectory);int l=960,r=-1,t=480,b=-1;for(int y=0;y<480;++y)for(int x=0;x<960;++x)if(pixels[(y*960+x)*4+3]){l=std::min(l,x);r=std::max(r,x);t=std::min(t,y);b=std::max(b,y);}Check(r>=l&&std::abs((l+r)-779)<=2&&std::abs((t+b)-284)<=2,"Status message is not centered in banner content area");Save(std::filesystem::path(argv[1])/(state.status=="PAUSED"?"guidance-paused.bmp":"guidance-calculating.bmp"),pixels,960,480);
    }
    render_guidance_text_bgra(pixels,960,480,{"CONTINUE","Join the planned route toward I-5 NORTH [provisoire]","APPROACHING_ROUTE",true},shieldDirectory);Save(std::filesystem::path(argv[1])/"guidance-route-entry.bmp",pixels,960,480);
    auto execute=guidance_presentation({"TURN_RIGHT","turn right now onto Navy Dr","EXECUTE",true});Check(execute.distance=="Now"&&execute.road=="Navy Dr","Immediate instruction lost");
    for(unsigned i=0;i<5;++i){render_guidance_text_bgra(pixels,960,480,states[i],shieldDirectory);unsigned ink=0,transparent=0;for(size_t j=0;j<pixels.size();j+=4){if(pixels[j+3]){++ink;const int x=static_cast<int>(j/4%960),y=static_cast<int>(j/4/960);const bool badge=(i==1||i==2)&&x>=200&&x<315&&y>=125&&y<295;Check(badge||(pixels[j]==255&&pixels[j+1]==255&&pixels[j+2]==255),"Text/arrow foreground is not white");}else ++transparent;}Check(ink>20&&transparent>960*480/2,"White rectangle remains or empty render");Save(std::filesystem::path(argv[1])/("guidance-"+std::to_string(i)+".bmp"),pixels,960,480);}
    render_guidance_text_bgra(pixels,960,480,{"TURN_RIGHT","turn right in 0.1 mi onto I-95 NORTH; then left","APPROACH",true},shieldDirectory);Save(std::filesystem::path(argv[1])/"guidance-interstate.bmp",pixels,960,480);
    render_guidance_text_bgra(pixels,960,480,{"TURN_RIGHT","turn right in 0.1 mi onto US-95 SOUTH; then left","APPROACH",true},shieldDirectory);Save(std::filesystem::path(argv[1])/"guidance-us-route.bmp",pixels,960,480);
    const bettergps_navigation::Display screenCases[]={
        {"TURN_LEFT","turn left in 320 ft toward I-10 EAST [provisoire] toward I-10 EAST; then left","APPROACH",true},
        {"CONTINUE","Continue toward I-10 EAST | continue in 0.0 mi toward I-10 EAST","CRUISE",true},
        {"KEEP_LEFT","keep left in 1.7 mi toward I-5 NORTH / I-10 EAST; then right","APPROACH",true},
        {"KEEP_RIGHT","Continue toward I-5 NORTH | keep right in 2.0 mi toward I-5 NORTH","CRUISE",true},
        {"KEEP_RIGHT","keep right in 2.0 mi toward I-5 NORTH / I-10 EAST; then left","APPROACH",true},
        {"CONTINUE","Continue for 6.8 mi on I-10 EAST / Milliken Ave | keep right in 6.8 mi; then right","CRUISE",true},
        {"TURN_RIGHT","turn right in 450 ft onto Boulevard de la R\xC3\xA9publique et des Anciens Combattants de la Californie du Sud; then left","APPROACH",true}
    };
    for(unsigned index=0;index<7;++index){const auto parsed=guidance_presentation(screenCases[index]);Check(parsed.road.find("turn ")==std::string::npos&&parsed.road.find("keep ")==std::string::npos&&parsed.distance.find("I-")==std::string::npos,"Action repeated or route crammed in distance line");if(index<6)Check(!parsed.road.empty()&&road_label_parts(parsed.road)[0].numbered,"Screenshot road was not promoted to badge-capable line");render_guidance_text_bgra(pixels,960,480,screenCases[index],shieldDirectory);Save(std::filesystem::path(argv[1])/("guidance-screen-"+std::to_string(index)+".bmp"),pixels,960,480);
        unsigned primaryInk=0;for(unsigned y=125;y<300;++y)for(unsigned x=200;x<780;++x)if(pixels[(y*960+x)*4+3])++primaryInk;Check(primaryInk>100,"Primary road disappeared from banner");
    }
    auto typedScreen=screenCases[0];typedScreen.targetRoad="I-10 EAST / Milliken Ave [provisional]";typedScreen.next="right";Check(guidance_presentation(typedScreen).road=="I-10 EAST / Milliken Ave"&&guidance_presentation(typedScreen).next=="right","Typed road/next fields ignored");
    g_haveTruckSpeed=true;g_truckSpeedMps=25;g_haveSpeedLimit=true;g_speedLimitMps=29;render_speed_text_bgra(pixels,192,352,current_speed_text());Save(std::filesystem::path(argv[1])/"speed.bmp",pixels,192,352);
    g_haveGameTime=true;g_gameTimeMinutes=11*60+24;render_clock_text_bgra(pixels,192,64,current_clock_text());Save(std::filesystem::path(argv[1])/"clock.bmp",pixels,192,64);
    g_haveNavTime=true;g_navTimeSec=7200;render_time_text_bgra(pixels,400,61,-42);const auto lightTime=pixels;
    render_guidance_text_bgra(pixels,960,480,states[0],shieldDirectory);const auto lightGuidance=pixels;
    render_distance_text_bgra(pixels,500,73,"138 mi - Tue 2:27 AM",-48);const auto lightDistance=pixels;
    set_dark_presentation(true);render_time_text_bgra(pixels,400,61,-42);Check(pixels==lightTime,"Dark mode inverted remaining trip time");
    render_guidance_text_bgra(pixels,960,480,states[0],shieldDirectory);Check(pixels==lightGuidance,"Dark mode changed navigation instructions");
    render_distance_text_bgra(pixels,500,73,"138 mi - Tue 2:27 AM",-48);for(size_t i=0;i<pixels.size();i+=4)Check(pixels[i+3]==lightDistance[i+3]&&pixels[i]+lightDistance[i]==255,"Dark ETA/miles changed alpha or failed inversion");Save(std::filesystem::path(argv[1])/"distance-dark.bmp",pixels,500,73);
    render_speed_text_bgra(pixels,192,352,current_speed_text());Save(std::filesystem::path(argv[1])/"speed-dark.bmp",pixels,192,352);set_dark_presentation(false);
    for(bool night:{false,true}){set_dark_presentation(night);render_speed_text_bgra(pixels,192,352,current_speed_text());
        auto pixel=[&](unsigned x,unsigned y){return pixels.data()+(y*192+x)*4;};
        Check(pixel(0,100)[0]==(night?0:255)&&pixel(0,100)[3]==205,"Outer speed card still has an outline");
        Check(pixel(16,230)[0]==(night?255:0),"Speed-limit border missing");
        Check(pixel(80,188)[0]==(night?0:255),"Speed limit still has a second outline");
    }set_dark_presentation(false);
    Check(MH_Initialize()==MH_OK,"MinHook initialize failed");Check(install_d3d_hooks(),"D3D hook initialization failed");
    ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;D3D_FEATURE_LEVEL level{};Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"WARP device failed");
    set_trip_presentation(true);
    OverlayBridge* bridges[]={&g_distBridge,&g_timeBridge,&g_clockBridge,&g_speedBridge,&g_guidanceBridge,&g_skinBridge,&g_mapBridge,&g_mapBackgroundBridge};
    for(unsigned b=0;b<8;++b){auto& bridge=*bridges[b];D3D11_TEXTURE2D_DESC desc{};desc.Width=bridge.width;desc.Height=bridge.height;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        pixels.assign(desc.Width*desc.Height*4,255);D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels.data();data.SysMemPitch=desc.Width*4;ID3D11Texture2D* unrelated=nullptr;
        Check(SUCCEEDED(device->CreateTexture2D(&desc,&data,&unrelated)),"Unrelated texture failed");Check(bridge.markerTexture==nullptr,"Unrelated texture hijacked by dimensions");unrelated->Release();
        for(unsigned i=0;i<4;++i){pixels[4*i]=0xA7;pixels[4*i+1]=0x5B;pixels[4*i+2]=static_cast<uint8_t>(0x31+b+1+i);pixels[4*i+3]=255;}
        ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* srv=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)),"Marker texture failed");Check(bridge.markerTexture==texture,"Marker not recognized");Check(SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&srv)),"Marker view failed");if(b==6){
            for(bool dark:{false,true}){set_dark_presentation(dark);context->PSSetShaderResources(0,1,&srv);ID3D11ShaderResourceView* native=nullptr;context->PSGetShaderResources(0,1,&native);Check(native==srv,"Native map marker was replaced by a filter");if(native)native->Release();}
            ID3D11ShaderResourceView* none=nullptr;context->PSSetShaderResources(0,1,&none);srv->Release();texture->Release();set_dark_presentation(false);continue;
        }Check(bridge.liveView!=nullptr,"Live bridge not created");
        context->PSSetShaderResources(0,1,&srv);ID3D11ShaderResourceView* actual=nullptr;context->PSGetShaderResources(0,1,&actual);Check(actual==bridge.liveView,"GPU binding not substituted");if(actual)actual->Release();ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);srv->Release();texture->Release();
        // Reproduce the streaming path missed by the first test: allocation and
        // SRV creation precede the image upload. A first empty bind must not
        // prevent later recognition, and typeless storage must also work.
        reset_marker_locked(bridge);
        desc.Format=DXGI_FORMAT_B8G8R8A8_TYPELESS;
        Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)),"Empty streamed allocation failed");
        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};viewDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;viewDesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;viewDesc.Texture2D.MipLevels=1;
        Check(SUCCEEDED(device->CreateShaderResourceView(texture,&viewDesc,&srv)),"Early streamed SRV failed");
        context->PSSetShaderResources(0,1,&srv);context->PSGetShaderResources(0,1,&actual);Check(actual==srv,"Empty texture falsely identified as a marker");if(actual)actual->Release();
        context->UpdateSubresource(texture,0,nullptr,pixels.data(),data.SysMemPitch,0);
        if(b==6){std::fill(pixels.begin(),pixels.begin()+16,255);context->UpdateSubresource(texture,0,nullptr,pixels.data(),data.SysMemPitch,0);Check(bridge.markerTexture==texture,"Map identity lost when ATS overwrote its marker before first display");}
        Sleep(260);
        context->PSSetShaderResources(0,1,&srv);context->PSGetShaderResources(0,1,&actual);Check(actual==bridge.liveView,"Uploaded streamed marker remained blank");if(actual)actual->Release();
        auto renderedDesc=desc;renderedDesc.Format=bridge.liveFormat;renderedDesc.Usage=D3D11_USAGE_STAGING;renderedDesc.BindFlags=0;renderedDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* rendered=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&renderedDesc,nullptr,&rendered)),"Rendered readback allocation failed");context->CopyResource(rendered,bridge.liveTexture);
        D3D11_MAPPED_SUBRESOURCE readback{};Check(SUCCEEDED(context->Map(rendered,0,D3D11_MAP_READ,0,&readback)),"Rendered readback failed");unsigned ink=0;
        for(UINT y=0;y<desc.Height;++y){auto* row=static_cast<const uint8_t*>(readback.pData)+y*readback.RowPitch;for(UINT x=0;x<desc.Width;++x)if(row[x*4+3]>0&&(b>=4||(row[x*4]<200||row[x*4+1]<200||row[x*4+2]<200)))++ink;}
        context->Unmap(rendered,0);rendered->Release();Check(b<5?ink==0:ink>20,"Unified phone or transparent legacy layer incorrect");
        if(b==5){for(bool trip:{false,true}){set_dark_presentation(true);set_trip_presentation(trip);context->PSSetShaderResources(0,1,&srv);std::vector<uint8_t> expected;render_configured_phone(expected);
            Check(SUCCEEDED(device->CreateTexture2D(&renderedDesc,nullptr,&rendered)),"Dark skin staging allocation failed");context->CopyResource(rendered,bridge.liveTexture);Check(SUCCEEDED(context->Map(rendered,0,D3D11_MAP_READ,0,&readback)),"Dark skin readback failed");for(UINT y=0;y<desc.Height;++y)Check(std::memcmp(static_cast<const uint8_t*>(readback.pData)+y*readback.RowPitch,expected.data()+static_cast<size_t>(y)*desc.Width*4,desc.Width*4)==0,"GPU dark skin differs from provided asset");context->Unmap(rendered,0);rendered->Release();}
            set_dark_presentation(false);set_trip_presentation(true);context->PSSetShaderResources(0,1,&srv);
        }
        if(b==0||b==1||b==3||b==4||b==5){
            set_trip_presentation(false);context->PSSetShaderResources(0,1,&srv);
            Check(SUCCEEDED(device->CreateTexture2D(&renderedDesc,nullptr,&rendered)),"No-trip readback allocation failed");context->CopyResource(rendered,bridge.liveTexture);Check(SUCCEEDED(context->Map(rendered,0,D3D11_MAP_READ,0,&readback)),"No-trip readback failed");unsigned visible=0;
            for(UINT y=0;y<desc.Height;++y){auto* row=static_cast<const uint8_t*>(readback.pData)+y*readback.RowPitch;for(UINT x=0;x<desc.Width;++x)if(row[x*4+3])++visible;}
            if(b<5)Check(visible==0,"No-trip retained ETA/distance/navigation pixels");else Check(visible>20,"No-trip hid speed or skin");
            if(b==5){std::vector<uint8_t> expected;render_configured_phone(expected);for(UINT y=0;y<desc.Height;++y)Check(std::memcmp(static_cast<const uint8_t*>(readback.pData)+y*readback.RowPitch,expected.data()+static_cast<size_t>(y)*desc.Width*4,desc.Width*4)==0,"No-trip GPU skin differs from supplied asset");}
            context->Unmap(rendered,0);rendered->Release();set_trip_presentation(true);context->PSSetShaderResources(0,1,&srv);
            Check(SUCCEEDED(device->CreateTexture2D(&renderedDesc,nullptr,&rendered)),"Restored trip readback allocation failed");context->CopyResource(rendered,bridge.liveTexture);Check(SUCCEEDED(context->Map(rendered,0,D3D11_MAP_READ,0,&readback)),"Restored trip readback failed");visible=0;
            for(UINT y=0;y<desc.Height;++y){auto* row=static_cast<const uint8_t*>(readback.pData)+y*readback.RowPitch;for(UINT x=0;x<desc.Width;++x)if(row[x*4+3])++visible;}Check(b<5?visible==0:visible>20,"Unified trip UI did not restore after recapture");context->Unmap(rendered,0);rendered->Release();
        }
        context->PSSetShaderResources(0,1,&empty);srv->Release();texture->Release();
    }
    // The actual ATS map, including city text, shields and player pixels,
    // must stay on the exact native SRV in either mode. Test RGBA and BGRA,
    // linear and sRGB views, early uploads, alpha and edge colours.
    const auto fixturePath=std::filesystem::path(__FILE__).parent_path()/"fixtures/ats-map-20261013.bgra";
    std::ifstream fixture(fixturePath,std::ios::binary);unsigned dimensions[2]{};fixture.read(reinterpret_cast<char*>(dimensions),8);Check(dimensions[0]==128&&dimensions[1]==128,"Actual ATS map fixture missing");
    std::vector<uint8_t> nativePixels(128*128*4);fixture.read(reinterpret_cast<char*>(nativePixels.data()),nativePixels.size());Check(fixture.good(),"Native map fixture truncated");
    for(auto format:{DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB}){
        D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=128;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=format;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        ID3D11Texture2D* source=nullptr;ID3D11ShaderResourceView* view=nullptr;
        Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&source))&&SUCCEEDED(device->CreateShaderResourceView(source,nullptr,&view)),"Native map texture allocation failed");
        context->UpdateSubresource(source,0,nullptr,nativePixels.data(),128*4,0);
        auto staging=desc;staging.Usage=D3D11_USAGE_STAGING;staging.BindFlags=0;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* readTexture=nullptr;
        Check(SUCCEEDED(device->CreateTexture2D(&staging,nullptr,&readTexture)),"Native readback allocation failed");
        for(bool dark:{false,true}){
            set_dark_presentation(dark);context->PSSetShaderResources(0,1,&view);ID3D11ShaderResourceView* bound=nullptr;context->PSGetShaderResources(0,1,&bound);
            Check(bound==view,"Native map SRV altered; glyphs and player can be corrupted");ID3D11Resource* resource=nullptr;bound->GetResource(&resource);context->CopyResource(readTexture,resource);resource->Release();bound->Release();
            D3D11_MAPPED_SUBRESOURCE read{};Check(SUCCEEDED(context->Map(readTexture,0,D3D11_MAP_READ,0,&read)),"Native map readback failed");
            for(unsigned y=0;y<128;++y)Check(std::memcmp((uint8_t*)read.pData+y*read.RowPitch,nativePixels.data()+y*128*4,128*4)==0,"Map pixels or alpha changed");
            context->Unmap(readTexture,0);
        }
        ID3D11ShaderResourceView* none=nullptr;context->PSSetShaderResources(0,1,&none);readTexture->Release();view->Release();source->Release();
    }
    set_dark_presentation(false);
    // Verify SDK compass conventions, actual skin coordinates and isolation
    // from every other part of the phone in all four skin states.
    for(bool trip:{false,true})for(bool night:{false,true})for(float heading:{0.f,.25f,.5f,.75f}){
        std::vector<uint8_t> base;render_phone_skin(base,360,740,trip,night);auto rotated=base;render_compass(rotated,360,740,trip,heading);
        const int cx=trip?320:324,cy=trip?407:592;double redX=0,redY=0,redCount=0;
        for(int y=0;y<740;++y)for(int x=0;x<360;++x){const size_t offset=(y*360+x)*4;const auto* p=rotated.data()+offset;
            if(x<cx-18||x>=cx+18||y<cy-18||y>=cy+18)Check(std::memcmp(p,base.data()+offset,4)==0,"Compass changed another phone element");
            else if(p[2]>90&&p[2]>p[1]*2&&p[2]>p[0]*2){redX+=x+.5-cx;redY+=y+.5-cy;++redCount;}
        }
        Check(redCount>10,"Compass red needle missing");redX/=redCount;redY/=redCount;
        const double expectedAngle=(heading+15.0/360.0)*6.2831853071795864769;
        Check(redX*std::sin(expectedAngle)-redY*std::cos(expectedAngle)>4,"Calibrated compass points in wrong direction");
        Check(std::abs(redX*std::cos(expectedAngle)+redY*std::sin(expectedAngle))<1,"Compass lacks 15-degree east calibration");
        if(heading==0){auto wrapped=base;render_compass(wrapped,360,740,trip,1);Check(wrapped==rotated,"Compass wraps incorrectly at north");}
        if(heading==.75f)Save(std::filesystem::path(argv[1])/(std::string("compass-")+(trip?"trip":"no-trip")+(night?"-dark.bmp":"-light.bmp")),rotated,360,740);
    }
    scs_value_t placement{};placement.type=SCS_VALUE_TYPE_dplacement;placement.value_dplacement.orientation.heading=.75f;
    world_position_cb(nullptr,0,&placement,nullptr);Check(g_haveCompassHeading&&g_compassHeading==.75f&&g_skinDirty,"World placement did not update compass");
    // Exercise the phone's real GPU skin upload with the heading callback.
    ID3D11Texture2D* readTexture=nullptr;ID3D11ShaderResourceView* noView=nullptr;
    D3D11_TEXTURE2D_DESC compassRead{};compassRead.Width=360;compassRead.Height=740;compassRead.MipLevels=compassRead.ArraySize=compassRead.SampleDesc.Count=1;compassRead.Format=g_skinBridge.liveFormat;compassRead.Usage=D3D11_USAGE_STAGING;compassRead.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Check(SUCCEEDED(device->CreateTexture2D(&compassRead,nullptr,&readTexture)),"Compass GPU staging allocation failed");
    for(bool trip:{false,true})for(bool night:{false,true}){
        set_trip_presentation(trip);set_dark_presentation(night);g_skinDirty=true;context->PSSetShaderResources(0,1,&g_skinBridge.markerView);context->CopyResource(readTexture,g_skinBridge.liveTexture);D3D11_MAPPED_SUBRESOURCE read{};Check(SUCCEEDED(context->Map(readTexture,0,D3D11_MAP_READ,0,&read)),"Compass GPU readback failed");
        std::vector<uint8_t> expected;render_configured_phone(expected);
        for(unsigned y=0;y<740;++y)Check(std::memcmp(static_cast<const uint8_t*>(read.pData)+y*read.RowPitch,expected.data()+y*360*4,360*4)==0,"GPU compass differs from renderer");context->Unmap(readTexture,0);
    }
    readTexture->Release();context->PSSetShaderResources(0,1,&noView);world_position_cb(nullptr,0,nullptr,nullptr);Check(!g_haveCompassHeading,"Missing telemetry left a false compass bearing");
    // Live module transforms and persistent settings: each attachment follows
    // its parent's local origin and independent horizontal/vertical scale.
    {settingsScale=2;settingsOffsetX=40;settingsOffsetY=60;settingsPreviousLeft=false;settingsClick=false;settings_pointer_at({440,260},false);Check(settingsMouseX==200&&settingsMouseY==100,"Mouse screen-to-menu mapping failed");settings_pointer_at({440,260},true);Check(settingsClick.exchange(false),"Mouse click polling missed press");settings_pointer_at({440,260},true);Check(!settingsClick.exchange(false),"Held mouse repeated click");settings_pointer_at({440,260},false);settingsScale=1;settingsOffsetX=settingsOffsetY=0;
        auto previousWindow=settingsWindow;auto warp=settingsOriginalSetCursorPos;auto clip=settingsOriginalClipCursor;static int warpCalls=0,clipCalls=0;settingsOriginalSetCursorPos=[](int,int)->BOOL{++warpCalls;return TRUE;};settingsOriginalClipCursor=[](const RECT*)->BOOL{++clipCalls;return TRUE;};settingsWindow=GetForegroundWindow();settingsOpen=true;SetCursorPos(10,10);RECT confined{0,0,1,1};ClipCursor(&confined);Check(!warpCalls&&!clipCalls,"Game can still recenter or confine menu mouse");settingsOpen=false;SetCursorPos(10,10);ClipCursor(&confined);Check(warpCalls==1&&clipCalls==1,"Normal game cursor behavior not restored");settingsOriginalSetCursorPos=warp;settingsOriginalClipCursor=clip;settingsWindow=previousWindow;
        settings_render(pixels,2);Check(pixels.size()==1920*1440*4,"High-resolution menu buffer missing");Save(std::filesystem::path(argv[1])/"settings-menu-hires.bmp",pixels,1920,1440);
        auto config=default_overlay_settings();auto original=attached_placement(config,3,{20,40,320,160});Check(original.x==20&&original.y==40&&original.w==320&&original.h==160,"Default attachment changed coordinates");config.items[3].x+=11;config.items[3].y-=7;overlay_apply_scale(config,3,2.0);auto changed=attached_placement(config,3,{20,40,320,160});Check(changed.x==51&&changed.y==46&&changed.w==640&&changed.h==320,"Guidance did not follow module scale and position");
        auto file=std::filesystem::path(argv[1])/"settings-test.cfg";overlay_load(file);overlay_theme(2);overlay_adjust(3,0,13);Check(overlay_save(),"Config atomic save failed");auto saved=overlay_snapshot();overlay_reset(-1);overlay_load(file);auto loaded=overlay_snapshot();Check(loaded.theme==2&&loaded.items[3].x==saved.items[3].x,"Settings not restored from disk");overlay_theme(1);set_dark_presentation(true);Check(!selected_dark(overlay_snapshot().theme,g_darkMode.load()),"Always-light override failed");overlay_theme(2);set_dark_presentation(false);Check(selected_dark(overlay_snapshot().theme,g_darkMode.load()),"Always-dark override failed");overlay_reset(-1);set_dark_presentation(false);Check(!selected_dark(0,false)&&selected_dark(0,true),"Automatic theme broken");
        settingsSelected=3;settings_action(870,340);Check(overlay_snapshot().items[3].x==1,"Menu plus-one click missed");settings_action(70,585);Check(overlay_snapshot().items[3].x==0,"Menu reset did not restore module");settings_action(800,450);auto scaled=overlay_snapshot();Check(std::abs(scaled.scales[3]-1.1)<.0001&&scaled.items[3].w==396&&scaled.items[3].h==187,"Scale button did not preserve proportions");Check(overlay_save(),"Scale save failed");overlay_load(file);Check(std::abs(overlay_snapshot().scales[3]-1.1)<.0001,"Scale multiplier not restored");settings_action(70,585);Check(overlay_snapshot().scales[3]==1.0,"Scale reset failed");
        {std::ofstream legacy(file);legacy<<"BetterGPSOverlay 1\n2\n";auto defaults=default_overlay_settings();for(size_t i=0;i<overlayItemCount;++i){auto p=defaults.items[i];legacy<<p.x+7<<' '<<p.y+9<<' '<<p.w*2<<' '<<p.h*2<<'\n';}legacy.close();overlay_load(file);auto migrated=overlay_snapshot();Check(migrated.theme==2&&migrated.items[3].x==7&&migrated.items[3].y==36&&migrated.scales[3]==2.0,"Old config migration lost positions or scale");Check(overlay_save(),"Migrated config save failed");std::ifstream saved(file);std::string magic;int version;saved>>magic>>version;Check(version==2,"Legacy config was not rewritten with scale format");overlay_reset(-1);}
        settings_action(765,150);Check(overlay_snapshot().theme==2,"Theme button missed");settings_render(pixels);Save(std::filesystem::path(argv[1])/"settings-menu.bmp",pixels,960,720);
        D3D11_TEXTURE2D_DESC menuDesc{};menuDesc.Width=960;menuDesc.Height=720;menuDesc.MipLevels=menuDesc.ArraySize=menuDesc.SampleDesc.Count=1;menuDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;menuDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA menuData{pixels.data(),960*4,0};ID3D11Texture2D* menuTexture=nullptr;ID3D11ShaderResourceView* menuView=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&menuDesc,&menuData,&menuTexture))&&SUCCEEDED(device->CreateShaderResourceView(menuTexture,nullptr,&menuView)),"Menu GPU allocation failed");
        menuDesc.BindFlags=D3D11_BIND_RENDER_TARGET;ID3D11Texture2D* menuTarget=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&menuDesc,nullptr,&menuTarget)),"Menu target failed");ID3D11RenderTargetView* menuRtv=nullptr;device->CreateRenderTargetView(menuTarget,nullptr,&menuRtv);float background[]={.2f,.3f,.4f,1};context->ClearRenderTargetView(menuRtv,background);D3D11_VIEWPORT savedViewport{3,4,100,120,0,1};context->RSSetViewports(1,&savedViewport);
        Check(g_settingsRenderer.render(context,menuView,menuTarget,0,0,960,720,g_origPSSetSRV),"Menu GPU render failed");D3D11_VIEWPORT restored{};UINT viewportCount=1;context->RSGetViewports(&viewportCount,&restored);Check(memcmp(&restored,&savedViewport,sizeof(restored))==0,"Menu leaked viewport");ID3D11BlendState* leaked=nullptr;float factor[4];UINT mask;context->OMGetBlendState(&leaked,factor,&mask);Check(!leaked,"Menu leaked alpha blend state");
        menuDesc.BindFlags=0;menuDesc.Usage=D3D11_USAGE_STAGING;menuDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* menuRead=nullptr;device->CreateTexture2D(&menuDesc,nullptr,&menuRead);context->CopyResource(menuRead,menuTarget);D3D11_MAPPED_SUBRESOURCE read{};Check(SUCCEEDED(context->Map(menuRead,0,D3D11_MAP_READ,0,&read)),"Menu readback failed");auto* outside=(uint8_t*)read.pData;Check(abs(outside[0]-102)<=1&&abs(outside[1]-77)<=1&&abs(outside[2]-51)<=1,"Menu overwrote background outside panel");auto* panel=(uint8_t*)read.pData+400*read.RowPitch+30*4;Check(panel[0]>29&&panel[0]<45,"Menu transparency missing");context->Unmap(menuRead,0);menuRead->Release();menuRtv->Release();menuTarget->Release();menuView->Release();menuTexture->Release();overlay_reset(-1);overlay_save();overlaySettingsPath.clear();
    }

    // Registered phone models get independent GPU markers and editable profiles.
    {
        auto source=std::filesystem::path(__FILE__).parent_path().parent_path();auto base=std::filesystem::absolute(std::filesystem::path(argv[1])/"registry-fixture");
        std::filesystem::create_directories(base/"layouts/vertical");for(auto name:{"phone.sii"})std::filesystem::copy_file(source/"layouts/vertical"/name,base/"layouts/vertical"/name,std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy(source/"layouts/vertical/surfaces",base/"layouts/vertical/surfaces",std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy(source/"layouts/horizontal",base/"layouts/horizontal",std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
        auto material=std::filesystem::path("BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew");std::filesystem::create_directories(base/material);
        for(auto name:{"phone_layout_marker.dds","phone_layout_marker.tobj"})std::filesystem::copy_file(source/material/name,base/material/name,std::filesystem::copy_options::overwrite_existing);
        {auto bytes=display_bytes(source/"config/BetterGPSDisplays.ini");std::string text(bytes.begin(),bytes.end());auto horizontal=text.find("[display.f150_infotainment]");if(horizontal!=std::string::npos)text.resize(horizontal);std::ofstream file(base/"BetterGPSDisplays.ini");file<<text;}
        {std::ofstream f(base/"BetterGPSDisplays.ini",std::ios::app);f<<"\n[display.mod_phone]\nname=Custom mod phone\nui_path=/ui/custom/mod_phone.sii\nlayout=vertical\n[display.future]\nui_path=/ui/dashboard/future.sii\nlayout=horizontal\n[display.unsafe]\nui_path=/ui/../../escape.sii\n[display.off]\nui_path=/ui/dashboard/off.sii\nenabled=false\n[display.duplicate]\nui_path=/ui/dashboard/rt_phone_gps.sii\n";}
        overlay_load(base/"BetterGPSOverlay.cfg");overlay_adjust(3,0,17);Check(overlay_save(),"Legacy global settings fixture save failed");display_read_registry(base);display_generate();display_activate_first();
        for(const auto& display:gpsDisplays)if(display.ready&&display.layout=="vertical"){auto bytes=display_bytes(base/"generated-displays"/display.ui.substr(1));std::string ui(bytes.begin(),bytes.end());Check(ui.find(".mat left=0 right="+std::to_string(360.0/gpsDisplays[display_source_index(size_t(&display-gpsDisplays.data()))].markerWidth)+" bottom=1 top=0")!=std::string::npos,"Overlay crop mirrored its vertical axis or changed its horizontal orientation");Check(ui.find("bare_map.mat xscale=stretch yscale=stretch left=0.375 right=0.625 bottom=0.25 top=0.75")!=std::string::npos,"Overlay orientation fix changed native map UVs");}
        Check(gpsDisplays[0].independent&&gpsDisplays[1].independent&&gpsDisplays[2].independent&&!gpsDisplays[3].independent&&displaySurfaceMounts.size()==2,"Independent surfaces did not stay scoped to their registered models");
        for(auto& display:gpsDisplays)if(display.independent){auto path=base/"generated-displays/material/ui/bettergps_displays"/(display.id+"_surface.mat");auto bytes=display_bytes(path);std::string material(bytes.begin(),bytes.end());Check(material.find("eut2.dif.lum.a.rfx")!=std::string::npos&&material.find("_surface.tobj")!=std::string::npos,"Independent material not generated");}
        Check(gpsDisplays.size()==9&&gpsDisplays[0].ready&&gpsDisplays[1].ready&&gpsDisplays[2].ready&&gpsDisplays[3].ready,"Configured custom phone was not enabled");
        Check(!gpsDisplays[4].ready&&!gpsDisplays[5].ready&&!gpsDisplays[6].ready&&!gpsDisplays[7].ready,"Invalid/disabled/future binding applied");
        Check(!std::filesystem::exists(base/"generated-displays/ui/dashboard/future.sii")&&!std::filesystem::exists(base/"generated-displays/ui/dashboard/off.sii"),"Unavailable layouts replaced native UI");
        Check(!std::filesystem::exists(base/"generated-displays/automat"),"Static material overrides disconnected the native UI producer");
        Check(display_snapshot(0).items[3].x==17&&display_snapshot(1).items[3].x==17,"Existing positions not inherited by both phones");
        for(auto& d:gpsDisplays)if(d.ready&&d.layout=="vertical")g_displayBridges.push_back(std::make_unique<OverlayBridge>(OverlayBridge{d.id.c_str(),d.markerWidth,740}));
        std::vector<ID3D11Texture2D*> markerTextures;std::vector<ID3D11ShaderResourceView*> markerViews;
        for(size_t i=0;i<g_displayBridges.size();++i){auto& d=gpsDisplays[i];auto bytes=display_bytes(base/"generated-displays/material/ui/bettergps_displays"/(d.id+".dds"));
            D3D11_TEXTURE2D_DESC desc{};desc.Width=d.markerWidth;desc.Height=740;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA data{};data.pSysMem=bytes.data()+128;data.SysMemPitch=d.markerWidth*4;ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;
            Check(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&view)),"Registered marker allocation failed");
            Check(g_displayBridges[i]->markerView==view&&g_displayBridges[i]->liveView,"Registered UI was not captured independently");markerTextures.push_back(texture);markerViews.push_back(view);
        }
        auto readDisplay=[&](size_t i){context->PSSetShaderResources(0,1,&markerViews[i]);auto* texture=g_displayBridges[i]->liveTexture;D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* staging=nullptr;Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)),"Registered staging allocation failed");context->CopyResource(staging,texture);D3D11_MAPPED_SUBRESOURCE read{};Check(SUCCEEDED(context->Map(staging,0,D3D11_MAP_READ,0,&read)),"Registered readback failed");std::vector<uint8_t> result(size_t(desc.Width)*740*4);for(unsigned y=0;y<740;++y)memcpy(result.data()+size_t(y)*desc.Width*4,(uint8_t*)read.pData+y*read.RowPitch,desc.Width*4);context->Unmap(staging,0);staging->Release();return result;};
        auto first=readDisplay(0),second=readDisplay(1);overlay_adjust(3,0,40);Check(readDisplay(0)!=first,"Live edit did not change selected display pixels");Check(readDisplay(1)==second,"Editing one phone changed the other phone");
        Check(overlay_save(),"Per-display profile not saved");settings_action(870,215);Check(selectedDisplay==1&&overlay_snapshot().items[3].x==17,"Display selector did not restore other profile");overlay_adjust(3,2,10);overlay_theme(1);Check(overlay_save(),"Second display profile save failed");settings_action(410,215);Check(selectedDisplay==0&&overlay_snapshot().items[3].x==57,"Display selector lost first profile");
        settings_render(pixels);Save(std::filesystem::path(argv[1])/"settings-displays.bmp",pixels,960,720);

        start_cpu_worker(false);
        settingsOpen=true;auto menuVersion=[&](){std::lock_guard<std::mutex> lock(g_preparedMenu.mutex);return g_preparedMenu.version;};auto menuDeadline=GetTickCount64()+2000;while(!menuVersion()&&GetTickCount64()<menuDeadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));Check(menuVersion()>0,"Menu background did not arrive");auto stableMenu=menuVersion();settingsMouseX=333;settingsMouseY=222;std::this_thread::sleep_for(std::chrono::milliseconds(80));Check(menuVersion()==stableMenu,"Cursor motion triggered a full menu redraw/upload");settingsOpen=false;
        auto version=[&](){std::lock_guard<std::mutex> lock(g_prepared[0].mutex);return g_prepared[0].version;};
        readDisplay(0);auto deadline=GetTickCount64()+2000;while(!version()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));Check(version()>0,"Background image did not arrive");auto asyncFirst=readDisplay(0);auto stableVersion=version();std::this_thread::sleep_for(std::chrono::milliseconds(80));readDisplay(0);Check(version()==stableVersion,"Unchanged phone was recomposed in background");
        overlay_adjust(3,0,12);deadline=GetTickCount64()+2000;while(version()==stableVersion&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));Check(version()>stableVersion&&readDisplay(0)!=asyncFirst,"Asynchronous edit never reached GPU");
        auto& prepared=g_prepared[0];{std::lock_guard<std::mutex> lock(prepared.mutex);Check(prepared.uploaded==prepared.version,"Prepared image upload revision mismatch");}
        auto originalClock=g_haveGameTime.load();g_haveGameTime=!originalClock;g_cpuWake.notify_one();std::this_thread::sleep_for(std::chrono::milliseconds(40));g_haveGameTime=originalClock;
        stop_cpu_worker();Check(!g_cpuRunning,"Background renderer survived shutdown");
        std::cout<<"PASS: asynchronous image preparation/upload, no idle redraw, live edit and worker shutdown."<<std::endl;
        context->PSSetShaderResources(0,1,&noView);for(auto* v:markerViews)v->Release();for(auto* t:markerTextures)t->Release();for(auto& b:g_displayBridges){reset_marker_locked(*b);release_live_locked(*b);}g_displayBridges.clear();
        overlay_load(base/"BetterGPSOverlay.cfg");Check(overlay_snapshot().theme==1&&overlay_snapshot().items[3].x==17,"Global theme persistence changed original coordinates");display_read_registry(base);Check(gpsDisplays[0].settings.items[3].x==57&&std::abs(gpsDisplays[1].settings.scales[3]-1.1)<.0001,"Per-display adjustments did not survive restart");
        gpsDisplays[0].enabled=false;display_generate();Check(!std::filesystem::exists(base/"generated-displays/ui/dashboard/rt_phone_gps.sii"),"Disabled display left stale generated override");
        overlaySettingsPath.clear();gpsDisplays.clear();displayErrors.clear();
        std::cout<<"PASS: registry, custom UI paths, disabled/future native fallback, separate GPU views, live per-display edits, persistence and global-theme migration."<<std::endl;
    }
    uninstall_d3d_hooks();release_phone_fonts();MH_Uninitialize();context->Release();device->Release();
    std::cout<<"PASS: Predator native light/dark assets; source blue route; unfiltered map SRV and byte-exact glyph/player/alpha preservation in four GPU formats; phone overlays, settings, compass and no-trip preserved."<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
