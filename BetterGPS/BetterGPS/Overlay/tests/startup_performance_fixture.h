#pragma once
#include <chrono>
namespace startup_performance_fixture {
using namespace bettergps_overlay;
inline void run(const std::filesystem::path& out){
 const auto installed=std::filesystem::path("H:/SteamLibrary/steamapps/common/American Truck Simulator/bin/win_x64/plugins/BetterGPS");
 const auto base=out/"plugin";std::filesystem::create_directories(base);
 for(const auto* directory:{"layouts","device-settings","display-settings"})if(std::filesystem::is_directory(installed/directory))std::filesystem::copy(installed/directory,base/directory,std::filesystem::copy_options::recursive);
 for(const auto& file:std::filesystem::directory_iterator(installed))if(file.is_regular_file()&&(file.path().extension()==".ini"||file.path().extension()==".cfg"))std::filesystem::copy_file(file.path(),base/file.path().filename());
 const auto markers=std::filesystem::path("BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew");std::filesystem::create_directories(base/markers);for(const auto* name:{"phone_layout_marker.dds","phone_layout_marker.tobj"})std::filesystem::copy_file(installed/markers/name,base/markers/name);
 auto measure=[&](const char* name,const auto& call){const auto start=std::chrono::steady_clock::now();call();const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();std::cout<<"STARTUP_BENCH "<<name<<" us="<<us<<"\n";};
 measure("settings",[&]{overlay_load(base/"BetterGPSOverlay.cfg");display_power_load(base/"BetterGPSDisplayPower.ini");map_views_init(base);managed_init(base);display_read_registry(base);});
 measure("display-generate",[&]{display_generate();managed_resolve_known();});
 measure("managed-templates",[&]{managed_build_templates();});
 measure("player-materials",[&]{native_player_build_materials(installed/"BetterGPS_assets");});
 measure("cluster-assets",[&]{cluster_intro_load(installed);mack_cluster_load(installed);vnl_cluster_load(installed);volvo18_cluster_load(installed);});
 display_policy_init(base);display_activate_first();managed_bridges_init();renderStopping=false;renderGameplayEnabled=true;renderGameplayActive=false;g_assetsReady=g_cpuRunning=true;
 g_log=+[](scs_log_type_t,const char* message){std::cout<<message<<'\n';};g_logOwner=GetCurrentThreadId();
 Check(MH_Initialize()==MH_OK,"Performance fixture MinHook unavailable");measure("graphics-hooks",[&]{Check(install_d3d_hooks(),"Performance fixture graphics hooks unavailable");});
 measure("graphics-stop",[&]{g_assetsReady=g_cpuRunning=false;uninstall_d3d_hooks();MH_Uninitialize();});
 uint64_t bytes=0,files=0;for(const auto& file:std::filesystem::recursive_directory_iterator(base/"generated-displays"))if(file.is_regular_file()){++files;bytes+=file.file_size();}std::cout<<"STARTUP_BENCH generated_files="<<files<<" bytes="<<bytes<<"\n";
}
}
