#pragma once
namespace bettergps_overlay {
inline constexpr unsigned zonarWidth=1240,zonarHeight=800,zonar5700Height=724;
inline constexpr int zonarHoldMs=4000,zonarBootMs=4500;
inline bool zonar_layout(int kind){return kind==10||kind==11;}
inline std::array<GarminModule,15> zonar_modules(bool wide=false){if(wide)return {{{"home_duty_status",274,476,144,44},
 {"Zonar_home",0,0,1240,724},{"home_drive_remaining",148,318,265,90},
 {"maneuver_distance",230,534,174,86},{"reserved",0,0,1,1},
 {"road_instructions_shields",420,534,575,84},{"Zonar_nav",0,0,1240,724},
 {"Zonar_notrip",0,0,1240,724},{"home_week_AMPM",950,0,278,30},
 {"remaining_distance",8,581,195,48},{"arrival_time",1035,581,195,48},
 {"real_speed",1137,437,78,43},{"bottom_controls",0,640,1240,84},
 {"home_profile_name",832,91,208,60},{"speed_limit",1137,329,78,104}}};
 return {{{"home_duty_status",225,525,168,52},
 {"Zonar_home",0,0,1240,800},{"home_drive_remaining",101,355,286,105},
 {"maneuver_distance",230,610,174,86},{"reserved",0,0,1,1},
 {"road_instructions_shields",420,610,575,84},{"Zonar_nav",0,0,1240,800},
 {"Zonar_notrip",0,0,1240,800},{"home_week_AMPM",976,0,252,36},
 {"remaining_distance",8,657,195,48},{"arrival_time",1035,657,195,48},
 {"real_speed",1137,513,78,43},{"bottom_controls",0,706,1240,94},
 {"home_profile_name",855,102,225,60},{"speed_limit",1137,405,78,104}}};}
}
