#pragma once
#include "DisplayRegistry.h"
#include <map>
#include <sstream>
namespace bettergps_overlay {
inline std::mutex displayPolicyMutex;
inline std::map<std::string,std::map<std::string,bool>> displayPolicies;
inline std::string displayVehicle;
inline std::filesystem::path displayPolicyPath;
inline std::atomic<bool> displayGlobalEnabled{true};
struct DisplayStyle {const char* id;const char* name;};
inline constexpr std::array<DisplayStyle,2> displayStyles{{{"google_maps","Google Maps"},{"waze","Waze"}}};

inline const DisplayStyle& display_style(){return displayStyles[std::min(displayStyleIndex.load(),displayStyles.size()-1)];}


inline std::atomic<bool> displayPolicyReady{false},displayPolicyDirty{false};
inline std::atomic<uint32_t> displayEnabledMask{~uint32_t(0)},displayCompatibleMask{0},displaySeenMask{0};
inline std::atomic<uint64_t> displayPolicyRevision{0};
inline void display_style_layout(){
 if(displayBase.empty())return;
 const int theme=overlay_snapshot().theme;const size_t selected=selectedDisplay.load();
 overlay_load(displayBase/(waze_style()?"BetterGPSOverlay.waze.cfg":"BetterGPSOverlay.cfg"));
 auto inherited=overlay_snapshot();inherited.theme=theme;
 {std::lock_guard<std::mutex> lock(displayMutex);for(size_t i=0;i<gpsDisplays.size();++i){auto value=gpsDisplays[i].layout=="horizontal"?default_overlay_settings(true):inherited;value.theme=theme;auto path=display_settings_path(i);if(std::filesystem::is_regular_file(path)){overlay_load(path,gpsDisplays[i].layout=="horizontal");value=overlay_snapshot();value.theme=theme;}gpsDisplays[i].settings=value;}
 if(!gpsDisplays.empty()){std::lock_guard<std::mutex> lock(overlaySettingsMutex);auto revision=overlaySettings.revision;overlaySettings=gpsDisplays[std::min(selected,gpsDisplays.size()-1)].settings;overlaySettings.revision=revision+1;overlaySettingsPath=display_settings_path(std::min(selected,gpsDisplays.size()-1));overlaySavedRevision=0;}}
}
inline void display_style_cycle(int step){auto previous=displayStyleIndex.load();auto next=(previous+displayStyles.size()+step)%displayStyles.size();if(previous!=next){overlay_save();displayStyleIndex=next;display_style_layout();displayPolicyDirty=true;++displayPolicyRevision;}}
inline std::string display_vehicle_key(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});if(value.rfind("vehicle.",0)==0)value.erase(0,8);if(value.empty()||value.size()>128||value.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-")!=std::string::npos)return {};return value;}
inline bool display_vehicle_matches(const GpsDisplay& d,const std::string& id){if(id.empty())return false;std::istringstream fields(d.vehicles);std::string field;while(std::getline(fields,field,',')){field=display_trim(field);if(field=="@trucks"){for(auto* brand:{"kenworth.","peterbilt.","freightliner.","westernstar.","intnational.","international.","mack.","volvo."})if(id.rfind(brand,0)==0)return true;}else if(!field.empty()&&field.back()=='*'){field.pop_back();if(id.rfind(field,0)==0)return true;}else if(field==id)return true;}return false;}
inline bool (*displayDeviceEnabled)(size_t)=nullptr;
inline bool display_active(size_t i){return (!displayDeviceEnabled||displayDeviceEnabled(i))&& displayGlobalEnabled.load(std::memory_order_relaxed)&&i<32&&(!displayPolicyReady.load(std::memory_order_relaxed)||(displayEnabledMask.load(std::memory_order_relaxed)&(uint32_t(1)<<i)));}
inline void display_policy_rebuild_locked(){uint32_t compatible=0,enabled=0;const auto seen=displaySeenMask.load();for(size_t i=0;i<gpsDisplays.size()&&i<32;++i){const auto& d=gpsDisplays[i];const auto bit=uint32_t(1)<<i;if(!d.ready||!d.enabled||(!display_vehicle_matches(d,displayVehicle)&&!(seen&bit)))continue;compatible|=bit;auto vehicle=displayPolicies.find(displayVehicle);bool on=true;if(vehicle!=displayPolicies.end()){auto pref=vehicle->second.find(d.id);if(pref!=vehicle->second.end())on=pref->second;}if(on)enabled|=bit;}displayCompatibleMask=compatible;displayEnabledMask=enabled;}
inline void display_vehicle_changed(const std::string& id){auto key=display_vehicle_key(id);std::lock_guard<std::mutex> lock(displayPolicyMutex);displayVehicle=key;displaySeenMask=0;displayPolicyReady=true;display_policy_rebuild_locked();++displayPolicyRevision;}
inline void display_policy_init(const std::filesystem::path& base){std::lock_guard<std::mutex> lock(displayPolicyMutex);displayPolicyPath=base/"BetterGPSDisplayAssignments.ini";displayPolicies.clear();displayGlobalEnabled=true;displayStyleIndex=0;bool global=false,hasStyle=false;std::ifstream file(displayPolicyPath);std::string line,vehicle;while(std::getline(file,line)){line=display_trim(line);if(line.empty()||line[0]=='#'||line[0]==';')continue;if(line.front()=='['){vehicle.clear();global=line=="[global]";if(line.rfind("[vehicle.",0)==0&&line.back()==']')vehicle=display_vehicle_key(line.substr(9,line.size()-10));continue;}auto split=line.find('=');if(global&&split!=std::string::npos&&display_trim(line.substr(0,split))=="enabled"){auto value=display_trim(line.substr(split+1));if(value=="0"||value=="1")displayGlobalEnabled=value=="1";}if(global&&split!=std::string::npos&&display_trim(line.substr(0,split))=="style"){hasStyle=true;auto value=display_trim(line.substr(split+1));for(size_t i=0;i<displayStyles.size();++i)if(value==displayStyles[i].id)displayStyleIndex=i;}if(!vehicle.empty()&&split!=std::string::npos){auto id=display_trim(line.substr(0,split)),value=display_trim(line.substr(split+1));if(display_id(id)&&(value=="0"||value=="1"))displayPolicies[vehicle][id]=value=="1";}}if(waze_style())display_style_layout();displayPolicyDirty=!hasStyle;if(displayPolicyReady)display_policy_rebuild_locked();++displayPolicyRevision;}
inline bool display_policy_save(){if(!displayPolicyDirty.exchange(false))return true;std::map<std::string,std::map<std::string,bool>> values;std::filesystem::path path;bool global=true;{std::lock_guard<std::mutex> lock(displayPolicyMutex);values=displayPolicies;path=displayPolicyPath;global=displayGlobalEnabled.load();}if(path.empty())return true;auto temporary=path;temporary+=L".tmp";std::ofstream file(temporary,std::ios::binary);file<<"# Per-vehicle BetterGPS displays: 1=BetterGPS, 0=native display\n[global]\nenabled="<<(global?1:0)<<"\nstyle="<<display_style().id<<"\n";for(auto& vehicle:values){file<<"[vehicle."<<vehicle.first<<"]\n";for(auto& pref:vehicle.second)file<<pref.first<<"="<<(pref.second?1:0)<<"\n";}file.close();if(file&&MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;displayPolicyDirty=true;return false;}
inline void display_global_enabled(bool on){std::lock_guard<std::mutex> lock(displayPolicyMutex);if(displayGlobalEnabled.exchange(on)==on)return;displayPolicyDirty=true;++displayPolicyRevision;}
inline void display_set_enabled(size_t i,bool enabled){if(i>=gpsDisplays.size()||i>=32)return;std::lock_guard<std::mutex> lock(displayPolicyMutex);if(displayVehicle.empty())return;displayPolicies[displayVehicle][gpsDisplays[i].id]=enabled;display_policy_rebuild_locked();displayPolicyDirty=true;++displayPolicyRevision;}
inline void display_note_seen(size_t i){if(i>=32||i>=gpsDisplays.size()||!displayPolicyReady)return;const auto bit=uint32_t(1)<<i;if(displaySeenMask.fetch_or(bit)&bit)return;std::lock_guard<std::mutex> lock(displayPolicyMutex);display_policy_rebuild_locked();++displayPolicyRevision;}
inline std::vector<size_t> display_current(bool activeOnly){std::vector<size_t> result;if(activeOnly&&!displayGlobalEnabled)return result;const bool ready=displayPolicyReady.load();uint32_t mask=activeOnly?displayEnabledMask.load():displayCompatibleMask.load();if(activeOnly&&ready)mask&=displaySeenMask.load();for(size_t i=0;i<gpsDisplays.size()&&i<32;++i)if(gpsDisplays[i].ready&&(!ready||(mask&(uint32_t(1)<<i))))result.push_back(i);return result;}
inline void display_pick_current(){auto current=display_current(true);if(!current.empty()&&std::find(current.begin(),current.end(),selectedDisplay.load())==current.end())display_select(current.front());}
inline void display_cycle_current(int step){auto current=display_current(true);if(current.empty())return;auto found=std::find(current.begin(),current.end(),selectedDisplay.load());size_t i=found==current.end()?0:size_t(found-current.begin());display_select(current[(i+current.size()+step)%current.size()]);}
inline bool display_source_active(size_t source){if(!displayGlobalEnabled)return false;if(!displayPolicyReady)return true;auto mask=displayEnabledMask.load(std::memory_order_relaxed);for(size_t i=0;i<gpsDisplays.size();++i)if(display_source_index(i)==source&&display_active(i)&&(mask&(uint32_t(1)<<i)))return true;return false;}
}
