#pragma once
#include "TouchGeometryView.h"
#include "TouchVertexCpu.h"
#include "TouchDepthSnapshot.h"
#include "TouchComposition.h"
#include "generated/TouchGpsBounds.h"
#include <wrl/client.h>
#include <d3d11_1.h>
#include <tuple>
namespace bettergps_overlay {
using Microsoft::WRL::ComPtr;
inline const GUID kGeometryCode={0x2a7e6981,0xcc71,0x45a1,{0xab,0x32,0x81,0xd6,0x81,0x73,0x61,0x01}};
inline const GUID kGeometryCodeInfo={0x2a7e6988,0xcc71,0x45a1,{0xab,0x32,0x81,0xd6,0x81,0x73,0x61,0x01}};
inline const GUID kGeometryLayout={0x2a7e6982,0xcc71,0x45a1,{0xab,0x32,0x81,0xd6,0x81,0x73,0x61,0x01}};
inline const GUID kGeometryVS={0x2a7e6983,0xcc71,0x45a1,{0xab,0x32,0x81,0xd6,0x81,0x73,0x61,0x01}};
inline const GUID kGeometryPS={0x2a7e6984,0xcc71,0x45a1,{0xab,0x32,0x81,0xd6,0x81,0x73,0x61,0x01}};
inline const GUID kGeometryExtensions={0x2a7e6985,0xcc71,0x45a1,{0xab,0x32,0x81,0xd6,0x81,0x73,0x61,0x01}};
inline const GUID kGeometryVSBindingInfo={0x2a7e6986,0xcc71,0x45a1,{0xab,0x32,0x81,0xd6,0x81,0x73,0x61,0x01}};
inline const GUID kGeometryPSBindingInfo={0x2a7e6987,0xcc71,0x45a1,{0xab,0x32,0x81,0xd6,0x81,0x73,0x61,0x01}};
struct GeometryShaderState {UINT classes=0;HRESULT store=E_PENDING;bool bound=false;};
struct GeometryCodeInfo {unsigned outcome=0;uint64_t length=0;HRESULT result=E_PENDING;};
struct GeometryCodeStats {std::atomic<size_t> bytes{0};std::atomic<unsigned> retained{0},budgetRejected{0},sizeRejected{0},storeFailed{0};};
inline GeometryCodeStats geometryCodeStats;
struct GeometryElement {char semantic[32]{};UINT index=0,slot=0,offset=0;DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;};
struct GeometryLayout {UINT count=0;std::array<GeometryElement,32> elements{};};
inline unsigned geometry_format_bytes(DXGI_FORMAT f){switch(f){
 case DXGI_FORMAT_R32G32B32A32_FLOAT:return 16;case DXGI_FORMAT_R32G32B32_FLOAT:return 12;
 case DXGI_FORMAT_R32G32_FLOAT:case DXGI_FORMAT_R16G16B16A16_FLOAT:case DXGI_FORMAT_R16G16B16A16_SNORM:case DXGI_FORMAT_R16G16B16A16_UNORM:return 8;
 case DXGI_FORMAT_R32_FLOAT:case DXGI_FORMAT_R16G16_FLOAT:case DXGI_FORMAT_R16G16_SNORM:case DXGI_FORMAT_R16G16_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_SNORM:case DXGI_FORMAT_R10G10B10A2_UNORM:return 4;default:return 0;}}
inline void geometry_tag_code(ID3D11DeviceChild* shader,const void* bytes,SIZE_T length){
 // Storage only: no compiler/disassembler, GPU execution or disk I/O in hooks.
 if(!shader||phoneInternalWork||!bytes||!length)return;
 const auto report=[&](unsigned outcome,HRESULT result){const GeometryCodeInfo info{outcome,uint64_t(length),result};shader->SetPrivateData(kGeometryCodeInfo,sizeof(info),&info);};
 if(length>65536){++geometryCodeStats.sizeRejected;report(2,E_BOUNDS);return;}
 size_t stored=geometryCodeStats.bytes.load();do{if(stored+length>16*1024*1024){++geometryCodeStats.budgetRejected;report(3,E_OUTOFMEMORY);return;}}while(!geometryCodeStats.bytes.compare_exchange_weak(stored,stored+length));
 const auto result=shader->SetPrivateData(kGeometryCode,UINT(length),bytes);report(SUCCEEDED(result)?1:4,result);if(SUCCEEDED(result))++geometryCodeStats.retained;else {geometryCodeStats.bytes.fetch_sub(length);++geometryCodeStats.storeFailed;}
}
inline void geometry_tag_layout(ID3D11InputLayout* layout,const D3D11_INPUT_ELEMENT_DESC* desc,UINT count){
 if(!layout||phoneInternalWork||!desc||count>32)return;GeometryLayout value;std::array<UINT,32> end{};
 for(UINT i=0;i<count;++i){if(!desc[i].SemanticName||strlen(desc[i].SemanticName)>=32||desc[i].InputSlot>=32||desc[i].InputSlotClass!=D3D11_INPUT_PER_VERTEX_DATA||!geometry_format_bytes(desc[i].Format))return;
  auto& element=value.elements[i];strcpy_s(element.semantic,desc[i].SemanticName);element.index=desc[i].SemanticIndex;element.slot=desc[i].InputSlot;element.format=desc[i].Format;element.offset=desc[i].AlignedByteOffset==D3D11_APPEND_ALIGNED_ELEMENT?end[element.slot]:desc[i].AlignedByteOffset;end[element.slot]=element.offset+geometry_format_bytes(element.format);}
 value.count=count;layout->SetPrivateData(kGeometryLayout,sizeof(value),&value);
}
inline void geometry_shadow_shader(ID3D11DeviceContext* c,const GUID& key,ID3D11DeviceChild* shader,UINT classes){
 if(phoneInternalWork)return;
 // Native binding lifetime is independent of diagnostic capture lifetime.
 // Observe binds even while the HUD is closed; no bytecode query/copy here.
 GeometryShaderState state{classes,c->SetPrivateDataInterface(key,shader),shader!=nullptr};const auto& infoKey=key==kGeometryVS?kGeometryVSBindingInfo:kGeometryPSBindingInfo;c->SetPrivateData(infoKey,sizeof(state),&state);
}
inline void geometry_shadow_extension(ID3D11DeviceContext* c,unsigned bit,bool on){if(phoneInternalWork)return;unsigned mask=0;UINT bytes=sizeof(mask);c->GetPrivateData(kGeometryExtensions,&bytes,&mask);mask=on?mask|bit:mask&~bit;c->SetPrivateData(kGeometryExtensions,sizeof(mask),&mask);}
struct TouchGeometryCapture {
 struct Buffer {ComPtr<ID3D11Buffer> staging;std::vector<uint8_t> data;UINT bytes=0,stride=0,offset=0,sourceBegin=0,nativeBytes=0;};
 struct Job {
  TouchSurface surface;uint64_t at=0,frame=0,session=0;UINT count=0,start=0;INT base=0;bool indexed=false;DXGI_FORMAT indexFormat=DXGI_FORMAT_UNKNOWN;UINT indexOffset=0;
  GeometryLayout layout;touch_geometry::Viewport viewport;std::vector<uint8_t> vs,ps;
  std::array<Buffer,32> vertices;std::array<Buffer,14> constants;Buffer indices;
  std::array<D3D11_SAMPLER_DESC,16> samplers{};std::array<bool,16> hasSampler{};
  unsigned textureSlot=0;ComPtr<ID3D11Query> completed;
  std::shared_ptr<TouchGeometryDepth> depth;
  const TouchGpsBounds* gps=nullptr;
 };
 std::mutex mutex;std::deque<std::unique_ptr<Job>> pending,ready;
 std::vector<TouchGeometryMesh> meshes;std::string reason="Awaiting native screen draw";std::atomic<unsigned> observed{0},dropped{0};unsigned completed=0;
 std::atomic<uint64_t> frame{1};std::array<uint64_t,managedDeviceCount+1> lastCapture{};std::map<uint64_t,std::string> savedEvidence;std::map<std::string,uint64_t> failures;uint64_t lastPublish=0,lastLogTrace=~uint64_t(0),lastLogAt=0,lastLogSession=~uint64_t(0),lastAcquisitionAt=0;std::string lastLog;
 std::array<uint64_t,managedDeviceCount+1> lastCaptureTrace{};std::array<bool,managedDeviceCount+1> lastCaptureUsable{};std::atomic<uint64_t> missedBoundary{0};std::atomic<bool> watchDepth{false};uint64_t nextDepthId=0;
 struct DrawClock {uint64_t at=0,trace=0;bool usable=false;};
 // Equal-size cluster/GPS draws can share the output and index/VB pools.
 // The native range AND bindings distinguish them, not count/texture alone.
 using DrawIdentity=std::tuple<unsigned,UINT,bool,uintptr_t,UINT,INT,uintptr_t,UINT,UINT,uintptr_t,UINT>;
 std::map<DrawIdentity,DrawClock> integratedClocks;
 void failed_locked(){const auto key=reason.substr(0,reason.find(' '));if(failures.size()<16||failures.count(key))++failures[key];}
 static bool code(ID3D11DeviceContext* context,const GUID& key,std::vector<uint8_t>& out,std::string* failure=nullptr){
  const auto fail=[&](const char* operation,HRESULT hr){if(failure){char text[256];snprintf(text,sizeof(text),"%s hr=0x%08lx retained=%u code_bytes=%zu budget_rejected=%u size_rejected=%u store_failed=%u",operation,static_cast<unsigned long>(hr),geometryCodeStats.retained.load(),geometryCodeStats.bytes.load(),geometryCodeStats.budgetRejected.load(),geometryCodeStats.sizeRejected.load(),geometryCodeStats.storeFailed.load());*failure=text;}return false;};
  GeometryShaderState state;UINT infoBytes=sizeof(state);const auto& infoKey=key==kGeometryVS?kGeometryVSBindingInfo:kGeometryPSBindingInfo;auto hr=context->GetPrivateData(infoKey,&infoBytes,&state);if(FAILED(hr)||infoBytes!=sizeof(state))return fail("binding-unobserved",hr);if(FAILED(state.store))return fail("binding-store-failed",state.store);if(!state.bound)return fail("shader-unbound",S_FALSE);if(state.classes)return fail("shader-class-linkage-unsupported",E_NOTIMPL);
  ComPtr<IUnknown> object;UINT size=sizeof(IUnknown*);hr=context->GetPrivateData(key,&size,object.GetAddressOf());if(FAILED(hr)||!object)return fail("binding-object-unavailable",hr);
  ComPtr<ID3D11DeviceChild> shader;hr=object.As(&shader);if(FAILED(hr))return fail("shader-interface-query",hr);size=0;hr=shader->GetPrivateData(kGeometryCode,&size,nullptr);if(FAILED(hr)||!size){GeometryCodeInfo info;UINT infoSize=sizeof(info);if(SUCCEEDED(shader->GetPrivateData(kGeometryCodeInfo,&infoSize,&info))){if(info.outcome==2)return fail("bytecode-size-rejected",info.result);if(info.outcome==3)return fail("bytecode-retention-budget",info.result);if(info.outcome==4)return fail("bytecode-store-failed",info.result);return fail("bytecode-not-retained creation=observed",hr);}return fail("bytecode-not-retained creation=unobserved",hr);}if(size>65536)return fail("bytecode-size-unsupported",E_BOUNDS);
  out.resize(size);hr=shader->GetPrivateData(kGeometryCode,&size,out.data());if(FAILED(hr))return fail("bytecode-read-failed",hr);return true;
 }
 void reject(std::string why){std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(lock)reason=std::move(why);else ++dropped;}
 void unsupported_draw(ID3D11DeviceContext* context,const char* why){if(phoneInternalWork||!touch_active())return;unsigned binding=0;UINT bytes=sizeof(binding);if(SUCCEEDED(context->GetPrivateData(kTouchBindings,&bytes,&binding))&&binding){++observed;reject(why);}}
 static std::shared_ptr<TouchGeometryDepth> depth_evidence(ID3D11DeviceContext* context,const D3D11_VIEWPORT& viewport){
  auto result=std::make_shared<TouchGeometryDepth>();result->min=viewport.MinDepth;result->max=viewport.MaxDepth;
  ComPtr<ID3D11DepthStencilView> view;context->OMGetRenderTargets(0,nullptr,&view);if(!view)return result;
  D3D11_DEPTH_STENCIL_VIEW_DESC vd{};view->GetDesc(&vd);if(vd.ViewDimension!=D3D11_DSV_DIMENSION_TEXTURE2D||vd.Texture2D.MipSlice){result->reason="Depth view is not single-sample base level";return result;}
  ComPtr<ID3D11DepthStencilState> state;UINT reference=0;context->OMGetDepthStencilState(&state,&reference);D3D11_DEPTH_STENCIL_DESC sd{};sd.DepthEnable=TRUE;sd.DepthFunc=D3D11_COMPARISON_LESS;if(state)state->GetDesc(&sd);
  result->state=sd;result->viewFormat=vd.Format;
  if(!sd.DepthEnable||sd.DepthFunc==D3D11_COMPARISON_ALWAYS||sd.DepthFunc==D3D11_COMPARISON_NEVER){char why[144];snprintf(why,sizeof(why),"DepthEnable=%u DepthFunc=%u WriteMask=%u StencilEnable=%u",unsigned(sd.DepthEnable),unsigned(sd.DepthFunc),unsigned(sd.DepthWriteMask),unsigned(sd.StencilEnable));result->reason=why;return result;}
  // Stencil can legitimately be enabled in a depth-tested native material.
  // It is not, by itself, evidence that the draw/surface is unsupported.
  ComPtr<ID3D11RasterizerState> raster;context->RSGetState(&raster);if(raster){D3D11_RASTERIZER_DESC rd{};raster->GetDesc(&rd);if(rd.DepthBias||rd.SlopeScaledDepthBias||rd.DepthBiasClamp||!rd.DepthClipEnable){result->reason="Biased/nonstandard depth rasterization";return result;}if(rd.ScissorEnable){UINT n=1;context->RSGetScissorRects(&n,&result->scissor);if(n!=1){result->reason="Missing/multiple scissor";return result;}result->scissored=true;}}
  ComPtr<ID3D11Resource> resource;view->GetResource(&resource);if(FAILED(resource.As(&result->texture)))return result;
  result->texture->GetDesc(&result->desc);result->usable=true;result->reason="Native depth available; awaiting coherent snapshot";return result;
 }
 void before_overwrite(ID3D11DeviceContext* context,ID3D11Resource* resource,const char* boundary="resource-overwrite"){
  if(phoneInternalWork||!watchDepth||!resource||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||!touch_active())return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock){++missedBoundary;return;}
  ComPtr<ID3D11Texture2D> texture;if(FAILED(resource->QueryInterface(IID_PPV_ARGS(&texture))))return;
  for(auto& job:pending)if(job->depth&&job->depth->texture.Get()==texture.Get()&&job->depth->continuity==missedBoundary.load())touch_depth_seal(context,*job->depth,boundary);
 }
 void before_depth_clear(ID3D11DeviceContext* context,ID3D11DepthStencilView* view,UINT flags){if(phoneInternalWork||!(flags&D3D11_CLEAR_DEPTH)||!view)return;ComPtr<ID3D11Resource> resource;view->GetResource(&resource);before_overwrite(context,resource.Get(),"before-depth-clear");}
 void before_draw(ID3D11DeviceContext* context,UINT count){
  TouchComposition::before_draw(context);
  if(phoneInternalWork||!watchDepth||(count!=3&&count!=6)||!touch_active())return;ComPtr<ID3D11DepthStencilState> state;UINT reference=0;context->OMGetDepthStencilState(&state,&reference);if(!state)return;D3D11_DEPTH_STENCIL_DESC d{};state->GetDesc(&d);
  if(d.DepthEnable&&d.DepthWriteMask==D3D11_DEPTH_WRITE_MASK_ALL&&d.DepthFunc==D3D11_COMPARISON_ALWAYS){ComPtr<ID3D11DepthStencilView> view;context->OMGetRenderTargets(0,nullptr,&view);if(view){ComPtr<ID3D11Resource> resource;view->GetResource(&resource);before_overwrite(context,resource.Get(),"before-depth-always-draw");}}
 }
 static bool stage_range(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Buffer* native,Buffer& out,size_t& total,uint64_t begin,uint64_t length,std::string& failure,const char* role,unsigned slot=0){
  D3D11_BUFFER_DESC source{};if(native)native->GetDesc(&source);
  const auto fail=[&](const char* operation,HRESULT hr){char text[288];snprintf(text,sizeof(text),"%s.%s slot=%u native=%u range=%llu+%llu total=%zu usage=%u bind=0x%x misc=0x%x hr=0x%08lx",role,operation,slot,source.ByteWidth,begin,length,total,unsigned(source.Usage),source.BindFlags,source.MiscFlags,static_cast<unsigned long>(hr));failure=text;return false;};
  if(!native)return fail("buffer-missing",E_POINTER);if(!length||begin>source.ByteWidth||length>source.ByteWidth-begin)return fail("range-invalid",E_BOUNDS);
  if(length>8*1024*1024)return fail("range-budget",E_OUTOFMEMORY);if(total+length>16*1024*1024)return fail("job-budget",E_OUTOFMEMORY);
  D3D11_BUFFER_DESC d{};d.ByteWidth=UINT(length);d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  const auto hr=device->CreateBuffer(&d,nullptr,&out.staging);if(FAILED(hr))return fail("create-staging",hr);
  out.bytes=UINT(length);out.sourceBegin=UINT(begin);out.nativeBytes=source.ByteWidth;total+=length;
  const D3D11_BOX box{UINT(begin),0,0,UINT(begin+length),1,1};context->CopySubresourceRegion(out.staging.Get(),0,0,0,0,native,0,&box);return true;
 }
 static bool index_at(const Job& job,unsigned i,uint32_t& value){
  const unsigned stride=job.indexFormat==DXGI_FORMAT_R16_UINT?2:4;const uint64_t address=uint64_t(job.indexOffset)+(uint64_t(job.start)+i)*stride;
  if(address<job.indices.sourceBegin||address-job.indices.sourceBegin+stride>job.indices.data.size())return false;value=0;memcpy(&value,job.indices.data.data()+address-job.indices.sourceBegin,stride);return true;
 }
 void draw(ID3D11DeviceContext* context,UINT count,UINT start,INT base,bool indexed,UINT instances=1,UINT firstInstance=0){
  if(phoneInternalWork||!touch_active()||!count)return;
  unsigned binding=0;UINT bytes=sizeof(binding);if(FAILED(context->GetPrivateData(kTouchBindings,&bytes,&binding))||!binding)return;
  ++observed;
  if(context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE){reject("deferred draw: state capture not supported yet");return;}
  if(!touchStatus.visible&&!touchInput.requested())return;
  // Some 2D shaders predate SDK loading. Certify their actual image transfer,
  // without guessing bytecode/coordinates or replaying a native draw.
  if(!touch_gpu_backend_allowed()&&touchComposition.capture(context,binding))return;
  if(instances!=1||firstInstance||count>1536){reject("draw budget/instancing unsupported");return;}
  unsigned extensions=0;bytes=sizeof(extensions);context->GetPrivateData(kGeometryExtensions,&bytes,&extensions);if(extensions){reject("geometry/tessellation shader active");return;}
  const auto now=GetTickCount64();std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock){++dropped;return;}
  if(pending.size()>=4||ready.size()>=4){++dropped;return;}
  auto job=std::make_unique<Job>();bool found=false;
  for(unsigned i=0;i<16;++i)if(binding&(1u<<i)){ComPtr<ID3D11ShaderResourceView> view;context->PSGetShaderResources(i,1,&view);ComPtr<ID3D11Resource> resource;if(view)view->GetResource(&resource);TouchSurface surface;
   if(touch_surface_read(resource.Get(),surface)){if(found){reason="multiple device textures in draw: ambiguous";return;}job->surface=surface;job->textureSlot=i;found=true;}}
  if(!found)return;if(job->surface.slot>=lastCapture.size())return;
  std::string shaderFailure;
  if(!code(context,kGeometryVS,job->vs,&shaderFailure)){reason="vs."+shaderFailure+" device_slot="+std::to_string(job->surface.slot);failed_locked();return;}
  if(!code(context,kGeometryPS,job->ps,&shaderFailure)){reason="ps."+shaderFailure+" device_slot="+std::to_string(job->surface.slot);failed_locked();return;}
  ComPtr<ID3D11InputLayout> layout;context->IAGetInputLayout(&layout);bytes=sizeof(job->layout);
  if(!layout||FAILED(layout->GetPrivateData(kGeometryLayout,&bytes,&job->layout))||!job->layout.count){reason="input layout missing/unsupported format or instance data";return;}
  D3D11_PRIMITIVE_TOPOLOGY topology;context->IAGetPrimitiveTopology(&topology);if(topology!=D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST||count%3){reason="non-triangle-list draw";return;}
  UINT n=1;D3D11_VIEWPORT viewport{};context->RSGetViewports(&n,&viewport);if(n!=1){reason="viewport missing/multiple";return;}
  job->depth=depth_evidence(context,viewport);
  TouchInput::Request request;const bool requested=touchInput.request(request);const auto slot=job->surface.slot;
  job->gps=touch_gps_bounds(managed_snapshot(slot));const bool integrated=job->gps!=nullptr;
  ComPtr<ID3D11RenderTargetView> target;context->OMGetRenderTargets(1,&target,nullptr);ComPtr<ID3D11Resource> resource;if(target)target->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;
  if(!resource||FAILED(resource.As(&texture))){reason="render target unavailable";return;}D3D11_TEXTURE2D_DESC td{};texture->GetDesc(&td);
  if(!td.Width||!td.Height||viewport.Width<1||viewport.Height<1){reason="invalid render viewport";return;}
  if(integrated&&touchMainTargetWidth&&touchMainTargetHeight&&(td.Width!=touchMainTargetWidth||td.Height!=touchMainTargetHeight||viewport.TopLeftX||viewport.TopLeftY||viewport.Width!=td.Width||viewport.Height!=td.Height)){reason="integrated GPS: non-main-view draw excluded";return;}
  // No shared cluster/auxiliary pass may consume the GPS's cadence lane.
  // Only measured integrated producers use this; phone/accessory acquisition
  // retains its existing path and every copy/depth/generation check.
  if(integrated&&integratedClocks.size()>=128)for(auto it=integratedClocks.begin();it!=integratedClocks.end();)if(now>=it->second.at&&now-it->second.at>250)it=integratedClocks.erase(it);else ++it;
  if(integrated&&integratedClocks.size()>=128){reason="integrated draw cadence budget";return;}
  DrawClock* lane=nullptr;
  if(integrated){
   unsigned positionSlot=32;for(unsigned i=0;i<job->layout.count;++i){const auto& e=job->layout.elements[i];if(!strcmp(e.semantic,"POSITION")&&e.index==0){positionSlot=e.slot;break;}}
   if(positionSlot>=32){reason="measured GPS position binding missing";return;}
   ComPtr<ID3D11Buffer> positions,indices;UINT stride=0,offset=0,indexOffset=0;DXGI_FORMAT format{};
   context->IAGetVertexBuffers(positionSlot,1,&positions,&stride,&offset);if(indexed)context->IAGetIndexBuffer(&indices,&format,&indexOffset);
   lane=&integratedClocks[{slot,count,indexed,uintptr_t(resource.Get()),start,base,uintptr_t(positions.Get()),offset,stride,uintptr_t(indices.Get()),indexOffset}];
  }
  auto& captured=lane?lane->at:lastCapture[slot];auto& capturedTrace=lane?lane->trace:lastCaptureTrace[slot];auto& capturedUsable=lane?lane->usable:lastCaptureUsable[slot];
  if(job->depth->usable){if(capturedUsable&&capturedTrace==(requested?request.trace:0)&&now-captured<66)return;}
  else if(now-captured<(capturedUsable?150:66))return;
  job->viewport={viewport.TopLeftX,viewport.TopLeftY,viewport.Width,viewport.Height,float(td.Width),float(td.Height)};
  job->at=now;job->frame=frame;job->session=touchStatus.sourceGeneration;job->count=count;job->start=start;job->base=base;job->indexed=indexed;ComPtr<ID3D11Device> device;context->GetDevice(&device);size_t total=0;
  if(requested){auto& d=*job->depth;RECT client{};if(settingsWindow&&GetClientRect(settingsWindow,&client)&&client.right>0&&client.bottom>0){d.cursor=request.cursor;d.clientW=client.right;d.clientH=client.bottom;d.trace=request.trace;d.at=now;d.frame=job->frame;d.id=++nextDepthId;d.continuity=missedBoundary.load();if(d.cursor.x<0||d.cursor.y<0||unsigned(d.cursor.x)>=d.clientW||unsigned(d.cursor.y)>=d.clientH)d.trace=0;else {d.x=unsigned((d.cursor.x+.5)*td.Width/d.clientW);d.y=unsigned((d.cursor.y+.5)*td.Height/d.clientH);}}}
  PhoneInternalScope own; // Copies cannot propagate BetterGPS surface tags or recurse into observers.
  if(indexed){ComPtr<ID3D11Buffer> buffer;context->IAGetIndexBuffer(&buffer,&job->indexFormat,&job->indexOffset);if(job->indexFormat!=DXGI_FORMAT_R16_UINT&&job->indexFormat!=DXGI_FORMAT_R32_UINT){reason="index.format-unsupported value="+std::to_string(unsigned(job->indexFormat));failed_locked();return;}const unsigned stride=job->indexFormat==DXGI_FORMAT_R16_UINT?2:4;const uint64_t begin=uint64_t(job->indexOffset)+uint64_t(start)*stride;if(!stage_range(device.Get(),context,buffer.Get(),job->indices,total,begin,uint64_t(count)*stride,reason,"index")){failed_locked();return;}}
  for(unsigned i=0;i<job->layout.count;++i){const auto slot=job->layout.elements[i].slot;auto& buffer=job->vertices[slot];if(buffer.staging)continue;ComPtr<ID3D11Buffer> native;context->IAGetVertexBuffers(slot,1,&native,&buffer.stride,&buffer.offset);if(!buffer.stride){reason="vertex.stride-zero slot="+std::to_string(slot);return;}D3D11_BUFFER_DESC d{};if(native)native->GetDesc(&d);
   // R16 bounds are guaranteed by the index format, not guessed from index count.
   // R32 keeps the full possible domain within the actual bound VB. If too large,
   // fail explicitly rather than reading indices from a different frame.
   const int64_t low=indexed?std::max<int64_t>(0,base):int64_t(start);
   const int64_t high=indexed?int64_t(base)+(job->indexFormat==DXGI_FORMAT_R16_UINT?65535ll:4294967295ll):int64_t(start)+count-1;
   const uint64_t begin=uint64_t(buffer.offset)+uint64_t(low)*buffer.stride;
   const uint64_t end=high>=low?std::min<uint64_t>(d.ByteWidth,uint64_t(buffer.offset)+(uint64_t(high)+1)*buffer.stride):begin;
   if(!stage_range(device.Get(),context,native.Get(),buffer,total,begin,end>begin?end-begin:0,reason,"vertex",slot)){failed_locked();return;}
  }
  ComPtr<ID3D11DeviceContext1> context1;context->QueryInterface(IID_PPV_ARGS(&context1));
  for(unsigned i=0;i<14;++i){ComPtr<ID3D11Buffer> native;UINT first=0,count=4096;if(context1)context1->VSGetConstantBuffers1(i,1,&native,&first,&count);else context->VSGetConstantBuffers(i,1,&native);job->constants[i].offset=first*16;job->constants[i].stride=count*16;if(native){D3D11_BUFFER_DESC d{};native->GetDesc(&d);if(!stage_range(device.Get(),context,native.Get(),job->constants[i],total,0,d.ByteWidth,reason,"constant",i))return;}}
  for(unsigned i=0;i<16;++i){ComPtr<ID3D11SamplerState> sampler;context->PSGetSamplers(i,1,&sampler);if(sampler){sampler->GetDesc(&job->samplers[i]);job->hasSampler[i]=true;}}
  D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};const auto queryHr=device->CreateQuery(&query,&job->completed);if(FAILED(queryHr)){char message[96];snprintf(message,sizeof(message),"query.create hr=0x%08lx",static_cast<unsigned long>(queryHr));reason=message;return;}context->End(job->completed.Get());captured=now;capturedTrace=requested?request.trace:0;capturedUsable=job->depth->usable;if(job->depth->trace&&job->depth->usable)watchDepth=true;pending.push_back(std::move(job));reason="native geometry readback queued (no replay)";
 }
 void poll(ID3D11DeviceContext* context){
  touchComposition.poll(context);
  ++frame;if(!touch_active()){std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(lock){pending.clear();ready.clear();reason="interaction mode inactive";}return;}
  std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock)return;PhoneInternalScope own;
  for(auto& job:pending)if(job->depth){auto& d=*job->depth;if(d.continuity!=missedBoundary.load()&&d.trace&&!d.sealed){d.usable=false;d.reason="Depth lifetime boundary missed: snapshot rejected";}else touch_depth_seal(context,d,"end-native-frame");}
  watchDepth=false;
  for(auto it=pending.begin();it!=pending.end();){auto& job=**it;BOOL done=FALSE;const auto hr=context->GetData(job.completed.Get(),&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
   if(FAILED(hr)||GetTickCount64()-job.at>200){char detail[160];snprintf(detail,sizeof(detail),"query.%s hr=0x%08lx age=%llu",FAILED(hr)?"get-data-failed":"expired",static_cast<unsigned long>(hr),GetTickCount64()-job.at);reason=detail;it=pending.erase(it);continue;}if(hr!=S_OK||!done){++it;continue;}
   if(job.depth&&!touch_depth_poll(context,*job.depth)){++it;continue;}
   bool mapped=true,fatal=false;auto read=[&](Buffer& b,const char* role,unsigned slot){if(!b.staging)return;D3D11_MAPPED_SUBRESOURCE map{};const auto mapHr=context->Map(b.staging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map);if(SUCCEEDED(mapHr)){b.data.assign((const uint8_t*)map.pData,(const uint8_t*)map.pData+b.bytes);context->Unmap(b.staging.Get(),0);b.staging.Reset();}else {mapped=false;if(mapHr!=DXGI_ERROR_WAS_STILL_DRAWING){fatal=true;char detail[160];snprintf(detail,sizeof(detail),"%s.map-failed slot=%u range=%u+%u hr=0x%08lx",role,slot,b.sourceBegin,b.bytes,static_cast<unsigned long>(mapHr));reason=detail;}}};
   read(job.indices,"index",0);for(unsigned i=0;i<job.vertices.size();++i)read(job.vertices[i],"vertex",i);for(unsigned i=0;i<job.constants.size();++i)read(job.constants[i],"constant",i);
   if(fatal){it=pending.erase(it);continue;}
   if(!mapped){++it;continue;}if(ready.size()<4)ready.push_back(std::move(*it));else ++dropped;it=pending.erase(it);
  }
 }
 static float half(uint16_t value){const unsigned sign=value>>15,exponent=(value>>10)&31,mantissa=value&1023;float result=exponent==0?std::ldexp(float(mantissa),-24):exponent==31?std::numeric_limits<float>::infinity():std::ldexp(float(1024+mantissa),int(exponent)-25);return sign?-result:result;}
 static touch_geometry::Register decode(const Buffer& buffer,const GeometryElement& element,unsigned vertex){
  using namespace touch_geometry;Register result;const uint64_t at=uint64_t(buffer.offset)+uint64_t(vertex)*buffer.stride+element.offset;const auto bytes=geometry_format_bytes(element.format);if(at<buffer.sourceBegin||at-buffer.sourceBegin+bytes>buffer.data.size())return result;
  const auto* p=buffer.data.data()+at-buffer.sourceBegin;result.value[3]=1;result.valid=15;unsigned components=4;
  if(element.format==DXGI_FORMAT_R32G32B32A32_FLOAT||element.format==DXGI_FORMAT_R32G32B32_FLOAT||element.format==DXGI_FORMAT_R32G32_FLOAT||element.format==DXGI_FORMAT_R32_FLOAT){components=bytes/4;memcpy(result.value.x,p,bytes);}
  else if(element.format==DXGI_FORMAT_R16G16_FLOAT||element.format==DXGI_FORMAT_R16G16B16A16_FLOAT){components=bytes/2;for(unsigned i=0;i<components;++i){uint16_t n;memcpy(&n,p+i*2,2);result.value[i]=half(n);}}
  else if(element.format==DXGI_FORMAT_R16G16_SNORM||element.format==DXGI_FORMAT_R16G16B16A16_SNORM){components=bytes/2;for(unsigned i=0;i<components;++i){int16_t n;memcpy(&n,p+i*2,2);result.value[i]=std::max(-1.f,float(n)/32767);}}
  else if(element.format==DXGI_FORMAT_R16G16_UNORM||element.format==DXGI_FORMAT_R16G16B16A16_UNORM){components=bytes/2;for(unsigned i=0;i<components;++i){uint16_t n;memcpy(&n,p+i*2,2);result.value[i]=float(n)/65535;}}
  else if(element.format==DXGI_FORMAT_R8G8B8A8_UNORM)for(unsigned i=0;i<4;++i)result.value[i]=p[i]/255.f;
  else if(element.format==DXGI_FORMAT_R8G8B8A8_SNORM)for(unsigned i=0;i<4;++i)result.value[i]=std::max(-1.f,float(int8_t(p[i]))/127);
  else if(element.format==DXGI_FORMAT_R10G10B10A2_UNORM){uint32_t n;memcpy(&n,p,4);for(unsigned i=0;i<3;++i)result.value[i]=float((n>>(i*10))&1023)/1023;result.value[3]=float(n>>30)/3;}
  else result.valid=0;return result;
 }
 static bool disassemble(const std::vector<uint8_t>& bytes,std::string& text,ComPtr<ID3D11ShaderReflection>& reflection){
  ComPtr<ID3DBlob> blob;if(FAILED(D3DReflect(bytes.data(),bytes.size(),IID_ID3D11ShaderReflection,(void**)&reflection))||FAILED(D3DDisassemble(bytes.data(),bytes.size(),0,nullptr,&blob)))return false;text.assign((const char*)blob->GetBufferPointer(),blob->GetBufferSize());return true;
 }
 static bool evaluate(const Job& job,TouchGeometryMesh& mesh,std::string& why,std::string& vsText,std::string& psText){
  using namespace touch_geometry;ComPtr<ID3D11ShaderReflection> vs,ps;if(!disassemble(job.vs,vsText,vs)||!disassemble(job.ps,psText,ps)){why="shader reflection/disassembly failed";return false;}
  // Establish the actual sampled VS output; never assume TEXCOORD0 or modulo UVs.
  unsigned inputReg=~0u,samplerSlot=~0u,samples=0;char components[8]{};std::istringstream lines(psText);std::string line;
  while(std::getline(lines,line)){line=trim(line);if(line.rfind("sample",0)!=0)continue;auto args=split_args(line.substr(line.find(' ')+1));if(args.size()<4)continue;unsigned t=~0u;int used=0;if(sscanf(args[2].c_str(),"t%u%n",&t,&used)!=1||t!=job.textureSlot)continue;++samples;
   if(sscanf(args[1].c_str(),"v%u.%7[xyzw]",&inputReg,components)!=2||strlen(components)<2||sscanf(args[3].c_str(),"s%u",&samplerSlot)!=1){why="calculated pixel UV unsupported";return false;}}
  if(samples!=1||samplerSlot>=16||!job.hasSampler[samplerSlot]){why="sample/sampler mapping missing or ambiguous";return false;}
  D3D11_SHADER_DESC pd{},vd{};ps->GetDesc(&pd);vs->GetDesc(&vd);std::string semantic;unsigned semanticIndex=0;
  for(unsigned i=0;i<pd.InputParameters;++i){D3D11_SIGNATURE_PARAMETER_DESC p{};ps->GetInputParameterDesc(i,&p);if(p.Register==inputReg){semantic=p.SemanticName;semanticIndex=p.SemanticIndex;}}
  unsigned positionReg=~0u,uvReg=~0u,uvMask=0,psMask=0;
  for(unsigned i=0;i<pd.InputParameters;++i){D3D11_SIGNATURE_PARAMETER_DESC p{};ps->GetInputParameterDesc(i,&p);if(p.SemanticName&&semantic==p.SemanticName&&p.SemanticIndex==semanticIndex)psMask=p.Mask;}
  for(unsigned i=0;i<vd.OutputParameters;++i){D3D11_SIGNATURE_PARAMETER_DESC p{};vs->GetOutputParameterDesc(i,&p);if(p.SystemValueType==D3D_NAME_POSITION)positionReg=p.Register;if(p.SemanticName&&semantic==p.SemanticName&&p.SemanticIndex==semanticIndex){uvReg=p.Register;uvMask=p.Mask;}}
  if(positionReg>=32||uvReg>=32){why="position or sampled UV output unavailable";return false;}
  if(uvMask!=psMask){why="packed VS/PS semantic remapping unsupported";return false;}
  // No noperspective/flat UV path is silently evaluated as perspective-correct.
  const std::string declaration=" v"+std::to_string(inputReg)+".";lines.clear();lines.str(psText);while(std::getline(lines,line))if(line.find("dcl_input_ps")!=line.npos&&line.find(declaration)!=line.npos&&(line.find("noperspective")!=line.npos||line.find("constant")!=line.npos)){why="non-perspective UV interpolation unsupported";return false;}
  Program program;if(!program.parse(vsText)){why=program.failure;return false;}std::array<std::vector<V4>,14> constants;
  for(unsigned i=0;i<14;++i){const auto& buffer=job.constants[i];if(buffer.offset>buffer.data.size())continue;const auto bytes=std::min<size_t>(buffer.data.size()-buffer.offset,buffer.stride);constants[i].resize(bytes/16);if(!constants[i].empty())memcpy(constants[i].data(),buffer.data.data()+buffer.offset,constants[i].size()*16);}
  const bool integrated=job.gps!=nullptr;
  std::map<unsigned,Vertex> cache;std::map<unsigned,bool> identities;std::vector<Vertex> vertices;std::vector<bool> physical;vertices.reserve(job.count);
  const auto component=[](char c)->unsigned {const auto p=std::string("xyzw").find(c);return p==std::string::npos?4:unsigned(p);};const auto ux=component(components[0]),uy=component(components[1]);if(ux>=4||uy>=4)return false;
  for(unsigned i=0;i<job.count;++i){int64_t index=int64_t(job.start)+i;
   if(job.indexed){uint32_t raw=0;if(!index_at(job,i,raw)){why="index buffer bounds";return false;}index=int64_t(raw)+job.base;}
   if(index<0||index>UINT_MAX){why="vertex index bounds";return false;}const auto found=cache.find(unsigned(index));if(found!=cache.end()){vertices.push_back(found->second);physical.push_back(identities[unsigned(index)]);continue;}
   if(cache.size()>=512){why="unique vertex budget";return false;}std::array<Register,32> inputs{},outputs{};
   bool accepted=!integrated;
   for(unsigned n=0;n<vd.InputParameters;++n){D3D11_SIGNATURE_PARAMETER_DESC p{};vs->GetInputParameterDesc(n,&p);if(p.Register>=32||p.SystemValueType!=D3D_NAME_UNDEFINED){why="system vertex input unsupported";return false;}for(unsigned e=0;e<job.layout.count;++e){const auto& element=job.layout.elements[e];if(p.SemanticName&&!strcmp(p.SemanticName,element.semantic)&&p.SemanticIndex==element.index){inputs[p.Register]=decode(job.vertices[element.slot],element,unsigned(index));if(integrated&&!strcmp(p.SemanticName,"POSITION")&&p.SemanticIndex==0)accepted=job.gps->contains(inputs[p.Register]);}}}
   if(!program.run(inputs,constants,outputs,why))return false;
   if(outputs[positionReg].valid!=15||(outputs[uvReg].valid&((1u<<ux)|(1u<<uy)))!=((1u<<ux)|(1u<<uy))){why="position/UV unresolved: "+why;return false;}
   Vertex value{outputs[positionReg].value,outputs[uvReg].value[ux],outputs[uvReg].value[uy]};cache.emplace(unsigned(index),value);identities.emplace(unsigned(index),accepted);vertices.push_back(value);physical.push_back(accepted);
  }
  mesh.slot=job.surface.slot;mesh.at=job.at;mesh.frame=job.frame;mesh.session=job.session;mesh.epoch=job.surface.epoch;mesh.power=job.surface.power;mesh.page=job.surface.page;mesh.viewport=job.viewport;mesh.addressU=job.samplers[samplerSlot].AddressU;mesh.addressV=job.samplers[samplerSlot].AddressV;mesh.left=job.surface.left;mesh.top=job.surface.top;mesh.width=job.surface.width;mesh.height=job.surface.height;
  mesh.depth=job.depth;mesh.proof=job.surface.proof;
  for(unsigned i=0;i<pd.OutputParameters;++i){D3D11_SIGNATURE_PARAMETER_DESC output{};ps->GetOutputParameterDesc(i,&output);if(output.SystemValueType==D3D_NAME_DEPTH||output.SystemValueType==D3D_NAME_DEPTH_GREATER_EQUAL||output.SystemValueType==D3D_NAME_DEPTH_LESS_EQUAL||output.SystemValueType==D3D_NAME_COVERAGE)mesh.depth.reset();}
  for(unsigned i=0;i+2<vertices.size();i+=3){if(!physical[i]||!physical[i+1]||!physical[i+2])continue;Triangle triangle{{vertices[i],vertices[i+1],vertices[i+2]}};Point p;bool valid=true;for(const auto& v:triangle.vertices)valid=valid&&project(v,mesh.viewport,mesh.viewport.targetW,mesh.viewport.targetH,p);if(valid)mesh.triangles.push_back(triangle);}
  if(mesh.triangles.empty()){why=integrated?"model-surface rejected: not measured central GPS (cluster/shared texture)":"all triangles clipped/invalid";return false;}why="CPU projection candidate; target mapping/occlusion UNVERIFIED";return true;
 }
 static void save_inputs(const Job& job,const std::filesystem::path& file){
  // Compact reproducible input evidence, not whole multi-megabyte shared VBs.
  // Format BGPGEOM1: native Windows little-endian PODs; versioned private debug data.
  std::ofstream output(file,std::ios::binary);if(!output)return;
  auto write=[&](const auto& value){output.write((const char*)&value,sizeof(value));};output.write("BGPGEOM1",8);write(job.surface);write(job.viewport);write(job.layout);
  for(unsigned i=0;i<14;++i){const auto& buffer=job.constants[i];const uint32_t count=buffer.offset<=buffer.data.size()?uint32_t(std::min<size_t>(buffer.data.size()-buffer.offset,buffer.stride)):0;write(count);if(count)output.write((const char*)buffer.data.data()+buffer.offset,count);}
  std::vector<uint32_t> indices;
  for(unsigned i=0;i<job.count;++i){int64_t vertex=int64_t(job.start)+i;if(job.indexed){uint32_t n=0;if(!index_at(job,i,n))continue;vertex=int64_t(n)+job.base;}if(vertex>=0&&vertex<=UINT_MAX&&std::find(indices.begin(),indices.end(),uint32_t(vertex))==indices.end()&&indices.size()<512)indices.push_back(uint32_t(vertex));}
  const uint32_t count=uint32_t(indices.size());write(count);for(auto vertex:indices){write(vertex);for(unsigned i=0;i<job.layout.count;++i){const auto& element=job.layout.elements[i];const auto value=decode(job.vertices[element.slot],element,vertex);write(value);}}
 }
 void worker(const std::filesystem::path& base,uint64_t now){
  touchCopyEvidence.flush(base);
  std::deque<std::unique_ptr<Job>> jobs;std::string current,failureSummary;{std::lock_guard<std::mutex> lock(mutex);jobs.swap(ready);current=reason;for(const auto& entry:failures){if(failureSummary.size()>220)break;failureSummary+=entry.first+":"+std::to_string(entry.second)+" ";}}
  if(!touch_active()){meshes.clear();touchGeometryView.publish({{},"Interaction mode inactive",observed.load(),completed,dropped.load()});return;}
  for(auto& job:jobs){if(job->session!=touchStatus.sourceGeneration.load())continue;TouchGeometryMesh mesh;std::string vs,ps,why;bool good=evaluate(*job,mesh,why,vs,ps);current=why;++completed;
   if(!lastAcquisitionAt||now-lastAcquisitionAt>=1000){lastAcquisitionAt=now;size_t copied=job->indices.bytes;for(const auto& b:job->vertices)copied+=b.bytes;for(const auto& b:job->constants)copied+=b.bytes;char detail[256];snprintf(detail,sizeof(detail),"slot=%u frame=%llu indices_native=%u indices_range=%u+%u copied_total=%zu result=%s",job->surface.slot,job->frame,job->indices.nativeBytes,job->indices.sourceBegin,job->indices.bytes,copied,why.c_str());touchStatus.diagnostic_event(job->session,"geometry-acquisition",detail);}
   const auto previous=std::find_if(meshes.begin(),meshes.end(),[&](const auto& old){return old.slot==job->surface.slot;});
   const bool keep=previous!=meshes.end()&&previous->depth&&previous->depth->usable&&now>=previous->at&&now-previous->at<=150&&(!good||!mesh.depth||!mesh.depth->usable);
   if(!keep){meshes.erase(std::remove_if(meshes.begin(),meshes.end(),[&](const auto& old){return old.slot==job->surface.slot;}),meshes.end());if(good)meshes.push_back(std::move(mesh));}
   // Bounded evidence, only on worker: shader text + actual draw declarations.
   uint64_t hash=14695981039346656037ull;for(auto b:job->vs){hash^=b;hash*=1099511628211ull;}for(auto b:job->ps){hash^=b;hash*=1099511628211ull;}
   // Same shaders do not imply the same physical display. Retain separate
   // evidence for the rejected cluster and accepted central GPS, within the
   // existing eight-report budget, so a future capture remains diagnosable.
   if(job->gps){hash^=job->count;hash*=1099511628211ull;hash^=unsigned(good);hash*=1099511628211ull;}
   if(savedEvidence.size()<8&&!savedEvidence.count(hash)){const auto root=base/"logs/touch";std::error_code ec;std::filesystem::create_directories(root,ec);auto path=root/("geometry-"+std::to_string(hash)+".txt");std::ofstream file(path);if(file){file<<"diagnostic_only=1 actions=disabled occlusion=unverified\nslot="<<job->surface.slot<<" tick="<<job->at<<" frame="<<job->frame<<" count="<<job->count<<" start="<<job->start<<" base="<<job->base<<" indexed="<<job->indexed<<" t="<<job->textureSlot<<"\nresult="<<why<<"\nviewport="<<job->viewport.x<<','<<job->viewport.y<<','<<job->viewport.w<<','<<job->viewport.h<<" target="<<job->viewport.targetW<<'x'<<job->viewport.targetH<<'\n';for(unsigned i=0;i<job->layout.count;++i){const auto& e=job->layout.elements[i];file<<e.semantic<<e.index<<" slot="<<e.slot<<" offset="<<e.offset<<" format="<<unsigned(e.format)<<" stride="<<job->vertices[e.slot].stride<<'\n';}file<<"\nVERTEX SHADER\n"<<vs<<"\nPIXEL SHADER\n"<<ps;savedEvidence[hash]=path.filename().string();auto inputs=path;inputs.replace_extension(".inputs");save_inputs(*job,inputs);touchStatus.note(touchDiagnostics.current.load(),"geometry-evidence",path.filename().string().c_str());}}
  }
  meshes.erase(std::remove_if(meshes.begin(),meshes.end(),[&](const auto& mesh){return now<mesh.at||now-mesh.at>150||mesh.epoch!=vehicleStart.epoch.load()||mesh.session!=touchStatus.sourceGeneration.load();}),meshes.end());
  if(meshes.empty()&&current.rfind("CPU projection candidate",0)==0)current="No fresh projection: native sample expired";
  if(now-lastPublish>=30||!jobs.empty()){touchGeometryView.publish({meshes,current,observed.load(),completed,dropped.load()});lastPublish=now;}
  const auto trace=touchDiagnostics.current.load(),session=touchStatus.sourceGeneration.load();if(now-lastLogAt>=1000||trace!=lastLogTrace||session!=lastLogSession){lastLog=current;lastLogTrace=trace;lastLogAt=now;lastLogSession=session;char detail[640];snprintf(detail,sizeof(detail),"observed=%u completed=%u meshes=%zu dropped=%u reason=%s failures=[%s]",observed.load(),completed,meshes.size(),dropped.load(),current.c_str(),failureSummary.c_str());touchStatus.diagnostic_event(session,"geometry",detail);}
 }
 void stop(){std::lock_guard<std::mutex> lock(mutex);pending.clear();ready.clear();meshes.clear();lastCapture={};lastCaptureTrace={};lastCaptureUsable={};integratedClocks.clear();watchDepth=false;touchGeometryView.publish({});}
};
inline TouchGeometryCapture touchGeometry;
}
