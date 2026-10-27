#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

namespace bettergps_navigation {
struct Display { std::string instruction, text, status; bool live{};
    std::string currentRoad,targetRoad,destination,next,exit;
};
// Pure decoder is exercised by native tests and by the production mapping reader.
Display Decode(const uint8_t* bytes, size_t size, int64_t now);
void Poll(bool profile_ready);
Display Current();
std::wstring ShieldDirectory();
void Start();
void RequestStop();
void Stop();
}
