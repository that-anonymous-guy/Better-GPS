#pragma once
#include "CameraState.h"
#include "CameraOpticsMath.h"
namespace bettergps_overlay {
// Read-only first stage: observe installed scene locators on the native frame
// thread. No render-camera object, scene pose or native camera field is written.
struct CameraNativeProbe {
 using Pose32=bettergps_camera::PackedPose;
 bool ownCab=false;Pose32 cabWorldPose{};std::array<Pose32,4> desiredWorld{},cabPoses{};unsigned cabReady=0;uint64_t poseEpoch=0;uintptr_t poseOwner=0;
 using LocateScene=bool(__fastcall*)(void*,Pose32*,const uint64_t*);
 using Locate=void*(__fastcall*)(void*,Pose32*,const uint64_t*);
 using Compose=Pose32*(__fastcall*)(const Pose32*,Pose32*,const Pose32*);
 Compose compose{};LocateScene locateScene{};const char* reason="not sampled";uintptr_t base=0;size_t size=0;bool supported=false;
 uint64_t epoch=0,last=0;unsigned lastMask=~0u;uintptr_t lastOwner=0;
 static constexpr uint64_t token(const char* name){uint64_t result=0,multiplier=1;for(;*name;++name){const char c=*name;const unsigned digit=c>='0'&&c<='9'?1+c-'0':c>='a'&&c<='z'?11+c-'a':37;result+=digit*multiplier;multiplier*=38;}return result;}
 bool resolve(const MapImage& image){
  base=uintptr_t(image.base);size=image.size;
  static constexpr uint8_t findSignature[]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x55,0x56};
  static constexpr uint8_t sceneSignature[]={0x48,0x89,0x5c,0x24,0x20,0x4c,0x89,0x44,0x24,0x18,0x48,0x89};
  static constexpr uint8_t composeSignature[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x78,0x10,0x55};
  supported=image.contains(base+0x15e4de0,sizeof(sceneSignature))&&image.executable(base+0x15e4de0)&&!memcmp(image.base+0x15e4de0,sceneSignature,sizeof(sceneSignature))&&image.contains(base+0x549a10,sizeof(findSignature))&&image.executable(base+0x549a10)&&!memcmp(image.base+0x549a10,findSignature,sizeof(findSignature))&&image.contains(base+0x139d890,sizeof(composeSignature))&&image.executable(base+0x139d890)&&!memcmp(image.base+0x139d890,composeSignature,sizeof(composeSignature));
  if(supported){compose=reinterpret_cast<Compose>(base+0x139d890);locateScene=reinterpret_cast<LocateScene>(base+0x15e4de0);}
  return supported;
 }
 static bool valid(const Pose32& pose){float norm=0;for(float p:pose.position)if(!std::isfinite(p)||std::abs(p)>1e7f)return false;for(float q:pose.rotation){if(!std::isfinite(q))return false;norm+=q*q;}return norm>.8f&&norm<1.2f;}
 bool combine(void* node,const Pose32& local,Pose32& result){
  if(!node){reason="locator missing";return false;}if(!valid(local)){reason="invalid local pose";return false;}
  if(!managed_native_readable(uintptr_t(node)+0x30,sizeof(Pose32))){reason="unreadable parent pose";return false;}
  // Native scene allocations need not meet Pose32's 16-byte alignment.
  // Keep the ABI-aligned local required by compose(), and load engine bytes
  // without typed dereferences that MSVC can turn into faulting MOVAPS.
  Pose32 parent{};memcpy(&parent,reinterpret_cast<const void*>(uintptr_t(node)+0x30),sizeof(parent));if(!valid(parent)){reason="invalid parent pose";return false;}
  compose(&parent,&result,&local);if(!valid(result)){reason="invalid combined pose";return false;}return true;
 }
 bool locate(uintptr_t truck,const char* name,Pose32& result,Pose32* attachmentFrame=nullptr){
  reason="unreadable truck";if(!managed_native_readable(truck,0x7d0))return false;
  const auto key=token(name);Pose32 local{};
  const auto entries=*reinterpret_cast<const uintptr_t*>(truck+0x798),count=*reinterpret_cast<const uintptr_t*>(truck+0x7a0);
  // Match native mirror discovery: direct attached scenes FIRST. Its recursive
  // fallback alone does not cover every scene in the truck model array.
  if(locateScene&&count<=256&&managed_native_readable(entries,count*0x30))for(size_t i=0;i<count;++i){
   const auto node=*reinterpret_cast<const uintptr_t*>(entries+i*0x30+0x10);
   if(!node||!managed_native_readable(node,8))continue;
   local={};Pose32 inFrame{};if(!locateScene(reinterpret_cast<void*>(node),&local,&key)||!combine(reinterpret_cast<void*>(node),local,inFrame))continue;
   const auto parent=*reinterpret_cast<const uintptr_t*>(entries+i*0x30+0x18);if(!combine(reinterpret_cast<void*>(parent),inFrame,result))continue;
   if(attachmentFrame)memcpy(attachmentFrame,reinterpret_cast<const void*>(parent+0x30),sizeof(Pose32));reason="direct scene";return true;
  }
  // Native addon/hookup scenes are a separate 0x18-byte array. The front
  // camera is a child of bumper, not a primary tuning model. Compose its
  // parent model and that model's actual world frame, never a fixed offset.
  const auto children=*reinterpret_cast<const uintptr_t*>(truck+0x7c0),childCount=*reinterpret_cast<const uintptr_t*>(truck+0x7c8);
  if(locateScene&&count<=256&&childCount<=256&&managed_native_readable(entries,count*0x30)&&managed_native_readable(children,childCount*0x18))for(size_t i=0;i<childCount;++i){
   const auto parentName=*reinterpret_cast<const uint64_t*>(children+i*0x18),node=*reinterpret_cast<const uintptr_t*>(children+i*0x18+8);
   if(!node||!managed_native_readable(node,8))continue;local={};Pose32 inParent{};
   if(!locateScene(reinterpret_cast<void*>(node),&local,&key)||!combine(reinterpret_cast<void*>(node),local,inParent))continue;
   for(size_t j=0;j<count;++j){if(*reinterpret_cast<const uint64_t*>(entries+j*0x30)!=parentName)continue;
    const auto parent=*reinterpret_cast<const uintptr_t*>(entries+j*0x30+0x10),frame=*reinterpret_cast<const uintptr_t*>(entries+j*0x30+0x18);Pose32 inFrame{};
    if(!combine(reinterpret_cast<void*>(parent),inParent,inFrame)||!combine(reinterpret_cast<void*>(frame),inFrame,result))continue;
    if(attachmentFrame)memcpy(attachmentFrame,reinterpret_cast<const void*>(frame+0x30),sizeof(Pose32));reason="hookup child and actual parent frame";return true;
   }
  }
  const auto table=*reinterpret_cast<const uintptr_t*>(truck);reason="invalid recursive vtable";
  if(table<base||table>base+size-0x120||!managed_native_readable(table+0x118,8))return false;
  const auto address=*reinterpret_cast<const uintptr_t*>(table+0x118);reason="invalid recursive function";
  if(address<base||address>=base+size)return false;
  MEMORY_BASIC_INFORMATION region{};if(!VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))||!(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
  local={};auto* node=reinterpret_cast<Locate>(address)(reinterpret_cast<void*>(truck),&local,&key);
  if(!combine(node,local,result))return false;if(attachmentFrame)memcpy(attachmentFrame,reinterpret_cast<const void*>(uintptr_t(node)+0x30),sizeof(Pose32));reason="root scene";return true;
 }
 void sample(void* owner,const std::string& vehicle,uint64_t vehicleEpoch,uint64_t now){
  if(epoch!=vehicleEpoch){epoch=vehicleEpoch;last=0;lastMask=~0u;cabReady=0;poseEpoch=0;poseOwner=0;ownCab=false;bettergps_camera::cameras.reset(vehicleEpoch);}
  if(!supported||!nativeFork.supported||!nativeFork.unpack||vehicle.find("t680_2022")==std::string::npos||!owner||!managed_native_readable(uintptr_t(owner)+0x118,8))return;
  if(vehicleEpoch!=epoch||lastOwner!=uintptr_t(owner)){epoch=vehicleEpoch;lastOwner=uintptr_t(owner);last=0;lastMask=~0u;}
  if(last&&now-last<1000&&!bettergps_camera::cameras.snapshot().active)return;last=now;
  const auto truck=*reinterpret_cast<uintptr_t*>(uintptr_t(owner)+0x118);if(!truck)return;
  static constexpr const char* names[]={"bgps_v_f","bgps_v_l","bgps_v_r","bgps_v_b"};
  std::array<bettergps_camera::Pose,4> poses{};std::array<const char*,4> results{};unsigned mask=0;cabReady=0;
  Pose32 cabWorld{};const bool haveCab=locate(truck,"bgps_frame",cabWorld);ownCab=haveCab;if(haveCab)cabWorldPose=cabWorld;
  for(unsigned i=0;i<4;++i){Pose32 p{},body{};if(!locate(truck,names[i],p,&body)){results[i]=reason;continue;}results[i]=reason;
   float view[4]{};bettergps_camera::camera_rotation(bettergps_camera::yawDegrees[i],bettergps_camera::downDegrees[i],view);bettergps_camera::quaternion_multiply(body.rotation,view,p.rotation);
   std::array<float,4> world{};nativeFork.unpack(world.data(),&p);if(!std::isfinite(world[0])||!std::isfinite(world[1])||!std::isfinite(world[2]))continue;
   mask|=1u<<i;for(unsigned j=0;j<3;++j)poses[i].position[j]=world[j];for(unsigned j=0;j<4;++j)poses[i].rotation[j]=p.rotation[j];
   if(haveCab){bettergps_camera::relative_pose(p,cabWorld,cabPoses[i]);if(valid(cabPoses[i])){
    // Position follows the actual mounted optic; viewing angles are defined
    // in truck/cab axes, never inherited from a rotated accessory bracket.
    std::copy_n(view,4,cabPoses[i].rotation);compose(&cabWorld,&desiredWorld[i],&cabPoses[i]);cabReady|=1u<<i;
   }}
  }
  poseEpoch=vehicleEpoch;poseOwner=uintptr_t(owner);
  bettergps_camera::cameras.hardware(vehicleEpoch,mask,poses);
  if(mask==lastMask)return;lastMask=mask;
  log(SCS_LOG_TYPE_message,"[CameraProbe] epoch=%llu installed=%x (Forward=1 Left=2 Right=4 Rear=8); native locators; owner=%p truck=%p scenes=%llu",(unsigned long long)vehicleEpoch,mask,owner,(void*)truck,(unsigned long long)(managed_native_readable(truck+0x7a0,8)?*(uintptr_t*)(truck+0x7a0):0));
  for(unsigned i=0;i<4;++i)log(SCS_LOG_TYPE_message,"[CameraProbe] %s lookup=%s",names[i],results[i]?results[i]:"not attempted");
  Pose32 baseline{};const bool mirrorKnown=locate(truck,"cam_d",baseline);log(SCS_LOG_TYPE_message,"[CameraProbe] stock cam_d=%d (%s)",int(mirrorKnown),reason);const bool frameKnown=locate(truck,"bgps_frame",baseline);log(SCS_LOG_TYPE_message,"[CameraProbe] plugin cab frame=%d (%s)",int(frameKnown),reason);
  for(unsigned i=0;i<4;++i)if(cabReady&(1u<<i))log(SCS_LOG_TYPE_message,"[CameraProbe] %s cab-relative origin=(%.6f,%.6f,%.6f)",names[i],cabPoses[i].position[0],cabPoses[i].position[1],cabPoses[i].position[2]);
  for(unsigned i=0;i<4;++i)if(mask&(1u<<i)){const auto& p=poses[i];log(SCS_LOG_TYPE_message,"[CameraProbe] %s native origin=(%.6f,%.6f,%.6f) parent quaternion=(%.6f,%.6f,%.6f,%.6f) hFOV=100",names[i],p.position[0],p.position[1],p.position[2],p.rotation[0],p.rotation[1],p.rotation[2],p.rotation[3]);}
 }
};
inline CameraNativeProbe cameraNativeProbe;
}
