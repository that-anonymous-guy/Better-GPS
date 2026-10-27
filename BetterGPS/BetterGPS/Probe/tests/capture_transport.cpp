#include "live_capture.hpp"
#include "scs_sdk_min.hpp"
#include "route_publication.hpp"
#include <iostream>
#include <cmath>
static void SCSAPIFUNC TestLog(scs_log_type_t,const char*) { }
static scs_result_t SCSAPIFUNC TestRegister(scs_event_t,scs_telemetry_event_callback_t,scs_context_t) { return SCS_RESULT_ok; }
int main(int argc,char** argv) {
    try {
        wchar_t module_path[32768]{}; GetModuleFileNameW(nullptr,module_path,32768);
        std::wstring path=module_path; path=path.substr(0,path.find_last_of(L"\\/"))+L"\\BetterGPSProbe.dll";
        HMODULE dll=LoadLibraryW(path.c_str()); if(!dll) return 10;
        using Init=scs_result_t(SCSAPIFUNC*)(scs_u32_t,const scs_telemetry_init_params_t*);
        using Shutdown=void(SCSAPIFUNC*)();
        auto init=reinterpret_cast<Init>(GetProcAddress(dll,"scs_telemetry_init"));
        auto shutdown=reinterpret_cast<Shutdown>(GetProcAddress(dll,"scs_telemetry_shutdown"));
        if(!init || !shutdown) return 11;
        scs_telemetry_init_params_v100_t params{};
        if(init(SCS_TELEMETRY_VERSION_1_01,nullptr)!=SCS_RESULT_invalid_parameter) return 12;
        if(init(0xFFFFFFFF,&params)!=SCS_RESULT_unsupported) return 13;
        params.common.log=&TestLog; params.common.game_id="ats"; params.common.game_name="American Truck Simulator 1.62.0";
        params.register_for_event=&TestRegister;
        if(init(SCS_TELEMETRY_VERSION_1_01,&params)!=SCS_RESULT_generic_error) return 14;
        shutdown(); shutdown(); FreeLibrary(dll);
        bettergps::LiveCapture capture;
        if(argc>1){std::string name=argv[1];capture.SetPipeForTest(L"\\\\.\\pipe\\"+std::wstring(name.begin(),name.end()));}
        auto route=std::make_shared<bettergps::CaptureRoute>(); route->fingerprint=1;
        route->physical={{1,0,100,0,10},{2,100,0,10,0}};
        route->logical={{0,0,1,0,100,0,10},{0,1,2,100,0,10,0}};
        bettergps::CaptureNode node;node.status="READ_OK";node.backward_valid=true;node.forward_valid=true;
        route->nodes={node,node};
        bettergps::CaptureControl control; control.type="RawRouteSnapshot"; control.generation=1; control.route=route;
        if(!capture.Publish(control)) return 1;
        std::uint64_t pending=1,accepted=1;bool valid=true;unsigned long long generation=1;
        capture.Start();
        for(int i=0;i<30;i++) {
            if(i>=10&&i<=12){const auto decision=bettergps::DecideRoutePublication(i==10?2:1,accepted,pending,valid);
                if(decision==bettergps::RoutePublication::Stabilize&&valid){bettergps::CaptureControl c;c.generation=++generation;c.timestamp=GetTickCount64();c.status="RECALCULATING";if(!capture.Publish(c))return 20;valid=false;}
                if(decision==bettergps::RoutePublication::Publish){control.generation=++generation;control.timestamp=GetTickCount64();if(!capture.Publish(control))return 21;valid=true;}
            }
            bettergps::CaptureVehicle v; v.timestamp=GetTickCount64(); v.generation=1;
            v.generation=generation;
            v.pose_valid=true; v.speed_valid=true; v.paused=false; v.heading=0.75f; v.speed=10; v.x=i;
            capture.Publish(v); Sleep(100);
        }
        auto before=GetTickCount64(); capture.Stop();
        if(GetTickCount64()-before>1000) return 2;
        // A DLL without MapBridge must also start/stop without waiting for a pipe.
        bettergps::LiveCapture disconnected; disconnected.Start(); Sleep(30); disconnected.Stop();
        std::cout << "PASS native transport, stop and SDK ABI\n"; return 0;
    } catch(const std::exception& e) { std::cerr << e.what(); return 3; }
}
