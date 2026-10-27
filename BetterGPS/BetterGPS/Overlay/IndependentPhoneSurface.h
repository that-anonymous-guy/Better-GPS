#pragma once
#include "PhoneScreenPower.h"
namespace bettergps_overlay {
// The front display exists before ATS has ever created its ignition-gated UI.
// CPU-prepared skin/startup frames are composed here, never via the native UI.
struct IndependentPhoneSurface {
    PhoneScreenPower power;
    ID3D11Texture2D *canvas{},*layer{};
    ID3D11RenderTargetView* target{};
    ID3D11ShaderResourceView* layerView{};
    size_t displayIndex=32;
    uint64_t seen=0,uploaded=0,composed=~uint64_t(0),baseRevision=0;
    uint64_t layerPowerState=0,composedPowerState=0;
    int layerPhase=-99999;
    bool dark=false,ready=false;
    ~IndependentPhoneSurface(){SettingsMenuRenderer::drop(layerView);SettingsMenuRenderer::drop(layer);SettingsMenuRenderer::drop(target);SettingsMenuRenderer::drop(canvas);}
    bool start(ID3D11Device* device,size_t index){
        displayIndex=index;D3D11_TEXTURE2D_DESC d{};d.Width=256;d.Height=512;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        if(FAILED(device->CreateTexture2D(&d,nullptr,&canvas))||FAILED(device->CreateRenderTargetView(canvas,nullptr,&target))||!power.start(canvas))return false;
        power.displayIndex=index;d.Width=360;d.Height=740;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.Usage=D3D11_USAGE_DYNAMIC;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(device->CreateTexture2D(&d,nullptr,&layer))&&SUCCEEDED(device->CreateShaderResourceView(layer,nullptr,&layerView));
    }
    bool upload(ID3D11DeviceContext* context,const std::vector<uint8_t>& pixels,unsigned stride,uint64_t version,uint64_t state,int phase){
        if(version==uploaded)return ready;if(stride<360||pixels.size()<size_t(stride)*740*4)return false;
        D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context->Map(layer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return false;
        for(unsigned y=0;y<740;++y)memcpy(static_cast<uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch,pixels.data()+size_t(y)*stride*4,360*4);
        context->Unmap(layer,0);uploaded=version;layerPowerState=state;layerPhase=phase;ready=true;return true;
    }
    ID3D11ShaderResourceView* view(int phase,uint64_t state){
        // A new startup must never borrow the preceding GPS/animation snapshot.
        if(phase>=0&&composedPowerState!=state)return power.blackView;
        return power.owned_view(phase);
    }
    void finish(ID3D11DeviceContext* context,PhoneScreenPower* base,int phase,uint64_t state,uint64_t now,bool night,ID3D11ShaderResourceView* map=nullptr,bool horizontal=false,PageVisual page={},bool waze=false){
        power.pageClosing=false;
        if(phase<startupBlackMs){power.finish_frame(context,phase,now);return;}
        if(layerPowerState!=state||layerPhase<startupBlackMs)return;
        const auto revision=map?now:base&&base->captured?base->lastCopy:0;
        // Closing must keep its completed GPU output even if the native map
        // producer is temporarily unavailable and the CPU Home has not changed.
        if(!ready||(page.transition>=0&&composed==uploaded&&baseRevision==revision&&dark==night))return;
        const bool closing=page.transition<0&&page.elapsed<pageCloseMs&&power.begin_page_close(context,page.serial);
        PhoneInternalScope isolated;ID3DDeviceContextState* saved=nullptr;ID3D11Predicate* predicate=nullptr;BOOL predValue=FALSE;
        auto* work=power.deferred?power.deferred:context;
        if(!power.deferred){context->GetPredication(&predicate,&predValue);power.isolatedContext->SwapDeviceContextState(power.isolatedState,&saved);}
        work->ClearState();work->SetPredication(nullptr,FALSE);
        const float bg[4]={horizontal?.001517635f:night?.054f:.75f,horizontal?.001517635f:night?.064f:.75f,horizontal?.001517635f:night?.081f:.75f,1};work->ClearRenderTargetView(target,bg);
        auto& renderer=power.renderer;D3D11_VIEWPORT viewport{0,0,256,512,0,1};
        work->OMSetRenderTargets(1,&target,nullptr);work->RSSetState(renderer.raster);work->RSSetViewports(1,&viewport);work->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        work->VSSetShader(renderer.vs,nullptr,0);work->PSSetShader(renderer.ps,nullptr,0);work->PSSetSamplers(0,1,&renderer.sampler);work->PSSetConstantBuffers(0,1,&renderer.constants);
        const float opaque[4]={0,1,0,0},alpha[4]={0,0,0,0};
        if(map){
            const float mapParameters[4]={horizontal?1.f:.25f,horizontal?.625f:.513888889f,-3,0};
            D3D11_VIEWPORT area=viewport;
            if(horizontal){const float height=360.f*500/800;area.TopLeftY=(740-height)*.5f/740*512;area.Height=height/740*512;}
            work->RSSetViewports(1,&area);work->UpdateSubresource(renderer.constants,0,nullptr,mapParameters,0,0);work->PSSetShaderResources(0,1,&map);work->Draw(3,0);work->RSSetViewports(1,&viewport);
        }else if(revision){work->UpdateSubresource(renderer.constants,0,nullptr,opaque,0,0);work->PSSetShaderResources(0,1,&base->snapshotView);work->Draw(3,0);}
        work->OMSetBlendState(renderer.blend,nullptr,0xffffffff);work->UpdateSubresource(renderer.constants,0,nullptr,alpha,0,0);work->PSSetShaderResources(0,1,&layerView);work->Draw(3,0);
        ID3D11ShaderResourceView* none=nullptr;work->PSSetShaderResources(0,1,&none);work->OMSetRenderTargets(0,nullptr,nullptr);
        bool complete=true;if(power.deferred){ID3D11CommandList* commands=nullptr;complete=SUCCEEDED(power.deferred->FinishCommandList(FALSE,&commands));if(complete){context->ExecuteCommandList(commands,TRUE);commands->Release();}}
        else{power.isolatedContext->SwapDeviceContextState(saved,nullptr);if(saved)saved->Release();context->SetPredication(predicate,predValue);if(predicate)predicate->Release();}
        if(complete){power.capture_base(context,now);composed=uploaded;composedPowerState=state;baseRevision=revision;dark=night;if(closing){const auto bounds=phone_app_bounds(1-double(page.elapsed)/pageCloseMs,waze);const float sx=power.width/360.f,sy=power.height/740.f;power.finish_page_close(context,bounds.x*sx,bounds.y*sy,bounds.w*sx,bounds.h*sy,float(14*double(page.elapsed)/pageCloseMs)*sx,now);}}
    }
};
}
