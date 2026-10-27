#pragma once
// The physical Dynafleet is not a touchscreen. Only its ordinary page shortcut
// and the shared route/page state are used; no arrows or shield art are painted.
static const GarminImage& vnl2014_image(const std::string& name){return page_image("garmin/"+name);}
static std::string vnl2014_action(std::string action){if(action.rfind("TURN_",0)==0)action.erase(0,5);return action;}
static std::string vnl2014_instruction(const bettergps_navigation::Display& state){
 const auto p=guidance_presentation(state);if(!p.message.empty())return clean_guidance_text(p.message);
 const std::map<std::string,std::string> verbs{{"LEFT","Turn left"},{"RIGHT","Turn right"},{"KEEP_LEFT","Keep left"},{"KEEP_RIGHT","Keep right"},{"MERGE_LEFT","Merge left"},{"MERGE_RIGHT","Merge right"},{"EXIT_LEFT","Take exit"},{"EXIT_RIGHT","Take exit"},{"UTURN","Make a U-turn"},{"CONTINUE","Continue"},{"ARRIVE","Arrive"},{"ROUNDABOUT_EXIT","Take the roundabout exit"}};
 const auto action=vnl2014_action(p.action);const auto found=verbs.find(action);const auto verb=found==verbs.end()?std::string("Continue"):found->second;
 const auto road=clean_guidance_text(p.road);if(!road.empty())return verb+(action=="CONTINUE"?" on ":action=="ARRIVE"?" at ":" to ")+road;
 return verb+(p.distance.empty()||p.distance=="Continue"?"":" in "+clean_guidance_text(p.distance));
}
static std::string vnl2014_turn_caption(const GuidancePresentation& p){const auto action=vnl2014_action(p.action);return action=="KEEP_LEFT"?"Keep Left In":action=="KEEP_RIGHT"?"Keep Right In":action.rfind("MERGE",0)==0?"Merge In":action.rfind("EXIT",0)==0?"Exit In":action=="ARRIVE"?"Arrive In":action=="CONTINUE"?"Continue For":"Turn In";}
static void render_vnl2014(std::vector<uint8_t>& pixels,const OverlaySettings& settings,const bettergps_navigation::Display& state=bettergps_navigation::Current()){
 constexpr int w=vnlGpsWidth,h=vnlGpsHeight;pixels.assign(w*h*4,0);const bool trip=g_tripPresent.load(),dark=presentation_dark(),home=presentation_home(trip);
 auto paint=[&](size_t slot,const std::string& name){const auto& image=vnl2014_image(name);overlay_blit(pixels,image.pixels,image.width,image.height,settings.items[slot],w,h);};
 if(home){paint(0,"volvodynafleet/volvydynafleet_home.dds");return;}
 if(!trip){paint(2,"nuvi600/garmin600_notrip.dds");return;}
 paint(1,"nuvi600/garmin600_nav.dds");if(dark)paint(3,"nuvi600/garmin600_nav_dark_overlay.dds");
 std::vector<uint8_t> header(886*54*4,0);garmin_text(header,886,54,vnl2014_instruction(state),{0,0,886,54},39,RGB(255,255,255),FW_NORMAL,true);overlay_blit(pixels,header,886,54,settings.items[4],w,h);
 constexpr int fw=176,fh=83;const auto ink=dark?RGB(255,255,255):RGB(0,0,0);const uint8_t paper=dark?32:255;
 auto box=[&]{std::vector<uint8_t> result(fw*fh*4,paper);for(size_t i=3;i<result.size();i+=4)result[i]=255;return result;};
 auto arrival=box();garmin_text(arrival,fw,fh,"Arrival",{0,0,fw,27},25,ink,FW_NORMAL,true);
 const auto eta=garmin_arrival();garmin_text(arrival,fw,fh,eta.time,{0,26,145,82},52,ink,FW_BOLD,true);for(size_t i=0;i<eta.period.size()&&i<2;++i)garmin_text(arrival,fw,fh,eta.period.substr(i,1),{146,30+int(i)*23,175,54+int(i)*23},22,ink,FW_NORMAL,true);
 overlay_blit(pixels,arrival,fw,fh,settings.items[5],w,h);
 const auto p=guidance_presentation(state);auto maneuver=box();garmin_text(maneuver,fw,fh,vnl2014_turn_caption(p),{0,0,fw,27},23,ink,FW_NORMAL,true);
 auto distance=clean_guidance_text(p.distance);const auto suffix=distance.find(" \xC2\xB7");if(suffix!=std::string::npos)distance.resize(suffix);const auto split=distance.find_last_of(' ');const auto unit=split==std::string::npos?"":distance.substr(split+1);const bool measured=unit=="mi"||unit=="ft"||unit=="km"||unit=="m";
 if(measured){garmin_text(maneuver,fw,fh,distance.substr(0,split),{0,26,137,82},52,ink,FW_BOLD,true);garmin_text(maneuver,fw,fh,unit,{138,29,175,77},23,ink,FW_NORMAL,true);}else garmin_text(maneuver,fw,fh,distance,{0,27,fw,82},34,ink,FW_BOLD,true);
 overlay_blit(pixels,maneuver,fw,fh,settings.items[6],w,h);
}
