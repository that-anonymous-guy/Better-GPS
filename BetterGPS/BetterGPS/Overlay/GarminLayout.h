#pragma once
// Nuvi layouts use the same editable module slots as Google/Waze. All positions
// are derived from the supplied 1200-pixel references with one uniform scale.
namespace bettergps_overlay {
inline const char* navigation_style_name(int style){return style==6?"Sygic / Volvo VNL 2025":style==5?"Volvo Dynafleet / Garmin 600":style==4?"TomTom Trucker 6000":style==3?"CoPilot V9":style==2?"Garmin Nuvi 465T":style==1?"Waze":"Google Maps";}
inline bool garmin_style(){return (displayStyleOverride>=0?size_t(displayStyleOverride):displayStyleIndex.load())==2;}
struct GarminModule {const char* name;int x,y,w,h;};
inline std::array<GarminModule,15> garmin_modules(int format,bool legacy=false){
 const bool three=format==2;const int height=three?800:672;
 auto modules=std::array<GarminModule,15>{{{"nav_above_light",0,0,1200,three?240:210},
 {"home_above",0,0,1200,73},{"nav_above_dark",0,0,1200,three?240:210},
 {"nav_instructions",0,0,250,three?240:210},
 {"nav_zoom",1050,80,150,280},{"nav_road_name",283,0,907,73},
 {"nav_below_light",165,height-140,870,140},{"nav_below_dark",165,height-140,870,140},
 {"home_middle_light",78,100,1044,400},{"home_middle_dark",78,100,1044,400},
 {"home_background_light",0,0,1200,height},{"home_background_dark",0,0,1200,height},
 {"home_below_light",0,height-(three?200:170),1200,three?200:170},
 {"home_below_dark",0,height-(three?200:170),1200,three?200:170},
 {"speed_limit",8,height-260,110,130}}};
 if(!legacy){
  // Match the larger NAV reference without stretching the complete header.
  // Its bubble is 1.3x, its bar 1.2x, its footer 1.25x and sign 1.4x.
  for(int i:{0,2})modules[i].h=int(std::lround(modules[i].h*1.3));
  modules[3].w=325;modules[3].h=int(std::lround(modules[3].h*1.3));
  modules[5]={"nav_road_name",339,0,861,88};
  for(int i:{6,7}){modules[i].x=56;modules[i].y=height-175;modules[i].w=1088;modules[i].h=175;}
  modules[14]={"speed_limit",12,height-340,154,182};
 }
 return modules;
}
inline std::pair<unsigned,unsigned> garmin_canvas(int format){return {900u,format==3?547u:format==2?600u:504u};}
}
