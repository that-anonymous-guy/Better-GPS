#pragma once
#include "TouchPicking.h"
#include "TouchGeometryCapture.h"
#include "TouchCpuPicker.h"
namespace bettergps_overlay {
using TouchCreatePixel=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11PixelShader**);
inline TouchCreatePixel touchCreatePixelOriginal=nullptr;
inline HRESULT STDMETHODCALLTYPE touch_create_pixel(ID3D11Device* d,const void* code,SIZE_T size,ID3D11ClassLinkage* links,ID3D11PixelShader** out){const auto result=touchCreatePixelOriginal(d,code,size,links,out);if(SUCCEEDED(result)&&out){geometry_tag_code(*out,code,size);if(touch_gpu_backend_allowed())touch_shader_metadata(*out,code,size);}return result;}
using GeometryCreateVertex=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11VertexShader**);
using GeometryCreateLayout=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const D3D11_INPUT_ELEMENT_DESC*,UINT,const void*,SIZE_T,ID3D11InputLayout**);
inline GeometryCreateVertex geometryCreateVertexOriginal=nullptr;
inline GeometryCreateLayout geometryCreateLayoutOriginal=nullptr;
inline HRESULT STDMETHODCALLTYPE geometry_create_vertex(ID3D11Device* d,const void* code,SIZE_T size,ID3D11ClassLinkage* links,ID3D11VertexShader** out){const auto hr=geometryCreateVertexOriginal(d,code,size,links,out);if(SUCCEEDED(hr)&&out)geometry_tag_code(*out,code,size);return hr;}
inline HRESULT STDMETHODCALLTYPE geometry_create_layout(ID3D11Device* d,const D3D11_INPUT_ELEMENT_DESC* desc,UINT count,const void* code,SIZE_T size,ID3D11InputLayout** out){const auto hr=geometryCreateLayoutOriginal(d,desc,count,code,size,out);if(SUCCEEDED(hr)&&out)geometry_tag_layout(*out,desc,count);return hr;}
template<class Shader>using GeometrySetShader=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,Shader*,ID3D11ClassInstance* const*,UINT);
struct GeometryShaderCalls {GeometrySetShader<ID3D11VertexShader> vs=nullptr;GeometrySetShader<ID3D11PixelShader> ps=nullptr;GeometrySetShader<ID3D11GeometryShader> gs=nullptr;GeometrySetShader<ID3D11HullShader> hs=nullptr;GeometrySetShader<ID3D11DomainShader> ds=nullptr;void(STDMETHODCALLTYPE* clear)(ID3D11DeviceContext*)=nullptr;void(STDMETHODCALLTYPE* execute)(ID3D11DeviceContext*,ID3D11CommandList*,BOOL)=nullptr;};
inline std::array<GeometryShaderCalls,2> geometryShaderCalls;
struct TouchDepthCalls {
 void(STDMETHODCALLTYPE* clear)(ID3D11DeviceContext*,ID3D11DepthStencilView*,UINT,FLOAT,UINT8)=nullptr;
 void(STDMETHODCALLTYPE* discard)(ID3D11DeviceContext1*,ID3D11Resource*)=nullptr;
 void(STDMETHODCALLTYPE* discardView)(ID3D11DeviceContext1*,ID3D11View*)=nullptr;
 void(STDMETHODCALLTYPE* clearView)(ID3D11DeviceContext1*,ID3D11View*,const FLOAT*,const D3D11_RECT*,UINT)=nullptr;
 void(STDMETHODCALLTYPE* discardView1)(ID3D11DeviceContext1*,ID3D11View*,const D3D11_RECT*,UINT)=nullptr;
};
inline std::array<TouchDepthCalls,2> touchDepthCalls;
inline void touch_before_view(ID3D11DeviceContext* c,ID3D11View* view,const char* boundary){TouchComposition::before_view(view);if(!phoneInternalWork&&touchGeometry.watchDepth&&view){ComPtr<ID3D11Resource> r;view->GetResource(&r);touchGeometry.before_overwrite(c,r.Get(),boundary);}}
template<size_t I>void STDMETHODCALLTYPE touch_clear_depth(ID3D11DeviceContext* c,ID3D11DepthStencilView* view,UINT flags,FLOAT depth,UINT8 stencil){touchGeometry.before_depth_clear(c,view,flags);touchDepthCalls[I].clear(c,view,flags,depth,stencil);}
template<size_t I>void STDMETHODCALLTYPE touch_discard(ID3D11DeviceContext1* c,ID3D11Resource* r){touchGeometry.before_overwrite(c,r,"before-discard-resource");touchDepthCalls[I].discard(c,r);}
template<size_t I>void STDMETHODCALLTYPE touch_discard_view(ID3D11DeviceContext1* c,ID3D11View* v){touch_before_view(c,v,"before-discard-view");touchDepthCalls[I].discardView(c,v);}
template<size_t I>void STDMETHODCALLTYPE touch_clear_view(ID3D11DeviceContext1* c,ID3D11View* v,const FLOAT* color,const D3D11_RECT* rects,UINT n){touch_before_view(c,v,"before-clear-view");touchDepthCalls[I].clearView(c,v,color,rects,n);}
template<size_t I>void STDMETHODCALLTYPE touch_discard_view1(ID3D11DeviceContext1* c,ID3D11View* v,const D3D11_RECT* rects,UINT n){touch_before_view(c,v,"before-discard-view1");touchDepthCalls[I].discardView1(c,v,rects,n);}
template<size_t I>void STDMETHODCALLTYPE geometry_set_vs(ID3D11DeviceContext* c,ID3D11VertexShader* s,ID3D11ClassInstance* const* instances,UINT n){geometryShaderCalls[I].vs(c,s,instances,n);geometry_shadow_shader(c,kGeometryVS,s,n);}
template<size_t I>void STDMETHODCALLTYPE geometry_set_ps(ID3D11DeviceContext* c,ID3D11PixelShader* s,ID3D11ClassInstance* const* instances,UINT n){geometryShaderCalls[I].ps(c,s,instances,n);geometry_shadow_shader(c,kGeometryPS,s,n);}
template<size_t I>void STDMETHODCALLTYPE geometry_set_gs(ID3D11DeviceContext* c,ID3D11GeometryShader* s,ID3D11ClassInstance* const* instances,UINT n){geometryShaderCalls[I].gs(c,s,instances,n);geometry_shadow_extension(c,1,s!=nullptr);}
template<size_t I>void STDMETHODCALLTYPE geometry_set_hs(ID3D11DeviceContext* c,ID3D11HullShader* s,ID3D11ClassInstance* const* instances,UINT n){geometryShaderCalls[I].hs(c,s,instances,n);geometry_shadow_extension(c,2,s!=nullptr);}
template<size_t I>void STDMETHODCALLTYPE geometry_set_ds(ID3D11DeviceContext* c,ID3D11DomainShader* s,ID3D11ClassInstance* const* instances,UINT n){geometryShaderCalls[I].ds(c,s,instances,n);geometry_shadow_extension(c,4,s!=nullptr);}
inline void geometry_invalidate_state(ID3D11DeviceContext* c){if(phoneInternalWork)return;geometry_shadow_shader(c,kGeometryVS,nullptr,0);geometry_shadow_shader(c,kGeometryPS,nullptr,0);const unsigned zero=0;c->SetPrivateData(kGeometryExtensions,sizeof(zero),&zero);c->SetPrivateData(kTouchBindings,sizeof(zero),&zero);}
template<size_t I>void STDMETHODCALLTYPE geometry_clear(ID3D11DeviceContext* c){geometryShaderCalls[I].clear(c);geometry_invalidate_state(c);}
template<size_t I>void STDMETHODCALLTYPE geometry_execute(ID3D11DeviceContext* c,ID3D11CommandList* list,BOOL restore){touchComposition.before_execute();geometryShaderCalls[I].execute(c,list,restore);if(!restore)geometry_invalidate_state(c);}
struct TouchDrawOriginals {
 using Indexed=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,INT);
 using Draw=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT);
 using IndexedInstanced=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);
 using Instanced=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,UINT);
 using Auto=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*);
 using Indirect=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Buffer*,UINT);
 Indexed indexed=nullptr;Draw draw=nullptr;IndexedInstanced indexedInstanced=nullptr;Instanced instanced=nullptr;Auto automatic=nullptr;Indirect indexedIndirect=nullptr,indirect=nullptr;
};
inline std::array<TouchDrawOriginals,2> touchDrawCalls;
inline std::vector<void*> touchHookAddresses;
template<size_t I>void STDMETHODCALLTYPE touch_draw_indexed(ID3D11DeviceContext* c,UINT n,UINT start,INT base){auto draw=[&]{touchDrawCalls[I].indexed(c,n,start,base);};touchGeometry.before_draw(c,n);draw();touchGeometry.draw(c,n,start,base,true);touchPicker.replay(c,draw);}
template<size_t I>void STDMETHODCALLTYPE touch_draw(ID3D11DeviceContext* c,UINT n,UINT start){auto draw=[&]{touchDrawCalls[I].draw(c,n,start);};touchGeometry.before_draw(c,n);draw();touchGeometry.draw(c,n,start,0,false);touchPicker.replay(c,draw);}
template<size_t I>void STDMETHODCALLTYPE touch_draw_indexed_instanced(ID3D11DeviceContext* c,UINT n,UINT instances,UINT start,INT base,UINT instance){auto draw=[&]{touchDrawCalls[I].indexedInstanced(c,n,instances,start,base,instance);};touchGeometry.before_draw(c,n);draw();touchGeometry.draw(c,n,start,base,true,instances,instance);touchPicker.replay(c,draw);}
template<size_t I>void STDMETHODCALLTYPE touch_draw_instanced(ID3D11DeviceContext* c,UINT n,UINT instances,UINT start,UINT instance){auto draw=[&]{touchDrawCalls[I].instanced(c,n,instances,start,instance);};touchGeometry.before_draw(c,n);draw();touchGeometry.draw(c,n,start,0,false,instances,instance);touchPicker.replay(c,draw);}
template<size_t I>void STDMETHODCALLTYPE touch_draw_auto(ID3D11DeviceContext* c){auto draw=[&]{touchDrawCalls[I].automatic(c);};TouchComposition::before_draw(c);draw();touchGeometry.unsupported_draw(c,"DrawAuto count unavailable");touchPicker.replay(c,draw);}
template<size_t I>void STDMETHODCALLTYPE touch_draw_indexed_indirect(ID3D11DeviceContext* c,ID3D11Buffer* args,UINT offset){auto draw=[&]{touchDrawCalls[I].indexedIndirect(c,args,offset);};TouchComposition::before_draw(c);draw();touchGeometry.unsupported_draw(c,"indirect draw arguments unsupported");touchPicker.replay(c,draw);}
template<size_t I>void STDMETHODCALLTYPE touch_draw_indirect(ID3D11DeviceContext* c,ID3D11Buffer* args,UINT offset){auto draw=[&]{touchDrawCalls[I].indirect(c,args,offset);};TouchComposition::before_draw(c);draw();touchGeometry.unsupported_draw(c,"indirect draw arguments unsupported");touchPicker.replay(c,draw);}
inline thread_local bool touchInstallingHookBatch=false;
struct TouchHookInstallBatch {bool previous=touchInstallingHookBatch;TouchHookInstallBatch(){touchInstallingHookBatch=true;}~TouchHookInstallBatch(){touchInstallingHookBatch=previous;}};
inline bool touch_add_hook(void* address,void* callback,void** original){
 if(std::find(touchHookAddresses.begin(),touchHookAddresses.end(),address)!=touchHookAddresses.end())return true;
 if(MH_CreateHook(address,callback,original)!=MH_OK)return false;touchHookAddresses.push_back(address);return touchInstallingHookBatch||MH_EnableHook(address)==MH_OK;
}
template<size_t I>bool touch_hook_context(ID3D11DeviceContext* context){
 auto** v=*reinterpret_cast<void***>(context);auto& o=touchDrawCalls[I];
 if(!touch_add_hook(v[53],(void*)&touch_clear_depth<I>,(void**)&touchDepthCalls[I].clear))return false;
 ComPtr<ID3D11DeviceContext1> context1;if(SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)))){auto** e=*reinterpret_cast<void***>(context1.Get());if(!touch_add_hook(e[117],(void*)&touch_discard<I>,(void**)&touchDepthCalls[I].discard)||!touch_add_hook(e[118],(void*)&touch_discard_view<I>,(void**)&touchDepthCalls[I].discardView)||!touch_add_hook(e[132],(void*)&touch_clear_view<I>,(void**)&touchDepthCalls[I].clearView)||!touch_add_hook(e[133],(void*)&touch_discard_view1<I>,(void**)&touchDepthCalls[I].discardView1))return false;}
 return touch_add_hook(v[11],(void*)&geometry_set_vs<I>,(void**)&geometryShaderCalls[I].vs)&&touch_add_hook(v[9],(void*)&geometry_set_ps<I>,(void**)&geometryShaderCalls[I].ps)&&
 touch_add_hook(v[110],(void*)&geometry_clear<I>,(void**)&geometryShaderCalls[I].clear)&&touch_add_hook(v[58],(void*)&geometry_execute<I>,(void**)&geometryShaderCalls[I].execute)&&
 touch_add_hook(v[23],(void*)&geometry_set_gs<I>,(void**)&geometryShaderCalls[I].gs)&&touch_add_hook(v[60],(void*)&geometry_set_hs<I>,(void**)&geometryShaderCalls[I].hs)&&touch_add_hook(v[64],(void*)&geometry_set_ds<I>,(void**)&geometryShaderCalls[I].ds)&&
 touch_add_hook(v[12],(void*)&touch_draw_indexed<I>,(void**)&o.indexed)&&touch_add_hook(v[13],(void*)&touch_draw<I>,(void**)&o.draw)&&
 touch_add_hook(v[20],(void*)&touch_draw_indexed_instanced<I>,(void**)&o.indexedInstanced)&&touch_add_hook(v[21],(void*)&touch_draw_instanced<I>,(void**)&o.instanced)&&
 touch_add_hook(v[38],(void*)&touch_draw_auto<I>,(void**)&o.automatic)&&touch_add_hook(v[39],(void*)&touch_draw_indexed_indirect<I>,(void**)&o.indexedIndirect)&&touch_add_hook(v[40],(void*)&touch_draw_indirect<I>,(void**)&o.indirect);
}
inline void touch_hooks_stop(){for(auto address:touchHookAddresses){MH_DisableHook(address);MH_RemoveHook(address);}touchHookAddresses.clear();touchCreatePixelOriginal=nullptr;geometryCreateVertexOriginal=nullptr;geometryCreateLayoutOriginal=nullptr;geometryShaderCalls={};touchDepthCalls={};touchDrawCalls={};touchGeometry.stop();touchComposition.stop();touchCpuPicker.stop();touchPicker.stop();touchInput.cancel();}
inline bool touch_hooks_install(ID3D11Device* device,ID3D11DeviceContext* context,std::vector<void*>* pending=nullptr){
 if(!touchHookAddresses.empty())return true;TouchHookInstallBatch batch;auto** v=*reinterpret_cast<void***>(device);
 bool ok=touch_add_hook(v[12],(void*)&geometry_create_vertex,(void**)&geometryCreateVertexOriginal)&&touch_add_hook(v[11],(void*)&geometry_create_layout,(void**)&geometryCreateLayoutOriginal)&&touch_add_hook(v[15],(void*)&touch_create_pixel,(void**)&touchCreatePixelOriginal)&&touch_hook_context<0>(context);
 ID3D11DeviceContext* deferred=nullptr;if(ok&&SUCCEEDED(device->CreateDeferredContext(0,&deferred))){ok=touch_hook_context<1>(deferred);deferred->Release();}
 if(ok){if(pending)pending->insert(pending->end(),touchHookAddresses.begin(),touchHookAddresses.end());else ok=enable_hook_batch(touchHookAddresses)==MH_OK;}
 if(!ok)touch_hooks_stop();return ok;
}
}
