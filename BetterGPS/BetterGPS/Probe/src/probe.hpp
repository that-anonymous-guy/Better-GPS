#pragma once

#include "game_layout.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "scs_sdk_min.hpp"
#include "live_capture.hpp"

#include <Windows.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace bettergps
{
    struct PhysicalSnapshotItem
    {
        layout::PhysicalRouteItemWin raw{};
        std::uint64_t node_uid = 0;
        CaptureNode evidence{};
    };

    struct RouteSnapshotItem
    {
        layout::RouteItemWin raw{};
        std::uint64_t item_uid = 0;
    };

    struct RouteSnapshot
    {
        void* route_task = nullptr;
        std::vector<PhysicalSnapshotItem> physical;
        std::vector<RouteSnapshotItem> logical;
        std::uint64_t fingerprint = 0;
    };

    struct GpsState
    {
        float trip_distance = 0.0f;
        float trip_time = 0.0f;
        float next_checkpoint_distance = 0.0f;
        float next_checkpoint_time = 0.0f;

        void* voice_value = nullptr;
        std::uint64_t voice_size = 0;
        std::uint64_t voice_capacity = 0;
        std::array<std::string, 5> voice{};
    };

    class Probe
    {
    public:
        explicit Probe(HMODULE module);
        ~Probe();

        bool Initialize(scs_u32_t sdk_version, const scs_telemetry_init_params_v100_t* params);
        void Shutdown();
        void Tick();

        static void SCSAPIFUNC FrameEndCallback(scs_event_t event, const void* event_info, scs_context_t context);
        static void SCSAPIFUNC ChannelCallback(scs_string_t name,scs_u32_t index,const scs_value_t* value,scs_context_t context);
        static void SCSAPIFUNC StateCallback(scs_event_t event,const void* info,scs_context_t context);

    private:
        friend struct ProbeTestAccess;
        struct Config
        {
            bool allow_unsupported_game_version = false;
            bool snapshot_on_route_change = true;
            bool verbose = true;
            unsigned poll_interval_ms = 250;
            unsigned state_log_interval_ms = 1000;
            std::uint64_t max_route_items = 20000;
        };

        bool LoadConfig();
        bool ValidateGame(const scs_telemetry_init_params_v100_t* params);
        bool ScanPatterns();
        void* GetGameCtrl() const;
        bool ReadUnitClassName(void* unit, char* out, std::size_t out_size) const;
        void* GetGpsManager() const;

        bool ReadGpsState(void* gps, GpsState& out) const;
        bool ReadRoute(void* gps, RouteSnapshot& out);
        bool ReadArrayHeader(const void* address, layout::ArrayDynWin& out) const;
        bool ValidateArrayHeader(const layout::ArrayDynWin& array, std::size_t element_size, const char* name);

        std::uint64_t ReadItemUid(const layout::RouteItemWin& item) const;
        std::uint64_t ComputeFingerprint(const RouteSnapshot& route) const;
        std::size_t EstimateCurrentPhysicalIndex(const RouteSnapshot& route, float trip_distance, float& distance_to_item) const;

        void ProcessVoiceChange(const GpsState& state);
        void ProcessRouteChange(const RouteSnapshot* route, const GpsState& state);
        void LogState(const RouteSnapshot* route, const GpsState& state);
        bool WriteRouteSnapshots(const RouteSnapshot& route, const GpsState& state);
        void AppendLiveCsv(const RouteSnapshot* route, const GpsState& state);

        std::wstring ModuleDirectory() const;
        std::wstring ConfigPath() const;
        std::wstring SnapshotBaseName() const;
        static std::string VoiceSignature(const GpsState& state);
        static std::string CsvEscape(const std::string& text);

        HMODULE module_ = nullptr;
        Logger log_;
        Config config_{};
        scs_telemetry_init_params_v100_t init_params_{};

        memory::ModuleInfo game_module_{};
        std::uintptr_t base_ctrl_global_ = 0;
        std::uint32_t gps_manager_offset_ = 0;

        std::atomic<bool> initialized_{false},worker_stop_{true},configuration_reset_{false};
        std::thread sampling_worker_;
        std::atomic<void*> gps_address_{nullptr};
        unsigned long long address_poll_=0;
        std::mutex vehicle_mutex_,wake_mutex_;
        std::condition_variable wake_;
        bool had_route_ = false;
        std::uint64_t last_route_fingerprint_ = 0;
        void* last_route_task_ = nullptr;
        std::string last_voice_signature_;
        std::uint64_t last_poll_tick_ = 0;
        std::uint64_t last_state_log_tick_ = 0;
        unsigned snapshot_sequence_ = 0;

        FILE* live_csv_ = nullptr;
        LiveCapture capture_;
        CaptureVehicle vehicle_{};
        unsigned long long live_tick_=0,generation_=0,pending_fingerprint_=0,accepted_fingerprint_=0;
        std::string capture_status_;
        std::string route_error_;
        void SetCaptureStatus(const std::string& status,unsigned long long now);
        void StartSamplingWorker();
        bool RouteUnchanged(void* gps,unsigned long long now);
        void RememberRoute(const RouteSnapshot& route,unsigned long long now);
        void* cached_task_=nullptr;unsigned long long cached_at_=0;
        std::vector<layout::PhysicalRouteItemWin> cached_physical_,poll_physical_;
        std::vector<layout::RouteItemWin> cached_logical_,poll_logical_;
    };
}
