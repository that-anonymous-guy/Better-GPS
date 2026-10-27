#include "logger.hpp"

#include <filesystem>
#include <iterator>

namespace bettergps
{
    namespace
    {
        std::wstring ParentDirectory(const std::wstring& path)
        {
            const auto pos = path.find_last_of(L"\\/");
            return pos == std::wstring::npos ? L"." : path.substr(0, pos);
        }
    }

    bool Logger::Initialize(const HMODULE module, const scs_log_t game_log)
    {
        game_log_ = game_log;
        owner_thread_ = GetCurrentThreadId();

        wchar_t dll_path[32768]{};
        const DWORD len = GetModuleFileNameW(module, dll_path, static_cast<DWORD>(std::size(dll_path)));
        if (len == 0 || len >= std::size(dll_path)) return false;

        output_dir_ = ParentDirectory(dll_path) + L"\\logs\\probe";
        std::error_code ec;
        std::filesystem::create_directories(output_dir_, ec);
        if (ec) return false;

        const std::wstring log_path = output_dir_ + L"\\BetterGPSProbe.log";
        if(std::filesystem::exists(log_path,ec)&&std::filesystem::file_size(log_path,ec)>=6*1024*1024){std::filesystem::remove(log_path+L".previous",ec);std::filesystem::rename(log_path,log_path+L".previous",ec);}
        file_ = _wfopen(log_path.c_str(), L"a");
        return file_ != nullptr;
    }

    void Logger::Shutdown()
    {
        std::lock_guard lock(mutex_);
        if (file_)
        {
            std::fflush(file_);
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    void Logger::Info(const char* fmt, ...)
    {
        va_list args; va_start(args, fmt); Write(SCS_LOG_TYPE_message, "INFO", fmt, args); va_end(args);
    }
    void Logger::Warn(const char* fmt, ...)
    {
        va_list args; va_start(args, fmt); Write(SCS_LOG_TYPE_warning, "WARN", fmt, args); va_end(args);
    }
    void Logger::Error(const char* fmt, ...)
    {
        va_list args; va_start(args, fmt); Write(SCS_LOG_TYPE_error, "ERROR", fmt, args); va_end(args);
    }
    void Logger::Debug(const char* fmt, ...)
    {
        if (!verbose_) return;
        va_list args; va_start(args, fmt); Write(SCS_LOG_TYPE_message, "DEBUG", fmt, args); va_end(args);
    }

    void Logger::Write(const scs_log_type_t type, const char* level, const char* fmt, va_list args)
    {
        char message[4096]{};
        _vsnprintf_s(message, sizeof(message), _TRUNCATE, fmt, args);

        char game_message[4352]{};
        _snprintf_s(game_message, sizeof(game_message), _TRUNCATE, "[BetterGPSProbe] [%s] %s", level, message);
        // SDK logging stays on its owning thread. Worker diagnostics remain in our file.
        if (game_log_ && GetCurrentThreadId() == owner_thread_) game_log_(type, game_message);

        std::lock_guard lock(mutex_);
        if (!file_) return;
        if(_ftelli64(file_)>=6*1024*1024){
            std::fclose(file_);file_=nullptr;const auto path=output_dir_+L"\\BetterGPSProbe.log";
            std::error_code ec;std::filesystem::remove(path+L".previous",ec);ec.clear();std::filesystem::rename(path,path+L".previous",ec);
            file_=_wfopen(path.c_str(),L"w");if(!file_)return;
        }

        SYSTEMTIME now{};
        GetLocalTime(&now);
        std::fprintf(file_, "%04u-%02u-%02u %02u:%02u:%02u.%03u [%s] %s\n",
            now.wYear, now.wMonth, now.wDay,
            now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
            level, message);
        const auto tick=GetTickCount64();
        if(type==SCS_LOG_TYPE_error||tick-last_flush_>=1000){std::fflush(file_);last_flush_=tick;}
    }
}
