#pragma once
// Existing worker only: no extra thread, hooks, timer service or Android runtime.
// Fixed output gain, independent of the user-facing volume preference.
static int16_t desk_clock_output_sample(int16_t sample,int volume,int master=10000){return int16_t(int64_t(sample)*std::clamp(volume,0,100)*30*std::clamp(master,0,10000)/100000000LL);}
static bool desk_clock_power_ready(const ManagedDevice& d,int phase){return phase>=0&&(!d.startup||phase>=copilotBootMs);}
static void desk_clock_worker(uint64_t now){
 game_audio_poll(now);
 const auto game=vehicleStart.eligible?current_game_minutes():-1;
 const auto utc=game<0||!(gameTimezoneState.load()&1)?-1:int64_t(g_gameTimeMinutes.load())+g_multiplayerTimeOffset.load()-gameTimezoneReferenceMinutes.load();
 bettergps_deskclock::clock.step(now,game,utc,bettergps_deskclock::utc_ms(),game<0?-1:int64_t(g_gameTimeMinutes.load()));bettergps_deskclock::clock.save(now);
 bool visible=false;if(render_gpu_allowed())for(unsigned i=0;i<managedDeviceCount;++i){const auto d=managed_snapshot(i);if(d.used&&d.enabled&&d.vehicle==managed_vehicle()&&managed_is_copilot(d)&&d.style==3&&managed_power_target(i)&&desk_clock_power_ready(d,managedPower[i].phase.load())){visible=true;break;}}
 const auto v=bettergps_deskclock::clock.view();const int master=gameMasterLevel.load();const bool play=visible&&master>0&&bettergps_deskclock::clock.audible();const auto key=play?(v.alerts.front().timer?"timer:":"alarm:")+std::to_string(v.alerts.front().id)+":"+std::to_string(v.volume)+":"+std::to_string(master):std::string{};
 static std::string playing;static std::vector<char> wave;if(key!=playing){if(!playing.empty())PlaySoundW(nullptr,nullptr,0);playing=key;if(play){const auto sound=std::filesystem::u8path(g_assetRoot)/"material/copilot/t680/deskclock"/(v.alerts.front().timer?"timer.wav":"alarm.wav");std::ifstream f(sound,std::ios::binary);wave.assign(std::istreambuf_iterator<char>(f),{});bool pcm=false;if(wave.size()>44&&!memcmp(wave.data(),"RIFF",4)&&!memcmp(wave.data()+8,"WAVE",4)){for(size_t at=12;at+8<=wave.size();){uint32_t count=0;memcpy(&count,wave.data()+at+4,4);if(count>wave.size()-at-8)break;if(!memcmp(wave.data()+at,"fmt ",4)&&count>=16){uint16_t fmt=0,bits=0;memcpy(&fmt,wave.data()+at+8,2);memcpy(&bits,wave.data()+at+22,2);pcm=fmt==1&&bits==16;}if(!memcmp(wave.data()+at,"data",4)&&pcm){for(size_t n=at+8;n+1<at+8+count;n+=2){int16_t value=0;memcpy(&value,wave.data()+n,2);value=desk_clock_output_sample(value,v.volume,master);memcpy(wave.data()+n,&value,2);}}at+=8+count+(count&1);}}if(wave.empty()||!PlaySoundW(reinterpret_cast<LPCWSTR>(wave.data()),nullptr,SND_MEMORY|SND_ASYNC|SND_LOOP|SND_NODEFAULT))log(SCS_LOG_TYPE_warning,"[DeskClock] Cannot play sound %s",sound.u8string().c_str());}}
}
