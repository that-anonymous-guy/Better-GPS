#include <Windows.h>
#include "StorageMaintenance.h"
#include "NavigationRuntime.h"
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

namespace bettergps_navigation {
namespace {
constexpr size_t capacity=4096;
constexpr wchar_t mapping_name[]=L"Local\\BetterGPSOverlayGuidance-v1";
std::atomic<bool> stopping{false};
HANDLE mapping=nullptr, job=nullptr, worker=nullptr;
HANDLE shieldJob=nullptr,shieldWorker=nullptr;bool shieldStarted=false;
const uint8_t* view=nullptr;
std::mutex state_mutex;
Display current{"","Waiting for live guidance...","OFFLINE",false};
ULONGLONG last_open=0,last_launch=0;
int64_t UtcNow() { FILETIME ft{};GetSystemTimeAsFileTime(&ft);ULARGE_INTEGER i{};i.LowPart=ft.dwLowDateTime;i.HighPart=ft.dwHighDateTime;return static_cast<int64_t>(i.QuadPart/10000)-11644473600000LL; }
template<class T>T Read(const uint8_t* p,size_t offset){T v{};std::memcpy(&v,p+offset,sizeof(v));return v;}
std::string Text(const uint8_t* p,size_t offset,size_t length){auto* b=reinterpret_cast<const char*>(p+offset);auto* end=static_cast<const char*>(std::memchr(b,0,length));return end?std::string(b,end):std::string{};}
std::wstring Directory(){HMODULE self=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&Poll),&self);std::vector<wchar_t> b(32768);auto n=GetModuleFileNameW(self,b.data(),static_cast<DWORD>(b.size()));if(!n||n==b.size())return {};std::wstring s(b.data(),n);return s.substr(0,s.find_last_of(L"\\/"));}
std::wstring Setting(const wchar_t* name){std::vector<wchar_t>b(32768);const auto file=Directory()+L"\\BetterGPSOverlay.ini";auto n=GetPrivateProfileStringW(L"Host",name,L"",b.data(),static_cast<DWORD>(b.size()),file.c_str());return std::wstring(b.data(),n);}
std::wstring Quote(const std::wstring& p){if(p.empty()||p.find_first_of(L"\"\r\n")!=std::wstring::npos)return {};return L"\""+p+L"\"";}
void Launch(){
    if(stopping)return;
    const auto now=GetTickCount64();if(last_launch&&now-last_launch<30000)return;last_launch=now;
    if(worker){if(WaitForSingleObject(worker,0)==WAIT_TIMEOUT)return;CloseHandle(worker);worker=nullptr;}
    if(HANDLE existing=OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\BetterGPSNavigation-host-v1")){CloseHandle(existing);return;}
    const auto exe=Setting(L"EngineExecutable"),game=Setting(L"GameRoot"),docs=Setting(L"DocumentsRoot"),cache=Setting(L"CacheDirectory"),output=Setting(L"OutputDirectory");
    if(Quote(exe).empty()||Quote(game).empty()||Quote(docs).empty()||Quote(cache).empty()||Quote(output).empty()||GetFileAttributesW(exe.c_str())==INVALID_FILE_ATTRIBUTES)return;
    storage::maintain(cache,output,false);
    if(stopping)return;
    if(job){CloseHandle(job);job=nullptr;}job=CreateJobObjectW(nullptr,nullptr);if(!job)return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};info.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&info,sizeof(info))){CloseHandle(job);job=nullptr;return;}
    std::wstring cmd=Quote(exe)+L" --capture serve --compact-session --require-node-evidence --game-root "+Quote(game)+L" --documents "+Quote(docs)+L" --cache "+Quote(cache)+L" --output "+Quote(output);
    const auto pipe=Setting(L"CapturePipe");
    if(!pipe.empty()){
        if(pipe.size()>100||pipe.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_")!=std::wstring::npos)return;
        cmd+=L" --pipe "+Quote(pipe);
    }
    std::vector<wchar_t> buffer(cmd.begin(),cmd.end());buffer.push_back(0);STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),buffer.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED|BELOW_NORMAL_PRIORITY_CLASS,nullptr,nullptr,&si,&pi)){CloseHandle(job);job=nullptr;return;}
    if(!AssignProcessToJobObject(job,pi.hProcess)){TerminateProcess(pi.hProcess,2);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(job);job=nullptr;return;}
    worker=pi.hProcess;ResumeThread(pi.hThread);CloseHandle(pi.hThread);
}
}
void RefreshShields(){
    if(stopping||shieldStarted)return;const auto exporter=Setting(L"ShieldExporter"),engine=Setting(L"EngineExecutable"),game=Setting(L"GameRoot"),docs=Setting(L"DocumentsRoot"),output=ShieldDirectory();
    if(Quote(exporter).empty()||Quote(engine).empty()||Quote(game).empty()||Quote(docs).empty()||Quote(output).empty()||GetFileAttributesW(exporter.c_str())==INVALID_FILE_ATTRIBUTES)return;
    shieldStarted=true;shieldJob=CreateJobObjectW(nullptr,nullptr);if(!shieldJob)return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};info.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(shieldJob,JobObjectExtendedLimitInformation,&info,sizeof(info))){CloseHandle(shieldJob);shieldJob=nullptr;return;}
    const auto bin=engine.substr(0,engine.find_last_of(L"\\/"));std::wstring command=Quote(exporter)+L" "+Quote(bin)+L" "+Quote(game)+L" "+Quote(docs)+L" "+Quote(output)+L" --export-shields";
    std::vector<wchar_t> buffer(command.begin(),command.end());buffer.push_back(0);STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exporter.c_str(),buffer.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED|BELOW_NORMAL_PRIORITY_CLASS,nullptr,nullptr,&si,&pi)){CloseHandle(shieldJob);shieldJob=nullptr;return;}
    if(!AssignProcessToJobObject(shieldJob,pi.hProcess)){TerminateProcess(pi.hProcess,2);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(shieldJob);shieldJob=nullptr;return;}
    shieldWorker=pi.hProcess;ResumeThread(pi.hThread);CloseHandle(pi.hThread);
}
Display Decode(const uint8_t* p,size_t size,int64_t now){
    Display offline{"","Waiting for live guidance...","OFFLINE",false};
    if(!p||size<capacity||Read<uint32_t>(p,0)!=0x48504753||Read<uint32_t>(p,4)!=1)return offline;
    const auto sequence=Read<uint64_t>(p,8);if(!sequence||(sequence&1))return offline;
    const auto utc=Read<int64_t>(p,16);if(utc>now+1000||now-utc>3000)return offline;
    const auto flags=Read<uint32_t>(p,24);if(flags>1)return offline;
    Display d{Text(p,64,256),Text(p,2368,512),Text(p,1344,256),true};
    d.currentRoad=Text(p,320,256);d.targetRoad=Text(p,576,256);d.destination=Text(p,832,256);d.next=Text(p,1088,256);d.exit=Text(p,1856,256);
    if(d.text.empty()||d.status.empty())return offline;
    if(flags==0)d.instruction.clear();
    if(d.status=="STOPPED")d.live=false;
    return d;
}
void Poll(bool profile_ready){
    if(stopping)return;
    if(profile_ready){RefreshShields();Launch();}
    const auto now=GetTickCount64();
    if(!mapping&&(!last_open||now-last_open>1000)){
        last_open=now;mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,mapping_name);
        if(mapping){view=static_cast<const uint8_t*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,capacity));if(!view){CloseHandle(mapping);mapping=nullptr;}}
    }
    Display next{"","Waiting for live guidance...","OFFLINE",false};
    if(view){
        // Aligned 64-bit loads are atomic on the supported Windows x64 target.
        auto* sequence=reinterpret_cast<const volatile uint64_t*>(view+8);
        for(int attempt=0;attempt<3;++attempt){const auto before=*sequence;if(before&1)continue;MemoryBarrier();std::array<uint8_t,capacity> copy{};std::memcpy(copy.data(),view,capacity);MemoryBarrier();const auto after=*sequence;if(before==after&&!(after&1)){next=Decode(copy.data(),copy.size(),UtcNow());break;}}
        if(!next.live&&(!last_open||now-last_open>1000)){UnmapViewOfFile(view);view=nullptr;CloseHandle(mapping);mapping=nullptr;last_open=now;}
    }
    std::lock_guard<std::mutex> lock(state_mutex);current=std::move(next);
}
Display Current(){std::lock_guard<std::mutex> lock(state_mutex);return current;}
std::wstring ShieldDirectory(){static const auto path=[](){auto configured=Setting(L"ShieldDirectory");return configured.empty()?Directory()+L"\\BetterGPSOverlayShields":configured;}();return path;}
void Start(){stopping=false;}
void RequestStop(){stopping=true;}
void Stop(){
    RequestStop();
    // The caller has stopped polling. Closing the private jobs terminates only
    // our owned children. Do not wait for processes or scan storage on ATS's UI
    // thread while the game is tearing down its windows and graphics device.
    if(shieldJob){CloseHandle(shieldJob);shieldJob=nullptr;}
    if(shieldWorker){CloseHandle(shieldWorker);shieldWorker=nullptr;}shieldStarted=false;
    if(job){CloseHandle(job);job=nullptr;}
    if(worker){CloseHandle(worker);worker=nullptr;}
    if(view){UnmapViewOfFile(view);view=nullptr;}
    if(mapping){CloseHandle(mapping);mapping=nullptr;}
    last_open=last_launch=0;std::lock_guard<std::mutex> lock(state_mutex);current={"","Guidance offline","OFFLINE",false};
}
}
