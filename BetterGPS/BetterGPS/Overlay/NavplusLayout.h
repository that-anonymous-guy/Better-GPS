#pragma once
// NavPlus uses the same fifteen independently editable slots as the Nuvi.
// Home artwork is intentionally one piece. W900 is 900x600; T680 uses the
// supplied 1275x775 ratio in 900x547 (one subpixel of rounding, no square buffer).
namespace bettergps_overlay {
inline constexpr int navplusPrimaryFont=62;
inline std::pair<unsigned,unsigned> navplus_canvas(int format,bool smartnav=false){return {900u,format==3?(smartnav?528u:547u):600u};}
inline std::array<GarminModule,15> navplus_modules(int format=2,bool smartnav=false,bool large579=false){auto modules=std::array<GarminModule,15>{{{"nav_above_light",0,0,900,118},
 {"home_artwork",0,0,900,600},{"nav_above_dark",0,0,900,118},
 {"nav_instructions",0,0,250,118},{"nav_zoom",790,82,100,195},
 {"nav_road_name",269,0,620,73},{"nav_footer_light",0,423,900,112},
 {"nav_footer_dark",0,423,900,112},{"home_clock_temperature",640,15,235,48},
 {"nav_footer_values",0,423,900,112},{"home_unused",0,0,1,1},
 {"nav_compass",315,540,58,43},{"bottom_frame",0,0,900,600},
 {"home_compass",315,540,58,43},{"speed_limit",800,396,78,100}}};
 if(format==3){
  modules[0].h=modules[2].h=139;modules[1].h=modules[12].h=547;
  modules[3]={"nav_instructions",0,0,294,139};modules[4]={"nav_zoom",796,77,94,184};
  modules[5]={"nav_road_name",313,0,578,69};
  for(int i:{6,7,9}){modules[i].y=384;modules[i].h=105;}
  modules[8]={"home_clock_temperature",657,13,221,45};
  for(int i:{11,13})modules[i]={i==11?"nav_compass":"home_compass",317,490,55,40};
  modules[14]={"speed_limit",806,359,73,94};
  if(smartnav){
   modules[1].h=modules[12].h=528;
   for(int i:{6,7,9})modules[i].y=366;
   modules[14].y-=18;
   for(int i:{11,13}){modules[i].x=318;modules[i].y=472;}
  }
 }
 if(large579&&format==2){modules[3].w=325;modules[3].h=153;modules[0].h=modules[2].h=153;modules[5]={"nav_road_name",346,0,543,73};modules[4].y=164;}
 return modules;
}
struct NavplusTarget {float left,top,right,bottom;};
// In source artwork coordinates, relative to the independently editable module.
// Notrip's Return starts at x=60 and is 249px wide; Nav's starts at zero/136px.
inline NavplusTarget navplus_target(bool home,bool nav,int format,bool smartnav=false){
 if(home)return format==3?(smartnav?NavplusTarget{647.f/1270,99.f/745,873.f/1270,351.f/745}:NavplusTarget{648.f/1275,103.f/775,883.f/1275,367.f/775}):NavplusTarget{.51f,.13f,.72f,.48f};
 const float scale=format==3?16.f/17:1.f;
 return {scale*(nav?0.f:60.f/1200),17.f/150,scale*(nav?136.f/1200:309.f/1200),136.f/150};
}
}
