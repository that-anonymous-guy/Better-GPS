#pragma once
#include <Windows.h>
#include <scssdk.h>
#include <cstdio>

namespace bettergps {
// Startup-thread diagnostics only. No files, workers, waits or DllMain work.
// QPC is independent of the game's cached log timestamps. Preserve last-error
// so the trace cannot change a caller's LoadLibrary/GetProcAddress diagnostics.
class StartupTrace {
    struct Stamp { LONGLONG wall{}, cpu{}; };
    scs_log_t log_;
    const char* component_;
    LONGLONG frequency_{};
    Stamp origin_;
    static Stamp stamp() noexcept {
        const DWORD error=GetLastError();
        LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
        FILETIME created{},exited{},kernel{},user{};
        const bool valid=GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)!=FALSE;
        auto ticks=[](FILETIME t){return (LONGLONG(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
        const Stamp result{now.QuadPart,valid?ticks(kernel)+ticks(user):-1};
        SetLastError(error); return result;
    }
    void emit(const char* event,const char* phase,Stamp start,Stamp now) const noexcept {
        if(!log_)return;
        const DWORD error=GetLastError(); char message[640]{};
        const double cpu=start.cpu<0||now.cpu<0?-1.0:double(now.cpu-start.cpu)/10000.0;
        std::snprintf(message,sizeof(message),"[BetterGPS] [StartupTrace] component=%s event=%s phase=%s wall_ms=%.3f cpu_ms=%.3f elapsed_ms=%.3f qpc_ms=%.3f thread=%lu",
            component_,event,phase,double(now.wall-start.wall)*1000.0/frequency_,cpu,
            double(now.wall-origin_.wall)*1000.0/frequency_,double(now.wall)*1000.0/frequency_,GetCurrentThreadId());
        log_(SCS_LOG_TYPE_message,message); SetLastError(error);
    }
public:
    StartupTrace(scs_log_t log,const char* component) noexcept:log_(log),component_(component) {
        const DWORD error=GetLastError(); LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
        frequency_=frequency.QuadPart?frequency.QuadPart:1; origin_=stamp();
        emit("enter","sdk-init",origin_,origin_); SetLastError(error);
    }
    ~StartupTrace(){emit("leave","sdk-init",origin_,stamp());}
    StartupTrace(const StartupTrace&)=delete;
    class Phase {
        const StartupTrace& trace_; const char* name_; Stamp start_;
    public:
        Phase(const StartupTrace& trace,const char* name):trace_(trace),name_(name),start_(stamp()){trace_.emit("begin",name_,start_,start_);}
        ~Phase(){trace_.emit("end",name_,start_,stamp());}
        Phase(const Phase&)=delete;
    };
    template<class F> decltype(auto) measure(const char* name,F&& call) const {Phase phase(*this,name);return call();}
};
}
