#pragma once
namespace phone_style_cleanup_fixture {
using namespace bettergps_overlay;
inline void run(const std::filesystem::path& out){
 std::filesystem::create_directories(out);const auto base=out/"fixture";
 const auto icons=base/"BetterGPS_assets/material/ui/phone_lockscreen/icons";
 std::filesystem::create_directories(icons);
 const auto source=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets");
 for(const auto* mix:{"normalmix","overlaymix"}){
  const auto name=std::string("iphone_lock_")+mix+".dds";
  std::filesystem::copy_file(source/"material/ui/phone_lockscreen/icons"/name,icons/name);
  // A retired asset left by an old installation must not re-enable the OS.
  std::filesystem::copy_file(icons/name,icons/(std::string("android_lock_")+mix+".dds"));
 }
 {std::ofstream old(base/"BetterGPSLockscreen.ini");old<<"ClockFontStyle=android\nIconsNormal=android_lock\nIconsOverlay=android\nShuffleMode=0\n";}
 home_init(base);
 Check(homeClockFont==1&&homeSelected[0]=="iphone_lock"&&homeSelected[1]=="iphone_lock","Legacy OS removal reset the selected clock font or left stale icon selections");
 Check(homeChoices[0].size()==1&&homeChoices[1].size()==1,"Retired Android choices reappeared after rescan");
 Check(home_save(),"Renamed clock style could not be saved");
 const auto savedBytes=display_bytes(base/"BetterGPSLockscreen.ini");const std::string saved(savedBytes.begin(),savedBytes.end());
 Check(saved.find("ClockFontStyle=style2")!=std::string::npos&&saved.find("android")==std::string::npos,"Clock style still persists an OS setting");
 home_init(base);Check(homeClockFont==1,"Style 2 did not survive reload");
 DevicePreferences p;p.font=1;p.wallpaper={"android_lock","android","my-background","my-top"};
 Check(device_preferences_read(p,"wallpaper.0","android_lock")&&device_preferences_read(p,"wallpaper.1","android"),"Legacy per-device preferences not read");
 Check(p.font==1&&p.wallpaper==std::array<std::string,4>{"iphone_lock","iphone_lock","my-background","my-top"},"Per-device migration changed the clock, wallpaper or foreground");
 g_assetRoot=source.u8string();load_phone_fonts();ManagedDevice d;d.prefs=p;d.overlay=default_overlay_settings(false);
 std::vector<uint8_t> first,second;d.prefs.font=0;render_phone_home(first,d,d.overlay);d.prefs.font=1;render_phone_home(second,d,d.overlay);
 Check(first==second&&phone_home_app_bounds(d,false).x==194&&phone_home_app_bounds(d,true).x==277,"Clock choice changed Home textures or app hitboxes");
 std::vector<uint8_t> mask1(360*740),mask2(360*740);startup_clock_mask(mask1,16*60+27,0);startup_clock_mask(mask2,16*60+27,1);
 Check(mask1!=mask2&&std::any_of(mask1.begin(),mask1.end(),[](auto p){return p!=0;})&&std::any_of(mask2.begin(),mask2.end(),[](auto p){return p!=0;}),"Removing Android removed a clock font style");
 settingsOpen=true;settingsSearch=L"Clock font style";settingsFirst=0;
 settings_hit(450,190);Check(homeClockFont==0,"Style 1 menu control missed");settings_hit(750,190);Check(homeClockFont==1,"Style 2 menu control missed");
 std::vector<uint8_t> menu;settings_render(menu,2,false);Save(out/"clock-style-menu.bmp",menu,1920,1440);settingsOpen=false;
 Save(out/"unified-home.bmp",second,360,740);
 std::cout<<"PASS: Android textures/settings retired, legacy icon selections migrate without resetting clock or wallpaper, Style 1/2 persist and render independently, unified Home hitboxes and both menu controls.\n";
}
}
