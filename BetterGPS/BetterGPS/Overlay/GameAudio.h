#pragma once
#include "VoiceNavigation.h"
namespace bettergps_overlay {
// Basis points avoid floating-point work per PCM sample. Unknown sources mute
// plugin audio; never fall back to full volume after a failed/profile-stale read.
inline std::atomic<int> gameMasterLevel{0};
inline std::atomic<bool> gameMasterReady{false};
inline int game_master_level(double volume,double muted){
 if(!std::isfinite(volume)||!std::isfinite(muted)||volume<0||muted<0||muted>0)return 0;
 return int(std::lround(std::clamp(volume,0.,1.)*10000.));
}
inline bool game_audio_values(uintptr_t volumeRoot,uintptr_t muteRoot,int& level,double& volume,double& muted){
 level=0;volume=muted=-1;
 if(!voice_value(volumeRoot,"s_master_volume",volume)||!voice_value(muteRoot,"s_master_mute",muted))return false;
 if(volume<0||muted<0)return false;level=game_master_level(volume,muted);return true;
}
inline void game_audio_poll(uint64_t now){
 static uint64_t previous=0;static bool noted=false;if(previous&&now-previous<200)return;previous=now;
 int level=0;double volume=-1,muted=-1;const bool ready=game_audio_values(voiceVariables[4],voiceVariables[5],level,volume,muted);
 const auto old=gameMasterLevel.exchange(ready?level:0);const auto wasReady=gameMasterReady.exchange(ready);
 if(!noted||old!=level||wasReady!=ready){noted=true;log(ready?SCS_LOG_TYPE_message:SCS_LOG_TYPE_warning,"[GameAudio] native=%d master=%.4f mute=%.0f clock-gain=%.4f",ready,volume,muted,(ready?level:0)/10000.);}
}
}
