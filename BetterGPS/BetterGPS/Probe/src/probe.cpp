#include "probe.hpp"
#include "../../shared/PerformanceDiagnostics.h"
#include "../../Shared/StartupTrace.h"
#include "patterns.hpp"
#include "route_identity.hpp"
#include "route_publication.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <iterator>
#include <sstream>

namespace bettergps
{
    namespace
    {
        constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
        constexpr std::uint64_t kFnvPrime = 1099511628211ull;
        constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);

        void HashBytes(std::uint64_t& hash, const void* data, const std::size_t size)
        {
            const auto* p = static_cast<const std::uint8_t*>(data);
            for (std::size_t i = 0; i < size; ++i)
            {
                hash ^= p[i];
                hash *= kFnvPrime;
            }
        }

        template <typename T>
        void HashValue(std::uint64_t& hash, const T& value)
        {
            HashBytes(hash, &value, sizeof(value));
        }

        bool IsFiniteRouteFloat(const float value)
        {
            return std::isfinite(value) && value >= 0.0f && value < 100000000.0f;
        }

        std::string SanitizeVoice(const char* bytes, const std::size_t count)
        {
            std::string out;
            out.reserve(count);
            for (std::size_t i = 0; i < count && bytes[i] != '\0'; ++i)
            {
                const unsigned char c = static_cast<unsigned char>(bytes[i]);
                if (c >= 32 && c <= 126) out.push_back(static_cast<char>(c));
                else out.push_back('?');
            }
            return out;
        }

