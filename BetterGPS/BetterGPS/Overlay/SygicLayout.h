#pragma once
namespace bettergps_overlay {
inline constexpr unsigned sygicWidth=900,sygicHeight=500;
inline constexpr int sygicBootMs=1350;
inline std::array<GarminModule,15> sygic_modules(){return {{{"Volvo_home_background",0,0,900,500},
 {"Volvo_bottom",0,439,900,61},{"Volvo_home_menu",435,388,449,44},
 {"Volvo_home_cases",13,13,873,368},{"Sygic_search",13,15,385,58},
 {"Sygic_zoom",829,158,58,58},{"Sygic_instructions",12,17,338,101},
 {"Sygic_footer",12,356,338,83},{"Sygic_speed",829,373,58,58},
 {"Sygic_limit",829,300,58,69},{"Volvo_clock",15,448,117,42},
 {"trip_progress",0,0,1,1},{"reserved_12",0,0,1,1},
 {"reserved_13",0,0,1,1},{"reserved_14",0,0,1,1}}};}
}
