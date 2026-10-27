#pragma once
#include <Windows.h>
#include <dxgi.h>
#include <atomic>
namespace bettergps_overlay {
// This gate controls only BetterGPS work. Never suppress or change an engine
// Present/Copy/Bind call: DXGI needs those calls to recover a hidden window.
inline std::atomic<bool> renderStopping{false};
inline std::atomic<bool> renderGameplayEnabled{false},renderGameplayActive{false};
inline bool render_gameplay_allowed(){return !renderGameplayEnabled.load(std::memory_order_relaxed)||renderGameplayActive.load(std::memory_order_relaxed);}
inline std::atomic<bool> renderActivityEnabled{false},renderForeground{true},renderVisible{true},renderOccluded{false},renderDeviceLost{false};
inline std::atomic<HWND> renderGameWindow{nullptr};
inline std::atomic<IDXGISwapChain*> renderMainChain{nullptr};
inline void (*renderActivityWake)()=nullptr;
inline void (*renderGameplaySuspend)()=nullptr;
inline bool render_gpu_allowed(){return !renderStopping.load(std::memory_order_relaxed)&&render_gameplay_allowed()&&(!renderActivityEnabled.load(std::memory_order_relaxed)||(renderVisible.load(std::memory_order_relaxed)&&!renderOccluded.load(std::memory_order_relaxed)&&!renderDeviceLost.load(std::memory_order_relaxed)));}
// A simulation pause can leave the cabin visible (F1 and driving panels).
// Sampling an existing completed screen requires no worker, upload or Draw.
inline bool render_cached_phone_allowed(){return !renderStopping.load(std::memory_order_relaxed)&&renderGameplayEnabled.load(std::memory_order_relaxed)&&!renderGameplayActive.load(std::memory_order_relaxed)&&(!renderActivityEnabled.load(std::memory_order_relaxed)||(renderVisible.load(std::memory_order_relaxed)&&!renderOccluded.load(std::memory_order_relaxed)&&!renderDeviceLost.load(std::memory_order_relaxed)));}
inline void render_set_foreground(bool foreground){if(renderForeground.exchange(foreground)!=foreground&&renderActivityWake)renderActivityWake();}
inline void render_set_visible(bool visible){if(renderVisible.exchange(visible)!=visible&&renderActivityWake)renderActivityWake();}
inline void render_window_message(HWND window,UINT message,WPARAM value){
 if(!renderActivityEnabled||window!=renderGameWindow)return;
 if((message==WM_CLOSE||message==WM_DESTROY)&&renderGameplaySuspend)renderGameplaySuspend();
 if(message==WM_ACTIVATEAPP)render_set_foreground(value!=0);
 if(message==WM_KILLFOCUS||message==WM_DESTROY)render_set_foreground(false);
 if(message==WM_SIZE)render_set_visible(value!=SIZE_MINIMIZED&&IsWindowVisible(window));
 if(message==WM_DESTROY)render_set_visible(false);
 // Focus controls input only. A visible game may keep rendering after Alt-Tab;
 // minimization, occlusion, lost devices and non-gameplay remain suspended.
}
inline bool render_window_candidate(HWND window){
 if(!window||!IsWindow(window)||GetAncestor(window,GA_ROOT)!=window)return false;DWORD process=0;GetWindowThreadProcessId(window,&process);RECT client{};
 return process==GetCurrentProcessId()&&GetClientRect(window,&client)&&client.right>=128&&client.bottom>=128;
}
inline bool render_bind_allowed(){
 if(renderActivityEnabled&&!renderVisible&&!renderOccluded&&!renderDeviceLost){const auto window=renderGameWindow.load();if(window&&IsWindowVisible(window)&&!IsIconic(window))render_set_visible(true);}
 return render_gpu_allowed();
}

inline bool render_window_active(HWND window){return window&&IsWindowVisible(window)&&!IsIconic(window)&&GetForegroundWindow()==window;}
inline void render_present_result(HRESULT result){
 if(result==DXGI_STATUS_OCCLUDED)renderOccluded=true;
 else if(result==S_OK)renderOccluded=false;
 if(result==DXGI_ERROR_DEVICE_REMOVED||result==DXGI_ERROR_DEVICE_RESET||result==DXGI_ERROR_DEVICE_HUNG||result==DXGI_ERROR_DRIVER_INTERNAL_ERROR)renderDeviceLost=true;
}
enum class RenderStage:unsigned {Idle,Preflight,Phone,Menu,NativePresent,Binding,IconPreparation,Copy};
// Fixed slots keep concurrent callbacks independent; a background callback must
// not hide a blocked Present or restore another thread's stale stage.
struct RenderTrace {
 std::atomic<DWORD> thread{0};
 std::atomic<RenderStage> stage{RenderStage::Idle};
 std::atomic<uint64_t> ticket{0},since{0};
};
inline RenderTrace renderTraces[8];
inline std::atomic<uint64_t> renderTicket{0};
inline thread_local RenderTrace* currentRenderTrace=nullptr;
inline void render_stage(RenderStage stage){if(auto* trace=currentRenderTrace){trace->since=GetTickCount64();trace->stage=stage;}}
struct RenderCall {
 RenderTrace* trace=currentRenderTrace;
 RenderStage previous=RenderStage::Idle;
 bool owner=false;
 explicit RenderCall(RenderStage stage=RenderStage::Preflight){
  if(!trace){for(auto& candidate:renderTraces){DWORD empty=0;if(candidate.thread.compare_exchange_strong(empty,GetCurrentThreadId())){trace=&candidate;currentRenderTrace=trace;owner=true;trace->ticket=++renderTicket;break;}}}
  if(trace){previous=trace->stage.load();render_stage(stage);}
 }
 ~RenderCall(){if(trace){render_stage(previous);if(owner){currentRenderTrace=nullptr;trace->thread=0;}}}
};
}
