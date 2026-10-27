#pragma once
// The ui_map creates a native player mesh/material before drawing the map.
// Select its material at that creation point, not a presumed 64x64 PS binding:
// engine batching/streaming is free to stop binding the source DDS directly.
namespace bettergps_overlay {
void log(scs_log_type_t,const char*,...);
inline bool native_player_is_truck(size_t style){return native_style_icon(style)==2||(native_style_icon(style)==0&&native_style_palette(style)==2);}
inline const char* native_player_material(size_t style,bool dark){
 const auto choice=native_style_icon(style),palette=native_style_palette(style);
 if(choice==1)return "/material/ui/bettergps_player/original.mat";
 if(native_player_is_truck(style))return "/material/ui/bettergps_player/truck.mat";
 if(palette==3)return "/material/ui/bettergps_player/copilot.mat";
 if(palette==5)return "/material/ui/bettergps_player/sygic.mat";
 if(palette==4)return "/material/ui/bettergps_player/tomtom.mat";
 if(palette==1)return dark?"/material/ui/bettergps_player/waze_dark.mat":"/material/ui/bettergps_player/waze_light.mat";
 return dark?"/material/ui/bettergps_player/google_dark.mat":"/material/ui/bettergps_player/google_light.mat";
}
inline thread_local const char* nativePlayerMaterial=nullptr;
inline thread_local bool nativePlayerInitializing=false;
inline thread_local unsigned nativePlayerLoads=0;
struct NativePlayerMaterialScope {
 const char* previous=nativePlayerMaterial;
 NativePlayerMaterialScope(size_t style,bool dark){nativePlayerMaterial=native_player_material(style,dark);}
 ~NativePlayerMaterialScope(){nativePlayerMaterial=previous;}
};
using NativePlayerInit=void(__fastcall*)(void*);
using NativePlayerMeshMaterial=void*(__fastcall*)(void*,unsigned,const char**,void*,unsigned,uintptr_t);
using NativePlayerTextureMaterial=uint32_t*(__fastcall*)(const char**,const void*);
inline NativePlayerInit nativePlayerOriginalInit=nullptr;
inline NativePlayerMeshMaterial nativePlayerOriginalMesh=nullptr;
inline NativePlayerTextureMaterial nativePlayerOriginalTexture=nullptr;
inline std::array<void*,3> nativePlayerHookAddresses{};
inline const char* native_player_path(const char* path){return nativePlayerInitializing&&nativePlayerMaterial&&path&&strcmp(path,"/material/ui/map/player.mat")==0?nativePlayerMaterial:path;}
inline void __fastcall native_player_init(void* object){
 const bool previous=nativePlayerInitializing;const unsigned loads=nativePlayerLoads;nativePlayerInitializing=nativePlayerMaterial!=nullptr;nativePlayerLoads=0;
 struct Restore {bool previous;unsigned loads;~Restore(){nativePlayerInitializing=previous;nativePlayerLoads=loads;}} restore{previous,loads};
 nativePlayerOriginalInit(object);
 static std::atomic<unsigned> reports{0};if(nativePlayerMaterial&&reports.fetch_add(1)<24)log(nativePlayerLoads==3?SCS_LOG_TYPE_message:SCS_LOG_TYPE_warning,"[PlayerMaterial] source=%s mesh=%u texture=%u",nativePlayerMaterial,unsigned(nativePlayerLoads&1),unsigned((nativePlayerLoads>>1)&1));
}
inline void* __fastcall native_player_mesh(void* manager,unsigned type,const char** name,void* key,unsigned flags,uintptr_t extra){
 const char* selected=name?native_player_path(*name):nullptr;
 if(name&&selected!=*name)nativePlayerLoads|=1;
 return nativePlayerOriginalMesh(manager,type,name&&selected!=*name?&selected:name,key,flags,extra);
}
inline uint32_t* __fastcall native_player_texture(const char** name,const void* flags){
 const char* selected=name?native_player_path(*name):nullptr;
 if(name&&selected!=*name)nativePlayerLoads|=2;
 return nativePlayerOriginalTexture(name&&selected!=*name?&selected:name,flags);
}
inline bool native_player_resolve(const MapImage& image){
 struct Guard {size_t rva;std::array<uint8_t,12> bytes;};
 const Guard guards[]={{0xffe180,{0x40,0x55,0x53,0x56,0x57,0x48,0x8d,0x6c,0x24,0xc1,0x48,0x81}},
 {0x1e3c60,{0x48,0x89,0x5c,0x24,0x20,0x89,0x54,0x24,0x10,0x55,0x57,0x41}},
 {0x1dd590,{0x40,0x55,0x41,0x54,0x41,0x56,0x48,0x83,0xec,0x30,0x4c,0x8b}}};
 for(size_t i=0;i<3;++i){const auto at=uintptr_t(image.base)+guards[i].rva;if(!image.contains(at,12)||!image.executable(at)||memcmp((void*)at,guards[i].bytes.data(),12))return false;nativePlayerHookAddresses[i]=(void*)at;}
 return image.text(uintptr_t(image.base)+0x2374ac0,"/material/ui/map/player.mat");
}
inline bool native_player_install(){
 void* callbacks[]={(void*)&native_player_init,(void*)&native_player_mesh,(void*)&native_player_texture};void** originals[]={(void**)&nativePlayerOriginalInit,(void**)&nativePlayerOriginalMesh,(void**)&nativePlayerOriginalTexture};
 size_t count=0;for(;count<3;++count)if(!nativePlayerHookAddresses[count]||MH_CreateHook(nativePlayerHookAddresses[count],callbacks[count],originals[count])!=MH_OK)break;
 bool ok=count==3;if(ok)ok=enable_hook_batch(std::vector<void*>(nativePlayerHookAddresses.begin(),nativePlayerHookAddresses.end()))==MH_OK;
 if(!ok)for(size_t i=0;i<count;++i){MH_DisableHook(nativePlayerHookAddresses[i]);MH_RemoveHook(nativePlayerHookAddresses[i]);}return ok;
}
inline void native_player_stop(){for(auto at:nativePlayerHookAddresses)if(at){MH_DisableHook(at);MH_RemoveHook(at);}nativePlayerHookAddresses={};}
inline void native_player_build_materials(const std::filesystem::path& assets=managedBase/"BetterGPS_assets"){
 const std::pair<const char*,const char*> sources[]={
 {"original","/native_themes/original/material/ui/map/player.dds"},
 {"copilot","/material/ui/map/player.dds"},
 {"tomtom","/material/ui/map/player.dds"},
 {"sygic","/material/ui/map/player.dds"},
 {"google_light","/native_themes/light/material/ui/map/player.dds"},{"google_dark","/native_themes/dark/material/ui/map/player.dds"},
 {"waze_light","/native_themes/waze_light/material/ui/map/player.dds"},{"waze_dark","/native_themes/waze_dark/material/ui/map/player.dds"},
 {"truck","/material/garmin/player_truck.dds"}};
 for(const auto& entry:sources){const std::string stem=std::string("material/ui/bettergps_player/")+entry.first;
  auto tobj=display_bytes(assets/"material/ui/map/player.tobj");if(tobj.size()<48)continue;tobj.resize(48);const std::string source="/"+stem+".dds";auto pixels=display_bytes(assets/std::string(entry.second).substr(1));if(pixels.empty())throw std::runtime_error("Missing native player DDS");if(std::string(entry.first)=="copilot"||std::string(entry.first)=="tomtom"||std::string(entry.first)=="sygic"){auto icon=native_icon_read(assets/std::string(entry.second).substr(1));icon=std::string(entry.first)=="sygic"?native_sygic_blue(std::move(icon)):std::string(entry.first)=="tomtom"?native_tomtom_cyan(std::move(icon)):native_copilot_red(std::move(icon));if(icon.pixels.empty()||pixels.size()<icon.pixels.size()+128)throw std::runtime_error("Invalid themed player DDS");const auto offset=pixels.size()-icon.pixels.size();memcpy(pixels.data()+offset,icon.pixels.data(),icon.pixels.size());}display_write(managedBase/"generated-displays"/(stem+".dds"),pixels);const uint32_t length=uint32_t(source.size());memcpy(tobj.data()+40,&length,4);tobj.insert(tobj.end(),source.begin(),source.end());display_write(managedBase/"generated-displays"/(stem+".tobj"),tobj);
  const auto text="effect : \"ui.rfx\" { texture : \"texture\" { source : \"/"+stem+".tobj\" u_address : clamp v_address : clamp mip_filter : none } }\n";display_write(managedBase/"generated-displays"/(stem+".mat"),{text.begin(),text.end()});
 }
}
}
