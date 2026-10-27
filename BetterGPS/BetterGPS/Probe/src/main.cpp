#include "probe.hpp"
#include "../../Shared/StartupTrace.h"

#include <Windows.h>
#include <memory>

namespace
{
    HMODULE g_module = nullptr;
    std::unique_ptr<bettergps::Probe> g_probe;
}

SCSAPI_RESULT scs_telemetry_init(
    const scs_u32_t version,
    const scs_telemetry_init_params_t* const raw_params)
{
    if (!raw_params || !g_module) return SCS_RESULT_invalid_parameter;

    if(version!=SCS_TELEMETRY_VERSION_1_00 && version!=SCS_TELEMETRY_VERSION_1_01) return SCS_RESULT_unsupported;
    const auto* params = static_cast<const scs_telemetry_init_params_v100_t*>(raw_params);
    bettergps::StartupTrace trace(params->common.log,"probe-entry");
    try {
    g_probe = trace.measure("construct-probe",[&]{return std::make_unique<bettergps::Probe>(g_module);});
    if (!trace.measure("initialize-probe",[&]{return g_probe->Initialize(version, params);}))
    {
        g_probe.reset();
        return SCS_RESULT_generic_error;
    }
    return SCS_RESULT_ok;
    } catch(...) { g_probe.reset(); return SCS_RESULT_generic_error; }
}

SCSAPI_VOID scs_telemetry_shutdown()
{
    if (g_probe)
    {
        g_probe->Shutdown();
        g_probe.reset();
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
