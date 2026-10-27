#pragma once
#include "TouchInput.h"
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <map>
#include <sstream>
#include "TouchTransfer.h"
namespace bettergps_overlay {
// A private ID/UV pass reuses the exact game's vertex pipeline, viewport and
// depth/stencil tests. Only a one-pixel scissor is rasterized. The native draw
// always happens first and none of its targets/depth values are overwritten.
inline const GUID kTouchShader={0xf67e51a0,0xe4f7,0x4915,{0xad,0x47,0x71,0xef,0x36,0x0e,0x28,0xd5}};
inline const GUID kTouchBindings={0x7bb4f643,0xf1d1,0x4617,{0x9b,0x02,0x5e,0x61,0x80,0x19,0x38,0x87}};
struct TouchShaderBinding {char semantic[32]{};unsigned index=0,components=0;char xy[3]{};};
struct TouchShaderInput {char semantic[32]{};unsigned index=0,reg=0,mask=0,type=0,system=0,interpolation=0;};
struct TouchShaderData {
 std::array<TouchShaderBinding,16> textures;uint64_t hash=14695981039346656037ull;
 std::array<TouchShaderInput,32> signature{};
 unsigned byteSize=0,inputs=0,samples=0,direct=0,calculated=0,unmatched=0,evidence=0;
 HRESULT reflection=E_PENDING,disassembly=E_PENDING;
};
inline std::atomic<unsigned> touchObservedShaderCount{0};
inline void touch_shader_metadata(ID3D11PixelShader* shader,const void* bytes,size_t size){
 if(!shader||phoneInternalWork)return;
 ++touchObservedShaderCount;
 ID3D11ShaderReflection* reflection=nullptr;ID3DBlob* assembly=nullptr;TouchShaderData data{};data.byteSize=unsigned(std::min<size_t>(size,UINT_MAX));
 for(size_t i=0;i<size;++i){data.hash^=static_cast<const uint8_t*>(bytes)[i];data.hash*=1099511628211ull;}
 data.reflection=D3DReflect(bytes,size,IID_ID3D11ShaderReflection,(void**)&reflection);
 if(FAILED(data.reflection)){shader->SetPrivateData(kTouchShader,sizeof(data),&data);return;}
 data.disassembly=D3DDisassemble(bytes,size,0,nullptr,&assembly);
 if(FAILED(data.disassembly)){shader->SetPrivateData(kTouchShader,sizeof(data),&data);reflection->Release();return;}
 D3D11_SHADER_DESC description{};reflection->GetDesc(&description);data.inputs=description.InputParameters;
 for(unsigned i=0;i<std::min<unsigned>(data.inputs,data.signature.size());++i){D3D11_SIGNATURE_PARAMETER_DESC p{};if(FAILED(reflection->GetInputParameterDesc(i,&p))||!p.SemanticName||strlen(p.SemanticName)>=32)continue;auto& field=data.signature[i];strcpy_s(field.semantic,p.SemanticName);field.index=p.SemanticIndex;field.reg=p.Register;field.mask=p.Mask;field.type=p.ComponentType;field.system=p.SystemValueType;}
 const auto evidenceSize=std::min<size_t>(assembly->GetBufferSize(),65536);const auto reserved=touchEvidenceBytes.fetch_add(evidenceSize);
 if(reserved+evidenceSize<=16*1024*1024){if(SUCCEEDED(shader->SetPrivateData(kTouchEvidence,UINT(evidenceSize),assembly->GetBufferPointer())))data.evidence=unsigned(evidenceSize);}
 else touchEvidenceBytes.fetch_sub(evidenceSize);
 std::istringstream lines(std::string((const char*)assembly->GetBufferPointer(),assembly->GetBufferSize()));std::string line;
 while(std::getline(lines,line)){
  // Preserve interpolation as well as the complete signature. Keeping only
  // the sampled TEXCOORD silently relocates it to another hardware register.
  if(line.find("dcl_input_ps")!=std::string::npos){const auto at=line.find(" v");unsigned reg=~0u;if(at!=std::string::npos&&sscanf_s(line.c_str()+at," v%u",&reg)==1)for(auto& field:data.signature)if(field.semantic[0]&&field.reg==reg)field.interpolation=(line.find("constant")!=std::string::npos?1u:0u)|(line.find("noperspective")!=std::string::npos?2u:0u)|(line.find("centroid")!=std::string::npos?4u:0u)|(line.find("sample")!=std::string::npos?8u:0u);}
  const auto start=line.find_first_not_of(" \t");if(start==std::string::npos||line.compare(start,6,"sample"))continue;
  ++data.samples;
  // Fail closed for calculated/indirect coordinates. Never guess TEXCOORD0.
  const auto coordinate=line.find(", v",start),texture=coordinate==std::string::npos?std::string::npos:line.find(", t",coordinate+1);
  if(coordinate==std::string::npos||texture==std::string::npos){++data.calculated;continue;}
  unsigned input=0,slot=0;char swizzle[8]{};
  if(sscanf_s(line.c_str()+coordinate,", v%u.%7[xyzw]",&input,swizzle,unsigned(sizeof(swizzle)))!=2||sscanf_s(line.c_str()+texture,", t%u",&slot)!=1||slot>=16||strlen(swizzle)<2){++data.unmatched;continue;}
  for(unsigned i=0;i<description.InputParameters;++i){D3D11_SIGNATURE_PARAMETER_DESC p{};if(FAILED(reflection->GetInputParameterDesc(i,&p))||p.Register!=input||p.SystemValueType!=D3D_NAME_UNDEFINED||p.ComponentType!=D3D_REGISTER_COMPONENT_FLOAT32||!p.SemanticName||strlen(p.SemanticName)>=32)continue;
   auto& target=data.textures[slot];strcpy_s(target.semantic,p.SemanticName);target.index=p.SemanticIndex;target.components=p.Mask&8?4:p.Mask&4?3:p.Mask&2?2:1;target.xy[0]=swizzle[0];target.xy[1]=swizzle[1];
   ++data.direct;
  }
 }
 shader->SetPrivateData(kTouchShader,sizeof(data),&data);assembly->Release();reflection->Release();
}
inline void touch_note_bindings(ID3D11DeviceContext* context,UINT first,UINT count,ID3D11ShaderResourceView* const* views){
 if(!touch_active()||first>=16)return;unsigned mask=0;UINT bytes=sizeof(mask);context->GetPrivateData(kTouchBindings,&bytes,&mask);
 for(UINT i=0;i<count&&first+i<16;++i){mask&=~(1u<<(first+i));if(!views||!views[i])continue;ID3D11Resource* resource=nullptr;views[i]->GetResource(&resource);TouchSurface surface;if(touch_surface_read(resource,surface))mask|=1u<<(first+i);if(resource)resource->Release();}
 context->SetPrivateData(kTouchBindings,sizeof(mask),&mask);
 if(mask)touchDiagnostics.note(TouchStage::Binding,"mask=0x%x context=%s",mask,context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED?"deferred":"immediate");
}
inline void touch_pass_description(ID3D11DeviceContext* context,char* message,size_t capacity){
 ID3D11RenderTargetView* target=nullptr;ID3D11DepthStencilView* depth=nullptr;context->OMGetRenderTargets(1,&target,&depth);D3D11_TEXTURE2D_DESC colorDesc{},depthDesc{};
 const auto describe=[](ID3D11View* view,D3D11_TEXTURE2D_DESC& desc){if(!view)return;ID3D11Resource* resource=nullptr;view->GetResource(&resource);ID3D11Texture2D* texture=nullptr;if(resource)resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture);if(texture){texture->GetDesc(&desc);texture->Release();}if(resource)resource->Release();};describe(target,colorDesc);describe(depth,depthDesc);
 ID3D11DepthStencilState* state=nullptr;UINT reference=0;context->OMGetDepthStencilState(&state,&reference);D3D11_DEPTH_STENCIL_DESC d{};if(state)state->GetDesc(&d);else d.DepthEnable=TRUE;
 snprintf(message,capacity,"target=%ux%u color_format=%u depth=%ux%u depth_format=%u depth_enabled=%u samples=%u",colorDesc.Width,colorDesc.Height,unsigned(colorDesc.Format),depthDesc.Width,depthDesc.Height,unsigned(depthDesc.Format),unsigned(d.DepthEnable),colorDesc.SampleDesc.Count);
 if(target)target->Release();if(depth)depth->Release();if(state)state->Release();
}
struct TouchPicker {
 struct Readback {ID3D11Texture2D* texture=nullptr;bool pending=false;POINT cursor{};uint64_t time=0,trace=0;unsigned token=0;std::array<TouchSurface,managedDeviceCount> surfaces;};
 struct Target {ID3D11Texture2D* texture=nullptr;ID3D11RenderTargetView* view=nullptr;ID3D11ShaderResourceView* source=nullptr;ID3D11Resource* depthNative=nullptr;ID3D11Texture2D *depthCopy=nullptr,*filtered=nullptr;ID3D11ShaderResourceView* depthView=nullptr;ID3D11RenderTargetView* filteredView=nullptr;float mode=1,epsilon=.000001f;UINT width=0,height=0;bool used=false;POINT cursor{};UINT x=0,y=0;std::array<TouchSurface,managedDeviceCount> surfaces;std::array<Readback,3> reads;};
 std::mutex mutex;ID3D11Device* device=nullptr;ID3D11Buffer *constants=nullptr,*filterConstants=nullptr;ID3D11DeviceContext* filterContext=nullptr;ID3D11VertexShader* filterVertex=nullptr;ID3D11PixelShader* filterPixel=nullptr;
 std::map<std::string,ID3D11DepthStencilState*> depths;std::map<std::string,ID3D11RasterizerState*> rasters;
 std::map<std::string,ID3D11PixelShader*> shaders;std::vector<Target> targets;
 unsigned token=1;std::atomic<unsigned> draws{0},hits{0},unsupported{0};
 template<class T>static void drop(T*& pointer){if(pointer){pointer->Release();pointer=nullptr;}}
 void release(){for(auto& t:targets){for(auto& read:t.reads)drop(read.texture);drop(t.filteredView);drop(t.filtered);drop(t.depthView);drop(t.depthCopy);drop(t.depthNative);drop(t.source);drop(t.view);drop(t.texture);}targets.clear();for(auto& s:shaders)drop(s.second);shaders.clear();for(auto& s:depths)drop(s.second);depths.clear();for(auto& s:rasters)drop(s.second);rasters.clear();drop(constants);drop(filterConstants);drop(filterPixel);drop(filterVertex);drop(filterContext);drop(device);}
 void stop(){std::lock_guard<std::mutex> lock(mutex);release();touchTransfer.stop();}
 bool start(ID3D11Device* next){
  if(device==next&&constants&&filterContext&&filterPixel&&filterVertex&&filterConstants)return true;release();device=next;device->AddRef();D3D11_BUFFER_DESC d{};d.ByteWidth=16;d.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
  auto initResult=device->CreateBuffer(&d,nullptr,&constants);if(FAILED(initResult)){touchDiagnostics.note(TouchStage::GpuInit,"operation=pick-constants hr=0x%08lx",static_cast<unsigned long>(initResult));return false;}
  initResult=device->CreateBuffer(&d,nullptr,&filterConstants);if(FAILED(initResult)){touchDiagnostics.note(TouchStage::GpuInit,"operation=filter-constants hr=0x%08lx",static_cast<unsigned long>(initResult));return false;}
  initResult=device->CreateDeferredContext(0,&filterContext);if(FAILED(initResult)){touchDiagnostics.note(TouchStage::GpuInit,"operation=filter-context hr=0x%08lx",static_cast<unsigned long>(initResult));return false;}
  const char* hlsl="Texture2D<float4> pick:register(t0);Texture2D<float> depth:register(t1);cbuffer Params:register(b0){float4 p;}float4 vertex(uint n:SV_VertexID):SV_Position{float2 uv=float2((n<<1)&2,n&2);return float4(uv*float2(2,-2)+float2(-1,1),0,1);}float4 pixel(float4 pos:SV_Position):SV_Target{int3 at=int3(p.xy,0);float4 hit=pick.Load(at);float z=depth.Load(at);bool visible=p.z==0?abs(hit.w-z)<=p.w:(hit.w-z)*p.z<=p.w;return visible?hit:float4(0,0,0,0);}";
  ID3DBlob *code=nullptr,*error=nullptr;bool ok=false;
  if(SUCCEEDED(D3DCompile(hlsl,strlen(hlsl),nullptr,nullptr,nullptr,"vertex","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error)))ok=SUCCEEDED(device->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&filterVertex));drop(code);drop(error);if(!ok)return false;
  if(SUCCEEDED(D3DCompile(hlsl,strlen(hlsl),nullptr,nullptr,nullptr,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error)))ok=SUCCEEDED(device->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&filterPixel));else ok=false;drop(code);drop(error);if(filterPixel){const unsigned own=1;filterPixel->SetPrivateData(kTouchInternalShader,sizeof(own),&own);}return ok;
 }
 ID3D11PixelShader* shader(const TouchShaderData& native,const TouchShaderBinding& binding){
  if(!binding.semantic[0]||binding.components<2)return nullptr;
  const std::string key=std::to_string(native.hash)+binding.semantic+std::to_string(binding.index)+binding.xy;auto found=shaders.find(key);if(found!=shaders.end()){if(!found->second)touchDiagnostics.note(TouchStage::PickShader,"reason=cached-compile-failure semantic=%s%u",binding.semantic,binding.index);return found->second;}if(shaders.size()>=32){touchDiagnostics.note(TouchStage::PickShader,"reason=shader-cache-cap");return nullptr;}
  if(!native.inputs||native.inputs>native.signature.size())return nullptr;
  int uv=-1,position=-1;std::ostringstream hlsl;hlsl<<"cbuffer Pick : register(b0) {float4 id;} struct Input {";
  for(unsigned i=0;i<native.inputs;++i){const auto& f=native.signature[i];const unsigned n=f.mask&8?4:f.mask&4?3:f.mask&2?2:1;if(!f.semantic[0]||f.mask!=((1u<<n)-1))return nullptr;
   if(f.interpolation&1)hlsl<<"nointerpolation ";else if(f.system!=D3D_NAME_POSITION){if(f.interpolation&2)hlsl<<"noperspective ";if(f.interpolation&4)hlsl<<"centroid ";if(f.interpolation&8)hlsl<<"sample ";}
   const char* type=f.type==D3D_REGISTER_COMPONENT_FLOAT32?"float":f.type==D3D_REGISTER_COMPONENT_UINT32?"uint":f.type==D3D_REGISTER_COMPONENT_SINT32?"int":nullptr;if(!type)return nullptr;
   hlsl<<type<<n<<" f"<<i<<":"<<f.semantic<<f.index<<";";if(f.system==D3D_NAME_POSITION)position=int(i);if(!strcmp(f.semantic,binding.semantic)&&f.index==binding.index)uv=int(i);
  }
  if(uv<0||position<0)return nullptr;
  hlsl<<"};float4 main(Input i):SV_Target{return float4(i.f"<<uv<<"."<<binding.xy<<",id.x,i.f"<<position<<".z);}";
  const auto source=hlsl.str();ID3DBlob *code=nullptr,*error=nullptr;ID3D11PixelShader* pixel=nullptr;
  if(SUCCEEDED(D3DCompile(source.data(),source.size(),nullptr,nullptr,nullptr,"main","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error))){
   ID3D11ShaderReflection* reflection=nullptr;bool compatible=SUCCEEDED(D3DReflect(code->GetBufferPointer(),code->GetBufferSize(),IID_ID3D11ShaderReflection,(void**)&reflection));D3D11_SHADER_DESC d{};if(compatible){reflection->GetDesc(&d);compatible=d.InputParameters==native.inputs;}
   for(unsigned i=0;compatible&&i<native.inputs;++i){D3D11_SIGNATURE_PARAMETER_DESC p{};const auto& f=native.signature[i];compatible=SUCCEEDED(reflection->GetInputParameterDesc(i,&p))&&p.Register==f.reg&&p.Mask==f.mask&&p.ComponentType==f.type&&p.SystemValueType==f.system&&p.SemanticIndex==f.index&&!strcmp(p.SemanticName,f.semantic);}
   drop(reflection);if(compatible)device->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&pixel);else touchDiagnostics.note(TouchStage::PickShader,"reason=input-signature-mismatch shader=%016llx",native.hash);
  }
  if(pixel){const unsigned own=1;pixel->SetPrivateData(kTouchInternalShader,sizeof(own),&own);}
  if(!pixel)touchDiagnostics.note(TouchStage::PickShader,"semantic=%s%u components=%u swizzle=%s",binding.semantic,binding.index,binding.components,binding.xy);
  drop(code);drop(error);shaders.emplace(key,pixel);return pixel;
 }
 ID3D11DepthStencilState* depth_state(const D3D11_DEPTH_STENCIL_DESC& desc){const std::string key((const char*)&desc,sizeof(desc));auto at=depths.find(key);if(at==depths.end()){if(depths.size()>=64)return nullptr;ID3D11DepthStencilState* state=nullptr;if(FAILED(device->CreateDepthStencilState(&desc,&state)))return nullptr;at=depths.emplace(key,state).first;}at->second->AddRef();return at->second;}
 ID3D11RasterizerState* raster_state(const D3D11_RASTERIZER_DESC& desc){const std::string key((const char*)&desc,sizeof(desc));auto at=rasters.find(key);if(at==rasters.end()){if(rasters.size()>=64)return nullptr;ID3D11RasterizerState* state=nullptr;if(FAILED(device->CreateRasterizerState(&desc,&state)))return nullptr;at=rasters.emplace(key,state).first;}at->second->AddRef();return at->second;}
 Target* target(UINT width,UINT height,ID3D11Resource* nativeDepth){
  for(auto& t:targets)if(t.width==width&&t.height==height&&t.depthNative==nativeDepth)return &t;if(targets.size()>=3||width<64||height<64||width>8192||height>8192){touchDiagnostics.note(TouchStage::Depth,"reason=target-size-or-cache-limit size=%ux%u targets=%zu",width,height,targets.size());return nullptr;}
  ID3D11Texture2D* original=nullptr;if(FAILED(nativeDepth->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&original)))return nullptr;D3D11_TEXTURE2D_DESC depthDesc{};original->GetDesc(&depthDesc);original->Release();
  if(depthDesc.Width!=width||depthDesc.Height!=height||depthDesc.SampleDesc.Count!=1||depthDesc.ArraySize!=1){touchDiagnostics.note(TouchStage::Depth,"reason=depth-dimensions-or-msaa color=%ux%u depth=%ux%u samples=%u array=%u",width,height,depthDesc.Width,depthDesc.Height,depthDesc.SampleDesc.Count,depthDesc.ArraySize);return nullptr;}
  DXGI_FORMAT depthFormat=DXGI_FORMAT_UNKNOWN;
  if(depthDesc.Format==DXGI_FORMAT_D32_FLOAT||depthDesc.Format==DXGI_FORMAT_R32_TYPELESS){depthDesc.Format=DXGI_FORMAT_R32_TYPELESS;depthFormat=DXGI_FORMAT_R32_FLOAT;}
  else if(depthDesc.Format==DXGI_FORMAT_D32_FLOAT_S8X24_UINT||depthDesc.Format==DXGI_FORMAT_R32G8X24_TYPELESS){depthDesc.Format=DXGI_FORMAT_R32G8X24_TYPELESS;depthFormat=DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;}
  else if(depthDesc.Format==DXGI_FORMAT_D24_UNORM_S8_UINT||depthDesc.Format==DXGI_FORMAT_R24G8_TYPELESS){depthDesc.Format=DXGI_FORMAT_R24G8_TYPELESS;depthFormat=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;}
  else if(depthDesc.Format==DXGI_FORMAT_D16_UNORM||depthDesc.Format==DXGI_FORMAT_R16_TYPELESS){depthDesc.Format=DXGI_FORMAT_R16_TYPELESS;depthFormat=DXGI_FORMAT_R16_UNORM;}
  else {touchDiagnostics.note(TouchStage::Depth,"reason=depth-format format=%u",unsigned(depthDesc.Format));return nullptr;}
  Target t;t.width=width;t.height=height;t.epsilon=depthFormat==DXGI_FORMAT_R16_UNORM?.00002f:.000001f;
  auto fail=[&]()->Target*{for(auto& read:t.reads)drop(read.texture);drop(t.filteredView);drop(t.filtered);drop(t.depthView);drop(t.depthCopy);drop(t.source);drop(t.view);drop(t.texture);return nullptr;};
  D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
  if(FAILED(device->CreateTexture2D(&d,nullptr,&t.texture))||FAILED(device->CreateRenderTargetView(t.texture,nullptr,&t.view))||FAILED(device->CreateShaderResourceView(t.texture,nullptr,&t.source)))return fail();
  depthDesc.Usage=D3D11_USAGE_DEFAULT;depthDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;depthDesc.CPUAccessFlags=depthDesc.MiscFlags=0;D3D11_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=depthFormat;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;
  if(FAILED(device->CreateTexture2D(&depthDesc,nullptr,&t.depthCopy))||FAILED(device->CreateShaderResourceView(t.depthCopy,&srv,&t.depthView)))return fail();
  d.Width=d.Height=1;d.BindFlags=D3D11_BIND_RENDER_TARGET;if(FAILED(device->CreateTexture2D(&d,nullptr,&t.filtered))||FAILED(device->CreateRenderTargetView(t.filtered,nullptr,&t.filteredView)))return fail();
  d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;for(auto& read:t.reads)if(FAILED(device->CreateTexture2D(&d,nullptr,&read.texture)))return fail();
  t.depthNative=nativeDepth;nativeDepth->AddRef();targets.push_back(t);return &targets.back();
 }
 template<class Draw>void replay(ID3D11DeviceContext* context,Draw draw){
  if(!touch_gpu_backend_allowed())return;
  if(phoneInternalWork||!touch_active())return;
  if(!touchInput.gesture_requested())return;touchDiagnostics.note(TouchStage::Draw,"context=%s",context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED?"deferred":"immediate");
  unsigned mask=0;UINT bytes=sizeof(mask);if(FAILED(context->GetPrivateData(kTouchBindings,&bytes,&mask))||!mask){touchDiagnostics.note(TouchStage::NoBinding,"draw has no tagged screen SRV");return;}
  std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock){touchDiagnostics.note(TouchStage::GpuBusy);return;}PhoneInternalScope own;
  ID3D11Device* current=nullptr;context->GetDevice(&current);bool ready=start(current);current->Release();if(!ready){touchDiagnostics.note(TouchStage::GpuInit);return;}
  if(touchTransfer.capture(context,mask,draw))return;
  ID3D11PixelShader* original=nullptr;ID3D11ClassInstance* classes[256]{};UINT classCount=256;context->PSGetShader(&original,classes,&classCount);
  unsigned internal=0;bytes=sizeof(internal);if(original&&SUCCEEDED(original->GetPrivateData(kTouchInternalShader,&bytes,&internal))&&internal){drop(original);for(auto& p:classes)drop(p);return;}
  TouchShaderData metadata{};bytes=sizeof(metadata);if(!original||FAILED(original->GetPrivateData(kTouchShader,&bytes,&metadata))){unsigned observed=~0u;for(unsigned slot=0;slot<16;++slot)if(mask&(1u<<slot)){ID3D11ShaderResourceView* view=nullptr;context->PSGetShaderResources(slot,1,&view);ID3D11Resource* resource=nullptr;if(view)view->GetResource(&resource);TouchSurface bound;if(touch_surface_read(resource,bound))observed=bound.slot;drop(resource);drop(view);}char environment[256]{};if(!(touchDiagnostics.seen.load()&(uint64_t(1)<<unsigned(original?TouchStage::NoMetadata:TouchStage::NoShader))))touch_pass_description(context,environment,sizeof(environment));touchDiagnostics.note(original?TouchStage::NoMetadata:TouchStage::NoShader,"shader=%p mask=0x%x slot=%u %s",original,mask,observed,environment);touchDiagnostics.capture(original,observed,mask,unsigned(context->GetType()),environment);++unsupported;drop(original);for(auto& p:classes)drop(p);return;}
  if(FAILED(metadata.reflection))touchDiagnostics.note(TouchStage::Reflection,"shader=%016llx hr=0x%08lx",metadata.hash,static_cast<unsigned long>(metadata.reflection));
  else if(FAILED(metadata.disassembly))touchDiagnostics.note(TouchStage::Disassembly,"shader=%016llx hr=0x%08lx",metadata.hash,static_cast<unsigned long>(metadata.disassembly));
  TouchSurface surface;ID3D11PixelShader* pick=nullptr;bool ambiguous=false;unsigned observedSlot=~0u;
  for(unsigned slot=0;slot<16;++slot)if(mask&(1u<<slot)){ID3D11ShaderResourceView* view=nullptr;context->PSGetShaderResources(slot,1,&view);ID3D11Resource* resource=nullptr;if(view)view->GetResource(&resource);TouchSurface candidate;const bool valid=touch_surface_read(resource,candidate);drop(resource);drop(view);if(valid){observedSlot=candidate.slot;auto* candidateShader=shader(metadata,metadata.textures[slot]);if(candidateShader){if(pick&&surface.slot!=candidate.slot)ambiguous=true;else{surface=candidate;pick=candidateShader;}}}}
  touchDiagnostics.capture(original,observedSlot,mask,unsigned(context->GetType()));
  if(!pick||ambiguous){touchDiagnostics.note(ambiguous?TouchStage::Ambiguous:TouchStage::NoCoordinates,"shader=%016llx mask=0x%x samples=%u direct=%u calculated=%u unmatched=%u",metadata.hash,mask,metadata.samples,metadata.direct,metadata.calculated,metadata.unmatched);++unsupported;drop(original);for(auto& p:classes)drop(p);return;}
  ID3D11RenderTargetView* oldTargets[8]{};ID3D11DepthStencilView* depth=nullptr;ID3D11UnorderedAccessView* uavs[8]{};
  context->OMGetRenderTargets(8,oldTargets,&depth);UINT boundCount=8;while(boundCount&&!oldTargets[boundCount-1])--boundCount;if(boundCount<8)context->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,boundCount,8-boundCount,uavs+boundCount);
  ID3D11DepthStencilState* oldDepth=nullptr;UINT stencil=0;context->OMGetDepthStencilState(&oldDepth,&stencil);D3D11_DEPTH_STENCIL_DESC dd{};if(oldDepth)oldDepth->GetDesc(&dd);else{dd.DepthEnable=TRUE;dd.DepthFunc=D3D11_COMPARISON_LESS;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;}
  ID3D11Texture2D* color=nullptr;ID3D11Resource* resource=nullptr;if(oldTargets[0])oldTargets[0]->GetResource(&resource);if(resource)resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&color);drop(resource);
  D3D11_TEXTURE2D_DESC description{};if(color)color->GetDesc(&description);drop(color);
  ID3D11Resource* nativeDepth=nullptr;if(depth)depth->GetResource(&nativeDepth);const bool comparison=dd.DepthFunc==D3D11_COMPARISON_LESS||dd.DepthFunc==D3D11_COMPARISON_LESS_EQUAL||dd.DepthFunc==D3D11_COMPARISON_GREATER||dd.DepthFunc==D3D11_COMPARISON_GREATER_EQUAL||dd.DepthFunc==D3D11_COMPARISON_EQUAL;Target* output=nativeDepth&&dd.DepthEnable&&comparison&&description.SampleDesc.Count==1?target(description.Width,description.Height,nativeDepth):nullptr;drop(nativeDepth);if(output)output->mode=dd.DepthFunc==D3D11_COMPARISON_EQUAL?0:dd.DepthFunc==D3D11_COMPARISON_GREATER||dd.DepthFunc==D3D11_COMPARISON_GREATER_EQUAL?-1.f:1.f;
  if(!output)touchDiagnostics.note(TouchStage::Depth,"rt=%ux%u format=%u samples=%u depth=%u enabled=%u comparison=%u targets=%zu",description.Width,description.Height,unsigned(description.Format),description.SampleDesc.Count,unsigned(depth!=nullptr),unsigned(dd.DepthEnable),unsigned(dd.DepthFunc),targets.size());
  if(output){
   POINT cursor=touchInput.cursor({menuCursorX.load(),menuCursorY.load()});RECT client{};GetClientRect(settingsWindow,&client);
   const UINT x=UINT(std::clamp(double(cursor.x)*description.Width/std::max<LONG>(1,client.right),0.,double(description.Width-1))),y=UINT(std::clamp(double(cursor.y)*description.Height/std::max<LONG>(1,client.bottom),0.,double(description.Height-1)));
   if(!output->used){const float clear[4]{};context->ClearRenderTargetView(output->view,clear);output->cursor=cursor;output->x=x;output->y=y;} // one cursor point per frame
   if(TouchInput::close_point(output->cursor,cursor,0)){
    dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;if(dd.DepthFunc==D3D11_COMPARISON_LESS)dd.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;if(dd.DepthFunc==D3D11_COMPARISON_GREATER)dd.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;dd.StencilWriteMask=0;dd.FrontFace.StencilFailOp=dd.FrontFace.StencilDepthFailOp=dd.FrontFace.StencilPassOp=D3D11_STENCIL_OP_KEEP;dd.BackFace.StencilFailOp=dd.BackFace.StencilDepthFailOp=dd.BackFace.StencilPassOp=D3D11_STENCIL_OP_KEEP;
    ID3D11DepthStencilState* readonly=nullptr;ID3D11RasterizerState* oldRaster=nullptr,*scissor=nullptr;context->RSGetState(&oldRaster);D3D11_RASTERIZER_DESC rd{};if(oldRaster)oldRaster->GetDesc(&rd);else{rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_BACK;rd.DepthClipEnable=TRUE;}rd.ScissorEnable=TRUE;
    readonly=depth_state(dd);scissor=raster_state(rd);if(!readonly||!scissor)touchDiagnostics.note(TouchStage::GpuInit,"depth/raster state creation or cache cap");if(readonly&&scissor){
     ID3D11BlendState* blend=nullptr;FLOAT factors[4]{};UINT samples=0;context->OMGetBlendState(&blend,factors,&samples);ID3D11Buffer* buffer=nullptr;context->PSGetConstantBuffers(0,1,&buffer);D3D11_RECT scissors[16]{};UINT count=16;context->RSGetScissorRects(&count,scissors);
     touchDiagnostics.note(TouchStage::Replay,"slot=%u shader=%016llx client=%ld,%ld target=%ux%u pixel=%u,%u fitted=%.5f,%.5f %.5fx%.5f",surface.slot,metadata.hash,cursor.x,cursor.y,description.Width,description.Height,x,y,surface.left,surface.top,surface.width,surface.height);
     const float id[4]={float(surface.slot+1+token*managedDeviceCount),0,0,0};context->UpdateSubresource(constants,0,nullptr,id,0,0);ID3D11UnorderedAccessView* empty[7]{};context->OMSetRenderTargetsAndUnorderedAccessViews(1,&output->view,depth,1,7,empty,nullptr);context->OMSetBlendState(nullptr,nullptr,0xffffffff);context->OMSetDepthStencilState(readonly,stencil);context->RSSetState(scissor);const D3D11_RECT point{LONG(x),LONG(y),LONG(x+1),LONG(y+1)};context->RSSetScissorRects(1,&point);context->PSSetShader(pick,nullptr,0);context->PSSetConstantBuffers(0,1,&constants);draw();
     UINT targetsCount=8;while(targetsCount&&!oldTargets[targetsCount-1])--targetsCount;UINT keep[8];std::fill(std::begin(keep),std::end(keep),~0u);if(targetsCount<8)context->OMSetRenderTargetsAndUnorderedAccessViews(targetsCount,oldTargets,depth,targetsCount,8-targetsCount,uavs+targetsCount,keep);else context->OMSetRenderTargets(8,oldTargets,depth);context->OMSetBlendState(blend,factors,samples);context->OMSetDepthStencilState(oldDepth,stencil);context->RSSetState(oldRaster);context->RSSetScissorRects(count,scissors);context->PSSetShader(original,classes,classCount);context->PSSetConstantBuffers(0,1,&buffer);drop(buffer);drop(blend);output->surfaces[surface.slot]=surface;output->used=true;++draws;
    }
    drop(readonly);drop(scissor);drop(oldRaster);
   }
  }
  for(auto& p:oldTargets)drop(p);for(auto& p:uavs)drop(p);drop(depth);drop(oldDepth);drop(original);for(auto& p:classes)drop(p);
 }
 void present(ID3D11DeviceContext* context,uint64_t now){
  if(!touch_gpu_backend_allowed())return;
  std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock)return;
  if(!touch_active()){release();touchTransfer.stop();return;}if(!device)return;PhoneInternalScope own;touchTransfer.present(context);
  for(auto& output:targets){
   for(auto& read:output.reads)if(read.pending){D3D11_MAPPED_SUBRESOURCE data{};const auto mapResult=context->Map(read.texture,0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&data);if(FAILED(mapResult))touchDiagnostics.note_for(read.trace,mapResult==DXGI_ERROR_WAS_STILL_DRAWING?TouchStage::ReadbackBusy:TouchStage::ReadbackError,"hr=0x%08lx",static_cast<unsigned long>(mapResult));if(SUCCEEDED(mapResult)){float pixel[4];memcpy(pixel,data.pData,16);context->Unmap(read.texture,0);const int packed=std::isfinite(pixel[2])?int(std::lround(pixel[2]))-1:-1,slot=packed>=0?packed%int(managedDeviceCount):-1;TouchHit hit;hit.cursor=read.cursor;hit.time=now;hit.trace=read.trace;
    if(slot>=0&&unsigned(slot)<managedDeviceCount&&unsigned(packed/int(managedDeviceCount))==read.token){hit.surface=read.surfaces[slot];const int proof=touchTransfer.verdict(hit.surface.proof);if(!proof&&now>=read.time&&now-read.time<=200){touchDiagnostics.note_for(read.trace,TouchStage::ProofPending,"proof=%llu",hit.surface.proof);continue;}if(proof!=1)touchDiagnostics.note_for(read.trace,TouchStage::ProofRejected,"proof=%llu state=%d age=%llu",hit.surface.proof,proof,now-read.time);const float u=pixel[0],v=pixel[1];hit.u=(u-hit.surface.left)/hit.surface.width;hit.v=(v-hit.surface.top)/hit.surface.height;hit.valid=proof==1&&hit.surface.slot==unsigned(slot)&&hit.surface.width>0&&hit.surface.height>0&&std::isfinite(hit.u)&&std::isfinite(hit.v)&&hit.u>=0&&hit.u<=1&&hit.v>=0&&hit.v<=1;if(hit.valid)++hits;}read.pending=false;
    touchDiagnostics.note_for(read.trace,hit.valid?TouchStage::Hit:slot<0?TouchStage::PixelMiss:unsigned(packed/int(managedDeviceCount))!=read.token?TouchStage::StalePixel:TouchStage::UvOutside,"slot=%d raw_uv=%.5f,%.5f content_uv=%.5f,%.5f age=%llu token=%u actual=%d",slot,pixel[0],pixel[1],hit.u,hit.v,now-read.time,read.token,packed>=0?packed/int(managedDeviceCount):-1);
    if(now>=read.time&&now-read.time<=200)touchInput.hit(hit);else touchDiagnostics.note_for(read.trace,TouchStage::StalePixel,"readback age exceeds 200 ms");
   }}
   if(output.used){for(auto& read:output.reads)if(!read.pending){filterContext->ClearState();filterContext->CopyResource(output.depthCopy,output.depthNative);filterContext->OMSetRenderTargets(1,&output.filteredView,nullptr);D3D11_VIEWPORT vp{0,0,1,1,0,1};filterContext->RSSetViewports(1,&vp);filterContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);filterContext->VSSetShader(filterVertex,nullptr,0);filterContext->PSSetShader(filterPixel,nullptr,0);ID3D11ShaderResourceView* sources[]={output.source,output.depthView};filterContext->PSSetShaderResources(0,2,sources);const float params[]={float(output.x),float(output.y),output.mode,output.epsilon};filterContext->UpdateSubresource(filterConstants,0,nullptr,params,0,0);filterContext->PSSetConstantBuffers(0,1,&filterConstants);filterContext->Draw(3,0);filterContext->OMSetRenderTargets(0,nullptr,nullptr);ID3D11ShaderResourceView* empty[2]{};filterContext->PSSetShaderResources(0,2,empty);filterContext->CopyResource(read.texture,output.filtered);ID3D11CommandList* commands=nullptr;if(FAILED(filterContext->FinishCommandList(FALSE,&commands)))break;context->ExecuteCommandList(commands,TRUE);commands->Release();read.pending=true;read.cursor=output.cursor;read.time=now;read.trace=touchDiagnostics.current.load();read.token=token;read.surfaces=output.surfaces;break;}output.used=false;}
  }
  token=token%1024+1;
 }
};
inline TouchPicker touchPicker;
}
