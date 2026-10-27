#pragma once
#include "PhoneStartup.h"
#include "SettingsMenuRenderer.h"
#include <d3d11_1.h>
namespace bettergps_overlay {
inline const GUID phoneTrackedResource={0x541fe202,0x3584,0x40f0,{0x88,0x1a,0xa2,0x38,0x09,0x10,0x23,0x24}};
inline const GUID phoneOwnedResource={0x541fe203,0x3584,0x40f0,{0x88,0x1a,0xa2,0x38,0x09,0x10,0x23,0x24}};
// Native UI generation and physical display sampling are distinct consumers.
// Never inject a Draw into a shader-resource binding or a game's active query.
struct PhoneScreenPower {
    ID3D11Texture2D *native{},*snapshot{},*output{},*black{};
    ID3D11ShaderResourceView *snapshotView{},*outputView{},*blackView{},*nativeView{};
    ID3D11RenderTargetView* outputTarget{};
    ID3D11DeviceContext* deferred{};ID3D11DeviceContext1* isolatedContext{};ID3DDeviceContextState* isolatedState{};ID3D11BlendState* fadeBlend{};
    SettingsMenuRenderer renderer;size_t displayIndex=32;
    int reportedPhase=-99999;unsigned reportBudget=24;
    unsigned width=0,height=0,mips=1;uint64_t lastCopy=0,rendered=~uint64_t(0);
    bool captured=false,outputReady=false,shutdownVisible=false,managedOutput=false;
    ID3D11Texture2D* pageSnapshot{};ID3D11ShaderResourceView* pageSnapshotView{};ID3D11PixelShader* pageShader{};
    uint64_t pageSerial=~uint64_t(0);bool pageClosing=false;
    ~PhoneScreenPower(){stop();}
    void stop(){
        SettingsMenuRenderer::drop(pageSnapshotView);SettingsMenuRenderer::drop(pageSnapshot);SettingsMenuRenderer::drop(pageShader);pageClosing=false;pageSerial=~uint64_t(0);
        renderer.stop();SettingsMenuRenderer::drop(isolatedContext);SettingsMenuRenderer::drop(isolatedState);SettingsMenuRenderer::drop(deferred);SettingsMenuRenderer::drop(fadeBlend);
        SettingsMenuRenderer::drop(outputTarget);SettingsMenuRenderer::drop(outputView);SettingsMenuRenderer::drop(snapshotView);
        SettingsMenuRenderer::drop(blackView);SettingsMenuRenderer::drop(nativeView);SettingsMenuRenderer::drop(output);
        SettingsMenuRenderer::drop(snapshot);SettingsMenuRenderer::drop(black);SettingsMenuRenderer::drop(native);
        captured=outputReady=shutdownVisible=false;
    }
    bool start(ID3D11Texture2D* source){
        stop();D3D11_TEXTURE2D_DESC d{};source->GetDesc(&d);
        if(d.SampleDesc.Count!=1||d.ArraySize!=1)return false;
        ID3D11Device* device=nullptr;source->GetDevice(&device);native=source;native->AddRef();width=d.Width;height=d.Height;mips=d.MipLevels;
        d.Usage=D3D11_USAGE_DEFAULT;d.CPUAccessFlags=0;d.MiscFlags=mips>1?D3D11_RESOURCE_MISC_GENERATE_MIPS:0;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        if(d.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS)d.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        if(d.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS)d.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        auto snapshotDesc=d;snapshotDesc.MipLevels=1;snapshotDesc.MiscFlags=0;snapshotDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        bool ok=SUCCEEDED(device->CreateTexture2D(&snapshotDesc,nullptr,&snapshot))&&SUCCEEDED(device->CreateTexture2D(&d,nullptr,&output))&&
            SUCCEEDED(device->CreateShaderResourceView(snapshot,nullptr,&snapshotView))&&SUCCEEDED(device->CreateShaderResourceView(output,nullptr,&outputView))&&
            SUCCEEDED(device->CreateRenderTargetView(output,nullptr,&outputTarget));
        if(ok){D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.Format=d.Format;view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;view.Texture2D.MipLevels=mips;ok=SUCCEEDED(device->CreateShaderResourceView(native,&view,&nativeView));}
        // A permanent opaque black SRV never needs to bind a render target in
        // the live engine context, including during first startup and final off.
        std::vector<std::vector<uint8_t>> pixels(mips);std::vector<D3D11_SUBRESOURCE_DATA> initial(mips);
        for(unsigned mip=0;mip<mips;++mip){auto w=std::max(1u,width>>mip),h=std::max(1u,height>>mip);pixels[mip].resize(size_t(w)*h*4);for(size_t i=3;i<pixels[mip].size();i+=4)pixels[mip][i]=255;initial[mip].pSysMem=pixels[mip].data();initial[mip].SysMemPitch=w*4;}
        auto bd=d;bd.BindFlags=D3D11_BIND_SHADER_RESOURCE;bd.MiscFlags=0;
        if(ok)ok=SUCCEEDED(device->CreateTexture2D(&bd,initial.data(),&black))&&SUCCEEDED(device->CreateShaderResourceView(black,nullptr,&blackView))&&renderer.start(device);
        // SINGLETHREADED game devices cannot create deferred contexts.
        // Use a complete D3D11.1 context-state swap there, never a partial save.
        if(ok&&FAILED(device->CreateDeferredContext(0,&deferred))){ID3D11Device1* extended=nullptr;ID3D11DeviceContext* immediate=nullptr;device->GetImmediateContext(&immediate);ok=SUCCEEDED(device->QueryInterface(__uuidof(ID3D11Device1),(void**)&extended))&&SUCCEEDED(immediate->QueryInterface(__uuidof(ID3D11DeviceContext1),(void**)&isolatedContext));if(ok){auto level=device->GetFeatureLevel();UINT flags=(device->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)?D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED:0;ok=SUCCEEDED(extended->CreateDeviceContextState(flags,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),nullptr,&isolatedState));}if(extended)extended->Release();immediate->Release();}
        // Black source, constant attenuation of destination RGB, opaque alpha.
        D3D11_BLEND_DESC blend{};auto& rt=blend.RenderTarget[0];rt.BlendEnable=TRUE;rt.SrcBlend=D3D11_BLEND_ZERO;rt.DestBlend=D3D11_BLEND_BLEND_FACTOR;rt.BlendOp=D3D11_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_ZERO;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        if(ok)ok=SUCCEEDED(device->CreateBlendState(&blend,&fadeBlend));
        if(ok){UINT tag=1;native->SetPrivateData(phoneTrackedResource,sizeof(tag),&tag);for(auto* texture:{snapshot,output,black})texture->SetPrivateData(phoneOwnedResource,sizeof(tag),&tag);}
        device->Release();if(!ok)stop();return ok;
    }
    bool display_view(ID3D11ShaderResourceView* view)const{
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};view->GetDesc(&desc);
        // Preserve the startup path, including the one-level SRV ATS uses
        // while the phone is powered on. Exclude reads of individual lower mips.
        return desc.Texture2D.MostDetailedMip==0;
    }
    void finish_frame(ID3D11DeviceContext* context,int phase,uint64_t now){
        if(phase>-3){shutdownVisible=false;return;}if(!captured||!shutdownVisible)return;
        const int off=-phase-3;
        if((off<shutdownHoldMs&&outputReady)||rendered==now)return;
        const float dim=std::clamp(float(off-shutdownHoldMs)/shutdownFadeMs,0.0f,1.0f);
        // All rendering is recorded in our private deferred context. Execute
        // with restoration TRUE preserves *all* engine bindings, predication,
        // render targets, UAVs, stream output and shaders, not just PS slot zero.
        PhoneInternalScope isolated;ID3DDeviceContextState* savedState=nullptr;ID3D11Predicate* savedPredicate=nullptr;BOOL savedValue=FALSE;
        ID3D11DeviceContext* work=deferred?deferred:context;
        if(!deferred){context->GetPredication(&savedPredicate,&savedValue);isolatedContext->SwapDeviceContextState(isolatedState,&savedState);}
        work->ClearState();work->SetPredication(nullptr,FALSE);work->CopySubresourceRegion(output,0,0,0,0,snapshot,0,nullptr);
        if(off>=shutdownHoldMs){
        const float factor[4]={1-dim,1-dim,1-dim,1-dim};
        const float parameters[4]={1,1,1,0};work->UpdateSubresource(renderer.constants,0,nullptr,parameters,0,0);
        D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};
        work->OMSetRenderTargets(1,&outputTarget,nullptr);work->OMSetBlendState(fadeBlend,factor,0xffffffff);
        work->RSSetState(renderer.raster);work->RSSetViewports(1,&viewport);
        work->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        work->VSSetShader(renderer.vs,nullptr,0);work->PSSetShader(renderer.ps,nullptr,0);work->PSSetConstantBuffers(0,1,&renderer.constants);
        work->SetPredication(nullptr,FALSE);work->Draw(3,0);work->OMSetRenderTargets(0,nullptr,nullptr);
        }
        if(mips>1)work->GenerateMips(outputView);
        if(deferred){ID3D11CommandList* commands=nullptr;if(SUCCEEDED(deferred->FinishCommandList(FALSE,&commands))){context->ExecuteCommandList(commands,TRUE);commands->Release();outputReady=true;rendered=now;}}
        else{isolatedContext->SwapDeviceContextState(savedState,nullptr);if(savedState)savedState->Release();context->SetPredication(savedPredicate,savedValue);if(savedPredicate)savedPredicate->Release();outputReady=true;rendered=now;}
    }
    void capture_base(ID3D11DeviceContext* context,uint64_t now){
        PhoneInternalScope isolated;ID3D11Predicate* predicate=nullptr;BOOL value=FALSE;context->GetPredication(&predicate,&value);context->SetPredication(nullptr,FALSE);context->CopySubresourceRegion(snapshot,0,0,0,0,native,0,nullptr);context->SetPredication(predicate,value);if(predicate)predicate->Release();lastCopy=now;captured=true;shutdownVisible=false;outputReady=false;rendered=~uint64_t(0);
    }
    bool begin_page_close(ID3D11DeviceContext* context,uint64_t serial,const D3D11_BOX* crop=nullptr){
        if(!captured)return false;if(pageSerial==serial)return pageSnapshotView&&pageShader;
        PhoneInternalScope internal;
        ID3D11Device* device=renderer.device;
        D3D11_TEXTURE2D_DESC desc{};snapshot->GetDesc(&desc);if(crop){if(crop->left>=crop->right||crop->top>=crop->bottom||crop->right>desc.Width||crop->bottom>desc.Height)return false;desc.Width=crop->right-crop->left;desc.Height=crop->bottom-crop->top;}
        if(pageSnapshot){D3D11_TEXTURE2D_DESC prior{};pageSnapshot->GetDesc(&prior);if(prior.Width!=desc.Width||prior.Height!=desc.Height){SettingsMenuRenderer::drop(pageSnapshotView);SettingsMenuRenderer::drop(pageSnapshot);}}
        if(!pageSnapshot){if(FAILED(device->CreateTexture2D(&desc,nullptr,&pageSnapshot))||FAILED(device->CreateShaderResourceView(pageSnapshot,nullptr,&pageSnapshotView)))return false;}
        if(!pageShader){HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&settings_shader_module),&module);auto resource=FindResourceW(module,MAKEINTRESOURCEW(292),RT_RCDATA);if(!resource)return false;auto loaded=LoadResource(module,resource);if(FAILED(device->CreatePixelShader(LockResource(loaded),SizeofResource(module,resource),nullptr,&pageShader)))return false;}
        // Preserve the completed native map AND app UI before the worker's Home
        // replaces the next base. No readback, recomputation or transition logo.
        UINT tag=1;pageSnapshot->SetPrivateData(phoneOwnedResource,sizeof(tag),&tag);ID3D11Predicate* predicate=nullptr;BOOL value=FALSE;context->GetPredication(&predicate,&value);context->SetPredication(nullptr,FALSE);context->CopySubresourceRegion(pageSnapshot,0,0,0,0,snapshot,0,crop);context->SetPredication(predicate,value);if(predicate)predicate->Release();pageSerial=serial;return true;
    }
    void finish_page_close(ID3D11DeviceContext* context,float left,float top,float w,float h,float radius,uint64_t now){
        if(!pageSnapshotView||!pageShader||!captured)return;
        PhoneInternalScope internal;ID3DDeviceContextState* saved=nullptr;ID3D11Predicate* predicate=nullptr;BOOL value=FALSE;ID3D11DeviceContext* work=deferred?deferred:context;
        if(!deferred){context->GetPredication(&predicate,&value);isolatedContext->SwapDeviceContextState(isolatedState,&saved);}
        work->ClearState();work->SetPredication(nullptr,FALSE);work->CopySubresourceRegion(output,0,0,0,0,snapshot,0,nullptr);
        const float parameters[4]={w,h,radius,0};work->UpdateSubresource(renderer.constants,0,nullptr,parameters,0,0);D3D11_VIEWPORT viewport{left,top,w,h,0,1};
        work->OMSetRenderTargets(1,&outputTarget,nullptr);work->OMSetBlendState(renderer.blend,nullptr,0xffffffff);work->RSSetState(renderer.raster);work->RSSetViewports(1,&viewport);
        work->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);work->VSSetShader(renderer.vs,nullptr,0);work->PSSetShader(pageShader,nullptr,0);work->PSSetConstantBuffers(0,1,&renderer.constants);work->PSSetSamplers(0,1,&renderer.sampler);work->PSSetShaderResources(0,1,&pageSnapshotView);work->Draw(3,0);
        ID3D11ShaderResourceView* empty=nullptr;work->PSSetShaderResources(0,1,&empty);work->OMSetRenderTargets(0,nullptr,nullptr);if(mips>1)work->GenerateMips(outputView);
        bool ready=false;if(deferred){ID3D11CommandList* commands=nullptr;if(SUCCEEDED(deferred->FinishCommandList(FALSE,&commands))){context->ExecuteCommandList(commands,TRUE);commands->Release();ready=true;}}
        else{isolatedContext->SwapDeviceContextState(saved,nullptr);if(saved)saved->Release();context->SetPredication(predicate,value);if(predicate)predicate->Release();ready=true;}
        if(ready){pageClosing=outputReady=true;rendered=now;}
    }
    ID3D11ShaderResourceView* owned_view(int phase){
        if(phase>=startupBlackMs&&captured){shutdownVisible=false;return pageClosing&&outputReady?outputView:snapshotView;}
        if(phase<=-3&&captured){shutdownVisible=true;return -phase-3<shutdownHoldMs||!outputReady?snapshotView:outputView;}
        return blackView;
    }
    ID3D11ShaderResourceView* present(ID3D11DeviceContext* context,ID3D11ShaderResourceView* original,int phase,uint64_t now,void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,ID3D11ShaderResourceView* const*)){
        if(!native||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||!display_view(original))return original;
        if(phase>=startupBlackMs){
            // Capture the displayed base image BEFORE ATS reuses it. Never
            // preserve lower native mips: they can contain obsolete UI passes.
            if(!captured||now-lastCopy>=16)capture_base(context,now);
            shutdownVisible=false;outputReady=false;rendered=~uint64_t(0);return original;
        }
        if(phase<=-3&&captured){shutdownVisible=true;const int off=-phase-3;return off<shutdownHoldMs||!outputReady?snapshotView:outputView;}
        return blackView;
    }
};
}
