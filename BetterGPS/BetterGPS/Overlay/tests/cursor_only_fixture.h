#pragma once
namespace cursor_only_fixture {
using namespace bettergps_overlay;
inline void run(const std::filesystem::path& out){
 displayFixtureModifiers=0;std::filesystem::create_directories(out);managedDevices={};managedSelected=-1;devicePageShortcutConflict=nullptr;displayPageKey=VK_F9;displayPageMods=2;
 const auto config=out/"shortcuts.ini";display_power_load(config);Check(settingsMenuKey==VK_F9&&settingsMenuMods==0&&displayPowerMods==1&&DevicePreferences{}.pageMods==2,"Requested shortcut defaults missing");Check(cursorOnlyKey==VK_RBUTTON&&!cursorOnlyToggle&&cursorOnlyDoubleTap,"Cursor must default to right mouse / hold with double press enabled");
 cursorOnlyHeld=false;Check(cursor_only_sample(true,true)&&cursor_only_sample(true,true)&&!cursor_only_sample(false,true),"Hold mode did not follow press/release");cursorOnlyToggle=1;Check(cursor_only_sample(true,true)&&cursor_only_sample(true,true)&&cursor_only_sample(false,true),"Toggle repeats or stops on release");Check(!cursor_only_sample(true,true),"Second toggle press did not stop cursor");cursor_only_sample(false,true);cursor_only_sample(true,true);Check(!cursor_only_sample(true,false)&&!cursor_only_sample(true,true),"Focus loss leaves toggle active or retriggers held key");cursor_only_sample(false,true);
 // Deterministic double presses, including the real apply/release path between
 // taps. No wall-clock sleeps or OS mouse injection are required here.
 const uint64_t window=GetDoubleClickTime(),tap=std::max<uint64_t>(1,window/4);
 cursorOnlyToggle=0;cursorOnlyDoubleTap=1;cursor_only_release(false);cursorOnlyHeld=false;
 Check(cursor_only_sample(true,true,1000),"First tap must activate Hold immediately");
 Check(!cursor_only_sample(false,true,1000+tap),"First release must end ordinary Hold");cursor_only_apply();
 Check(cursor_only_sample(true,true,1000+tap*2)&&cursor_only_sample(false,true,1000+tap*3),"Double press did not remain active after release/apply");
 Check(cursor_only_sample(false,true,1000+window*4),"Double-press latch expired without another press");
 Check(!cursor_only_sample(true,true,1000+window*4+1)&&!cursor_only_sample(true,true,1000+window*4+2),"Exit press reactivated Hold while still pressed");
 Check(!cursor_only_sample(false,true,1000+window*4+3),"Exit release reactivated cursor");
 Check(cursor_only_sample(true,true,1000+window*4+4),"Hold did not recover after leaving latch");cursor_only_release(false);cursorOnlyHeld=false;
 Check(cursor_only_sample(true,true,10000)&&cursor_only_sample(true,true,10000+window+1)&&!cursor_only_sample(false,true,10000+window+2),"Long hold incorrectly became a toggle");
 Check(cursor_only_sample(true,true,10000+window+3)&&!cursor_only_sample(false,true,10000+window+4),"Slow successive presses became a double press");
 cursor_only_release(false);cursorOnlyHeld=false;cursorOnlyDoubleTap=0;
 Check(cursor_only_sample(true,true,20000)&&!cursor_only_sample(false,true,20000+tap)&&cursor_only_sample(true,true,20000+tap*2)&&!cursor_only_sample(false,true,20000+tap*3),"Disabled double press still latches");
 cursorOnlyDoubleTap=1;cursor_only_release(false);cursorOnlyHeld=false;
 cursor_only_sample(true,true,30000);cursor_only_sample(false,true,30000+tap);cursor_only_sample(true,true,30000+tap*2);
 Check(!cursor_only_sample(true,false,30000+tap*3)&&!cursor_only_sample(false,true,30000+tap*4),"Focus loss failed to clear double-press latch");
 cursor_only_sample(true,true,40000);cursor_only_sample(false,true,40000+tap);cursor_only_release(false);
 Check(cursor_only_sample(true,true,40000+tap*2)&&!cursor_only_sample(false,true,40000+tap*3),"Opening menu failed to clear pending double press");
 cursor_only_sample(true,true,50000);cursor_only_sample(false,true,50000+tap);cursorOnlyKey=VK_MBUTTON;
 Check(cursor_only_sample(true,true,50000+tap*2)&&!cursor_only_sample(false,true,50000+tap*3),"Rebinding joined taps from two different shortcuts");cursorOnlyKey=VK_RBUTTON;
 Check(cursor_only_message_key(WM_RBUTTONDBLCLK,0)==VK_RBUTTON&&cursor_only_message_key(WM_XBUTTONDBLCLK,MAKEWPARAM(0,XBUTTON2))==VK_XBUTTON2,"Windows double-click messages not recognized");
 cursorOnlyKey='G';cursorOnlyMods=4;cursor_only_release(false);cursorOnlyHeld=false;
 Check(cursor_only_sample(true,true,60000)&&!cursor_only_sample(false,true,60000+tap)&&cursor_only_sample(true,true,60000+tap*2)&&cursor_only_sample(false,true,60000+tap*3),"Configured keyboard chord cannot double-press latch");cursor_only_release(false);cursorOnlyHeld=false;
 Check(!cursor_shortcut_set(VK_F9,0)&&!cursor_shortcut_set(VK_F9,1)&&!cursor_shortcut_set(VK_F9,2)&&!cursor_shortcut_set(VK_LBUTTON,0),"Cursor steals another shortcut or primary touch button");Check(cursor_shortcut_set('G',4),"Cursor keyboard chord rejected");Check(!settings_menu_shortcut('G',4)&&!display_key_valid('G',4)&&!managed_page_shortcut_valid('G',4),"Another action steals cursor shortcut");cursorOnlyToggle=1;cursorOnlyDoubleTap=0;Check(display_power_save(),"Cursor configuration save failed");display_power_load(config);Check(cursorOnlyKey=='G'&&cursorOnlyMods==4&&cursorOnlyToggle==1&&!cursorOnlyDoubleTap,"Cursor mode/key/double-press option not persisted");
 // Exercise real shortcut rows and mouse-button capture, without opening a
 // game menu or replacing the existing tactile action pipeline.
 settingsOpen=true;settingsCategory=1;settingsSearch.clear();settingsFirst=0;auto rows=settings_matches();auto at=std::find(rows.begin(),rows.end(),52);Check(at!=rows.end(),"Cursor row absent from Shortcuts");settings_hit(500,170+int(at-rows.begin())*50+10);Check(settingsCaptureCursorKey,"Cursor capture not armed");settings_wndproc(nullptr,WM_RBUTTONDOWN,0,0);Check(cursorOnlyKey==VK_RBUTTON&&!settingsCaptureCursorKey,"Right mouse capture failed");
 settingsCaptureCursorKey=true;settings_wndproc(nullptr,WM_KEYDOWN,VK_F9,0);Check(settingsShortcutConflict&&settingsCaptureCursorKey,"Conflicting cursor key has no feedback");settings_wndproc(nullptr,WM_KEYDOWN,VK_ESCAPE,0);Check(settingsOpen&&!settingsCaptureCursorKey,"Escape closed menu instead of cancelling cursor capture");
 rows=settings_matches();at=std::find(rows.begin(),rows.end(),54);Check(at!=rows.end(),"Double-press option absent from Shortcuts");const int optionY=170+int(at-rows.begin())*50+10;
 settings_hit(500,optionY);Check(cursorOnlyDoubleTap,"Enabled button did not enable double press");Check(display_power_save(),"Enabled double-press option save failed");display_power_load(config);Check(cursorOnlyDoubleTap,"Enabled double-press option not restored");
 settings_hit(700,optionY);Check(!cursorOnlyDoubleTap,"Disabled button did not disable double press");settings_hit(500,optionY);
 settingsOpen=false;settingsWindow=GetForegroundWindow();Check(settingsWindow,"No foreground test window");renderStopping=false;renderGameplayEnabled=false;cursorOnlyKey=VK_RBUTTON;cursorOnlyMods=0;cursorOnlyToggle=0;cursorOnlyFixtureDown=1;Check(cursor_only_poll()&&settings_capture_mouse_input()&&settings_owns_mouse()&&touch_active(),"Cursor-only mode does not own input/tactile without settings UI");Check(!settingsOpen,"Cursor-only opened the menu");
 const auto originalClip=settingsOriginalClipCursor;const auto originalWarp=settingsOriginalSetCursorPos;static int clips=0,warps=0;settingsOriginalClipCursor=+[](const RECT*)->BOOL{++clips;return TRUE;};settingsOriginalSetCursorPos=+[](int,int)->BOOL{++warps;return TRUE;};cursor_only_apply();Check(cursorOnlyEngaged&&clips==1,"Cursor not released from game clip");settings_set_cursor(0,0);settings_clip_cursor(nullptr);Check(!warps&&clips==1,"Game can recenter or confine cursor-only mode");cursorOnlyFixtureDown=0;cursor_only_poll();cursor_only_apply();Check(!cursorOnlyEngaged&&!touch_active()&&clips==2&&warps==1,"Release failed to restore original cursor policy");
 settingsOriginalClipCursor=originalClip;settingsOriginalSetCursorPos=originalWarp;cursorOnlyFixtureDown=-1;settingsWindow=nullptr;
 // Legacy defaults migrate once; a current custom chord remains unchanged.
 {std::ofstream f(config);f<<"MenuKey=120\nMenuModifiers=2\n";}display_power_load(config);Check(settingsMenuMods==0,"Legacy menu default did not migrate");{std::ofstream f(config);f<<"ShortcutDefaultsVersion=2\nMenuKey=71\nMenuModifiers=4\n";}display_power_load(config);Check(settingsMenuKey=='G'&&settingsMenuMods==4,"Migration overwrote custom menu shortcut");
 const auto profiles=out/"profiles";std::filesystem::create_directories(profiles);{std::ofstream f(profiles/"BetterGPSDevices.ini");f<<"[device.0]\nvehicle=kenworth.t680_2022\nui=/ui/dashboard/kenworth_t680_2022_gps.sii\nalias=/vehicle/truck/share/gps.tobj\npage_key=120\npage_modifiers=3\nprefs_version=1\n";}managed_init(profiles);Check(managedDevices[0].prefs.pageMods==2&&managedDirty,"Legacy Home/Map default did not migrate");Check(managed_save(),"Migrated shortcut not saved");managed_init(profiles);Check(managedDevices[0].prefs.pageMods==2&&!managedDirty,"Shortcut migration repeats");
 // A real queued tactile action must reach the existing device worker while
 // the settings menu and dock remain closed.
 auto device=managedDevices[0];device.used=device.enabled=device.horizontal=true;device.style=3;device.format=3;device.prefs.bootHome=0;{DisplayStyleScope style(3);device.overlay=default_overlay_settings(true,3,4);}managedDevices[0]=device;displayVehicle=device.vehicle;vehicleStart.eligible=true;++vehicleStart.epoch;managedPower[0].on=true;managedPower[0].phase=copilotBootMs+100;managedPower[0].serial=9876;
 managed_page_sample(0,device,1000,false,true);managed_page_destination(0,true);managed_page_sample(0,device,1001,false,true);managed_page_sample(0,device,5001,false,true);
 settingsOpen=false;settingsWindow=GetForegroundWindow();cursorOnlyFixtureDown=1;cursor_only_poll();const auto artwork=device.overlay.items[1];TouchHit hit;hit.valid=true;hit.trace=99;hit.u=(artwork.x+artwork.w*816.f/1324)/900.f;hit.v=(artwork.y+artwork.h*580.f/800)/544.f;hit.surface={0,0,0,1,1,vehicleStart.epoch.load(),managedPower[0].serial.load(),managed_page_visual(0).serial};{std::lock_guard<std::mutex> lock(touchInput.mutex);touchInput.actions.push_back({hit,hit,false});}touch_actions_worker();Check(managed_page_sample(0,device,5002,false,true).page==DevicePage::Clock&&!settingsOpen,"Cursor-only tactile action did not open Clock without menu");cursorOnlyFixtureDown=-1;cursorOnlyActive=false;settingsWindow=nullptr;
 settingsMenuKey=VK_F9;settingsMenuMods=0;managedSelected=0;device_preferences_to_editor(device.prefs,{});std::vector<uint8_t> pixels;settingsCategory=1;settingsOpen=true;settings_render(pixels,2,false);Save(out/"shortcuts.bmp",pixels,1920,1440);settingsOpen=false;managedSelected=-1;
 std::cout<<"PASS: cursor-only hold/toggle/double press, disabled option and persistence, focus/rebind/menu resets, real mouse capture, shortcut conflicts/defaults/migration, tactile activation and cursor warp/clip isolation.\n";
}
}
