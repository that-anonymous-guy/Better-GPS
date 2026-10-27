#pragma once
namespace bettergps_overlay {
// Only the original Mack GPS producer carries this transparent guard marker.
// It covers the brief interval before its generated BetterGPS UI is acquired.
// Disabled/Original is a user choice and must reveal the unchanged native map.
inline bool mack_boot_guard_needed(){
 if(!displayGlobalEnabled)return false;const auto vehicle=managed_vehicle();
 if(!vehicle.empty()&&vehicle!="mack.anthem"&&vehicle!="mack.pinnacle")return false;
 std::lock_guard<std::mutex> lock(managedMutex);
 for(const auto& d:managedDevices)if(d.used&&managed_is_mack(d)&&(vehicle.empty()||d.vehicle==vehicle)){
  if(!d.enabled)return false;
  if(d.known>=0&&d.known<32&&size_t(d.known)<gpsDisplays.size()&&!(displayEnabledMask.load()&(uint32_t(1)<<d.known)))return false;
 }
 return true;
}
}
