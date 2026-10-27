#include <Windows.h>
#include <filesystem>
#include "SdkMultiplexer.h"
#include "../Shared/StartupTrace.h"
using Init=scs_result_t(SCSAPIFUNC*)(scs_u32_t,const scs_telemetry_init_params_t*);
using Stop=void(SCSAPIFUNC*)();
static HMODULE modules[2]{};static Stop stops[2]{};static bool started[2]{};
static void shutdown_modules(){
    for(int i=1;i>=0;--i)if(started[i]){stops[i]();started[i]=false;}
    bettergps::SdkMultiplexer::reset();
    for(int i=1;i>=0;--i){if(modules[i])FreeLibrary(modules[i]);modules[i]=nullptr;stops[i]=nullptr;}
}
#pragma comment(linker, "/export:scs_telemetry_init=scs_telemetry_init")
SCSAPI_RESULT scs_telemetry_init(scs_u32_t version,const scs_telemetry_init_params_t* params){
    if(!params)return SCS_RESULT_invalid_parameter;
    if(version!=SCS_TELEMETRY_VERSION_1_00&&version!=SCS_TELEMETRY_VERSION_1_01)return SCS_RESULT_unsupported;
    auto* sdk=static_cast<const scs_telemetry_init_params_v100_t*>(params);
    bettergps::StartupTrace trace(sdk->common.log,"loader");
    if(!sdk->register_for_channel||!sdk->unregister_from_channel||!sdk->register_for_event||!sdk->unregister_from_event)return SCS_RESULT_invalid_parameter;
    bettergps::SdkMultiplexer::original=*sdk;
    HMODULE self{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)&scs_telemetry_init,&self);
    wchar_t path[32768]{};if(!GetModuleFileNameW(self,path,32768))return SCS_RESULT_generic_error;
    auto directory=std::filesystem::path(path).parent_path()/L"BetterGPS";
    const wchar_t* names[]={L"BetterGPSProbe.dll",L"BetterGPSOverlay.dll"};
    for(unsigned i=0;i<2;++i){
        modules[i]=trace.measure(i==0?"load-probe-dll":"load-overlay-dll",[&]{return LoadLibraryExW((directory/names[i]).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);});
        bettergps::StartupTrace::Phase exports(trace,i==0?"probe-exports-and-init":"overlay-exports-and-init");
        auto init=modules[i]?(Init)GetProcAddress(modules[i],"scs_telemetry_init"):nullptr;
        stops[i]=modules[i]?(Stop)GetProcAddress(modules[i],"scs_telemetry_shutdown"):nullptr;
        if(!init||!stops[i]){if(sdk->common.log)sdk->common.log(SCS_LOG_TYPE_error,"[BetterGPS] Required internal DLL is missing or could not be loaded.");shutdown_modules();return SCS_RESULT_generic_error;}
        auto delegated=i==0?bettergps::SdkMultiplexer::parameters<0>():bettergps::SdkMultiplexer::parameters<1>();
        auto result=trace.measure(i==0?"probe-sdk-init":"overlay-sdk-init",[&]{return init(version,&delegated);});
        if(result!=SCS_RESULT_ok){stops[i]();shutdown_modules();return result;}
        started[i]=true;
    }
    if(sdk->common.log)sdk->common.log(SCS_LOG_TYPE_message,"[BetterGPS] Probe and overlay loaded from plugins/BetterGPS.");
    return SCS_RESULT_ok;
}
#pragma comment(linker, "/export:scs_telemetry_shutdown=scs_telemetry_shutdown")
SCSAPI_VOID scs_telemetry_shutdown(){shutdown_modules();}
