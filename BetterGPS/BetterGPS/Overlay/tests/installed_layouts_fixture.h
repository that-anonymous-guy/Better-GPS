#pragma once
namespace installed_layouts_fixture {
using namespace bettergps_overlay;
inline void run(const std::filesystem::path& out){
 const auto installed=std::filesystem::path("H:/SteamLibrary/steamapps/common/American Truck Simulator/bin/win_x64/plugins/BetterGPS");
 const auto base=out/"plugin";std::filesystem::create_directories(base);
 std::vector<std::pair<std::filesystem::path,std::vector<uint8_t>>> originals;
 for(const auto& file:std::filesystem::recursive_directory_iterator(installed/"layouts"))if(file.is_regular_file())originals.emplace_back(file.path(),display_bytes(file.path()));
 for(const auto* directory:{"layouts","device-settings","display-settings"})if(std::filesystem::is_directory(installed/directory))std::filesystem::copy(installed/directory,base/directory,std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
 for(const auto& file:std::filesystem::directory_iterator(installed))if(file.is_regular_file()&&(file.path().extension()==".ini"||file.path().extension()==".cfg"))std::filesystem::copy_file(file.path(),base/file.path().filename(),std::filesystem::copy_options::overwrite_existing);
 const auto markers=std::filesystem::path("BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew");std::filesystem::create_directories(base/markers);for(const auto* name:{"phone_layout_marker.dds","phone_layout_marker.tobj"})std::filesystem::copy_file(installed/markers/name,base/markers/name,std::filesystem::copy_options::overwrite_existing);
 overlay_load(base/"BetterGPSOverlay.cfg");display_power_load(base/"BetterGPSDisplayPower.ini");map_views_init(base);managed_init(base);display_read_registry(base);display_generate();managed_resolve_known();managed_build_templates();native_player_build_materials(installed/"BetterGPS_assets");Check(std::filesystem::is_regular_file(base/"generated-displays/material/ui/bettergps_player/truck.mat")&&std::filesystem::is_regular_file(base/"generated-displays/material/ui/bettergps_player/truck.dds"),"Native player material/DDS absent from their exact VFS directory");display_policy_init(base);display_activate_first();
 unsigned ready=0,models=0;for(const auto& d:gpsDisplays)if(d.ready){++ready;Check(std::filesystem::is_regular_file(base/"generated-displays"/d.ui.substr(1)),"Installed display did not generate its native UI");}
 const std::regex material("/material/ui/[A-Za-z0-9_./-]+\\.mat");for(const auto& file:std::filesystem::recursive_directory_iterator(base/"generated-displays"))if(file.is_regular_file()&&file.path().extension()==".pmd"){
  auto bytes=display_bytes(file.path());std::string binary(bytes.begin(),bytes.end());for(std::sregex_iterator it(binary.begin(),binary.end(),material),end;it!=end;++it){Check(std::filesystem::is_regular_file(base/"generated-displays"/it->str().substr(1)),"Phone model references a material outside the generated namespace");++models;}
 }
 Check(ready>=5&&models>=3,"Installed phone/F150/Bronco layout generation incomplete");Check(managed_snapshot(11).style==2&&managed_snapshot(10).frameSkin=="Crown_Victoria_Sony.dds","Installed Garmin/Crown settings were not retained");
 Check(native_icons_load(installed/"BetterGPS_assets"),"Installed native map icon catalog invalid");for(const auto& original:originals)Check(display_bytes(original.first)==original.second,"Preflight modified an installed layout");
 settingsOpen=true;settingsSearch.clear();settingsFirst=0;settingsCategory=0;std::vector<uint8_t> menu;settings_render(menu,2,false);Save(out/"menu.bmp",menu,1920,1440);settingsOpen=false;
 std::cout<<"PASS: actual installed layouts/configurations generate all phone/F150/Bronco/device UIs, every PMD surface material resolves, Garmin/Crown settings and map icons are retained, and the settings menu renders. Installed inputs are unchanged.\n";
}
}
