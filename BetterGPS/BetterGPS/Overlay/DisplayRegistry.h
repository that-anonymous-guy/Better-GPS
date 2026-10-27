#pragma once
#include "OverlaySettings.h"
#include "DeviceLimits.h"
#include <atomic>
#include <set>
#include <string>
#include <stdexcept>
#include <cstring>
namespace bettergps_overlay {
struct GpsDisplay {std::string id,name,ui,layout;bool enabled=true,ready=false;unsigned markerWidth=0;OverlaySettings settings;bool independent=false;unsigned surfaceWidth=0;std::string source,vehicles;size_t sourceIndex=32;bool nativeFallback=false;};
inline bool ford_console_id(const std::string& id){return id=="f150_infotainment"||id=="bronco_infotainment";}
inline std::vector<GpsDisplay> gpsDisplays;
inline std::vector<std::string> displayErrors;
inline std::atomic<size_t> selectedDisplay{0};
inline std::mutex displayMutex;
inline std::filesystem::path displayBase;
inline std::set<std::string> displaySurfaceMounts;
inline std::string display_trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");return a==std::string::npos?"":s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
inline bool display_id(const std::string& s){return !s.empty()&&s.size()<=64&&s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos;}
inline bool display_ui_path(const std::string& s){if(!(s.rfind("/ui/",0)==0)||!(s.size()>=4&&s.substr(s.size()-4)==".sii")||s.size()>240||s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-/.")!=std::string::npos)return false;for(auto& part:std::filesystem::path(s))if(part==".."||part==".")return false;return s.find("//")==std::string::npos;}
inline std::filesystem::path display_settings_path(size_t i){return displayBase/"display-settings"/(gpsDisplays[i].id+(waze_style()?".waze.cfg":".cfg"));}
inline void display_read_registry(const std::filesystem::path& base){
 displayBase=base;gpsDisplays.clear();displayErrors.clear();selectedDisplay=0;
 std::ifstream input(base/"BetterGPSDisplays.ini");if(!input){displayErrors.push_back("BetterGPSDisplays.ini missing; no display overridden");return;}
 std::string line;GpsDisplay* current=nullptr;std::set<std::string> ids,paths;
 while(std::getline(input,line)){
  if((line.rfind("\xEF\xBB\xBF",0)==0))line.erase(0,3);line=display_trim(line);if(line.empty()||line[0]==';'||line[0]=='#')continue;
  if(line[0]=='['){current=nullptr;if(line.back()!=']'){displayErrors.push_back("Invalid registry section");continue;}auto section=line.substr(1,line.size()-2);if(!(section.rfind("display.",0)==0))continue;auto id=section.substr(8);
   if(!display_id(id)||!ids.insert(id).second||gpsDisplays.size()>=32){displayErrors.push_back("Invalid/duplicate display ID or limit exceeded: "+id);continue;}
   gpsDisplays.push_back({id,id,"","vertical"});current=&gpsDisplays.back();continue;}
  auto split=line.find('=');if(!current||split==std::string::npos)continue;auto key=display_trim(line.substr(0,split)),value=display_trim(line.substr(split+1));
  if(key=="name")current->name=value.substr(0,64);else if(key=="ui_path")current->ui=value;else if(key=="layout")current->layout=value;else if(key=="source_display")current->source=value;else if(key=="vehicles")current->vehicles=value;
  else if(key=="enabled"){if(value=="true"||value=="1")current->enabled=true;else if(value=="false"||value=="0")current->enabled=false;else{current->enabled=false;displayErrors.push_back("Invalid enabled value: "+current->id);}}
 }
 // Preserve older registries while adding a distinct Road Trip console slot.
 auto road=std::find_if(gpsDisplays.begin(),gpsDisplays.end(),[](auto& d){return d.id=="road_trip_phone";});
 if(road!=gpsDisplays.end()&&gpsDisplays.size()<32&&std::none_of(gpsDisplays.begin(),gpsDisplays.end(),[](auto& d){return d.id=="road_trip_console";})){GpsDisplay console;console.id="road_trip_console";console.name="Center console phone";console.enabled=road->enabled;console.layout=road->layout;console.ui=road->ui;console.source="road_trip_phone";gpsDisplays.push_back(console);}
 if(gpsDisplays.size()<32&&std::none_of(gpsDisplays.begin(),gpsDisplays.end(),[](auto& d){return d.id=="f150_infotainment"||d.ui=="/ui/dashboard/ford_f150_gps_23.sii";})){GpsDisplay d;d.id="f150_infotainment";d.name="F150 infotainment";d.ui="/ui/dashboard/ford_f150_gps_23.sii";d.layout="horizontal";d.vehicles="ford.f150_23";gpsDisplays.push_back(d);}
 if(std::any_of(gpsDisplays.begin(),gpsDisplays.end(),[](auto& d){return d.id=="road_trip_phone";})&&gpsDisplays.size()<32&&std::filesystem::is_regular_file(base/"layouts/horizontal/bronco.sii")&&std::none_of(gpsDisplays.begin(),gpsDisplays.end(),[](auto& d){return d.id=="bronco_infotainment"||d.ui=="/ui/dashboard/ford_bronco_gps_24.sii";})){GpsDisplay d;d.id="bronco_infotainment";d.name="Bronco infotainment";d.ui="/ui/dashboard/ford_bronco_gps_24.sii";d.layout="horizontal";d.vehicles="ford.bronco_24";gpsDisplays.push_back(d);}
 for(size_t i=0;i<gpsDisplays.size();++i){auto& d=gpsDisplays[i];d.sourceIndex=i;if(d.id=="road_trip_phone"){if(d.name=="Road Trip phone")d.name="Windshield phone";if(d.vehicles.empty())d.vehicles="ford.mustang_67,ford.f150_23,ford.victoria_06,ford.bronco_24";}if(d.id=="road_trip_console"&&d.vehicles.empty())d.vehicles="ford.mustang_67,ford.f150_23,ford.victoria_06,ford.bronco_24";if(d.id=="cabin_phone"&&d.vehicles.empty())d.vehicles="@trucks";
  if(!d.source.empty()){auto parent=std::find_if(gpsDisplays.begin(),gpsDisplays.end(),[&](auto& p){return p.id==d.source&&p.source.empty();});if(parent==gpsDisplays.end()){d.enabled=false;displayErrors.push_back("Invalid source display: "+d.id);}else{d.sourceIndex=size_t(parent-gpsDisplays.begin());d.ui=parent->ui;}}
 }
 auto inherited=overlay_snapshot();
 for(size_t i=0;i<gpsDisplays.size();++i){auto& d=gpsDisplays[i];d.settings=d.layout=="horizontal"?default_overlay_settings(true):inherited;d.settings.theme=inherited.theme;d.markerWidth=(d.layout=="horizontal"?804:364)+unsigned(i)*4;
  if(!display_ui_path(d.ui)||(d.source.empty()&&!paths.insert(d.ui).second)){d.enabled=false;displayErrors.push_back("Invalid/duplicate UI path: "+d.id);}
  if(d.layout!="vertical"&&d.layout!="horizontal"&&d.layout!="horizontal_4_3"){d.enabled=false;displayErrors.push_back("Unknown layout: "+d.id);}
  if(d.enabled&&d.layout!="vertical"&&!ford_console_id(d.id))displayErrors.push_back("Layout not supplied yet; native UI retained: "+d.id+" / "+d.layout);
  if(std::filesystem::is_regular_file(display_settings_path(i))){std::ifstream f(display_settings_path(i));std::string magic;int version,theme;if(f>>magic>>version>>theme&&magic=="BetterGPSOverlay"&&version==2){auto s=d.settings;bool valid=true;for(size_t j=0;j<overlayItemCount;++j){int x,y;double scale;if(!(f>>x>>y>>scale)||!std::isfinite(scale)){valid=false;break;}s.items[j].x=std::clamp(x,s.horizontal?-800:-360,s.horizontal?1600:720);s.items[j].y=std::clamp(y,s.horizontal?-500:-740,s.horizontal?1000:1480);overlay_apply_scale(s,j,scale);}if(valid)d.settings=s;else displayErrors.push_back("Invalid settings; inherited defaults: "+d.id);}}
 }
}
inline size_t display_source_index(size_t i){return i<gpsDisplays.size()&&gpsDisplays[i].sourceIndex<gpsDisplays.size()?gpsDisplays[i].sourceIndex:i;}
inline OverlaySettings display_snapshot(size_t i){std::lock_guard<std::mutex> l(displayMutex);if(i==selectedDisplay.load()&&!overlayManagedSelected.load())return overlay_snapshot();return gpsDisplays[i].settings;}
inline void display_select(size_t index){if(gpsDisplays.empty()||index>=gpsDisplays.size())return;overlay_save();auto old=overlay_snapshot();{std::lock_guard<std::mutex> l(displayMutex);if(!overlayManagedSelected)gpsDisplays[selectedDisplay.load()].settings=old;overlayManagedSelected=false;auto next=gpsDisplays[index].settings;{std::lock_guard<std::mutex> lock(overlaySettingsMutex);next.revision=overlaySettings.revision+1;overlaySettings=next;overlaySettingsPath=display_settings_path(index);overlaySavedRevision=0;}selectedDisplay=index;}}
inline void display_activate_first(){if(gpsDisplays.empty())return;std::filesystem::create_directories(displayBase/"display-settings");auto initial=gpsDisplays[0].settings;std::lock_guard<std::mutex> lock(overlaySettingsMutex);initial.revision=overlaySettings.revision+1;overlaySettings=initial;overlaySettingsPath=display_settings_path(0);overlaySavedRevision=0;}
inline std::vector<uint8_t> display_bytes(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Missing display asset: "+path.string());return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
// Marker generation overwrites the entire pixel body. Read only the source
// header rather than rereading a megabyte of discarded pixels for every slot.
inline std::vector<uint8_t> display_prefix_bytes(const std::filesystem::path& path,size_t bytes){std::ifstream f(path,std::ios::binary);std::vector<uint8_t> data(bytes);if(!f.read(reinterpret_cast<char*>(data.data()),std::streamsize(bytes)))throw std::runtime_error("Short display header: "+path.string());return data;}
// A preserved layout may predate a resource rename. Normalize only virtual
// asset paths in memory; retain every coordinate, UV, node and source file.
// The generated namespace remains stable because PMD models embed its paths.
inline std::string display_layout(std::string text){
 const std::pair<const char*,const char*> paths[]={
  {"/material/ui/dashboard/googlephonenew/","/material/ui/dashboard/bettergps_overlaynew/"},
  {"/material/ui/bettergps_navigations/","/material/ui/bettergps_displays/"}
 };
 for(const auto& path:paths){size_t at=0;const size_t from=std::strlen(path.first),to=std::strlen(path.second);while((at=text.find(path.first,at))!=std::string::npos){text.replace(at,from,path.second);at+=to;}}
 return text;
}
inline void display_write(const std::filesystem::path& path,const std::vector<uint8_t>& data){std::filesystem::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(data.data()),data.size());if(!f)throw std::runtime_error("Display asset write failed: "+path.string());}
// A queued layer is only executed if a native material consumes its output.
// GPU-side replacement of the front marker cannot create that native graph edge.
// Put tiny consumers behind the existing opaque native phone background. The
// original physical plane thus keeps each custom composition alive, while its
// visible appearance and its Original renderer remain unchanged.
inline void display_link_outputs(std::string& ui){
 const unsigned samples=2*managedDeviceCount;
 // Only private map inputs are consumed by the original native phone UI.
 // The visible front is composed at Present, independently for each phone.
 size_t layerAt=0;while((layerAt=ui.find("\n layer:",layerAt))!=std::string::npos){auto first=ui.find_first_of("0123456789",layerAt+8),last=ui.find_first_not_of("0123456789",first);auto value=std::stoul(ui.substr(first,last-first));ui.replace(first,last-first,std::to_string(value+64));layerAt=first+2;}
 const std::string rootLayer="\n layer: 64";if(auto root=ui.find(rootLayer);root!=std::string::npos)ui.replace(root,rootLayer.size(),"\n layer: 0");
 const auto at=ui.find("my_children:");if(at==std::string::npos)return;const auto end=ui.find('\n',at);const unsigned count=unsigned(std::stoul(ui.substr(at+12,end-at-12)));std::string old=ui.substr(at,end-at),children="my_children: "+std::to_string(count+samples);
 for(unsigned i=count;i-->0;){const auto from="my_children["+std::to_string(i)+"]:";const auto pos=ui.find(from);if(pos!=std::string::npos)ui.replace(pos,from.size(),"my_children["+std::to_string(i+samples)+"]:");}
 for(unsigned i=0;i<samples;++i)children+="\n my_children["+std::to_string(i)+"]: _nameless.bettergps.sample"+std::to_string(i);
 ui.replace(at,old.size(),children);const auto close=ui.rfind('}');std::string nodes;
 // Reuse the existing hidden 0..63 layer bank. Visible native phone layers
 // stay at 64+, independently of how many private consumers are required.
 for(unsigned i=0;i<samples;++i){const auto image=i%2?"/material/ui/dashboard/bettergps_map_device_"+std::to_string(i/2)+".mat":"/material/ui/bettergps_displays/managed_"+std::to_string(i/2)+"_source.mat";
 nodes+="ui::text : _nameless.bettergps.sample"+std::to_string(i)+" {\n text: \"<img src="+image+" xscale=stretch yscale=stretch>\"\n coords_l: 0\n coords_r: 180\n coords_t: 370\n coords_b: 0\n area_l: 1\n area_r: 0\n area_t: 0\n area_b: 1\n id: 0\n layer: "+std::to_string(i%64)+"\n tab: -1\n pointer: -1\n my_parent: _nameless.wnd\n}\n";
 }
 ui.insert(close,nodes);
}
inline void display_generate(bool privateMapReady=true){
 displaySurfaceMounts.clear();
 const auto root=displayBase/"generated-displays";
 if(root.parent_path()!=displayBase||root.filename()!="generated-displays")throw std::runtime_error("Invalid generated display root");
 if(std::filesystem::exists(root)){if(GetFileAttributesW(root.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Generated display root is a link");for(auto& e:std::filesystem::recursive_directory_iterator(root))if(GetFileAttributesW(e.path().c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Generated display tree contains a link");std::filesystem::remove_all(root);}
 auto templateBytes=display_bytes(displayBase/"layouts/vertical/phone.sii");std::string uiTemplate=display_layout(std::string(templateBytes.begin(),templateBytes.end()));
 auto marker=display_prefix_bytes(displayBase/"BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.dds",128);
 auto tobj=display_prefix_bytes(displayBase/"BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.tobj",48);
 if(marker.size()<128||tobj.size()<48)throw std::runtime_error("Invalid display marker templates");
 for(auto& d:gpsDisplays){d.ready=false;d.independent=false;d.nativeFallback=false;if(!d.enabled||(d.layout!="vertical"&&!ford_console_id(d.id)))continue;
  if(ford_console_id(d.id)){
   auto prefix="/material/ui/bettergps_displays/"+d.id;auto dds=marker;dds.resize(128+size_t(d.markerWidth)*500*4);std::fill(dds.begin()+128,dds.end(),0);for(size_t pixel=131;pixel<dds.size();pixel+=4)dds[pixel]=255;
   uint32_t h=500,w=d.markerWidth,pitch=w*4;memcpy(dds.data()+12,&h,4);memcpy(dds.data()+16,&w,4);memcpy(dds.data()+20,&pitch,4);for(unsigned j=0;j<4;++j){auto* p=dds.data()+128+j*4;p[0]=0xA7;p[1]=0x5B;p[2]=0x37+j;p[3]=255;}
   display_write(root/(prefix.substr(1)+".dds"),dds);auto tob=tobj;tob.resize(48);auto path=prefix+".dds";uint32_t len=uint32_t(path.size());memcpy(tob.data()+40,&len,4);tob.insert(tob.end(),path.begin(),path.end());display_write(root/(prefix.substr(1)+".tobj"),tob);
   auto material="effect : \"ui.rfx\" { texture : \"texture\" { source : \""+prefix+".tobj\" u_address : clamp v_address : clamp mip_filter : none } }\n";
   display_write(root/(prefix.substr(1)+".mat"),{material.begin(),material.end()});
   // A separate semantic marker lets ATS draw the live map with its native
   // UI pipeline. Its transparent replacement restores the untouched stock page.
   {auto image=marker;const uint32_t mw=797,mh=497,mp=mw*4;image.resize(128+size_t(mw)*mh*4);std::fill(image.begin()+128,image.end(),0);memcpy(image.data()+12,&mh,4);memcpy(image.data()+16,&mw,4);memcpy(image.data()+20,&mp,4);
    for(unsigned j=0;j<4;++j){auto* p=image.data()+128+j*4;p[0]=0xA7;p[1]=0x5B;p[2]=0x3d+j;p[3]=255;}
    const auto mapPrefix=prefix+"_map";display_write(root/(mapPrefix.substr(1)+".dds"),image);auto mt=tobj;mt.resize(48);auto texture=mapPrefix+".dds";uint32_t length=uint32_t(texture.size());memcpy(mt.data()+40,&length,4);mt.insert(mt.end(),texture.begin(),texture.end());display_write(root/(mapPrefix.substr(1)+".tobj"),mt);
    auto mm="effect : \"ui.rfx\" { texture : \"texture\" { source : \""+mapPrefix+".tobj\" u_address : clamp v_address : clamp mip_filter : none } }\n";display_write(root/(mapPrefix.substr(1)+".mat"),{mm.begin(),mm.end()});}
   {auto image=marker;const uint32_t mw=795,mh=495,mp=mw*4;image.resize(128+size_t(mw)*mh*4);std::fill(image.begin()+128,image.end(),0);memcpy(image.data()+12,&mh,4);memcpy(image.data()+16,&mw,4);memcpy(image.data()+20,&mp,4);
    for(unsigned j=0;j<4;++j){auto* p=image.data()+128+j*4;p[0]=0xA7;p[1]=0x5B;p[2]=0x3e+j;p[3]=255;}
    const auto sourcePrefix=prefix+"_source";display_write(root/(sourcePrefix.substr(1)+".dds"),image);auto mt=tobj;mt.resize(48);auto texture=sourcePrefix+".dds";uint32_t length=uint32_t(texture.size());memcpy(mt.data()+40,&length,4);mt.insert(mt.end(),texture.begin(),texture.end());display_write(root/(sourcePrefix.substr(1)+".tobj"),mt);
    auto mm="effect : \"ui.rfx\" { texture : \"texture\" { source : \""+sourcePrefix+".tobj\" u_address : clamp v_address : clamp mip_filter : none } }\n";display_write(root/(sourcePrefix.substr(1)+".mat"),{mm.begin(),mm.end()});}
   auto bytes=display_bytes(displayBase/(d.id=="bronco_infotainment"?"layouts/horizontal/bronco.sii":"layouts/horizontal/f150.sii"));std::string ui=display_layout(std::string(bytes.begin(),bytes.end()));const std::string original="/material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.mat";auto at=ui.find(original);if(at==std::string::npos)throw std::runtime_error("Horizontal layout missing marker");ui.replace(at,original.size(),prefix+".mat left=0 right="+std::to_string(800.0/d.markerWidth)+" top=0 bottom=1");display_write(root/d.ui.substr(1),{ui.begin(),ui.end()});d.ready=true;continue;
  }
  auto prefix="/material/ui/bettergps_displays/"+d.id;auto dds=marker;dds.resize(128+size_t(d.markerWidth)*740*4);std::fill(dds.begin()+128,dds.end(),0);
  // ATS can draw the marker before its asynchronously streamed SRV has been
  // recognized. Its fallback must already cover the native GPS with black.
  for(size_t pixel=128+3;pixel<dds.size();pixel+=4)dds[pixel]=255;
  auto put=[&](size_t offset,uint32_t value){memcpy(dds.data()+offset,&value,4);};put(12,740);put(16,d.markerWidth);put(20,d.markerWidth*4);
  for(unsigned j=0;j<4;++j){auto* p=dds.data()+128+j*4;p[0]=0xA7;p[1]=0x5B;p[2]=0x37+j;p[3]=255;}
  display_write(root/(prefix.substr(1)+".dds"),dds);auto tob=tobj;tob.resize(48);auto texture=prefix+".dds";uint32_t len=uint32_t(texture.size());memcpy(tob.data()+40,&len,4);tob.insert(tob.end(),texture.begin(),texture.end());display_write(root/(prefix.substr(1)+".tobj"),tob);
  auto mat="effect : \"ui.rfx\" { texture : \"texture\" { source : \""+prefix+".tobj\" u_address : clamp v_address : clamp mip_filter : none } }\n";display_write(root/(prefix.substr(1)+".mat"),{mat.begin(),mat.end()});
  // SCS img V=0 is the top edge. Crop only the padded horizontal pixels;
  // keep top=0 / bottom=1 so the overlay is not mirrored vertically.
  const auto nativePath=displayBase/"layouts/vertical/native-phone.sii";const auto surface=displayBase/"layouts/vertical/surfaces"/d.id;
  d.nativeFallback=std::filesystem::is_directory(surface)&&std::filesystem::is_regular_file(nativePath);
  auto nativeBytes=d.nativeFallback?display_bytes(nativePath):std::vector<uint8_t>{};auto ui=d.nativeFallback?display_layout(std::string(nativeBytes.begin(),nativeBytes.end())):uiTemplate;std::string old="/material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.mat";auto pos=ui.find(old);if(pos==std::string::npos)throw std::runtime_error("Vertical UI has no BetterGPS marker");ui.replace(pos,old.size(),prefix+".mat left=0 right="+std::to_string(360.0/d.markerWidth)+" bottom=1 top=0");if(!privateMapReady){auto at=ui.find("/material/ui/dashboard/bettergps_map.mat");if(at!=std::string::npos)ui.replace(at,strlen("/material/ui/dashboard/bettergps_map.mat"),"/material/ui/dashboard/bare_map.mat");}if(d.nativeFallback)display_link_outputs(ui);if(d.source.empty())display_write(root/d.ui.substr(1),{ui.begin(),ui.end()});d.ready=true;
  if(std::filesystem::is_directory(surface)){
   if(GetFileAttributesW(surface.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Surface asset root is a link");
   for(const auto& entry:std::filesystem::recursive_directory_iterator(surface)){
    if(GetFileAttributesW(entry.path().c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Surface asset is a link");
    if(!entry.is_regular_file())continue;auto relative=entry.path().lexically_relative(surface);auto virtualPath="/"+relative.generic_string();
    if(virtualPath.rfind("/vehicle/",0)!=0||(entry.path().extension()!=".pmd"&&entry.path().extension()!=".pmg"))throw std::runtime_error("Invalid independent surface asset");
    display_write(root/relative,display_bytes(entry.path()));displaySurfaceMounts.insert(relative.parent_path().generic_string());
   }
   d.independent=true;d.surfaceWidth=37+unsigned(&d-gpsDisplays.data())*4;
   const auto name=prefix+"_surface";auto image=marker;image.resize(128+size_t(d.surfaceWidth)*41*4);std::fill(image.begin()+128,image.end(),0);for(size_t j=131;j<image.size();j+=4)image[j]=255;
   uint32_t w=d.surfaceWidth,h=41,pitch=w*4;memcpy(image.data()+12,&h,4);memcpy(image.data()+16,&w,4);memcpy(image.data()+20,&pitch,4);
   for(unsigned j=0;j<4;++j){auto* pixel=image.data()+128+j*4;pixel[0]=0xA7;pixel[1]=0x5B;pixel[2]=0x3A+j;pixel[3]=255;}
   display_write(root/(name.substr(1)+".dds"),image);
   auto descriptor=tobj;descriptor.resize(48);auto texture=name+".dds";uint32_t length=uint32_t(texture.size());memcpy(descriptor.data()+40,&length,4);descriptor.insert(descriptor.end(),texture.begin(),texture.end());display_write(root/(name.substr(1)+".tobj"),descriptor);
   auto material="effect : \"eut2.dif.lum.a.rfx\" { additional_ambient : 0 diffuse : { 0.132868 , 0.132868 , 0.132868 } luminance_night : "+std::string(d.ui=="/ui/dashboard/rt_phone_gps.sii"?"3.1":"10.0")+" luminance_output : 2000 reflection : 0 shininess : 40 specular : { 0.010023 , 0.010023 , 0.010023 } texture : \"texture_base\" { source : \""+name+".tobj\" sampler : default } }\n";
   display_write(root/(name.substr(1)+".mat"),{material.begin(),material.end()});
  }


 }
}
}
