#pragma once
namespace navplus_t680_fixture {
using namespace bettergps_overlay;
inline void run(const std::filesystem::path& out){
 std::filesystem::create_directories(out);g_assetRoot=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets").u8string();managed_init(out);gpsDisplays.clear();displayVehicle="kenworth.t680";
 std::vector<uint8_t> owner(0x1600),model(0x198),layer(0x168),window(64);
 *(uintptr_t*)(owner.data()+0x128)=uintptr_t(layer.data());*(uintptr_t*)(owner.data()+0xc8)=uintptr_t(window.data());
 DeviceCandidate c;c.owner=uintptr_t(owner.data());c.pair=uintptr_t(owner.data()+0x128);c.windowSlot=uintptr_t(owner.data()+0xc8);c.layer=uintptr_t(layer.data());c.window=uintptr_t(window.data());c.model=uintptr_t(model.data());c.vehicle=displayVehicle;c.ui="/ui/gps.sii";c.alias="/vehicle/truck/share/gps.tobj";c.width=c.height=256;c.seen=100;
 managedCandidates.push_back(c);managed_native_defaults(owner.data(),101);auto d=managed_snapshot(0);
 Check(d.used&&d.enabled&&d.name=="Kenworth T680 NavPlus"&&d.format==3&&d.style==2&&d.splash=="garmin/kenworth_navplus"&&d.overlay.navplus,"Observed T680 did not acquire its own 5:3 NavPlus defaults");
 Check(managed_canvas(d)==std::make_pair(900u,547u)&&managed_settings_path(0,d).filename()=="device_0_5by3_navplus.cfg","T680 canvas/profile does not preserve its device format");
 for(const auto* vehicle:{"kenworth.t680_2022","kenworth.t680_2025","peterbilt.589"})Check(!managed_navplus_identity(vehicle,c.ui,c.alias),"An unrelated dashboard matched T680 classic");
 Check(!managed_navplus_identity(c.vehicle,"/ui/dashboard/kenworth_t680.sii",c.alias)&&!managed_navplus_identity(c.vehicle,"/ui/dashboard/gps.dlc_toys.sii","/vehicle/truck/upgrade/interior_decors/toyglass/gps_ui.tobj"),"T680 cluster/accessory mistaken for integrated NavPlus");
 managedDevices[0].enabled=false;managed_native_defaults(owner.data(),102);Check(!managed_snapshot(0).enabled&&!managed_snapshot(1).used,"T680 automatic setup overwrote a saved Original choice");managedDevices[0]=d;
 for(const auto* name:{"home_kenworth_navplus_5by3.dds","kenworth_navplus_5by3_below_frame.dds","splash_kenworth_navplus_5by3.dds"}){const auto& a=garmin_image(name,"kenworth_navplus");Check(a.width==1275&&a.height==775&&a.pixels.size()==1275*775*4,"T680 non-block-aligned source DDS lost its original dimensions/pixels");}
 DisplayStyleScope style(2);const auto w900=default_overlay_settings(true,2,true);const auto t680=default_overlay_settings(true,3,true);
 Check(t680.items[1].h==547&&w900.items[1].h==600&&t680.items[8].x==657&&t680.items[4].x==796,"T680 modules were not anchored independently");
 g_haveGameTime=g_haveNavTime=g_haveTruckSpeed=g_haveSpeedLimit=g_haveCompassHeading=true;g_gameTimeMinutes=11*60+11;g_multiplayerTimeOffset=0;g_navTimeSec=6*3600+14*60;g_truckSpeedMps=22.352f;g_speedLimitMps=29.0576f;g_compassHeading=.875f;navplusTemperatureF=70;navplusHaveTemperature=true;
 for(bool dark:{false,true})for(DevicePage page:{DevicePage::Home,DevicePage::Notrip,DevicePage::Nav}){
  DevicePageScope scope(page);DisplayThemeScope theme(dark);g_tripPresent=page==DevicePage::Nav;std::vector<uint8_t> pixels;render_navplus(pixels,t680);Check(pixels.size()==900*547*4,"T680 CPU render has wrong dimensions");
  if(page==DevicePage::Home){for(size_t i=3;i<pixels.size();i+=4)Check(pixels[i]==255,"T680 Home leaks the map");}
  else{Check(pixels[(200*900+450)*4+3]==0,"T680 frame hides native map");const unsigned alpha=pixels[(430*900+260)*4+3];Check(page==DevicePage::Nav?alpha>240:alpha==0,"T680 Notrip still paints Arrival or Nav lost its Arrival plate");}
  Save(out/(std::string(page==DevicePage::Home?"home":page==DevicePage::Notrip?"notrip":"nav")+(dark?"-dark.bmp":"-light.bmp")),pixels,900,547);
 }
 std::vector<uint8_t> first(900*547*4,255),last=first;managed_startup(first,900,547,0,d,0);managed_startup(last,900,547,0,d,4999);Check(first==last&&managed_garmin_splash_ms(d)==5000,"T680 splash is not stable for five seconds");Save(out/"splash.bmp",first,900,547);
 const auto markers=out/"BetterGPS_assets/material/ui/dashboard/bettergps_overlaynew";std::filesystem::create_directories(markers);for(const auto* name:{"phone_layout_marker.dds","phone_layout_marker.tobj"})std::filesystem::copy_file(std::filesystem::path(g_assetRoot)/"material/ui/dashboard/bettergps_overlaynew"/name,markers/name,std::filesystem::copy_options::overwrite_existing);
 const auto rect=managed_navplus_rect(d,1024,1024,900,547);Check(rect.x==1&&rect.y==201&&rect.w==1021&&rect.h==621,"T680 content ignores measured native UV exposure");
 managed_build_ui(0,d);const auto bytes=display_bytes(out/"generated-displays"/managed_ui_name(0,d.revision).substr(1));const std::string ui(bytes.begin(),bytes.end());Check(ui.find("coords_l: 1\n coords_r: 1022\n coords_t: 822\n coords_b: 201")!=std::string::npos,"T680 generated UI and touch content rectangle disagree");
 managedDevices[1]=d;managedDevices[1].vehicle="kenworth.w900";managedDevices[1].format=2;managedDevices[1].overlay=w900;
 managed_select(0);overlay_adjust(4,0,-11);overlay_adjust(4,2,10);Check(overlay_save(),"T680 profile save failed");const auto custom=overlay_snapshot();Check(custom.items[4].x==785&&custom.items[4].w==103,"T680 editing uses W900 base geometry");
 managed_edit(0,-1);Check(managed_format(managed_snapshot(0))==2,"T680 format switch failed");managed_edit(0,1);Check(managed_format(managed_snapshot(0))==3&&overlay_snapshot().items[4].x==785&&overlay_snapshot().scales[4]==1.1,"T680 independent 5:3 profile was lost on format switch");
 managed_flush_selection();managedDirty=true;Check(managed_save(),"T680 device save failed");managed_init(out);Check(managed_snapshot(0).format==3&&managed_snapshot(0).overlay.items[4].x==785&&managed_snapshot(1).overlay.items[4].x==790,"T680 reload modified another vehicle's profile");
 managedSelected=-1;overlayManagedSelected=false;std::cout<<"PASS: observed-only T680 classic NavPlus, measured UV placement, pixel-exact 1275x775 art, 5-second splash, independent 5:3 profiles/modules/format, Home/Notrip/Nav day/night previews.\n";
}
}
