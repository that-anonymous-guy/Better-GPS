#include "../CarplayResources.h"
#include <thread>
extern "C" __declspec(dllexport) bool decode_carplay(const wchar_t* root){
    std::thread worker([&]{bettergps_overlay::horizontal_carplay_prepare(root,GetTickCount64());});worker.join();
    return !bettergps_overlay::horizontalCarplayPixels[0].empty()&&!bettergps_overlay::horizontalCarplayPixels[1].empty();
}