        // unit_t::get_unit_descriptor is virtual slot 5 in the Windows layout used by
        // ETS2LA/plugin 1.61: dtor, destroy, clone, vfunc_3, vfunc_4, get_unit_descriptor.
        void* TryCallGetUnitDescriptor(void* object) noexcept
        {
            if (!object) return nullptr;
#if defined(_MSC_VER)
            __try
            {
                auto** vtable = *reinterpret_cast<void***>(object);
                if (!vtable) return nullptr;
                using Fn = void*(__fastcall*)(void*);
                const auto fn = reinterpret_cast<Fn>(vtable[5]);
                if (!fn) return nullptr;
                return fn(object);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return nullptr;
            }
#else
            return nullptr;
#endif
        }

#pragma pack(push, 1)
        struct UnitDescriptorWin
        {
            const char** p_class_name;
            void* constructor;
            void* destructor;
            UnitDescriptorWin* parent_class;
            std::uint8_t pad[24];
        };
#pragma pack(pop)
        static_assert(sizeof(UnitDescriptorWin) == 0x38);
    }

    Probe::Probe(const HMODULE module) : module_(module) {}

    Probe::~Probe()
    {
        Shutdown();
    }

    bool Probe::Initialize(const scs_u32_t sdk_version, const scs_telemetry_init_params_v100_t* params)
    {
        if (!params || !params->common.log || !params->register_for_event) return false;
        StartupTrace trace(params->common.log,"probe");

        init_params_ = *params;
        if (!trace.measure("open-probe-log",[&]{return log_.Initialize(module_, params->common.log);})) return false;

        trace.measure("diagnostics-shared-memory",[]{bettergps_diag::Init(2);});
        trace.measure("read-config",[&]{LoadConfig();});
        log_.set_verbose(config_.verbose);
        log_.Info("BetterGPSProbe live 0.3.0 starting (protocol v2, BetterGPSProbe-v1 pipe)");
        log_.Info("Telemetry SDK requested: %u.%u", (sdk_version >> 16u) & 0xFFFFu, sdk_version & 0xFFFFu);

        if (sdk_version != SCS_TELEMETRY_VERSION_1_00 && sdk_version != SCS_TELEMETRY_VERSION_1_01)
        {
            log_.Error("Unsupported telemetry SDK version 0x%08X", sdk_version);
            return false;
        }

        if (!ValidateGame(params)) return false;
        if (!memory::GetMainModuleInfo(game_module_))
        {
            log_.Error("Could not read ATS executable module information");
            return false;
        }
        log_.Debug("ATS image base=0x%llX size=0x%llX",
            static_cast<unsigned long long>(game_module_.base),
            static_cast<unsigned long long>(game_module_.size));

        if (!trace.measure("scan-game-patterns",[&]{return ScanPatterns();})) return false;

        initialized_ = true;
        {
        StartupTrace::Phase registration(trace,"register-sdk-callbacks");
        if(!params->register_for_channel) { Shutdown(); return false; }
        const char* channels[]={SCS_TELEMETRY_TRUCK_CHANNEL_world_placement,SCS_TELEMETRY_TRUCK_CHANNEL_speed,
            SCS_TELEMETRY_TRUCK_CHANNEL_navigation_distance};
        const scs_value_type_t types[]={SCS_VALUE_TYPE_dplacement,SCS_VALUE_TYPE_float,SCS_VALUE_TYPE_float};
        for(int i=0;i<3;i++)
            if(params->register_for_channel(channels[i],SCS_U32_NIL,types[i],
                SCS_TELEMETRY_CHANNEL_FLAG_each_frame|SCS_TELEMETRY_CHANNEL_FLAG_no_value,&Probe::ChannelCallback,this)!=SCS_RESULT_ok)
            { log_.Error("Cannot register telemetry channel %s",channels[i]); Shutdown(); return false; }
        for(auto event:{SCS_TELEMETRY_EVENT_paused,SCS_TELEMETRY_EVENT_started,SCS_TELEMETRY_EVENT_configuration})
            if(params->register_for_event(event,&Probe::StateCallback,this)!=SCS_RESULT_ok) { Shutdown(); return false; }
        if(params->register_for_event(SCS_TELEMETRY_EVENT_frame_end,&Probe::FrameEndCallback,this)!=SCS_RESULT_ok)
        { Shutdown(); return false; }
        }
        capture_.SetSource(params->common.game_name,gps_manager_offset_);
        trace.measure("start-capture",[&]{capture_.Start();});
        try{trace.measure("start-sampling-worker",[&]{StartSamplingWorker();});}catch(...){Shutdown();return false;}
        SetCaptureStatus("WAITING_ROUTE",GetTickCount64());
        log_.Info("Probe initialized. Output: %ls", log_.output_dir().c_str());
        return true;
    }

    void Probe::StartSamplingWorker(){worker_stop_=false;sampling_worker_=std::thread([this]{SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);while(!worker_stop_.load()){bettergps_diag::Refresh();try{Tick();}catch(...){accepted_fingerprint_=pending_fingerprint_=0;try{SetCaptureStatus("INVALID_ROUTE",GetTickCount64());}catch(...){}}std::unique_lock<std::mutex> lock(wake_mutex_);wake_.wait_for(lock,std::chrono::milliseconds(50),[this]{return worker_stop_.load();});}});}

    void Probe::Shutdown()
    {
        worker_stop_=true;wake_.notify_all();if(sampling_worker_.joinable())sampling_worker_.join();
        if (!initialized_ && !live_csv_)
        {
            log_.Shutdown();
            return;
        }

        if (initialized_ && init_params_.unregister_from_event)
        {
            init_params_.unregister_from_event(SCS_TELEMETRY_EVENT_frame_end);
            init_params_.unregister_from_event(SCS_TELEMETRY_EVENT_paused);
            init_params_.unregister_from_event(SCS_TELEMETRY_EVENT_started);
        }
        if(initialized_ && init_params_.unregister_from_channel)
        {
            init_params_.unregister_from_channel(SCS_TELEMETRY_TRUCK_CHANNEL_world_placement,SCS_U32_NIL,SCS_VALUE_TYPE_dplacement);
            init_params_.unregister_from_channel(SCS_TELEMETRY_TRUCK_CHANNEL_speed,SCS_U32_NIL,SCS_VALUE_TYPE_float);
            init_params_.unregister_from_channel(SCS_TELEMETRY_TRUCK_CHANNEL_navigation_distance,SCS_U32_NIL,SCS_VALUE_TYPE_float);
        }
        initialized_ = false;
        capture_.Stop();

        if (live_csv_)
        {
            std::fflush(live_csv_);
            std::fclose(live_csv_);
            live_csv_ = nullptr;
        }
        bettergps_diag::Stop();
        log_.Info("BetterGPSProbe stopped");
        log_.Shutdown();
    }

    bool Probe::LoadConfig()
    {
        const std::wstring path = ConfigPath();
        // The live release always enforces the supported memory layout.
        config_.allow_unsupported_game_version = false;
        config_.snapshot_on_route_change = GetPrivateProfileIntW(L"probe", L"snapshot_on_route_change", 1, path.c_str()) != 0;
        config_.verbose = GetPrivateProfileIntW(L"probe", L"verbose", 1, path.c_str()) != 0;
        config_.poll_interval_ms = (std::max)(25u, GetPrivateProfileIntW(L"probe", L"poll_interval_ms", 250, path.c_str()));
        config_.state_log_interval_ms = (std::max)(100u, GetPrivateProfileIntW(L"probe", L"state_log_interval_ms", 1000, path.c_str()));
        const int max_items = GetPrivateProfileIntW(L"probe", L"max_route_items", 20000, path.c_str());
        config_.max_route_items = static_cast<std::uint64_t>((std::clamp)(max_items, 100, 20000));
        return true;
    }

    bool Probe::ValidateGame(const scs_telemetry_init_params_v100_t* params)
    {
        const char* game_id = params->common.game_id ? params->common.game_id : "";
        const char* game_name = params->common.game_name ? params->common.game_name : "";
        const unsigned telemetry_major = (params->common.game_version >> 16u) & 0xFFFFu;
        const unsigned telemetry_minor = params->common.game_version & 0xFFFFu;

        // Important: common.game_version is the ATS *telemetry API* version (currently 1.05),
        // not the executable patch (1.61.x). ETS2LA itself keys its memory layouts from
        // the version string embedded in common.game_name.
        log_.Info("Game: id='%s' name='%s' telemetry game API=%u.%u",
            game_id, game_name, telemetry_major, telemetry_minor);
        if (std::strcmp(game_id, "ats") != 0)
        {
            log_.Error("This build is intentionally restricted to American Truck Simulator (game_id=ats)");
            return false;
        }

        const bool is_161 = std::strstr(game_name, "1.61.") != nullptr;
        if (!is_161)
        {
            if (!config_.allow_unsupported_game_version)
            {
                log_.Error("Unsupported ATS executable version in game_name: '%s'. This probe requires ATS 1.61.x; the version guard cannot be disabled.", game_name);
                return false;
            }
            log_.Warn("UNSAFE OVERRIDE: attempting 1.61 memory signatures on '%s'", game_name);
        }
        return true;
    }

    bool Probe::ScanPatterns()
    {
        const std::uintptr_t base_match = memory::ScanExecutablePattern(game_module_, patterns::kBaseCtrl,true);
        if (!base_match)
        {
            log_.Error("base_ctrl signature not found");
            return false;
        }
        base_ctrl_global_ = memory::ResolveRipRelative(base_match, patterns::kBaseCtrlRipDispOffset);
        if (!base_ctrl_global_)
        {
            log_.Error("Failed to resolve base_ctrl RIP-relative global");
            return false;
        }

        const std::uintptr_t gps_match = memory::ScanExecutablePattern(game_module_, patterns::kGpsManagerOffset,false);
        if (!gps_match)
        {
            log_.Error("gps_manager offset signature not found");
            return false;
        }
        if (!memory::SafeRead(reinterpret_cast<void*>(gps_match + patterns::kGpsManagerDispOffset), gps_manager_offset_))
        {
            log_.Error("Could not read gps_manager displacement");
            return false;
        }
        if (gps_manager_offset_ < 0x100 || gps_manager_offset_ > 0x20000)
        {
            log_.Error("gps_manager displacement 0x%X failed sanity check", gps_manager_offset_);
            return false;
        }

        log_.Info("base_ctrl signature @ ATS+0x%llX -> global ATS+0x%llX",
            static_cast<unsigned long long>(base_match - game_module_.base),
            static_cast<unsigned long long>(base_ctrl_global_ - game_module_.base));
        log_.Info("gps_manager signature @ ATS+0x%llX -> game_ctrl+0x%X",
            static_cast<unsigned long long>(gps_match - game_module_.base), gps_manager_offset_);
        return true;
    }

    bool Probe::ReadUnitClassName(void* unit, char* out, const std::size_t out_size) const
    {
        if (!unit || !out || out_size == 0) return false;
        out[0] = '\0';

        void* desc_ptr = TryCallGetUnitDescriptor(unit);
        if (!desc_ptr) return false;

        UnitDescriptorWin desc{};
        if (!memory::SafeRead(desc_ptr, desc) || !desc.p_class_name) return false;
        const char* class_name = nullptr;
        if (!memory::SafeRead(desc.p_class_name, class_name) || !class_name) return false;
        return memory::SafeReadCString(class_name, out, out_size);
    }

    void* Probe::GetGameCtrl() const
    {
        if (!base_ctrl_global_) return nullptr;
        void* base_ctrl = nullptr;
        if (!memory::SafeRead(reinterpret_cast<void*>(base_ctrl_global_), base_ctrl) || !base_ctrl) return nullptr;

        char class_name[128]{};
        if (!ReadUnitClassName(base_ctrl, class_name, sizeof(class_name))) return nullptr;
        if (std::strcmp(class_name, "game_ctrl") != 0) return nullptr;
        return base_ctrl;
    }

    void* Probe::GetGpsManager() const
    {
        void* game_ctrl = GetGameCtrl();
        if (!game_ctrl || gps_manager_offset_ == 0) return nullptr;
        auto* gps = reinterpret_cast<std::uint8_t*>(game_ctrl) + gps_manager_offset_;
        return memory::IsReadable(gps, layout::GPS_MANAGER_SIZE) ? gps : nullptr;
    }

    bool Probe::ReadGpsState(void* gps, GpsState& out) const
    {
        if (!gps) return false;
        if (!memory::SafeReadAt(gps, layout::GPS_TRIP_DISTANCE, out.trip_distance) ||
            !memory::SafeReadAt(gps, layout::GPS_TRIP_TIME, out.trip_time) ||
            !memory::SafeReadAt(gps, layout::GPS_NEXT_CHECKPOINT_DISTANCE, out.next_checkpoint_distance) ||
            !memory::SafeReadAt(gps, layout::GPS_NEXT_CHECKPOINT_TIME, out.next_checkpoint_time))
        {
            return false;
        }

        if (!IsFiniteRouteFloat(out.trip_distance) || !IsFiniteRouteFloat(out.trip_time) ||
            !IsFiniteRouteFloat(out.next_checkpoint_distance) || !IsFiniteRouteFloat(out.next_checkpoint_time))
        {
            return false;
        }

        layout::ArrayLocalHeaderWin voice_header{};
        if (memory::SafeReadAt(gps, layout::GPS_VOICE_EVENTS, voice_header))
        {
            out.voice_value = voice_header.value;
            out.voice_size = voice_header.size;
            out.voice_capacity = voice_header.capacity;
        }

        // array_local<char[48],5>: inline storage begins immediately after the 0x20-byte header.
        // Prefer the live value pointer, but fall back to the inline address if needed.
        const auto* inline_voice = reinterpret_cast<const std::uint8_t*>(gps) + layout::GPS_VOICE_EVENTS + sizeof(layout::ArrayLocalHeaderWin);
        const void* voice_base = out.voice_value;
        if (!voice_base || !memory::IsReadable(voice_base, 48)) voice_base = inline_voice;

        for (std::size_t i = 0; i < out.voice.size(); ++i)
        {
            char slot[49]{};
            if (memory::SafeCopy(reinterpret_cast<const std::uint8_t*>(voice_base) + i * 48, slot, 48))
            {
                slot[48] = '\0';
                out.voice[i] = SanitizeVoice(slot, 48);
            }
        }
        return true;
    }

    bool Probe::ReadArrayHeader(const void* address, layout::ArrayDynWin& out) const
    {
        return memory::SafeRead(address, out);
    }

    bool Probe::ValidateArrayHeader(const layout::ArrayDynWin& array, const std::size_t element_size, const char*)
    {
        if (array.size > config_.max_route_items)
        {
            return false;
        }
        if (array.capacity < array.size || array.capacity > config_.max_route_items * 8ull)
        {
            return false;
        }
        if (array.size == 0) return true;
        if (!array.value) return false;
        if (array.size > std::numeric_limits<std::size_t>::max() / element_size) return false;
        return memory::IsReadable(array.value, static_cast<std::size_t>(array.size) * element_size);
    }

    bool Probe::ReadRoute(void* gps, RouteSnapshot& out)
    {bettergps_diag::Scope timing(bettergps_diag::ReadRoute);
        route_error_="MEMORY_OR_HEADER_UNSTABLE";
        out = {};
        void* route_task = nullptr;
        if (!memory::SafeReadAt(gps, layout::GPS_ROUTE_TASK, route_task)) return false;
        if (!route_task) return true; // Valid state: no route set.
        if (!memory::IsReadable(route_task, layout::ROUTE_TASK_SIZE)) return false;
        out.route_task = route_task;

        layout::ArrayDynWin physical_header{};
        layout::ArrayDynWin logical_header{};
        if (!memory::SafeReadAt(route_task, layout::ROUTE_TASK_PHYSICAL_ITEMS, physical_header) ||
            !memory::SafeReadAt(route_task, layout::ROUTE_TASK_ROUTE_ITEMS, logical_header))
        {
            return false;
        }
        if (!ValidateArrayHeader(physical_header, sizeof(layout::PhysicalRouteItemWin), "physical_route_items") ||
            !ValidateArrayHeader(logical_header, sizeof(layout::RouteItemWin), "route_items"))
        {
            return false;
        }

        std::vector<layout::PhysicalRouteItemWin> physical_raw(static_cast<std::size_t>(physical_header.size));
        if (!physical_raw.empty() && !memory::SafeCopy(physical_header.value, physical_raw.data(), physical_raw.size() * sizeof(physical_raw[0])))
            return false;

        std::vector<layout::RouteItemWin> logical_raw(static_cast<std::size_t>(logical_header.size));
        if (!logical_raw.empty() && !memory::SafeCopy(logical_header.value, logical_raw.data(), logical_raw.size() * sizeof(logical_raw[0])))
            return false;

        out.physical.resize(physical_raw.size());
        for (std::size_t i = 0; i < physical_raw.size(); ++i)
        {
            out.physical[i].raw = physical_raw[i];
            if (physical_raw[i].node)
            {
                layout::NodeItemWin n{}, verify{};
                if(!memory::SafeRead(physical_raw[i].node,n)) {route_error_="NODE_READ_"+std::to_string(i);return false;}
                out.physical[i].node_uid=n.uid;
                auto& e=out.physical[i].evidence;
                e.x=n.x;e.y=n.y;e.z=n.z;e.flags=n.flags;
                e.qw=n.qw;e.qx=n.qx;e.qy=n.qy;e.qz=n.qz;
                e.backward_valid=!n.backward || (memory::SafeReadAt(n.backward,layout::KDOP_ITEM_UID,e.backward)&&e.backward!=0);
                e.forward_valid=!n.forward || (memory::SafeReadAt(n.forward,layout::KDOP_ITEM_UID,e.forward)&&e.forward!=0);
                if(!memory::SafeRead(physical_raw[i].node,verify) || std::memcmp(&n,&verify,sizeof(n))!=0) {route_error_="NODE_CHANGED_"+std::to_string(i);return false;}
                const float norm=n.qw*n.qw+n.qx*n.qx+n.qy*n.qy+n.qz*n.qz;
                if(!std::isfinite(norm) || norm<0.9f || norm>1.1f) {route_error_="NODE_ROTATION_"+std::to_string(i);return false;}
                e.status=e.backward_valid&&e.forward_valid?"READ_OK":"CONNECTION_UNAVAILABLE";
            }
        }

        out.logical.resize(logical_raw.size());
        for (std::size_t i = 0; i < logical_raw.size(); ++i)
        {
            out.logical[i].raw = logical_raw[i];
            out.logical[i].item_uid = ReadItemUid(logical_raw[i]);
        }

        // Compare semantic fields, not unknown padding. The game supplies no atomic snapshot API.
        for(std::size_t i=0;i<physical_raw.size();i++) {
            layout::PhysicalRouteItemWin v{};const auto& p=physical_raw[i];
            if(!memory::SafeRead(static_cast<const layout::PhysicalRouteItemWin*>(physical_header.value)+i,v) ||
                v.node!=p.node || v.distance_from_previous!=p.distance_from_previous || v.time_from_previous!=p.time_from_previous ||
                v.total_distance_till_end!=p.total_distance_till_end || v.total_time_till_end!=p.total_time_till_end) {route_error_="PHYSICAL_CHANGED_"+std::to_string(i);return false;}
        }
        for(std::size_t i=0;i<logical_raw.size();i++) {
            layout::RouteItemWin v{};const auto& p=logical_raw[i];
            if(!memory::SafeRead(static_cast<const layout::RouteItemWin*>(logical_header.value)+i,v) ||
                v.type!=p.type || v.node_id!=p.node_id || v.item_ptr!=p.item_ptr ||
                v.distance_from_previous!=p.distance_from_previous || v.time_from_previous!=p.time_from_previous ||
                v.total_distance_till_end!=p.total_distance_till_end || v.total_time_till_end!=p.total_time_till_end ||
                ReadItemUid(v)!=out.logical[i].item_uid) {route_error_="LOGICAL_CHANGED_"+std::to_string(i);return false;}
        }
        void* verify_task=nullptr;
        layout::ArrayDynWin verify_physical{},verify_logical{};
        if(!memory::SafeReadAt(gps,layout::GPS_ROUTE_TASK,verify_task) || verify_task!=route_task ||
            !memory::SafeReadAt(route_task,layout::ROUTE_TASK_PHYSICAL_ITEMS,verify_physical) ||
            !memory::SafeReadAt(route_task,layout::ROUTE_TASK_ROUTE_ITEMS,verify_logical) ||
            std::memcmp(&physical_header,&verify_physical,sizeof(physical_header))!=0 ||
            std::memcmp(&logical_header,&verify_logical,sizeof(logical_header))!=0) return false;
        for(const auto& item:out.physical)
            if(!item.node_uid || !IsFiniteRouteFloat(item.raw.distance_from_previous) ||
                !IsFiniteRouteFloat(item.raw.total_distance_till_end) || !IsFiniteRouteFloat(item.raw.time_from_previous) ||
                !IsFiniteRouteFloat(item.raw.total_time_till_end)) return false;
        for(const auto& item:out.logical)
            if(!item.item_uid || item.raw.type>1 || !IsFiniteRouteFloat(item.raw.distance_from_previous) ||
                !IsFiniteRouteFloat(item.raw.total_distance_till_end) || !IsFiniteRouteFloat(item.raw.time_from_previous) ||
                !IsFiniteRouteFloat(item.raw.total_time_till_end)) return false;
        std::size_t node_index=0;
        for(const auto& item:out.logical) if(item.raw.type==0) {
            if(node_index>=out.physical.size() || item.item_uid!=out.physical[node_index++].node_uid) return false;
        }
        if(node_index!=out.physical.size()) return false;
        out.fingerprint = ComputeFingerprint(out);
        return true;
    }

    std::uint64_t Probe::ReadItemUid(const layout::RouteItemWin& item) const
    {
        if (!item.item_ptr) return 0;
        std::uint64_t uid = 0;
        if (item.type == 0)
        {
            memory::SafeReadAt(item.item_ptr, layout::NODE_ITEM_UID, uid);
        }
        else if (item.type == 1)
        {
            memory::SafeReadAt(item.item_ptr, layout::KDOP_ITEM_UID, uid);
        }
        return uid;
    }

    std::uint64_t Probe::ComputeFingerprint(const RouteSnapshot& route) const
    {
        return RouteIdentityFingerprint(route);
    }

    std::size_t Probe::EstimateCurrentPhysicalIndex(const RouteSnapshot& route, const float trip_distance, float& distance_to_item) const
    {
        distance_to_item = std::numeric_limits<float>::quiet_NaN();
        if (route.physical.empty() || !std::isfinite(trip_distance)) return kInvalidIndex;

        std::size_t best = kInvalidIndex;
        float best_delta = std::numeric_limits<float>::max();

        // Prefer the nearest upcoming route node: current remaining distance should be
        // slightly greater than that node's total_distance_till_end.
        for (std::size_t i = 0; i < route.physical.size(); ++i)
        {
            const float total = route.physical[i].raw.total_distance_till_end;
            if (!std::isfinite(total)) continue;
            const float delta = trip_distance - total;
            if (delta >= -5.0f && delta < best_delta)
            {
                best = i;
                best_delta = delta;
            }
        }

        if (best == kInvalidIndex)
        {
            float best_abs = std::numeric_limits<float>::max();
            for (std::size_t i = 0; i < route.physical.size(); ++i)
            {
                const float total = route.physical[i].raw.total_distance_till_end;
                if (!std::isfinite(total)) continue;
                const float delta = trip_distance - total;
                if (std::fabs(delta) < best_abs)
                {
                    best = i;
                    best_abs = std::fabs(delta);
                    best_delta = delta;
                }
            }
        }

        if (best != kInvalidIndex) distance_to_item = best_delta;
        return best;
    }

    std::string Probe::VoiceSignature(const GpsState& state)
    {
        std::ostringstream ss;
        ss << state.voice_size << ':' << state.voice_capacity;
        for (const auto& v : state.voice) ss << '|' << v;
        return ss.str();
    }

    void Probe::ProcessVoiceChange(const GpsState& state)
    {
        const std::string signature = VoiceSignature(state);
        if (signature == last_voice_signature_) return;
        last_voice_signature_ = signature;

        log_.Info("GPS VOICE changed: trip=%.1fm checkpoint=%.1fm header(size=%llu cap=%llu value=%p) slots=['%s','%s','%s','%s','%s']",
            state.trip_distance, state.next_checkpoint_distance,
            static_cast<unsigned long long>(state.voice_size),
            static_cast<unsigned long long>(state.voice_capacity), state.voice_value,
            state.voice[0].c_str(), state.voice[1].c_str(), state.voice[2].c_str(), state.voice[3].c_str(), state.voice[4].c_str());
    }

    void Probe::ProcessRouteChange(const RouteSnapshot* route, const GpsState& state)
    {
        if (!route || !route->route_task)
        {
            if (had_route_)
            {
                log_.Info("ROUTE CLEARED at trip_distance=%.1fm", state.trip_distance);
                had_route_ = false;
                last_route_fingerprint_ = 0;
                last_route_task_ = nullptr;
            }
            return;
        }

        if (!had_route_ || route->route_task != last_route_task_ || route->fingerprint != last_route_fingerprint_)
        {
            const bool created = !had_route_;
            had_route_ = true;
            last_route_task_ = route->route_task;
            last_route_fingerprint_ = route->fingerprint;
            ++snapshot_sequence_;

            log_.Info("ROUTE %s: task=%p physical=%zu logical=%zu fingerprint=%016llX trip=%.1fm",
                created ? "CREATED" : "CHANGED/RECALCULATED",
                route->route_task, route->physical.size(), route->logical.size(),
                static_cast<unsigned long long>(route->fingerprint), state.trip_distance);

            if (!route->physical.empty())
            {
                log_.Info("  physical first uid=%016llX total=%.1fm | last uid=%016llX total=%.1fm",
                    static_cast<unsigned long long>(route->physical.front().node_uid), route->physical.front().raw.total_distance_till_end,
                    static_cast<unsigned long long>(route->physical.back().node_uid), route->physical.back().raw.total_distance_till_end);
            }
            if (config_.snapshot_on_route_change) WriteRouteSnapshots(*route, state);
        }
    }

    void Probe::LogState(const RouteSnapshot* route, const GpsState& state)
    {
        if (!route || !route->route_task)
        {
            log_.Info("STATE no-route trip=%.1fm checkpoint=%.1fm", state.trip_distance, state.next_checkpoint_distance);
            return;
        }

        float distance_to_item = std::numeric_limits<float>::quiet_NaN();
        const std::size_t index = EstimateCurrentPhysicalIndex(*route, state.trip_distance, distance_to_item);
        std::uint64_t uid = 0;
        float item_total = 0.0f;
        if (index != kInvalidIndex && index < route->physical.size())
        {
            uid = route->physical[index].node_uid;
            item_total = route->physical[index].raw.total_distance_till_end;
        }

        if (index == kInvalidIndex)
        {
            log_.Info("STATE trip=%.1fm (%.2fmi) checkpoint=%.1fm physical=%zu logical=%zu estimated_index=UNKNOWN",
                state.trip_distance, state.trip_distance / 1609.344f, state.next_checkpoint_distance,
                route->physical.size(), route->logical.size());
        }
        else
        {
            log_.Info("STATE trip=%.1fm (%.2fmi) checkpoint=%.1fm physical=%zu logical=%zu estimated_index=%zu next_uid=%016llX item_total=%.1fm delta=%.1fm",
                state.trip_distance, state.trip_distance / 1609.344f, state.next_checkpoint_distance,
                route->physical.size(), route->logical.size(), index,
                static_cast<unsigned long long>(uid), item_total, distance_to_item);
        }
    }

    std::wstring Probe::SnapshotBaseName() const
    {
        wchar_t name[128]{};
        SYSTEMTIME now{};
        GetLocalTime(&now);
        _snwprintf_s(name, std::size(name), _TRUNCATE,
            L"route_%03u_%04u%02u%02u_%02u%02u%02u",
            snapshot_sequence_, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
        return log_.output_dir() + L"\\" + name;
    }

    bool Probe::WriteRouteSnapshots(const RouteSnapshot& route, const GpsState& state)
    {
        const std::wstring base = SnapshotBaseName();
        const std::wstring physical_path = base + L"_physical.csv";
        const std::wstring logical_path = base + L"_logical.csv";
        const std::wstring meta_path = base + L"_meta.txt";

        FILE* physical = _wfopen(physical_path.c_str(), L"w");
        FILE* logical = _wfopen(logical_path.c_str(), L"w");
        FILE* meta = _wfopen(meta_path.c_str(), L"w");
        if (!physical || !logical || !meta)
        {
            if (physical) std::fclose(physical);
            if (logical) std::fclose(logical);
            if (meta) std::fclose(meta);
            log_.Warn("Failed to create one or more route snapshot files");
            return false;
        }

        std::fprintf(physical, "index,node_ptr,node_uid,distance_from_previous_m,time_from_previous_s,total_distance_till_end_m,total_time_till_end_s\n");
        for (std::size_t i = 0; i < route.physical.size(); ++i)
        {
            const auto& x = route.physical[i];
            std::fprintf(physical, "%zu,0x%llX,%016llX,%.6f,%.6f,%.6f,%.6f\n",
                i,
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(x.raw.node)),
                static_cast<unsigned long long>(x.node_uid),
                x.raw.distance_from_previous, x.raw.time_from_previous,
                x.raw.total_distance_till_end, x.raw.total_time_till_end);
        }

        std::fprintf(logical, "index,type,node_id,item_ptr,item_uid,distance_from_previous_m,time_from_previous_s,unknown_20,unknown_22,total_distance_till_end_m,total_time_till_end_s\n");
        for (std::size_t i = 0; i < route.logical.size(); ++i)
        {
            const auto& x = route.logical[i];
            std::fprintf(logical, "%zu,%u,%u,0x%llX,%016llX,%.6f,%.6f,%u,%u,%.6f,%.6f\n",
                i, x.raw.type, x.raw.node_id,
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(x.raw.item_ptr)),
                static_cast<unsigned long long>(x.item_uid),
                x.raw.distance_from_previous, x.raw.time_from_previous,
                x.raw.unknown_20, x.raw.unknown_22,
                x.raw.total_distance_till_end, x.raw.total_time_till_end);
        }

        float delta = std::numeric_limits<float>::quiet_NaN();
        const std::size_t estimated = EstimateCurrentPhysicalIndex(route, state.trip_distance, delta);
        std::fprintf(meta, "route_task=0x%llX\n", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(route.route_task)));
        std::fprintf(meta, "fingerprint=%016llX\n", static_cast<unsigned long long>(route.fingerprint));
        std::fprintf(meta, "trip_distance_m=%.6f\ntrip_time_s=%.6f\n", state.trip_distance, state.trip_time);
        std::fprintf(meta, "next_checkpoint_distance_m=%.6f\nnext_checkpoint_time_s=%.6f\n", state.next_checkpoint_distance, state.next_checkpoint_time);
        std::fprintf(meta, "physical_count=%zu\nroute_count=%zu\n", route.physical.size(), route.logical.size());
        if (estimated != kInvalidIndex)
        {
            std::fprintf(meta, "estimated_physical_index=%zu\nestimated_distance_to_item_m=%.6f\n", estimated, delta);
        }
        else
        {
            std::fprintf(meta, "estimated_physical_index=unknown\n");
        }
        for (std::size_t i = 0; i < state.voice.size(); ++i)
            std::fprintf(meta, "voice_%zu=%s\n", i, state.voice[i].c_str());

        std::fclose(physical);
        std::fclose(logical);
        std::fclose(meta);
        log_.Info("Route snapshots written: %ls_[physical|logical|meta]", base.c_str());
        return true;
    }

    std::string Probe::CsvEscape(const std::string& text)
    {
        std::string out = "\"";
        for (const char c : text)
        {
            if (c == '\"') out += "\"\"";
            else out.push_back(c);
        }
        out.push_back('\"');
        return out;
    }

    void Probe::AppendLiveCsv(const RouteSnapshot* route, const GpsState& state)
    {
        if (!live_csv_) return;

        std::size_t physical_count = 0;
        std::size_t logical_count = 0;
        std::size_t estimated = kInvalidIndex;
        float delta = std::numeric_limits<float>::quiet_NaN();
        std::uint64_t uid = 0;
        if (route && route->route_task)
        {
            physical_count = route->physical.size();
            logical_count = route->logical.size();
            estimated = EstimateCurrentPhysicalIndex(*route, state.trip_distance, delta);
            if (estimated != kInvalidIndex && estimated < route->physical.size()) uid = route->physical[estimated].node_uid;
        }

        const auto tick = GetTickCount64();
        if (estimated == kInvalidIndex)
        {
            std::fprintf(live_csv_, "%llu,%.6f,%.6f,%.6f,%.6f,%zu,%zu,,,%016llX",
                static_cast<unsigned long long>(tick), state.trip_distance, state.trip_time,
                state.next_checkpoint_distance, state.next_checkpoint_time,
                physical_count, logical_count, static_cast<unsigned long long>(uid));
        }
        else
        {
            std::fprintf(live_csv_, "%llu,%.6f,%.6f,%.6f,%.6f,%zu,%zu,%zu,%.6f,%016llX",
                static_cast<unsigned long long>(tick), state.trip_distance, state.trip_time,
                state.next_checkpoint_distance, state.next_checkpoint_time,
                physical_count, logical_count, estimated, delta, static_cast<unsigned long long>(uid));
        }
        for (const auto& voice : state.voice)
        {
            const std::string escaped = CsvEscape(voice);
            std::fprintf(live_csv_, ",%s", escaped.c_str());
        }
        std::fputc('\n', live_csv_);
        std::fflush(live_csv_);
    }

    std::wstring Probe::ModuleDirectory() const
    {
        wchar_t dll_path[32768]{};
        const DWORD len = GetModuleFileNameW(module_, dll_path, static_cast<DWORD>(std::size(dll_path)));
        if (len == 0 || len >= std::size(dll_path)) return L".";
        std::wstring path(dll_path, len);
        const auto pos = path.find_last_of(L"\\/");
        return pos == std::wstring::npos ? L"." : path.substr(0, pos);
    }

    std::wstring Probe::ConfigPath() const
    {
        return ModuleDirectory() + L"\\BetterGPSProbe.ini";
    }


    void Probe::RememberRoute(const RouteSnapshot& route,unsigned long long now){cached_task_=route.route_task;cached_at_=now;cached_physical_.clear();cached_logical_.clear();for(auto& x:route.physical)cached_physical_.push_back(x.raw);for(auto& x:route.logical)cached_logical_.push_back(x.raw);}
    bool Probe::RouteUnchanged(void* gps,unsigned long long now){
        if(capture_status_!="VALID"||!accepted_fingerprint_||!cached_task_||now-cached_at_>=2000)return false;
        void* task=nullptr;layout::ArrayDynWin physical{},logical{};
        if(!memory::SafeReadAt(gps,layout::GPS_ROUTE_TASK,task)||task!=cached_task_||!memory::SafeReadAt(task,layout::ROUTE_TASK_PHYSICAL_ITEMS,physical)||!memory::SafeReadAt(task,layout::ROUTE_TASK_ROUTE_ITEMS,logical))return false;
        if(physical.size!=cached_physical_.size()||logical.size!=cached_logical_.size()||!ValidateArrayHeader(physical,sizeof(layout::PhysicalRouteItemWin),"physical")||!ValidateArrayHeader(logical,sizeof(layout::RouteItemWin),"logical"))return false;
        poll_physical_.resize(physical.size);poll_logical_.resize(logical.size);
        if(!memory::SafeCopy(physical.value,poll_physical_.data(),poll_physical_.size()*sizeof(poll_physical_[0]))||!memory::SafeCopy(logical.value,poll_logical_.data(),poll_logical_.size()*sizeof(poll_logical_[0])))return false;
        void* verify=nullptr;layout::ArrayDynWin vp{},vl{};
        if(!memory::SafeReadAt(gps,layout::GPS_ROUTE_TASK,verify)||verify!=task||!memory::SafeReadAt(task,layout::ROUTE_TASK_PHYSICAL_ITEMS,vp)||!memory::SafeReadAt(task,layout::ROUTE_TASK_ROUTE_ITEMS,vl)||memcmp(&vp,&physical,sizeof(vp))||memcmp(&vl,&logical,sizeof(vl)))return false;
        for(size_t i=0;i<poll_physical_.size();++i){auto& a=poll_physical_[i];auto& b=cached_physical_[i];if(a.node!=b.node||a.distance_from_previous!=b.distance_from_previous||a.time_from_previous!=b.time_from_previous||a.total_distance_till_end!=b.total_distance_till_end||a.total_time_till_end!=b.total_time_till_end)return false;}
        for(size_t i=0;i<poll_logical_.size();++i){auto& a=poll_logical_[i];auto& b=cached_logical_[i];if(a.type!=b.type||a.node_id!=b.node_id||a.item_ptr!=b.item_ptr||a.distance_from_previous!=b.distance_from_previous||a.time_from_previous!=b.time_from_previous||a.total_distance_till_end!=b.total_distance_till_end||a.total_time_till_end!=b.total_time_till_end)return false;}return true;
    }
    void Probe::Tick()
    {bettergps_diag::Scope timing(bettergps_diag::ProbeTick);
        if (!initialized_) return;
        if(configuration_reset_.exchange(false)){accepted_fingerprint_=pending_fingerprint_=0;capture_status_.clear();}
        CaptureVehicle sample;{std::lock_guard<std::mutex> lock(vehicle_mutex_);sample=vehicle_;}
        const std::uint64_t now = GetTickCount64();
        if(now-live_tick_>=100)
        {
            sample.timestamp=now; sample.generation=generation_;
            capture_.Publish(sample); live_tick_=now;
        }
        if(sample.paused) return;
        if (now - last_poll_tick_ < config_.poll_interval_ms) return;
        last_poll_tick_ = now;

        void* gps = gps_address_.load();
        if (!gps) { SetCaptureStatus("GPS_UNAVAILABLE",now); accepted_fingerprint_=0; pending_fingerprint_=0; return; }

        if(RouteUnchanged(gps,now))return;
        RouteSnapshot route{};
        const bool route_ok = ReadRoute(gps, route);
        if (!route_ok)
        {
            SetCaptureStatus("INVALID_ROUTE_"+route_error_,now); accepted_fingerprint_=0; pending_fingerprint_=0;
            return;
        }
        if(!route.route_task)
        {
            pending_fingerprint_=0;
            if(capture_status_!="CLEAR_PENDING" && capture_status_!="NO_ROUTE")
            { SetCaptureStatus("CLEAR_PENDING",now); return; }
            if(capture_status_!="NO_ROUTE")
            {
                CaptureControl clear; clear.type="RouteCleared"; clear.timestamp=now; clear.generation=generation_+1;
                if(capture_.Publish(clear)) { generation_++; capture_status_="NO_ROUTE"; accepted_fingerprint_=0; }
            }
            return;
        }
        if(route.physical.size()<2 || route.logical.empty())
        { SetCaptureStatus("TRANSIENT_ROUTE",now); accepted_fingerprint_=0; pending_fingerprint_=0; return; }
        const auto publication=DecideRoutePublication(route.fingerprint,accepted_fingerprint_,pending_fingerprint_,capture_status_=="VALID");
        if(publication==RoutePublication::Retain){RememberRoute(route,now);return;}
        if(publication==RoutePublication::Stabilize)
        { SetCaptureStatus("RECALCULATING",now); return; }
        if(worker_stop_.load()||configuration_reset_.load())return;
        auto snapshot=std::make_shared<CaptureRoute>(); snapshot->fingerprint=route.fingerprint; snapshot->captured_at=now;
        for(const auto& p:route.physical) snapshot->nodes.push_back(p.evidence);
        for(const auto& p:route.physical) snapshot->physical.push_back({p.node_uid,p.raw.distance_from_previous,
            p.raw.total_distance_till_end,p.raw.time_from_previous,p.raw.total_time_till_end});
        for(const auto& p:route.logical) snapshot->logical.push_back({p.raw.type,p.raw.node_id,p.item_uid,
            p.raw.distance_from_previous,p.raw.total_distance_till_end,p.raw.time_from_previous,p.raw.total_time_till_end});
        CaptureControl control; control.type="RawRouteSnapshot"; control.timestamp=now; control.generation=generation_+1; control.route=snapshot;
        if(capture_.Publish(control)) { generation_++; accepted_fingerprint_=route.fingerprint; capture_status_="VALID"; RememberRoute(route,now); }
    }

    void Probe::SetCaptureStatus(const std::string& status,unsigned long long now)
    {
        if(capture_status_==status) return;
        CaptureControl c; c.status=status; c.timestamp=now; c.generation=generation_+1;
        if(capture_.Publish(c)) { generation_++; capture_status_=status; }
    }

    void SCSAPIFUNC Probe::ChannelCallback(scs_string_t name,scs_u32_t,const scs_value_t* value,scs_context_t context)
    {
        auto* self=static_cast<Probe*>(context); if(!self || !self->initialized_) return;
        std::lock_guard<std::mutex> lock(self->vehicle_mutex_);auto& v=self->vehicle_;
        if(std::strcmp(name,SCS_TELEMETRY_TRUCK_CHANNEL_world_placement)==0)
        {
            v.pose_valid=value && value->type==SCS_VALUE_TYPE_dplacement;
            if(v.pose_valid)
            {
                const auto& p=value->value_dplacement;
                v.pose_valid=std::isfinite(p.position.x)&&std::isfinite(p.position.y)&&std::isfinite(p.position.z)&&std::isfinite(p.orientation.heading);
                if(v.pose_valid) { v.x=p.position.x; v.y=p.position.y; v.z=p.position.z;
                    v.heading=std::fmod(std::fmod(p.orientation.heading,1.0f)+1.0f,1.0f); }
            }
        }
        else if(std::strcmp(name,SCS_TELEMETRY_TRUCK_CHANNEL_speed)==0)
        {
            v.speed_valid=value && value->type==SCS_VALUE_TYPE_float && std::isfinite(value->value_float.value);
            if(v.speed_valid) v.speed=value->value_float.value;
        }
        else if(std::strcmp(name,SCS_TELEMETRY_TRUCK_CHANNEL_navigation_distance)==0)
        {
            v.navigation_valid=value && value->type==SCS_VALUE_TYPE_float && std::isfinite(value->value_float.value) && value->value_float.value>=0;
            if(v.navigation_valid) v.navigation=value->value_float.value;
        }
    }
    void SCSAPIFUNC Probe::StateCallback(scs_event_t event,const void*,scs_context_t context)
    {
        auto* self=static_cast<Probe*>(context); if(!self)return;
        if(event==SCS_TELEMETRY_EVENT_configuration)
        {
            self->gps_address_=nullptr;self->configuration_reset_=true;
        }
        else {std::lock_guard<std::mutex> lock(self->vehicle_mutex_);self->vehicle_.paused=event!=SCS_TELEMETRY_EVENT_started;}
    }

    void SCSAPIFUNC Probe::FrameEndCallback(const scs_event_t, const void*, const scs_context_t context)
    {bettergps_diag::Refresh();bettergps_diag::Scope timing(bettergps_diag::FrameCallback);
        auto* self = static_cast<Probe*>(context);
        // Only resolve the bounded game-control descriptor on its owning SDK thread.
        // Full route traversal and consistency checks remain on the sampling worker.
        if(self&&self->initialized_.load()){auto now=GetTickCount64();if(now-self->address_poll_>=100){self->address_poll_=now;try{self->gps_address_=self->GetGpsManager();}catch(...){self->gps_address_=nullptr;}}}

    }
}
