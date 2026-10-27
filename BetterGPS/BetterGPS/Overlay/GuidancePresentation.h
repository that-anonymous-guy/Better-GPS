#pragma once
#include "GoogleManeuverIcons.h"
#include <regex>

namespace bettergps_overlay {
struct GuidancePresentation { std::string action, distance, road, detail, next, message; };
inline std::string clean_guidance_text(const std::string& text) {
    static const std::regex annotations(R"(\s*[\[(]\s*(?:provisoires?|provisionals?|confirmee|hypothese|guidage partiel)\s*[\])])",std::regex::icase);
    static const std::regex provisional(R"(\b(?:provisoires?|provisionals?)\b)",std::regex::icase);
    static const std::regex spaces(R"(\s+)");
    auto clean=std::regex_replace(std::regex_replace(std::regex_replace(text,annotations,""),provisional,""),spaces," ");
    if(!clean.empty()&&clean.front()==' ')clean.erase(0,1);
    if(!clean.empty()&&clean.back()==' ')clean.pop_back();
    return clean;
}
// Split the engine's existing human-readable announcement for presentation only.
// Retain resolved road names; hide internal confidence labels on the phone.
inline GuidancePresentation guidance_presentation(const bettergps_navigation::Display& state) {
    GuidancePresentation p{state.instruction,"",state.text,"",""};
    if(state.instruction.empty()||!state.live||state.status=="PAUSED") {
        p.action.clear();p.road.clear();
        if(state.status=="PAUSED"||state.status=="REVERSING")return p;
        else if(state.status=="ARRIVED"||state.status=="COMPLETE")p.message="You have arrived";
        else if(state.status=="ROUTE_REJECTED"||state.status=="ROUTE_UNAVAILABLE"||state.status=="UNCERTAIN_ACTION")p.message="Guidance unavailable";
        else if(state.status=="PREPARING_ROUTE"||state.status=="ROUTE_PREPARING"||state.status=="RECALCULATING")p.message="Calculating Route...";
        else if(state.status=="NO_ROUTE"||state.status=="WAITING_ROUTE"||state.status=="WAITING_FOR_ROUTE")p.message="Set a route in the game";
        else if(state.status=="WAITING_POSITION"||state.status=="NO_TELEMETRY"||state.status=="STALE")p.message="Waiting for vehicle position";
        else if(!state.live||state.status=="OFFLINE"||state.status=="DISCONNECTED"||state.status=="STOPPED")p.message="Waiting for navigation";
        else p.message="Guidance unavailable";
        return p;
    }
    std::string text=clean_guidance_text(state.text);
    if(state.status=="SERVICE_AREA"){
        const auto divider=text.find("\xE2\x80\x94");p.action.clear();p.road.clear();p.message=divider==std::string::npos?"Service area":text.substr(0,divider);return p;
    }
    if(state.status=="FINAL_APPROACH"){p.distance="Arriving";p.road="Destination entrance";return p;}
    if(state.status=="WAITING_AT_MANEUVER") {
        static const std::regex ready(R"(\s*\bwhen ready\b)",std::regex::icase);
        text=std::regex_replace(text,ready,"");
    }
    if(state.status=="APPROACHING_ROUTE"&&text.rfind("Join the planned route",0)==0) {
        const auto following=text.find(" | ");
        if(following!=std::string::npos)text=text.substr(following+3);
        else {
            const auto toward=text.find(" toward ");
            p.road=toward==std::string::npos?"":text.substr(toward+8);
            p.distance=p.road.empty()?"Start route":"Head toward";
            p.action="CONTINUE";return p;
        }
    }
    const auto cruise=text.find(" | ");
    if(state.status=="CRUISE" && cruise!=std::string::npos && text.rfind("Continue",0)==0) {
        p.action="CONTINUE";p.next=state.instruction;text.resize(cruise);
    } else {
        const auto then=text.find("; then ");
        if(then!=std::string::npos) { p.next=text.substr(then+7);text.resize(then); }
    }
    const auto annotation=p.next.find(" [");
    if(annotation!=std::string::npos) { p.detail=p.next.substr(annotation+1);p.next.resize(annotation); }
    size_t roadPos=text.find(" onto ");size_t connector=6;
    if(roadPos==std::string::npos) { roadPos=text.find(" on ");connector=4; }
    size_t distancePos=text.find(" in ");size_t distancePrefix=4;
    if(distancePos==std::string::npos) { distancePos=text.find(" for ");distancePrefix=5; }
    const auto toward=text.find(" toward ");const auto exit=text.find(" (exit ");
    size_t end=text.size();if(toward!=std::string::npos)end=std::min(end,toward);if(exit!=std::string::npos)end=std::min(end,exit);
    if(distancePos!=std::string::npos) {
        const auto distanceEnd=roadPos!=std::string::npos?roadPos:end;
        if(distanceEnd>distancePos+distancePrefix)p.distance=text.substr(distancePos+distancePrefix,distanceEnd-distancePos-distancePrefix);
    }
    p.road=roadPos!=std::string::npos&&roadPos<end?text.substr(roadPos+connector,end-roadPos-connector):"";
    if(end<text.size())p.detail=text.substr(end+1)+(p.detail.empty()?"":" / "+p.detail);
    if(p.distance.empty()&&state.status=="EXECUTE")p.distance="Now";
    if(p.distance.empty()&&state.status=="WAITING_AT_MANEUVER")p.distance="When ready";
    if(state.instruction=="ROUNDABOUT") {
        // ATS is right-hand traffic; never invent an exit bearing from its ordinal.
        const auto ordinal=text.find("take exit ");
        if(ordinal!=std::string::npos) {
            auto stop=text.find(" in ",ordinal);auto label=text.substr(ordinal,stop-ordinal);
            p.detail=label+(p.detail.empty()?"":" / "+p.detail);
            p.action="ROUNDABOUT_EXIT";
        }
    }
    // Use typed road fields from the existing shared-memory protocol when
    // present. Human action phrases are represented by the maneuver icon.
    const auto typed=clean_guidance_text(state.status=="CRUISE"?state.currentRoad:state.targetRoad);
    if(!typed.empty()&&state.status!="APPROACHING_ROUTE")p.road=typed;
    if(p.road.empty()&&toward!=std::string::npos){
        auto target=text.substr(toward+8);const auto duplicate=target.find(" toward ");if(duplicate!=std::string::npos)target.resize(duplicate);
        const auto e=target.find(" (exit ");if(e!=std::string::npos)target.resize(e);
        p.road=target;p.detail.clear();
    }
    if(!state.next.empty()&&state.status!="CRUISE")p.next=state.next;
    if(p.distance.empty())p.distance="Continue";
    if(maneuver_icon(p.action).alpha==nullptr&&p.road.empty())p.road=text;
    // Repeated road/direction destinations add no information. Keep distinct
    // destinations and validated exit numbers, without repeated "toward".
    auto comparable=[](std::string s){for(auto& c:s)c=static_cast<char>(tolower(static_cast<unsigned char>(c)));return s;};
    if(p.detail.rfind("toward ",0)==0){auto target=p.detail.substr(7);const auto e=target.find(" (exit ");const auto exitText=e==std::string::npos?"":target.substr(e);if(e!=std::string::npos)target.resize(e);
        if(comparable(target)==comparable(p.road))p.detail=exitText.empty()?"":exitText.substr(1);
    }
    static const std::regex exitNumber(R"(\(exit ([^)]+)\))",std::regex::icase);
    p.detail=std::regex_replace(p.detail,exitNumber,"Exit $1");
    if(p.detail.rfind("take exit ",0)==0)p.detail.replace(0,10,"Exit ");
    if(p.road.empty()&&p.detail.rfind("Exit ",0)==0){p.road=p.detail;p.detail.clear();}
    if(state.instruction!="ROUNDABOUT"){
        static const std::regex number(R"(Exit ([A-Za-z0-9-]+))");std::smatch match;
        auto exit=state.exit;if(exit.empty()&&std::regex_search(p.detail,match,number))exit=match[1].str();
        if(!exit.empty()){p.distance+=" \xC2\xB7 Exit "+exit;p.detail=std::regex_replace(p.detail,number,"");p.detail=clean_guidance_text(p.detail);}
    }
    return p;
}
}
