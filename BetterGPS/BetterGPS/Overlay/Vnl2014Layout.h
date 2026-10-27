#pragma once
namespace bettergps_overlay {
inline constexpr unsigned vnlGpsWidth=900,vnlGpsHeight=457;
inline constexpr int vnlGpsBootMs=3000;
inline std::array<GarminModule,15> vnl2014_modules(){return {{{"Dynafleet_home",0,0,900,457},{"Garmin600_nav",0,0,900,457},
 {"Garmin600_notrip",0,0,900,457},{"Garmin600_dark_overlay",0,0,900,457},
 {"text_instructions",7,1,886,54},{"arrival",8,364,176,83},{"turn_distance",714,364,176,83},
 {"reserved",0,0,1,1},{"reserved",0,0,1,1},{"reserved",0,0,1,1},{"reserved",0,0,1,1},
 {"reserved",0,0,1,1},{"reserved",0,0,1,1},{"reserved",0,0,1,1},{"reserved",0,0,1,1}}};}
}
