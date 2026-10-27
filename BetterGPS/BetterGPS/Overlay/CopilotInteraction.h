#pragma once
#include <array>
#include <mutex>
#include "DeviceLimits.h"
namespace bettergps_overlay {
inline constexpr uint64_t copilotSlideMs=320,copilotMapWaitMs=3000,copilotMenuMs=260,copilotOpenDelayMs=500,copilotCloseDelayMs=250;
inline uint64_t copilot_motion_elapsed(uint64_t elapsed,bool opening){const auto delay=opening?copilotOpenDelayMs:copilotCloseDelayMs;return elapsed>delay?elapsed-delay:0;}
// The status DDS ends at 63px but its opaque band ends at 51px. Home's
// page dots sit behind the transparent tail and must travel with the app.
inline constexpr int copilotAppTop=51,copilotMapTop=63;
inline double copilot_ease(double t){t=std::clamp(t,0.,1.);return 1-(1-t)*(1-t)*(1-t);}
struct CopilotMenuVisual {double progress=0;bool target=false;uint64_t revision=0;bool blocked()const{return target||progress>0;}};
struct CopilotMenuRuntime {
 struct State {CopilotMenuVisual visual;double from=0;uint64_t origin=0,epoch=0,power=0;};
 std::mutex mutex;std::array<State,managedDeviceCount> states{};
 CopilotMenuVisual get(unsigned slot){std::lock_guard<std::mutex> lock(mutex);return slot<states.size()?states[slot].visual:CopilotMenuVisual{};}
 void request(unsigned slot,bool open,uint64_t now){if(slot>=states.size())return;std::lock_guard<std::mutex> lock(mutex);auto& s=states[slot];if(s.visual.target==open)return;s.from=s.visual.progress;s.visual.target=open;s.origin=now;++s.visual.revision;}
 CopilotMenuVisual sample(unsigned slot,bool eligible,uint64_t epoch,uint64_t power,uint64_t now){
  std::lock_guard<std::mutex> lock(mutex);auto& s=states[slot];if(s.epoch!=epoch||s.power!=power||(!eligible&&s.visual.blocked())){const auto revision=s.visual.revision+1;s={};s.epoch=epoch;s.power=power;s.visual.revision=revision;return s.visual;}if(!eligible)return s.visual;
  const double destination=s.visual.target?1:0,t=now>=s.origin?double(now-s.origin)/copilotMenuMs:0;s.visual.progress=s.from+(destination-s.from)*copilot_ease(t);return s.visual;
 }
};
inline CopilotMenuRuntime copilotMenu;
}
