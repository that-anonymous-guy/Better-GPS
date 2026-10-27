#pragma once
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cwchar>
// Fixed-size, opt-in diagnostics. No files, heap allocations or waits in Record/Scope.
namespace bettergps_diag {
enum Metric:unsigned { FrameInterval,PresentWait,OverlayPresent,TextureBinding,TextureUpload,UploadMap,MenuUpload,ComposePhone,ComposeMenu,HostPoll,SettingsSave,ProbeTick,ReadRoute,FrameCallback,PacketSerialize,PipeWrite,MapSourcePrepare=27,MetricCount };
constexpr unsigned metricSlots=32,bucketCount=24,ringCount=1024,mapBytes=65536;
struct Control {uint32_t magic,version;volatile LONG64 heartbeat;LONG64 frequency;volatile LONG enabled;LONG reserved;volatile LONG64 session;};
struct MetricData {volatile LONG64 count,ticks,maximum;volatile LONG64 buckets[bucketCount];};
struct Event {volatile LONG64 sequence;LONG64 qpc,ticks;uint32_t metric,thread;uint64_t value;};
struct Header {uint32_t magic,version,pid,component;LONG64 frequency;volatile LONG64 write;uint8_t reserved[96];};
static_assert(sizeof(Header)==128&&sizeof(MetricData)==216&&sizeof(Event)==40);
struct State {HANDLE controlHandle{},dataHandle{};Control* control{};Header* header{};std::atomic<bool> active{false};std::atomic<LONG64> session{0};};
inline State state;
inline LONG64 Now(){LARGE_INTEGER t;QueryPerformanceCounter(&t);return t.QuadPart;}
inline void Init(unsigned component){if(state.header)return;wchar_t name[128];swprintf_s(name,L"Local\\BetterGPS-Diagnostics-Control-%lu",GetCurrentProcessId());state.controlHandle=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,4096,name);if(state.controlHandle)state.control=(Control*)MapViewOfFile(state.controlHandle,FILE_MAP_ALL_ACCESS,0,0,4096);
 swprintf_s(name,L"Local\\BetterGPS-Diagnostics-%lu-%u",GetCurrentProcessId(),component);state.dataHandle=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,mapBytes,name);if(state.dataHandle)state.header=(Header*)MapViewOfFile(state.dataHandle,FILE_MAP_ALL_ACCESS,0,0,mapBytes);if(state.header){state.header->version=1;state.header->pid=GetCurrentProcessId();state.header->component=component;LARGE_INTEGER f;QueryPerformanceFrequency(&f);state.header->frequency=f.QuadPart;MemoryBarrier();state.header->magic=0x42475044;}}
inline void Refresh(){auto* c=state.control;bool on=c&&state.header&&c->magic==0x42475043&&c->version==1&&c->enabled;LONG64 token=0;if(on){const auto heart=InterlockedCompareExchange64(&c->heartbeat,0,0);const auto delta=Now()-heart;on=c->frequency==state.header->frequency&&delta>=0&&delta<c->frequency*3;token=InterlockedCompareExchange64(&c->session,0,0);}state.session=token;state.active.store(on,std::memory_order_relaxed);}
inline void Record(Metric metric,LONG64 start,LONG64 ticks,uint64_t value=0,bool event=false){auto* h=state.header;if(!h||ticks<0)return;auto* stats=(MetricData*)((uint8_t*)h+sizeof(Header));auto& s=stats[metric];InterlockedIncrement64(&s.count);InterlockedAdd64(&s.ticks,ticks);LONG64 old=s.maximum;while(ticks>old){auto seen=InterlockedCompareExchange64(&s.maximum,ticks,old);if(seen==old)break;old=seen;}const double us=double(ticks)*1e6/h->frequency;unsigned bucket=0;double bound=1;while(bucket+1<bucketCount&&us>bound){++bucket;bound*=2;}InterlockedIncrement64(&s.buckets[bucket]);if(event||us>=500){const auto ticket=InterlockedIncrement64(&h->write);auto* ring=(Event*)((uint8_t*)h+128+metricSlots*sizeof(MetricData));auto& e=ring[(ticket-1)%ringCount];InterlockedExchange64(&e.sequence,-ticket);e.qpc=start;e.ticks=ticks;e.metric=metric;e.thread=GetCurrentThreadId();e.value=value;MemoryBarrier();InterlockedExchange64(&e.sequence,ticket);}}
struct Scope {Metric metric;LONG64 start=0,session=0;explicit Scope(Metric m,unsigned sample=1):metric(m){if(state.active.load(std::memory_order_relaxed)){thread_local unsigned calls[MetricCount]{};if((calls[m]++%sample)==0){session=state.session.load();start=Now();}}}~Scope(){if(start&&session==state.session.load())Record(metric,start,Now()-start);}Scope(const Scope&)=delete;};
inline void Frame(void* chain,bool focused,bool menu){Refresh();struct Last{void* chain{};LONG64 when{},session{};bool focused=false;};thread_local Last last[8];if(!state.active.load()){for(auto& l:last)l={};return;}auto now=Now(),session=state.session.load();for(auto& l:last)if(!l.chain||l.chain==chain){if(l.chain&&l.session==session)Record(FrameInterval,now,now-l.when,(uint64_t(chain)&~uint64_t(3))|(focused&&l.focused?1:0)|(menu?2:0),true);l={chain,now,session,focused};break;}}
inline void Stop(){state.active=false;if(state.header){UnmapViewOfFile(state.header);state.header=nullptr;}if(state.control){UnmapViewOfFile(state.control);state.control=nullptr;}if(state.dataHandle){CloseHandle(state.dataHandle);state.dataHandle=nullptr;}if(state.controlHandle){CloseHandle(state.controlHandle);state.controlHandle=nullptr;}}
}
