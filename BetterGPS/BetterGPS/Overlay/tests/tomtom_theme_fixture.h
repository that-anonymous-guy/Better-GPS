#pragma once
namespace tomtom_theme_fixture {
using namespace bettergps_overlay;
inline void run(const std::filesystem::path& out){
 const auto assets=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets");
 const auto base=read_native_palette(assets/"native_themes/light/colour_data.sui");
 for(bool dark:{false,true}){
  auto& palette=tomtomMapPalettes[dark];palette=read_native_palette(assets/"native_themes"/(dark?"tomtom_dark":"tomtom_light")/"colour_data.sui");
  Check(palette.areas.size()==base.areas.size()&&palette.discovered.size()==base.discovered.size()&&palette.colors.size()==base.colors.size(),"TomTom changed native map indices/reflection fields");
  for(const auto& field:base.colors)Check(palette.colors.count(field.first)!=0,"TomTom missing native map field");
  for(const auto* field:{"navigation_color","navigation_current_color","navigation_highlight_color","navigation_fade_color","navigation_arrow_color"})Check(palette.colors.at(field)==0xFFF9EA01,"TomTom route is not exact #01EAF9");
  for(int icon=0;icon<3;++icon){const auto style=native_style_encode(4,icon);Check(style==12+icon&&native_style_palette(style)==4&&native_style_icon(style)==icon&&&device_map_palette(style,dark)==&palette,"TomTom palette/player preference is not isolated");}
 }
 for(unsigned a=0;a<nativeMapThemeCount;++a)for(unsigned b=0;b<a;++b)Check(native_map_clear(a)!=native_map_clear(b),"Deferred TomTom theme identity collides");
 for(int palette=0;palette<4;++palette)for(int icon=0;icon<3;++icon)Check(native_style_encode(palette,icon)==(palette==3?9+icon:palette+icon*3),"TomTom changed an existing native style ID");
 Check(std::string(native_player_material(12,false))=="/material/ui/bettergps_player/tomtom.mat"&&std::string(native_player_material(13,false))=="/material/ui/bettergps_player/original.mat"&&native_player_is_truck(14),"TomTom player material choices leak into another palette");
 const auto blue=native_icon_read(assets/"material/ui/map/player.dds"),cyan=native_tomtom_cyan(blue);Check(!cyan.pixels.empty()&&cyan.pixels!=blue.pixels,"TomTom player retained default blue");
 unsigned colored=0;for(size_t p=0;p<cyan.pixels.size();p+=4){Check(cyan.pixels[p+3]==blue.pixels[p+3],"TomTom player lost original alpha");const auto* pixel=cyan.pixels.data()+p;const bool bgra=cyan.format==DXGI_FORMAT_B8G8R8A8_UNORM;const unsigned r=pixel[bgra?2:0],b=pixel[bgra?0:2];if(pixel[3]>128&&b>r+40&&pixel[1]>r+40)++colored;}Check(colored>50,"TomTom player is not visibly light cyan");
 std::filesystem::create_directories(out);Save(out/"tomtom-player-cyan.bmp",cyan.pixels,cyan.width,cyan.height);
}
}
