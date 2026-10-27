#include "memory.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace bettergps::memory
{
    namespace
    {
        std::vector<int> ParsePattern(const std::string_view text)
        {
            std::vector<int> bytes;
            std::size_t i = 0;

            auto hex = [](const char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + c - 'a';
                if (c >= 'A' && c <= 'F') return 10 + c - 'A';
                return -1;
            };

            while (i < text.size())
            {
                while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
                if (i >= text.size()) break;

                if (text[i] == '?')
                {
                    bytes.push_back(-1);
                    ++i;
                    if (i < text.size() && text[i] == '?') ++i;
                }
                else
                {
                    if (i + 1 >= text.size()) return {};
                    const int hi = hex(text[i]);
                    const int lo = hex(text[i + 1]);
                    if (hi < 0 || lo < 0) return {};
                    bytes.push_back((hi << 4) | lo);
                    i += 2;
                }

                while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i]))) ++i;
            }
            return bytes;
        }

        std::vector<std::uintptr_t> ScanRange(const std::uintptr_t start, const std::size_t length, const std::vector<int>& pattern)
        {
            if (pattern.empty() || length < pattern.size()) return {};
            const auto* data = reinterpret_cast<const std::uint8_t*>(start);
            const std::size_t last = length - pattern.size();
            std::vector<std::uintptr_t> matches;

            for (std::size_t i = 0; i <= last; ++i)
            {
                bool match = true;
                for (std::size_t j = 0; j < pattern.size(); ++j)
                {
                    if (pattern[j] >= 0 && data[i + j] != static_cast<std::uint8_t>(pattern[j]))
                    {
                        match = false;
                        break;
                    }
                }
                if (match) matches.push_back(start+i);
            }
            return matches;
        }
    }

    bool GetMainModuleInfo(ModuleInfo& out) noexcept
    {
        out = {};
        const HMODULE module = GetModuleHandleW(nullptr);
        if (!module) return false;

        const auto base = reinterpret_cast<std::uintptr_t>(module);
        IMAGE_DOS_HEADER dos{};
        if (!SafeRead(reinterpret_cast<void*>(base), dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) return false;

        IMAGE_NT_HEADERS64 nt{};
        if (!SafeRead(reinterpret_cast<void*>(base + static_cast<std::uintptr_t>(dos.e_lfanew)), nt)) return false;
        if (nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;

        out.base = base;
        out.size = nt.OptionalHeader.SizeOfImage;
        return out.size != 0;
    }

    std::uintptr_t ScanExecutablePattern(const ModuleInfo& module, const std::string_view pattern_text,const bool rip_relative)
    {
        const auto pattern = ParsePattern(pattern_text);
        if (pattern.empty() || module.base == 0 || module.size == 0) return 0;

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module.base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(module.base + static_cast<std::uintptr_t>(dos->e_lfanew));
        const auto* section = IMAGE_FIRST_SECTION(nt);
        std::uintptr_t first=0,target=0;

        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            if ((section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;

            const std::size_t virtual_size = static_cast<std::size_t>(section[i].Misc.VirtualSize);
            const std::size_t raw_size = static_cast<std::size_t>(section[i].SizeOfRawData);
            std::size_t size = (std::max)(virtual_size, raw_size);
            const std::uintptr_t start = module.base + section[i].VirtualAddress;
            if (start < module.base || start >= module.base + module.size) continue;
            size = (std::min)(size, module.base + module.size - start);

            if (!IsReadable(reinterpret_cast<void*>(start), size)) continue;
            for(const auto match:ScanRange(start,size,pattern)) {
                std::uintptr_t current=0;
                if(rip_relative)current=ResolveRipRelative(match,3);
                else {std::uint32_t offset=0;if(!SafeRead(reinterpret_cast<void*>(match+3),offset))return 0;current=offset;}
                if(!current || (first&&current!=target))return 0;
                if(!first){first=match;target=current;}
            }
        }
        return first; // Multiple instructions are valid only when they resolve to one target.
    }

    std::uintptr_t ResolveRipRelative(const std::uintptr_t instruction, const std::size_t displacement_offset) noexcept
    {
        std::int32_t displacement = 0;
        if (!SafeRead(reinterpret_cast<const void*>(instruction + displacement_offset), displacement)) return 0;
        return instruction + displacement_offset + sizeof(displacement) + static_cast<std::intptr_t>(displacement);
    }

    bool IsReadable(const void* address, const std::size_t bytes) noexcept
    {
        if (!address || bytes == 0) return false;
        const auto begin = reinterpret_cast<std::uintptr_t>(address);
        const auto end = begin + bytes - 1;
        if (end < begin) return false;

        std::uintptr_t cursor = begin;
        while (cursor <= end)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<void*>(cursor), &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
            if (mbi.State != MEM_COMMIT) return false;
            if ((mbi.Protect & PAGE_GUARD) != 0 || (mbi.Protect & PAGE_NOACCESS) != 0) return false;
            const DWORD p = mbi.Protect & 0xFF;
            const bool readable = p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
                                  p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
            if (!readable) return false;

            const auto region_end = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (region_end == 0 || region_end <= cursor) return false;
            if (region_end > end) break;
            cursor = region_end;
        }
        return true;
    }

    bool SafeCopy(const void* source, void* destination, const std::size_t bytes) noexcept
    {
        if (!source || !destination || bytes == 0) return false;
        if (!IsReadable(source, bytes)) return false;
#if defined(_MSC_VER)
        __try
        {
            std::memcpy(destination, source, bytes);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
#else
        std::memcpy(destination, source, bytes);
        return true;
#endif
    }

    bool SafeReadCString(const char* source, char* destination, const std::size_t destination_size) noexcept
    {
        if (!source || !destination || destination_size == 0) return false;
        destination[0] = '\0';
        for (std::size_t i = 0; i + 1 < destination_size; ++i)
        {
            char c = '\0';
            if (!SafeRead(source + i, c)) return false;
            destination[i] = c;
            if (c == '\0') return true;
        }
        destination[destination_size - 1] = '\0';
        return true;
    }
}
