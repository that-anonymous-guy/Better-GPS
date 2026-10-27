#pragma once
#include "DeviceManagerState.h"
namespace bettergps_overlay {
inline unsigned managed_marker_width(unsigned slot,unsigned role){return role==0?911+slot*2:role==1?611+slot*2:641+slot*2;}
inline unsigned managed_marker_height(unsigned role){return role==0?751:role==1?23:19;}
// The physical marker identifies a slot and stays fixed. A logical canvas may
// exceed either axis; its stored content must fit before computing UI UVs.
inline std::pair<unsigned,unsigned> managed_marker_content(unsigned slot,unsigned w,unsigned h){
 if(!w||!h)return {0,0};const double fit=std::min({1.,double(managed_marker_width(slot,0))/w,double(managed_marker_height(0))/h});
 return {std::min(managed_marker_width(slot,0),std::max(1u,unsigned(std::lround(w*fit)))),std::min(managed_marker_height(0),std::max(1u,unsigned(std::lround(h*fit))))};
}
inline unsigned managed_marker_role(unsigned slot,unsigned role){return 40+slot*3+role;}
inline std::string managed_prefix(unsigned slot,unsigned role){return "/material/ui/bettergps_displays/managed_"+std::to_string(slot)+(role==0?"_overlay":role==1?"_source":"_end");}
inline std::string managed_map_name(unsigned slot){return "/material/ui/dashboard/bettergps_map_device_"+std::to_string(slot);}
inline std::string managed_ui_name(unsigned slot,uint64_t revision,bool orange=false,bool camera=false,bool nativeRear=false){return "/ui/dashboard/bettergps_device_"+std::to_string(slot)+(orange?"_orange":"_"+std::to_string(revision))+(camera?nativeRear?"_camera_native_rear":"_camera":"")+".sii";}
inline void managed_texture_file(const std::string& prefix,unsigned w,unsigned h,unsigned role,bool orange=false){
 auto root=managedBase/"generated-displays";auto image=display_prefix_bytes(managedBase/"BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.dds",128);image.resize(128+size_t(w)*h*4);std::fill(image.begin()+128,image.end(),0);const uint32_t pitch=w*4;memcpy(image.data()+12,&h,4);memcpy(image.data()+16,&w,4);memcpy(image.data()+20,&pitch,4);
 if(orange){image[128]=0;image[129]=147;image[130]=255;image[131]=255;}else for(unsigned j=0;j<4;++j){auto* p=image.data()+128+j*4;p[0]=0xA7;p[1]=0x5B;p[2]=uint8_t(0x31+role+j);p[3]=255;}
 display_write(root/(prefix.substr(1)+".dds"),image);auto tobj=display_prefix_bytes(managedBase/"BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.tobj",48);tobj.resize(48);auto path=prefix+".dds";uint32_t length=uint32_t(path.size());memcpy(tobj.data()+40,&length,4);tobj.insert(tobj.end(),path.begin(),path.end());display_write(root/(prefix.substr(1)+".tobj"),tobj);
 auto material="effect : \"ui.rfx\" { texture : \"texture\" { source : \""+prefix+".tobj\" u_address : clamp v_address : clamp mip_filter : none } }\n";display_write(root/(prefix.substr(1)+".mat"),{material.begin(),material.end()});
}
inline std::string managed_image_node(const std::string& id,const std::string& image,unsigned w,unsigned h,unsigned layer,unsigned x=0,unsigned y=0){return "ui::text : _nameless."+id+" {\n text: \"<img src="+image+" xscale=stretch yscale=stretch>\"\n coords_l: "+std::to_string(x)+"\n coords_r: "+std::to_string(x+w)+"\n coords_t: "+std::to_string(y+h)+"\n coords_b: "+std::to_string(y)+"\n area_l: 1\n area_r: 0\n area_t: 0\n area_b: 1\n id: 0\n layer: "+std::to_string(layer)+"\n tab: -1\n pointer: -1\n my_parent: _nameless.wnd\n}\n";}
inline void managed_build_ui(unsigned slot,const ManagedDevice& d,bool orange=false,bool camera=false,bool nativeRear=false){
 const auto canvas=managed_canvas(d);const unsigned w=canvas.first,h=canvas.second;const auto root=managedBase/"generated-displays";
 const bool cameraCapable=!orange&&d.style==3&&managed_is_copilot(d)&&d.vehicle.find("t680_2022")!=std::string::npos;
 const bool known=d.known>=0&&size_t(d.known)<gpsDisplays.size();
 // A captured/non-standard screen has its own generated marker surface. Use
 // that surface as the UI coordinate space so the horizontal 800x500 preset
 // is fitted inside it instead of being stretched by ATS a second time.
 const auto renderSize=known?std::pair<unsigned,unsigned>{gpsDisplays[d.known].layout=="horizontal"?800u:360u,gpsDisplays[d.known].layout=="horizontal"?500u:740u}:managed_render_dimensions(d.nativeWidth?d.nativeWidth:w,d.nativeHeight?d.nativeHeight:h);
 const unsigned vw=renderSize.first,vh=renderSize.second;
 unsigned fw=0,fh=0;
 if(d.style==2&&d.ui=="/ui/dashboard/gps.dlc_toys.sii"){const double physical=1200./672.,content=double(w)/h;fw=unsigned(std::lround(vw*std::min(1.,content/physical)));fh=unsigned(std::lround(vh*std::min(1.,physical/content)));}
 else{const double fit=std::min(double(vw)/w,double(vh)/h);fw=unsigned(std::lround(w*fit));fh=unsigned(std::lround(h*fit));}
 unsigned fx=(vw-fw)/2,fy=(vh-fh)/2;
 // Each supported NavPlus mesh exposes its own measured part of the square GPS texture.
 // Fit into that visible region, for every selectable style, not the full DDS.
 if(managed_is_international(d)||managed_is_navplus(d)||managed_is_copilot(d)||managed_is_zonar(d)||managed_is_icc5(d)||managed_is_mack(d)||(managed_is_volvo18(d)||managed_is_volvo25(d))||managed_is_vnl2014(d)){const auto p=managed_navplus_rect(d,vw,vh,w,h);fx=p.x;fy=p.y;fw=p.w;fh=p.h;}
 if(!orange)for(unsigned role=0;role<3;++role)if(!std::filesystem::is_regular_file(root/(managed_prefix(slot,role).substr(1)+".dds")))managed_texture_file(managed_prefix(slot,role),managed_marker_width(slot,role),managed_marker_height(role),managed_marker_role(slot,role));
 std::string ui="SiiNunit\n{\nui::window : _nameless.wnd {\n window_handler: null\n clip_children: true\n keep_aspect: none\n user_string_data: \"\"\n first_direction_focus_id: 0\n fitting: false\n my_children: "+std::to_string(orange?1:cameraCapable&&camera?5:4)+"\n";
 for(unsigned i=0;i<(orange?1u:cameraCapable&&camera?5u:4u);++i)ui+=" my_children["+std::to_string(i)+"]: _nameless.device"+std::to_string(i)+"\n";
 ui+=" coords_l: 0\n coords_r: "+std::to_string(vw)+"\n coords_t: "+std::to_string(vh)+"\n coords_b: 0\n area_l: 0\n area_r: "+std::to_string(vw)+"\n area_t: "+std::to_string(vh)+"\n area_b: 0\n id: 0\n layer: 0\n tab: -1\n pointer: -1\n my_parent: null\n}\n";
 if(orange)ui+=managed_image_node("device0","/material/ui/bettergps_displays/managed_orange.mat",w,h,0);
 else{
 ui+=managed_image_node("device0",camera?"/material/ui/white.mat color=FF0A0A0A":managed_prefix(slot,1)+".mat",vw,vh,0);
 std::string mapUv=d.horizontal?"left=0 right=1 top=0.8125 bottom=0.1875":"left=0.375 right=0.625 top=0.7569444444 bottom=0.2430555556";
 if(d.style>=2||managed_format(d)>=2){std::ostringstream uv;uv.precision(12);uv<<"left=0 right=1 top="<<.5+double(h)/(2*w)<<" bottom="<<.5-double(h)/(2*w);mapUv=uv.str();}
 if(camera){
  // Same native parking material as the Ford/Bronco. Keep the horizontal FOV,
  // crop its square render to the app body, flip Y and undo mirror X.
  std::ostringstream uv;uv.precision(12);uv<<"/material/environment/mirror/park_1x1_ui.mat "<<(nativeRear?"left=0 right=1":"left=1 right=0")<<" top="<<(nativeRear?1.:.5+420./1800)<<" bottom="<<(nativeRear?0.:.5-420./1800);
  const auto bodyBottom=unsigned(std::lround(fh*61./544)),bodyHeight=unsigned(std::lround(fh*420./544));
  ui+=managed_image_node("device1",uv.str(),fw,bodyHeight,1,fx,fy+bodyBottom);
 }else ui+=managed_image_node("device1",managed_map_name(slot)+".mat "+mapUv,fw,fh,1,fx,fy);
 const auto stored=managed_marker_content(slot,w,h);std::ostringstream crop;crop.precision(12);crop<<managed_prefix(slot,0)<<".mat left=0 right="<<double(stored.first)/managed_marker_width(slot,0)<<" top=0 bottom="<<double(stored.second)/managed_marker_height(0);ui+=managed_image_node("device2",crop.str(),fw,fh,2,fx,fy);
 ui+=managed_image_node("device3",managed_prefix(slot,2)+".mat",vw,vh,3);
 if(cameraCapable&&camera){
  // Native parking-page metadata registers camera producer 7. The actual
  // visible image above remains independent of the stock reverse-only group.
  ui+="ui::group : _nameless.device4 {\n fitting: false\n my_children: 1\n my_children[0]: _nameless.camera_meta\n coords_l: 0\n coords_r: "+std::to_string(vw)+"\n coords_t: "+std::to_string(vh)+"\n coords_b: 0\n id: 910\n layer: 0\n tab: -1\n pointer: -1\n my_parent: _nameless.wnd\n}\n";
  auto meta=managed_image_node("camera_meta","/material/environment/mirror/park_1x1_ui.mat top=1 bottom=0",1,1,0);
  meta.replace(meta.find("<img src="),strlen("<img src="),"<color value=00000000><img src=");
  const auto parent=meta.find("my_parent: _nameless.wnd");meta.replace(parent,strlen("my_parent: _nameless.wnd"),"my_parent: _nameless.device4");ui+=meta;
 }
 }
 if(camera)ui.replace(ui.find("<img src=/material/ui/white.mat color=FF0A0A0A"),strlen("<img src=/material/ui/white.mat color=FF0A0A0A"),"<color value=FF0A0A0A><img src=/material/ui/white.mat");
 ui+="}\n";display_write(root/managed_ui_name(slot,d.revision,orange,camera,nativeRear).substr(1),{ui.begin(),ui.end()});
 if(cameraCapable&&!camera){managed_build_ui(slot,d,false,true);managed_build_ui(slot,d,false,true,true);}
}
inline void managed_build_templates(){
 managed_texture_file("/material/ui/bettergps_displays/managed_orange",1,1,0,true);
 for(unsigned i=0;i<managedDeviceCount;++i){auto d=managed_snapshot(i);managed_texture_file(managed_prefix(i,1),managed_marker_width(i,1),managed_marker_height(1),managed_marker_role(i,1));managed_build_ui(i,d,true);if(d.used)managed_build_ui(i,d);}
}
}
