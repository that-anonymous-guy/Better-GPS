#pragma once

namespace bettergps::patterns
{
    // ETS2LA/plugin 1.61 Windows signatures.
    // base_ctrl: RIP-relative address of the global base_ctrl pointer.
    inline constexpr const char* kBaseCtrl = "48 8b ? ? ? ? ? 48 8b ? 48 8b ? 48 8b 80";
    inline constexpr unsigned kBaseCtrlRipDispOffset = 3;

    // game_ctrl::gps_manager: immediate displacement used by LEA RCX,[RAX+disp32].
    inline constexpr const char* kGpsManagerOffset = "48 8d 88 ? ? ? ? 48 85 c9 74 ? 48 8b 01 48 8d 54";
    inline constexpr unsigned kGpsManagerDispOffset = 3;
}
