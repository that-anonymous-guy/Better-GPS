#pragma once
#include "TouchDepthSnapshot.h"
#include "TouchComposition.h"
namespace bettergps_overlay {
// Only consumes immutable same-draw/frame evidence. GPU copying is done at
// the native lifetime boundary, never from an older projected mesh.
struct TouchCpuPicker {
 struct Candidate {TouchHit hit;std::shared_ptr<const TouchGeometryDepth> depth;float z=0;touch_geometry::Viewport viewport;};
 std::array<uint64_t,managedDeviceCount> lastSample{};
 static bool fresh(const TouchGeometryMesh& m,uint64_t now){return m.session==touchStatus.sourceGeneration.load()&&now>=m.at&&now-m.at<=150&&m.epoch==vehicleStart.epoch.load();}
 static bool current(const TouchHit& h){
  if(h.surface.slot>=managedDeviceCount||h.surface.epoch!=vehicleStart.epoch.load()||!vehicleStart.eligible)return false;
  const auto d=managed_snapshot(h.surface.slot);const auto page=managed_page_visual(h.surface.slot);
  return d.used&&d.enabled&&d.vehicle==managed_vehicle()&&managed_page_kind(d)!=PageDevice::None&&managed_power_target(h.surface.slot)&&h.surface.power==managedPower[h.surface.slot].serial&&h.surface.page==page.serial&&!page.transition;
 }
 bool find(const TouchGeometryScene& scene,const TouchInput::Request& request,unsigned targetW,unsigned targetH,unsigned width,unsigned height,uint64_t now,Candidate& found,std::string& why){
  bool any=false;why="Awaiting same-frame device/depth ticket";
  for(const auto& m:scene.meshes){
   if(!fresh(m,now)||m.slot>=managedDeviceCount||(request.slot<managedDeviceCount&&m.slot!=request.slot)||!m.depth)continue;
   const auto& d=*m.depth;const auto& vp=m.viewport;
   if(d.trace!=request.trace||d.id<=lastSample[m.slot]||d.at<request.began||!TouchInput::close_point(d.cursor,request.cursor))continue;
   if(d.clientW!=width||d.clientH!=height){why="Client resized since native snapshot";continue;}
   if(vp.targetW!=targetW||vp.targetH!=targetH||vp.x!=0||vp.y!=0||vp.w!=targetW||vp.h!=targetH){why="Native pass is not the current full backbuffer";continue;}
   for(const auto& triangle:m.triangles){float u=0,v=0,z=0;
   if(!touch_geometry::intersect(triangle,m.viewport,float(targetW),float(targetH),{d.x+.5f,d.y+.5f},u,v,z))continue;
   float a=0,b=0;if(!touch_geometry::address(u,m.addressU,a)||!touch_geometry::address(v,m.addressV,b)||!(m.width>0&&m.height>0)){why="Unsupported texture addressing/content fit";return false;}
   unsigned axes=0;const int copy=touchComposition.mapping(m.proof,now,axes);if(copy!=1){why=copy==0?"Native copy mapping pending":"Native copy mapping rejected/ambiguous/expired";continue;}TouchComposition::transform(axes,a,b);
   a=(a-m.left)/m.width;b=(b-m.top)/m.height;if(a<0||a>1||b<0||b>1)continue;
   TouchHit hit;hit.valid=true;hit.cursor=d.cursor;hit.time=m.at;hit.trace=request.trace;hit.u=a;hit.v=b;hit.surface={m.slot,m.left,m.top,m.width,m.height,m.epoch,m.power,m.page,m.proof};
   if(!current(hit)){why="Device/page/power generation is not current";continue;}
   if(any){if(found.hit.surface.slot!=m.slot||found.depth->id!=d.id||std::abs(found.hit.u-a)>.002f||std::abs(found.hit.v-b)>.002f||std::abs(found.z-z)>.000001f){why="Ambiguous projected surfaces";return false;}continue;}
   found={hit,m.depth,z,m.viewport};any=true;
  }}
  if(!any)return false;
  const auto proof=touchComposition.verdict(found.hit.surface.proof,now);if(proof!=1){why=proof==0?"Actual native image-copy verification pending":"Native image-copy verification rejected/expired";return false;}
  if(!found.depth||!found.depth->usable){why=found.depth?found.depth->reason:"No native depth evidence";return false;}
  return true;
 }
 static bool matches(float actual,float expected,DXGI_FORMAT format){
  const float epsilon=format==DXGI_FORMAT_D16_UNORM?2.f/65535.f:format==DXGI_FORMAT_D24_UNORM_S8_UINT?2.f/16777215.f:std::max(1e-9f,2*(std::nextafter(expected,1.f)-expected));
  return std::isfinite(actual)&&std::isfinite(expected)&&expected>0&&expected<1&&std::abs(actual-expected)<=epsilon;
 }
 void stop(){lastSample={};}
 void present(ID3D11DeviceContext*,unsigned targetW,unsigned targetH,unsigned width,unsigned height,uint64_t now){
  touchMainTargetWidth=targetW;touchMainTargetHeight=targetH;
  if(!touch_active()){stop();return;}TouchInput::Request request;if(!touchInput.request(request)){stop();return;}
  if(!width||!height||!targetW||!targetH||request.cursor.x<0||request.cursor.y<0||unsigned(request.cursor.x)>=width||unsigned(request.cursor.y)>=height||menuDock.blocks_world(request.cursor)){if(request.hover)immersiveHoverProbe.cancel();else{touchDiagnostics.note_for(request.trace,TouchStage::Cancelled,"Pointer outside client or covered by controls");touchInput.cancel();}return;}
  auto scene=touchGeometryView.read();if(!scene)return;
  Candidate candidate;std::string why;
  const bool located=find(*scene,request,targetW,targetH,width,height,now,candidate,why);if(candidate.hit.valid)touchStatus.candidate(candidate.hit.cursor,now);
  if(!located){if(!request.hover)touchDiagnostics.note_for(request.trace,TouchStage::NoCoordinates,"cpu-frame %s",why.c_str());return;}
  const auto& depth=*candidate.depth;
  if(!depth.ready||!depth.sealed){if(!depth.usable)touchDiagnostics.note_for(request.trace,TouchStage::Depth,"snapshot rejected: %s",depth.reason.c_str());return;}lastSample[candidate.hit.surface.slot]=depth.id;
  auto hit=candidate.hit;hit.cursor=depth.cursor;hit.trace=request.trace;
  if(depth.scissored&&(int(depth.x)<depth.scissor.left||int(depth.y)<depth.scissor.top||int(depth.x)>=depth.scissor.right||int(depth.y)>=depth.scissor.bottom)){touchDiagnostics.note_for(request.trace,TouchStage::PixelMiss,"Outside native scissor");hit.valid=false;if(!request.hover)touchInput.hit(hit);return;}
  const float expected=depth.min+candidate.z*(depth.max-depth.min);
  hit.valid=matches(depth.actual,expected,depth.viewFormat)&&current(hit)&&!menuDock.blocks_world(hit.cursor);uint64_t raw=0;memcpy(&raw,depth.raw.data(),8);
  if(!request.hover)touchDiagnostics.note_for(hit.trace,hit.valid?TouchStage::Hit:TouchStage::PixelMiss,"backend=cpu-frame slot=%u frame=%llu snapshot=%llu uv=%.4f,%.4f z=%.9f expected=%.9f geometry_age=%llu format=%u raw=%016llx boundary=%s depth=%u write=%u func=%u stencil=%u",hit.surface.slot,depth.frame,depth.id,hit.u,hit.v,depth.actual,expected,now-depth.at,unsigned(depth.viewFormat),raw,depth.boundary.c_str(),unsigned(depth.state.DepthEnable),unsigned(depth.state.DepthWriteMask),unsigned(depth.state.DepthFunc),unsigned(depth.state.StencilEnable));
  if(request.hover)immersiveHoverProbe.hit(hit,now);else touchInput.hit(hit);
 }
};
inline TouchCpuPicker touchCpuPicker;
}
