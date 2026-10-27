#pragma once
#include "NativeMapTheme.h"
#include "DesignStyle.h"
#include <MinHook.h>
#include "HookBatch.h"
#include "OpcodeScan.h"
#include <array>
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include "RenderActivity.h"
#include <string>
#include <algorithm>
namespace bettergps_overlay {
// Resolve the engine's own map_data reflection records. No heap scan, fixed
// object address, GPU readback, map filter, or guessed layout is involved.
struct MapUnitLayout {
 uintptr_t owner=0,vtable=0,loaded=0,destroy=0,cacheInit=0,cacheDestroy=0,cacheReset=0;
 size_t cacheBytes=0;
 std::map<std::string,size_t> cacheColors;
 size_t bytes=0,areas=0,discovered=0;
 std::map<std::string,size_t> colors;
};
struct MapImage {
 uint8_t* base=nullptr;size_t size=0;
 bool contains(uintptr_t p,size_t bytes=1)const{return p>=uintptr_t(base)&&bytes<=size&&p-uintptr_t(base)<=size-bytes;}
 template<class T>T get(uintptr_t p)const{T value{};if(contains(p,sizeof(T)))memcpy(&value,(void*)p,sizeof(T));return value;}
 bool text(uintptr_t p,const char* expected)const{const auto n=strlen(expected)+1;return contains(p,n)&&memcmp((void*)p,expected,n)==0;}
 uintptr_t function(uintptr_t address,bool ending=false)const{
  const auto nt=(IMAGE_NT_HEADERS64*)(base+get<uint32_t>(uintptr_t(base)+60));const auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
  if(!contains(uintptr_t(base)+directory.VirtualAddress,directory.Size))return 0;
  auto* entries=(IMAGE_RUNTIME_FUNCTION_ENTRY*)(base+directory.VirtualAddress);const auto count=directory.Size/sizeof(*entries);const auto rva=address-uintptr_t(base);
  size_t lo=0,hi=count;while(lo<hi){const auto mid=(lo+hi)/2;if(entries[mid].EndAddress<=rva)lo=mid+1;else hi=mid;}
  if(lo>=count||entries[lo].BeginAddress>rva)return 0;
  // MSVC splits a function into chained unwind regions after shrink-wrapped
  // register saves. Resolve those to the actual entry, not a mid-function label.
  auto root=[&](IMAGE_RUNTIME_FUNCTION_ENTRY entry){for(unsigned depth=0;depth<16;++depth){const auto unwind=uintptr_t(base)+entry.UnwindInfoAddress;if(!(get<uint8_t>(unwind)>>3&UNW_FLAG_CHAININFO))break;const auto codes=get<uint8_t>(unwind+2);const auto chain=unwind+4+((codes+1)&~1u)*2;entry=get<IMAGE_RUNTIME_FUNCTION_ENTRY>(chain);}return entry.BeginAddress;};
  const auto first=root(entries[lo]);if(!ending)return uintptr_t(base)+first;
  auto last=entries[lo].EndAddress;for(size_t next=lo+1;next<count&&next<lo+64&&entries[next].BeginAddress==last&&root(entries[next])==first;++next)last=entries[next].EndAddress;
  return uintptr_t(base)+last;
 }
 bool executable(uintptr_t p)const{
  if(size<256||!contains(uintptr_t(base),64))return false;const auto pe=get<uint32_t>(uintptr_t(base)+60);if(pe>size-256)return false;
  const auto header=uintptr_t(base)+pe;const auto count=get<uint16_t>(header+6),optional=get<uint16_t>(header+20);
  for(unsigned i=0;i<count;++i){const auto section=header+24+optional+i*40;const auto rva=get<uint32_t>(section+12),length=get<uint32_t>(section+8),flags=get<uint32_t>(section+36);if((flags&IMAGE_SCN_MEM_EXECUTE)&&p>=uintptr_t(base)+rva&&p-uintptr_t(base)-rva<length)return true;}return false;
 }
};
inline MapUnitLayout resolve_map_unit(const MapImage& image){
 MapUnitLayout result;const auto begin=uintptr_t(image.base),end=begin+image.size;
 // A property record is {owner, offset, type, auxiliary, name}. The owning
 // class descriptor points to its name, factory, size and first property.
 for(uintptr_t at=begin;at+40<=end;at+=8){
  if(image.get<uint64_t>(at+16)!=39||!image.text(image.get<uintptr_t>(at+32),"road_color"))continue;
  const auto owner=image.get<uintptr_t>(at);if(!image.text(image.get<uintptr_t>(image.get<uintptr_t>(owner)),"map_data"))continue;
  if(result.owner&&result.owner!=owner)return {};result.owner=owner;
 }
 if(!result.owner)return {};result.bytes=image.get<uint64_t>(result.owner+48);if(result.bytes<128||result.bytes>4096)return {};
 for(uintptr_t at=begin;at+40<=end;at+=8){if(image.get<uintptr_t>(at)!=result.owner)continue;const auto offset=image.get<uint64_t>(at+8),type=image.get<uint64_t>(at+16),name=image.get<uintptr_t>(at+32);if(offset+4>result.bytes)continue;
  for(const char* key:{"road_color","road_discovered_color","prefab_color","prefab_obstacle_color","prefab_grass_color","prefab_discovered_color","prefab_obstacle_discovered_color","prefab_grass_discovered_color","outline_color","navigation_color","navigation_current_color","navigation_highlight_color","navigation_fade_color","navigation_arrow_color"})if(type==39&&image.text(name,key))result.colors.emplace(key,size_t(offset));
  if(type==48&&offset+40<=result.bytes){if(image.text(name,"map_area_color"))result.areas=size_t(offset);if(image.text(name,"map_area_discovered_color"))result.discovered=size_t(offset);}
 }
 if(result.colors.size()!=14||!result.areas||!result.discovered)return {};
 // The type-query leaf returns exactly this descriptor. Its vtable slot is
 // verified before using the unit's post-load and destruction callbacks.
 for(uintptr_t at=opcode_candidate(begin,end,8,0x48);at;at=opcode_candidate(at+1,end,8,0x48)){if((image.get<uint32_t>(at)&0xffffff)!=0x058d48)continue;
  if(image.get<uint8_t>(at+7)!=0xc3||at+7+image.get<int32_t>(at+3)!=result.owner||!image.executable(at))continue;
  for(uintptr_t ptr=begin+40;ptr+56<=end;ptr+=8){if(image.get<uintptr_t>(ptr)!=at)continue;const auto table=ptr-40;bool valid=true;for(unsigned i=0;i<12;++i)valid=valid&&image.executable(image.get<uintptr_t>(table+i*8));if(!valid)continue;
   if(result.vtable&&result.vtable!=table)return {};result.vtable=table;result.destroy=image.get<uintptr_t>(table);result.loaded=image.get<uintptr_t>(table+88);
  }
 }
 if(!result.vtable)return {};
 const auto loadedEnd=image.function(result.loaded,true);bool areaCheck=false,discoveredCheck=false,success=false;
 if(loadedEnd<=result.loaded||loadedEnd-result.loaded>8192)return {};
 for(auto at=result.loaded;at+7<=loadedEnd;++at){if((image.get<uint32_t>(at)&0xffffff)==0x833b48){const auto offset=image.get<uint32_t>(at+3);areaCheck=areaCheck||offset==result.areas+16;discoveredCheck=discoveredCheck||offset==result.discovered+16;}success=success||image.get<uint16_t>(at)==0x01b0;}
 if(!areaCheck||!discoveredCheck||!success)return {};
 // ui_map caches the scalar palette. Resolve its copy routine from all eight
 // reflected source offsets; updating only map_data would leave these stale.
 for(uintptr_t at=opcode_candidate(begin,end,14*12,0x8b);at;at=opcode_candidate(at+1,end,14*12,0x8b)){if(image.get<uint16_t>(at)!=0x818b||image.get<uint32_t>(at+2)!=result.colors.at("road_color"))continue;
  std::map<std::string,size_t> colors;unsigned pairs=0;
  for(unsigned i=0;i<14;++i){const auto op=at+i*12;if(image.get<uint16_t>(op)!=0x818b||image.get<uint16_t>(op+6)!=0x8789)break;++pairs;
   const auto src=image.get<uint32_t>(op+2),dst=image.get<uint32_t>(op+8);for(auto& field:result.colors)if(src==field.second)colors[field.first]=dst;
  }
  if(pairs!=14||colors.size()!=14||!image.executable(at))continue;const auto init=image.function(at);if(!init)return {};
  for(uintptr_t ptr=begin+96;ptr+8<=end;ptr+=8){if(image.get<uintptr_t>(ptr)!=init)continue;const auto table=ptr-96,getType=image.get<uintptr_t>(table+40);
   if((image.get<uint32_t>(getType)&0xffffff)!=0x058d48||image.get<uint8_t>(getType+7)!=0xc3)continue;
   const auto type=getType+7+image.get<int32_t>(getType+3);if(!image.text(image.get<uintptr_t>(image.get<uintptr_t>(type)),"ui_map"))continue;
   const auto bytes=image.get<uint64_t>(type+48);bool valid=bytes>=512&&bytes<=32768;for(auto& field:colors)valid=valid&&field.second+4<=bytes;if(!valid)continue;
   const auto deleting=image.get<uintptr_t>(table);uintptr_t destroy=0;
   // The deleting wrapper calls the actual destructor before freeing storage.
   for(unsigned i=0;i<48;++i)if(image.get<uint8_t>(deleting+i)==0xe8){destroy=deleting+i+5+image.get<int32_t>(deleting+i+1);break;}
   if(!image.executable(destroy)||!image.executable(image.get<uintptr_t>(table+8)))continue;
   if(result.cacheInit&&result.cacheInit!=init)return {};result.cacheInit=init;result.cacheDestroy=destroy;result.cacheReset=image.get<uintptr_t>(table+8);result.cacheBytes=size_t(bytes);result.cacheColors=colors;
  }
 }
 return result.cacheInit?result:MapUnitLayout{};
}
struct NativePalette {
 std::map<std::string,uint32_t> colors;
 std::vector<uint32_t> areas,discovered;
};
inline NativePalette read_native_palette(const std::filesystem::path& file){
 NativePalette palette;std::ifstream stream(file);std::string line;
 while(std::getline(stream,line)){const auto colon=line.find(':'),hex=line.find("0x",colon);if(colon==std::string::npos||hex==std::string::npos)continue;auto key=line.substr(0,colon);key.erase(0,key.find_first_not_of(" \t"));key.erase(key.find_last_not_of(" \t")+1);try{const auto value=uint32_t(std::stoul(line.substr(hex+2),nullptr,16));if(key=="map_area_color[]")palette.areas.push_back(value);else if(key=="map_area_discovered_color[]")palette.discovered.push_back(value);else palette.colors[key]=value;}catch(...){}}
 return palette;
}
inline bool map_memory(uintptr_t at,size_t bytes){
 if(!at||!bytes||bytes>65536||at+bytes<at)return false;MEMORY_BASIC_INFORMATION memory{};
 if(!VirtualQuery((void*)at,&memory,sizeof(memory))||memory.State!=MEM_COMMIT||(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
 const auto protection=memory.Protect&0xff;return (protection==PAGE_READWRITE||protection==PAGE_WRITECOPY)&&at+bytes<=uintptr_t(memory.BaseAddress)+memory.RegionSize;
}
inline bool map_unit_write(const MapUnitLayout& layout,uintptr_t unit,const NativePalette& palette,bool write=true){
 if(!map_memory(unit,layout.bytes)||*(uintptr_t*)unit!=layout.vtable)return false;
 // Validate every destination first. Arrays are engine-owned and may be replaced
 // by a save/profile reload. Never retain their data pointers across callbacks.
 const auto a=*(uintptr_t*)(unit+layout.areas+8),ac=*(size_t*)(unit+layout.areas+16);
 const auto d=*(uintptr_t*)(unit+layout.discovered+8),dc=*(size_t*)(unit+layout.discovered+16);
 if(ac!=palette.areas.size()||dc!=palette.discovered.size()||ac>512||!map_memory(a,ac*4)||!map_memory(d,dc*4))return false;
 for(const auto& field:layout.colors)if(!palette.colors.count(field.first))return false;
 if(!write)return true;
 for(const auto& field:layout.colors)*(uint32_t*)(unit+field.second)=palette.colors.at(field.first);
 memcpy((void*)a,palette.areas.data(),ac*4);memcpy((void*)d,palette.discovered.data(),dc*4);return true;
}
inline std::mutex liveMapMutex;
inline std::recursive_mutex liveMapGeometryMutex;
inline MapUnitLayout liveMapLayout;
inline std::array<NativePalette,2> liveMapPalettes;
inline std::array<NativePalette,2> wazeMapPalettes;
inline std::array<NativePalette,2> garminMapPalettes;
inline std::array<NativePalette,2> copilotMapPalettes;
inline std::array<NativePalette,2> tomtomMapPalettes;
inline std::array<NativePalette,2> sygicMapPalettes;
inline const NativePalette& device_map_palette(size_t style,bool dark){const auto palette=native_style_palette(style);return (palette==5?sygicMapPalettes:palette==4?tomtomMapPalettes:palette==3?copilotMapPalettes:palette==2?garminMapPalettes:palette==1?wazeMapPalettes:liveMapPalettes)[dark?1:0];}
// The clear travels with each deferred map job. A sub-byte alpha difference
// carries its icon preference; all variants still round to opaque 8-bit alpha.
inline std::array<float,4> native_map_clear(unsigned theme){static const std::array<std::array<float,4>,6> colors{{{.75f,.75f,.75f,1},{.0103f,.0194f,.0369f,1},{.84f,.88f,.90f,1},{.0185f,.0295f,.045f,1},{.63076f,.68669f,.7454f,1},{.02956f,.02956f,.02956f,1}}};if(theme>=30){std::array<float,4> color=theme%2?std::array<float,4>{.040915f,.074214f,.0865f,1}:std::array<float,4>{.814847f,.806952f,.863157f,1};color[3]=1.f-((theme-30)/2)*.0001f;return color;}if(theme>=24){std::array<float,4> color=theme%2?std::array<float,4>{.02217f,.02732f,.03434f,1}:std::array<float,4>{.75294f,.73791f,.65141f,1};color[3]=1.f-((theme-24)/2)*.0001f;return color;}if(theme>=18){std::array<float,4> color=theme%2?std::array<float,4>{.03071f,.03310f,.03560f,1}:std::array<float,4>{.91310f,.85499f,.78354f,1};color[3]=1.f-((theme-18)/2)*.0001f;return color;}auto color=colors[theme%6];color[3]=1.f-(theme/6)*.0001f;return color;}
inline std::array<uintptr_t,8> liveMapUnits{};
struct MapCacheEntry {uintptr_t pointer=0,vtable=0;};
inline std::array<MapCacheEntry,32> liveMapCaches{};
inline std::atomic<bool> liveMapDark{false};
inline std::atomic<bool> liveMapReady{false};
inline uint64_t liveMapGeneration=0,liveMapAppliedGeneration=~uint64_t(0);
using MapLoaded=bool(__fastcall*)(void*);
using MapDestroy=void*(__fastcall*)(void*,unsigned);
inline MapLoaded liveMapOriginalLoaded=nullptr;
inline MapDestroy liveMapOriginalDestroy=nullptr;
inline bool __fastcall live_map_loaded(void* object){
 const auto ok=liveMapOriginalLoaded(object);if(!ok)return ok;std::lock_guard<std::mutex> lock(liveMapMutex);
 const auto unit=uintptr_t(object);auto entry=std::find(liveMapUnits.begin(),liveMapUnits.end(),unit);

 if(entry==liveMapUnits.end())entry=std::find(liveMapUnits.begin(),liveMapUnits.end(),uintptr_t(0));if(entry!=liveMapUnits.end()){*entry=unit;++liveMapGeneration;}return ok;
}
inline void* __fastcall live_map_destroy(void* object,unsigned flags){
 {std::lock_guard<std::mutex> lock(liveMapMutex);for(auto& unit:liveMapUnits)if(unit==uintptr_t(object)){unit=0;++liveMapGeneration;}}
 return liveMapOriginalDestroy(object,flags);
}
using MapCacheCall=void*(__fastcall*)(void*);
inline MapCacheCall liveMapOriginalCacheInit=nullptr,liveMapOriginalCacheDestroy=nullptr,liveMapOriginalCacheReset=nullptr;
inline bool map_cache_write(const MapUnitLayout& layout,MapCacheEntry entry,const NativePalette& palette,bool write=true){
 if(!map_memory(entry.pointer,layout.cacheBytes)||*(uintptr_t*)entry.pointer!=entry.vtable)return false;
 for(auto& field:layout.cacheColors)if(!palette.colors.count(field.first))return false;
 if(write)for(auto& field:layout.cacheColors){auto* color=(uint32_t*)(entry.pointer+field.second);
  // Derived job/world map components may deliberately override advisor colors.
  // Preserve those overrides rather than imposing the phone's palette on them.
  const auto light=liveMapPalettes[0].colors.find(field.first),dark=liveMapPalettes[1].colors.find(field.first);
  if(light!=liveMapPalettes[0].colors.end()&&dark!=liveMapPalettes[1].colors.end()&&(*color==light->second||*color==dark->second))*color=palette.colors.at(field.first);
 }return true;
}
inline void* __fastcall live_map_cache_init(void* object){
 auto* result=liveMapOriginalCacheInit(object);std::lock_guard<std::mutex> lock(liveMapMutex);
 auto entry=std::find_if(liveMapCaches.begin(),liveMapCaches.end(),[&](auto c){return c.pointer==uintptr_t(object);});
 if(entry==liveMapCaches.end())entry=std::find_if(liveMapCaches.begin(),liveMapCaches.end(),[](auto c){return !c.pointer;});
 if(entry!=liveMapCaches.end()){*entry={uintptr_t(object),*(uintptr_t*)object};++liveMapGeneration;}return result;
}
inline void (*liveMapForgetObserver)(void*)=nullptr;
inline void live_map_cache_forget(void* object){if(liveMapForgetObserver)liveMapForgetObserver(object);std::lock_guard<std::mutex> lock(liveMapMutex);for(auto& cache:liveMapCaches)if(cache.pointer==uintptr_t(object)){cache={};++liveMapGeneration;}}
inline void* __fastcall live_map_cache_destroy(void* object){live_map_cache_forget(object);return liveMapOriginalCacheDestroy(object);}
inline void* __fastcall live_map_cache_reset(void* object){live_map_cache_forget(object);return liveMapOriginalCacheReset(object);}
inline bool live_map_install(const MapImage& image,const std::filesystem::path& assets,bool dark){
 liveMapLayout=resolve_map_unit(image);if(!liveMapLayout.vtable)return false;
 for(int i=0;i<2;++i){liveMapPalettes[i]=read_native_palette(assets/"native_themes"/(i?"dark":"light")/"colour_data.sui");if(liveMapPalettes[i].areas.empty()||liveMapPalettes[i].areas.size()!=liveMapPalettes[i].discovered.size())return false;for(auto& field:liveMapLayout.colors)if(!liveMapPalettes[i].colors.count(field.first))return false;}
 for(int i=0;i<2;++i){wazeMapPalettes[i]=read_native_palette(assets/"native_themes"/(i?"waze_dark":"waze_light")/"colour_data.sui");if(wazeMapPalettes[i].areas.size()!=liveMapPalettes[i].areas.size()||wazeMapPalettes[i].discovered.size()!=liveMapPalettes[i].discovered.size())return false;for(auto& field:liveMapLayout.colors)if(!wazeMapPalettes[i].colors.count(field.first))return false;}
 for(int i=0;i<2;++i){garminMapPalettes[i]=read_native_palette(assets/"native_themes"/(i?"garminmap_dark":"garminmap_light")/"colour_data.sui");if(garminMapPalettes[i].areas.size()!=liveMapPalettes[i].areas.size()||garminMapPalettes[i].discovered.size()!=liveMapPalettes[i].discovered.size())return false;for(auto& field:liveMapLayout.colors)if(!garminMapPalettes[i].colors.count(field.first))return false;}
 for(int i=0;i<2;++i){copilotMapPalettes[i]=read_native_palette(assets/"native_themes"/(i?"copilot_dark":"copilot_light")/"colour_data.sui");if(copilotMapPalettes[i].areas.size()!=liveMapPalettes[i].areas.size()||copilotMapPalettes[i].discovered.size()!=liveMapPalettes[i].discovered.size())return false;for(auto& field:liveMapLayout.colors)if(!copilotMapPalettes[i].colors.count(field.first))return false;}
 for(int i=0;i<2;++i){sygicMapPalettes[i]=read_native_palette(assets/"native_themes"/(i?"sygic_dark":"sygic_light")/"colour_data.sui");if(sygicMapPalettes[i].areas.size()!=liveMapPalettes[i].areas.size()||sygicMapPalettes[i].discovered.size()!=liveMapPalettes[i].discovered.size())return false;for(auto& field:liveMapLayout.colors)if(!sygicMapPalettes[i].colors.count(field.first))return false;}
 for(int i=0;i<2;++i){tomtomMapPalettes[i]=read_native_palette(assets/"native_themes"/(i?"tomtom_dark":"tomtom_light")/"colour_data.sui");if(tomtomMapPalettes[i].areas.size()!=liveMapPalettes[i].areas.size()||tomtomMapPalettes[i].discovered.size()!=liveMapPalettes[i].discovered.size())return false;for(auto& field:liveMapLayout.colors)if(!tomtomMapPalettes[i].colors.count(field.first))return false;}
 if(liveMapPalettes[0].areas.size()!=liveMapPalettes[1].areas.size())return false;liveMapDark=dark;liveMapUnits={};++liveMapGeneration;
 struct Hook {uintptr_t address;void* callback;void** original;};
 const Hook hooks[]={{liveMapLayout.loaded,(void*)&live_map_loaded,(void**)&liveMapOriginalLoaded},{liveMapLayout.destroy,(void*)&live_map_destroy,(void**)&liveMapOriginalDestroy},{liveMapLayout.cacheInit,(void*)&live_map_cache_init,(void**)&liveMapOriginalCacheInit},{liveMapLayout.cacheDestroy,(void*)&live_map_cache_destroy,(void**)&liveMapOriginalCacheDestroy},{liveMapLayout.cacheReset,(void*)&live_map_cache_reset,(void**)&liveMapOriginalCacheReset}};
 size_t created=0;for(auto& hook:hooks){if(MH_CreateHook((void*)hook.address,hook.callback,hook.original)!=MH_OK)break;++created;}
 bool ok=created==std::size(hooks);if(ok){std::vector<void*> addresses;for(auto& hook:hooks)addresses.push_back((void*)hook.address);ok=enable_hook_batch(addresses)==MH_OK;}
 if(!ok){for(size_t i=0;i<created;++i){MH_DisableHook((void*)hooks[i].address);MH_RemoveHook((void*)hooks[i].address);}return false;}liveMapCaches={};liveMapReady=true;return true;
}
// Called on the game's telemetry thread, never by the preparation worker.
inline bool map_source_valid(const MapUnitLayout& layout,uintptr_t unit,size_t minimum){if(!map_memory(unit,layout.bytes)||*(uintptr_t*)unit!=layout.vtable)return false;for(auto offset:{layout.areas,layout.discovered}){auto pointer=*(uintptr_t*)(unit+offset+8);auto count=*(size_t*)(unit+offset+16);if(count<minimum||count>512||!map_memory(pointer,count*4))return false;}return true;}
inline bool live_map_apply(bool dark){
 if(!liveMapReady)return false;std::lock_guard<std::mutex> lock(liveMapMutex);liveMapDark=dark;bool any=false;for(auto unit:liveMapUnits)if(unit){if(!map_source_valid(liveMapLayout,unit,liveMapPalettes[dark?1:0].areas.size()))return false;any=true;}for(auto cache:liveMapCaches)if(cache.pointer&&!map_cache_write(liveMapLayout,cache,liveMapPalettes[dark?1:0],false))return false;return any;
}
// Transaction restricted to the private map's CPU geometry generation. Restore
// source unit fields immediately; never install Google colours in other UI caches.
struct PrivateMapPaletteScope {
 std::unique_lock<std::recursive_mutex> geometryLock{liveMapGeometryMutex};
 bool restoreCache=true;
 struct Backup {uintptr_t unit=0,area=0,discovered=0;size_t count=0;std::array<uint32_t,16> colors{};std::array<uint32_t,512> a{},d{};};
 std::array<Backup,8> backups{};std::array<uint32_t,16> cache{};uintptr_t map=0;bool applied=false;
 PrivateMapPaletteScope(uintptr_t privateMap,bool dark,size_t style=displayStyleIndex.load(),bool restore=true):restoreCache(restore){if(!liveMapReady||liveMapLayout.colors.size()>16||liveMapLayout.cacheColors.size()>16)return;const auto& palette=device_map_palette(style,dark);std::lock_guard<std::mutex> lock(liveMapMutex);
  if(!map_memory(privateMap,liveMapLayout.cacheBytes))return;
  for(auto unit:liveMapUnits)if(unit&&!map_source_valid(liveMapLayout,unit,palette.areas.size()))return;
  size_t n=0;for(auto unit:liveMapUnits)if(unit){auto& b=backups[n++];b.unit=unit;b.area=*(uintptr_t*)(unit+liveMapLayout.areas+8);b.discovered=*(uintptr_t*)(unit+liveMapLayout.discovered+8);b.count=palette.areas.size();size_t k=0;for(auto& f:liveMapLayout.colors)b.colors[k++]=*(uint32_t*)(unit+f.second);memcpy(b.a.data(),(void*)b.area,b.count*4);memcpy(b.d.data(),(void*)b.discovered,b.count*4);for(auto& f:liveMapLayout.colors)*(uint32_t*)(unit+f.second)=palette.colors.at(f.first);memcpy((void*)b.area,palette.areas.data(),b.count*4);memcpy((void*)b.discovered,palette.discovered.data(),b.count*4);}
  size_t k=0;for(auto& f:liveMapLayout.cacheColors){cache[k++]=*(uint32_t*)(privateMap+f.second);*(uint32_t*)(privateMap+f.second)=palette.colors.at(f.first);}map=privateMap;applied=true;
 }
 ~PrivateMapPaletteScope(){if(!applied)return;std::lock_guard<std::mutex> lock(liveMapMutex);for(auto& b:backups)if(b.unit&&std::find(liveMapUnits.begin(),liveMapUnits.end(),b.unit)!=liveMapUnits.end()&&map_memory(b.unit,liveMapLayout.bytes)&&*(uintptr_t*)(b.unit+liveMapLayout.areas+8)==b.area&&*(uintptr_t*)(b.unit+liveMapLayout.discovered+8)==b.discovered){size_t k=0;for(auto& f:liveMapLayout.colors)*(uint32_t*)(b.unit+f.second)=b.colors[k++];memcpy((void*)b.area,b.a.data(),b.count*4);memcpy((void*)b.discovered,b.d.data(),b.count*4);}if(restoreCache&&map_memory(map,liveMapLayout.cacheBytes)){size_t k=0;for(auto& f:liveMapLayout.cacheColors)*(uint32_t*)(map+f.second)=cache[k++];}}
};

inline void live_map_stop(){
 if(liveMapReady.exchange(false))for(auto address:{liveMapLayout.loaded,liveMapLayout.destroy,liveMapLayout.cacheInit,liveMapLayout.cacheDestroy,liveMapLayout.cacheReset}){MH_DisableHook((void*)address);MH_RemoveHook((void*)address);}std::lock_guard<std::mutex> lock(liveMapMutex);liveMapUnits={};liveMapCaches={};
}
}
