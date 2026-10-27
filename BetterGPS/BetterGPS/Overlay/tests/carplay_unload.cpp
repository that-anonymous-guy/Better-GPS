#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <string>
int wmain(int argc,wchar_t** argv){
    if(argc==4&&std::wstring(argv[1])==L"--child"){
        for(unsigned i=0;i<5;++i){
            auto module=LoadLibraryExW(argv[2],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if(!module)return 2;
            auto decode=reinterpret_cast<bool(*)(const wchar_t*)>(GetProcAddress(module,"decode_carplay"));
            if(!decode||!decode(argv[3]))return 3;
            if(!FreeLibrary(module))return 4;
        }
        return 0;
    }
    if(argc!=3)return 1;
    wchar_t self[32768]{};GetModuleFileNameW(nullptr,self,32768);
    auto dll=std::filesystem::absolute(argv[1]).wstring(),assets=std::filesystem::absolute(argv[2]).wstring();
    std::wstring command=L"\""+std::wstring(self)+L"\" --child \""+dll+L"\" \""+assets+L"\"";
    STARTUPINFOW start{};start.cb=sizeof(start);PROCESS_INFORMATION process{};
    if(!CreateProcessW(self,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&start,&process))return 5;
    const auto wait=WaitForSingleObject(process.hProcess,15000);DWORD code=6;
    if(wait==WAIT_OBJECT_0)GetExitCodeProcess(process.hProcess,&code);
    else {TerminateProcess(process.hProcess,6);WaitForSingleObject(process.hProcess,1000);}
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    if(code){std::cerr<<"CarPlay decode/FreeLibrary regression: "<<code<<"\n";return int(code);}
    std::cout<<"PASS: 5 real DLL load/decode/FreeLibrary cycles using external DDS, with a bounded process-exit timeout.\n";
    return 0;
}
