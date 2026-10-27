#include <Windows.h>
#include <winioctl.h>
#include "../StorageMaintenance.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
static void check(bool test){if(!test)throw std::runtime_error("Storage lifecycle regression");}
static void sparse(const std::filesystem::path& path,uint64_t size){
 std::filesystem::create_directories(path.parent_path());auto file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);check(file!=INVALID_HANDLE_VALUE);
 DWORD done{};check(DeviceIoControl(file,FSCTL_SET_SPARSE,nullptr,0,nullptr,0,&done,nullptr));LARGE_INTEGER end{};end.QuadPart=size;check(SetFilePointerEx(file,end,nullptr,FILE_BEGIN)&&SetEndOfFile(file));CloseHandle(file);
}
int main(int argc,char** argv){try{
 check(argc==2);namespace fs=std::filesystem;auto root=fs::absolute(argv[1]);auto cache=root/"cache",logs=root/"logs";fs::create_directories(cache/"live-route-evidence");std::ofstream(cache/"live-route-evidence/test.json")<<"expired route";std::ofstream(cache/"user-note.txt")<<"preserve";
 auto older=cache/"world-v1/map/old/0_0.wgc",newer=cache/"world-v1/map/current/1_0.wgc";
 sparse(older,400ull*1024*1024);sparse(newer,300ull*1024*1024);fs::last_write_time(older,fs::file_time_type::clock::now()-std::chrono::hours(24));
 auto oldLog=logs/"20261013_000000_aaaaaaaa",recentLog=logs/"20261013_010000_bbbbbbbb";
 sparse(oldLog/"guidance.jsonl",40ull*1024*1024);sparse(recentLog/"guidance.jsonl",5ull*1024*1024);fs::last_write_time(oldLog,fs::file_time_type::clock::now()-std::chrono::hours(24));
 fs::create_directories(logs/"user-directory");std::ofstream(logs/"user-directory/note.txt")<<"preserve";
 bettergps_navigation::storage::maintain(cache,logs);
 check(!fs::exists(cache/"live-route-evidence")&&!fs::exists(older)&&fs::exists(newer)&&fs::exists(cache/"user-note.txt"));
 check(!fs::exists(oldLog)&&fs::exists(recentLog)&&fs::exists(logs/"user-directory/note.txt"));
 check(bettergps_navigation::storage::size(cache)<=640ull*1024*1024&&bettergps_navigation::storage::size(logs)<=32ull*1024*1024);
 std::cout<<"PASS: closed-session evidence cleanup, oldest world-page eviction, bounded session history, unknown user files preserved."<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
