#pragma once
#include <Windows.h>
#include <atomic>
#include <array>
#include <deque>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <cstdarg>
#include <cstdio>
#include "TouchStatus.h"
namespace bettergps_overlay {
enum class TouchStage:unsigned {
 Press,Release,Cancelled,Timeout,Draw,NoBinding,Binding,GpuBusy,GpuInit,
 NoShader,NoMetadata,Reflection,Disassembly,NoCoordinates,Ambiguous,PickShader,
 Material,Depth,Replay,ReadbackBusy,ReadbackError,PixelMiss,StalePixel,UvOutside,
 Hit,GestureRejected,GestureQueued,ActionRejected,Action,Transfer,TransferResult,ProofPending,ProofRejected,Count
};
inline const char* touch_stage_name(TouchStage s){
 static constexpr const char* names[]={"press","release","cancelled","timeout","draw-observed","no-screen-binding","screen-binding","picker-busy","picker-init-failed","no-pixel-shader","shader-not-observed","reflection-failed","disassembly-failed","uv-unresolved","ambiguous-devices","pick-shader-failed","material","depth-or-target-unsupported","replay","readback-busy","readback-error","pixel-miss","stale-readback","uv-outside-content","surface-hit","gesture-rejected","gesture-queued","action-rejected","action","screen-transfer","screen-transfer-result","screen-proof-pending","screen-proof-rejected"};
 static_assert(sizeof(names)/sizeof(*names)==unsigned(TouchStage::Count));return names[unsigned(s)];
}
struct TouchDiagnosticEvent {uint64_t gesture=0,tick=0;TouchStage stage{};char detail[384]{};};
struct TouchDiagnosticMaterial {IUnknown* shader=nullptr;uint64_t gesture=0;unsigned slot=~0u,texture=~0u,context=0;char environment[256]{};};
inline const GUID kTouchEvidence={0x392b3412,0x9d0e,0x46a3,{0x8b,0x22,0x91,0x70,0x1e,0x9d,0xee,0x01}};
inline const GUID kTouchReported={0x392b3413,0x9d0e,0x46a3,{0x8b,0x22,0x91,0x70,0x1e,0x9d,0xee,0x01}};
inline const GUID kTouchInternalShader={0x392b3414,0x9d0e,0x46a3,{0x8b,0x22,0x91,0x70,0x1e,0x9d,0xee,0x01}};
inline std::atomic<size_t> touchEvidenceBytes{0};
struct TouchDiagnostics {
 static constexpr unsigned maxGestures=64,maxEvents=256,maxMaterials=12;
 std::atomic<uint64_t> current{0},sequence{0},seen{0};std::atomic<unsigned> dropped{0};
 std::mutex mutex;std::deque<TouchDiagnosticEvent> events;std::deque<TouchDiagnosticMaterial> materials;
 std::filesystem::path root;unsigned materialCount=0;bool opened=false;std::ofstream output;
 void init(const std::filesystem::path& base){root=base/"logs/touch";}
 void record(uint64_t id,TouchStage stage,const char* format,va_list args){
  if(!id||id!=current.load())return;
  const uint64_t bit=uint64_t(1)<<unsigned(stage);if(seen.fetch_or(bit)&bit)return;
  TouchDiagnosticEvent event;event.gesture=id;event.tick=GetTickCount64();event.stage=stage;
  vsnprintf(event.detail,sizeof(event.detail),format,args);
  touchStatus.note(id,touch_stage_name(stage),event.detail,stage==TouchStage::Action||stage==TouchStage::ActionRejected||stage==TouchStage::Timeout||stage==TouchStage::Cancelled||stage==TouchStage::GestureRejected||stage==TouchStage::PixelMiss);
  if(id>maxGestures)return; // The restartable HUD capture has its own independent budget.
  std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock||events.size()>=maxEvents){++dropped;return;}events.push_back(event);
 }
 void note(TouchStage stage,const char* format="",...){va_list args;va_start(args,format);record(current.load(),stage,format,args);va_end(args);}
 void note_for(uint64_t id,TouchStage stage,const char* format="",...){va_list args;va_start(args,format);record(id,stage,format,args);va_end(args);}
 uint64_t begin(POINT p,unsigned owner=4){const auto id=++sequence;current=id;seen=0;note(TouchStage::Press,"client=%ld,%ld owner=%s",p.x,p.y,TouchStatus::owner_name(owner));return id;}
 template<class Shader>void capture(Shader* shader,unsigned slot,unsigned texture,unsigned context,const char* environment=nullptr){
  if(!shader||!current||current>maxGestures)return;unsigned reported=0;UINT bytes=sizeof(reported);if(SUCCEEDED(shader->GetPrivateData(kTouchReported,&bytes,&reported))&&reported)return;
  std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock){++dropped;return;}if(materialCount>=maxMaterials)return;
  ++materialCount;reported=1;shader->SetPrivateData(kTouchReported,sizeof(reported),&reported);shader->AddRef();materials.push_back({shader,current.load(),slot,texture,context});if(environment)strncpy_s(materials.back().environment,environment,_TRUNCATE);
 }
 std::deque<TouchDiagnosticEvent> take(){std::lock_guard<std::mutex> lock(mutex);std::deque<TouchDiagnosticEvent> result;result.swap(events);return result;}
 std::deque<TouchDiagnosticMaterial> take_materials(){std::lock_guard<std::mutex> lock(mutex);std::deque<TouchDiagnosticMaterial> result;result.swap(materials);return result;}
 void open(){
  if(opened||root.empty())return;opened=true;std::error_code ec;std::filesystem::create_directories(root,ec);if(ec)return;
  if(std::filesystem::exists(root/"touch.log",ec))std::filesystem::copy_file(root/"touch.log",root/"touch.previous.log",std::filesystem::copy_options::overwrite_existing,ec);
  output.open(root/"touch.log",std::ios::trunc);if(output)output<<"BetterGPS touch diagnostics v1; bounded to 64 presses / 12 materials; timestamps are GetTickCount64 milliseconds\n";
 }
 void write(const TouchDiagnosticEvent& event){open();if(output)output<<"gesture="<<event.gesture<<" tick="<<event.tick<<" stage="<<touch_stage_name(event.stage)<<" "<<event.detail<<'\n';}
 void close(){for(auto& material:take_materials())material.shader->Release();if(output)output.flush();output.close();}
 ~TouchDiagnostics(){close();}
};
inline TouchDiagnostics touchDiagnostics;
}
