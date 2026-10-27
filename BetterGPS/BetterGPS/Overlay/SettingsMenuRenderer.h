#pragma once
#include <d3d11.h>
#include <cstring>
namespace bettergps_overlay {
inline thread_local bool phoneInternalWork=false;
struct PhoneInternalScope {bool previous=phoneInternalWork;PhoneInternalScope(){phoneInternalWork=true;}~PhoneInternalScope(){phoneInternalWork=previous;}};
inline void settings_shader_module() {}
struct SettingsMenuRenderer {
    ID3D11Device* device{};ID3D11VertexShader* vs{};ID3D11PixelShader* ps{};
    ID3D11SamplerState* sampler{};ID3D11Buffer* constants{};ID3D11RasterizerState* raster{};ID3D11BlendState* blend{};
    template<class T>static void drop(T*& p){if(p){p->Release();p=nullptr;}}
    void stop(){drop(vs);drop(ps);drop(sampler);drop(constants);drop(raster);drop(blend);drop(device);}
    bool start(ID3D11Device* next){
        if(device==next&&vs&&ps&&sampler&&constants&&raster)return true;stop();device=next;device->AddRef();
        HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&settings_shader_module),&module);
        auto vsResource=FindResourceW(module,MAKEINTRESOURCEW(290),RT_RCDATA),psResource=FindResourceW(module,MAKEINTRESOURCEW(291),RT_RCDATA);
        if(!vsResource||!psResource)return false;
        HRESULT hr=device->CreateVertexShader(LockResource(LoadResource(module,vsResource)),SizeofResource(module,vsResource),nullptr,&vs);if(FAILED(hr))return false;
        hr=device->CreatePixelShader(LockResource(LoadResource(module,psResource)),SizeofResource(module,psResource),nullptr,&ps);if(FAILED(hr))return false;
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
        D3D11_BLEND_DESC bs{};bs.RenderTarget[0].BlendEnable=TRUE;bs.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;bs.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;bs.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;bs.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;bs.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;bs.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;bs.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        if(FAILED(device->CreateBlendState(&bs,&blend)))return false;
        return SUCCEEDED(device->CreateSamplerState(&sd,&sampler))&&SUCCEEDED(device->CreateBuffer(&bd,nullptr,&constants))&&SUCCEEDED(device->CreateRasterizerState(&rd,&raster));
    }
    bool render(ID3D11DeviceContext* context,ID3D11ShaderResourceView* source,ID3D11Texture2D* target,float left,float top,float width,float height,void(STDMETHODCALLTYPE* bind)(ID3D11DeviceContext*,UINT,UINT,ID3D11ShaderResourceView* const*),float dim=0,bool opaque=false,bool blackBlend=false,ID3D11RenderTargetView* cachedTarget=nullptr,float opacity=1,bool copyOutput=false){
        PhoneInternalScope internal; // Never observe/reclassify our own menu/compositor draws as native screens.
        if(context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
        ID3D11Device* next=nullptr;context->GetDevice(&next);bool ready=start(next);drop(next);if(!ready)return false;
        ID3D11RenderTargetView* output=cachedTarget;if(output)output->AddRef();else if(FAILED(device->CreateRenderTargetView(target,nullptr,&output)))return false;
        ID3D11RenderTargetView* oldTargets[8]{};ID3D11DepthStencilView* oldDepth=nullptr;
        ID3D11BlendState* oldBlend=nullptr;FLOAT blendFactor[4]{};UINT sampleMask=0;
        ID3D11DepthStencilState* oldDepthState=nullptr;UINT stencil=0;ID3D11RasterizerState* oldRaster=nullptr;
        D3D11_VIEWPORT viewports[16]{};UINT count=16;ID3D11VertexShader* oldVs=nullptr;ID3D11PixelShader* oldPs=nullptr;
        ID3D11GeometryShader* oldGs=nullptr;ID3D11HullShader* oldHs=nullptr;ID3D11DomainShader* oldDs=nullptr;
        ID3D11ClassInstance* vsClasses[256]{},*psClasses[256]{},*gsClasses[256]{},*hsClasses[256]{},*dsClasses[256]{};
        UINT vsCount=256,psCount=256,gsCount=256,hsCount=256,dsCount=256;
        ID3D11InputLayout* oldLayout=nullptr;D3D11_PRIMITIVE_TOPOLOGY topology;
        ID3D11ShaderResourceView* oldSource=nullptr;ID3D11SamplerState* oldSampler=nullptr;ID3D11Buffer* oldConstants=nullptr;
        context->OMGetRenderTargets(8,oldTargets,&oldDepth);context->OMGetBlendState(&oldBlend,blendFactor,&sampleMask);context->OMGetDepthStencilState(&oldDepthState,&stencil);context->RSGetState(&oldRaster);context->RSGetViewports(&count,viewports);
        context->VSGetShader(&oldVs,vsClasses,&vsCount);context->PSGetShader(&oldPs,psClasses,&psCount);
        context->GSGetShader(&oldGs,gsClasses,&gsCount);context->HSGetShader(&oldHs,hsClasses,&hsCount);context->DSGetShader(&oldDs,dsClasses,&dsCount);
        context->IAGetInputLayout(&oldLayout);context->IAGetPrimitiveTopology(&topology);context->PSGetShaderResources(0,1,&oldSource);context->PSGetSamplers(0,1,&oldSampler);context->PSGetConstantBuffers(0,1,&oldConstants);
        struct Parameters{float setting[4];} data{{dim,opaque?1.0f:0.0f,blackBlend?1.0f:0.0f,1-opacity}};
        context->UpdateSubresource(constants,0,nullptr,&data,0,0);
        D3D11_TEXTURE2D_DESC td{};target->GetDesc(&td);D3D11_VIEWPORT viewport{left,top,width,height,0,1};
        context->OMSetRenderTargets(1,&output,nullptr);context->OMSetBlendState(copyOutput?nullptr:blend,nullptr,0xffffffff);context->OMSetDepthStencilState(nullptr,0);context->RSSetState(raster);context->RSSetViewports(1,&viewport);
        context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->GSSetShader(nullptr,nullptr,0);context->HSSetShader(nullptr,nullptr,0);context->DSSetShader(nullptr,nullptr,0);context->VSSetShader(vs,nullptr,0);context->PSSetShader(ps,nullptr,0);context->PSSetSamplers(0,1,&sampler);context->PSSetConstantBuffers(0,1,&constants);bind(context,0,1,&source);context->Draw(3,0);
        ID3D11ShaderResourceView* empty=nullptr;bind(context,0,1,&empty);
        context->OMSetRenderTargets(8,oldTargets,oldDepth);context->OMSetBlendState(oldBlend,blendFactor,sampleMask);context->OMSetDepthStencilState(oldDepthState,stencil);context->RSSetState(oldRaster);context->RSSetViewports(count,viewports);
        context->IASetInputLayout(oldLayout);context->IASetPrimitiveTopology(topology);context->VSSetShader(oldVs,vsClasses,vsCount);context->PSSetShader(oldPs,psClasses,psCount);context->GSSetShader(oldGs,gsClasses,gsCount);context->HSSetShader(oldHs,hsClasses,hsCount);context->DSSetShader(oldDs,dsClasses,dsCount);context->PSSetSamplers(0,1,&oldSampler);context->PSSetConstantBuffers(0,1,&oldConstants);bind(context,0,1,&oldSource);
        for(auto& p:vsClasses)drop(p);for(auto& p:psClasses)drop(p);for(auto& p:gsClasses)drop(p);for(auto& p:hsClasses)drop(p);for(auto& p:dsClasses)drop(p);drop(oldGs);drop(oldHs);drop(oldDs);
        for(auto& p:oldTargets)drop(p);drop(oldDepth);drop(oldBlend);drop(oldDepthState);drop(oldRaster);drop(oldVs);drop(oldPs);drop(oldLayout);drop(oldSource);drop(oldSampler);drop(oldConstants);drop(output);return true;
    }
};
}
