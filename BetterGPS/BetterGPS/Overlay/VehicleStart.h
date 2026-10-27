#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
namespace bettergps_overlay {
// A timer restart, a menu resume or ignition changes never mint this token.
// Only a successful player-actor initialization, confirmed with fresh driving
// telemetry and the same actor/truck binding, may publish a new generation.
struct VehicleStartTracker {
 std::mutex mutex;
 uintptr_t actor=0,truck=0;
 uint64_t pending=0,confirmed=0;
 std::atomic<uint64_t> epoch{0};
 std::atomic<bool> eligible{false};
 void observed(uintptr_t a,uintptr_t t){
  if(!a||!t)return;
  std::lock_guard<std::mutex> lock(mutex);actor=a;truck=t;++pending;eligible=false;
 }
 bool waiting(){std::lock_guard<std::mutex> lock(mutex);return pending!=confirmed;}
 bool confirm(uintptr_t a,uintptr_t t,bool freshDriving){
  std::lock_guard<std::mutex> lock(mutex);
  if(!freshDriving||pending==confirmed)return false;
  confirmed=pending;
  if(!a||!t||a!=actor||t!=truck){eligible=false;return false;}
  // Publish eligibility before the generation: a preparation worker that
  // acquires the new epoch must never initialize it as an ineligible boot.
  eligible=true;++epoch;return true;
 }
};
inline VehicleStartTracker vehicleStart;
}
