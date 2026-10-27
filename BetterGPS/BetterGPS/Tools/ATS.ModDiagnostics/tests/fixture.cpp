#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <filesystem>
#include <vector>
#include <cstdlib>
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 1;
    auto window=CreateWindowExW(0,L"STATIC",L"ATS diagnostic fixture",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 2;
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=1;desc.BufferDesc.Width=64;desc.BufferDesc.Height=64;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=window;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain* chain{};ID3D11Device* device{};ID3D11DeviceContext* context{};D3D_FEATURE_LEVEL level{};
    if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&device,&level,&context)))return 3;
    auto file=CreateFileW(argv[1],GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Sleep(2000);auto start=GetTickCount64();int frame=0;std::vector<char> buffer(65536);
    while(GetTickCount64()-start<static_cast<ULONGLONG>(_wtoi(argv[2]))*1000){
        if(file!=INVALID_HANDLE_VALUE){SetFilePointer(file,0,nullptr,FILE_BEGIN);DWORD got{};while(ReadFile(file,buffer.data(),static_cast<DWORD>(buffer.size()),&got,nullptr)&&got){} }
        chain->Present(0,0);Sleep(++frame%100==0?80:16);
    }
    if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    context->Release();device->Release();chain->Release();DestroyWindow(window);return 0;
}
