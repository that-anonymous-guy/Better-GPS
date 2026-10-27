#pragma once
// ATS can compose a completed device UI into a second texture with an early,
// unobserved 2D shader. Certify that copy using the original vertex/pixel
// shaders and a private UV/checker texture. Never infer identity from size.
// The entire private output is checked on the GPU; only a four-byte verdict
// is read back, asynchronously. The game texture is never modified.
namespace bettergps_overlay {
struct TouchTransfer {
 template<class T> struct Ref {T* p=nullptr;Ref()=default;Ref(const Ref&)=delete;Ref& operator=(const Ref&)=delete;~Ref(){if(p)p->Release();}T** out(){return &p;}};
 template<class T>static void drop(T*& p){if(p)p->Release();p=nullptr;}
 struct Job {
  ID3D11Texture2D* image=nullptr;ID3D11RenderTargetView* target=nullptr;
  ID3D11ShaderResourceView* input=nullptr;ID3D11Buffer *verdict=nullptr,*read=nullptr;
  ID3D11UnorderedAccessView* flag=nullptr;
  ID3D11Query* completed=nullptr;
  ID3D11ShaderResourceView* reference=nullptr;ID3D11SamplerState* sampler=nullptr;
  UINT width=0,height=0;unsigned state=0;uint64_t token=0,trace=0,began=0;
 };
 ID3D11Device* device=nullptr;ID3D11DeviceContext* worker=nullptr;ID3D11ComputeShader* check=nullptr;
 ID3D11Texture2D* coordinates=nullptr;ID3D11ShaderResourceView* coordinatesView=nullptr;
 UINT coordinateWidth=0,coordinateHeight=0;uint64_t sequence=0;
 std::array<Job,3> jobs{};std::map<uint64_t,int> results;
 void stop(){for(auto& j:jobs){drop(j.reference);drop(j.sampler);drop(j.completed);drop(j.flag);drop(j.read);drop(j.verdict);drop(j.input);drop(j.target);drop(j.image);j={};}drop(coordinatesView);drop(coordinates);drop(check);drop(worker);drop(device);coordinateWidth=coordinateHeight=0;results.clear();}
 int verdict(uint64_t token)const {if(!token)return 1;const auto at=results.find(token);return at==results.end()?-1:at->second;}
 bool start(ID3D11Device* d){
  if(device==d&&check&&worker)return true;stop();device=d;device->AddRef();
  if(FAILED(device->CreateDeferredContext(0,&worker)))return false;
  const char* source=R"(
Texture2D<float4> image:register(t0);Texture2D<float4> reference:register(t1);
SamplerState nativeSampler:register(s0);RWBuffer<uint> bad:register(u0);
groupshared uint mismatch;
[numthreads(16,16,1)] void main(uint3 at:SV_DispatchThreadID,uint index:SV_GroupIndex){
 if(index==0)mismatch=0;GroupMemoryBarrierWithGroupSync();
 uint w,h;image.GetDimensions(w,h);
 if(at.x<w&&at.y<h){float4 c=image.Load(int3(at.xy,0));
  float4 expected=reference.SampleLevel(nativeSampler,(float2(at.xy)+.5)/float2(w,h),0);
  if(!all(isfinite(c))||any(abs(c-expected)>.001))InterlockedOr(mismatch,1);
 }GroupMemoryBarrierWithGroupSync();if(index==0&&mismatch)InterlockedOr(bad[0],1);
})";
  Ref<ID3DBlob> code,error;
  return SUCCEEDED(D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,code.out(),error.out()))&&SUCCEEDED(device->CreateComputeShader(code.p->GetBufferPointer(),code.p->GetBufferSize(),nullptr,&check));
 }
 bool gradient(UINT w,UINT h){
  if(coordinatesView&&coordinateWidth==w&&coordinateHeight==h)return true;
  drop(coordinatesView);drop(coordinates);coordinateWidth=coordinateHeight=0;
  std::vector<float> pixels(size_t(w)*h*4);
  for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){const size_t i=(size_t(y)*w+x)*4;pixels[i]=(x+.5f)/w;pixels[i+1]=(y+.5f)/h;pixels[i+2]=((x/16+y/16)&1)?.75f:.25f;pixels[i+3]=1;}
  D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{pixels.data(),w*16,0};
  if(FAILED(device->CreateTexture2D(&d,&data,&coordinates))||FAILED(device->CreateShaderResourceView(coordinates,nullptr,&coordinatesView)))return false;coordinateWidth=w;coordinateHeight=h;return true;
 }
 bool prepare(Job& j,UINT w,UINT h){
  if(j.image&&j.width==w&&j.height==h)return true;
  drop(j.reference);drop(j.sampler);drop(j.completed);drop(j.flag);drop(j.read);drop(j.verdict);drop(j.input);drop(j.target);drop(j.image);j={};
  D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
  if(FAILED(device->CreateTexture2D(&d,nullptr,&j.image))||FAILED(device->CreateRenderTargetView(j.image,nullptr,&j.target))||FAILED(device->CreateShaderResourceView(j.image,nullptr,&j.input)))return false;
  D3D11_BUFFER_DESC b{};b.ByteWidth=4;b.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
  D3D11_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=DXGI_FORMAT_R32_UINT;u.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;u.Buffer.NumElements=1;
  if(FAILED(device->CreateBuffer(&b,nullptr,&j.verdict))||FAILED(device->CreateUnorderedAccessView(j.verdict,&u,&j.flag)))return false;
  b.Usage=D3D11_USAGE_STAGING;b.BindFlags=0;b.CPUAccessFlags=D3D11_CPU_ACCESS_READ;if(FAILED(device->CreateBuffer(&b,nullptr,&j.read)))return false;j.width=w;j.height=h;return true;
 }
 template<class Draw>bool capture(ID3D11DeviceContext* c,unsigned mask,Draw draw){
  Ref<ID3D11DepthStencilView> depth;Ref<ID3D11RenderTargetView> target;
  c->OMGetRenderTargets(1,target.out(),depth.out());if(depth.p||!target.p)return false;
  // An exact resource association is mandatory; a same-size unrelated UI
  // never enters this path. Multiple devices in a draw are ambiguous.
  if(!mask||(mask&(mask-1)))return false;unsigned slot=0;while(!(mask&(1u<<slot)))++slot;
  Ref<ID3D11ShaderResourceView> source;c->PSGetShaderResources(slot,1,source.out());if(!source.p)return false;
  Ref<ID3D11Resource> src,dst;source.p->GetResource(src.out());target.p->GetResource(dst.out());TouchSurface surface;
  if(src.p==dst.p||!touch_surface_read(src.p,surface))return false;
  Ref<ID3D11Texture2D> st,dt;src.p->QueryInterface(__uuidof(ID3D11Texture2D),(void**)st.out());dst.p->QueryInterface(__uuidof(ID3D11Texture2D),(void**)dt.out());if(!st.p||!dt.p)return false;
  D3D11_TEXTURE2D_DESC sd{},dd{};st.p->GetDesc(&sd);dt.p->GetDesc(&dd);
  const auto bounded=[](const D3D11_TEXTURE2D_DESC& d){return d.Width>=64&&d.Height>=64&&d.Width<=4096&&d.Height<=4096&&uint64_t(d.Width)*d.Height<=1024*1024&&d.SampleDesc.Count==1&&d.ArraySize==1;};if(!bounded(sd)||!bounded(dd))return false;
  D3D11_SHADER_RESOURCE_VIEW_DESC sv{};source.p->GetDesc(&sv);D3D11_RENDER_TARGET_VIEW_DESC tv{};target.p->GetDesc(&tv);
  if(sv.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D||sv.Texture2D.MostDetailedMip||tv.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D||tv.Texture2D.MipSlice)return false;
  D3D11_VIEWPORT views[16]{};UINT n=16;c->RSGetViewports(&n,views);if(n!=1||views[0].TopLeftX||views[0].TopLeftY||views[0].Width!=dd.Width||views[0].Height!=dd.Height)return false;
  ID3D11RenderTargetView* targets[8]{};ID3D11UnorderedAccessView* uavs[7]{};c->OMGetRenderTargets(8,targets,nullptr);c->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,1,7,uavs);
  bool simple=true;for(unsigned i=0;i<8;++i){if(i&&targets[i])simple=false;drop(targets[i]);}for(auto& u:uavs){if(u)simple=false;drop(u);}if(!simple)return false;
  Ref<ID3D11SamplerState> sampler;c->PSGetSamplers(0,1,sampler.out());if(!sampler.p)return false;
  Ref<ID3D11Device> d;c->GetDevice(d.out());if(!start(d.p))return true;
  Job* job=nullptr;for(auto& j:jobs)if(!j.state){job=&j;break;}if(!job||!gradient(sd.Width,sd.Height)||!prepare(*job,dd.Width,dd.Height))return true;
  const auto parent=surface.proof;if(parent&&verdict(parent)!=1)return true;
  // Fresh sample-count evidence proves that THIS draw executed (a fresh
  // EVENT query is already signalled on some drivers). Reusing the previous probe
  // image must not certify a deferred command list which ATS discarded.
  drop(job->completed);D3D11_QUERY_DESC query{D3D11_QUERY_OCCLUSION,0};if(FAILED(device->CreateQuery(&query,&job->completed)))return true;
  drop(job->reference);drop(job->sampler);job->reference=coordinatesView;coordinatesView->AddRef();job->sampler=sampler.p;sampler.p->AddRef();
  Ref<ID3D11BlendState> blend;FLOAT factors[4]{};UINT samples=0;c->OMGetBlendState(blend.out(),factors,&samples);
  const float invalid[4]={-1,-1,-1,-1};c->ClearRenderTargetView(job->target,invalid);
  c->OMSetRenderTargets(1,&job->target,nullptr);c->OMSetBlendState(nullptr,nullptr,~0u);c->PSSetShaderResources(slot,1,&coordinatesView);c->Begin(job->completed);draw();c->End(job->completed);
  c->PSSetShaderResources(slot,1,&source.p);c->OMSetRenderTargets(1,&target.p,nullptr);c->OMSetBlendState(blend.p,factors,samples);
  job->state=1;job->token=++sequence;job->trace=touchDiagnostics.current.load();job->began=GetTickCount64();results[job->token]=0;
  surface.proof=job->token;dst.p->SetPrivateData(kTouchSurface,sizeof(surface),&surface);
  while(results.size()>256)results.erase(results.begin());
  touchDiagnostics.note(TouchStage::Transfer,"operation=verify-copy slot=%u source=%ux%u target=%ux%u proof=%llu",surface.slot,sd.Width,sd.Height,dd.Width,dd.Height,job->token);return true;
 }
 void present(ID3D11DeviceContext* c){
  if(!device)return;
  for(auto& j:jobs){
   if(j.state&&GetTickCount64()-j.began>200){results[j.token]=-1;j.state=0;touchDiagnostics.note_for(j.trace,TouchStage::TransferResult,"operation=copy-verdict proof=%llu reason=expired-or-discarded",j.token);}
   if(j.state==2){D3D11_MAPPED_SUBRESOURCE map{};const auto hr=c->Map(j.read,0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map);if(SUCCEEDED(hr)){const bool good=*static_cast<const UINT*>(map.pData)==0;c->Unmap(j.read,0);results[j.token]=good?1:-1;j.state=0;touchDiagnostics.note_for(j.trace,TouchStage::TransferResult,"operation=copy-verdict proof=%llu identity=%u",j.token,unsigned(good));}else if(hr!=DXGI_ERROR_WAS_STILL_DRAWING){results[j.token]=-1;j.state=0;touchDiagnostics.note_for(j.trace,TouchStage::TransferResult,"operation=copy-verdict proof=%llu hr=0x%08lx",j.token,static_cast<unsigned long>(hr));}}
   if(j.state==1){UINT64 executed=0;const auto hr=c->GetData(j.completed,&executed,sizeof(executed),D3D11_ASYNC_GETDATA_DONOTFLUSH);if(FAILED(hr)){results[j.token]=-1;j.state=0;continue;}if(hr!=S_OK||!executed)continue;worker->ClearState();const UINT zero[4]{};worker->ClearUnorderedAccessViewUint(j.flag,zero);worker->CSSetShader(check,nullptr,0);ID3D11ShaderResourceView* inputs[]={j.input,j.reference};worker->CSSetShaderResources(0,2,inputs);worker->CSSetSamplers(0,1,&j.sampler);worker->CSSetUnorderedAccessViews(0,1,&j.flag,nullptr);worker->Dispatch((j.width+15)/16,(j.height+15)/16,1);ID3D11UnorderedAccessView* empty=nullptr;worker->CSSetUnorderedAccessViews(0,1,&empty,nullptr);worker->CopyResource(j.read,j.verdict);Ref<ID3D11CommandList> commands;if(SUCCEEDED(worker->FinishCommandList(FALSE,commands.out()))){c->ExecuteCommandList(commands.p,TRUE);j.state=2;}else{results[j.token]=-1;j.state=0;}}
  }
 }
};
inline TouchTransfer touchTransfer;
}
