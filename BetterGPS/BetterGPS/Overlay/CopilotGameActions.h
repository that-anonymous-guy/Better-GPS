#pragma once
// The epoch travels with the posted request, so a truck switch cancels it.
// Window-thread requests only. Never call game actions from the preparation
// worker, and never change the user's bindings or route structures in memory.
inline constexpr UINT copilotGameMessage=WM_APP+0x46b;
inline constexpr UINT_PTR copilotMapKeyTimer=0x42475053;
inline bool copilotMapKeyDown=false;
inline void copilot_release_map_key(HWND window){
 KillTimer(window,copilotMapKeyTimer);if(!copilotMapKeyDown)return;INPUT input{};input.type=INPUT_KEYBOARD;input.ki.wScan=0x32;input.ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;SendInput(1,&input,sizeof(input));copilotMapKeyDown=false;
}
inline bool copilot_post_game_map(){return settingsWindow&&PostMessageW(settingsWindow,copilotGameMessage,1,LPARAM(vehicleStart.epoch.load()));}
inline bool(*copilotGameMapRequest)()=&copilot_post_game_map;
inline bool copilot_game_message(HWND window,UINT message,WPARAM param,LPARAM epoch){
 if(message==WM_TIMER&&param==copilotMapKeyTimer){copilot_release_map_key(window);return true;}
 if(message!=copilotGameMessage)return false;
 if(param!=1||uint64_t(epoch)!=vehicleStart.epoch||GetForegroundWindow()!=window||!render_gpu_allowed()||!vehicleStart.eligible)return true;
 settings_close();copilot_release_map_key(window);
 INPUT input{};input.type=INPUT_KEYBOARD;input.ki.wScan=0x32;input.ki.dwFlags=KEYEVENTF_SCANCODE;
 if(SetTimer(window,copilotMapKeyTimer,80,nullptr)){copilotMapKeyDown=SendInput(1,&input,sizeof(input))==1;if(!copilotMapKeyDown)KillTimer(window,copilotMapKeyTimer);}return true;
}
