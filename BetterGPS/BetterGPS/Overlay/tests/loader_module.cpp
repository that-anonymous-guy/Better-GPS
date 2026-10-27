#include <scssdk_telemetry.h>
#include <cstring>
#include <Windows.h>
static scs_telemetry_init_params_v100_t sdk{};static int received;
static void SCSAPIFUNC channel(scs_string_t,scs_u32_t,const scs_value_t*,scs_context_t){++received;}
static void SCSAPIFUNC event(scs_event_t,const void*,scs_context_t){++received;}
#pragma comment(linker,"/export:scs_telemetry_init=scs_telemetry_init")
SCSAPI_RESULT scs_telemetry_init(scs_u32_t,const scs_telemetry_init_params_t* raw){sdk=*static_cast<const scs_telemetry_init_params_v100_t*>(raw);
 if(TEST_OWNER==0&&std::strcmp(sdk.common.game_name,"startup-slow-probe")==0)Sleep(40);
 auto result=sdk.register_for_channel("truck.speed",SCS_U32_NIL,SCS_VALUE_TYPE_float,SCS_TELEMETRY_CHANNEL_FLAG_no_value,channel,nullptr);if(result!=SCS_RESULT_ok)return result;
 result=sdk.register_for_event(SCS_TELEMETRY_EVENT_frame_start,event,nullptr);if(result!=SCS_RESULT_ok)return result;
 if(TEST_OWNER==1&&std::strcmp(sdk.common.game_name,"fail-overlay")==0)return SCS_RESULT_generic_error;return SCS_RESULT_ok;}
#pragma comment(linker,"/export:scs_telemetry_shutdown=scs_telemetry_shutdown")
SCSAPI_VOID scs_telemetry_shutdown(){sdk.unregister_from_channel("truck.speed",SCS_U32_NIL,SCS_VALUE_TYPE_float);sdk.unregister_from_event(SCS_TELEMETRY_EVENT_frame_start);}
extern "C" __declspec(dllexport) int received_count(){return received;}
