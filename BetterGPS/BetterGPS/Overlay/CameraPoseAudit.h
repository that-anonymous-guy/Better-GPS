#pragma once
// Observe raw renderer and displayed-image axes; this class never writes them.
namespace bettergps_overlay {
struct CameraPoseAudit {
 struct Target {uint64_t epoch=0,selection=0;unsigned view=0;CameraNativeProbe::Pose32 world{},cab{};bool nativeReference=false;};
 std::mutex mutex;Target target{};std::atomic<uintptr_t> unit{0};std::atomic<unsigned> nativeMask{0};std::atomic<uint64_t> sourceUpdates{0};bool enabled=true;
 uint64_t configAfter=0,last=0,lastEpoch=0,lastSelection=~uint64_t(0);unsigned samples=0;
 bool due(uint64_t epoch,uint64_t selection,uint64_t now){
  if(!enabled||!epoch)return false;if(lastEpoch==epoch&&lastSelection==selection&&now>=last&&now-last<1000)return false;
  lastEpoch=epoch;lastSelection=selection;last=now;++samples;return true;
 }
 void publish(uintptr_t next,const Target& t){{std::lock_guard<std::mutex> lock(mutex);target=t;}unit=next;}
 bool get(void* source,Target& t){if(!unit.load()||unit.load()!=uintptr_t(source))return false;std::lock_guard<std::mutex> lock(mutex);t=target;return unit.load()==uintptr_t(source);}
 void observe(void* source,void* output,uint64_t now,const CameraNativeProbe::Pose32* before=nullptr,bool corrected=false){
  if(unit.load()!=uintptr_t(source)||!unit.load()||!managed_native_readable(uintptr_t(output)+0x24,sizeof(CameraNativeProbe::Pose32)))return;
  if(now>=configAfter){configAfter=now+1000;enabled=GetPrivateProfileIntW(L"Camera",L"PoseAudit",1,(managedBase/L"BetterGPSCamera.ini").c_str())!=0;}
  Target t;{std::lock_guard<std::mutex> lock(mutex);t=target;}if(!due(t.epoch,t.selection,now))return;
  CameraNativeProbe::Pose32 actual{},actualCab{},wantedCab{};memcpy(&actual,(const void*)(uintptr_t(output)+0x24),sizeof(actual));
  if(!CameraNativeProbe::valid(actual)||!CameraNativeProbe::valid(t.world))return;
  const auto raw=actual;actual=t.nativeReference?raw:bettergps_camera::camera_image_basis(raw);
  bettergps_camera::relative_pose(actual,t.cab,actualCab);bettergps_camera::relative_pose(t.world,t.cab,wantedCab);
  const auto a=bettergps_camera::view_metrics(actual),w=bettergps_camera::view_metrics(t.world),ac=bettergps_camera::view_metrics(actualCab),wc=bettergps_camera::view_metrics(wantedCab);
  double distance=0;for(unsigned i=0;i<3;++i)distance+=(a.position[i]-w.position[i])*(a.position[i]-w.position[i]);
  static constexpr const char* names[]={"forward","left","right","reverse"};const auto* name=names[std::min(t.view,3u)];
  if(before){CameraNativeProbe::Pose32 oldCab{};bettergps_camera::relative_pose(*before,t.cab,oldCab);const auto oldRaw=bettergps_camera::view_metrics(oldCab);oldCab=bettergps_camera::camera_image_basis(oldCab);const auto oldDisplay=bettergps_camera::view_metrics(oldCab);
   log(SCS_LOG_TYPE_message,"[CameraAudit] epoch=%llu view=%s SCS_BEFORE displayYaw=%.2f down=%.2f displayRoll=%.2f rawRenderRoll=%.2f corrected=%d",(unsigned long long)t.epoch,name,oldDisplay.yaw,oldDisplay.down,oldDisplay.roll,oldRaw.roll,int(corrected));
  }
  log(SCS_LOG_TYPE_message,"[CameraAudit] epoch=%llu view=%s %s cab=(%.4f,%.4f,%.4f) yaw=%.2f down=%.2f roll=%.2f dirWorld=(%.5f,%.5f,%.5f) rawRenderQ=(%.6f,%.6f,%.6f,%.6f)",(unsigned long long)t.epoch,name,t.nativeReference?"NATIVE_REFERENCE_RAW":"ACTUAL_DISPLAY",ac.position[0],ac.position[1],ac.position[2],ac.yaw,ac.down,ac.roll,a.forward[0],a.forward[1],a.forward[2],raw.rotation[0],raw.rotation[1],raw.rotation[2],raw.rotation[3]);
  log(SCS_LOG_TYPE_message,"[CameraAudit] epoch=%llu view=%s WANTED cab=(%.4f,%.4f,%.4f) yaw=%.2f down=%.2f roll=%.2f dirWorld=(%.5f,%.5f,%.5f) errorPositionM=%.5f errorDirectionDeg=%.3f path=%s reverseGear=%d nativeMask=0x%x sourceUpdates=%llu exportPoseWrites=0 requestedHFOV=100",(unsigned long long)t.epoch,name,wc.position[0],wc.position[1],wc.position[2],wc.yaw,wc.down,wc.roll,w.forward[0],w.forward[1],w.forward[2],std::sqrt(distance),bettergps_camera::direction_error(a,w),t.nativeReference?"SCS_VALIDATED_REAR_UNMODIFIED_U0_V1":"SCS_SOURCE_BEFORE_VISIBILITY_U1_V1",int(bettergps_camera::reverseGear.load()),nativeMask.load(),(unsigned long long)sourceUpdates.load());
 }
};
inline CameraPoseAudit cameraPoseAudit;
}
