#pragma once
// Included inside bettergps_overlay on the CPU preparation thread. COM/D3D decoders
// are local, so none can be destroyed under Windows' DLL loader lock.
static std::shared_ptr<const HomeLayers> managed_home_layers(unsigned slot,ManagedDevice& d,uint64_t now){
 auto p=d.prefs;bool changed=false;const int64_t day=g_haveGameTime?std::max<int64_t>(0,int64_t(g_gameTimeMinutes)+g_multiplayerTimeOffset)/1440:-1;
 std::array<std::filesystem::path,4> paths;
 {std::lock_guard<std::mutex> lock(homeMutex);
  for(unsigned i=0;i<2;++i){const auto migrated=home_icon_selection(p.wallpaper[i]);if(migrated!=p.wallpaper[i]){p.wallpaper[i]=migrated;changed=true;}}
  const auto power=managedPower[slot].serial.load();const bool shuffle=p.shuffle==1&&(p.pending||p.lastPower!=power)||p.shuffle==2&&day>=0&&(p.pending||p.lastDay!=day);
  if(shuffle&&!homeChoices[2].empty()){
   if(!p.seed)p.seed=home_random_mix(homeShuffleSeed^uint64_t(slot+1));if(p.dayOrigin<0)p.dayOrigin=day;
   const uint64_t count=homeChoices[2].size(),draw=p.shuffle==2?uint64_t(day)+count-uint64_t(p.dayOrigin)%count:p.counter++;
   const size_t index=home_shuffle_index(draw,p.seed);p.wallpaper[2]=homeChoices[2][index].name;p.wallpaper[3]="__none__";for(auto& top:homeChoices[3])if(home_lower(top.name)==home_lower(p.wallpaper[2])){p.wallpaper[3]=top.name;break;}
   p.lastPower=power;p.lastDay=day;p.pending=false;changed=true;
  }
  for(unsigned i=0;i<4;++i)for(auto& choice:homeChoices[i])if(home_lower(choice.name)==home_lower(p.wallpaper[i])||(i<2&&home_lower(choice.name)==home_lower(p.wallpaper[i])+"_lock")){paths[i]=choice.path;break;}
 }
 if(changed){d.prefs=p;{std::lock_guard<std::mutex> lock(managedMutex);managedDevices[slot].prefs=p;++managedDevices[slot].prefsRevision;d.prefsRevision=managedDevices[slot].prefsRevision;managedDirty=true;}if(managedSelected==int(slot))device_preferences_to_editor(p,managedBase/"device-settings"/("device_"+std::to_string(slot)+"_lockscreen.ini"));}
 struct LayerCache {std::string key;uint64_t check=0;std::shared_ptr<const HomeLayers> layers;};static std::array<LayerCache,managedDeviceCount> cache;auto& item=cache[slot];std::string selection;for(auto& name:p.wallpaper)selection+=name+'|';
 if(item.layers&&now<item.check&&item.key.rfind(selection,0)==0)return item.layers;item.check=now+1000;std::string key=selection;for(auto& path:paths){std::error_code ec;key+=':'+std::to_string(std::filesystem::last_write_time(path,ec).time_since_epoch().count());}
 if(item.layers&&item.key==key)return item.layers;
 static std::map<std::string,std::shared_ptr<const HomePixels>> images;HomeDecoder decoder;auto layers=std::make_shared<HomeLayers>();
 for(unsigned i=0;i<4;++i){if(paths[i].empty())continue;std::error_code ec;const auto imageKey=paths[i].u8string()+std::to_string(std::filesystem::last_write_time(paths[i],ec).time_since_epoch().count());auto found=images.find(imageKey);if(found==images.end()){auto decoded=decoder.image(paths[i]);found=images.emplace(imageKey,decoded.empty()?std::shared_ptr<const HomePixels>{}:std::make_shared<const HomePixels>(std::move(decoded))).first;}layers->pixels[i]=found->second;}
 if(p.wallpaper[3]=="__none__")layers->pixels[3]=std::make_shared<const HomePixels>(360*740*4,0);
 while(images.size()>32)images.erase(images.begin());static uint64_t revision=uint64_t(1)<<32;layers->revision=++revision;item.layers=layers;item.key=key;return layers;
}
