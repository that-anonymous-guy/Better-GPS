#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
namespace bettergps_camera {
enum class View : unsigned {Forward=0,Left=1,Right=2,Reverse=3};
inline constexpr unsigned bit(View v){return 1u<<unsigned(v);}
struct Pose {std::array<float,3> position{};std::array<float,4> rotation{1,0,0,0};};
struct Snapshot {
 unsigned available=0;View selected=View::Forward;bool known=false,active=false,reverse=false,left=false,right=false;
 uint64_t epoch=0,revision=0,selection=0;std::array<Pose,4> poses{};
 bool playable()const{return active&&known&&(available&bit(selected));}
};
class State {
 std::mutex mutex;Snapshot state;bool previousReverse=false,previousLeft=false,previousRight=false,wasActive=false;
public:
 void reset(uint64_t epoch){std::lock_guard<std::mutex> lock(mutex);auto revision=state.revision+1;state={};state.epoch=epoch;state.revision=revision;previousReverse=previousLeft=previousRight=wasActive=false;}
 Snapshot snapshot(){std::lock_guard<std::mutex> lock(mutex);return state;}
 void hardware(uint64_t epoch,unsigned mask,const std::array<Pose,4>& poses,bool known=true){
  std::lock_guard<std::mutex> lock(mutex);if(state.epoch!=epoch){const auto revision=state.revision+1;state={};state.epoch=epoch;state.revision=revision;previousReverse=previousLeft=previousRight=wasActive=false;}
  if(state.available!=(mask&15)||state.known!=known){state.available=mask&15;state.known=known;++state.revision;++state.selection;}
  state.poses=poses;
 }
 void sample(bool active,bool reverse,bool left,bool right){
  std::lock_guard<std::mutex> lock(mutex);state.reverse=reverse;state.left=left;state.right=right;
  const bool opening=active&&!wasActive;
  const bool triggered=active&&(reverse!=previousReverse||left!=previousLeft||right!=previousRight);
  const bool activeChanged=state.active!=active;state.active=active;
  if(opening||triggered){const auto selected=reverse?View::Reverse:left&&!right?View::Left:right&&!left?View::Right:View::Forward;
   // Entry uses the current automatic trigger, or Forward. Missing Forward
   // stays RGB(10,10,10), never silently falls back to another camera.
   state.selected=selected;++state.revision;++state.selection;
  }else if(activeChanged)++state.revision;
  previousReverse=reverse;previousLeft=left;previousRight=right;wasActive=active;
 }
 bool select(View view){std::lock_guard<std::mutex> lock(mutex);if(!state.active||!state.known||!(state.available&bit(view)))return false;state.selected=view;++state.revision;++state.selection;return true;}
};
// A source-pose update is not a completed native video frame. Readiness is
// granted only after two submitted frames have crossed Present for this view.
struct VideoFrames {
 uint64_t epoch=0,selection=0;unsigned frames=0;
 bool present(uint64_t e,uint64_t s,bool submitted){if(epoch!=e||selection!=s){epoch=e;selection=s;frames=0;}if(!submitted)return e&&frames>=2;if(frames<2)++frames;return e&&frames>=2;}
};
inline State cameras;
inline std::atomic<uint64_t> renderEpoch{0},renderSelection{0};
// Published only after a validated native producer has the selected pose.
inline bool feed_ready(const Snapshot& s){return s.playable()&&s.epoch&&renderEpoch.load()==s.epoch&&renderSelection.load()==s.selection;}
inline std::atomic<bool> reverseGear{false},leftIndicator{false},rightIndicator{false};
inline constexpr std::array<float,4> horizontalFov{100,100,100,100};
inline constexpr std::array<float,4> yawDegrees{0,115,245,180};
inline constexpr std::array<float,4> downDegrees{5,0,0,30};
}
