#pragma once
namespace game_audio_fixture {
using namespace bettergps_overlay;
inline void run(const std::filesystem::path& out){
 std::filesystem::create_directories(out);
 Check(game_master_level(1,0)==10000&&game_master_level(.5,0)==5000&&game_master_level(0,0)==0&&game_master_level(.5,1)==0,"Master volume/mute mapping");
 Check(game_master_level(std::numeric_limits<double>::quiet_NaN(),0)==0&&game_master_level(-1,0)==0,"Invalid master gain accepted");
 std::ifstream f("H:/SteamLibrary/steamapps/common/American Truck Simulator/bin/win_x64/amtrucks.exe",std::ios::binary|std::ios::ate);Check(bool(f),"ATS executable missing");std::vector<uint8_t> raw(size_t(f.tellg()));f.seekg(0);f.read(reinterpret_cast<char*>(raw.data()),raw.size());const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(raw.data()+reinterpret_cast<const IMAGE_DOS_HEADER*>(raw.data())->e_lfanew);std::vector<uint8_t> image(nt->OptionalHeader.SizeOfImage);memcpy(image.data(),raw.data(),nt->OptionalHeader.SizeOfHeaders);for(auto section=IMAGE_FIRST_SECTION(nt);section<IMAGE_FIRST_SECTION(nt)+nt->FileHeader.NumberOfSections;++section)memcpy(image.data()+section->VirtualAddress,raw.data()+section->PointerToRawData,section->SizeOfRawData);
 const auto resolved=voice_resolve(image.data(),image.size());Check(resolved[4]&&resolved[5],"Native ATS master constructor resolution failed");
 std::array<uint8_t,0x130> volume{},mute{},profile{};memcpy(volume.data()+8,"s_master_volume",16);memcpy(mute.data()+8,"s_master_mute",14);
 auto set=[](auto& node,const char* text){memset(node.data()+0xb1,0,101);memcpy(node.data()+0xb1,text,strlen(text)+1);node[0xa1]=0;};set(volume,"1");set(mute,"0");
 int gain=0;double v=0,m=0;auto read=[&](){return game_audio_values(uintptr_t(volume.data()),uintptr_t(mute.data()),gain,v,m);};
 Check(read()&&gain==10000,"Root/default master unreadable");set(profile,"0.5");const auto overrideAt=uintptr_t(profile.data());memcpy(volume.data()+0x120,&overrideAt,8);Check(read()&&gain==5000,"Active profile override ignored");set(profile,"0.25");Check(read()&&gain==2500,"Live master changes not read");set(mute,"1");Check(read()&&gain==0,"Master mute ignored");set(mute,"0");Check(read()&&gain==2500,"Unmute did not restore current master");
 set(profile,"NaN");Check(!read()&&gain==0,"Nonfinite master retained stale audio");set(profile,"0.5");memcpy(profile.data()+0x120,&overrideAt,8);Check(!read()&&gain==0,"Cvar override cycle accepted");memset(profile.data()+0x120,0,8);memset(volume.data()+0x120,0,8);set(volume,"0.4");Check(read()&&gain==4000,"Profile removal retained previous master");
 voiceVariables[4]=uintptr_t(volume.data());voiceVariables[5]=uintptr_t(mute.data());game_audio_poll(1000);Check(gameMasterReady&&gameMasterLevel==4000,"Worker master state not published");set(volume,"0.5");game_audio_poll(1200);Check(gameMasterLevel==5000,"Worker did not follow live volume");set(mute,"1");game_audio_poll(1400);Check(gameMasterLevel==0,"Worker did not follow mute");voiceVariables[4]=0;game_audio_poll(1600);Check(!gameMasterReady&&gameMasterLevel==0,"Unavailable master became full volume");
 Check(desk_clock_output_sample(10000,50,10000)==1500&&desk_clock_output_sample(10000,50,5000)==750&&desk_clock_output_sample(-10000,50,5000)==-750&&desk_clock_output_sample(32767,100,10000)==9830,"PCM gain chain or full-scale overflow");Check(desk_clock_output_sample(10000,50,0)==0,"Zero master did not mute PCM");
 bettergps_deskclock::Clock defaults;defaults.init(out/"fresh",1000000);Check(defaults.view().volume==50,"Clock notifications must default to 50 percent");
 std::cout<<"PASS: actual ATS master/mute named constructors, profile overrides/live changes/removal, invalid/cyclic guards, worker state, 50% default, 30% output attenuation and master PCM multiplication.\n";
}
}
