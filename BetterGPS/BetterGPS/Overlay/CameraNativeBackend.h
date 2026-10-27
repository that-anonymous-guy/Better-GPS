#pragma once
#include "CameraPoseAudit.h"
namespace bettergps_overlay {
inline uint32_t __fastcall camera_native_mask(void*,unsigned);
inline void* __fastcall camera_native_export(void*,void*);
inline void __fastcall camera_native_source_update(void*,float);
struct CameraNativeBackend {
 using Mask=uint32_t(__fastcall*)(void*,unsigned);using Export=void*(__fastcall*)(void*,void*);using SourceUpdate=void(__fastcall*)(void*,float);
 Mask original{};Export originalExport{};SourceUpdate originalUpdate{};
 uintptr_t address=0,exportAddress=0,updateAddress=0,base=0;bool supported=false,installed=false,exportInstalled=false,updateInstalled=false;
 std::atomic<uintptr_t> renderOwner{0},filterOwner{0};std::atomic<bool> wanted{false},nativeRear{false};
 std::atomic<uint64_t> submittedEpoch{0},submittedSelection{0};bettergps_camera::VideoFrames videoFrames;
 uintptr_t savedOwner=0,savedUnit=0;uint64_t savedEpoch=0,lastSelection=~uint64_t(0);float savedFov=100;
 static bool applySource(void* source,const CameraPoseAudit::Target& target,uint64_t epoch){
  if(!source||target.nativeReference||!epoch||target.epoch!=epoch||!CameraNativeProbe::valid(target.world))return false;
  const auto at=uintptr_t(source)+0x40;MEMORY_BASIC_INFORMATION memory{};
  if(!VirtualQuery((void*)at,&memory,sizeof(memory))||memory.State!=MEM_COMMIT||(memory.Protect&(PAGE_NOACCESS|PAGE_GUARD))||at+sizeof(target.world)>uintptr_t(memory.BaseAddress)+memory.RegionSize)return false;
  const auto protection=memory.Protect&255;if(protection!=PAGE_READWRITE&&protection!=PAGE_WRITECOPY&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return false;
  // SCS update 87ffa0 has finished computing the native SOURCE pose. Set the
  // optic here, before visibility/frustum construction and export 881ba0.
  // Export itself performs the fixed camera-local Z half turn. Do not add
  // that turn to the source and do not rewrite the exported render object.
  memcpy((void*)at,&target.world,sizeof(target.world));return true;
 }
 bool updateSource(void* source){
  CameraPoseAudit::Target target;if(!wanted.load()||!render_gpu_allowed()||!cameraPoseAudit.get(source,target))return false;
  const auto state=bettergps_camera::cameras.snapshot();if(!state.playable()||state.selection!=target.selection||state.epoch!=target.epoch)return false;
  if(!applySource(source,target,vehicleStart.epoch.load()))return false;
  ++cameraPoseAudit.sourceUpdates;return true;
 }
 void submitted(void* source){
  CameraPoseAudit::Target target;if(!wanted.load()||!cameraPoseAudit.get(source,target))return;const auto state=bettergps_camera::cameras.snapshot();
  if(!state.playable()||state.epoch!=target.epoch||state.selection!=target.selection)return;
  submittedSelection=target.selection;submittedEpoch=target.epoch;
 }
 void present(){
  const auto state=bettergps_camera::cameras.snapshot();const auto epoch=submittedEpoch.exchange(0),selection=submittedSelection.load();
  if(!wanted.load()||!state.playable()){bettergps_camera::renderEpoch=0;return;}
  const bool rendered=state.epoch==epoch&&state.selection==selection;
  if(videoFrames.present(state.epoch,state.selection,rendered)){bettergps_camera::renderSelection=state.selection;bettergps_camera::renderEpoch=state.epoch;}
 }
 bool readableUnit(uintptr_t owner,uintptr_t& unit)const{
  if(!managed_native_readable(owner+0x13b0,16))return false;const auto array=*(uintptr_t*)(owner+0x13b0),count=*(uintptr_t*)(owner+0x13b8);
  if(count!=9||!managed_native_readable(array,9*8))return false;unit=*(uintptr_t*)(array+7*8);
  return managed_native_readable(unit,0x548)&&*(uintptr_t*)unit==base+0x22cb9a8;
 }
 bool resolve(const MapImage& image){
  base=uintptr_t(image.base);address=base+0x539170;exportAddress=base+0x881ba0;updateAddress=base+0x87ffa0;
  static constexpr uint8_t mask[]={0x40,0x53,0x48,0x83,0xec,0x40,0x48,0x8b,0xd9,0x83,0xfa,0x02};
  static constexpr uint8_t source[]={0x4c,0x8b,0xdc,0x55,0x53,0x49,0x8d,0xab,0xf8,0xfc,0xff,0xff,0x48,0x81,0xec,0xf8};
  supported=cameraNativeProbe.supported&&image.contains(address,sizeof(mask))&&image.executable(address)&&!memcmp((void*)address,mask,sizeof(mask))&&image.contains(updateAddress,sizeof(source))&&image.executable(updateAddress)&&!memcmp((void*)updateAddress,source,sizeof(source));return supported;
 }
 bool install(){
  if(installed)return true;if(!supported||MH_CreateHook((void*)address,(void*)&camera_native_mask,(void**)&original)!=MH_OK)return false;
  if(MH_EnableHook((void*)address)!=MH_OK){MH_RemoveHook((void*)address);original=nullptr;return false;}installed=true;
  if(MH_CreateHook((void*)updateAddress,(void*)&camera_native_source_update,(void**)&originalUpdate)==MH_OK){if(MH_EnableHook((void*)updateAddress)==MH_OK)updateInstalled=true;else MH_RemoveHook((void*)updateAddress);}
  static constexpr uint8_t signature[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x50};
  if(managed_native_readable(exportAddress,sizeof(signature))&&!memcmp((const void*)exportAddress,signature,sizeof(signature))&&MH_CreateHook((void*)exportAddress,(void*)&camera_native_export,(void**)&originalExport)==MH_OK){if(MH_EnableHook((void*)exportAddress)==MH_OK)exportInstalled=true;else MH_RemoveHook((void*)exportAddress);}
  log(SCS_LOG_TYPE_message,"[CameraVideo] source pose before culling=%s; native render export unchanged; rear uses validated SCS initialization",updateInstalled?"installed":"unavailable");
  log(SCS_LOG_TYPE_message,"[CameraAudit] read-only final render observer %s; BetterGPSCamera.ini [Camera] PoseAudit=0 disables logging only",exportInstalled?"installed":"unavailable");return true;
 }
 void release(bool restore){
  cameraPoseAudit.unit=0;submittedEpoch=0;nativeRear=false;if(!restore)filterOwner=0;wanted=false;bettergps_camera::renderEpoch=0;bettergps_camera::renderSelection=0;renderOwner=0;
  uintptr_t unit=0;if(restore&&savedUnit&&readableUnit(savedOwner,unit)&&unit==savedUnit)memcpy((void*)(unit+0x20),&savedFov,4);
  savedOwner=savedUnit=savedEpoch=0;
 }
 void sample(void* object,const std::string& vehicle,uint64_t now){
  const auto epoch=vehicleStart.epoch.load();if(savedEpoch&&savedEpoch!=epoch)release(false);
  const auto owner=uintptr_t(object);uintptr_t owned=0;const bool ours=vehicle.find("t680_2022")!=std::string::npos&&cameraNativeProbe.poseOwner==owner&&cameraNativeProbe.ownCab&&readableUnit(owner,owned);if(ours)filterOwner=owner;
  bool app=false;if(supported&&installed&&render_gpu_allowed()&&displayGlobalEnabled&&vehicle.find("t680_2022")!=std::string::npos)for(unsigned slot=0;slot<managedDeviceCount;++slot){const auto d=managed_snapshot(slot);if(d.used&&d.enabled&&d.vehicle==vehicle&&d.style==3&&managed_is_copilot(d)&&managed_power_target(slot)&&managedPower[slot].phase>=(d.startup?copilotBootMs:0)&&copilot_camera_live(managed_page_visual(slot))){app=true;break;}}
  const bool wasActive=bettergps_camera::cameras.snapshot().active;bettergps_camera::cameras.sample(app,bettergps_camera::reverseGear,bettergps_camera::leftIndicator,bettergps_camera::rightIndicator);if(app&&!wasActive)cameraNativeProbe.sample(object,vehicle,epoch,now);
  const auto state=bettergps_camera::cameras.snapshot();const unsigned selected=unsigned(state.selected);uintptr_t unit=0;const bool reference=state.selected==bettergps_camera::View::Reverse;
  bool layoutReady=false;for(unsigned slot=0;slot<managedDeviceCount;++slot){const auto d=managed_snapshot(slot);const auto& native=managedNative[slot];if(d.used&&d.enabled&&d.vehicle==vehicle&&copilot_camera_live(managed_page_visual(slot))&&native.owner==owner&&native.applied==d.revision&&native.cameraLayout&&native.cameraRearLayout==reference)layoutReady=true;}
  if(!app||!ours||!layoutReady||!state.playable()||cameraNativeProbe.poseEpoch!=epoch||cameraNativeProbe.poseOwner!=owner||!(cameraNativeProbe.cabReady&(1u<<selected))||!readableUnit(owner,unit)||(!reference&&!updateInstalled)){if(savedOwner==owner)release(true);wanted=false;bettergps_camera::renderEpoch=0;return;}
  if(savedOwner!=owner||savedUnit!=unit){release(false);savedOwner=owner;savedUnit=unit;savedEpoch=epoch;memcpy(&savedFov,(void*)(unit+0x20),4);}
  nativeRear=reference;cameraPoseAudit.publish(unit,{epoch,state.selection,selected,cameraNativeProbe.desiredWorld[selected],cameraNativeProbe.cabWorldPose,reference});const float fov=100;memcpy((void*)(unit+0x20),&fov,4);
  if(bettergps_camera::renderSelection.load()!=state.selection)bettergps_camera::renderEpoch=0;
  const auto& pose=cameraNativeProbe.cabPoses[selected];if(lastSelection!=state.selection){lastSelection=state.selection;log(SCS_LOG_TYPE_message,"[CameraVideo] epoch=%llu view=%u installed=%x cab=(%.4f,%.4f,%.4f) hFOV=100 path=%s",(unsigned long long)epoch,selected,state.available,pose.position[0],pose.position[1],pose.position[2],reference?"SCS_NATIVE_REAR_30":"SCS_SOURCE_BEFORE_VISIBILITY");}
  filterOwner=owner;renderOwner=owner;wanted=true;
 }
 void stop(){release(false);filterOwner=0;if(updateInstalled){MH_DisableHook((void*)updateAddress);MH_RemoveHook((void*)updateAddress);}if(exportInstalled){MH_DisableHook((void*)exportAddress);MH_RemoveHook((void*)exportAddress);}if(installed){MH_DisableHook((void*)address);MH_RemoveHook((void*)address);}updateInstalled=exportInstalled=installed=false;originalUpdate=nullptr;originalExport=nullptr;original=nullptr;}
};
inline CameraNativeBackend cameraNativeBackend;
inline uint32_t __fastcall camera_native_mask(void* owner,unsigned mode){const auto native=cameraNativeBackend.original(owner,mode);if(cameraNativeBackend.filterOwner.load()!=uintptr_t(owner))return native;cameraPoseAudit.nativeMask=native;return cameraNativeBackend.wanted.load()&&render_gpu_allowed()&&(mode==2||mode==12)?native|(1u<<7):native&~(1u<<7);}
inline void __fastcall camera_native_source_update(void* source,float dt){cameraNativeBackend.originalUpdate(source,dt);if(cameraNativeBackend.wanted.load()&&cameraPoseAudit.unit.load()==uintptr_t(source))cameraNativeBackend.updateSource(source);}
inline void* __fastcall camera_native_export(void* source,void* output){auto* result=cameraNativeBackend.originalExport(source,output);if(cameraNativeBackend.wanted.load()&&cameraPoseAudit.unit.load()==uintptr_t(source)){cameraNativeBackend.submitted(source);cameraPoseAudit.observe(source,output,GetTickCount64());}return result;}
}
