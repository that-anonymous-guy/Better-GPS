#pragma once
namespace bettergps_overlay {
inline constexpr unsigned copilotWidth=900,copilotHeight=544;
inline constexpr int copilotIntroFrames=98,copilotIntroFps=30;
inline constexpr int copilotIntroMs=(copilotIntroFrames*1000+copilotIntroFps-1)/copilotIntroFps;
inline constexpr int copilotBootMs=500+copilotIntroMs+250+500;
inline std::array<GarminModule,15> copilot_modules(){return {{{"status_bar",0,0,900,63},
 {"home_artwork",0,0,900,544},{"driving_menu",120,87,660,372},
 {"maneuver_distance",176,403,154,80},{"zoom_controls",16,74,868,41},
 {"road_instructions_shields",340,400,384,80},{"navigation_footer",0,380,900,103},
 {"notrip_apps",10,420,114,56},{"clock_weekday",660,8,235,42},
 {"remaining_distance",0,441,148,40},{"arrival_time",749,441,144,40},
 {"real_speed",812,352,78,43},{"bottom_controls",0,483,900,61},
 {"re-center",18,337,150,34},{"speed_limit",812,245,78,104}}};}
}
