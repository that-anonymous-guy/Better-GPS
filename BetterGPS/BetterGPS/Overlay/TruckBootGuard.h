#pragma once
namespace bettergps_overlay {
// Covers only supported integrated GPS producers, until their generated UI
// replaces the original. No native UI job or private render export is added.
inline bool truck_boot_vehicle(const std::string& v){return managed_international_vehicle(v)||managed_zonar_vehicle(v)||managed_navplus_vehicle(v)||v=="kenworth.t680_2022"||v=="mack.anthem"||v=="mack.pinnacle"||v=="volvo.vnl";}
inline int truck_boot_guard_tone(){const auto v=managed_vehicle();return managed_zonar_vehicle(v)?4:v=="volvo.vnl"?(displayIgnition==1?2:1):managed_international_vehicle(v)||v=="kenworth.t680_2022"||v=="mack.anthem"||v=="mack.pinnacle"?1:0;}
inline bool truck_boot_guard_needed(){
 if(!displayGlobalEnabled)return false;const auto v=managed_vehicle();if(!v.empty()&&!truck_boot_vehicle(v))return false;
 std::lock_guard<std::mutex> lock(managedMutex);
 for(const auto& d:managedDevices)if(d.used&&(managed_is_international(d)||managed_is_navplus(d)||managed_is_copilot(d)||managed_is_zonar(d)||managed_is_mack(d)||managed_is_vnl2014(d))&&(v.empty()||d.vehicle==v)&&!d.enabled)return false;
 return true;
}
}
