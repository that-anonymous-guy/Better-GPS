#pragma once
// Included inside bettergps_overlay. SDK callbacks only publish state; the
// existing worker follows the active profile in the game log, bounded I/O.
static std::atomic<bool> zonarJobActive{false};
static std::string zonarProfileName;
static uint64_t zonarProfileRevision=0;
static void zonar_job_configuration(const scs_telemetry_configuration_t* config){if(config&&config->id&&!strcmp(config->id,"job"))zonarJobActive=config->attributes&&config->attributes[0].name;}
static bool zonar_profile_line(const std::string& line,std::string& name){
 const auto at=line.find("New profile selected: '");if(at==std::string::npos)return false;const auto begin=at+23,end=line.find_last_of('\'');if(end<=begin||end-begin>512)return false;name=line.substr(begin,end-begin);return true;
}
static void zonar_profile_poll(uint64_t now){
 static uint64_t next=0,offset=0;static std::filesystem::path path;static std::string pending;if(now<next)return;next=now+1000;
 if(path.empty()){wchar_t configured[32768]{};const auto ini=managedBase/L"BetterGPSOverlay.ini";GetPrivateProfileStringW(L"Host",L"DocumentsRoot",L"",configured,32768,ini.c_str());if(*configured)path=std::filesystem::path(configured)/"game.log.txt";else{PWSTR documents=nullptr;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents,KF_FLAG_DONT_VERIFY,nullptr,&documents))&&documents)path=std::filesystem::path(documents)/"American Truck Simulator/game.log.txt";CoTaskMemFree(documents);}}
 std::ifstream file(path,std::ios::binary|std::ios::ate);const auto length=file.tellg();if(length<0)return;if(uint64_t(length)<offset){offset=0;pending.clear();if(!zonarProfileName.empty()){zonarProfileName.clear();++zonarProfileRevision;}}
 file.seekg(offset);char data[65536];file.read(data,sizeof(data));const size_t count=size_t(file.gcount());offset+=count;pending.append(data,count);size_t start=0,end=0;while((end=pending.find('\n',start))!=std::string::npos){std::string name;if(zonar_profile_line(pending.substr(start,end-start),name)&&name!=zonarProfileName){zonarProfileName=std::move(name);++zonarProfileRevision;}start=end+1;}pending.erase(0,start);if(pending.size()>2048)pending.clear();
}
