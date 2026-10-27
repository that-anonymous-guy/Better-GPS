#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>

namespace bettergps::memory
{
    struct ModuleInfo
    {
        std::uintptr_t base = 0;
        std::size_t size = 0;
    };

    bool GetMainModuleInfo(ModuleInfo& out) noexcept;
    std::uintptr_t ScanExecutablePattern(const ModuleInfo& module, std::string_view pattern,bool rip_relative);
    std::uintptr_t ResolveRipRelative(std::uintptr_t instruction, std::size_t displacement_offset) noexcept;

    bool IsReadable(const void* address, std::size_t bytes = 1) noexcept;

    bool SafeCopy(const void* source, void* destination, std::size_t bytes) noexcept;

    template <typename T>
    bool SafeRead(const void* source, T& out) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>);
        return SafeCopy(source, &out, sizeof(T));
    }

    template <typename T>
    bool SafeReadAt(const void* base, const std::size_t offset, T& out) noexcept
    {
        return SafeRead(reinterpret_cast<const std::uint8_t*>(base) + offset, out);
    }

    bool SafeReadCString(const char* source, char* destination, std::size_t destination_size) noexcept;
}
