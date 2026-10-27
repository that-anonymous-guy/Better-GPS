#pragma once
namespace bettergps_overlay {
inline constexpr unsigned internationalWidth=1473,internationalHeight=800;
inline constexpr unsigned internationalFrames=301,internationalMovieWidth=1480;
inline constexpr int internationalBootMs=(internationalFrames*1000+29)/30;
inline std::array<GarminModule,15> international_modules(){return {{{"status_bar",0,0,1473,74},
 {"T8520_home",0,0,1473,800},{"reserved",0,0,1,1},
 {"maneuver_distance",310,680,180,104},{"zoom_controls",20,90,1433,67},
 {"road_instructions_shields",530,662,655,132},{"navigation_footer",0,631,1473,169},
 {"notrip_apps",10,667,250,123},{"clock_AMPM",1180,4,265,70},
 {"remaining_distance",0,748,250,50},{"arrival_time",1242,748,225,50},
 {"real_speed",1308,538,145,80},{"reserved_bottom",0,0,1,1},
 {"home_navigation",130,160,325,225},{"speed_limit",1308,339,145,193}}};}
}
