#pragma once
namespace copilot_background_apps_fixture {
using namespace bettergps_overlay;
inline void run(const std::filesystem::path& out){
 std::filesystem::create_directories(out);managed_init(out);g_assetRoot=std::filesystem::absolute("BetterGPS/Overlay/BetterGPS_assets").u8string();managedSelected=-1;overlayManagedSelected=false;displayVehicle="kenworth.t680_2022";vehicleStart.epoch=1401;vehicleStart.eligible=true;g_tripPresent=false;
 ManagedDevice d;d.used=d.enabled=d.horizontal=true;d.vehicle=displayVehicle;d.ui="/ui/dashboard/kenworth_t680_2022_gps.sii";d.alias="/vehicle/truck/share/gps.tobj";d.style=3;d.format=3;d.prefs.bootHome=0;{DisplayStyleScope style(3);d.overlay=default_overlay_settings(true,3,4);}managedDevices[0]=managedDevices[1]=d;uint64_t now=10000;
 auto page=[&](unsigned delta,bool visible=true,bool covered=false){return managed_page_sample(0,d,now+=delta,g_tripPresent,visible,covered);};
 auto render=[&](PageVisual visual){DisplayStyleScope style(3);DevicePageScope scope(visual.page);std::vector<uint8_t> pixels;render_copilot(pixels,d.overlay);copilot_interaction(pixels,d.overlay,visual,{});return pixels;};
 page(1,false);managed_page_destination(0,true);page(1);managed_page_destination(0,false);const auto first=page(1);Check(first.copilotLoading&&first.transition==1&&!first.backgroundApps,"First CoPilot launch not cold");
 auto logo=first;logo.elapsed=820;Save(out/"first-launch-logo.bmp",render(logo),900,544);Check(page(3819).transition==1,"First launch lost its three-second logo");const auto running=page(1);Check(!running.transition&&!running.copilotLoading&&(running.backgroundApps&CopilotRunning),"CoPilot is not resident after initialization");
 const auto bounds=copilot_background_icon_bounds(d.overlay,0),next=copilot_background_icon_bounds(d.overlay,1);Check(bounds.x==13&&bounds.y==12&&bounds.w==32&&bounds.h==32,"Status icon moved from original top artwork registration");Check(next.x>bounds.x+bounds.w&&next.y==bounds.y,"Future status icons have no consistent horizontal spacing");
 auto withIcon=render(running);auto absent=running;absent.backgroundApps=0;const auto withoutIcon=render(absent);Check(withIcon!=withoutIcon,"Resident status icon missing");for(int y=0;y<544;++y)for(int x=0;x<900;++x)if(x<bounds.x||x>=bounds.x+bounds.w||y<bounds.y||y>=bounds.y+bounds.h){const auto at=(y*900+x)*4;Check(std::equal(withIcon.begin()+at,withIcon.begin()+at+4,withoutIcon.begin()+at),"Status icon changed unrelated UI");}Save(out/"resident-map.bmp",withIcon,900,544);
 managed_page_destination(0,true);const auto home=page(1);Check(home.backgroundApps&CopilotRunning,"Home closes the background app");page(570);Save(out/"resident-home.bmp",render(page(1)),900,544);
 managed_page_clock(0);page(1);Check(page(820).backgroundApps&CopilotRunning,"Clock loses CoPilot residency");managed_page_destination(0,true);page(1);page(570);managed_page_destination(0,false);const auto warm=page(1);Check(warm.transition==1&&!warm.copilotLoading&&(warm.backgroundApps&CopilotRunning),"Resident app relaunches cold");Check(page(819).transition==1&&page(1).transition==0,"Warm resume still waits for the logo");
 ++managedPower[0].serial;page(10000,false);const auto repowered=page(1);Check(repowered.backgroundApps&CopilotRunning&&!repowered.copilotLoading&&!repowered.transition,"Ignition/power cycle discards resident state");
 page(1,true,true);managed_pages_pause();Check(page(100000).backgroundApps&CopilotRunning,"Pause loses resident state");
 const auto other=managed_page_sample(1,d,10000,false,true);Check(other.copilotLoading&&!other.backgroundApps,"Device slots share resident state");
 ++vehicleStart.epoch;const auto reset=page(1);Check(reset.copilotLoading&&!reset.backgroundApps,"New vehicle initialization did not reset resident apps and auto-start CoPilot");managed_page_destination(0,true);page(100);page(10000);managed_page_destination(0,false);Check(page(1).copilotLoading,"Cancelled first launch incorrectly marks app initialized");
 std::cout<<"PASS: cold boot/logo, resident status icon and spacing, warm resume, Home/Clock, power/pause retention, per-device isolation, initialization reset and cancelled launch.\n";
}
}
