#include "../NavigationRuntime.h"
#include <Windows.h>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <filesystem>
#include <vector>
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    if(argc==7&&std::string(argv[1])=="--shield-smoke"){
        std::vector<wchar_t> self(32768);GetModuleFileNameW(nullptr,self.data(),static_cast<DWORD>(self.size()));const auto ini=std::filesystem::path(self.data()).parent_path()/L"BetterGPSOverlay.ini";
        Check(!std::filesystem::exists(ini),"Shield test would overwrite configuration");
        const wchar_t* keys[]={L"ShieldExporter",L"EngineExecutable",L"GameRoot",L"DocumentsRoot",L"ShieldDirectory"};
        for(int i=0;i<5;++i)WritePrivateProfileStringW(L"Host",keys[i],std::filesystem::absolute(argv[i+2]).c_str(),ini.c_str());
        const auto output=std::filesystem::absolute(argv[6]);std::filesystem::create_directories(output);std::filesystem::remove(output/"manifest.json");
        const auto start=GetTickCount64();bettergps_navigation::Poll(true);const auto launchMilliseconds=GetTickCount64()-start;
        bool ready=false;for(int i=0;i<150&&!ready;++i){Sleep(100);ready=std::filesystem::exists(output/"manifest.json");}
        bettergps_navigation::Stop();std::filesystem::remove(ini);
        Check(ready&&std::filesystem::exists(output/"I-95.dds"),"Automatic ATS shield refresh failed");
        std::cout<<"PASS: hidden ATS/mod shield refresh, nonblocking Poll "<<launchMilliseconds<<" ms, owned shutdown."<<std::endl;return 0;
    }
    if(argc==7&&std::string(argv[1])=="--host-smoke"){
        std::vector<wchar_t> self(32768);GetModuleFileNameW(nullptr,self.data(),static_cast<DWORD>(self.size()));const auto ini=std::filesystem::path(self.data()).parent_path()/L"BetterGPSOverlay.ini";
        Check(!std::filesystem::exists(ini),"Smoke test would overwrite configuration");
        const wchar_t* keys[]={L"EngineExecutable",L"GameRoot",L"DocumentsRoot",L"CacheDirectory",L"OutputDirectory"};
        for(int i=0;i<5;++i)WritePrivateProfileStringW(L"Host",keys[i],std::filesystem::absolute(argv[i+2]).c_str(),ini.c_str());
        WritePrivateProfileStringW(L"Host",L"CapturePipe",L"BetterGPSOverlay-smoke-v1",ini.c_str());
        bettergps_navigation::Start();bettergps_navigation::Poll(true);HANDLE pipe=INVALID_HANDLE_VALUE;
        for(int i=0;i<300&&pipe==INVALID_HANDLE_VALUE;++i){pipe=CreateFileW(L"\\\\.\\pipe\\BetterGPSOverlay-smoke-v1",GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(pipe==INVALID_HANDLE_VALUE)Sleep(100);}
        if(pipe==INVALID_HANDLE_VALUE){bettergps_navigation::Stop();std::filesystem::remove(ini);throw std::runtime_error("Real hidden GPS host did not reach its capture pipe");}
        const std::string hello=R"({"version":2,"type":"Hello","sessionId":"PHONE_SMOKE","sequence":0,"timestampMs":0,"generation":0,"status":"ATS_1_61_CAPTURE_V2","source":{"probeVersion":"0.3.0","gameBuild":"American Truck Simulator 1.61.3.1s","gpsManagerOffset":16680}})";
        uint32_t length=static_cast<uint32_t>(hello.size());DWORD written{};Check(WriteFile(pipe,&length,4,&written,nullptr)&&written==4,"Header write failed");Check(WriteFile(pipe,hello.data(),length,&written,nullptr)&&written==length,"Hello write failed");
        bool seen=false;for(int i=0;i<50&&!seen;++i){Sleep(100);bettergps_navigation::Poll(false);auto d=bettergps_navigation::Current();seen=d.live&&d.status=="WAITING_ROUTE"&&d.text=="Set a route in the game";}
        CloseHandle(pipe);bettergps_navigation::Stop();std::filesystem::remove(ini);Check(seen,"Real engine did not publish a decodable phone state");
        std::cout<<"PASS: real engine starts without a console, mounts game/mods, accepts native capture and feeds the phone mapping."<<std::endl;return 0;
    }
    // Stand in for the semantic worker to exercise hidden startup, argument
    // quoting and kill-on-close ownership without launching ATS or loading maps.
    if(argc>2&&std::string(argv[1])=="--capture"){
        std::string output;for(int i=1;i+1<argc;++i)if(std::string(argv[i])=="--output")output=argv[i+1];
        Check(!output.empty(),"Host output argument was lost");
        HANDLE marker=CreateMutexW(nullptr,FALSE,L"Local\\BetterGPSNavigation-host-v1");
        std::filesystem::create_directories(output);std::ofstream(std::filesystem::path(output)/"host-started.txt")<<GetCurrentProcessId()<<"\n"<<GetCommandLineA();
        while(true)Sleep(1000);
    }
    Check(argc==2,"Expected C# wire fixture path");std::array<uint8_t,4096> b{};std::ifstream in(argv[1],std::ios::binary);in.read(reinterpret_cast<char*>(b.data()),b.size());Check(in.gcount()==b.size(),"Wrong fixture length");
    int64_t utc{};std::memcpy(&utc,b.data()+16,8);
    auto d=bettergps_navigation::Decode(b.data(),b.size(),utc);
    Check(d.live&&d.instruction=="TURN_RIGHT"&&d.text.find("Navy Dr")!=std::string::npos&&d.text.find("then left")!=std::string::npos,"C# instruction/road/then did not survive native decoding");
    Check(!bettergps_navigation::Decode(b.data(),64,utc).live,"Short mapping accepted");
    Check(!bettergps_navigation::Decode(b.data(),b.size(),utc+3001).live,"Stale turn retained");
    Check(!bettergps_navigation::Decode(b.data(),b.size(),utc-1001).live,"Future timestamp accepted");
    auto broken=b;broken[4]=99;Check(!bettergps_navigation::Decode(broken.data(),broken.size(),utc).live,"Unknown version accepted");
    broken=b;broken[8]=3;Check(!bettergps_navigation::Decode(broken.data(),broken.size(),utc).live,"Torn snapshot accepted");
    broken=b;std::memset(broken.data()+2368,'X',512);Check(!bettergps_navigation::Decode(broken.data(),broken.size(),utc).live,"Unterminated text accepted");
    HANDLE map=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,4096,L"Local\\BetterGPSOverlayGuidance-v1");Check(map!=nullptr,"Create mapping failed");auto* p=MapViewOfFile(map,FILE_MAP_WRITE,0,0,4096);Check(p!=nullptr,"Map failed");
    FILETIME ft{};GetSystemTimeAsFileTime(&ft);ULARGE_INTEGER t{};t.LowPart=ft.dwLowDateTime;t.HighPart=ft.dwHighDateTime;int64_t now=static_cast<int64_t>(t.QuadPart/10000)-11644473600000LL;std::memcpy(b.data()+16,&now,8);std::memcpy(p,b.data(),b.size());
    bettergps_navigation::Poll(false);Check(bettergps_navigation::Current().text==d.text,"Production mapped reader differs from wire decoder");
    std::memset(static_cast<uint8_t*>(p)+8,0,8);bettergps_navigation::Poll(false);Check(!bettergps_navigation::Current().live,"Cleared mapping retained an old turn");
    bettergps_navigation::Stop();UnmapViewOfFile(p);CloseHandle(map);
    std::vector<wchar_t> exe(32768);GetModuleFileNameW(nullptr,exe.data(),static_cast<DWORD>(exe.size()));
    const auto directory=std::filesystem::path(exe.data()).parent_path(),ini=directory/L"BetterGPSOverlay.ini";
    Check(!std::filesystem::exists(ini),"Host test would overwrite an existing configuration");
    const auto output=std::filesystem::absolute(std::filesystem::path(argv[1]).parent_path()/"host lifecycle with spaces");std::filesystem::create_directories(output);std::filesystem::remove(output/"host-started.txt");
    WritePrivateProfileStringW(L"Host",L"EngineExecutable",exe.data(),ini.c_str());
    for(auto key:{L"GameRoot",L"DocumentsRoot",L"CacheDirectory",L"OutputDirectory"})WritePrivateProfileStringW(L"Host",key,output.c_str(),ini.c_str());
    bettergps_navigation::Start();bettergps_navigation::Poll(true);
    for(int i=0;i<100&&!std::filesystem::exists(output/"host-started.txt");++i)Sleep(50);
    Check(std::filesystem::exists(output/"host-started.txt"),"Automatic host did not start");
    DWORD pid{};std::ifstream(output/"host-started.txt")>>pid;HANDLE child=OpenProcess(SYNCHRONIZE,FALSE,pid);Check(child!=nullptr,"Worker handle unavailable");
    bettergps_navigation::Stop();Check(WaitForSingleObject(child,2000)==WAIT_OBJECT_0,"Worker survived plugin shutdown");CloseHandle(child);std::filesystem::remove(ini);
    std::cout<<"PASS: C# / C++ phone wire, stale/torn/cleared state, live mapping, hidden automatic host and owned shutdown."<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
