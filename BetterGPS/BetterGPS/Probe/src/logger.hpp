#pragma once

#include "scs_sdk_min.hpp"

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace bettergps
{
    class Logger
    {
    public:
        bool Initialize(HMODULE module, scs_log_t game_log);
        void Shutdown();

        void Info(const char* fmt, ...);
        void Warn(const char* fmt, ...);
        void Error(const char* fmt, ...);
        void Debug(const char* fmt, ...);

        const std::wstring& output_dir() const { return output_dir_; }
        bool verbose() const { return verbose_; }
        void set_verbose(bool v) { verbose_ = v; }

    private:
        void Write(scs_log_type_t type, const char* level, const char* fmt, va_list args);

        scs_log_t game_log_ = nullptr;
        FILE* file_ = nullptr;
        std::wstring output_dir_;
        std::mutex mutex_;
        bool verbose_ = true;
        DWORD owner_thread_ = 0;
        ULONGLONG last_flush_ = 0;
    };
}
