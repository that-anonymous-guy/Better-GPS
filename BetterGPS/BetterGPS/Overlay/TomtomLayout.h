#pragma once
namespace bettergps_overlay {
inline constexpr unsigned tomtomWidth=900,tomtomHeight=544;
inline constexpr int mackIntroMs=2000;
inline constexpr int mackBootMs=mackIntroMs+1250;
inline constexpr int volvo18IntroMs=2000,volvo18BootMs=volvo18IntroMs+1250;
inline constexpr unsigned icc5Width=1596,icc5Height=600;
// Keep the initial background/overlay timing; 1s fades, 2s holds, 1s gaps.
inline constexpr int icc5RunsmartMs=1250,icc5DetroitMs=6250,icc5BootMs=11250;
inline std::array<GarminModule,15> icc5_modules(){return {{{"ICC5_home",0,0,1596,600},
 {"home_clock",1310,0,135,62},{"outside_temperature",8,550,132,48},{"Navigation_case",580,85,430,350},
 {"ICC5_persistent_overlay",0,0,1596,600},{"TomTom_left_controls",165,170,70,410},
 {"compass",165,170,70,102},{"instructions",365,8,640,79},
 {"speed_current_road",365,548,640,52},{"trip_revisisted",1123,0,325,600},
 {"ETA",1275,0,166,52},{"remaining_time",1270,52,171,50},
 {"route_column",1144,120,22,413},{"player_remaining_distance",1144,540,297,60},
 {"current_clock",1305,548,137,49}}};}
inline std::array<GarminModule,15> tomtom_modules(bool volvo=false){auto modules=std::array<GarminModule,15>{{{"home_background",0,0,900,544},
 {"home_top",0,0,900,63},{"home_menus",46,84,812,40},{"home_cases",55,148,796,373},
 {"Mack_return_home",0,0,84,544},{"TomTom_left_controls",90,180,51,349},
 {"compass",90,180,51,74},{"instructions",165,8,570,70},
 {"speed_current_road",165,486,570,52},{"trip_background",783,0,117,544},
 {"ETA",789,0,106,42},{"remaining_time",789,42,106,34},
 {"route_column",791,90,22,420},{"player_remaining_distance",786,496,110,45},
 {"home_clock",0,0,1,1}}};if(volvo){modules[0]={"Volvo_home_background",84,0,816,544};modules[1]={"Volvo_home_top",84,0,816,84};modules[2]={"Volvo_home_menus",84,84,816,112};modules[3]={"Volvo_home_cases",98,197,731,326};modules[4]={"Volvo_return_home",0,0,84,544};}return modules;}
}
