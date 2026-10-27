#pragma once
#include "TouchInput.h"
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <map>
#include "TouchCopyEvidence.h"
namespace bettergps_overlay {
// Certify the actual output of a native 2D composition, not a replay of its
// shader. The two immutable images are copied after the native draw. Only our
// compute shader runs, on our deferred context; native shader objects and state
// are never queried/restored. A four-byte asynchronous verdict tests each RGB
// channel for four axis orientations (point OR linear). Alpha is reported, not
// used as a coordinate: native UI passes can legitimately force it to opaque.
struct TouchComposition {
 template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
 struct Result {uint64_t at=0,parent=0;int value=0;unsigned axes=0;};
 struct Job {Ptr<ID3D11Texture2D> source,image;Ptr<ID3D11ShaderResourceView> sourceView,imageView;Ptr<ID3D11Buffer> flag,read;Ptr<ID3D11UnorderedAccessView> output;Ptr<ID3D11Query> done;Ptr<ID3D11Resource> destination;uint64_t token=0,trace=0;unsigned slot=0;bool log=true;};
 Ptr<ID3D11Device> device;Ptr<ID3D11DeviceContext> worker;Ptr<ID3D11ComputeShader> shader;Ptr<ID3D11SamplerState> linear,point;
 std::deque<Job> jobs;std::map<uint64_t,Result> results;mutable std::mutex resultMutex;
 struct Association {Ptr<ID3D11Resource> resource;uint64_t at=0,trace=0,notice=0;unsigned state=~0u;};std::vector<Association> associations;
 uint64_t sequence=0;
 inline static std::atomic<uint64_t> activeUntil{0};
 static bool tagged_copy(ID3D11Resource* r){TouchSurface s;return touch_surface_read(r,s)&&(s.proof>>63);}
 static void invalidate(ID3D11Resource* r){if(r&&tagged_copy(r))r->SetPrivateData(kTouchSurface,0,nullptr);}
 static void before_view(ID3D11View* v){if(phoneInternalWork||!v)return;Ptr<ID3D11Resource> r;v->GetResource(&r);invalidate(r.Get());}
 static void before_draw(ID3D11DeviceContext* c){if(phoneInternalWork||GetTickCount64()>activeUntil.load()||!touch_active()||c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;Ptr<ID3D11RenderTargetView> v;c->OMGetRenderTargets(1,&v,nullptr);before_view(v.Get());}
 void before_execute(){if(phoneInternalWork)return;for(auto& a:associations)invalidate(a.resource.Get());}
 int mapping(uint64_t token,uint64_t now,unsigned& axes)const {
  axes=0;if(!token)return 1;if(!(token>>63))return -1;std::unique_lock<std::mutex> lock(resultMutex,std::try_to_lock);if(!lock)return 0;
  // Bounded ancestry: no recursive walk on an untrusted render graph.
  bool pending=false;for(unsigned n=0;n<8&&token;++n){auto at=results.find(token);if(at==results.end()||now<at->second.at||now-at->second.at>250||at->second.value<0)return -1;pending|=at->second.value==0;axes^=at->second.axes;token=at->second.parent;}return token?-1:pending?0:1;
 }
 int verdict(uint64_t token,uint64_t now)const {unsigned axes=0;return mapping(token,now,axes);}
 static void transform(unsigned axes,float& u,float& v){if(axes&1)u=1-u;if(axes&2)v=1-v;}
 void stop(){jobs.clear();touchCopyEvidence.stop();{std::lock_guard<std::mutex> lock(resultMutex);results.clear();}associations.clear();shader.Reset();linear.Reset();point.Reset();worker.Reset();device.Reset();activeUntil=0;}
 bool start(ID3D11Device* d){
  if(device.Get()==d&&shader&&worker)return true;stop();device=d;
  const char* code=R"(
Texture2D<float4> actual:register(t0);Texture2D<float4> source:register(t1);
SamplerState linearSample:register(s0);SamplerState pointSample:register(s1);
RWBuffer<uint> bad:register(u0);groupshared uint groupBad;
[numthreads(16,16,1)] void main(uint3 p:SV_DispatchThreadID,uint i:SV_GroupIndex){
 if(i==0)groupBad=0;GroupMemoryBarrierWithGroupSync();uint w,h;actual.GetDimensions(w,h);
 if(p.x<w&&p.y<h){float2 uv=(float2(p.xy)+.5)/float2(w,h);float4 a=actual.Load(int3(p.xy,0));
  // sRGB8/UNORM8 output rounding plus an 8-bit subtexel filter-weight step.
  // Odd-sized native viewports need this: interpolated PS coordinates and CS
  // pixel-centre division can round to opposite sides of a filter-weight step.
  uint fail=0;[unroll]for(uint axes=0;axes<4;++axes){float2 q=uv;if(axes&1)q.x=1-q.x;if(axes&2)q.y=1-q.y;
   float4 b=source.SampleLevel(linearSample,q,0),c=source.SampleLevel(pointSample,q,0);
   [unroll]for(uint channel=0;channel<4;++channel){if(!isfinite(a[channel])||!isfinite(b[channel])||abs(a[channel]-b[channel])>.0085)fail|=1u<<(axes*8+channel);
    if(!isfinite(a[channel])||!isfinite(c[channel])||abs(a[channel]-c[channel])>.0085)fail|=1u<<(axes*8+4+channel);}
  }
  if(fail)InterlockedOr(groupBad,fail);
 }GroupMemoryBarrierWithGroupSync();if(i==0&&groupBad)InterlockedOr(bad[0],groupBad);
})";
  Ptr<ID3DBlob> bytes,error;if(FAILED(D3DCompile(code,strlen(code),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&bytes,&error))||FAILED(d->CreateComputeShader(bytes->GetBufferPointer(),bytes->GetBufferSize(),nullptr,&shader))||FAILED(d->CreateDeferredContext(0,&worker)))return false;
  D3D11_SAMPLER_DESC s{};s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;s.MaxLOD=D3D11_FLOAT32_MAX;s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;if(FAILED(d->CreateSamplerState(&s,&linear)))return false;s.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;return SUCCEEDED(d->CreateSamplerState(&s,&point));
 }
 static bool bounded(const D3D11_TEXTURE2D_DESC& d){return d.Width>=16&&d.Height>=16&&d.Width<=4096&&d.Height<=4096&&uint64_t(d.Width)*d.Height<=1024*1024&&d.SampleDesc.Count==1&&d.ArraySize==1;}
 static bool color(DXGI_FORMAT f){return f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||f==DXGI_FORMAT_B8G8R8A8_UNORM||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;}
 bool snapshot(ID3D11DeviceContext* c,ID3D11Texture2D* input,DXGI_FORMAT format,Ptr<ID3D11Texture2D>& image,Ptr<ID3D11ShaderResourceView>& view){
  D3D11_TEXTURE2D_DESC d{};input->GetDesc(&d);d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.CPUAccessFlags=d.MiscFlags=0;d.MipLevels=1;
  D3D11_SHADER_RESOURCE_VIEW_DESC v{};v.Format=format;v.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;v.Texture2D.MipLevels=1;
  if(FAILED(device->CreateTexture2D(&d,nullptr,&image))||FAILED(device->CreateShaderResourceView(image.Get(),&v,&view)))return false;c->CopySubresourceRegion(image.Get(),0,0,0,0,input,0,nullptr);return true;
 }
 bool capture(ID3D11DeviceContext* c,unsigned mask){
  if(phoneInternalWork||!touch_active()||c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||!mask||(mask&(mask-1)))return false;
  TouchInput::Request request;if(!touchInput.request(request)){if(!touchStatus.visible)return false;request.cursor=touchStatus.snapshot().cursor;request.trace=0;}
  Ptr<ID3D11RenderTargetView> target;Ptr<ID3D11DepthStencilView> depth;c->OMGetRenderTargets(1,&target,&depth);if(depth||!target)return false;
  unsigned binding=0;while(!(mask&(1u<<binding)))++binding;Ptr<ID3D11ShaderResourceView> source;c->PSGetShaderResources(binding,1,&source);if(!source)return false;
  Ptr<ID3D11Resource> sr,dr;source->GetResource(&sr);target->GetResource(&dr);TouchSurface surface;if(sr.Get()==dr.Get()||!touch_surface_read(sr.Get(),surface))return false;
  const auto now=GetTickCount64();if(surface.slot>=managedDeviceCount||surface.epoch!=vehicleStart.epoch.load()||surface.proof&&verdict(surface.proof,now)<0)return false;
  Ptr<ID3D11Texture2D> st,dt;if(FAILED(sr.As(&st))||FAILED(dr.As(&dt)))return false;D3D11_TEXTURE2D_DESC sd{},dd{};st->GetDesc(&sd);dt->GetDesc(&dd);if(!bounded(sd)||!bounded(dd))return false;
  D3D11_SHADER_RESOURCE_VIEW_DESC sv{};D3D11_RENDER_TARGET_VIEW_DESC tv{};source->GetDesc(&sv);target->GetDesc(&tv);if(sv.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D||sv.Texture2D.MostDetailedMip||tv.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D||tv.Texture2D.MipSlice||!color(sv.Format)||!color(tv.Format))return false;
  UINT n=1;D3D11_VIEWPORT vp{};c->RSGetViewports(&n,&vp);if(n!=1||vp.TopLeftX||vp.TopLeftY||vp.Width!=dd.Width||vp.Height!=dd.Height)return false;
  Ptr<ID3D11Device> d;c->GetDevice(&d);PhoneInternalScope own;if(!start(d.Get())||jobs.size()>=3)return true;
  // A device can traverse several native targets in ONE frame. Throttling by
  // device would suppress the second hop; throttle the exact target instead.
  auto association=std::find_if(associations.begin(),associations.end(),[&](const auto& a){return a.resource.Get()==dr.Get();});if(association==associations.end()){if(associations.size()>=12)return true;associations.push_back({dr,now,request.trace});association=std::prev(associations.end());}else{if(association->trace==request.trace&&now>=association->at&&now-association->at<66)return true;association->at=now;association->trace=request.trace;}
  Job job;job.slot=surface.slot;job.trace=request.trace;job.destination=dr;job.log=request.trace||!association->notice||now-association->notice>=5000;if(job.log)association->notice=now;if(!snapshot(c,st.Get(),sv.Format,job.source,job.sourceView)||!snapshot(c,dt.Get(),tv.Format,job.image,job.imageView))return true;
  D3D11_BUFFER_DESC b{};b.ByteWidth=4;b.BindFlags=D3D11_BIND_UNORDERED_ACCESS;D3D11_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=DXGI_FORMAT_R32_UINT;u.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;u.Buffer.NumElements=1;
  if(FAILED(device->CreateBuffer(&b,nullptr,&job.flag))||FAILED(device->CreateUnorderedAccessView(job.flag.Get(),&u,&job.output)))return true;b.Usage=D3D11_USAGE_STAGING;b.BindFlags=0;b.CPUAccessFlags=D3D11_CPU_ACCESS_READ;D3D11_QUERY_DESC q{D3D11_QUERY_EVENT,0};if(FAILED(device->CreateBuffer(&b,nullptr,&job.read))||FAILED(device->CreateQuery(&q,&job.done)))return true;
  worker->ClearState();const UINT zero[4]{};worker->ClearUnorderedAccessViewUint(job.output.Get(),zero);worker->CSSetShader(shader.Get(),nullptr,0);ID3D11ShaderResourceView* inputs[]={job.imageView.Get(),job.sourceView.Get()};ID3D11SamplerState* samples[]={linear.Get(),point.Get()};worker->CSSetShaderResources(0,2,inputs);worker->CSSetSamplers(0,2,samples);worker->CSSetUnorderedAccessViews(0,1,job.output.GetAddressOf(),nullptr);worker->Dispatch((dd.Width+15)/16,(dd.Height+15)/16,1);worker->ClearState();worker->CopyResource(job.read.Get(),job.flag.Get());Ptr<ID3D11CommandList> commands;if(FAILED(worker->FinishCommandList(FALSE,&commands)))return true;c->ExecuteCommandList(commands.Get(),TRUE);c->End(job.done.Get());
  const auto parent=surface.proof;job.token=(uint64_t(1)<<63)|++sequence;{std::lock_guard<std::mutex> lock(resultMutex);results[job.token]={now,parent,0};while(results.size()>256)results.erase(results.begin());}surface.proof=job.token;dr->SetPrivateData(kTouchSurface,sizeof(surface),&surface);activeUntil=now+250;
  if(job.log){char detail[288];snprintf(detail,sizeof(detail),"operation=actual-image-copy slot=%u trace=%llu source=%ux%u target=%ux%u proof=%llu parent=%llu",surface.slot,request.trace,sd.Width,sd.Height,dd.Width,dd.Height,job.token,parent);touchDiagnostics.note_for(request.trace,TouchStage::Transfer,"%s",detail);touchStatus.diagnostic_event(touchStatus.sourceGeneration.load(),"native-copy",detail);}jobs.push_back(std::move(job));return true;
 }
 void poll(ID3D11DeviceContext* c){
  {PhoneInternalScope own;touchCopyEvidence.poll(c);}
  const auto now=GetTickCount64();associations.erase(std::remove_if(associations.begin(),associations.end(),[&](const auto& a){return now<a.at||now-a.at>250;}),associations.end());
  if(jobs.empty())return;PhoneInternalScope own;
  Ptr<ID3D11Device> current;c->GetDevice(&current);if(current.Get()!=device.Get()){stop();return;}
  for(auto it=jobs.begin();it!=jobs.end();){auto& j=*it;Result result;{std::lock_guard<std::mutex> lock(resultMutex);result=results[j.token];}BOOL done=FALSE;auto hr=c->GetData(j.done.Get(),&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);if(GetTickCount64()-result.at>200||FAILED(hr)){{std::lock_guard<std::mutex> lock(resultMutex);results[j.token].value=-1;}it=jobs.erase(it);continue;}if(hr!=S_OK||!done){++it;continue;}D3D11_MAPPED_SUBRESOURCE map{};hr=c->Map(j.read.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map);if(hr==DXGI_ERROR_WAS_STILL_DRAWING){++it;continue;}unsigned flags=~0u;if(SUCCEEDED(hr)){memcpy(&flags,map.pData,4);c->Unmap(j.read.Get(),0);}unsigned allowed=0;for(unsigned axes=0;axes<4;++axes)if(((flags>>(axes*8))&7)==0||((flags>>(axes*8+4))&7)==0)allowed|=1u<<axes;
   result.value=SUCCEEDED(hr)&&allowed&&!(allowed&(allowed-1))?1:-1;if(result.value==1){result.axes=0;while(!(allowed&(1u<<result.axes)))++result.axes;}{std::lock_guard<std::mutex> lock(resultMutex);results[j.token]=result;}
   if(SUCCEEDED(hr)&&!allowed)touchCopyEvidence.capture(device.Get(),c,j.source.Get(),j.image.Get(),j.slot,j.trace,flags);
   auto association=std::find_if(associations.begin(),associations.end(),[&](const auto& a){return a.resource.Get()==j.destination.Get();});const unsigned state=(result.value==1?0x100:0)|allowed;const bool changed=association!=associations.end()&&association->state!=state;if(association!=associations.end())association->state=state;
   if(j.log||changed){char detail[288];snprintf(detail,sizeof(detail),"operation=actual-image-verdict slot=%u trace=%llu proof=%llu parent=%llu mapping=%s axes=%u candidates=0x%x rgba_mismatch=0x%08x",j.slot,j.trace,j.token,result.parent,result.value==1?"verified":allowed?"ambiguous":"rejected",result.axes,allowed,flags);touchDiagnostics.note_for(j.trace,TouchStage::TransferResult,"%s",detail);touchStatus.diagnostic_event(touchStatus.sourceGeneration.load(),"native-copy-verdict",detail);}it=jobs.erase(it);
  }
 }
};
inline TouchComposition touchComposition;
}
