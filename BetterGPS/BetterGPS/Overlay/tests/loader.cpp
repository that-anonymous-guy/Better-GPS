#include <Windows.h>
#include <scssdk_telemetry.h>
#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
static std::vector<std::string> logs;
static void SCSAPIFUNC gameLog(scs_log_type_t,scs_string_t message){logs.emplace_back(message);}
static void check(bool test){if(!test)throw std::runtime_error("Nested loader regression");}
static scs_telemetry_channel_callback_t channel;static scs_context_t channelContext;
static scs_telemetry_event_callback_t event;static scs_context_t eventContext;static int registrations;
static scs_result_t SCSAPIFUNC reg(scs_string_t,scs_u32_t,scs_value_type_t,scs_u32_t,scs_telemetry_channel_callback_t cb,scs_context_t ctx){check(!channel);channel=cb;channelContext=ctx;++registrations;return SCS_RESULT_ok;}
static scs_result_t SCSAPIFUNC unreg(scs_string_t,scs_u32_t,scs_value_type_t){channel=nullptr;return SCS_RESULT_ok;}
static scs_result_t SCSAPIFUNC regEvent(scs_event_t,scs_telemetry_event_callback_t cb,scs_context_t ctx){check(!event);event=cb;eventContext=ctx;return SCS_RESULT_ok;}
static scs_result_t SCSAPIFUNC unregEvent(scs_event_t){event=nullptr;return SCS_RESULT_ok;}
int main(int argc,char** argv){try{
 check(argc==2);namespace fs=std::filesystem;auto output=fs::absolute(argv[1]);fs::create_directories(output/"BetterGPS");wchar_t self[32768]{};GetModuleFileNameW(nullptr,self,32768);auto bin=fs::path(self).parent_path();
 for(auto pair:{std::pair{"BetterGPS.dll","BetterGPS.dll"},std::pair{"FakeProbe.dll","BetterGPS/BetterGPSProbe.dll"},std::pair{"FakeOverlay.dll","BetterGPS/BetterGPSOverlay.dll"}})fs::copy_file(bin/pair.first,output/pair.second,fs::copy_options::overwrite_existing);
 auto loader=LoadLibraryExW((output/"BetterGPS.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);check(loader);
 auto init=(scs_result_t(SCSAPIFUNC*)(scs_u32_t,const scs_telemetry_init_params_t*))GetProcAddress(loader,"scs_telemetry_init");auto stop=(void(SCSAPIFUNC*)())GetProcAddress(loader,"scs_telemetry_shutdown");check(init&&stop);
 scs_telemetry_init_params_v100_t sdk{};sdk.common.game_name="startup-slow-probe";sdk.common.log=gameLog;sdk.register_for_channel=reg;sdk.unregister_from_channel=unreg;sdk.register_for_event=regEvent;sdk.unregister_from_event=unregEvent;
 check(init(0xFFFF,&sdk)==SCS_RESULT_unsupported&&!channel);check(init(SCS_TELEMETRY_VERSION_1_01,&sdk)==SCS_RESULT_ok&&registrations==1);
 for(auto phase:{"load-probe-dll","load-overlay-dll","probe-sdk-init","overlay-sdk-init"}){bool begin=false,end=false;for(const auto& line:logs)if(line.find(std::string("phase=")+phase+" ")!=std::string::npos){if(line.find("event=begin ")!=std::string::npos)begin=true;if(line.find("event=end ")!=std::string::npos){check(begin);end=true;if(std::string(phase)=="probe-sdk-init"){auto pos=line.find("wall_ms=");check(pos!=std::string::npos&&std::strtod(line.c_str()+pos+8,nullptr)>=25.0);}}}check(begin&&end);}
 scs_value_t value{};channel("truck.speed",SCS_U32_NIL,&value,channelContext);event(SCS_TELEMETRY_EVENT_frame_start,nullptr,eventContext);
 for(auto name:{L"BetterGPSProbe.dll",L"BetterGPSOverlay.dll"}){auto module=GetModuleHandleW(name);check(module);auto count=(int(*)())GetProcAddress(module,"received_count");check(count&&count()==2);}
 stop();check(!channel&&!event&&!GetModuleHandleW(L"BetterGPSProbe.dll")&&!GetModuleHandleW(L"BetterGPSOverlay.dll"));
 sdk.common.game_name="fail-overlay";check(init(SCS_TELEMETRY_VERSION_1_00,&sdk)==SCS_RESULT_generic_error);check(!channel&&!event&&!GetModuleHandleW(L"BetterGPSProbe.dll"));
 fs::remove(output/"BetterGPS/BetterGPSOverlay.dll");sdk.common.game_name="test";check(init(SCS_TELEMETRY_VERSION_1_01,&sdk)==SCS_RESULT_generic_error&&!channel&&!event);FreeLibrary(loader);
 std::cout<<"PASS: actual loader DLL loads both nested payloads, fans out SDK callbacks, handles initialization rollback, missing payload and both API versions; startup begin/end trace measures injected 40ms Probe delay."<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
