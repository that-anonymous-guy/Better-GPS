// BetterGPS - DLL-only virtual mod for American Truck Simulator.
//
// VFS mounting strategy adapted from SPF-Framework (Apache-2.0):
// https://github.com/TrackAndTruckDevs/SPF-Framework
// Uses the reverse-engineered Prism3D UFS_MountDevice ABI/signature on ATS 1.61.
//
// Embedded asset set is a user-requested private merge of the supplied
// Google Maps Light + RWR phone GPS mods, with Road Trip phone compatibility.

#include <Windows.h>
#include "../shared/PerformanceDiagnostics.h"
#include <scssdk_telemetry.h>
#include "../Shared/StartupTrace.h"
#include <d3d11.h>
#include <dxgi.h>
#include <MinHook.h>
#include "NavigationRuntime.h"
#include "GuidancePresentation.h"
#include "RouteShields.h"
#include "TripPresentation.h"
#include "PhoneStartup.h"
#include "CompassPresentation.h"
#include "NightMode.h"
#include "NativeMapTheme.h"
#include "LiveMapTheme.h"
#include "NativeMapIcons.h"
#include "DisplayRegistry.h"
#include "SettingsMenu.h"
#include "SettingsMenuRenderer.h"
#include "MenuDockGpu.h"
#include "PhoneScreenPower.h"
#include "IndependentPhoneSurface.h"
#include "CarplayResources.h"
#include "F150Startup.h"
#include "NativeMapFork.h"
#include "VehicleInitialization.h"
#include "GameTimezone.h"
#include "GameAudio.h"
#include "DeviceNativeRuntime.h"
#include "TouchHooks.h"
#include "MapGpuAudit.h"

#include <array>
#include <atomic>
#include <cmath>
#include <mutex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <condition_variable>

namespace bettergps_overlay {

static scs_log_t g_log = nullptr;
static DWORD g_logOwner=0;
static std::mutex g_pendingLogMutex;
static std::vector<std::pair<scs_log_type_t,std::string>> g_pendingLogs;
static std::atomic<bool> g_shutdownStarted{false};
static std::filesystem::path g_shutdownPath;
static void shutdown_note(const char* stage,bool begin=false){if(g_shutdownPath.empty())return;std::ofstream out(g_shutdownPath,begin?std::ios::trunc:std::ios::app);out<<GetTickCount64()<<" "<<stage<<"\n";}
static uintptr_t g_mountFunction = 0;

static constexpr const char kAssetPackId[] = "folder-exact-v1";

struct MountSpec {
    const char* folder;
    const char* vpath;
    int order;
};

// Mount each modified directory onto the exact virtual directory it belongs to.
// This deliberately mirrors the VFS proof-of-concept that was validated in ATS,
// rather than replacing the broad /ui, /def and /material roots in one shot.
static constexpr MountSpec kMounts[] = {
    // Optional physical cameras bundled with the plugin; no separate mod.
    {"camera-hardware/automat/2f", "/automat/2f", 5800},
    {"camera-hardware/automat/8d", "/automat/8d", 5800},
    {"camera-hardware/automat/9d", "/automat/9d", 5800},
    {"camera-hardware/automat/d0", "/automat/d0", 5800},
    {"camera-hardware/automat/d3", "/automat/d3", 5800},
    {"camera-hardware/def/vehicle", "/def/vehicle", 5800},
    {"camera-hardware/def/vehicle/addon_hookups", "/def/vehicle/addon_hookups", 5800},
    {"camera-hardware/def/vehicle/truck/kenworth.t680_2022/accessory/bgps_cam", "/def/vehicle/truck/kenworth.t680_2022/accessory/bgps_cam", 5800},
    {"camera-hardware/def/vehicle/truck/kenworth.t680_2022/accessory/bgps_cam_l", "/def/vehicle/truck/kenworth.t680_2022/accessory/bgps_cam_l", 5800},
    {"camera-hardware/def/vehicle/truck/kenworth.t680_2022/accessory/bgps_cam_r", "/def/vehicle/truck/kenworth.t680_2022/accessory/bgps_cam_r", 5800},
    {"camera-hardware/def/vehicle/truck/kenworth.t680_2022/accessory/bgps_left", "/def/vehicle/truck/kenworth.t680_2022/accessory/bgps_left", 5800},
    {"camera-hardware/def/vehicle/truck/kenworth.t680_2022/accessory/bgps_right", "/def/vehicle/truck/kenworth.t680_2022/accessory/bgps_right", 5800},
    {"camera-hardware/def/vehicle/truck/kenworth.t680_2022/accessory/f_banner", "/def/vehicle/truck/kenworth.t680_2022/accessory/f_banner", 5800},
    // Blank only the stock labelled parking placeholder; live frames keep their native alias.
    {"camera-hardware/material/environment/mirror", "/material/environment/mirror", 5800},
    {"camera-hardware/material/ui/accessory/bettergps", "/material/ui/accessory/bettergps", 5800},
    {"camera-hardware/vehicle/truck/kenworth_t680_2022/accessory/bumper", "/vehicle/truck/kenworth_t680_2022/accessory/bumper", 5800},
    {"camera-hardware/vehicle/truck/kenworth_t680_2022/cabin", "/vehicle/truck/kenworth_t680_2022/cabin", 5800},
    {"camera-hardware/vehicle/truck/upgrade/bettergps/camera_finish", "/vehicle/truck/upgrade/bettergps/camera_finish", 5800},
    {"camera-hardware/vehicle/truck/upgrade/bettergps/pp804395", "/vehicle/truck/upgrade/bettergps/pp804395", 5800},
    {"camera-hardware/vehicle/truck/upgrade/bettergps/pp804396", "/vehicle/truck/upgrade/bettergps/pp804396", 5800},
    {"camera-hardware/vehicle/truck/upgrade/bettergps/pp804397", "/vehicle/truck/upgrade/bettergps/pp804397", 5800},
    {"camera-hardware/vehicle/truck/upgrade/bettergps/pp807032", "/vehicle/truck/upgrade/bettergps/pp807032", 5800},


    {"material/ui/dashboard", "/material/ui/dashboard", 5300},
    {"material/ui/dashboard/bettergps_overlaynew", "/material/ui/dashboard/bettergps_overlaynew", 5400},
    {"material/ui/f150_startup", "/material/ui/f150_startup", 5400},
    {"material/ui/f150_info_startup", "/material/ui/f150_info_startup", 5400},
    {"material/ui/bronco_startup", "/material/ui/bronco_startup", 5400},
    {"material/ui/bronco_info_startup", "/material/ui/bronco_info_startup", 5400},

    {"ui/dashboard", "/ui/dashboard", 5500},
    {"ui/template", "/ui/template", 5400},
};

static constexpr size_t kMountCount = sizeof(kMounts) / sizeof(kMounts[0]);
static std::array<uintptr_t, kMountCount> g_devices{};
static std::array<bool, kMountCount> g_mounted{};
static bool g_assetsReady = false;
static bool g_displayMountsReady=false;
static unsigned g_frameCount = 0;
static std::string g_assetRoot;

void log(scs_log_type_t type, const char* fmt, ...) {
    if(!g_log)return;
    char b[1400]{};
    va_list va;
    va_start(va, fmt);
    vsnprintf_s(b, sizeof(b), _TRUNCATE, fmt, va);
    va_end(va);
    std::string s = "[BetterGPS] ";
    s += b;
    if(g_logOwner&&GetCurrentThreadId()!=g_logOwner){std::lock_guard<std::mutex> lock(g_pendingLogMutex);if(g_pendingLogs.size()<256)g_pendingLogs.emplace_back(type,std::move(s));return;}
    g_log(type, s.c_str());
}
static void flush_pending_logs(){if(!g_log||g_logOwner&&GetCurrentThreadId()!=g_logOwner)return;std::vector<std::pair<scs_log_type_t,std::string>> pending;{std::lock_guard<std::mutex> lock(g_pendingLogMutex);pending.swap(g_pendingLogs);}for(auto& line:pending)g_log(line.first,line.second.c_str());}

struct PeView {
    uint8_t* base{};
    size_t imageSize{};
    uint8_t* text{};
    size_t textSize{};
};

bool get_pe_view(PeView& v) {
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    if (!base) return false;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    v.base = base;
    v.imageSize = nt->OptionalHeader.SizeOfImage;
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        char name[9]{};
        std::memcpy(name, sec->Name, 8);
        if (std::strcmp(name, ".text") == 0) {
            v.text = base + sec->VirtualAddress;
            v.textSize = sec->Misc.VirtualSize;
            break;
        }
    }
    return v.text && v.textSize;
}

uintptr_t find_ascii(const PeView& v, const char* needle) {
    const size_t n = std::strlen(needle);
    if (!n || n > v.imageSize) return 0;
    for (size_t i = 0; i + n <= v.imageSize; ++i) {
        if (std::memcmp(v.base + i, needle, n) == 0)
            return reinterpret_cast<uintptr_t>(v.base + i);
    }
    return 0;
}

uintptr_t find_lea_rcx_xref(const PeView& v, uintptr_t target) {
    // x64: LEA RCX,[RIP+disp32] -> 48 8D 0D xx xx xx xx
    for (size_t i = 0; i + 7 <= v.textSize; ++i) {
        uint8_t* p = v.text + i;
        if (p[0] != 0x48 || p[1] != 0x8D || p[2] != 0x0D) continue;
        int32_t disp{};
        std::memcpy(&disp, p + 3, sizeof(disp));
        const uintptr_t resolved = reinterpret_cast<uintptr_t>(p + 7) + disp;
        if (resolved == target)
            return reinterpret_cast<uintptr_t>(p);
    }
    return 0;
}

uintptr_t find_nearest_call_before(const PeView& v, uintptr_t anchor, size_t maxBack) {
    const uintptr_t textStart = reinterpret_cast<uintptr_t>(v.text);
    if (anchor < textStart || anchor >= textStart + v.textSize) return 0;
    const size_t back = static_cast<size_t>(anchor - textStart);
    const size_t limit = back > maxBack ? back - maxBack : 0;
    for (size_t pos = back; pos-- > limit;) {
        uint8_t* p = v.text + pos;
        if (*p != 0xE8 || pos + 5 > v.textSize) continue;
        int32_t disp{};
        std::memcpy(&disp, p + 1, sizeof(disp));
        const uintptr_t target = reinterpret_cast<uintptr_t>(p + 5) + disp;
        const uintptr_t moduleStart = reinterpret_cast<uintptr_t>(v.base);
        const uintptr_t moduleEnd = moduleStart + v.imageSize;
        if (target >= moduleStart && target < moduleEnd)
            return target;
    }
    return 0;
}

// Reverse-engineered by SPF-Framework for ATS/ETS2 1.61.
// Prism3D UFS_MountDevice stack parameter block (x64): 40 bytes.
struct UfsMountParams {
    int32_t type;         // 1 = standard directory device
    int32_t pool;         // 0 core, 1 user, 2 mod, 3 scs
    const char* physical;
    const char* vpath;
    int32_t field24;
    int32_t order;
    int32_t field32;
    int32_t field36;
};
static_assert(sizeof(UfsMountParams) == 40, "Unexpected Prism3D UFS ABI layout");
using UfsMountDeviceFn = uintptr_t (*)(UfsMountParams*);

std::string plugin_directory() {
    HMODULE self{};
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&plugin_directory), &self))
        return {};
    char path[MAX_PATH * 4]{};
    const DWORD n = GetModuleFileNameA(self, path, static_cast<DWORD>(sizeof(path)));
    if (!n || n >= sizeof(path)) return {};
    std::string s(path, n);
    const size_t slash = s.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : s.substr(0, slash);
}

static bool g_nativeMapDark=false;
static std::filesystem::path g_nativeThemeStatePath;
static void initialize_native_theme(bool dark);

bool prepare_assets() {
    if (g_assetsReady) return true;

    const std::string base = plugin_directory();
    if (base.empty()) {
        log(SCS_LOG_TYPE_error, "Could not resolve plugin directory");
        return false;
    }

    g_assetRoot = base + "\\BetterGPS_assets";
    const std::filesystem::path root(g_assetRoot);

    const std::filesystem::path required[] = {
        std::filesystem::path(base) / "layouts/vertical/phone.sii",
        root / "ui/template/dashboard_text.gglphn.sii",
        root / "material/ui/dashboard/bare_map.dds",
        root / "material/ui/dashboard/phone_map_background.mat",
        root / "material/ui/dashboard/phone_map_background.tobj",
        root / "material/ui/dashboard/phone_map_background.dds",
        root / "material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.mat",
        root / "material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.tobj",
        root / "material/ui/dashboard/bettergps_overlaynew/phone_layout_marker.dds",
        root / "def/map_data.sii"
    };

    for (const auto& file : required) {
        if (!std::filesystem::is_regular_file(file)) {
            log(SCS_LOG_TYPE_error, "Required asset missing: %s", file.string().c_str());
            return false;
        }
    }

    g_nativeThemeStatePath=std::filesystem::path(base)/L"BetterGPSMapTheme.state";
    g_nativeMapDark=native_map_dark(overlay_snapshot().theme,g_nativeThemeStatePath);
    if(!native_icons_load(root)){log(SCS_LOG_TYPE_error,"Native map icon pairs missing or invalid");return false;}
    initialize_native_theme(g_nativeMapDark);
    PeView image{};if(get_pe_view(image))voiceVariables=voice_resolve(image.base,image.imageSize);if(get_pe_view(image)&&live_map_install({image.base,image.imageSize},root,g_nativeMapDark))
        log(SCS_LOG_TYPE_message,"Native map lifecycle ready; game palettes remain unchanged");
    else log(SCS_LOG_TYPE_warning,"Live native palette unavailable: keeping startup map palette coherent");
    if(nativeFork.supported&&!nativeForkInstalled){if(native_fork_install()){log(SCS_LOG_TYPE_message,"[MapFork] Private native map source installed");const bool playerReady=native_player_install();log(playerReady?SCS_LOG_TYPE_message:SCS_LOG_TYPE_warning,"[PlayerMaterial] Native per-device player material hooks %s",playerReady?"installed":"unavailable");}else{log(SCS_LOG_TYPE_warning,"[MapFork] Hook unavailable; retaining native map source");display_generate(false);native_player_build_materials(root);}}
    if(!native_map_save(g_nativeThemeStatePath,g_nativeMapDark))log(SCS_LOG_TYPE_warning,"Native map theme state could not be saved");
    log(SCS_LOG_TYPE_message,"Native Predator %s map loaded; map SRV passes through unchanged",g_nativeMapDark?"dark":"light");

    log(SCS_LOG_TYPE_message,
        "Asset validation OK: using external BetterGPS_assets folder at %s",
        g_assetRoot.c_str());
    g_assetsReady = true;
    return true;
}

uintptr_t resolve_mount_function() {
    if (g_mountFunction) return g_mountFunction;
    PeView v{};
    if (!get_pe_view(v)) {
        log(SCS_LOG_TYPE_error, "Could not inspect amtrucks.exe PE image");
        return 0;
    }
    const char* marker = "[ufs] Home directory mount failed!";
    const uintptr_t str = find_ascii(v, marker);
    if (!str) {
        log(SCS_LOG_TYPE_error, "Could not find Prism3D UFS home-mount marker");
        return 0;
    }
    const uintptr_t xref = find_lea_rcx_xref(v, str);
    if (!xref) {
        log(SCS_LOG_TYPE_error, "Found UFS marker but no LEA RCX xref");
        return 0;
    }
    const uintptr_t fn = find_nearest_call_before(v, xref, 64);
    if (!fn) {
        log(SCS_LOG_TYPE_error, "Found UFS marker xref but no nearby UFS_MountDevice call");
        return 0;
    }
    g_mountFunction = fn;
    log(SCS_LOG_TYPE_message, "Resolved Prism3D UFS_MountDevice=%p", reinterpret_cast<void*>(fn));
    return fn;
}

bool mount_one(size_t index) {
    if (index >= kMountCount) return false;
    if (g_mounted[index]) return true;
    if (!prepare_assets()) return false;
    const uintptr_t addr = resolve_mount_function();
    if (!addr) return false;

    const MountSpec& spec = kMounts[index];
    const std::string physical = g_assetRoot + "\\" + spec.folder;
    if (!std::filesystem::is_directory(std::filesystem::path(physical))) {
        log(SCS_LOG_TYPE_error, "Missing asset directory %s", physical.c_str());
        return false;
    }

    UfsMountParams params{};
    params.type = 1;
    params.pool = 1;           // USER: resolves before MOD and SCS pools.
    params.physical = physical.c_str();
    params.vpath = spec.vpath;
    params.field24 = 0;
    params.order = spec.order;
    params.field32 = 0;
    params.field36 = 0;

    auto fn = reinterpret_cast<UfsMountDeviceFn>(addr);
    const uintptr_t device = fn(&params);
    if (!device) {
        log(SCS_LOG_TYPE_error, "UFS_MountDevice returned null for %s -> %s",
            physical.c_str(), spec.vpath);
        return false;
    }

    g_devices[index] = device;
    g_mounted[index] = true;
    log(SCS_LOG_TYPE_message, "Mounted %s -> %s in USER pool (order=%d device=%p)",
        physical.c_str(), spec.vpath, spec.order, reinterpret_cast<void*>(device));
    return true;
}

bool mount_all() {
    bool ok = true;
    size_t mountedCount = 0;
    for (size_t i = 0; i < kMountCount; ++i) {
        if (!mount_one(i)) ok = false;
        if (g_mounted[i]) ++mountedCount;
    }

    if(ok&&!g_displayMountsReady){std::set<std::string> dirs;for(auto& d:gpsDisplays)if(d.ready)dirs.insert(std::filesystem::path(d.ui).parent_path().generic_string());
        if(!dirs.empty())dirs.insert("/material/ui/bettergps_displays");dirs.insert("/material/ui/bettergps_player");
        for(auto& dir:displaySurfaceMounts)dirs.insert("/"+dir);
        for(auto& dir:dirs){auto physical=(displayBase/"generated-displays"/dir.substr(1)).string();UfsMountParams p{};p.type=1;p.pool=1;p.physical=physical.c_str();p.vpath=dir.c_str();p.order=5700;auto device=reinterpret_cast<UfsMountDeviceFn>(g_mountFunction)(&p);if(!device){ok=false;break;}log(SCS_LOG_TYPE_message,"Registered display directory mounted: %s -> %s",physical.c_str(),dir.c_str());}
        g_displayMountsReady=ok;
    }
    if (ok) {
        if(nativeFork.supported&&!native_fork_output_alias_register())log(SCS_LOG_TYPE_error,"[DeviceOutput] Dynamic output resource initialization failed after VFS mount");
        log(SCS_LOG_TYPE_message,
            "Virtual Google/RWR mod ACTIVE: %llu/%llu exact VFS directories mounted. "
            "Road Trip /ui/dashboard uses the same exact-mount strategy as the validated VFS test.",
            static_cast<unsigned long long>(mountedCount),
            static_cast<unsigned long long>(kMountCount));
    } else {
        log(SCS_LOG_TYPE_warning,
            "Virtual mod incomplete: %llu/%llu VFS directories mounted.",
            static_cast<unsigned long long>(mountedCount),
            static_cast<unsigned long long>(kMountCount));
    }
    return ok;
}

bool all_mounted() {
    if(!g_displayMountsReady)return false;
    for (bool mounted : g_mounted) {
        if (!mounted) return false;
    }
    return true;
}


// -----------------------------------------------------------------------------
// Navigation trip-info bridge: distance + remaining time.
//
// We NEVER rewrite or Map() a texture owned by Prism3D.  The UI contains two
// dedicated static marker textures with deliberately odd dimensions.  We observe
// those resources at creation time, capture the SRVs Prism3D creates for them,
// then substitute plugin-owned dynamic SRVs only when those exact marker SRVs
// are bound.  All Map()/Unmap() work is therefore performed only on textures
// owned by this plugin, on the D3D render thread.
// -----------------------------------------------------------------------------
static const GUID kNativePhoneView={0x6218aac3,0x2180,0x4321,{0x92,0x71,0x15,0x64,0x78,0x22,0x30,0x41}};
static const GUID kGoogleMapTexture={0x9acbed31,0x851b,0x4180,{0x84,0x12,0x81,0x12,0x55,0x13,0x28,0x39}};
static const GUID kGoogleMapTarget={0x9acbed32,0x851b,0x4180,{0x84,0x12,0x81,0x12,0x55,0x13,0x28,0x39}};
static const GUID kGoogleMapCaptureGeneration={0x9acbed35,0x851b,0x4180,{0x84,0x12,0x81,0x12,0x55,0x13,0x28,0x39}};
static const GUID kGoogleMapStyle={0x9acbed34,0x851b,0x4180,{0x84,0x12,0x81,0x12,0x55,0x13,0x28,0x39}};
static const GUID kGoogleMapDark={0x9acbed33,0x851b,0x4180,{0x84,0x12,0x81,0x12,0x55,0x13,0x28,0x39}};
static std::atomic<uint64_t> g_googleMapIdentityEpoch{1};
static std::mutex g_horizontalMapMutex;
static ID3D11ShaderResourceView* g_horizontalMapInput=nullptr;
static uint64_t g_horizontalMapGeneration=0;
struct PhoneMapInput {ID3D11ShaderResourceView* view=nullptr;uint64_t generation=0;};
static std::array<PhoneMapInput,managedDeviceCount> g_phoneMapInputs;
static std::mutex g_phoneMapInputMutex;
static thread_local int g_expectManagedMap=-1;
static ID3D11RenderTargetView* native_bound_target(ID3D11DeviceContext* context);
static std::atomic<unsigned> g_horizontalMapState{0},g_horizontalSourceMip{~0u},g_horizontalSourceMipCount{0};
static thread_local ID3D11DeviceContext* g_expectGoogleMap=nullptr;
static thread_local ID3D11Resource* g_expectGoogleMapTarget=nullptr; // Identity only, never dereferenced.
static thread_local unsigned g_expectGoogleMapBudget=0;
static thread_local bool g_expectHorizontalMap=false;
static void expect_google_map_source(ID3D11DeviceContext* context,bool horizontal=false){
 g_expectManagedMap=-1;
 g_expectHorizontalMap=horizontal;
 g_expectGoogleMap=context;g_expectGoogleMapBudget=32;g_expectGoogleMapTarget=nullptr;
 ID3D11RenderTargetView* target=native_bound_target(context);
 if(target){target->GetResource(&g_expectGoogleMapTarget);if(g_expectGoogleMapTarget)g_expectGoogleMapTarget->Release();target->Release();}
}
struct GoogleMapTargetState {uint64_t epoch=0,generation=0;bool google=false;};
static bool google_map_target(ID3D11RenderTargetView* view){if(!view)return false;GoogleMapTargetState state{};UINT size=sizeof(state);const auto epoch=g_googleMapIdentityEpoch.load(std::memory_order_relaxed),generation=nativeForkSourceGeneration.load();if(SUCCEEDED(view->GetPrivateData(kGoogleMapTarget,&size,&state))&&state.epoch==epoch&&state.generation==generation)return state.google;ID3D11Resource* resource=nullptr;view->GetResource(&resource);uint64_t captured=0;size=sizeof(captured);state.google=resource&&SUCCEEDED(resource->GetPrivateData(kGoogleMapTexture,&size,&captured))&&captured==generation;state.epoch=epoch;state.generation=generation;if(resource)resource->Release();view->SetPrivateData(kGoogleMapTarget,sizeof(state),&state);return state.google;}
static bool google_map_context(ID3D11DeviceContext* context,bool& dark,size_t* style=nullptr){
 ID3D11RenderTargetView* target=native_bound_target(context);const bool selected=google_map_target(target);
 if(selected){ID3D11Resource* resource=nullptr;target->GetResource(&resource);uint64_t stamp=0;UINT size=sizeof(stamp);const bool semantic=resource&&SUCCEEDED(resource->GetPrivateData(kGoogleMapCaptureGeneration,&size,&stamp))&&stamp==nativeForkSourceGeneration.load();if(semantic){size=sizeof(dark);resource->GetPrivateData(kGoogleMapDark,&size,&dark);if(style){size=sizeof(*style);resource->GetPrivateData(kGoogleMapStyle,&size,style);}resource->Release();target->Release();return true;}if(resource)resource->Release();uint64_t generation=0;UINT bytes=sizeof(generation);const auto queued=nativeForkQueuedThemeId.load();const bool current=SUCCEEDED(target->GetPrivateData(kGoogleMapCaptureGeneration,&bytes,&generation))&&generation==nativeForkSourceGeneration.load();bytes=sizeof(dark);if(!current||FAILED(target->GetPrivateData(kGoogleMapDark,&bytes,&dark)))dark=queued%2;if(style){bytes=sizeof(*style);if(!current||FAILED(target->GetPrivateData(kGoogleMapStyle,&bytes,style)))*style=queued/2;}}
 if(target)target->Release();return selected;
}
static void tag_google_map_source(ID3D11DeviceContext* context,ID3D11ShaderResourceView* view){
 if(g_expectGoogleMap!=context||!view)return;
 // UI batching may bind a font atlas or a null slot between these images.
 // Only a real map in this same UI target consumes the semantic expectation.
 if(g_expectGoogleMapBudget&&!--g_expectGoogleMapBudget){g_expectGoogleMap=nullptr;return;}
 ID3D11RenderTargetView* target=native_bound_target(context);ID3D11Resource* current=nullptr;
 if(target){target->GetResource(&current);target->Release();}const bool same=current==g_expectGoogleMapTarget;if(current)current->Release();
 if(!same){g_expectGoogleMap=nullptr;return;}
 ID3D11Resource* resource=nullptr;view->GetResource(&resource);ID3D11Texture2D* texture=nullptr;
 if(resource&&SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture))){
  D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
  if(desc.Width==1024&&desc.Height==1024&&(desc.BindFlags&D3D11_BIND_RENDER_TARGET)){const unsigned source=g_expectManagedMap>=0?unsigned(g_expectManagedMap)+2:g_expectHorizontalMap?1:0;const size_t style=managed_native_style(source);const bool dark=managed_native_dark(source,liveMapDark.load());const auto capturedGeneration=nativeForkSourceGeneration.load();resource->SetPrivateData(kGoogleMapStyle,sizeof(style),&style);resource->SetPrivateData(kGoogleMapDark,sizeof(dark),&dark);resource->SetPrivateData(kGoogleMapCaptureGeneration,sizeof(capturedGeneration),&capturedGeneration);
   if(g_expectManagedMap>=0&&g_expectManagedMap<int(managedDeviceCount)){
    std::lock_guard<std::mutex> lock(g_phoneMapInputMutex);auto& input=g_phoneMapInputs[g_expectManagedMap];ID3D11Resource* previous=nullptr;if(input.view)input.view->GetResource(&previous);const bool same=previous==resource;if(previous)previous->Release();
    if(!same){D3D11_SHADER_RESOURCE_VIEW_DESC full{};view->GetDesc(&full);full.Texture2D.MostDetailedMip=0;full.Texture2D.MipLevels=1;ID3D11Device* device=nullptr;texture->GetDevice(&device);ID3D11ShaderResourceView* selected=nullptr;{PhoneInternalScope internal;device->CreateShaderResourceView(texture,&full,&selected);}device->Release();if(selected){if(input.view)input.view->Release();input.view=selected;log(SCS_LOG_TYPE_message,"[PhoneMapInput] slot=%d native private map captured",g_expectManagedMap);}}
    if(input.view)input.generation=nativeForkSourceGeneration.load();g_expectManagedMap=-1;
   }
   g_expectGoogleMap=nullptr;mapGpuAudit.sample(context,view,0);if(g_expectHorizontalMap){D3D11_SHADER_RESOURCE_VIEW_DESC nativeView{};view->GetDesc(&nativeView);g_horizontalSourceMip=nativeView.Texture2D.MostDetailedMip;g_horizontalSourceMipCount=desc.MipLevels;
   {std::lock_guard<std::mutex> lock(g_horizontalMapMutex);
    // ATS may bind a reduced-mip SRV for its own UI pass. The audit reads
    // resource mip 0, which is where the map is actually rendered. Own that
    // exact full-resolution view rather than inheriting the consumer's LOD.
    ID3D11Resource* previous=nullptr;if(g_horizontalMapInput)g_horizontalMapInput->GetResource(&previous);const bool same=previous==resource;if(previous)previous->Release();
    if(!same){D3D11_SHADER_RESOURCE_VIEW_DESC base{};view->GetDesc(&base);
     if(base.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D){base.Texture2D.MostDetailedMip=0;base.Texture2D.MipLevels=1;
      ID3D11Device* device=nullptr;texture->GetDevice(&device);ID3D11ShaderResourceView* full=nullptr;
      {PhoneInternalScope internal;if(SUCCEEDED(device->CreateShaderResourceView(texture,&base,&full))){if(g_horizontalMapInput)g_horizontalMapInput->Release();g_horizontalMapInput=full;}}device->Release();
     }
    }
    if(same||g_horizontalMapInput){ID3D11Resource* selected=nullptr;g_horizontalMapInput->GetResource(&selected);if(selected==resource)g_horizontalMapGeneration=nativeForkSourceGeneration.load();if(selected)selected->Release();}
   }
   }
   uint64_t captured=0,generation=nativeForkSourceGeneration.load();UINT bytes=sizeof(captured);
   if(FAILED(resource->GetPrivateData(kGoogleMapTexture,&bytes,&captured))||captured!=generation){resource->SetPrivateData(kGoogleMapTexture,sizeof(generation),&generation);++g_googleMapIdentityEpoch;}
  }texture->Release();
 }if(resource)resource->Release();
}

static const GUID kPausedPhoneView={0xb7106413,0x5154,0x4535,{0x91,0x82,0x55,0x27,0x31,0x69,0x08,0x10}};
struct OverlayBridge {
    const char* name;
    UINT width;
    UINT height;
    ID3D11Texture2D* markerTexture{};          // game-owned, AddRef'd
    ID3D11ShaderResourceView* markerView{};    // game-owned, AddRef'd
    ID3D11Texture2D* liveTexture{};            // plugin-owned
    ID3D11ShaderResourceView* liveView{};      // plugin-owned
    DXGI_FORMAT liveFormat{DXGI_FORMAT_UNKNOWN};
    bool cleared=false;
    ID3D11ShaderResourceView* disabledView=nullptr;
    ID3D11ShaderResourceView* pausedView{}; // Owned by markerView private data.
};

static ID3D11ShaderResourceView* g_f150BlackView=nullptr;
static ID3D11ShaderResourceView* g_f150ClearView=nullptr;
static ID3D11ShaderResourceView* g_f150InfoBlackView=nullptr;
static ID3D11ShaderResourceView* g_f150InfoClearView=nullptr;

struct F150InfoFade {
 UINT width=f150InfoWidth,height=f150InfoHeight;
 SettingsMenuRenderer renderer;ID3D11Texture2D* target=nullptr;ID3D11ShaderResourceView* view=nullptr;ID3D11RenderTargetView* rtv=nullptr;
 void stop(){SettingsMenuRenderer::drop(rtv);SettingsMenuRenderer::drop(view);SettingsMenuRenderer::drop(target);renderer.stop();}
 bool start(ID3D11Device* device,UINT requestedWidth=f150InfoWidth,UINT requestedHeight=f150InfoHeight){stop();width=requestedWidth;height=requestedHeight;D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;return SUCCEEDED(device->CreateTexture2D(&d,nullptr,&target))&&SUCCEEDED(device->CreateShaderResourceView(target,nullptr,&view))&&SUCCEEDED(device->CreateRenderTargetView(target,nullptr,&rtv))&&renderer.start(device);}
 bool draw(ID3D11DeviceContext* context,ID3D11ShaderResourceView* source,void(STDMETHODCALLTYPE* bind)(ID3D11DeviceContext*,UINT,UINT,ID3D11ShaderResourceView* const*),float opacity){
  // A prior UI draw may still bind our fade SRV in another slot. Unbind every
  // alias before rendering into it and restore those bindings afterward.
  ID3D11ShaderResourceView* slots[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};context->PSGetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,slots);ID3D11ShaderResourceView* empty=nullptr;for(UINT i=0;i<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;++i)if(slots[i]==view)bind(context,i,1,&empty);
  const bool ok=renderer.render(context,source,target,0,0,float(width),float(height),bind,0,false,false,rtv,opacity,true);
  for(UINT i=0;i<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;++i){if(slots[i]==view)bind(context,i,1,&slots[i]);SettingsMenuRenderer::drop(slots[i]);}return ok;
 }
};
static F150InfoFade g_f150InfoFade;
static F150InfoFade g_f150CenterFade;
static OverlayBridge g_clusterIntroBridge{"T680ClusterIntro",clusterWidth,clusterHeight};
static std::atomic<uint64_t> clusterIntroUploaded{0};
static OverlayBridge g_mackClusterBridge{"MackClusterIntro",mackClusterWidth,mackClusterHeight};
static OverlayBridge g_mackBootBridge{"MackGpsBootGuard",8,4};
static OverlayBridge g_vnlClusterBridge{"Vnl2014ClusterIntro",vnlClusterWidth,vnlClusterHeight};
static OverlayBridge g_vnlBootBridge{"Vnl2014GpsBootGuard",12,4};
static std::atomic<uint64_t> vnlClusterUploaded{0};
static OverlayBridge g_intlClusterBridge{"InternationalClusterIntro",intlClusterWidth,intlClusterHeight};
static std::atomic<uint64_t> intlClusterUploaded{0};
static OverlayBridge g_icucClusterBridge{"IcucClusterIntro",icucClusterWidth,icucClusterHeight};
static OverlayBridge g_icuc5700ClusterBridge{"Icuc5700ClusterIntro",icuc5700ClusterWidth,icuc5700ClusterHeight};
static std::atomic<uint64_t> icuc5700ClusterUploaded{0};
static std::atomic<uint64_t> icucClusterUploaded{0};
static OverlayBridge g_icc5ClusterBridge{"Icc5ClusterIntro",icc5ClusterWidth,icc5ClusterHeight};
static OverlayBridge g_icc5BootBridge{"Icc5GpsBootGuard",32,4};
static std::atomic<uint64_t> icc5ClusterUploaded{0};
static OverlayBridge g_volvo25ClusterBridge{"Volvo25ClusterIntro",volvo25ClusterWidth,volvo25ClusterHeight};
static OverlayBridge g_volvo25BootBridge{"Volvo25GpsBootGuard",24,4};
static std::atomic<uint64_t> volvo25ClusterUploaded{0};
static OverlayBridge g_volvo18ClusterBridge{"Volvo18ClusterIntro",volvo18ClusterWidth,volvo18ClusterHeight};
static OverlayBridge g_volvo18BootBridge{"Volvo18GpsBootGuard",16,4};
static std::atomic<uint64_t> volvo18ClusterUploaded{0};
static OverlayBridge g_truckBootBridge{"TruckGpsBootGuard",20,4};
static std::atomic<uint64_t> mackClusterUploaded{0};
static bool is_cluster_bridge(const OverlayBridge* b){return b==&g_intlClusterBridge||b==&g_icuc5700ClusterBridge||b==&g_icucClusterBridge||b==&g_icc5ClusterBridge||b==&g_icc5BootBridge||b==&g_clusterIntroBridge||b==&g_mackClusterBridge||b==&g_mackBootBridge||b==&g_vnlClusterBridge||b==&g_vnlBootBridge||b==&g_volvo18ClusterBridge||b==&g_volvo18BootBridge||b==&g_truckBootBridge||b==&g_volvo25ClusterBridge||b==&g_volvo25BootBridge;}
static bool cluster_bridge_guard_needed(const OverlayBridge* b){return b==&g_intlClusterBridge?icuc_cluster_guard_needed(2):b==&g_icuc5700ClusterBridge?icuc_cluster_guard_needed(true):b==&g_icucClusterBridge?icuc_cluster_guard_needed():b==&g_icc5ClusterBridge?icc5_cluster_guard_needed():b==&g_icc5BootBridge?icc5_boot_guard_needed():b==&g_volvo25ClusterBridge?volvo25_cluster_guard_needed():b==&g_volvo25BootBridge?volvo25_boot_guard_needed():b==&g_truckBootBridge?truck_boot_guard_needed():b==&g_volvo18BootBridge?volvo18_boot_guard_needed():b==&g_volvo18ClusterBridge?volvo18_cluster_guard_needed():b==&g_vnlBootBridge?vnl_boot_guard_needed():b==&g_vnlClusterBridge?vnl_cluster_guard_needed():b==&g_mackBootBridge?mack_boot_guard_needed():b==&g_mackClusterBridge?mack_cluster_guard_needed():cluster_intro_guard_needed();}
static ID3D11ShaderResourceView* cluster_intro_guard(ID3D11Device* device,int tone=1);
static ID3D11ShaderResourceView* mack_boot_guard_view(ID3D11DeviceContext* context);
static ID3D11ShaderResourceView* vnl_boot_guard_view(ID3D11DeviceContext* context);
static ID3D11ShaderResourceView* volvo18_boot_guard_view(ID3D11DeviceContext* context);
static ID3D11ShaderResourceView* volvo25_boot_guard_view(ID3D11DeviceContext* context);
static ID3D11ShaderResourceView* icc5_boot_guard_view(ID3D11DeviceContext* context);
static ID3D11ShaderResourceView* truck_boot_guard_view(ID3D11DeviceContext* context);
static uint8_t startup_guard_gray(int tone){return tone==4?20:tone==3?13:tone==2?245:tone==1?10:0;}
static int startup_guard_tone(const OverlayBridge* b){return b==&g_intlClusterBridge||b==&g_icuc5700ClusterBridge||b==&g_icucClusterBridge?3:b==&g_icc5ClusterBridge||b==&g_icc5BootBridge?0:b==&g_truckBootBridge?truck_boot_guard_tone():b==&g_vnlBootBridge&&displayIgnition==1?2:1;}
static OverlayBridge g_f150InfoBridge{"F150InfoStartup",f150InfoWidth,f150InfoHeight};
static OverlayBridge g_f150SourceBridge{"F150MapSource",795,495};
static OverlayBridge g_f150MapBridge{"F150NativeMap",797,497};
static OverlayBridge g_f150Bridge{"F150Startup",f150Width,f150Height};
static OverlayBridge g_distBridge{"NavDist", 500, 73};
static OverlayBridge g_timeBridge{"NavTime", 400, 61};
static OverlayBridge g_clockBridge{"NavClock", 192, 64};
static OverlayBridge g_speedBridge{"NavSpeed", 192, 320};
static OverlayBridge g_guidanceBridge{"SmartGuidance", 960, 480};
static std::atomic<bool> g_guidanceDirty{true};
static OverlayBridge g_skinBridge{"PhoneSkin",360,740};
static std::atomic<bool> g_skinDirty{true};
static std::vector<std::unique_ptr<OverlayBridge>> g_displayBridges,g_surfaceBridges,g_managedBridges;
static std::array<std::string,managedDeviceCount*3> g_managedBridgeNames;
static int managed_bridge(const OverlayBridge* b){for(size_t i=0;i<g_managedBridges.size();++i)if(g_managedBridges[i].get()==b)return int(i);return -1;}
static void managed_bridges_init(){g_managedBridges.clear();for(unsigned slot=0;slot<managedDeviceCount;++slot)for(unsigned role=0;role<3;++role){auto i=slot*3+role;g_managedBridgeNames[i]="Managed_"+std::to_string(slot)+"_"+std::to_string(role);g_managedBridges.push_back(std::make_unique<OverlayBridge>(OverlayBridge{g_managedBridgeNames[i].c_str(),managed_marker_width(slot,role),managed_marker_height(role)}));}}

static bool surface_bridge(const OverlayBridge* b){for(auto& surface:g_surfaceBridges)if(surface.get()==b)return true;return false;}
static size_t surface_index(const OverlayBridge* b){for(size_t i=0;i<gpsDisplays.size();++i)if(gpsDisplays[i].surfaceWidth==b->width&&gpsDisplays[i].independent)return i;return 32;}


static std::atomic<float> g_compassHeading{0};
static bool g_drivingStarted=false,g_gameplayTransition=false;
static std::atomic<bool> g_freshDrivingPose{false},g_freshDrivingSpeed{false};
static std::atomic<bool> g_warmResumeReady{false},g_preservePreparedOnStart{false};
static uint64_t g_gameplayPausedAt=0;
static void suspend_gameplay(uint64_t now);
static bool update_gameplay();
static std::atomic<bool> g_haveCompassHeading{false};
static std::atomic<bool> g_tripPresent{false};
static std::atomic<bool> g_darkMode{false},g_requestedMapDark{false},g_themeSaveDirty{false};
static bool presentation_dark(){return displayDarkOverride>=0?displayDarkOverride!=0:g_darkMode.load();}
static std::atomic<bool> g_solarReset{true};
static std::atomic<uint64_t> g_mapThemeEpoch{0};
static std::shared_ptr<const SolarSettings> g_solarSettings=std::make_shared<const SolarSettings>();
static std::atomic<double> g_worldX{0},g_worldZ{0};
static std::atomic<bool> g_haveWorldPosition{false};
static OverlayBridge g_mapBridge{"PhoneMap",256,256};
static OverlayBridge g_mapBackgroundBridge{"MapBackground",360,720};
static std::atomic<bool> g_mapBackgroundDirty{true};
static std::vector<OverlayBridge*> all_bridges(){std::vector<OverlayBridge*> result={&g_distBridge,&g_timeBridge,&g_clockBridge,&g_speedBridge,&g_guidanceBridge,&g_skinBridge};for(auto& b:g_displayBridges)result.push_back(b.get());for(auto& b:g_surfaceBridges)result.push_back(b.get());for(auto& b:g_managedBridges)result.push_back(b.get());result.push_back(&g_f150SourceBridge);result.push_back(&g_f150MapBridge);result.push_back(&g_f150Bridge);result.push_back(&g_f150InfoBridge);result.push_back(&g_clusterIntroBridge);result.push_back(&g_mackClusterBridge);result.push_back(&g_mackBootBridge);result.push_back(&g_vnlClusterBridge);result.push_back(&g_vnlBootBridge);result.push_back(&g_intlClusterBridge);result.push_back(&g_icuc5700ClusterBridge);result.push_back(&g_icucClusterBridge);result.push_back(&g_icc5ClusterBridge);result.push_back(&g_icc5BootBridge);result.push_back(&g_volvo25ClusterBridge);result.push_back(&g_volvo25BootBridge);result.push_back(&g_volvo18ClusterBridge);result.push_back(&g_volvo18BootBridge);result.push_back(&g_truckBootBridge);result.push_back(&g_mapBridge);result.push_back(&g_mapBackgroundBridge);return result;}


struct PreparedImage {std::mutex mutex;std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0,uploaded=0,powerState=0,powerSession=0,powerEpoch=0,themeEpoch=0,managedRevision=0;int powerIgnition=-1,powerPhase=-99999;PageVisual page;bool pageReady=false;std::atomic<uint64_t> lastUse{0};};
static std::array<PreparedImage,33> g_prepared;
static PreparedImage g_preparedMenu;
static std::array<PreparedImage,managedDeviceCount> g_managedPrepared;
static PreparedImage g_clusterIntroPrepared;
static PreparedImage g_mackClusterPrepared;
static PreparedImage g_vnlClusterPrepared;
static PreparedImage g_volvo18ClusterPrepared;
static PreparedImage g_volvo25ClusterPrepared;
static PreparedImage g_icucClusterPrepared,g_icuc5700ClusterPrepared,g_intlClusterPrepared;
static PreparedImage g_icc5ClusterPrepared;
static std::array<uint64_t,managedDeviceCount> g_managedPreparedRevision{};
static std::atomic<bool> g_cpuRunning{false},g_cpuStop{false};static std::thread g_cpuWorker;
static std::mutex g_cpuWaitMutex;static std::condition_variable g_cpuWake;
static void start_cpu_worker(bool pollHost=true);
static void stop_cpu_worker();
static size_t prepared_index(const OverlayBridge& b){return &b==&g_skinBridge?32:(b.width-(b.height==500?804:364))/4;}
static std::atomic<float> g_navDistanceM{0.0f};
static std::atomic<int> g_navDistanceTenths{-1};
static std::atomic<bool> g_haveNavDistance{false};
static std::atomic<bool> g_distDirty{true};
static bool g_loggedFirstDistance = false;

static std::atomic<float> g_navTimeSec{0.0f};
static std::atomic<int> g_navTimeMinutes{-1};
static std::atomic<bool> g_haveNavTime{false};
static std::atomic<bool> g_timeDirty{true};
static bool g_loggedFirstTime = false;

static std::atomic<uint32_t> g_gameTimeMinutes{0};
static std::atomic<bool> g_haveGameTime{false};
static std::atomic<int32_t> g_multiplayerTimeOffset{0};
static bool g_loggedFirstGameTime = false;
static int64_t current_game_minutes(){return g_haveGameTime.load(std::memory_order_acquire)?game_display_minutes(g_gameTimeMinutes.load(std::memory_order_acquire),g_multiplayerTimeOffset.load(std::memory_order_acquire),gameTimezoneState.load(std::memory_order_acquire)):-1;}
static std::atomic<bool> g_clockDirty{true};
static std::atomic<bool> g_speedDirty{true};
static std::atomic<float> g_truckSpeedMps{0.0f};
static std::atomic<float> g_speedLimitMps{0.0f};
static std::atomic<bool> g_haveTruckSpeed{false};
static std::atomic<bool> g_haveSpeedLimit{false};

using CreateTexture2D_t = HRESULT(STDMETHODCALLTYPE*)(
    ID3D11Device*, const D3D11_TEXTURE2D_DESC*,
    const D3D11_SUBRESOURCE_DATA*, ID3D11Texture2D**);
using CreateShaderResourceView_t = HRESULT(STDMETHODCALLTYPE*)(
    ID3D11Device*, ID3D11Resource*,
    const D3D11_SHADER_RESOURCE_VIEW_DESC*, ID3D11ShaderResourceView**);
using PSSetShaderResources_t = void(STDMETHODCALLTYPE*)(
    ID3D11DeviceContext*, UINT, UINT, ID3D11ShaderResourceView* const*);
using UpdateSubresource_t=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT);
using CopyResource_t=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,ID3D11Resource*);
using CopyRegion_t=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,UINT,UINT,UINT,ID3D11Resource*,UINT,const D3D11_BOX*);

static CreateTexture2D_t g_origCreateTexture2D = nullptr;
static CreateShaderResourceView_t g_origCreateSRV = nullptr;
static PSSetShaderResources_t g_origPSSetSRV = nullptr;
static UpdateSubresource_t g_origUpdateSubresource=nullptr;
static CopyResource_t g_origCopyResource=nullptr;static CopyRegion_t g_origCopyRegion=nullptr;
static void* g_addrCopyResource=nullptr;static void* g_addrCopyRegion=nullptr;
static void* g_addrCreateTexture2D = nullptr;
static void* g_addrCreateSRV = nullptr;
static void* g_addrPSSetSRV = nullptr;
static void* g_addrUpdateSubresource=nullptr;
static void* g_addrClearRTV=nullptr;
static std::array<void*,6> g_deferredHookAddresses{};
static PSSetShaderResources_t g_origDeferredPSSetSRV=nullptr;
static UpdateSubresource_t g_origDeferredUpdate=nullptr;
static void (STDMETHODCALLTYPE* g_origDeferredClear)(ID3D11DeviceContext*,ID3D11RenderTargetView*,const FLOAT*)=nullptr;
static void STDMETHODCALLTYPE bind_original_srv(ID3D11DeviceContext* c,UINT start,UINT count,ID3D11ShaderResourceView* const* views){auto fn=c->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED&&g_origDeferredPSSetSRV?g_origDeferredPSSetSRV:g_origPSSetSRV;fn(c,start,count,views);}

static const GUID kManagedDeferredTarget={0x68afe321,0xc018,0x47ac,{0xb2,0xf9,0x34,0x22,0x73,0x42,0x12,0x91}};
using SetTargets_t=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,ID3D11RenderTargetView* const*,ID3D11DepthStencilView*);
using SetTargetsUav_t=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,ID3D11RenderTargetView* const*,ID3D11DepthStencilView*,UINT,UINT,ID3D11UnorderedAccessView* const*,const UINT*);
using FinishCommands_t=HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,BOOL,ID3D11CommandList**);
static SetTargets_t g_origSetTargets=nullptr;static SetTargetsUav_t g_origSetTargetsUav=nullptr;static FinishCommands_t g_origFinishCommands=nullptr;
static SetTargets_t g_origDeferredTargets=nullptr;static SetTargetsUav_t g_origDeferredTargetsUav=nullptr;static FinishCommands_t g_origDeferredFinish=nullptr;
static void *g_addrSetTargets=nullptr,*g_addrSetTargetsUav=nullptr,*g_addrFinishCommands=nullptr;
static ID3D11RenderTargetView* native_bound_target(ID3D11DeviceContext* context){ID3D11RenderTargetView* target=nullptr;if(context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED){UINT bytes=sizeof(target);context->GetPrivateData(kManagedDeferredTarget,&bytes,&target);}else context->OMGetRenderTargets(1,&target,nullptr);return target;}
static void managed_remember_target(ID3D11DeviceContext* c,UINT n,ID3D11RenderTargetView* const* v){if(!phoneInternalWork&&c->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED)c->SetPrivateDataInterface(kManagedDeferredTarget,n&&v?v[0]:nullptr);}
static void STDMETHODCALLTYPE hook_SetTargets(ID3D11DeviceContext* c,UINT n,ID3D11RenderTargetView* const* v,ID3D11DepthStencilView* d){(c->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED&&g_origDeferredTargets?g_origDeferredTargets:g_origSetTargets)(c,n,v,d);managed_remember_target(c,n,v);}
static void STDMETHODCALLTYPE hook_SetTargetsUav(ID3D11DeviceContext* c,UINT n,ID3D11RenderTargetView* const* v,ID3D11DepthStencilView* d,UINT first,UINT count,ID3D11UnorderedAccessView* const* u,const UINT* counts){(c->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED&&g_origDeferredTargetsUav?g_origDeferredTargetsUav:g_origSetTargetsUav)(c,n,v,d,first,count,u,counts);if(n!=D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)managed_remember_target(c,n,v);}
static HRESULT STDMETHODCALLTYPE hook_FinishCommands(ID3D11DeviceContext* c,BOOL restore,ID3D11CommandList** commands){auto result=(c->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED&&g_origDeferredFinish?g_origDeferredFinish:g_origFinishCommands)(c,restore,commands);if(!restore)c->SetPrivateDataInterface(kManagedDeferredTarget,nullptr);return result;}

static void (STDMETHODCALLTYPE* g_origClearRTV)(ID3D11DeviceContext*,ID3D11RenderTargetView*,const FLOAT*)=nullptr;
static void STDMETHODCALLTYPE hook_ClearRTV(ID3D11DeviceContext* context,ID3D11RenderTargetView* target,const FLOAT* color){
 TouchComposition::before_view(target);
 if(!phoneInternalWork&&!renderStopping&&color&&target){bool palette=false;unsigned theme=0;for(unsigned i=0;i<nativeMapThemeCount;++i){const auto known=native_map_clear(i);if(std::abs(color[0]-known[0])<.00001f&&std::abs(color[1]-known[1])<.00001f&&std::abs(color[2]-known[2])<.00001f&&std::abs(color[3]-known[3])<.00001f){palette=true;theme=i;break;}}
  if(palette||google_map_target(target)){bool candidate=google_map_target(target);if(!candidate&&palette){ID3D11Resource* resource=nullptr;target->GetResource(&resource);ID3D11Texture2D* texture=nullptr;if(resource&&SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture))){D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);candidate=d.Width==1024&&d.Height==1024;texture->Release();}if(resource)resource->Release();}
   if(candidate&&palette){const bool dark=palette?theme%2:color[0]<.5f;const size_t style=palette?theme/2:0;target->SetPrivateData(kGoogleMapDark,sizeof(dark),&dark);target->SetPrivateData(kGoogleMapStyle,sizeof(style),&style);const auto generation=nativeForkSourceGeneration.load();target->SetPrivateData(kGoogleMapCaptureGeneration,sizeof(generation),&generation);ID3D11Resource* resource=nullptr;target->GetResource(&resource);if(resource){resource->SetPrivateData(kGoogleMapStyle,sizeof(style),&style);resource->SetPrivateData(kGoogleMapDark,sizeof(dark),&dark);resource->SetPrivateData(kGoogleMapCaptureGeneration,sizeof(generation),&generation);resource->Release();}}
  }
 }
 (context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED&&g_origDeferredClear?g_origDeferredClear:g_origClearRTV)(context,target,color);
}

static std::mutex g_d3dMutex;
static bool g_hooksReady = false;

static bool marker_format(DXGI_FORMAT f) {
    return f == DXGI_FORMAT_B8G8R8A8_UNORM ||
           f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
           f == DXGI_FORMAT_R8G8B8A8_UNORM ||
           f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
           f == DXGI_FORMAT_R8G8B8A8_TYPELESS ||
           f == DXGI_FORMAT_B8G8R8A8_TYPELESS;
}

static void reset_marker_locked(OverlayBridge& b) {
    if(b.markerView)b.markerView->SetPrivateDataInterface(kPausedPhoneView,nullptr);
    if(b.markerView)b.markerView->SetPrivateDataInterface(kNativePhoneView,nullptr);
    b.pausedView=nullptr;
    if (b.markerView) { b.markerView->Release(); b.markerView = nullptr; }
    if (b.markerTexture) { b.markerTexture->Release(); b.markerTexture = nullptr; }
}

static void release_live_locked(OverlayBridge& b) {
    if(b.disabledView){b.disabledView->Release();b.disabledView=nullptr;}
    if (b.liveView) { b.liveView->Release(); b.liveView = nullptr; }
    if (b.liveTexture) { b.liveTexture->Release(); b.liveTexture = nullptr; }
    b.liveFormat = DXGI_FORMAT_UNKNOWN;
}

static OverlayBridge* bridge_for_desc(const D3D11_TEXTURE2D_DESC* d) {
    if (!d || !marker_format(d->Format) || !(d->BindFlags & D3D11_BIND_SHADER_RESOURCE))
        return nullptr;
    if (d->Width == g_distBridge.width && d->Height == g_distBridge.height)
        return &g_distBridge;
    if (d->Width == g_timeBridge.width && d->Height == g_timeBridge.height)
        return &g_timeBridge;
    if (d->Width == g_clockBridge.width && d->Height == g_clockBridge.height)
        return &g_clockBridge;
    if (d->Width == g_speedBridge.width && d->Height == g_speedBridge.height)
        return &g_speedBridge;
    if (d->Width == g_guidanceBridge.width && d->Height == g_guidanceBridge.height)
        return &g_guidanceBridge;
    if(d->Width==g_f150InfoBridge.width&&d->Height==g_f150InfoBridge.height)return &g_f150InfoBridge;
    if(d->Width==clusterWidth&&d->Height==clusterHeight)return &g_clusterIntroBridge;
    if(d->Width==mackClusterWidth&&d->Height==mackClusterHeight)return &g_mackClusterBridge;
    if(d->Width==vnlClusterWidth&&d->Height==vnlClusterHeight)return &g_vnlClusterBridge;
    if(d->Width==12&&d->Height==4)return &g_vnlBootBridge;
    if(d->Width==intlClusterWidth&&d->Height==intlClusterHeight)return &g_intlClusterBridge;
    if(d->Width==icuc5700ClusterWidth&&d->Height==icuc5700ClusterHeight)return &g_icuc5700ClusterBridge;
    if(d->Width==icucClusterWidth&&d->Height==icucClusterHeight)return &g_icucClusterBridge;
    if(d->Width==icc5ClusterWidth&&d->Height==icc5ClusterHeight)return &g_icc5ClusterBridge;
    if(d->Width==32&&d->Height==4)return &g_icc5BootBridge;
    if(d->Width==volvo25ClusterWidth&&d->Height==volvo25ClusterHeight)return &g_volvo25ClusterBridge;
    if(d->Width==24&&d->Height==4)return &g_volvo25BootBridge;
    if(d->Width==volvo18ClusterWidth&&d->Height==volvo18ClusterHeight)return &g_volvo18ClusterBridge;
    if(d->Width==16&&d->Height==4)return &g_volvo18BootBridge;
    if(d->Width==20&&d->Height==4)return &g_truckBootBridge;
    if(d->Width==g_mackBootBridge.width&&d->Height==g_mackBootBridge.height)return &g_mackBootBridge;
    if(d->Width==g_f150Bridge.width&&d->Height==g_f150Bridge.height)return &g_f150Bridge;
    if(d->Width==g_skinBridge.width&&d->Height==g_skinBridge.height)return &g_skinBridge;
    for(auto& b:g_displayBridges)if(d->Width==b->width&&d->Height==b->height)return b.get();for(auto& b:g_surfaceBridges)if(d->Width==b->width&&d->Height==b->height)return b.get();
    for(auto& b:g_managedBridges)if(d->Width==b->width&&d->Height==b->height)return b.get();
    if(d->Width==g_f150SourceBridge.width&&d->Height==g_f150SourceBridge.height)return &g_f150SourceBridge;
    if(d->Width==g_f150MapBridge.width&&d->Height==g_f150MapBridge.height)return &g_f150MapBridge;
    if(d->Width==g_mapBridge.width&&d->Height==g_mapBridge.height)return &g_mapBridge;
    if(d->Width==g_mapBackgroundBridge.width&&d->Height==g_mapBackgroundBridge.height)return &g_mapBackgroundBridge;
    return nullptr;
}

static bool marker_signature(OverlayBridge* bridge,const D3D11_TEXTURE2D_DESC* desc,const D3D11_SUBRESOURCE_DATA* initial) {
    if(!initial||!initial->pSysMem||initial->SysMemPitch<desc->Width*4)return false;
    OverlayBridge* bridges[]={&g_distBridge,&g_timeBridge,&g_clockBridge,&g_speedBridge,&g_guidanceBridge, &g_skinBridge, &g_mapBridge, &g_mapBackgroundBridge};
    unsigned role=bridge==&g_f150Bridge?10:bridge==&g_f150InfoBridge?11:bridge==&g_f150MapBridge?12:bridge==&g_f150SourceBridge?13:0;for(unsigned i=0;i<8;++i)if(bridges[i]==bridge)role=i+1;for(auto& b:g_displayBridges)if(b.get()==bridge)role=6;if(surface_bridge(bridge))role=9;
    const int managed=managed_bridge(bridge);if(managed>=0)role=managed_marker_role(unsigned(managed/3),unsigned(managed%3));
    if(bridge==&g_clusterIntroBridge)role=150;
    if(bridge==&g_mackClusterBridge)role=151;
    if(bridge==&g_vnlClusterBridge)role=153;
    if(bridge==&g_vnlBootBridge)role=154;
    if(bridge==&g_volvo18ClusterBridge)role=155;
    if(bridge==&g_volvo18BootBridge)role=156;
    if(bridge==&g_truckBootBridge)role=157;
    if(bridge==&g_volvo25ClusterBridge)role=158;
    if(bridge==&g_icucClusterBridge)role=162;
    if(bridge==&g_icuc5700ClusterBridge)role=163;
    if(bridge==&g_intlClusterBridge)role=164;
    if(bridge==&g_icc5ClusterBridge)role=160;
    if(bridge==&g_icc5BootBridge)role=161;
    if(bridge==&g_volvo25BootBridge)role=159;
    if(bridge==&g_mackBootBridge)role=152;
    auto* p=static_cast<const uint8_t*>(initial->pSysMem);
    const bool bgra=desc->Format==DXGI_FORMAT_B8G8R8A8_UNORM||desc->Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||desc->Format==DXGI_FORMAT_B8G8R8A8_TYPELESS;
    for(unsigned i=0;i<4;++i){const uint8_t red=static_cast<uint8_t>(0x31+role+i);if(p[4*i]!=(bgra?0xA7:red)||p[4*i+1]!=0x5B||p[4*i+2]!=(bgra?red:0xA7)||p[4*i+3]!=255)return false;}
    return true;
}

static HRESULT STDMETHODCALLTYPE hook_CreateTexture2D(
    ID3D11Device* device,
    const D3D11_TEXTURE2D_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial,
    ID3D11Texture2D** out)
{
    const HRESULT hr = g_origCreateTexture2D(device, desc, initial, out);
    if (!renderStopping && SUCCEEDED(hr) && out && *out) {
        if(!phoneInternalWork&&desc&&initial)native_icon_identify(*out,*desc,initial->pSysMem,initial->SysMemPitch);
        if (OverlayBridge* b = bridge_for_desc(desc); b && marker_signature(b,desc,initial)) {
            std::unique_lock<std::mutex> lock(g_d3dMutex,std::try_to_lock);if(!lock.owns_lock())return hr;
            reset_marker_locked(*b);
            b->markerTexture = *out;
            b->markerTexture->AddRef();
            log(SCS_LOG_TYPE_message,
                "%s marker texture observed: %ux%u fmt=%u usage=%u bind=0x%X mips=%u initial=%s resource=%p",
                b->name, desc->Width, desc->Height, static_cast<unsigned>(desc->Format),
                static_cast<unsigned>(desc->Usage), desc->BindFlags, desc->MipLevels,
                initial ? "yes" : "no", b->markerTexture);
        }
    }
    return hr;
}

static int managed_boot_guard_tone(const ManagedDevice& config){return config.splash=="copilot/international"?1:config.splash=="copilot/zonar"?4:config.splash=="tomtom/cascadia_icc5"?0:config.splash=="garmin/vnl2014"?(displayIgnition==1?2:1):int(config.splash=="copilot/t680"||config.splash=="tomtom/cascadia_icc5"||config.splash=="tomtom/mack"||config.splash=="tomtom/volvo18"||config.splash=="sygic/vnl2025");}
static bool create_live_resources_locked(OverlayBridge& b, ID3D11Device* device) {
    if (b.liveTexture && b.liveView){ID3D11Device* previous=nullptr;b.liveTexture->GetDevice(&previous);const bool same=previous==device;previous->Release();if(same){if(b.disabledView&&b.markerView)b.markerView->SetPrivateDataInterface(kNativePhoneView,b.disabledView);if(is_cluster_bridge(&b)&&b.markerView)b.markerView->SetPrivateDataInterface(kPausedPhoneView,cluster_bridge_guard_needed(&b)?cluster_intro_guard(device,startup_guard_tone(&b)):b.disabledView);return true;}}

    release_live_locked(b);
    if(&b==&g_f150Bridge&&g_f150BlackView){g_f150BlackView->Release();g_f150BlackView=nullptr;}
    if(&b==&g_f150Bridge&&g_f150ClearView){g_f150ClearView->Release();g_f150ClearView=nullptr;}
    if(&b==&g_f150InfoBridge)g_f150InfoFade.stop();
    if(&b==&g_f150Bridge)g_f150CenterFade.stop();
    if(&b==&g_f150InfoBridge&&g_f150InfoBlackView){g_f150InfoBlackView->Release();g_f150InfoBlackView=nullptr;}
    if(&b==&g_f150InfoBridge&&g_f150InfoClearView){g_f150InfoClearView->Release();g_f150InfoClearView=nullptr;}

    D3D11_TEXTURE2D_DESC d{};
    d.Width = b.width;
    d.Height = b.height;
    d.MipLevels = 1;
    d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DYNAMIC;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if(b.height==500||managed_bridge(&b)>=0||is_cluster_bridge(&b))d.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    if(&b==&g_clusterIntroBridge)clusterIntroUploaded=0;
    if(&b==&g_mackClusterBridge)mackClusterUploaded=0;
    if(&b==&g_vnlClusterBridge)vnlClusterUploaded=0;
    if(&b==&g_volvo18ClusterBridge)volvo18ClusterUploaded=0;
    if(&b==&g_volvo25ClusterBridge)volvo25ClusterUploaded=0;
    if(&b==&g_icucClusterBridge)icucClusterUploaded=0;
    if(&b==&g_icuc5700ClusterBridge)icuc5700ClusterUploaded=0;
    if(&b==&g_intlClusterBridge)intlClusterUploaded=0;
    if(&b==&g_icc5ClusterBridge)icc5ClusterUploaded=0;
    if(&b==&g_mapBridge){d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags|=D3D11_BIND_RENDER_TARGET;d.CPUAccessFlags=0;}
    // Never expose uninitialized or transparent skin pixels before the worker
    // publishes its first image. This is independent of the ignition telemetry.
    std::vector<uint8_t> initialBlack;D3D11_SUBRESOURCE_DATA initialData{};
    if(is_cluster_bridge(&b)){initialBlack.assign(size_t(d.Width)*d.Height*4,startup_guard_gray(startup_guard_tone(&b)));for(size_t i=3;i<initialBlack.size();i+=4)initialBlack[i]=255;initialData.pSysMem=initialBlack.data();initialData.SysMemPitch=d.Width*4;}
    if(&b==&g_skinBridge||((b.height==740&&b.width>=364)||(b.height==500&&b.width>=804))||surface_bridge(&b)){initialBlack.resize(size_t(d.Width)*d.Height*4);for(size_t i=3;i<initialBlack.size();i+=4)initialBlack[i]=255;initialData.pSysMem=initialBlack.data();initialData.SysMemPitch=d.Width*4;}
    if(managed_bridge(&b)>=0){const int managed=managed_bridge(&b);const auto config=managed_snapshot(unsigned(managed/3));const int tone=managed_boot_guard_tone(config);initialBlack.assign(size_t(d.Width)*d.Height*4,managed%3==0&&managed_visible_splash(config)?startup_guard_gray(tone):0);if(managed%3==0)for(size_t i=3;i<initialBlack.size();i+=4)initialBlack[i]=255;initialData.pSysMem=initialBlack.data();initialData.SysMemPitch=d.Width*4;}
    if(&b==&g_f150Bridge){d.Format=car_video_selected(false)?DXGI_FORMAT_B8G8R8A8_UNORM:DXGI_FORMAT_BC1_UNORM;d.Usage=D3D11_USAGE_DEFAULT;d.CPUAccessFlags=0;{std::lock_guard<std::mutex> lock(carMutex);initialBlack=f150First;}if(car_video_selected(false)){if(initialBlack.size()!=size_t(f150Width)*f150Height*4){initialBlack.assign(size_t(f150Width)*f150Height*4,5);for(size_t i=3;i<initialBlack.size();i+=4)initialBlack[i]=255;}initialData.pSysMem=initialBlack.data();initialData.SysMemPitch=f150Width*4;}else{if(initialBlack.size()!=f150FrameBytes)initialBlack.assign(f150FrameBytes,0);initialData.pSysMem=initialBlack.data();initialData.SysMemPitch=(f150Width/4)*8;}f150Uploaded=-1;}
    if(&b==&g_f150InfoBridge){d.Format=car_video_selected(true)?DXGI_FORMAT_B8G8R8A8_UNORM:DXGI_FORMAT_BC1_UNORM;d.Usage=D3D11_USAGE_DEFAULT;d.CPUAccessFlags=0;{std::lock_guard<std::mutex> lock(carMutex);initialBlack=f150InfoFirst;}if(car_video_selected(true)){if(initialBlack.size()!=size_t(f150InfoWidth)*f150InfoHeight*4){initialBlack.assign(size_t(f150InfoWidth)*f150InfoHeight*4,5);for(size_t i=3;i<initialBlack.size();i+=4)initialBlack[i]=255;}initialData.pSysMem=initialBlack.data();initialData.SysMemPitch=f150InfoWidth*4;}else{if(initialBlack.size()!=f150InfoFrameBytes)initialBlack.assign(f150InfoFrameBytes,0);initialData.pSysMem=initialBlack.data();initialData.SysMemPitch=(f150InfoWidth/4)*8;}f150InfoUploaded=-99;}
    HRESULT hr = g_origCreateTexture2D(device, &d, initialData.pSysMem?&initialData:nullptr, &b.liveTexture);
    if ((FAILED(hr) || !b.liveTexture)&& &b!=&g_f150Bridge&& &b!=&g_f150InfoBridge) {
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        hr = g_origCreateTexture2D(device, &d, initialData.pSysMem?&initialData:nullptr, &b.liveTexture);
    }
    if (FAILED(hr) || !b.liveTexture) {
        log(SCS_LOG_TYPE_error, "Could not create plugin-owned %s texture (hr=0x%08X)", b.name, hr);
        release_live_locked(b);
        return false;
    }

    b.liveFormat = d.Format;b.cleared=false;
    if(managed_bridge(&b)>=0){auto& image=g_managedPrepared[managed_bridge(&b)/3];std::lock_guard<std::mutex> lock(image.mutex);image.uploaded=0;}
    if(&b==&g_skinBridge||((b.height==740&&b.width>=364)||(b.height==500&&b.width>=804))){auto& image=g_prepared[prepared_index(b)];std::lock_guard<std::mutex> lock(image.mutex);image.uploaded=0;}
    hr = g_origCreateSRV(device, b.liveTexture, nullptr, &b.liveView);
    if (FAILED(hr) || !b.liveView) {
        log(SCS_LOG_TYPE_error, "Could not create %s SRV (hr=0x%08X)", b.name, hr);
        release_live_locked(b);
        return false;
    }

    if(managed_bridge(&b)>=0||&b==&g_f150SourceBridge||&b==&g_f150MapBridge||is_cluster_bridge(&b)||surface_bridge(&b)||b.height==500){
        // Alpha-tested front disappears; ATS keeps drawing its original plane behind it.
        D3D11_TEXTURE2D_DESC clear{};clear.Width=clear.Height=clear.MipLevels=clear.ArraySize=clear.SampleDesc.Count=1;clear.Format=DXGI_FORMAT_B8G8R8A8_UNORM;clear.Usage=D3D11_USAGE_IMMUTABLE;clear.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        const uint8_t pixel[]={0,0,0,0};D3D11_SUBRESOURCE_DATA data{pixel,4,0};ID3D11Texture2D* texture=nullptr;
        if(SUCCEEDED(g_origCreateTexture2D(device,&clear,&data,&texture))){g_origCreateSRV(device,texture,nullptr,&b.disabledView);texture->Release();}
        if(!b.disabledView){release_live_locked(b);return false;}
        if(b.markerView)b.markerView->SetPrivateDataInterface(kNativePhoneView,b.disabledView);
    }
    if(&b==&g_f150Bridge){
        // Exact waiting gray, plus explicit transparency for native handoff.
        D3D11_TEXTURE2D_DESC black{};black.Width=black.Height=black.MipLevels=black.ArraySize=black.SampleDesc.Count=1;black.Format=DXGI_FORMAT_B8G8R8A8_UNORM;black.Usage=D3D11_USAGE_IMMUTABLE;black.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        const uint8_t pixel[]={5,5,5,255};D3D11_SUBRESOURCE_DATA data{pixel,4,0};ID3D11Texture2D* texture=nullptr;
        if(SUCCEEDED(g_origCreateTexture2D(device,&black,&data,&texture))){g_origCreateSRV(device,texture,nullptr,&g_f150BlackView);texture->Release();}
        const uint8_t clear[]={0,0,0,0};data.pSysMem=clear;texture=nullptr;
        if(SUCCEEDED(g_origCreateTexture2D(device,&black,&data,&texture))){g_origCreateSRV(device,texture,nullptr,&g_f150ClearView);texture->Release();}
    }
    if(&b==&g_f150InfoBridge){
        {PhoneInternalScope isolated;g_f150InfoFade.start(device);}
        // Exact waiting gray, plus explicit transparency for native handoff.
        D3D11_TEXTURE2D_DESC black{};black.Width=black.Height=black.MipLevels=black.ArraySize=black.SampleDesc.Count=1;black.Format=DXGI_FORMAT_B8G8R8A8_UNORM;black.Usage=D3D11_USAGE_IMMUTABLE;black.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        const uint8_t pixel[]={5,5,5,255};D3D11_SUBRESOURCE_DATA data{pixel,4,0};ID3D11Texture2D* texture=nullptr;
        if(SUCCEEDED(g_origCreateTexture2D(device,&black,&data,&texture))){g_origCreateSRV(device,texture,nullptr,&g_f150InfoBlackView);texture->Release();}
        const uint8_t clear[]={0,0,0,0};data.pSysMem=clear;texture=nullptr;
        if(SUCCEEDED(g_origCreateTexture2D(device,&black,&data,&texture))){g_origCreateSRV(device,texture,nullptr,&g_f150InfoClearView);texture->Release();}
    }
    if(&b==&g_f150Bridge){PhoneInternalScope isolated;g_f150CenterFade.start(device,f150Width,f150Height);}
    if (&b == &g_distBridge) g_distDirty.store(true, std::memory_order_release);
    if (&b == &g_timeBridge) g_timeDirty.store(true, std::memory_order_release);
    if (&b == &g_clockBridge) g_clockDirty.store(true, std::memory_order_release);
    if (&b == &g_speedBridge) g_speedDirty.store(true, std::memory_order_release);
    if (&b == &g_guidanceBridge) g_guidanceDirty.store(true, std::memory_order_release);
    if (&b == &g_skinBridge) g_skinDirty.store(true, std::memory_order_release);
    if (&b == &g_mapBackgroundBridge) g_mapBackgroundDirty.store(true, std::memory_order_release);

    log(SCS_LOG_TYPE_message,
        "Created plugin-owned %s texture/SRV: %ux%u fmt=%u texture=%p srv=%p",
        b.name, b.width, b.height, static_cast<unsigned>(b.liveFormat), b.liveTexture, b.liveView);
    if(is_cluster_bridge(&b)&&b.markerView)b.markerView->SetPrivateDataInterface(kPausedPhoneView,cluster_bridge_guard_needed(&b)?cluster_intro_guard(device,startup_guard_tone(&b)):b.disabledView);
    return true;
}

static HRESULT STDMETHODCALLTYPE hook_CreateShaderResourceView(
    ID3D11Device* device,
    ID3D11Resource* resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* desc,
    ID3D11ShaderResourceView** out)
{
    const HRESULT hr = g_origCreateSRV(device, resource, desc, out);
    if (!renderStopping && SUCCEEDED(hr) && out && *out) {
        std::unique_lock<std::mutex> lock(g_d3dMutex,std::try_to_lock);if(!lock.owns_lock())return hr;
        auto visit=[&](OverlayBridge* b) {
            if (b->markerTexture && resource == static_cast<ID3D11Resource*>(b->markerTexture)) {
                if (b->markerView){b->markerView->SetPrivateDataInterface(kPausedPhoneView,nullptr);b->markerView->SetPrivateDataInterface(kNativePhoneView,nullptr);b->markerView->Release();}
                b->pausedView=nullptr;
                b->markerView = *out;
                b->markerView->AddRef();
                // The cluster marker may first be loaded before gameplay/SDK
                // initialization. Prepare only its opaque RGB10 guard now;
                // native UI loading is already using this valid D3D device.
                const int managed=managed_bridge(b);
                if(render_gpu_allowed()||is_cluster_bridge(b)||(managed>=0&&managed%3==0&&managed_visible_splash(managed_snapshot(unsigned(managed/3)))))create_live_resources_locked(*b, device);
                log(SCS_LOG_TYPE_message,
                    "%s marker SRV captured by COM resource identity: srv=%p resource=%p",
                    b->name, b->markerView, resource);
            }
        };
        // One registry prevents a new cluster/guard from missing the cold-SRV path.
        for(auto* bridge:all_bridges())visit(bridge);
    }
    return hr;
}

static void split_12h(int hour24, int& hour12, const char*& ampm) {
    hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    ampm = (hour24 < 12) ? "AM" : "PM";
}

static std::string current_clock_text() {
    if (!g_haveGameTime.load(std::memory_order_acquire)) return "--:-- --";

    int64_t totalMinutes = current_game_minutes();
    if (totalMinutes < 0) totalMinutes = 0;

    const int minuteOfDay = static_cast<int>(totalMinutes % (24 * 60));
    const int hour24 = minuteOfDay / 60;
    const int minute = minuteOfDay % 60;
    int hour12 = 12;
    const char* ampm = "AM";
    split_12h(hour24, hour12, ampm);

    char b[64]{};
    std::snprintf(b, sizeof(b), "%d:%02d", hour12, minute);
    return b;
}

static constexpr float kMpsToMph = 2.2369362920544f;

static int speed_mph_from_mps(float mps) {
    if (!std::isfinite(mps)) return 0;
    return static_cast<int>(std::lround(std::fabs(mps) * kMpsToMph));
}

static bool current_speed_is_over_limit() {
    if (!g_haveTruckSpeed.load(std::memory_order_acquire) ||
        !g_haveSpeedLimit.load(std::memory_order_acquire))
        return false;
    const float speed = std::fabs(g_truckSpeedMps.load(std::memory_order_acquire));
    const float limit = g_speedLimitMps.load(std::memory_order_acquire);
    return std::isfinite(speed) && std::isfinite(limit) && limit > 0.1f &&
           speed > (limit + 0.05f);
}

static std::string current_speed_text() {
    if (!g_haveTruckSpeed.load(std::memory_order_acquire)) return "--";
    const float mps = g_truckSpeedMps.load(std::memory_order_acquire);
    if (!std::isfinite(mps)) return "--";
    char b[16]{};
    std::snprintf(b, sizeof(b), "%d", speed_mph_from_mps(mps));
    return b;
}

static std::string current_speed_limit_text() {
    if (!g_haveSpeedLimit.load(std::memory_order_acquire)) return "-";
    const float mps = g_speedLimitMps.load(std::memory_order_acquire);
    if (!std::isfinite(mps) || mps <= 0.1f) return "-";
    char b[16]{};
    std::snprintf(b, sizeof(b), "%d", speed_mph_from_mps(mps));
    return b;
}

static std::string current_arrival_text(bool showDay=true) {
    if (!g_haveGameTime.load(std::memory_order_acquire) ||
        !g_haveNavTime.load(std::memory_order_acquire))
        return "--- --h--";

    const float seconds = g_navTimeSec.load(std::memory_order_acquire);
    if (!std::isfinite(seconds) || seconds < 0.0f)
        return "--- --h--";

    // Format ETA in the same local clock as the dashboard. Durations stay
    // independent of timezone and keep the multiplayer correction once.
    const int64_t remainingMinutes =
        static_cast<int64_t>(std::ceil(seconds / 60.0f));
    const int64_t gameMinutes = current_game_minutes();
    const int64_t multiplayerOffset = 0;

    int64_t arrivalMinutes = gameMinutes + multiplayerOffset + remainingMinutes;
    if (arrivalMinutes < 0) arrivalMinutes = 0;

    static const char* kDays[7] = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };
    const int64_t day = (arrivalMinutes / (24 * 60)) % 7;
    const int minuteOfDay = static_cast<int>(arrivalMinutes % (24 * 60));
    const int hour24 = minuteOfDay / 60;
    const int minute = minuteOfDay % 60;
    int hour12 = 12;
    const char* ampm = "AM";
    split_12h(hour24, hour12, ampm);

    char b[64]{};
    const int64_t currentDay=std::max<int64_t>(0,gameMinutes+multiplayerOffset)/1440;
    if(showDay||arrivalMinutes/1440!=currentDay)std::snprintf(b,sizeof(b),"%s %d:%02d",kDays[static_cast<size_t>(day)],hour12,minute);
    else std::snprintf(b,sizeof(b),"%d:%02d",hour12,minute);
    return b;
}

static std::string current_distance_text() {
    std::string distance = "--.- mi";
    if (g_haveNavDistance.load(std::memory_order_acquire)) {
        const float m = g_navDistanceM.load(std::memory_order_acquire);
        if (std::isfinite(m) && m >= 0.0f) {
            const float mi = m / 1609.344f;
            char b[64]{};
            if (mi >= 100.0f)
                std::snprintf(b, sizeof(b), "%.0f mi", mi);
            else
                std::snprintf(b, sizeof(b), "%.1f mi", mi);
            distance = b;
        }
    }

    distance += " - ";
    distance += current_arrival_text();
    return distance;
}

static std::string current_time_text() {
    if (!g_haveNavTime.load(std::memory_order_acquire)) return "--h --min";
    const float seconds = g_navTimeSec.load(std::memory_order_acquire);
    if (!std::isfinite(seconds) || seconds < 0.0f) return "--h --min";

    // The SDK channel is navigation ETA in seconds.  Round up to the next
    // whole minute so the display behaves like a remaining-time estimate.
    const int totalMinutes = static_cast<int>(std::ceil(seconds / 60.0f));
    const int hours = totalMinutes / 60;
    const int minutes = totalMinutes % 60;
    char b[64]{};
    std::snprintf(b, sizeof(b), "%dh %02dmin", hours, minutes);
    return b;
}

static void render_text_bgra(
    std::vector<uint8_t>& pixels,
    UINT width,
    UINT height,
    const std::string& text,
    int fontHeight)
{
    pixels.assign(static_cast<size_t>(width) * height * 4, 255);

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = static_cast<LONG>(width);
    bi.bmiHeader.biHeight = -static_cast<LONG>(height);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(dc);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    RECT r{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(dc, &r, white);
    DeleteObject(white);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(75, 75, 75));
    HFONT font = CreateFontA(
        fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HGDIOBJ oldFont = SelectObject(dc, font);
    DrawTextA(dc, text.c_str(), -1, &r,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    std::memcpy(pixels.data(), bits, pixels.size());
    for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;

    SelectObject(dc, oldFont);
    DeleteObject(font);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}


static int measure_time_left_x(HDC dc)
{
    // NavTime is 400px wide and is rendered with a -42 base font at 150%.
    const int timeFontHeight = -63;
    HFONT numberFont = CreateFontA(
        timeFontHeight, 0, 0, 0, FW_BLACK, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT unitFont = CreateFontA(
        timeFontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");

    const float seconds = g_navTimeSec.load(std::memory_order_acquire);
    const bool valid = g_haveNavTime.load(std::memory_order_acquire) &&
                       std::isfinite(seconds) && seconds >= 0.0f;
    int hours = 0;
    int minutes = 0;
    if (valid) {
        const int totalMinutes = static_cast<int>(std::ceil(seconds / 60.0f));
        hours = totalMinutes / 60;
        minutes = totalMinutes % 60;
    }

    char hourBuf[16]{};
    char minuteBuf[16]{};
    if (valid) {
        std::snprintf(hourBuf, sizeof(hourBuf), "%d", hours);
        std::snprintf(minuteBuf, sizeof(minuteBuf), "%02d", minutes);
    } else {
        std::snprintf(hourBuf, sizeof(hourBuf), "--");
        std::snprintf(minuteBuf, sizeof(minuteBuf), "--");
    }

    struct Run { const char* text; HFONT font; };
    Run runs[] = {
        {hourBuf, numberFont}, {"h", unitFont}, {" ", unitFont},
        {minuteBuf, numberFont}, {"min", unitFont},
    };

    int totalWidth = 0;
    for (const auto& run : runs) {
        SIZE sz{};
        HGDIOBJ oldFont = SelectObject(dc, run.font);
        GetTextExtentPoint32A(dc, run.text, static_cast<int>(std::strlen(run.text)), &sz);
        SelectObject(dc, oldFont);
        totalWidth += sz.cx;
    }

    DeleteObject(numberFont);
    DeleteObject(unitFont);
    return (400 - totalWidth) / 2;
}

static void render_distance_text_bgra(
    std::vector<uint8_t>& pixels,
    UINT width,
    UINT height,
    const std::string& text,
    int fontHeight)
{
    pixels.assign(static_cast<size_t>(width) * height * 4, 0);

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = static_cast<LONG>(width);
    bi.bmiHeader.biHeight = -static_cast<LONG>(height);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(dc);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    RECT r{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &r, black);
    DeleteObject(black);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));

    // 25% larger than v4: 62.5% of the original distance font size.
    int distanceFontHeight = static_cast<int>(std::lround(static_cast<double>(fontHeight) * 0.625));
    if (distanceFontHeight == 0)
        distanceFontHeight = (fontHeight < 0) ? -1 : 1;

    HFONT font = CreateFontA(
        distanceFontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");

    SIZE size{};
    HGDIOBJ oldFont = SelectObject(dc, font);
    GetTextExtentPoint32A(dc, text.c_str(), static_cast<int>(text.size()), &size);

    // NavDist and NavTime are both displayed at ~0.5 UI units per texture pixel.
    // Using NavTime's dynamic pixel-left therefore gives the same visible left edge.
    int x = measure_time_left_x(dc);
    if (x < 0) x = 0;
    if (x + size.cx > static_cast<int>(width))
        x = std::max(0, static_cast<int>(width) - static_cast<int>(size.cx));

    // Center inside NavDist; the UI asset itself is positioned directly below ETA.
    const int y = (static_cast<int>(height) - static_cast<int>(size.cy)) / 2;
    TextOutA(dc, x, y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(dc, oldFont);

    std::memcpy(pixels.data(), bits, pixels.size());
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        uint8_t coverage = pixels[i + 0];
        if (pixels[i + 1] > coverage) coverage = pixels[i + 1];
        if (pixels[i + 2] > coverage) coverage = pixels[i + 2];
        // Keep the original distance colour (#4b4b4b / dark grey).
        pixels[i + 0] = presentation_dark()?0xb4:0x4b;
        pixels[i + 1] = presentation_dark()?0xb4:0x4b;
        pixels[i + 2] = presentation_dark()?0xb4:0x4b;
        pixels[i + 3] = coverage;
    }

    DeleteObject(font);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

static void render_time_text_bgra(
    std::vector<uint8_t>& pixels,
    UINT width,
    UINT height,
    int fontHeight)
{
    pixels.assign(static_cast<size_t>(width) * height * 4, 0);

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = static_cast<LONG>(width);
    bi.bmiHeader.biHeight = -static_cast<LONG>(height);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(dc);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    RECT r{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &r, black);
    DeleteObject(black);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    const COLORREF targetColor = RGB(43, 127, 63); // #2b7f3f

    int styledFontHeight = static_cast<int>(std::lround(static_cast<double>(fontHeight) * 1.5));
    if (styledFontHeight == 0)
        styledFontHeight = (fontHeight < 0) ? -1 : 1;

    HFONT numberFont = CreateFontA(
        styledFontHeight, 0, 0, 0, FW_BLACK, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT unitFont = CreateFontA(
        styledFontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");

    const float seconds = g_navTimeSec.load(std::memory_order_acquire);
    const bool valid = g_haveNavTime.load(std::memory_order_acquire) &&
                       std::isfinite(seconds) && seconds >= 0.0f;

    int hours = 0;
    int minutes = 0;
    if (valid) {
        const int totalMinutes = static_cast<int>(std::ceil(seconds / 60.0f));
        hours = totalMinutes / 60;
        minutes = totalMinutes % 60;
    }

    char hourBuf[16]{};
    char minuteBuf[16]{};
    if (valid) {
        std::snprintf(hourBuf, sizeof(hourBuf), "%d", hours);
        std::snprintf(minuteBuf, sizeof(minuteBuf), "%02d", minutes);
    } else {
        std::snprintf(hourBuf, sizeof(hourBuf), "--");
        std::snprintf(minuteBuf, sizeof(minuteBuf), "--");
    }

    struct Run {
        const char* text;
        HFONT font;
        SIZE size;
    };

    Run runs[] = {
        {hourBuf, numberFont, {}},
        {"h", unitFont, {}},
        {" ", unitFont, {}},
        {minuteBuf, numberFont, {}},
        {"min", unitFont, {}},
    };

    int totalWidth = 0;
    int maxHeight = 0;
    for (auto& run : runs) {
        HGDIOBJ oldFont = SelectObject(dc, run.font);
        GetTextExtentPoint32A(dc, run.text, static_cast<int>(std::strlen(run.text)), &run.size);
        SelectObject(dc, oldFont);
        totalWidth += run.size.cx;
        if (run.size.cy > maxHeight) maxHeight = run.size.cy;
    }

    int x = (static_cast<int>(width) - totalWidth) / 2;
    const int y = (static_cast<int>(height) - maxHeight) / 2;
    for (const auto& run : runs) {
        HGDIOBJ oldFont = SelectObject(dc, run.font);
        TextOutA(dc, x, y, run.text, static_cast<int>(std::strlen(run.text)));
        SelectObject(dc, oldFont);
        x += run.size.cx;
    }

    std::memcpy(pixels.data(), bits, pixels.size());
    const uint8_t targetB = GetBValue(targetColor);
    const uint8_t targetG = GetGValue(targetColor);
    const uint8_t targetR = GetRValue(targetColor);
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        uint8_t coverage = pixels[i + 0];
        if (pixels[i + 1] > coverage) coverage = pixels[i + 1];
        if (pixels[i + 2] > coverage) coverage = pixels[i + 2];
        pixels[i + 0] = targetB;
        pixels[i + 1] = targetG;
        pixels[i + 2] = targetR;
        pixels[i + 3] = coverage;
    }

    DeleteObject(numberFont);
    DeleteObject(unitFont);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

static void render_clock_text_bgra(
    std::vector<uint8_t>& pixels,
    UINT width,
    UINT height,
    const std::string& text)
{
    pixels.assign(static_cast<size_t>(width) * height * 4, 0);

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = static_cast<LONG>(width);
    bi.bmiHeader.biHeight = -static_cast<LONG>(height);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(dc);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    RECT r{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &r, black);
    DeleteObject(black);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));

    // v10: +25% versus the previous -31px clock font.
    // Keep the numeric time clearly heavier than the AM/PM suffix.
    constexpr int kClockFontHeight = -39;
    HFONT timeFont = CreateFontA(
        kClockFontHeight, 0, 0, 0, FW_BLACK, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT suffixFont = CreateFontA(
        kClockFontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");

    std::string timePart = text;
    std::string suffixPart;
    const size_t split = text.rfind(' ');
    if (split != std::string::npos) {
        timePart = text.substr(0, split);
        suffixPart = text.substr(split + 1);
    }

    SIZE timeSize{};
    SIZE spaceSize{};
    SIZE suffixSize{};

    HGDIOBJ oldFont = SelectObject(dc, timeFont);
    GetTextExtentPoint32A(dc, timePart.c_str(), static_cast<int>(timePart.size()), &timeSize);
    SelectObject(dc, oldFont);

    oldFont = SelectObject(dc, suffixFont);
    GetTextExtentPoint32A(dc, " ", 1, &spaceSize);
    if (!suffixPart.empty())
        GetTextExtentPoint32A(dc, suffixPart.c_str(), static_cast<int>(suffixPart.size()), &suffixSize);
    SelectObject(dc, oldFont);

    const int totalWidth = timeSize.cx + (suffixPart.empty() ? 0 : spaceSize.cx + suffixSize.cx);
    const int maxHeight = std::max(static_cast<int>(timeSize.cy), static_cast<int>(suffixSize.cy));
    int x = (static_cast<int>(width) - totalWidth) / 2;
    const int y = (static_cast<int>(height) - maxHeight) / 2;

    oldFont = SelectObject(dc, timeFont);
    TextOutA(dc, x, y, timePart.c_str(), static_cast<int>(timePart.size()));
    SelectObject(dc, oldFont);
    x += timeSize.cx;

    if (!suffixPart.empty()) {
        x += spaceSize.cx;
        oldFont = SelectObject(dc, suffixFont);
        TextOutA(dc, x, y, suffixPart.c_str(), static_cast<int>(suffixPart.size()));
        SelectObject(dc, oldFont);
    }

    std::memcpy(pixels.data(), bits, pixels.size());
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        uint8_t coverage = pixels[i + 0];
        if (pixels[i + 1] > coverage) coverage = pixels[i + 1];
        if (pixels[i + 2] > coverage) coverage = pixels[i + 2];
        // Original phone status-clock colour #636161.
        pixels[i + 0] = 0x61;
        pixels[i + 1] = 0x61;
        pixels[i + 2] = 0x63;
        pixels[i + 3] = coverage;
    }

    DeleteObject(timeFont);
    DeleteObject(suffixFont);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

static void render_speed_text_bgra(
    std::vector<uint8_t>& pixels,
    UINT width,
    UINT height,
    const std::string& actualSpeedText)
{
    pixels.assign(static_cast<size_t>(width) * height * 4, 0);

    auto blend_pixel = [&](UINT x, UINT y,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if(presentation_dark()&&r==g&&g==b){r=255-r;g=255-g;b=255-b;}
        if (x >= width || y >= height || a == 0) return;
        const size_t i = (static_cast<size_t>(y) * width + x) * 4;
        const int dstA = pixels[i + 3];
        const int invA = 255 - static_cast<int>(a);
        const int outA = static_cast<int>(a) + (dstA * invA + 127) / 255;
        if (outA <= 0) return;

        const int dstBPremul = static_cast<int>(pixels[i + 0]) * dstA;
        const int dstGPremul = static_cast<int>(pixels[i + 1]) * dstA;
        const int dstRPremul = static_cast<int>(pixels[i + 2]) * dstA;
        const int outBPremul = static_cast<int>(b) * a + (dstBPremul * invA + 127) / 255;
        const int outGPremul = static_cast<int>(g) * a + (dstGPremul * invA + 127) / 255;
        const int outRPremul = static_cast<int>(r) * a + (dstRPremul * invA + 127) / 255;

        pixels[i + 0] = static_cast<uint8_t>((outBPremul + outA / 2) / outA);
        pixels[i + 1] = static_cast<uint8_t>((outGPremul + outA / 2) / outA);
        pixels[i + 2] = static_cast<uint8_t>((outRPremul + outA / 2) / outA);
        pixels[i + 3] = static_cast<uint8_t>(outA);
    };

    // Supersampled rounded rectangles keep corners smooth at phone scale.
    auto round_coverage=[](double x,double y,int l,int t,int r,int b,double radius){
        const double cx=(l+r)*.5,cy=(t+b)*.5;
        const double qx=std::abs(x-cx)-(r-l)*.5+radius,qy=std::abs(y-cy)-(b-t)*.5+radius;
        return std::hypot(std::max(qx,0.0),std::max(qy,0.0))+std::min(std::max(qx,qy),0.0)<=radius;
    };
    auto rounded_rect=[&](int l,int t,int r,int b,int radius,int border,
                          uint8_t cr,uint8_t cg,uint8_t cb,uint8_t ca){
        for(int y=std::max(0,t);y<std::min(int(height),b);++y)for(int x=std::max(0,l);x<std::min(int(width),r);++x){
            unsigned covered=0;
            for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx){double px=x+(sx+.5)/4,py=y+(sy+.5)/4;
                bool outer=round_coverage(px,py,l,t,r,b,radius);
                bool inner=border>0&&round_coverage(px,py,l+border,t+border,r-border,b-border,std::max(0,radius-border));
                if(outer&&!inner)++covered;
            }
            blend_pixel(x,y,cr,cg,cb,uint8_t((ca*covered+8)/16));
        }
    };
    const int topPadding=height>336?16:0;
    const int signOffset=(height>320?8:0)+topPadding;
    const int signL=16,signT=154+signOffset,signR=176,signB=306+signOffset;
    static std::map<uint64_t,std::vector<uint8_t>> cardBackgrounds;
    auto cardKey=(uint64_t(width)<<32)|(uint64_t(height)<<1)|(presentation_dark()?1:0);
    auto foundBackground=cardBackgrounds.find(cardKey);
    if(foundBackground!=cardBackgrounds.end())pixels=foundBackground->second;
    else {
    // No outer outline; one rounded border on the speed-limit sign.
    rounded_rect(0,0,int(width),int(height),22,0,255,255,255,205);
    rounded_rect(signL,signT,signR,signB,16,0,255,255,255,238);
    rounded_rect(signL,signT,signR,signB,16,6,0,0,0,255);
    cardBackgrounds.emplace(cardKey,pixels);
    }

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = static_cast<LONG>(width);
    bi.bmiHeader.biHeight = -static_cast<LONG>(height);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(dc);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    SetBkMode(dc, TRANSPARENT);

    auto draw_text_mask = [&](const std::string& value, RECT rect,
                              int fontHeight, int fontWeight,
                              uint8_t cr, uint8_t cg, uint8_t cb, int inkWidth=0) {
        RECT full{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
        HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(dc, &full, black);
        DeleteObject(black);
        SetTextColor(dc, RGB(255, 255, 255));

        HFONT font = CreateFontA(
            fontHeight, 0, 0, 0, fontWeight, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
        HGDIOBJ oldFont = SelectObject(dc, font);
        DrawTextA(dc, value.c_str(), -1, &rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, oldFont);
        DeleteObject(font);

        const uint8_t* src = static_cast<const uint8_t*>(bits);
        if(inkWidth>0){
            int minX=int(width),maxX=-1;
            for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x)if(src[(y*width+x)*4]){minX=std::min(minX,int(x));maxX=std::max(maxX,int(x));}
            if(maxX>=minX){
                int left=(rect.left+rect.right-inkWidth)/2;double span=maxX-minX+1;
                for(UINT y=0;y<height;++y)for(int x=0;x<inkWidth;++x){
                    double u=minX+(x+.5)*span/inkWidth-.5;int i=int(std::floor(u));double f=u-i;
                    auto sample=[&](int px){return px>=minX&&px<=maxX?src[(y*width+px)*4]:0;};
                    auto coverage=uint8_t(std::lround(sample(i)*(1-f)+sample(i+1)*f));
                    if(left+x>=0)blend_pixel(left+x,y,cr,cg,cb,coverage);
                }
            }
            return;
        }
        for (UINT y = 0; y < height; ++y) {
            for (UINT x = 0; x < width; ++x) {
                const size_t i = (static_cast<size_t>(y) * width + x) * 4;
                uint8_t coverage = src[i + 0];
                if (src[i + 1] > coverage) coverage = src[i + 1];
                if (src[i + 2] > coverage) coverage = src[i + 2];
                if (coverage)
                    blend_pixel(x, y, cr, cg, cb, coverage);
            }
        }
    };

    const bool over = current_speed_is_over_limit();
    const uint8_t actualR = over ? 0xd3 : 0x00;
    const uint8_t actualG = over ? 0x2f : 0x00;
    const uint8_t actualB = over ? 0x2f : 0x00;

    RECT actualRect{8, -8+topPadding, static_cast<LONG>(width) - 8, 104+topPadding};
    draw_text_mask(actualSpeedText, actualRect, -104, FW_BLACK,
                   actualR, actualG, actualB);

    // Use the font's natural proportions; keep the smaller unit close to the speed.
    RECT unitRect{8,94+topPadding,static_cast<LONG>(width)-8,136+topPadding};
    draw_text_mask("mph",unitRect,-36,FW_NORMAL,0,0,0);

    RECT labelRect{24, 166+signOffset, static_cast<LONG>(width) - 24, 218+signOffset};
    draw_text_mask("LIMIT", labelRect, -34, FW_BLACK, 0, 0, 0);

    const std::string limitText = current_speed_limit_text();
    RECT limitRect{22, 207+signOffset, static_cast<LONG>(width) - 22, 294+signOffset};
    draw_text_mask(limitText, limitRect, -78, FW_BLACK, 0, 0, 0);

    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}


static std::wstring wide_text(const std::string& text) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if(n<=0)return L"";std::wstring out(n,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),out.data(),n);return out;
}
static std::mutex g_fontMutex;
static HANDLE g_phoneFonts[5]{};
static bool load_phone_fonts() {
    std::lock_guard<std::mutex> lock(g_fontMutex);
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&load_phone_fonts),&module))return false;
    for(int i=0;i<5;++i)if(!g_phoneFonts[i]) {
        const auto resource=FindResourceW(module,MAKEINTRESOURCEW(240+i),RT_RCDATA);
        if(!resource)continue;const auto loaded=LoadResource(module,resource);const auto bytes=SizeofResource(module,resource);DWORD count=0;
        if(loaded&&bytes)g_phoneFonts[i]=AddFontMemResourceEx(LockResource(loaded),bytes,nullptr,&count);
    }
    return g_phoneFonts[0]&&g_phoneFonts[1]&&g_phoneFonts[2]&&g_phoneFonts[3]&&g_phoneFonts[4];
}
static void release_phone_fonts() {
    std::lock_guard<std::mutex> lock(g_fontMutex);
    for(auto& font:g_phoneFonts)if(font){RemoveFontMemResourceEx(font);font=nullptr;}
}
static void render_guidance_text_bgra(std::vector<uint8_t>& pixels,UINT width,UINT height,const bettergps_navigation::Display& state,const std::wstring& shieldDirectory=bettergps_navigation::ShieldDirectory()) {
    pixels.assign(static_cast<size_t>(width)*height*4,0);
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-static_cast<LONG>(height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);if(!bitmap||!bits){if(bitmap)DeleteObject(bitmap);DeleteDC(dc);return;}
    auto old=SelectObject(dc,bitmap);std::memset(bits,0,pixels.size());
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    const auto presentation=guidance_presentation(state);
    const auto family=load_phone_fonts()?L"Google Sans":L"Segoe UI";
    auto draw=[&](const std::string& text,RECT rect,int size,int minimum,int weight,bool wrap,bool center=false) {
        const auto wide=wide_text(text);if(wide.empty())return;
        const int available=rect.right-rect.left;
        for(int n=size;n>=minimum;n-=2) {
            HFONT font=CreateFontW(-static_cast<int>(n*1.5),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,family);auto oldFont=SelectObject(dc,font);
            std::vector<std::wstring> lines;
            if(!wrap)lines.push_back(wide);
            else for(size_t start=0;start<wide.size();) {
                int fit=0;SIZE extent{};GetTextExtentExPointW(dc,wide.data()+start,static_cast<int>(wide.size()-start),available,&fit,nullptr,&extent);
                size_t length=std::max(1,fit);length=std::min(length,wide.size()-start);
                if(start+length<wide.size()){const auto space=wide.rfind(L' ',start+length);if(space!=std::wstring::npos&&space>start)length=space-start;}
                lines.push_back(wide.substr(start,length));start+=length;while(start<wide.size()&&wide[start]==L' ')++start;
            }
            if(n<=minimum){
                const size_t maxLines=std::min(wrap?2:1,std::max<int>(1,(rect.bottom-rect.top+5)/(static_cast<int>(n*1.25)+5)));
                if(lines.size()>maxLines){lines.resize(maxLines);lines.back()+=L"\x2026";}
                for(auto& line:lines){SIZE extent{};GetTextExtentPoint32W(dc,line.data(),static_cast<int>(line.size()),&extent);
                    if(extent.cx>available){SIZE dots{};GetTextExtentPoint32W(dc,L"\x2026",1,&dots);int fit=0;GetTextExtentExPointW(dc,line.data(),static_cast<int>(line.size()),std::max<int>(1,available-dots.cx),&fit,nullptr,&extent);line.resize(std::max(0,fit));while(!line.empty()&&line.back()==' ')line.pop_back();if(!line.empty()&&line.back()>=0xd800&&line.back()<=0xdbff)line.pop_back();line+=L"\x2026";}
                }
            }
            struct Ink {int width,height;std::vector<uint8_t> alpha;};std::vector<Ink> inks;int totalHeight=0;bool fits=true;
            for(const auto& line:lines) {
                SIZE extent{};GetTextExtentPoint32W(dc,line.data(),static_cast<int>(line.size()),&extent);if(extent.cx>available)fits=false;
                std::memset(bits,0,pixels.size());RECT scratch{0,0,static_cast<LONG>(width),static_cast<LONG>(height)};
                DrawTextW(dc,line.c_str(),-1,&scratch,DT_NOPREFIX|DT_SINGLELINE);GdiFlush();
                auto* mask=static_cast<const uint8_t*>(bits);int left=width,top=height,right=-1,bottom=-1;
                for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x)if(mask[(static_cast<size_t>(y)*width+x)*4+1]){left=std::min(left,static_cast<int>(x));right=std::max(right,static_cast<int>(x));top=std::min(top,static_cast<int>(y));bottom=std::max(bottom,static_cast<int>(y));}
                if(right<0)continue;Ink ink{right-left+1,bottom-top+1,{}};ink.alpha.resize(ink.width*ink.height);
                for(int y=0;y<ink.height;++y)for(int x=0;x<ink.width;++x)ink.alpha[y*ink.width+x]=mask[(static_cast<size_t>(top+y)*width+left+x)*4+1];
                totalHeight+=ink.height;inks.push_back(std::move(ink));
            }
            const int gap=std::max(5,n/8);if(!inks.empty())totalHeight+=gap*static_cast<int>(inks.size()-1);
            fits=fits&&totalHeight<=rect.bottom-rect.top;
            if(fits||n<=minimum) {
                int top=rect.top+(rect.bottom-rect.top-totalHeight)/2;
                for(const auto& ink:inks){const int left=rect.left+(center?(available-ink.width)/2:0);for(int y=0;y<ink.height;++y)for(int x=0;x<ink.width;++x){const int px=left+x,py=top+y;if(px>=rect.right||py<rect.top||py>=rect.bottom||px<0||py<0||px>=static_cast<int>(width)||py>=static_cast<int>(height))continue;
                    const auto alpha=ink.alpha[y*ink.width+x];if(!alpha)continue;const size_t i=(static_cast<size_t>(py)*width+px)*4;pixels[i]=pixels[i+1]=pixels[i+2]=255;pixels[i+3]=std::max(pixels[i+3],alpha);
                }top+=ink.height+gap;}
                SelectObject(dc,oldFont);DeleteObject(font);break;
            }
            SelectObject(dc,oldFont);DeleteObject(font);
        }
    };
    if(!presentation.message.empty()) {
        draw(presentation.message,{0,0,780,285},60,54,FW_NORMAL,true,true);
        SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return;
    }
    // Coordinates match the navigation banner module and its smaller Then tab.
    // Reserve its microphone area; the rest of the texture stays transparent.
    // Center the two-line text block vertically in the main banner. Long names
    // and sign details use the extra line space rather than moving into the tab.
    const bool compact=presentation.detail.empty()&&presentation.road.size()<30;
    auto labels=road_label_parts(presentation.road);bool anyShield=false;
    for(auto& label:labels){if(label.numbered&&load_route_shield(label.shield.identity,shieldDirectory).bgra.empty())label.numbered=false;anyShield=anyShield||label.numbered;}
    const bool singleHeader=compact&&labels.size()<2;
    draw(presentation.distance,{200,singleHeader?20:5,780,singleHeader?130:115},82,54,FW_MEDIUM,false);
    const bool supplementary=!compact&&presentation.road.size()<24&&labels.size()<2&&!presentation.detail.empty();
    if(!anyShield)draw(presentation.road,{200,compact?140:125,780,compact?285:supplementary?225:295},60,44,FW_NORMAL,true);
    else {
        auto measure=[&](const std::string& text,int n){auto font=CreateFontW(-static_cast<int>(n*1.5),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,family);auto previous=SelectObject(dc,font);const auto wide=wide_text(text);SIZE extent{};GetTextExtentPoint32W(dc,wide.data(),static_cast<int>(wide.size()),&extent);SelectObject(dc,previous);DeleteObject(font);return extent.cx;};
        int font=52,total=0;for(;font>44;font-=2){total=0;for(const auto& label:labels)total+=(label.numbered?116:0)+measure(label.numbered?label.shield.beside:label.text,font)+24;if(total<=580)break;}
        total=0;for(const auto& label:labels)total+=(label.numbered?116:0)+measure(label.numbered?label.shield.beside:label.text,font)+24;
        const bool twoRows=total>580,largePair=twoRows&&labels.size()==2;
        if(largePair)font=52;
        const int rowHeight=largePair?90:twoRows?78:supplementary?100:140,top=largePair?115:compact?135:125,shieldSize=largePair?90:twoRows?74:104;
        int x=200,row=0;
        for(size_t i=0;i<labels.size();++i){const auto& label=labels[i];const auto content=label.numbered?label.shield.beside:label.text;int needed=(label.numbered?shieldSize+12:0)+measure(content,font);
            if(largePair&&i==1){row=1;x=200;}
            if(x>200&&x+needed>780){++row;x=200;}if(row>1)break;
            const int y=top+row*rowHeight;
            if(label.numbered){paint_route_shield(pixels,width,height,load_route_shield(label.shield.identity,shieldDirectory),x,y+(rowHeight-shieldSize)/2,shieldSize);x+=shieldSize+12;}
            const int right=std::min(780,x+std::max(needed-(label.numbered?shieldSize+12:0),24));draw(content,{x,y,right,y+rowHeight},font,44,FW_NORMAL,false);x=right+24;
        }
    }
    // Supplementary sign text never forces the primary road into tiny type.
    if(supplementary)draw(presentation.detail,{200,235,780,295},42,40,FW_NORMAL,false);
    if(!presentation.next.empty())draw("Then",{0,346,125,410},48,32,FW_NORMAL,false);
    // Each text block is centered using its actual visible ink bounds, not its
    // font line box. GDI grayscale coverage supplies the transparent alpha.
    auto icon=[&](ManeuverIcon image,int left,int top,int size) {
        if(!image.alpha)return;
        int minX=image.size,minY=image.size,maxX=-1,maxY=-1;
        for(int y=0;y<image.size;++y)for(int x=0;x<image.size;++x)if(image.alpha[y*image.size+x]){minX=std::min(minX,x);minY=std::min(minY,y);maxX=std::max(maxX,x);maxY=std::max(maxY,y);}
        if(maxX<0)return;
        // Center the visible silhouette, including asymmetrical source padding.
        left+=(image.size-minX-maxX-1)*size/(2*image.size);
        top+=(image.size-minY-maxY-1)*size/(2*image.size);
        for(int y=0;y<size;++y)for(int x=0;x<size;++x){int px=left+x,py=top+y;if(px<0||py<0||px>=static_cast<int>(width)||py>=static_cast<int>(height))continue;
            const auto alpha=image.alpha[(y*image.size/size)*image.size+x*image.size/size];size_t i=(static_cast<size_t>(py)*width+px)*4;
            if(alpha){pixels[i]=pixels[i+1]=pixels[i+2]=255;pixels[i+3]=std::max(pixels[i+3],alpha);}
        }
    };
    icon(maneuver_icon(presentation.action),0,55,175);
    icon(maneuver_icon(presentation.next),126,336,85);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
}

#include "WazePresentation.h"

static void render_configured_phone(std::vector<uint8_t>& pixels,const OverlaySettings* profile=nullptr,size_t profileTag=0){
    const auto config=profile?*profile:overlay_snapshot();bool trip=g_tripPresent.load(),dark=presentation_dark();
    struct BaseCache {uint64_t revision=~uint64_t(0);int state=-1;std::vector<uint8_t> pixels;};static std::array<BaseCache,34+managedDeviceCount> bases;auto& base=bases[std::min(profileTag,bases.size()-1)];
    int state=(dark?2:0)+(trip?0:1)+(waze_style()?4:0)+(navigationVoiceMuted?8:0);if(base.revision!=config.revision||base.state!=state){render_phone_layout(base.pixels,360,740,trip,dark,&config);base.revision=config.revision;base.state=state;}pixels=base.pixels;
    auto attach=[&](size_t item,Placement base,const std::vector<uint8_t>& layer,int w,int h){overlay_blit(pixels,layer,w,h,attached_placement(config,item,base));};
    static std::vector<uint8_t> clock,speed,distance,time,guidance;
    static std::string clockKey,speedKey,distanceKey,timeKey,guidanceKey;
    const std::string theme=dark?"D":"L";
    const bool whiteClock=dark||(waze_style()&&trip);auto clockText=current_clock_text();auto ck=clockText+(whiteClock?"W":"B");if(clockKey!=ck||clock.empty()){render_clock_text_bgra(clock,192,64,clockText);if(whiteClock)for(size_t i=0;i<clock.size();i+=4){clock[i]=255-clock[i];clock[i+1]=255-clock[i+1];clock[i+2]=255-clock[i+2];}clockKey=ck;}
    attach(waze_style()&&trip?1:dark?(trip?1:2):0,{14,2,80,26},clock,192,64);
    if(waze_style()){render_waze_contents(pixels,config,trip,dark);return;}
    auto speedText=current_speed_text();auto sk=speedText+current_speed_limit_text()+theme+(current_speed_is_over_limit()?"O":"N");if(speedKey!=sk||speed.empty()){render_speed_text_bgra(speed,192,352,speedText);speedKey=sk;}attach(overlayModuleCount,{18,514,72,132},speed,192,352);
    if(trip){auto display=bettergps_navigation::Current();auto gk=display.instruction+"|"+display.text+"|"+display.status+"|"+display.currentRoad+"|"+display.targetRoad+"|"+display.destination+"|"+display.next+"|"+display.exit+"|"+std::to_string(shield_revision(bettergps_navigation::ShieldDirectory()));if(guidanceKey!=gk||guidance.empty()){render_guidance_text_bgra(guidance,960,480,display);guidanceKey=gk;}attach(3,{20,40,320,160},guidance,960,480);
        size_t footer=dark?7:6;auto distanceText=current_distance_text();auto dk=distanceText+theme;if(distanceKey!=dk||distance.empty()){render_distance_text_bgra(distance,500,73,distanceText,-48);distanceKey=dk;}attach(footer,{0,686,251,36},distance,500,73);
        auto tk=current_time_text();if(timeKey!=tk||time.empty()){render_time_text_bgra(time,400,61,-42);timeKey=tk;}attach(footer,{0,663,200,30},time,400,61);}
    if(g_haveCompassHeading.load()){static std::vector<uint8_t> needle;render_compass_needle(needle,g_compassHeading.load());size_t buttons=trip?(dark?5:4):(dark?11:10);int cx=trip?320:324,cy=trip?407:592;attach(buttons,{cx-18,cy-18,36,36},needle,36,36);}
}

#include "HorizontalPresentation.h"
#include "GarminPresentation.h"
#include "NavplusPresentation.h"
#include "HomePresentation.h"
#include "CopilotPresentation.h"
#include "TomtomPresentation.h"
#include "ZonarData.h"
#include "CopilotRouteAction.h"
#include "ZonarPresentation.h"
#include "InternationalPresentation.h"
#include "CascadiaPresentation.h"
#include "SygicPresentation.h"
#include "Vnl2014Presentation.h"

// Black and the opaque lockscreen cover every GPS pixel. Do not prepare hidden
// text, shields and compass during those frames; keep the live host polling.
static void render_prepared_phone(std::vector<uint8_t>& canvas,const OverlaySettings& profile,size_t profileTag,int startup){
    const auto& images=startup_images();
    const bool covered=startup==-2||(startup>=0&&startup<=startupBlackMs+400+phoneStartupHoldMs.load()&&!images[0].empty()&&!images[1].empty());
    if(covered)canvas.assign(360*740*4,0);else render_configured_phone(canvas,&profile,profileTag);
    if(startup>=startupBlackMs&&startup<startup_total_ms())load_phone_fonts();
    render_phone_startup(canvas,startup,current_game_minutes(),presentation_dark());
}

static void update_bridge_on_render_thread(
    ID3D11DeviceContext* context,
    OverlayBridge& b,
    std::atomic<bool>& dirty,
    const std::string& text,
    int fontHeight)
{
    if (!context || !dirty.exchange(false, std::memory_order_acq_rel)) return;
    bettergps_diag::Scope uploadTiming(bettergps_diag::TextureUpload);

    ID3D11Texture2D* texture = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    {
        std::unique_lock<std::mutex> lock(g_d3dMutex,std::try_to_lock);if(!lock.owns_lock()){dirty=true;return;}
        texture = b.liveTexture;
        format = b.liveFormat;
        if (texture) texture->AddRef();
    }
    if (!texture) {
        dirty.store(true, std::memory_order_release);
        return;
    }

    std::vector<uint8_t> bgra;
    std::shared_ptr<const std::vector<uint8_t>> preparedPixels;uint64_t preparedVersion=0;
    const bool nativeBase=b.height==740&&b.width>=364&&size_t((b.width-364)/4)<gpsDisplays.size()&&gpsDisplays[(b.width-364)/4].nativeFallback;
    if(nativeBase&&b.cleared){texture->Release();return;}
    const bool preparedSkin=!nativeBase&&(&b==&g_skinBridge||((b.height==740&&b.width>=364)||(b.height==500&&b.width>=804)));
    if(preparedSkin&&g_cpuRunning.load()){
        auto& image=g_prepared[prepared_index(b)];image.lastUse=GetTickCount64();std::unique_lock<std::mutex> lock(image.mutex,std::try_to_lock);
        if(!lock.owns_lock()||!image.pixels||image.version==image.uploaded||(g_assetsReady&&image.themeEpoch!=g_mapThemeEpoch.load())){texture->Release();dirty=true;return;}
        const auto known=prepared_index(b);const int managed=known<gpsDisplays.size()?managed_for_known(known):-1;if(managed>=0&&image.managedRevision>managedAppliedRevision[managed].load()){texture->Release();dirty=true;return;}
        preparedPixels=image.pixels;preparedVersion=image.version;
    }
    if(b.height==500&&!preparedPixels)bgra.assign(size_t(b.width)*b.height*4,0);
    else if(nativeBase)bgra.assign(size_t(b.width)*b.height*4,0);
    else if(preparedPixels){} // CPU work is already complete.
    else if(&b==&g_clockBridge||&b==&g_speedBridge||&b==&g_distBridge||&b==&g_timeBridge||&b==&g_guidanceBridge)
        bgra.assign(static_cast<size_t>(b.width)*b.height*4,0);
    else if (&b == &g_skinBridge){
        render_configured_phone(bgra);
    }
    else if((b.height==740&&b.width>=364)||(b.height==500&&b.width>=804)){
        size_t index=(b.width-364)/4;auto settings=display_snapshot(index);std::vector<uint8_t> canvas;render_configured_phone(canvas,&settings,index+1);
        bgra.resize(size_t(b.width)*b.height*4);
        std::fill(bgra.begin(),bgra.end(),0);for(unsigned y=0;y<740;++y)memcpy(bgra.data()+size_t(y)*b.width*4,canvas.data()+size_t(y)*360*4,360*4);
    }
    else if(&b==&g_mapBackgroundBridge){const bool dark=g_darkMode.load(std::memory_order_acquire);bgra.resize(static_cast<size_t>(b.width)*b.height*4);for(size_t i=0;i<bgra.size();i+=4){bgra[i]=waze_style()?(dark?0x3b:0xf2):(dark?0x36:0xe1);bgra[i+1]=waze_style()?(dark?0x30:0xf1):(dark?0x26:0xe1);bgra[i+2]=waze_style()?(dark?0x26:0xec):(dark?0x1a:0xe1);bgra[i+3]=255;}}
    else if(!g_tripPresent.load(std::memory_order_acquire)&&(&b==&g_distBridge||&b==&g_timeBridge||&b==&g_guidanceBridge))
        bgra.assign(static_cast<size_t>(b.width)*b.height*4,0);
    else if (&b == &g_guidanceBridge)
        render_guidance_text_bgra(bgra,b.width,b.height,bettergps_navigation::Current());
    else if (&b == &g_timeBridge)
        render_time_text_bgra(bgra, b.width, b.height, fontHeight);
    else if (&b == &g_distBridge)
        render_distance_text_bgra(bgra, b.width, b.height, text, fontHeight);
    else if (&b == &g_clockBridge)
        render_clock_text_bgra(bgra, b.width, b.height, text);
    else if (&b == &g_speedBridge)
        render_speed_text_bgra(bgra, b.width, b.height, text);
    else
        render_text_bgra(bgra, b.width, b.height, text, fontHeight);

    if(&b==&g_clockBridge&&g_darkMode.load())for(size_t i=0;i<bgra.size();i+=4){bgra[i]=255-bgra[i];bgra[i+1]=255-bgra[i+1];bgra[i+2]=255-bgra[i+2];}
    if(&b==&g_clockBridge||&b==&g_speedBridge||&b==&g_distBridge||&b==&g_timeBridge||&b==&g_guidanceBridge)std::fill(bgra.begin(),bgra.end(),0);
    const auto& upload=preparedPixels?*preparedPixels:bgra;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr;{bettergps_diag::Scope mapTiming(bettergps_diag::UploadMap);hr = context->Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);}
    if (SUCCEEDED(hr)) {
        const bool bgraFormat = format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                                format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        for (UINT y = 0; y < b.height; ++y) {
            const uint8_t* src = upload.data() + static_cast<size_t>(y) * b.width * 4;
            uint8_t* dst = static_cast<uint8_t*>(mapped.pData) +
                           static_cast<size_t>(y) * mapped.RowPitch;
            if (bgraFormat) {
                std::memcpy(dst, src, static_cast<size_t>(b.width) * 4);
            } else {
                for (UINT x = 0; x < b.width; ++x) {
                    dst[x*4 + 0] = src[x*4 + 2];
                    dst[x*4 + 1] = src[x*4 + 1];
                    dst[x*4 + 2] = src[x*4 + 0];
                    dst[x*4 + 3] = src[x*4 + 3];
                }
            }
        }
        context->Unmap(texture, 0);b.cleared=true;
        if(preparedPixels){auto& image=g_prepared[prepared_index(b)];std::lock_guard<std::mutex> lock(image.mutex);image.uploaded=preparedVersion;}
    } else {
        dirty.store(true, std::memory_order_release);
        log(SCS_LOG_TYPE_warning, "%s Map failed on plugin-owned texture (hr=0x%08X)", b.name, hr);
    }
    texture->Release();
}

// ATS may allocate an empty texture and upload its DDS later. Recognition at
// CreateTexture2D alone cannot see that data. Recover the exact marker from its
// pixels at first shader binding, irrespective of the upload path or SRV order.

struct MarkerRetry { ID3D11ShaderResourceView* view; ULONGLONG next; }; // compatibility with cleanup
static std::vector<MarkerRetry> g_markerRetries;
static std::vector<std::unique_ptr<PhoneScreenPower>> g_phoneScreens;
static std::vector<std::unique_ptr<IndependentPhoneSurface>> g_independentSurfaces;
static std::mutex g_phoneScreenMutex;

static PhoneScreenPower* phone_screen(ID3D11Texture2D* texture){for(auto& screen:g_phoneScreens)if(screen->native==texture)return screen.get();return nullptr;}
#include "DeviceGpu.h"
#include "InternationalPageGpu.h"
#include "CopilotPageGpu.h"
#include "CameraFrameEvidence.h"
#include "T680ClusterGpu.h"
#include "MackClusterGpu.h"
#include "VnlClusterGpu.h"
#include "Volvo18ClusterGpu.h"
#include "Volvo25ClusterGpu.h"
#include "CascadiaClusterGpu.h"
#include "IcucClusterGpu.h"
static ID3D11ShaderResourceView* icc5_boot_guard_view(ID3D11DeviceContext* context){return icc5_boot_guard_needed()?managed_black_view(context,0):g_icc5BootBridge.disabledView;}
static ID3D11ShaderResourceView* volvo25_boot_guard_view(ID3D11DeviceContext* context){return volvo25_boot_guard_needed()?managed_black_view(context,1):g_volvo25BootBridge.disabledView;}
static ID3D11ShaderResourceView* volvo18_boot_guard_view(ID3D11DeviceContext* context){return volvo18_boot_guard_needed()?managed_black_view(context,1):g_volvo18BootBridge.disabledView;}
static ID3D11ShaderResourceView* truck_boot_guard_view(ID3D11DeviceContext* context){return truck_boot_guard_needed()?managed_black_view(context,truck_boot_guard_tone()):g_truckBootBridge.disabledView;}
static ID3D11ShaderResourceView* mack_boot_guard_view(ID3D11DeviceContext* context){return mack_boot_guard_needed()?managed_black_view(context,true):g_mackBootBridge.disabledView;}
static ID3D11ShaderResourceView* vnl_boot_guard_view(ID3D11DeviceContext* context){return vnl_boot_guard_needed()?managed_black_view(context,displayIgnition==1?2:1):g_vnlBootBridge.disabledView;}

static void identify_phone_drawable(ID3D11DeviceContext* context,const OverlayBridge& overlay){
    mapGpuAudit.target(context,1);
    static std::atomic<unsigned> traceCount{0};const bool trace=traceCount.fetch_add(1)<12;
    const auto contextType=context->GetType();
    if(contextType!=D3D11_DEVICE_CONTEXT_IMMEDIATE){if(trace)log(SCS_LOG_TYPE_message,"[PhonePower/v2] Verified phone draw: context=%p type=%u (deferred)",context,unsigned(contextType));return;}
    ID3D11RenderTargetView* target=native_bound_target(context);if(!target){if(trace)log(SCS_LOG_TYPE_message,"[PhonePower/v2] Verified phone draw: context=%p no render target",context);return;}
    ID3D11Resource* resource=nullptr;target->GetResource(&resource);target->Release();ID3D11Texture2D* texture=nullptr;
    if(resource){resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture);resource->Release();}if(!texture)return;
    D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
    if(trace){UINT count=1;D3D11_VIEWPORT viewport{};context->RSGetViewports(&count,&viewport);log(SCS_LOG_TYPE_message,"[PhonePower/v2] Verified phone draw: target=%p size=%ux%u format=%u bind=%u mip=%u samples=%u viewport=%.0f,%.0f %.0fx%.0f",texture,d.Width,d.Height,unsigned(d.Format),d.BindFlags,d.MipLevels,d.SampleDesc.Count,viewport.TopLeftX,viewport.TopLeftY,viewport.Width,viewport.Height);}
    // The native Road Trip and Cabin Accessories drawable is 256 x 512.
    // Dimensions alone never activate this path: a verified BetterGPS marker
    // must be drawn into the target before it is considered a phone screen.
    std::unique_lock<std::mutex> screenLock(g_phoneScreenMutex,std::try_to_lock);if(!screenLock.owns_lock()){texture->Release();return;}
    if(d.Width==256&&d.Height==512&&!phone_screen(texture)&&g_phoneScreens.size()<33){auto screen=std::make_unique<PhoneScreenPower>();
        if(screen->start(texture)){screen->displayIndex=prepared_index(overlay);log(SCS_LOG_TYPE_message,"Phone power control attached to final drawable %ux%u display=%llu",d.Width,d.Height,(unsigned long long)screen->displayIndex);g_phoneScreens.push_back(std::move(screen));}}
    if(auto* screen=phone_screen(texture);screen&&screen->displayIndex<gpsDisplays.size()&&gpsDisplays[screen->displayIndex].independent){
        // The map is complete, but our overlay has not yet been drawn. Retain
        // this clean base so a dormant native UI cannot freeze the home screen.
        if(display_source_active(screen->displayIndex)&&startup_elapsed(GetTickCount64())>=startupBlackMs){screen->capture_base(context,GetTickCount64());mapGpuAudit.capture(context,screen->snapshot,2);}
    }
    texture->Release();
}
static void follow_phone_copy(ID3D11Resource* destination,ID3D11Resource* source){
    if(!source||!destination||source==destination)return;UINT tag=0,size=sizeof(tag);
    if(FAILED(source->GetPrivateData(phoneTrackedResource,&size,&tag))||tag!=1)return;
    size=sizeof(tag);if(SUCCEEDED(destination->GetPrivateData(phoneOwnedResource,&size,&tag)))return;
    ID3D11Texture2D *src=nullptr,*dst=nullptr;source->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&src);destination->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&dst);
    if(src&&dst){std::unique_lock<std::mutex> screenLock(g_phoneScreenMutex,std::try_to_lock);if(!screenLock.owns_lock()){src->Release();dst->Release();return;}auto* parent=phone_screen(src);D3D11_TEXTURE2D_DESC d{};dst->GetDesc(&d);
        if(parent&&!parent->managedOutput&&d.Width==parent->width&&d.Height==parent->height&&(d.BindFlags&D3D11_BIND_SHADER_RESOURCE)&&!phone_screen(dst)&&g_phoneScreens.size()<33){RenderCall work(RenderStage::Copy);
            auto alias=std::make_unique<PhoneScreenPower>();if(alias->start(dst)){alias->displayIndex=parent->displayIndex;log(SCS_LOG_TYPE_message,"[PhonePower/v4] Followed native drawable copy %p -> %p format=%u mip=%u",src,dst,unsigned(d.Format),d.MipLevels);g_phoneScreens.push_back(std::move(alias));}}
    }if(src)src->Release();if(dst)dst->Release();
}
static void STDMETHODCALLTYPE hook_CopyResource(ID3D11DeviceContext* context,ID3D11Resource* destination,ID3D11Resource* source){touchGeometry.before_overwrite(context,destination,"before-copy-resource");if(!phoneInternalWork)TouchComposition::invalidate(destination);g_origCopyResource(context,destination,source);if(!phoneInternalWork)touch_copy_tag(destination,source);if(!phoneInternalWork&&render_gpu_allowed())follow_phone_copy(destination,source);}
static void STDMETHODCALLTYPE hook_CopyRegion(ID3D11DeviceContext* context,ID3D11Resource* destination,UINT dstSub,UINT x,UINT y,UINT z,ID3D11Resource* source,UINT srcSub,const D3D11_BOX* box){
    touchGeometry.before_overwrite(context,destination,"before-copy-region");if(!phoneInternalWork)TouchComposition::invalidate(destination);g_origCopyRegion(context,destination,dstSub,x,y,z,source,srcSub,box);if(!phoneInternalWork)touch_copy_region_tag(destination,dstSub,x,y,z,source,srcSub,box);if(!phoneInternalWork&&render_gpu_allowed()&&!dstSub&&!srcSub&&!x&&!y&&!z&&(!box||(!box->left&&!box->top&&box->right==256&&box->bottom==512)))follow_phone_copy(destination,source);
}
static const GUID kViewTag={0xb7106411,0x5154,0x4535,{0x91,0x82,0x55,0x27,0x31,0x69,0x08,0x10}};
static const GUID kPendingStage={0xb7106412,0x5154,0x4535,{0x91,0x82,0x55,0x27,0x31,0x69,0x08,0x10}};
struct ViewTag {uintptr_t epoch;OverlayBridge* bridge;ID3D11Texture2D* texture;uint64_t retry;unsigned nativeIcon=0;ID3D11Texture2D* iconTexture=nullptr;ID3D11ShaderResourceView *iconLight=nullptr,*iconDark=nullptr;size_t iconStyle=~size_t(0);};
static const uintptr_t g_viewEpoch=uintptr_t(&g_d3dMutex)^uintptr_t(GetTickCount64());
static ViewTag prepare_icon_views(ID3D11ShaderResourceView* view,ViewTag tag,bool force=false,size_t style=displayStyleIndex.load()){if(g_assetsReady&&!force)return tag;
 if(tag.nativeIcon&&(!tag.iconLight||!tag.iconDark||tag.iconStyle!=style)){RenderCall work(RenderStage::IconPreparation);PhoneInternalScope isolated;tag.iconLight=native_icon_select(view,tag.nativeIcon,false,style);tag.iconDark=native_icon_select(view,tag.nativeIcon,true,style);tag.iconStyle=style;view->SetPrivateData(kViewTag,sizeof(tag),&tag);}return tag;
}
static ViewTag classify_view(ID3D11ShaderResourceView* view){ViewTag tag{};if(!view)return tag;UINT size=sizeof(tag);if(SUCCEEDED(view->GetPrivateData(kViewTag,&size,&tag))&&size==sizeof(tag)&&tag.epoch==g_viewEpoch){if(!tag.nativeIcon&&tag.iconTexture){UINT bytes=sizeof(tag.nativeIcon);tag.iconTexture->GetPrivateData(nativeIconTag,&bytes,&tag.nativeIcon);}return prepare_icon_views(view,tag);}
 tag={g_viewEpoch,nullptr,nullptr,0};ID3D11Resource* resource=nullptr;view->GetResource(&resource);if(resource){ID3D11Texture2D* texture=nullptr;if(SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture))){D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);D3D11_SHADER_RESOURCE_VIEW_DESC v{};view->GetDesc(&v);auto* b=bridge_for_desc(&d);if((d.Width==32&&d.Height==32)||(d.Width==64&&d.Height==64)||(d.Width==128&&d.Height==128)){tag.iconTexture=texture;UINT bytes=sizeof(tag.nativeIcon);texture->GetPrivateData(nativeIconTag,&bytes,&tag.nativeIcon);}if(b&&b!=&g_mapBridge&&d.SampleDesc.Count==1&&v.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D){std::unique_lock<std::mutex> lock(g_d3dMutex,std::try_to_lock);if(!lock.owns_lock()){texture->Release();resource->Release();return tag;}if(texture!=b->liveTexture){tag.bridge=b;tag.texture=texture;}}else if(d.Width==256&&d.Height==512&&d.SampleDesc.Count==1&&v.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D)tag.texture=texture;texture->Release();}resource->Release();}view->SetPrivateData(kViewTag,sizeof(tag),&tag);return prepare_icon_views(view,tag);
}
static void recover_marker_at_binding(ID3D11DeviceContext* context,ID3D11ShaderResourceView* view){auto tag=classify_view(view);auto* b=tag.bridge;if(!b)return;
 {std::unique_lock<std::mutex> lock(g_d3dMutex,std::try_to_lock);if(!lock.owns_lock())return;if(view==b->liveView||(view==b->markerView&&b->liveView))return;}
 bool recognized=false;{std::unique_lock<std::mutex> lock(g_d3dMutex,std::try_to_lock);if(!lock.owns_lock())return;recognized=b->markerTexture==tag.texture;}
 // A streamed marker can be uploaded after its SRV is created. Once the
 // upload signature is verified, creating our own resources needs no readback
 // and is valid on a deferred binding too. Only staging readback is immediate.
 if(!recognized&&context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
 ID3D11Device* device=nullptr;context->GetDevice(&device);if(!device)return;
 // The first phone marker must be verified before that same UI frame is
 // sampled on the accessory. A deferred first readback leaks one native frame.
 bool firstPhone=managed_bridge(b)>=0||surface_bridge(b)||b==&g_f150Bridge||b==&g_f150InfoBridge||is_cluster_bridge(b);
 if(b==&g_skinBridge||b->height==740){ID3D11RenderTargetView* target=native_bound_target(context);if(target){ID3D11Resource* resource=nullptr;target->GetResource(&resource);target->Release();ID3D11Texture2D* texture=nullptr;if(SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&texture))){D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);firstPhone=desc.Width==256&&desc.Height==512;texture->Release();}resource->Release();}}
 if(!recognized){auto now=GetTickCount64();if(now<tag.retry){device->Release();return;}
  ID3D11Texture2D* stage=nullptr;UINT size=sizeof(stage);view->GetPrivateData(kPendingStage,&size,&stage);
  if(!stage){D3D11_TEXTURE2D_DESC d{};tag.texture->GetDesc(&d);D3D11_SHADER_RESOURCE_VIEW_DESC v{};view->GetDesc(&v);d.Width=4;d.Height=1;d.MipLevels=d.ArraySize=1;d.Format=v.Format;d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
   if(SUCCEEDED(g_origCreateTexture2D(device,&d,nullptr,&stage))){D3D11_BOX box{0,0,0,4,1,1};context->CopySubresourceRegion(stage,0,0,0,0,tag.texture,v.Texture2D.MostDetailedMip,&box);view->SetPrivateDataInterface(kPendingStage,stage);}if(!stage||!firstPhone){if(stage)stage->Release();device->Release();return;}}
  D3D11_MAPPED_SUBRESOURCE mapped{};auto hr=context->Map(stage,0,D3D11_MAP_READ,firstPhone?0:D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
  if(SUCCEEDED(hr)){D3D11_TEXTURE2D_DESC d{};stage->GetDesc(&d);D3D11_SUBRESOURCE_DATA data{};data.pSysMem=mapped.pData;data.SysMemPitch=mapped.RowPitch;recognized=marker_signature(b,&d,&data);context->Unmap(stage,0);view->SetPrivateDataInterface(kPendingStage,nullptr);if(!recognized){tag.retry=now+1000;view->SetPrivateData(kViewTag,sizeof(tag),&tag);}}
  stage->Release();if(!recognized){device->Release();return;}
 }
 {std::unique_lock<std::mutex> lock(g_d3dMutex,std::try_to_lock);if(!lock.owns_lock()){device->Release();return;}reset_marker_locked(*b);b->markerTexture=tag.texture;tag.texture->AddRef();b->markerView=view;view->AddRef();create_live_resources_locked(*b,device);}
 device->Release();
}
static void bind_cached_phone(ID3D11DeviceContext* context,UINT startSlot,UINT count,ID3D11ShaderResourceView* const* views){
 if(!render_cached_phone_allowed()||!views||!count||count>128){bind_original_srv(context,startSlot,count,views);return;}
 std::array<ID3D11ShaderResourceView*,128> replacement{},retained{};bool changed=false;
 for(UINT i=0;i<count;++i){replacement[i]=views[i];UINT bytes=sizeof(ID3D11ShaderResourceView*);
  // A streamed marker may be uploaded after its SRV was created. Recover
  // only its already verified resource identity, never inspect other screens.
  if(views[i])for(auto* clusterBridge:{&g_intlClusterBridge,&g_icuc5700ClusterBridge,&g_icucClusterBridge,&g_icc5ClusterBridge,&g_icc5BootBridge,&g_clusterIntroBridge,&g_mackClusterBridge,&g_mackBootBridge,&g_vnlClusterBridge,&g_vnlBootBridge,&g_volvo18ClusterBridge,&g_volvo18BootBridge,&g_volvo25ClusterBridge,&g_volvo25BootBridge,&g_truckBootBridge})if(clusterBridge->markerTexture&&(views[i]!=clusterBridge->markerView||!clusterBridge->liveView)){ID3D11Resource* resource=nullptr;views[i]->GetResource(&resource);const bool cluster=resource==static_cast<ID3D11Resource*>(clusterBridge->markerTexture);if(resource)resource->Release();if(cluster){recover_marker_at_binding(context,views[i]);break;}}
  if(views[i]&&views[i]==g_truckBootBridge.markerView){if(auto* guard=truck_boot_guard_view(context)){guard->AddRef();replacement[i]=retained[i]=guard;changed=true;}continue;}
  if(views[i]&&views[i]==g_mackBootBridge.markerView){if(auto* guard=mack_boot_guard_view(context)){guard->AddRef();replacement[i]=retained[i]=guard;changed=true;}continue;}
  if(views[i]&&views[i]==g_icc5BootBridge.markerView){if(auto* guard=icc5_boot_guard_view(context)){guard->AddRef();replacement[i]=retained[i]=guard;changed=true;}continue;}
  if(views[i]&&views[i]==g_volvo25BootBridge.markerView){if(auto* guard=volvo25_boot_guard_view(context)){guard->AddRef();replacement[i]=retained[i]=guard;changed=true;}continue;}
  if(views[i]&&views[i]==g_volvo18BootBridge.markerView){if(auto* guard=volvo18_boot_guard_view(context)){guard->AddRef();replacement[i]=retained[i]=guard;changed=true;}continue;}
  if(views[i]&&views[i]==g_vnlBootBridge.markerView){if(auto* guard=vnl_boot_guard_view(context)){guard->AddRef();replacement[i]=retained[i]=guard;changed=true;}continue;}
  // The very first paused SRV has not passed classify_view yet. Its COM
  // resource is already signature-verified at upload: use that identity too.
  bool guardedManaged=false;
  if(views[i]){ID3D11Resource* resource=nullptr;views[i]->GetResource(&resource);for(size_t slot=0;slot<g_managedBridges.size()/3;++slot){auto& bridge=*g_managedBridges[slot*3];if(bridge.markerTexture&&resource==static_cast<ID3D11Resource*>(bridge.markerTexture)&&managed_visible_splash(managed_snapshot(unsigned(slot)))){if(auto* guarded=managed_guarded_overlay_view(context,bridge,unsigned(slot))){guarded->AddRef();replacement[i]=retained[i]=guarded;changed=true;}guardedManaged=true;break;}}if(resource)resource->Release();}
  if(guardedManaged)continue;
  ViewTag tag{};UINT tagBytes=sizeof(tag);const bool tagged=views[i]&&SUCCEEDED(views[i]->GetPrivateData(kViewTag,&tagBytes,&tag));
  const int managed=tagged&&tag.bridge?managed_bridge(tag.bridge):-1;
  // Paused/loading frames still need the current startup guard. Do not reuse
  // a previously cached navigation SRV until the worker confirms this boot.
  if(managed>=0&&managed%3==0&&managed_visible_splash(managed_snapshot(unsigned(managed/3)))){if(auto* guarded=managed_guarded_overlay_view(context,*tag.bridge,unsigned(managed/3))){guarded->AddRef();replacement[i]=retained[i]=guarded;changed=true;}continue;}
  const bool disabled=tagged&&tag.bridge&&(surface_bridge(tag.bridge)||tag.bridge->height==500)&&!display_active(surface_bridge(tag.bridge)?surface_index(tag.bridge):prepared_index(*tag.bridge));
  if(views[i]&&SUCCEEDED(views[i]->GetPrivateData(disabled?kNativePhoneView:kPausedPhoneView,&bytes,&retained[i]))&&retained[i]){replacement[i]=retained[i];changed=true;}}
 bind_original_srv(context,startSlot,count,changed?replacement.data():views);touch_note_bindings(context,startSlot,count,changed?replacement.data():views);
 for(UINT i=0;i<count;++i)if(retained[i])retained[i]->Release();
}
static void STDMETHODCALLTYPE hook_PSSetShaderResources(ID3D11DeviceContext* context,UINT startSlot,UINT count,ID3D11ShaderResourceView* const* views){bettergps_diag::Scope bindingTiming(bettergps_diag::TextureBinding,64);
 if(phoneInternalWork){bind_original_srv(context,startSlot,count,views);return;}
 if(!render_bind_allowed()){bind_cached_phone(context,startSlot,count,views);return;}
 if(!views||!count||count>128){bind_original_srv(context,startSlot,count,views);return;}
 std::array<ID3D11ShaderResourceView*,128> replacement{};std::array<ID3D11ShaderResourceView*,128> retained{};bool changed=false;ID3D11ShaderResourceView* previous=nullptr;ViewTag previousTag{};
 for(UINT i=0;i<count;++i){replacement[i]=views[i];auto tag=views[i]&&views[i]==previous?previousTag:classify_view(views[i]);previous=views[i];previousTag=tag;auto* b=tag.bridge;
  if(!tag.nativeIcon&&tag.iconTexture&&g_assetsReady){PhoneInternalScope internal;native_icon_probe(context,views[i],tag.iconTexture);}if(tag.nativeIcon){bool dark=g_darkMode.load();size_t style=displayStyleIndex.load();if(!g_assetsReady||google_map_context(context,dark,&style)){tag=prepare_icon_views(views[i],tag,true,style);auto* icon=dark?tag.iconDark:tag.iconLight;if(icon){icon->AddRef();replacement[i]=retained[i]=icon;changed=true;}}continue;}
  if(!b){cameraFrameEvidence.consumed(views[i]);tag_google_map_source(context,views[i]);if(tag.texture&&context->GetType()==D3D11_DEVICE_CONTEXT_IMMEDIATE){
    std::unique_lock<std::mutex> lock(g_phoneScreenMutex,std::try_to_lock);if(!lock.owns_lock())continue;if(auto* screen=phone_screen(tag.texture)){RenderCall work(RenderStage::Binding);
      // The same drawable can be sampled while generating its mip chain.
      // Keep those producer passes native; control every other consumer,
      // including intermediate 256x512 targets used by ATS's display renderer.
      ID3D11RenderTargetView* target=native_bound_target(context);ID3D11Resource* resource=nullptr;
      if(target){target->GetResource(&resource);target->Release();}const bool producer=resource==static_cast<ID3D11Resource*>(screen->native);if(resource)resource->Release();
      if(!producer&&screen->display_view(views[i])){const auto now=GetTickCount64();const int phase=startup_elapsed(now);
        auto* heldScreen=screen;
        if(phase<=-3&&!screen->captured){for(auto& previousScreen:g_phoneScreens)if(previousScreen->displayIndex==screen->displayIndex&&previousScreen->renderer.device==screen->renderer.device&&previousScreen->captured&&(!heldScreen->captured||previousScreen->lastCopy>heldScreen->lastCopy))heldScreen=previousScreen.get();}
        auto* live=views[i];
        const bool independent=screen->displayIndex<gpsDisplays.size()&&gpsDisplays[screen->displayIndex].independent;
        if(independent){/* Keep the original physical screen binding entirely native. */}
        else live=heldScreen->present(context,views[i],phase,now,g_origPSSetSRV);
        const int state=phase>=startupBlackMs?1:phase>=0?0:phase<=-3?(-phase-3<shutdownHoldMs?2:3):4;
        if(screen->reportedPhase!=state&&screen->reportBudget){screen->reportedPhase=state;--screen->reportBudget;D3D11_SHADER_RESOURCE_VIEW_DESC desc{};views[i]->GetDesc(&desc);log(SCS_LOG_TYPE_message,"[PhonePower/v8] Source binding=%p state=%d phase=%d captured=%u view-format=%u mip=%u+%u",tag.texture,state,phase,unsigned(screen->captured),unsigned(desc.Format),desc.Texture2D.MostDetailedMip,desc.Texture2D.MipLevels);}
        if(live!=views[i]){live->AddRef();replacement[i]=retained[i]=live;changed=true;}
      }
    }
  }continue;}
  RenderCall work(RenderStage::Binding);recover_marker_at_binding(context,views[i]);ID3D11ShaderResourceView* live=nullptr;{std::unique_lock<std::mutex> lock(g_d3dMutex,std::try_to_lock);if(!lock.owns_lock())continue;if(views[i]==b->markerView&&b->liveView){live=b->liveView;live->AddRef();}}if(!live)continue;

  const int managed=managed_bridge(b);
  if(b==&g_clusterIntroBridge){live->Release();live=cluster_intro_view(context,GetTickCount64());if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_mackClusterBridge){live->Release();live=mack_cluster_view(context,GetTickCount64());if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_vnlClusterBridge){live->Release();live=vnl_cluster_view(context,GetTickCount64());if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_intlClusterBridge||b==&g_icucClusterBridge||b==&g_icuc5700ClusterBridge){live->Release();live=icuc_cluster_view(context,GetTickCount64(),b==&g_intlClusterBridge?2:b==&g_icuc5700ClusterBridge?1:0);if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_icc5ClusterBridge){live->Release();live=icc5_cluster_view(context,GetTickCount64());if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_icc5BootBridge){live->Release();live=icc5_boot_guard_view(context);if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_volvo25ClusterBridge){live->Release();live=volvo25_cluster_view(context,GetTickCount64());if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_volvo25BootBridge){live->Release();live=volvo25_boot_guard_view(context);if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_volvo18ClusterBridge){live->Release();live=volvo18_cluster_view(context,GetTickCount64());if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_volvo18BootBridge){live->Release();live=volvo18_boot_guard_view(context);if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_vnlBootBridge){live->Release();live=vnl_boot_guard_view(context);if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_truckBootBridge){live->Release();live=truck_boot_guard_view(context);if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(b==&g_mackBootBridge){live->Release();live=mack_boot_guard_view(context);if(live){live->AddRef();replacement[i]=retained[i]=live;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}continue;}
  if(managed>=0){const unsigned slot=unsigned(managed/3),role=unsigned(managed%3);if(role==0){managed_upload(context,*b,slot);if(auto* guarded=managed_guarded_overlay_view(context,*b,slot)){live->Release();live=guarded;live->AddRef();views[i]->SetPrivateDataInterface(kPausedPhoneView,live);}cameraFrameEvidence.binding(context,slot,live);}else{live->Release();live=b->disabledView;if(live)live->AddRef();if(role==1){g_expectGoogleMap=nullptr;g_expectManagedMap=-1;if(nativeForkPublished&&managedDeviceActive[slot]&&managed_power_target(slot)){expect_google_map_source(context);g_expectManagedMap=int(slot);}}else managed_capture_target(context,slot);}
    if(live){replacement[i]=retained[i]=live;changed=true;}continue;
  }
  const bool phoneSurface=surface_bridge(b);if(phoneSurface)display_note_seen(surface_index(b));const size_t known=phoneSurface?surface_index(b):b->height==500&&b->width>=804?prepared_index(*b):32;
  if(known<gpsDisplays.size()&&managed_highlight_known(known)){if(auto* orange=managed_orange_view(context)){orange->AddRef();live->Release();replacement[i]=retained[i]=orange;changed=true;continue;}}
  if(b==&g_f150SourceBridge){
    live->Release();if(b->disabledView){b->disabledView->AddRef();replacement[i]=retained[i]=b->disabledView;changed=true;}
    bool active=false;for(size_t j=0;j<gpsDisplays.size();++j)if(ford_console_id(gpsDisplays[j].id)&&display_vehicle_matches(gpsDisplays[j],managed_vehicle())&&display_active(j)){active=true;break;}
    if(active){const auto now=GetTickCount64();nativeForkLastSurfaceUse=now;nativeForkHorizontalLastUse=now;if(nativeForkPublished)expect_google_map_source(context,true);}continue;
  }
  if(b==&g_f150MapBridge){
    live->Release();g_expectGoogleMap=nullptr;
    size_t index=gpsDisplays.size();for(size_t j=0;j<gpsDisplays.size();++j)if(ford_console_id(gpsDisplays[j].id)&&display_vehicle_matches(gpsDisplays[j],managed_vehicle())){index=j;break;}
    ID3D11ShaderResourceView* map=nullptr;
    if(index<gpsDisplays.size()&&display_active(index)){
      const auto now=GetTickCount64();nativeForkLastSurfaceUse=now;nativeForkHorizontalLastUse=now;
      std::lock_guard<std::mutex> lock(g_horizontalMapMutex);
      if(g_horizontalMapInput&&g_horizontalMapGeneration==nativeForkSourceGeneration.load()){map=g_horizontalMapInput;map->AddRef();}
    }
    if(!map&&b->disabledView){map=b->disabledView;map->AddRef();}
    if(map){replacement[i]=retained[i]=map;changed=true;views[i]->SetPrivateDataInterface(kPausedPhoneView,map);}
    g_horizontalMapState=map&&map!=b->disabledView?4:1;continue;
  }
  if(b->height==500&&b->width>=804){
    g_expectGoogleMap=nullptr;
    const auto index=prepared_index(*b);display_note_seen(index);g_prepared[index].lastUse=GetTickCount64();
    if(!display_active(index)){live->Release();if(b->disabledView){b->disabledView->AddRef();replacement[i]=retained[i]=b->disabledView;changed=true;}continue;}
    const auto now=GetTickCount64();nativeForkLastSurfaceUse=now;nativeForkHorizontalLastUse=now;
    // ATS draws the private map using its own UI shader and cropped quad.
    // This marker is only the transparent CarPlay overlay, as on the phone.
    std::atomic<bool> dirty{true};update_bridge_on_render_thread(context,*b,dirty,"",0);
    views[i]->SetPrivateDataInterface(kPausedPhoneView,live);
    mapGpuAudit.target(context,5);{std::lock_guard<std::mutex> lock(g_horizontalMapMutex);if(g_horizontalMapInput)mapGpuAudit.sample(context,g_horizontalMapInput,4);}
    replacement[i]=retained[i]=live;changed=true;continue;
  }
  if(b==&g_f150InfoBridge){
    if(context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE){live->Release();continue;}
    f150InfoRequested=true;
    auto clear=[&]{live->Release();if(g_f150InfoClearView){g_f150InfoClearView->AddRef();replacement[i]=retained[i]=g_f150InfoClearView;changed=true;}f150InfoUploaded=-2;};
    if(!carInfoEnabled||!displayGlobalEnabled){f150InfoSeenSerial=~uint64_t(0);clear();continue;}
    const auto now=GetTickCount64(),serial=f150Serial.load();
    if(f150InfoSeenSerial!=serial){f150InfoSeenSerial=serial;f150InfoOrigin=now;f150InfoUploaded=-99;f150InfoWaiting=false;}
    const auto animation=std::atomic_load(&f150InfoAnimation);
    if(animation&&animation->video){animation->video->start_if_needed();auto video=animation->video->snapshot();if(video.pixels&&video.serial!=uint64_t(std::max(-1,f150InfoUploaded))){PhoneInternalScope isolated;context->UpdateSubresource(b->liveTexture,0,nullptr,video.pixels->data(),f150InfoWidth*4,0);f150InfoUploaded=int(video.serial);}if(video.done){const auto fadeElapsed=now>video.doneAt?now-video.doneAt:0;if(fadeElapsed>=500){clear();continue;}if(g_f150InfoFade.view){PhoneInternalScope isolated;if(g_f150InfoFade.draw(context,live,g_origPSSetSRV,float(500-fadeElapsed)/500.f)){live->Release();g_f150InfoFade.view->AddRef();replacement[i]=retained[i]=g_f150InfoFade.view;changed=true;continue;}}}replacement[i]=retained[i]=live;changed=true;continue;}
    // The immutable on-disk marker and initial GPU frame both contain frame 0.
    // A cold/late worker holds that image, never exposing the native dashboard.
    if(!animation){f150InfoWaiting=true;if(f150InfoLoadFinished){clear();continue;}}
    if(!animation&&f150InfoUploaded==-99){std::lock_guard<std::mutex> lock(carMutex);if(f150InfoFirst.size()==f150InfoFrameBytes){PhoneInternalScope isolated;context->UpdateSubresource(b->liveTexture,0,nullptr,f150InfoFirst.data(),(f150InfoWidth/4)*8,0);f150InfoUploaded=0;}}
    if(animation&&f150InfoWaiting){f150InfoOrigin=now;f150InfoWaiting=false;}
    static uint64_t uploadedRevision=~uint64_t(0);if(animation&&uploadedRevision!=animation->revision){uploadedRevision=animation->revision;f150InfoUploaded=-99;}
    const auto elapsed=f150Ignition.load()?now-f150InfoOrigin.load():0;
    const int frame=animation?f150_info_frame(elapsed,animation->count):0;
    if(frame==-2){clear();continue;}
    const int stored=animation&&frame>=0?int(animation_storage_frame(*animation,unsigned(frame))):frame;
    if(animation&&stored!=f150InfoUploaded){PhoneInternalScope isolated;context->UpdateSubresource(b->liveTexture,0,nullptr,animation_frame_data(*animation,unsigned(frame)),(f150InfoWidth/4)*8,0);f150InfoUploaded=stored;}
    const float opacity=animation?f150_info_opacity(elapsed,animation->count):1;
    if(opacity<1&&g_f150InfoFade.view){PhoneInternalScope isolated;if(g_f150InfoFade.draw(context,live,g_origPSSetSRV,opacity)){live->Release();g_f150InfoFade.view->AddRef();replacement[i]=retained[i]=g_f150InfoFade.view;changed=true;continue;}}
    replacement[i]=retained[i]=live;changed=true;continue;
  }
  if(b==&g_f150Bridge){
    // Native ignition visibility and UI geometry remain untouched. Only this
    // exact marker is replaced, never another 1024x512 display. Its on-disk
    // pixels already hide the GPS before the first ignition channel arrives.
    int managedF150=-1;for(size_t n=0;n<gpsDisplays.size();++n)if(ford_console_id(gpsDisplays[n].id)&&display_vehicle_matches(gpsDisplays[n],managed_vehicle())){managedF150=managed_for_known(n);break;}
    if(managedF150>=0){auto configured=managed_snapshot(unsigned(managedF150));if(configured.enabled&&displayGlobalEnabled){auto* startup=managed_f150_startup_view(context,unsigned(managedF150));if(!startup&&configured.splash.rfind("bronco/",0)==0){replacement[i]=retained[i]=live;changed=true;continue;}live->Release();if(!startup)startup=g_f150ClearView;if(startup){startup->AddRef();replacement[i]=retained[i]=startup;changed=true;}continue;}}
    f150Requested=true;
    if(!carSplashEnabled||!displayGlobalEnabled){f150SeenSerial=~uint64_t(0);live->Release();if(g_f150ClearView){g_f150ClearView->AddRef();replacement[i]=retained[i]=g_f150ClearView;changed=true;f150Uploaded=-2;}continue;}
    if(!f150Ignition.load()){live->Release();if(g_f150BlackView){g_f150BlackView->AddRef();replacement[i]=retained[i]=g_f150BlackView;changed=true;f150Uploaded=-1;}continue;}
    const auto now=GetTickCount64(),serial=f150Serial.load();
    if(f150SeenSerial!=serial){f150SeenSerial=serial;f150Origin=now;f150Uploaded=-99;f150Waiting=false;}
    const auto animation=std::atomic_load(&f150Animation);
    if(animation&&animation->video){animation->video->start_if_needed();auto video=animation->video->snapshot();if(video.pixels&&video.serial!=uint64_t(std::max(-1,f150Uploaded))){PhoneInternalScope isolated;context->UpdateSubresource(b->liveTexture,0,nullptr,video.pixels->data(),f150Width*4,0);f150Uploaded=int(video.serial);}if(video.done){const auto fadeElapsed=now>video.doneAt?now-video.doneAt:0;if(fadeElapsed>=500){live->Release();if(g_f150ClearView){g_f150ClearView->AddRef();replacement[i]=retained[i]=g_f150ClearView;changed=true;}continue;}if(g_f150CenterFade.view){PhoneInternalScope isolated;if(g_f150CenterFade.draw(context,live,g_origPSSetSRV,float(500-fadeElapsed)/500.f)){live->Release();g_f150CenterFade.view->AddRef();replacement[i]=retained[i]=g_f150CenterFade.view;changed=true;continue;}}}replacement[i]=retained[i]=live;changed=true;continue;}
    const bool bronco=carIsBronco.load();
    if(!animation&&(bronco||now-f150Origin>=1000)&&!f150LoadFinished.load())f150Waiting=true;
    if(!animation&&bronco&&f150Uploaded!=0){std::lock_guard<std::mutex> lock(carMutex);if(f150First.size()==f150FrameBytes){PhoneInternalScope isolated;context->UpdateSubresource(b->liveTexture,0,nullptr,f150First.data(),(f150Width/4)*8,0);f150Uploaded=0;}}
    if(animation&&f150Waiting){f150Origin=now-(bronco?0:1000);f150Waiting=false;}
    static uint64_t uploadedRevision=~uint64_t(0);if(animation&&uploadedRevision!=animation->revision){uploadedRevision=animation->revision;f150Uploaded=-99;}
    int frame=animation?f150_frame(now-f150Origin,animation->count,bronco):(bronco&&!f150LoadFinished.load()?0:((now-f150Origin<1000||!f150LoadFinished.load())?-1:-2));
    if(frame==-2){live->Release();if(g_f150ClearView){g_f150ClearView->AddRef();replacement[i]=retained[i]=g_f150ClearView;changed=true;f150Uploaded=-2;}continue;}
    if(frame==-1){live->Release();if(g_f150BlackView){g_f150BlackView->AddRef();replacement[i]=retained[i]=g_f150BlackView;changed=true;f150Uploaded=-1;}continue;}
    const int stored=animation&&frame>=0?int(animation_storage_frame(*animation,unsigned(frame))):frame;
    if(stored!=f150Uploaded){PhoneInternalScope isolated;
      const auto* pixels=animation_frame_data(*animation,unsigned(frame));
      context->UpdateSubresource(b->liveTexture,0,nullptr,pixels,(f150Width/4)*8,0);f150Uploaded=stored;
    }
    replacement[i]=retained[i]=live;changed=true;continue;
  }
  if(surface_bridge(b)){
    const size_t index=surface_index(b);display_note_seen(index);
    if(!display_active(index)){live->Release();if(b->disabledView){b->disabledView->AddRef();replacement[i]=retained[i]=b->disabledView;changed=true;}continue;}

    const auto now=GetTickCount64();nativeForkLastSurfaceUse=now;const int slot=managed_for_known(index);const auto timing=slot>=0?StartupPhase{managedPower[slot].phase.load(),managedPower[slot].serial.load()}:startup_sample(now);const int phase=timing.elapsed;
    if(index<33)g_prepared[index].lastUse=now;
    {std::unique_lock<std::mutex> lock(g_phoneScreenMutex,std::try_to_lock);if(!lock.owns_lock()){live->Release();continue;}IndependentPhoneSurface* display=nullptr;ID3D11Device* device=nullptr;context->GetDevice(&device);
      for(auto& surface:g_independentSurfaces)if(surface->displayIndex==index&&surface->power.renderer.device==device)display=surface.get();
      if(!display&&g_independentSurfaces.size()<33){auto surface=std::make_unique<IndependentPhoneSurface>();if(surface->start(device,index)){display=surface.get();g_independentSurfaces.push_back(std::move(surface));log(SCS_LOG_TYPE_message,"[PhonePower/v9] Independent display ready: index=%llu, native UI not required",(unsigned long long)index);}}
      device->Release();if(display){display->seen=now;auto* owned=display->view(phase,timing.state);const int touchSlot=managed_for_known(index);if(touchSlot>=0)touch_tag_view(owned,unsigned(touchSlot));owned->AddRef();live->Release();live=owned;}}
    {std::unique_lock<std::mutex> lock(g_d3dMutex,std::try_to_lock);if(lock.owns_lock()&&b->markerView==views[i]&&b->pausedView!=live){if(SUCCEEDED(views[i]->SetPrivateDataInterface(kPausedPhoneView,live)))b->pausedView=live;}}
    replacement[i]=retained[i]=live;changed=true;continue;
  }
  if(b==&g_skinBridge||b->height==740){g_expectGoogleMap=nullptr;if(b->height==740){auto index=prepared_index(*b);if(index<gpsDisplays.size()&&!gpsDisplays[index].independent)display_note_seen(index);}identify_phone_drawable(context,*b);std::atomic<bool> dirty{true};update_bridge_on_render_thread(context,*b,dirty,"",0);}
  else if(b==&g_mapBackgroundBridge){update_bridge_on_render_thread(context,*b,g_mapBackgroundDirty,"",0);if(nativeForkPublished)expect_google_map_source(context);}
  else {std::atomic<bool> dirty{true}; // Retired per-field markers stay transparent.
    if(b->liveTexture&&!b->cleared)update_bridge_on_render_thread(context,*b,dirty,"",0);}
  replacement[i]=live;retained[i]=live;changed=true;
 }
 bind_original_srv(context,startSlot,count,changed?replacement.data():views);
 // Publish the final, possibly substituted views on the active gameplay path.
 // Tracking only bind_cached_phone left normal native screen draws invisible
 // to the touch picker even though both the menu and draw hooks were working.
 touch_note_bindings(context,startSlot,count,changed?replacement.data():views);
 for(UINT i=0;i<count;++i)if(retained[i])retained[i]->Release();
}

static void finish_phone_frame(ID3D11DeviceContext* context){cameraFrameEvidence.present(context,GetTickCount64());native_icon_poll(context);
    std::unique_lock<std::mutex> lock(g_phoneScreenMutex,std::try_to_lock);if(!lock.owns_lock())return;const auto now=GetTickCount64();const auto timing=g_phoneScreens.empty()&&g_independentSurfaces.empty()?StartupPhase{startup_total_ms(),0}:startup_sample(now);const int phase=timing.elapsed;
    cameraNativeBackend.present();international_pages_finish(context,now);copilot_pages_finish(context,now);
    managed_finish_targets(context,phase,now);
    ID3D11Device* currentDevice=nullptr;context->GetDevice(&currentDevice);
    for(auto& screen:g_phoneScreens)if(screen->renderer.device==currentDevice){const int slot=screen->managedOutput?managed_for_known(screen->displayIndex):-1;screen->finish_frame(context,slot>=0?managedPower[slot].phase.load():phase,now);}
    if(currentDevice)currentDevice->Release();
    for(auto& display:g_independentSurfaces){if(now-display->seen>500||!display_active(display->displayIndex))continue;ID3D11Device* device=nullptr;context->GetDevice(&device);const bool same=display->power.renderer.device==device;device->Release();if(!same)continue;
        auto& image=g_prepared[display->displayIndex];std::shared_ptr<const std::vector<uint8_t>> pixels;uint64_t version=0,powerState=0,themeEpoch=0,pageSerial=0;bool pageReady=false;int powerPhase=-99999;
        {std::unique_lock<std::mutex> lock(image.mutex,std::try_to_lock);if(lock.owns_lock()){pixels=image.pixels;version=image.version;powerState=image.powerState;powerPhase=image.powerPhase;themeEpoch=image.themeEpoch;pageSerial=image.page.serial;pageReady=image.pageReady;}}
        if(g_assetsReady&&themeEpoch!=g_mapThemeEpoch.load())continue;
        if(pixels)display->upload(context,*pixels,gpsDisplays[display->displayIndex].markerWidth,version,powerState,powerPhase);
        PhoneScreenPower* latest=nullptr;for(auto& source:g_phoneScreens)if(!source->managedOutput&&source->displayIndex==display_source_index(display->displayIndex)&&source->renderer.device==display->power.renderer.device&&source->captured&&(!latest||source->lastCopy>latest->lastCopy))latest=source.get();
        // A suspended native producer keeps its entire last map image (roads,
        // background and icons together). Never block home/power animations.
        const bool hadImage=display->power.captured;const int slot=managed_for_known(display->displayIndex);ID3D11ShaderResourceView* map=nullptr;auto phoneTiming=timing;PageVisual page;bool waze=false;bool night=g_darkMode.load(),horizontal=false;if(slot>=0){auto d=managed_snapshot(unsigned(slot));phoneTiming={managedPower[slot].phase.load(),managedPower[slot].serial.load()};page=managed_page_kind(d)==PageDevice::Phone?managed_page_visual(unsigned(slot)):PageVisual{};waze=d.style==1;night=selected_dark(d.theme,night);horizontal=d.horizontal;latest=nullptr;std::lock_guard<std::mutex> lock(g_phoneMapInputMutex);auto& source=g_phoneMapInputs[slot];if(source.view&&source.generation==nativeForkSourceGeneration.load()){map=source.view;map->AddRef();}}display->finish(context,latest,phoneTiming.elapsed,phoneTiming.state,now,night,map,horizontal,page,waze);if(slot>=0&&pageReady&&display->uploaded==version&&display->power.captured&&display->layerPowerState==phoneTiming.state&&display->composed==display->uploaded)managed_page_presented(unsigned(slot),pageSerial,now);if(map)map->Release();mapGpuAudit.capture(context,display->canvas,3);
        if(!hadImage&&display->power.captured)log(SCS_LOG_TYPE_message,"[PhonePower/v10] First front frame composed at Present: display=%llu phase=%d ignition=%d native-base=%u",(unsigned long long)display->displayIndex,phase,displayIgnition.load(),unsigned(latest!=nullptr));
    }
}

static void STDMETHODCALLTYPE hook_UpdateSubresource(ID3D11DeviceContext* context,ID3D11Resource* destination,UINT subresource,const D3D11_BOX* box,const void* source,UINT pitch,UINT depthPitch){
    if(!phoneInternalWork)TouchComposition::invalidate(destination);
    // Tag the exact uploaded marker before ATS paints its live map over it.
    // Buffer updates have zero row pitch and skip this path entirely.
    if(!renderStopping&&destination&&source&&pitch&&subresource==0&&(!box||(box->left==0&&box->top==0&&box->right>=4&&box->bottom>=1))){
        ID3D11Texture2D* texture=nullptr;
        if(SUCCEEDED(destination->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&texture)))){
            D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);if(!phoneInternalWork&&!subresource&&(!box||!box->left&&!box->top&&!box->front&&box->right==desc.Width&&box->bottom==desc.Height&&box->back==1))native_icon_identify(texture,desc,source,pitch);auto* bridge=bridge_for_desc(&desc);D3D11_SUBRESOURCE_DATA data{};data.pSysMem=source;data.SysMemPitch=pitch;
            if(bridge&&marker_signature(bridge,&desc,&data)){std::unique_lock<std::mutex> lock(g_d3dMutex,std::try_to_lock);if(lock.owns_lock()&&bridge->markerTexture!=texture){reset_marker_locked(*bridge);bridge->markerTexture=texture;texture->AddRef();}}
            texture->Release();
        }
    }
    (context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED&&g_origDeferredUpdate?g_origDeferredUpdate:g_origUpdateSubresource)(context,destination,subresource,box,source,pitch,depthPitch);
}

static void* g_addrPresent=nullptr;
static HRESULT (STDMETHODCALLTYPE* g_origPresent)(IDXGISwapChain*,UINT,UINT)=nullptr;
static SettingsMenuRenderer g_settingsRenderer;
static ID3D11Texture2D* g_settingsTexture=nullptr;
static ID3D11ShaderResourceView* g_settingsView=nullptr;
static ID3D11Device* g_settingsDevice=nullptr;
static void update_solar_theme();
static ID3D11Texture2D* g_cursorTexture=nullptr;static ID3D11ShaderResourceView* g_cursorView=nullptr;
static uint64_t g_menuUploaded=0;
static void release_settings_gpu(){g_menuUploaded=0;SettingsMenuRenderer::drop(g_cursorView);SettingsMenuRenderer::drop(g_cursorTexture);SettingsMenuRenderer::drop(g_settingsView);SettingsMenuRenderer::drop(g_settingsTexture);SettingsMenuRenderer::drop(g_settingsDevice);g_settingsRenderer.stop();}
static ID3D11Device* g_presentDevice=nullptr;
static std::filesystem::path g_graphicsWatchPath;
static bool prepare_present_device(IDXGISwapChain* chain){
 ID3D11Device* device=nullptr;if(FAILED(chain->GetDevice(__uuidof(ID3D11Device),(void**)&device)))return false;
 const auto status=device->GetDeviceRemovedReason();if(FAILED(status)){renderDeviceLost=true;device->Release();return false;}
 if(device!=g_presentDevice){
  // A recovered game device must never consume retained resources or command
  // lists from the preceding device. This path runs only on a device change.
  std::unique_lock<std::mutex> screens(g_phoneScreenMutex,std::try_to_lock),bridges(g_d3dMutex,std::try_to_lock);
  if(!screens.owns_lock()||!bridges.owns_lock()){device->Release();return false;}
  g_independentSurfaces.erase(std::remove_if(g_independentSurfaces.begin(),g_independentSurfaces.end(),[&](const auto& item){return item->power.renderer.device!=device;}),g_independentSurfaces.end());
  g_phoneScreens.erase(std::remove_if(g_phoneScreens.begin(),g_phoneScreens.end(),[&](const auto& item){return item->renderer.device!=device;}),g_phoneScreens.end());
  for(auto* bridge:all_bridges()){
   ID3D11Device* owner=nullptr;if(bridge->liveTexture){bridge->liveTexture->GetDevice(&owner);if(owner!=device)release_live_locked(*bridge);owner->Release();}
   if(bridge->markerTexture){bridge->markerTexture->GetDevice(&owner);if(owner!=device)reset_marker_locked(*bridge);owner->Release();}
  }
  if(g_presentDevice)g_presentDevice->Release();g_presentDevice=device;device->AddRef();
 }
 renderDeviceLost=false;device->Release();return true;
}
static HRESULT forward_present(IDXGISwapChain* chain,UINT interval,UINT flags){
 render_stage(RenderStage::NativePresent);bettergps_diag::Scope waitTiming(bettergps_diag::PresentWait);
 const auto result=g_origPresent(chain,interval,flags);render_present_result(result);return result;
}
static void graphics_watchdog(){
 static uint64_t recorded[8]{};static unsigned lines=0;
 if(g_graphicsWatchPath.empty()||lines>=8)return;
 for(size_t index=0;index<8&&lines<8;++index){
  const auto& trace=renderTraces[index];const auto ticket=trace.ticket.load(),since=trace.since.load();const auto stage=trace.stage.load();const auto thread=trace.thread.load();
  if(!thread||stage==RenderStage::Idle||ticket==recorded[index]||GetTickCount64()-since<5000||ticket!=trace.ticket.load())continue;
  recorded[index]=ticket;++lines;std::error_code error;const auto size=std::filesystem::file_size(g_graphicsWatchPath,error);
  std::ofstream out(g_graphicsWatchPath,!error&&size<32768?std::ios::app:std::ios::trunc);
  out<<"tick="<<GetTickCount64()<<" call="<<ticket<<" thread="<<thread<<" stage="<<unsigned(stage)<<" stalled_ms="<<GetTickCount64()-since<<" foreground="<<render_window_active(renderGameWindow.load())<<" occluded="<<renderOccluded.load()<<" lost="<<renderDeviceLost.load()<<" stages=1:preflight,2:phone,3:menu,4:engine-present,5:binding,6:icons,7:copy\n";
 }
}
static HRESULT STDMETHODCALLTYPE hook_Present(IDXGISwapChain* chain,UINT interval,UINT flags){
    static thread_local bool nested=false;
    if(renderStopping||!render_gameplay_allowed()||nested||phoneInternalWork)return g_origPresent(chain,interval,flags);
    struct Recursion {bool& flag;Recursion(bool& value):flag(value){flag=true;}~Recursion(){flag=false;}} recursion(nested);
    // A DXGI visibility probe must remain a probe, with zero BetterGPS GPU work.
    if(flags&DXGI_PRESENT_TEST){const auto result=g_origPresent(chain,interval,flags);if(!renderActivityEnabled||chain==renderMainChain.load())render_present_result(result);return result;}
    DXGI_SWAP_CHAIN_DESC observed{};
    if(renderActivityEnabled){
      if(FAILED(chain->GetDesc(&observed)))return g_origPresent(chain,interval,flags);
      const auto known=renderGameWindow.load();
      if(!known&&render_window_candidate(observed.OutputWindow))renderGameWindow=observed.OutputWindow;
      if(observed.OutputWindow!=renderGameWindow.load())return g_origPresent(chain,interval,flags);
      renderMainChain=chain;
    }
    RenderCall call;
    if(renderActivityEnabled){
      render_set_foreground(render_window_active(observed.OutputWindow));
      render_set_visible(IsWindowVisible(observed.OutputWindow)&&!IsIconic(observed.OutputWindow));
      if(!renderVisible||renderOccluded)return forward_present(chain,interval,flags);
      if(!prepare_present_device(chain))return forward_present(chain,interval,flags);
    }

    if(!(flags&DXGI_PRESENT_TEST))bettergps_diag::Frame(chain,GetForegroundWindow()==settingsWindow,settingsOpen.load());
    {bettergps_diag::Scope presentTiming(bettergps_diag::OverlayPresent);
    if(!(flags&DXGI_PRESENT_TEST)){DXGI_SWAP_CHAIN_DESC desc{};chain->GetDesc(&desc);if(desc.OutputWindow){settings_attach(desc.OutputWindow);settingsTouchPointer=&touch_pointer;settingsTouchCancel=&touch_cancel;settings_input();}
        render_stage(RenderStage::Phone);
        // Retain the completed phone image before Present; no readback or allocation.
        if(!g_phoneScreens.empty()||!g_independentSurfaces.empty()||nativeForkHorizontalLastUse.load()!=0){ID3D11Device* device=nullptr;if(SUCCEEDED(chain->GetDevice(__uuidof(ID3D11Device),(void**)&device))){ID3D11DeviceContext* context=nullptr;device->GetImmediateContext(&context);finish_phone_frame(context);touchPicker.present(context,GetTickCount64());mapGpuAudit.poll(context);context->Release();device->Release();}}
        // Settings revisions are observed by the CPU worker; Present never waits for settings I/O.

        if(settingsOpen||cursorOnlyActive){ID3D11Device* gd=nullptr;if(SUCCEEDED(chain->GetDevice(__uuidof(ID3D11Device),(void**)&gd))){ID3D11DeviceContext* gc=nullptr;gd->GetImmediateContext(&gc);touchGeometry.poll(gc);ID3D11Texture2D* buffer=nullptr;if(SUCCEEDED(chain->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&buffer))){D3D11_TEXTURE2D_DESC bd{};buffer->GetDesc(&bd);RECT client{};GetClientRect(desc.OutputWindow,&client);touchCpuPicker.present(gc,bd.Width,bd.Height,unsigned(client.right),unsigned(client.bottom),GetTickCount64());buffer->Release();}gc->Release();gd->Release();}}else touchCpuPicker.stop();
        render_stage(RenderStage::Menu);
        if(settingsOpen.load()||cursorOnlyActive.load()){ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;ID3D11Texture2D* target=nullptr;if(SUCCEEDED(chain->GetDevice(__uuidof(ID3D11Device),(void**)&device))&&SUCCEEDED(chain->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&target))){device->GetImmediateContext(&context);D3D11_TEXTURE2D_DESC td{};target->GetDesc(&td);RECT client{};GetClientRect(desc.OutputWindow,&client);menuDock.layout(float(client.right),float(client.bottom));const auto panel=menuDock.panel();const float scale=menuDock.scale*td.Width/std::max<LONG>(1,client.right),left=panel.left*float(td.Width)/std::max<LONG>(1,client.right),top=panel.top*float(td.Height)/std::max<LONG>(1,client.bottom);
            float ratioX=float(client.right)/td.Width,ratioY=float(client.bottom)/td.Height;settingsOffsetX=left*ratioX;settingsOffsetY=top*ratioY;settingsScale=scale*ratioX;
            if(g_settingsDevice!=device){release_settings_gpu();g_settingsDevice=device;device->AddRef();}
            if(settingsOpen&&!g_settingsTexture){D3D11_TEXTURE2D_DESC d{};d.Width=1920;d.Height=1440;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DYNAMIC;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;if(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&g_settingsTexture)))device->CreateShaderResourceView(g_settingsTexture,nullptr,&g_settingsView);}
            if(settingsOpen&&g_settingsView){std::shared_ptr<const std::vector<uint8_t>> ready;uint64_t version=0;{std::unique_lock<std::mutex> lock(g_preparedMenu.mutex,std::try_to_lock);if(lock.owns_lock()){ready=g_preparedMenu.pixels;version=g_preparedMenu.version;}}if(!g_cpuRunning.load()){auto pixels=std::make_shared<std::vector<uint8_t>>();settings_render(*pixels,2);ready=pixels;version=g_menuUploaded+1;}if(ready&&version!=g_menuUploaded){bettergps_diag::Scope menuUploadTiming(bettergps_diag::MenuUpload);D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context->Map(g_settingsTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){for(unsigned y=0;y<1440;++y)memcpy((uint8_t*)mapped.pData+y*mapped.RowPitch,ready->data()+size_t(y)*1920*4,1920*4);context->Unmap(g_settingsTexture,0);g_menuUploaded=version;}}if(ready){if(settingsExpanded)g_settingsRenderer.render(context,g_settingsView,target,left,top,960*scale,720*scale,g_origPSSetSRV);menuDockGpu.draw(context,target,g_settingsRenderer,g_origPSSetSRV,1/ratioX,1/ratioY);}}
 if(!g_cursorView){std::array<uint8_t,20*34*4> cursor{};for(unsigned y=0;y<34;++y)for(unsigned x=0;x<=y/2;++x){auto* p=&cursor[(y*20+x)*4];p[0]=p[1]=p[2]=(x<2||x+2>y/2)?35:255;p[3]=255;}D3D11_TEXTURE2D_DESC d{};d.Width=20;d.Height=34;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=cursor.data();initial.SysMemPitch=80;if(SUCCEEDED(device->CreateTexture2D(&d,&initial,&g_cursorTexture)))device->CreateShaderResourceView(g_cursorTexture,nullptr,&g_cursorView);}
 if(g_cursorView)g_settingsRenderer.render(context,g_cursorView,target,menuCursorX.load()/ratioX,menuCursorY.load()/ratioY,10*scale,17*scale,g_origPSSetSRV);
            context->Release();
        }if(target)target->Release();if(device)device->Release();}}
    }return forward_present(chain,interval,flags);
}

static bool install_d3d_hooks() {
    bettergps::StartupTrace trace(g_log,"graphics-install");
    renderStopping=false;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL level{};
    HWND dummy=CreateWindowExW(0,L"STATIC",L"BetterGPS hook discovery",WS_POPUP,0,0,8,8,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    DXGI_SWAP_CHAIN_DESC sd{};sd.BufferDesc.Width=sd.BufferDesc.Height=8;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=1;sd.OutputWindow=dummy;sd.Windowed=TRUE;
    IDXGISwapChain* chain=nullptr;
    HRESULT hr = trace.measure("create-hardware-device",[&]{return D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&chain,&device,&level,&context);});
    if (FAILED(hr)) {
        hr = trace.measure("create-warp-fallback",[&]{return D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&chain,&device,&level,&context);});
    }
    if (FAILED(hr) || !device || !context || !chain) {
        log(SCS_LOG_TYPE_error, "Could not create dummy D3D11 device for hooks (hr=0x%08X)", hr);
        if (context) context->Release();
        if (device) device->Release();
        if(chain)chain->Release();if(dummy)DestroyWindow(dummy);
        return false;
    }

    void** dv = *reinterpret_cast<void***>(device);
    void** cv = *reinterpret_cast<void***>(context);
    g_addrCreateTexture2D = dv[5];
    g_addrCreateSRV = dv[7];
    g_addrPSSetSRV = cv[8];
    g_addrUpdateSubresource=cv[48];
    g_addrSetTargets=cv[33];g_addrSetTargetsUav=cv[34];g_addrFinishCommands=cv[114];g_addrCopyResource=cv[47];g_addrCopyRegion=cv[46];g_addrClearRTV=cv[50];

    g_addrPresent=(*reinterpret_cast<void***>(chain))[8];
    // No callback is enabled until every original/trampoline is ready. Keep
    // standalone touch/input installers immediate; this owner commits exactly
    // its new graphics/input cohort once, before releasing discovery objects.
    std::vector<void*> preparedHooks;
    const bool touchReady=trace.measure("prepare-touch-hooks",[&]{return touch_hooks_install(device,context,&preparedHooks);});log(touchReady?SCS_LOG_TYPE_message:SCS_LOG_TYPE_warning,"[Touch] Read-only native geometry observer %s; diagnostic only; GPU replay remains blocked in production",touchReady?"prepared":"unavailable");
    bool ok = true;
    MH_STATUS s=MH_OK;
    {bettergps::StartupTrace::Phase phase(trace,"prepare-core-hooks");
    s = MH_CreateHook(g_addrCreateTexture2D,
        &hook_CreateTexture2D, reinterpret_cast<void**>(&g_origCreateTexture2D));
    if (s != MH_OK && s != MH_ERROR_ALREADY_CREATED) ok = false;
    if (ok) {
        s = MH_CreateHook(g_addrCreateSRV,
            &hook_CreateShaderResourceView, reinterpret_cast<void**>(&g_origCreateSRV));
        if (s != MH_OK && s != MH_ERROR_ALREADY_CREATED) ok = false;
    }
    if (ok) {
        s = MH_CreateHook(g_addrPSSetSRV,
            &hook_PSSetShaderResources, reinterpret_cast<void**>(&g_origPSSetSRV));
        if (s != MH_OK && s != MH_ERROR_ALREADY_CREATED) ok = false;
    }

    if (ok) {
        s=MH_CreateHook(g_addrUpdateSubresource,&hook_UpdateSubresource,reinterpret_cast<void**>(&g_origUpdateSubresource));
        if(s!=MH_OK&&s!=MH_ERROR_ALREADY_CREATED)ok=false;
    }
    if(ok){s=MH_CreateHook(g_addrSetTargets,&hook_SetTargets,reinterpret_cast<void**>(&g_origSetTargets));if(s!=MH_OK&&s!=MH_ERROR_ALREADY_CREATED)ok=false;}
    if(ok){s=MH_CreateHook(g_addrSetTargetsUav,&hook_SetTargetsUav,reinterpret_cast<void**>(&g_origSetTargetsUav));if(s!=MH_OK&&s!=MH_ERROR_ALREADY_CREATED)ok=false;}
    if(ok){s=MH_CreateHook(g_addrFinishCommands,&hook_FinishCommands,reinterpret_cast<void**>(&g_origFinishCommands));if(s!=MH_OK&&s!=MH_ERROR_ALREADY_CREATED)ok=false;}
    if(ok){s=MH_CreateHook(g_addrClearRTV,&hook_ClearRTV,reinterpret_cast<void**>(&g_origClearRTV));if(s!=MH_OK&&s!=MH_ERROR_ALREADY_CREATED)ok=false;}
    if(ok){s=MH_CreateHook(g_addrCopyResource,&hook_CopyResource,reinterpret_cast<void**>(&g_origCopyResource));if(s!=MH_OK&&s!=MH_ERROR_ALREADY_CREATED)ok=false;}
    if(ok){s=MH_CreateHook(g_addrCopyRegion,&hook_CopyRegion,reinterpret_cast<void**>(&g_origCopyRegion));if(s!=MH_OK&&s!=MH_ERROR_ALREADY_CREATED)ok=false;}
    if (ok) {
        preparedHooks.insert(preparedHooks.end(),{g_addrCreateTexture2D,g_addrCreateSRV,g_addrPSSetSRV,g_addrUpdateSubresource,g_addrSetTargets,g_addrSetTargetsUav,g_addrFinishCommands,g_addrClearRTV,g_addrCopyResource,g_addrCopyRegion});
    }
    }

    if(ok){ID3D11DeviceContext* deferred=nullptr;if(SUCCEEDED(device->CreateDeferredContext(0,&deferred))){auto** methods=*reinterpret_cast<void***>(deferred);const unsigned slots[]={8,48,50,33,34,114};void* immediate[]={g_addrPSSetSRV,g_addrUpdateSubresource,g_addrClearRTV,g_addrSetTargets,g_addrSetTargetsUav,g_addrFinishCommands};void* callbacks[]={(void*)&hook_PSSetShaderResources,(void*)&hook_UpdateSubresource,(void*)&hook_ClearRTV,(void*)&hook_SetTargets,(void*)&hook_SetTargetsUav,(void*)&hook_FinishCommands};void** originals[]={(void**)&g_origDeferredPSSetSRV,(void**)&g_origDeferredUpdate,(void**)&g_origDeferredClear,(void**)&g_origDeferredTargets,(void**)&g_origDeferredTargetsUav,(void**)&g_origDeferredFinish};for(unsigned i=0;i<6;++i)if(methods[slots[i]]!=immediate[i]){auto address=methods[slots[i]];auto status=MH_CreateHook(address,callbacks[i],originals[i]);if(status!=MH_OK&&status!=MH_ERROR_ALREADY_CREATED){ok=false;break;}g_deferredHookAddresses[i]=address;preparedHooks.push_back(address);}deferred->Release();}}
    if(ok)ok=trace.measure("prepare-mouse-hooks",[&]{return settings_install_mouse_hooks(&preparedHooks);});
    if(ok){s=MH_CreateHook(g_addrPresent,&hook_Present,reinterpret_cast<void**>(&g_origPresent));if(s!=MH_OK&&s!=MH_ERROR_ALREADY_CREATED)ok=false;else preparedHooks.push_back(g_addrPresent);}
    if(ok)ok=trace.measure("activate-graphics-input-cohort",[&]{return enable_hook_batch(preparedHooks)==MH_OK;});
    chain->Release();DestroyWindow(dummy);
    context->Release();
    device->Release();
    g_hooksReady = ok;
    log(ok ? SCS_LOG_TYPE_message : SCS_LOG_TYPE_error,
        "TripInfo D3D hooks %s (CreateTexture2D=%p CreateSRV=%p PSSetSRV=%p)",
        ok ? "READY" : "FAILED", g_addrCreateTexture2D, g_addrCreateSRV, g_addrPSSetSRV);
    return ok;
}

static void uninstall_d3d_hooks() {
    renderStopping=true;stop_cpu_worker();MH_DisableHook(MH_ALL_HOOKS);shutdown_note("hooks-disabled");renderActivityEnabled=false;renderActivityWake=nullptr;renderGameplaySuspend=nullptr;renderGameWindow=nullptr;renderMainChain=nullptr;if(g_presentDevice){g_presentDevice->Release();g_presentDevice=nullptr;}touch_hooks_stop();player_actor_stop();native_fork_stop();live_map_stop();settings_detach();menuDockGpu.stop();menuDock.save();overlay_save();display_power_save();home_save();display_policy_save();map_views_save();if(g_addrPresent){MH_DisableHook(g_addrPresent);MH_RemoveHook(g_addrPresent);}release_settings_gpu();
    if(g_addrCopyResource){MH_DisableHook(g_addrCopyResource);MH_RemoveHook(g_addrCopyResource);}if(g_addrCopyRegion){MH_DisableHook(g_addrCopyRegion);MH_RemoveHook(g_addrCopyRegion);}
    for(auto& address:g_deferredHookAddresses)if(address){MH_DisableHook(address);MH_RemoveHook(address);address=nullptr;}
    for(auto address:{g_addrSetTargets,g_addrSetTargetsUav,g_addrFinishCommands})if(address){MH_DisableHook(address);MH_RemoveHook(address);}
    if(g_addrClearRTV){MH_DisableHook(g_addrClearRTV);MH_RemoveHook(g_addrClearRTV);}
    if(g_addrUpdateSubresource){MH_DisableHook(g_addrUpdateSubresource);MH_RemoveHook(g_addrUpdateSubresource);}
    if (g_addrPSSetSRV) { MH_DisableHook(g_addrPSSetSRV); MH_RemoveHook(g_addrPSSetSRV); }
    if (g_addrCreateSRV) { MH_DisableHook(g_addrCreateSRV); MH_RemoveHook(g_addrCreateSRV); }
    if (g_addrCreateTexture2D) { MH_DisableHook(g_addrCreateTexture2D); MH_RemoveHook(g_addrCreateTexture2D); }
    {std::lock_guard<std::mutex> screenLock(g_phoneScreenMutex);g_independentSurfaces.clear();g_phoneScreens.clear();managed_gpu_stop();{std::lock_guard<std::mutex> lock(g_horizontalMapMutex);SettingsMenuRenderer::drop(g_horizontalMapInput);g_horizontalMapGeneration=0;}mapGpuAudit.stop();}
    std::lock_guard<std::mutex> lock(g_d3dMutex);
    for(auto& retry:g_markerRetries)retry.view->Release();g_markerRetries.clear();
    reset_marker_locked(g_clusterIntroBridge);release_live_locked(g_clusterIntroBridge);clusterIntroUploaded=0;
    reset_marker_locked(g_mackClusterBridge);release_live_locked(g_mackClusterBridge);mackClusterUploaded=0;
    reset_marker_locked(g_vnlClusterBridge);release_live_locked(g_vnlClusterBridge);vnlClusterUploaded=0;
    reset_marker_locked(g_vnlBootBridge);release_live_locked(g_vnlBootBridge);
    reset_marker_locked(g_volvo18ClusterBridge);release_live_locked(g_volvo18ClusterBridge);volvo18ClusterUploaded=0;
    reset_marker_locked(g_volvo18BootBridge);release_live_locked(g_volvo18BootBridge);
    reset_marker_locked(g_volvo25ClusterBridge);release_live_locked(g_volvo25ClusterBridge);volvo25ClusterUploaded=0;
    reset_marker_locked(g_icucClusterBridge);release_live_locked(g_icucClusterBridge);icucClusterUploaded=0;
    reset_marker_locked(g_icuc5700ClusterBridge);release_live_locked(g_icuc5700ClusterBridge);icuc5700ClusterUploaded=0;
    reset_marker_locked(g_intlClusterBridge);release_live_locked(g_intlClusterBridge);intlClusterUploaded=0;
    reset_marker_locked(g_icc5ClusterBridge);release_live_locked(g_icc5ClusterBridge);icc5ClusterUploaded=0;
    reset_marker_locked(g_icc5BootBridge);release_live_locked(g_icc5BootBridge);
    reset_marker_locked(g_volvo25BootBridge);release_live_locked(g_volvo25BootBridge);
    reset_marker_locked(g_truckBootBridge);release_live_locked(g_truckBootBridge);
    reset_marker_locked(g_mackBootBridge);release_live_locked(g_mackBootBridge);
    g_f150InfoFade.stop();g_f150CenterFade.stop();reset_marker_locked(g_f150InfoBridge);release_live_locked(g_f150InfoBridge);if(g_f150InfoBlackView){g_f150InfoBlackView->Release();g_f150InfoBlackView=nullptr;}if(g_f150InfoClearView){g_f150InfoClearView->Release();g_f150InfoClearView=nullptr;}
    reset_marker_locked(g_f150Bridge);release_live_locked(g_f150Bridge);if(g_f150BlackView){g_f150BlackView->Release();g_f150BlackView=nullptr;}
    if(g_f150ClearView){g_f150ClearView->Release();g_f150ClearView=nullptr;}
    reset_marker_locked(g_distBridge);
    reset_marker_locked(g_timeBridge);
    reset_marker_locked(g_clockBridge);
    reset_marker_locked(g_speedBridge);
    reset_marker_locked(g_guidanceBridge);
    reset_marker_locked(g_skinBridge);
    reset_marker_locked(g_f150SourceBridge);release_live_locked(g_f150SourceBridge);
    reset_marker_locked(g_f150MapBridge);release_live_locked(g_f150MapBridge);
    reset_marker_locked(g_mapBridge);
    reset_marker_locked(g_mapBackgroundBridge);
    release_live_locked(g_distBridge);
    release_live_locked(g_timeBridge);
    release_live_locked(g_clockBridge);
    release_live_locked(g_speedBridge);
    release_live_locked(g_guidanceBridge);
    release_live_locked(g_skinBridge);
    release_live_locked(g_mapBridge);
    release_live_locked(g_mapBackgroundBridge);
    for(auto& b:g_managedBridges){reset_marker_locked(*b);release_live_locked(*b);}for(auto& b:g_displayBridges){reset_marker_locked(*b);release_live_locked(*b);}for(auto& b:g_surfaceBridges){reset_marker_locked(*b);release_live_locked(*b);}
    g_hooksReady = false;shutdown_note("graphics-released");
}

SCSAPI_VOID camera_sdk_cb(const scs_string_t,const scs_u32_t,const scs_value_t* const value,const scs_context_t context){
 const auto kind=reinterpret_cast<uintptr_t>(context);if(kind==0)bettergps_camera::reverseGear=value&&value->type==SCS_VALUE_TYPE_s32&&value->value_s32.value<0;
 else if(kind==1)bettergps_camera::leftIndicator=value&&value->type==SCS_VALUE_TYPE_bool&&value->value_bool.value;
 else bettergps_camera::rightIndicator=value&&value->type==SCS_VALUE_TYPE_bool&&value->value_bool.value;
}
SCSAPI_VOID truck_speed_cb(
    const scs_string_t,
    const scs_u32_t,
    const scs_value_t* value,
    const scs_context_t)
{
    if (!value || value->type != SCS_VALUE_TYPE_float) {
        g_freshDrivingSpeed=false;g_haveTruckSpeed.store(false, std::memory_order_release);
        g_speedDirty.store(true, std::memory_order_release);
        return;
    }

    const float speed = value->value_float.value;
    g_freshDrivingSpeed=std::isfinite(speed);
    const float previous = g_truckSpeedMps.exchange(speed, std::memory_order_acq_rel);
    const bool had = g_haveTruckSpeed.exchange(true, std::memory_order_acq_rel);

    const float limit = g_speedLimitMps.load(std::memory_order_acquire);
    const bool haveLimit = g_haveSpeedLimit.load(std::memory_order_acquire);
    const bool oldOver = had && haveLimit && std::isfinite(previous) &&
                         std::isfinite(limit) && limit > 0.1f &&
                         std::fabs(previous) > (limit + 0.05f);
    const bool newOver = haveLimit && std::isfinite(speed) &&
                         std::isfinite(limit) && limit > 0.1f &&
                         std::fabs(speed) > (limit + 0.05f);

    if (!had || speed_mph_from_mps(previous) != speed_mph_from_mps(speed) || oldOver != newOver)
        g_speedDirty.store(true, std::memory_order_release);
}

SCSAPI_VOID speed_limit_cb(
    const scs_string_t,
    const scs_u32_t,
    const scs_value_t* value,
    const scs_context_t)
{
    if (!value || value->type != SCS_VALUE_TYPE_float) {
        g_haveSpeedLimit.store(false, std::memory_order_release);
        g_speedDirty.store(true, std::memory_order_release);
        return;
    }

    const float limit = value->value_float.value;
    const float previous = g_speedLimitMps.exchange(limit, std::memory_order_acq_rel);
    const bool had = g_haveSpeedLimit.exchange(true, std::memory_order_acq_rel);
    if (!had || !std::isfinite(previous) || !std::isfinite(limit) ||
        std::fabs(previous - limit) > 0.01f)
        g_speedDirty.store(true, std::memory_order_release);
}

SCSAPI_VOID game_time_cb(
    const scs_string_t,
    const scs_u32_t,
    const scs_value_t* value,
    const scs_context_t)
{
    if (!value || value->type != SCS_VALUE_TYPE_u32) {
        g_haveGameTime.store(false, std::memory_order_release);
        g_distDirty.store(true, std::memory_order_release);
        g_clockDirty.store(true, std::memory_order_release);
        return;
    }

    const uint32_t minutes = value->value_u32.value;
    const uint32_t previous =
        g_gameTimeMinutes.exchange(minutes, std::memory_order_acq_rel);
    g_haveGameTime.store(true, std::memory_order_release);

    if (minutes != previous) {
        g_distDirty.store(true, std::memory_order_release);
        g_clockDirty.store(true, std::memory_order_release);
    }

    if (!g_loggedFirstGameTime) {
        g_loggedFirstGameTime = true;
        log(SCS_LOG_TYPE_message,
            "Telemetry game.time received: %u min -> arrival %s",
            static_cast<unsigned>(minutes), current_arrival_text().c_str());
    }
}

SCSAPI_VOID multiplayer_time_offset_cb(
    const scs_string_t,
    const scs_u32_t,
    const scs_value_t* value,
    const scs_context_t)
{
    if (!value || value->type != SCS_VALUE_TYPE_s32) {
        g_multiplayerTimeOffset.store(0, std::memory_order_release);
        g_distDirty.store(true, std::memory_order_release);
        g_clockDirty.store(true, std::memory_order_release);
        return;
    }

    const int32_t offset = value->value_s32.value;
    const int32_t previous =
        g_multiplayerTimeOffset.exchange(offset, std::memory_order_acq_rel);
    if (offset != previous) {
        g_distDirty.store(true, std::memory_order_release);
        g_clockDirty.store(true, std::memory_order_release);
    }
}

SCSAPI_VOID nav_distance_cb(
    const scs_string_t,
    const scs_u32_t,
    const scs_value_t* value,
    const scs_context_t)
{
    if (!value || value->type != SCS_VALUE_TYPE_float) {
        g_haveNavDistance.store(false, std::memory_order_release);
        g_distDirty.store(true, std::memory_order_release);
        return;
    }

    const float m = value->value_float.value;
    g_navDistanceM.store(m, std::memory_order_release);
    g_haveNavDistance.store(true, std::memory_order_release);
    const int tenths = static_cast<int>(std::lround((m / 1609.344f) * 10.0f));
    const int previous = g_navDistanceTenths.exchange(tenths, std::memory_order_acq_rel);
    if (tenths != previous) g_distDirty.store(true, std::memory_order_release);

    if (!g_loggedFirstDistance) {
        g_loggedFirstDistance = true;
        log(SCS_LOG_TYPE_message,
            "Telemetry navigation distance received: %.1f m (%.1f mi)",
            m, m / 1609.344f);
    }
}

SCSAPI_VOID nav_time_cb(
    const scs_string_t,
    const scs_u32_t,
    const scs_value_t* value,
    const scs_context_t)
{
    if (!value || value->type != SCS_VALUE_TYPE_float) {
        g_haveNavTime.store(false, std::memory_order_release);
        g_timeDirty.store(true, std::memory_order_release);
        return;
    }

    const float seconds = value->value_float.value;
    g_navTimeSec.store(seconds, std::memory_order_release);
    g_haveNavTime.store(true, std::memory_order_release);
    const int minutes = (std::isfinite(seconds) && seconds >= 0.0f)
        ? static_cast<int>(std::ceil(seconds / 60.0f)) : -1;
    const int previous = g_navTimeMinutes.exchange(minutes, std::memory_order_acq_rel);
    if (minutes != previous) {
        g_timeDirty.store(true, std::memory_order_release);
        g_distDirty.store(true, std::memory_order_release);
    }

    if (!g_loggedFirstTime) {
        g_loggedFirstTime = true;
        log(SCS_LOG_TYPE_message,
            "Telemetry navigation time received: %.1f s -> %s",
            seconds, current_time_text().c_str());
    }
}

static void set_trip_presentation(bool trip) {
    if(g_tripPresent.exchange(trip,std::memory_order_acq_rel)==trip)return;
    g_skinDirty.store(true,std::memory_order_release);g_distDirty.store(true,std::memory_order_release);g_timeDirty.store(true,std::memory_order_release);g_guidanceDirty.store(true,std::memory_order_release);
}

SCSAPI_VOID world_position_cb(const scs_string_t,const scs_u32_t,const scs_value_t* value,const scs_context_t) {
    if(!value||value->type!=SCS_VALUE_TYPE_dplacement){g_freshDrivingPose=false;g_haveWorldPosition=false;if(g_haveCompassHeading.exchange(false))g_skinDirty=true;return;}
    const auto heading=value->value_dplacement.orientation.heading;
    if(std::isfinite(heading)){const auto previous=g_compassHeading.exchange(static_cast<float>(compass_turn(heading)));if(!g_haveCompassHeading.exchange(true)||std::abs(previous-g_compassHeading.load())>.00001f)g_skinDirty=true;}
    else if(g_haveCompassHeading.exchange(false))g_skinDirty=true;
    const auto& p=value->value_dplacement.position;if(!std::isfinite(p.x)||!std::isfinite(p.z)){g_freshDrivingPose=false;g_haveWorldPosition=false;return;}
    g_freshDrivingPose=std::isfinite(heading);g_worldX=p.x;g_worldZ=p.z;g_haveWorldPosition=true;
}
static void initialize_native_theme(bool dark){g_darkMode.store(dark);g_requestedMapDark=dark;}
static void publish_dark_presentation(bool dark){
    settingsMapThemePending=false;if(g_darkMode.exchange(dark)==dark)return;
    ++g_mapThemeEpoch;g_skinDirty=true;g_distDirty=true;g_speedDirty=true;g_clockDirty=true;g_mapBackgroundDirty=true;g_themeSaveDirty=true;
}
static void set_dark_presentation(bool dark){
    g_requestedMapDark=dark;
    // Production commits on the game thread, together with the native palette.
    // Standalone renderer fixtures have no engine unit lifecycle.
    if(!g_assetsReady)publish_dark_presentation(dark);
}
static void refresh_solar_settings(){
    static uint64_t generation=~uint64_t(0);const auto directory=bettergps_navigation::ShieldDirectory();const auto revision=shield_revision(directory);
    if(revision!=generation){generation=revision;std::atomic_store(&g_solarSettings,std::make_shared<const SolarSettings>(solar_settings(directory)));}
    if(g_themeSaveDirty.exchange(false)&&!native_map_save(g_nativeThemeStatePath,g_darkMode.load()))log(SCS_LOG_TYPE_warning,"Could not save native map theme state");
}
static void update_solar_theme(){
    static bool initialized=false;static int lastMode=-1;static int64_t lastMinutes=-1;
    const auto config=overlay_snapshot();if(g_solarReset.exchange(false)||config.theme!=lastMode){initialized=false;lastMinutes=-1;}lastMode=config.theme;
    
    if(!g_haveGameTime.load()||!g_haveWorldPosition.load()){initialized=false;lastMinutes=-1;return;}
    const int64_t minutes=int64_t(g_gameTimeMinutes.load())+g_multiplayerTimeOffset.load();
    // Loading an older save, sleeping, ferries and teleports must recompute the
    // actual sun state instead of inheriting the previous twilight hysteresis.
    if(lastMinutes>=0&&(minutes<lastMinutes||minutes-lastMinutes>5))initialized=false;lastMinutes=minutes;
    const auto settings=std::atomic_load(&g_solarSettings);
    const auto elevation=solar_elevation(minutes,g_haveWorldPosition.load()?g_worldX.load():0,g_haveWorldPosition.load()?g_worldZ.load():0,*settings);
    set_dark_presentation(solar_night(g_darkMode.load(),initialized,elevation,settings->threshold));initialized=true;
}
SCSAPI_VOID on_theme_session(const scs_event_t event,const void* const info,scs_context_t){
    if(event==SCS_TELEMETRY_EVENT_configuration&&info){auto* c=static_cast<const scs_telemetry_configuration_t*>(info);zonar_job_configuration(c);if(c->id&&std::string(c->id)=="truck"){t680Gauges.reset();bettergps_camera::reverseGear=bettergps_camera::leftIndicator=bettergps_camera::rightIndicator=false;bettergps_camera::cameras.reset(vehicleStart.epoch.load());}if(c->id&&std::string(c->id)=="truck"){std::string id,name,brand;for(auto* a=c->attributes;a&&a->name;++a)if(a->value.type==SCS_VALUE_TYPE_string&&a->value.value_string.value){std::string key=a->name,value=a->value.value_string.value;if(key=="id")id=value;if(key=="name")name=value;if(key=="brand")brand=value;}car_vehicle(id,brand.empty()?name:brand+" "+name);}}
    g_solarReset=true;if(!renderGameplayEnabled)return;
    // SCS explicitly distinguishes menus/paused state from player driving.
    // Configuration can also change while driving; require fresh truck channels
    // again, without treating a configuration packet as proof of gameplay.
    if(event==SCS_TELEMETRY_EVENT_paused){if(renderGameplayActive)g_warmResumeReady=g_freshDrivingPose&&g_freshDrivingSpeed&&g_haveWorldPosition&&g_haveTruckSpeed;g_drivingStarted=false;}
    if(event==SCS_TELEMETRY_EVENT_started){g_drivingStarted=true;if(g_warmResumeReady.exchange(false)&&!vehicleStart.waiting()&&g_haveWorldPosition&&g_haveTruckSpeed){g_freshDrivingPose=g_freshDrivingSpeed=true;g_preservePreparedOnStart=true;update_gameplay();return;}}else if(event!=SCS_TELEMETRY_EVENT_paused)g_warmResumeReady=false;
    g_freshDrivingPose=g_freshDrivingSpeed=false;suspend_gameplay(GetTickCount64());
}
SCSAPI_VOID on_theme_frame(const scs_event_t,const void* const,scs_context_t){
    if(g_drivingStarted&&g_freshDrivingPose&&g_freshDrivingSpeed&&vehicleStart.waiting()){
        uintptr_t actor=0,truck=0;player_actor_binding(actor,truck);
        if(vehicleStart.confirm(actor,truck,true))log(SCS_LOG_TYPE_message,"[VehicleStart] Confirmed player initialization generation=%llu actor=%p truck=%p",(unsigned long long)vehicleStart.epoch.load(),(void*)actor,(void*)truck);
    }
    if(!update_gameplay())return;
    copilot_clear_route_frame();
    static unsigned forkNotes=0;static int lastForkNote=-1;const auto forkStatus=nativeForkStatus.load();if(forkStatus!=lastForkNote&&forkNotes<12){lastForkNote=forkStatus;++forkNotes;log(SCS_LOG_TYPE_message,"[MapFork] status=%d supported=%u installed=%u published=%u view=%u phoneSources=%u wakeRequests=%llu",forkStatus,unsigned(nativeFork.supported),unsigned(nativeForkInstalled.load()),unsigned(nativeForkPublished.load()),mapViewCurrent.load(),nativePhoneJobCount.load(),(unsigned long long)nativePhoneWakeRequests.load());}
    static unsigned mapNote=0,mapNotes=0;const auto mapState=g_horizontalMapState.load();
    if(mapState!=mapNote&&mapNotes<20){mapNote=mapState;++mapNotes;log(SCS_LOG_TYPE_message,"[F150Map] source=%s generation=%llu queued=%u native-mip=%u resource-mips=%u normalized-mip=0",mapState==4?"native-ui-bound":mapState==3?"ready+composed":mapState==2?"ready":mapState==1?"missing":"unused",(unsigned long long)nativeForkSourceGeneration.load(),unsigned(nativeForkPublished.load()),g_horizontalSourceMip.load(),g_horizontalSourceMipCount.load());}
    static size_t renderedStyle=~size_t(0);const auto style=displayStyleIndex.load();if(style!=renderedStyle){renderedStyle=style;++g_mapThemeEpoch;g_skinDirty=true;g_mapBackgroundDirty=true;}
    const auto clockNow=GetTickCount64();voice_poll(clockNow);
    static uint64_t clockPoll=0;if(!clockPoll||clockNow-clockPoll>=200){clockPoll=clockNow;int mode=0,current=0,reference=0;const bool ready=game_timezone_read(gameTimezoneBindings,voiceVariables[3],mode,current,reference);if(ready)gameTimezoneReferenceMinutes=reference;const auto state=game_timezone_state(ready,mode,ready?current-reference:0);if(gameTimezoneState.exchange(state)!=state){g_clockDirty=true;g_distDirty=true;log(SCS_LOG_TYPE_message,"[GameClock] native=%d mode=%d local-zone=%d economy-zone=%d offset=%d sdk=%u convoy=%d clock=%s",ready,mode,current,reference,game_timezone_offset(state),g_gameTimeMinutes.load(),g_multiplayerTimeOffset.load(),current_clock_text().c_str());}}
    update_solar_theme();const bool desired=g_requestedMapDark.load();
    if(live_map_apply(desired)){const bool changed=g_darkMode.load()!=desired;publish_dark_presentation(desired);if(changed)log(SCS_LOG_TYPE_message,"[MapTheme] Complete native %s palette committed (source + UI caches + icons)",desired?"dark":"light");}
    else {if(g_assetsReady&&!liveMapReady)publish_dark_presentation(g_nativeMapDark);settingsMapThemePending=g_assetsReady&&desired!=g_darkMode.load();}
}



SCSAPI_VOID electric_enabled_cb(const scs_string_t,const scs_u32_t,const scs_value_t* const value,const scs_context_t){
    static std::atomic<int> previous{-9};static std::atomic<unsigned> traces{0};
    const int state=!value?-1:value->type==SCS_VALUE_TYPE_bool?(value->value_bool.value?1:0):-2;
    if(previous.exchange(state)!=state&&traces.fetch_add(1)<20)log(SCS_LOG_TYPE_message,"[PhonePower/v2] Ignition telemetry state=%d type=%d tick=%llu",state,value?int(value->type):-1,GetTickCount64());
    if(!value)return; // A transient missing channel must not cancel shutdown/startup.
    if(value->type==SCS_VALUE_TYPE_bool){f150_ignition(value->value_bool.value!=0);display_ignition(value->value_bool.value!=0);}
}
static std::string prepared_key(const OverlaySettings& s,uint64_t shields){auto d=bettergps_navigation::Current();return std::to_string(navigationVoiceRevision.load())+":"+std::to_string(displayStyleIndex.load())+":"+std::to_string(g_mapThemeEpoch.load())+":"+std::to_string(s.revision)+":"+std::to_string(g_tripPresent.load())+":"+std::to_string(g_darkMode.load())+":"+current_clock_text()+":"+current_speed_text()+":"+current_speed_limit_text()+":"+std::to_string(current_speed_is_over_limit())+":"+current_distance_text()+":"+current_time_text()+":"+std::to_string(g_haveCompassHeading.load()?int(compass_turn(g_compassHeading.load())*1000):-1)+":"+d.instruction+":"+d.text+":"+d.status+":"+d.currentRoad+":"+d.targetRoad+":"+d.destination+":"+d.next+":"+d.exit+":"+std::to_string(shields)+":"+std::to_string(homePublishedRevision.load());}
#include "DeviceRenderer.h"
#include "T680ClusterRenderer.h"
#include "MackClusterRenderer.h"
#include "VnlClusterRenderer.h"
#include "Volvo18ClusterRenderer.h"
#include "Volvo25ClusterRenderer.h"
#include "CascadiaClusterRenderer.h"
#include "IcucClusterRenderer.h"
#include "TouchActions.h"
#include "DeskClockRuntime.h"

static void start_cpu_worker(bool pollHost){if(g_cpuRunning.exchange(true))return;g_cpuStop=false;if(!g_preservePreparedOnStart.exchange(false))for(auto& image:g_prepared){std::lock_guard<std::mutex> lock(image.mutex);image.pixels.reset();image.version=image.uploaded=0;image.lastUse=0;}
 g_cpuWorker=std::thread([pollHost]{SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);std::array<std::string,33> keys;std::array<std::array<std::shared_ptr<std::vector<uint8_t>>,3>,33> buffers;std::vector<uint8_t> canvas;std::string menuKey;uint64_t poll=0,save=0,lastWarning=0,shieldPoll=0,shields=0;
 while(!g_cpuStop.load()){auto now=GetTickCount64();bettergps_diag::Refresh();graphics_watchdog();bool failed=false;try{desk_clock_worker(now);mapGpuAudit.flush();if(managedSelected<0)home_shuffle_update(homePowerOnSerial.load(),g_haveGameTime.load()?std::max<int64_t>(0,int64_t(g_gameTimeMinutes.load())+g_multiplayerTimeOffset.load())/1440:-1);if(render_gpu_allowed()){home_prepare();f150_prepare(std::filesystem::u8path(g_assetRoot));f150_info_prepare(std::filesystem::u8path(g_assetRoot));}
  if(pollHost&&now-poll>=100){poll=now;bettergps_diag::Scope hostTiming(bettergps_diag::HostPoll);bettergps_navigation::Poll(g_haveGameTime.load()&&g_haveTruckSpeed.load()&&g_haveWorldPosition.load());auto after=bettergps_navigation::Current();static std::string guidanceStatus;static uint64_t guidanceNote=0;if(after.status!=guidanceStatus||now-guidanceNote>=15000){guidanceStatus=after.status;guidanceNote=now;log(SCS_LOG_TYPE_message,"[Guidance] status=%s live=%u navigation-distance=%u driving-pose=%u",after.status.c_str(),unsigned(after.live),unsigned(g_haveNavDistance.load()),unsigned(g_haveWorldPosition.load()));}refresh_solar_settings();set_trip_presentation(trip_present(g_navDistanceM.load(),g_haveNavDistance.load(),g_navTimeSec.load(),g_haveNavTime.load(),after));}
  if(pollHost&&(now-save>=750||settingsNeedsSave.exchange(false))){save=now;bettergps_diag::Scope saveTiming(bettergps_diag::SettingsSave);managed_flush_selection();const bool deviceOk=managed_save()&&menuDock.save();const bool overlayOk=overlay_save(),powerOk=display_power_save(),homeOk=home_save(),carOk=car_save(),assignmentOk=display_policy_save(),viewsOk=map_views_save();settingsSaveFailed=!deviceOk||!overlayOk||!powerOk||!homeOk||!carOk||!assignmentOk||!viewsOk;}
  if(!shieldPoll||now-shieldPoll>=1000){shieldPoll=now;shields=shield_revision(bettergps_navigation::ShieldDirectory());}
  if(render_gpu_allowed()){touch_actions_worker();sygic_trip_sample();managed_prepare_worker(now);cluster_intro_prepare(now);mack_cluster_prepare(now);vnl_cluster_prepare(now);volvo18_cluster_prepare(now);volvo25_cluster_prepare(now);icc5_cluster_prepare(now);icuc_cluster_prepare(now);}else managed_pages_pause();
  touch_diagnostics_flush();
  touchGeometry.worker(managedBase,now);
  touch_status_prepare_worker(now);
  if(render_gpu_allowed())for(size_t slot=0;slot<33;++slot){
   auto& image=g_prepared[slot];auto used=image.lastUse.load();if(!used||now-used>500)continue;if(slot!=32&&(slot>=gpsDisplays.size()||!gpsDisplays[slot].ready||!display_active(slot)))continue;
   const bool physicalHorizontal=slot!=32&&gpsDisplays[slot].layout=="horizontal";int managed=slot!=32?managed_for_known(slot):-1;ManagedDevice configured;if(managed>=0)configured=managed_snapshot(unsigned(managed));const bool custom=managed>=0&&configured.enabled;
   if(custom)continue; // Navigation and startup are published together by managed_prepare_worker.
   DisplayStyleScope style(custom?configured.style:-1);auto profile=custom?(managedSelected==managed?overlay_snapshot():configured.overlay):slot==32?overlay_snapshot():display_snapshot(slot);DisplayThemeScope deviceTheme(selected_dark(profile.theme,g_darkMode.load()));if(profile.horizontal)horizontal_carplay_prepare(std::filesystem::u8path(g_assetRoot),now);
   const auto timing=physicalHorizontal?StartupPhase{startup_total_ms(),0}:startup_sample(now);const int startup=timing.elapsed;
   auto key=prepared_key(profile,shields)+":"+std::to_string(physicalHorizontal?0:startup_frame(startup))+":"+std::to_string(physicalHorizontal?0:timing.state)+":"+std::to_string(physicalHorizontal?horizontalCarplayRevision:0)+":"+std::to_string(custom?configured.revision:0);
   if(keys[slot]==key)continue;std::shared_ptr<std::vector<uint8_t>> pixels;for(auto& buffer:buffers[slot]){if(!buffer)buffer=std::make_shared<std::vector<uint8_t>>();if(buffer.use_count()==1){pixels=buffer;break;}}if(!pixels)continue;const auto themeEpoch=g_mapThemeEpoch.load();
   {bettergps_diag::Scope composeTiming(bettergps_diag::ComposePhone);if(custom){configured.overlay=profile;configured.startup=0;managed_render(canvas,unsigned(managed),configured,startup_total_ms());std::vector<uint8_t> sized;managed_resize(sized,canvas,profile.horizontal?800:360,profile.horizontal?500:740,800,500);canvas=std::move(sized);}else if(profile.horizontal)render_configured_horizontal(canvas,profile,slot+1);else render_prepared_phone(canvas,profile,slot+1,startup);}
   const unsigned width=slot==32?360:gpsDisplays[slot].markerWidth,rows=physicalHorizontal?500:740,columns=physicalHorizontal?800:360;pixels->assign(size_t(width)*rows*4,0);for(unsigned y=0;y<rows;++y)memcpy(pixels->data()+size_t(y)*width*4,canvas.data()+size_t(y)*columns*4,columns*4);if(themeEpoch!=g_mapThemeEpoch.load())continue;
   {std::lock_guard<std::mutex> lock(image.mutex);image.themeEpoch=themeEpoch;image.pixels=std::move(pixels);image.powerState=timing.state;image.powerPhase=startup;image.managedRevision=custom?configured.revision:0;++image.version;}if(custom&&g_managedPreparedRevision[managed]==configured.revision)managedOverlayReady[managed]=configured.revision;keys[slot]=std::move(key);
  }
  if(render_gpu_allowed()&&settingsOpen.load()){managed_pick_current();auto snapshot=overlay_snapshot();auto key=std::to_string(snapshot.revision)+":"+std::to_string(selectedDisplay.load())+":"+std::to_string(settingsSelected)+":"+std::to_string(settingsSaveFailed.load())+":"+std::to_string(settingsMapThemePending.load())+":"+std::to_string(displayPowerRevision.load())+":"+std::to_string(settingsLockscreen.load())+":"+std::to_string(homeRevision.load())+":"+std::to_string(homePublishedRevision.load())+":"+std::to_string(homeImportRevision.load())+":"+std::to_string(settingsMenuRevision.load())+":"+std::to_string(carRevision.load())+":"+std::to_string(carLoadRevision.load())+":"+std::to_string(displayPolicyRevision.load())+":"+std::to_string(mapViewRevision.load())+":"+std::to_string(mapViewEditing.load())+":"+std::to_string(managedRevision.load())+":"+std::to_string(managedScan&&GetTickCount64()-managedScanStarted.load()>1500);if(key!=menuKey||settingsClick.load()){auto image=std::make_shared<std::vector<uint8_t>>();{bettergps_diag::Scope menuTiming(bettergps_diag::ComposeMenu);settings_render(*image,2,false);}{std::lock_guard<std::mutex> lock(g_preparedMenu.mutex);g_preparedMenu.pixels=image;++g_preparedMenu.version;}menuKey=key;}}
 if(render_gpu_allowed()&&settingsOpen.load())home_import_prepare();
 }catch(const std::exception& e){failed=true;if(!lastWarning||now-lastWarning>=5000){lastWarning=now;log(SCS_LOG_TYPE_warning,"Background display preparation: %s",e.what());}}
 std::unique_lock<std::mutex> lock(g_cpuWaitMutex);g_cpuWake.wait_for(lock,std::chrono::milliseconds(failed?1000:20));
 }});
}
static void stop_cpu_worker(){
 if(!g_cpuWorker.joinable()){g_cpuRunning=false;bettergps_gallery::gallery.stop();return;}
 g_cpuStop=true;g_cpuWake.notify_all();
 if(g_cpuWorker.joinable()){
  // D3D/COM teardown on the worker can synchronously message this thread.
  // Keep sent messages flowing instead of joining while the UI is unresponsive.
  const HANDLE thread=g_cpuWorker.native_handle();bool quit=false;WPARAM quitCode=0;
  for(;;){const auto result=MsgWaitForMultipleObjectsEx(1,&thread,50,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
   if(result==WAIT_OBJECT_0)break;
   MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
    if(message.message==WM_QUIT){quit=true;quitCode=message.wParam;continue;}
    TranslateMessage(&message);DispatchMessageW(&message);
   }
  }
  g_cpuWorker.join();if(quit)PostQuitMessage(int(quitCode));
 }
 touchStatus.finish();g_cpuRunning=false;
 bettergps_gallery::gallery.stop();
}
static void suspend_gameplay(uint64_t now){
 cluster_intro_pause(now);
 mack_cluster_pause(now);
 vnl_cluster_pause(now);
 volvo18_cluster_pause(now);volvo25_cluster_pause(now);icc5_cluster_pause(now);icuc_cluster_pause(now);
 if(!g_gameplayPausedAt)g_gameplayPausedAt=now;
 renderGameplayActive=false;
 if(g_gameplayTransition)return;
 g_gameplayTransition=true;settings_close();stop_cpu_worker();managed_native_suspend();g_gameplayTransition=false;
}
static bool update_gameplay(){
 if(!renderGameplayEnabled)return !renderStopping;
 if(renderStopping||g_gameplayTransition)return false;
 if(!g_drivingStarted||!g_freshDrivingPose||!g_freshDrivingSpeed){if(renderGameplayActive)suspend_gameplay(GetTickCount64());return false;}
 if(!renderGameplayActive){
  const auto now=GetTickCount64();
  if(g_gameplayPausedAt){phone_startup_resume_wait(g_gameplayPausedAt,now);auto state=phonePower.load();
   // Freeze both startup and shutdown animation while no gameplay is rendered.
   if((state&phoneShutdownBit)||state>=3){const auto origin=state&phoneShutdownBit?state&~phoneShutdownBit:state-3;const auto frozenFrom=std::max(g_gameplayPausedAt,origin);phonePower=state+(now>frozenFrom?now-frozenFrom:0);}
   auto infoOrigin=f150InfoOrigin.load();if(infoOrigin){const auto frozenFrom=std::max(g_gameplayPausedAt,infoOrigin);f150InfoOrigin.compare_exchange_strong(infoOrigin,infoOrigin+(now>frozenFrom?now-frozenFrom:0));}
   auto origin=f150Origin.load();if(origin){const auto frozenFrom=std::max(g_gameplayPausedAt,origin);f150Origin.compare_exchange_strong(origin,origin+(now>frozenFrom?now-frozenFrom:0));}
   g_gameplayPausedAt=0;
  }
  renderGameplayActive=true;
  try{start_cpu_worker(true);}catch(...){renderGameplayActive=false;g_cpuRunning=false;return false;}
 }
 return true;
}

SCSAPI_VOID on_frame(const scs_event_t, const void* const info, scs_context_t) {
    flush_pending_logs();
    ++g_frameCount;
    if(info&&(static_cast<const scs_telemetry_frame_start_t*>(info)->flags&SCS_TELEMETRY_FRAME_START_FLAG_timer_restart)){
        g_warmResumeReady=false;g_solarReset=true;phone_startup_session();display_power_refresh();if(renderGameplayEnabled){g_freshDrivingPose=g_freshDrivingSpeed=false;suspend_gameplay(GetTickCount64());}
    }

    if (!all_mounted() &&
        (g_frameCount == 1 || g_frameCount == 30 ||
         g_frameCount == 120 || g_frameCount == 300))
        mount_all();
    // Deliberately no D3D work here.  GPU updates happen only in the
    // PSSetShaderResources render-thread hook when the dedicated image is used.
}

} // namespace bettergps_overlay

#pragma comment(linker, "/export:scs_telemetry_init=scs_telemetry_init")
SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version,
    const scs_telemetry_init_params_t* const params) {
    if (!params) return SCS_RESULT_invalid_parameter;
    if(version!=SCS_TELEMETRY_VERSION_1_00&&version!=SCS_TELEMETRY_VERSION_1_01)return SCS_RESULT_unsupported;
    auto* p = reinterpret_cast<const scs_telemetry_init_params_v101_t*>(params);
    bettergps::StartupTrace trace(p->common.log,"overlay");
    trace.measure("diagnostics-shared-memory",[]{bettergps_diag::Init(1);});
    bettergps_overlay::g_log = p->common.log;bettergps_overlay::g_logOwner=GetCurrentThreadId();
    const auto startupBegin=GetTickCount64();auto startupPrevious=startupBegin;
    auto startupPhase=[&](const char* phase){const auto now=GetTickCount64();bettergps_overlay::log(SCS_LOG_TYPE_message,"[StartupTiming] phase=%s ms=%llu total_ms=%llu",phase,(unsigned long long)(now-startupPrevious),(unsigned long long)(now-startupBegin));startupPrevious=now;};
    bettergps_overlay::renderStopping=false;bettergps_overlay::g_shutdownStarted=false;bettergps_navigation::Start();
    bettergps_overlay::renderGameplayEnabled=true;bettergps_overlay::renderGameplayActive=false;
    bettergps_overlay::g_drivingStarted=false;bettergps_overlay::g_freshDrivingPose=bettergps_overlay::g_freshDrivingSpeed=false;bettergps_overlay::g_gameplayPausedAt=GetTickCount64();
    bettergps_overlay::g_shutdownPath=std::filesystem::path(bettergps_overlay::plugin_directory())/L"BetterGPSShutdown.log";
    bettergps_overlay::g_graphicsWatchPath=std::filesystem::path(bettergps_overlay::plugin_directory())/L"BetterGPSGraphicsWatch.log";
    bettergps_overlay::renderGameplaySuspend=+[]{bettergps_overlay::g_drivingStarted=false;bettergps_overlay::g_warmResumeReady=false;bettergps_overlay::suspend_gameplay(GetTickCount64());};
    bettergps_overlay::renderActivityEnabled=true;bettergps_overlay::renderActivityWake=+[]{bettergps_overlay::g_cpuWake.notify_all();};
    bettergps_overlay::log(SCS_LOG_TYPE_message,"[PhonePower/v2] Bounded native-screen diagnosis enabled (%s %s)",__DATE__,__TIME__);
    bettergps_overlay::phone_startup_session();
    bettergps_overlay::log(SCS_LOG_TYPE_message,
        "Starting Google Maps/RWR VFS + SAFE TripInfo bridge (SDK=%u, assets=%s)",
        version, bettergps_overlay::kAssetPackId);

    bettergps_overlay::overlay_load(std::filesystem::path(bettergps_overlay::plugin_directory())/L"BetterGPSOverlay.cfg");
    bettergps_overlay::display_power_load(std::filesystem::path(bettergps_overlay::plugin_directory())/L"BetterGPSDisplayPower.ini");
    bettergps_overlay::home_init(bettergps_overlay::plugin_directory());
    bettergps_overlay::menuDock.init(bettergps_overlay::plugin_directory());
    bettergps_overlay::touchDiagnostics.init(bettergps_overlay::plugin_directory());
    bettergps_overlay::map_views_init(bettergps_overlay::plugin_directory());
    bettergps_overlay::managed_init(bettergps_overlay::plugin_directory());
    bettergps_deskclock::clock.init(bettergps_overlay::plugin_directory());
    bettergps_overlay::managed_native_install();
    startupPhase("settings-and-devices");
    trace.measure("load-t680-cluster",[]{bettergps_overlay::cluster_intro_load(bettergps_overlay::plugin_directory());});
    trace.measure("load-mack-cluster",[]{bettergps_overlay::mack_cluster_load(bettergps_overlay::plugin_directory());});
    trace.measure("load-vnl2014-cluster",[]{bettergps_overlay::vnl_cluster_load(bettergps_overlay::plugin_directory());});
    trace.measure("load-volvo18-cluster",[]{bettergps_overlay::volvo18_cluster_load(bettergps_overlay::plugin_directory());});
    trace.measure("load-volvo25-cluster",[]{bettergps_overlay::volvo25_cluster_load(bettergps_overlay::plugin_directory());});
    trace.measure("load-icc5-cluster",[]{bettergps_overlay::icc5_cluster_load(bettergps_overlay::plugin_directory());});
    trace.measure("load-icuc-cluster",[]{bettergps_overlay::icuc_cluster_load(bettergps_overlay::plugin_directory());});
    startupPhase("cluster-assets");
    bettergps_overlay::mapGpuAudit.init(bettergps_overlay::plugin_directory());
    {bettergps_overlay::PeView image{};if(bettergps_overlay::get_pe_view(image)){bettergps_overlay::native_fork_resolve({image.base,image.imageSize});bettergps_overlay::managed_native_resolve({image.base,image.imageSize});bettergps_overlay::native_player_resolve({image.base,image.imageSize});bettergps_overlay::player_actor_resolve({image.base,image.imageSize});bettergps_overlay::game_timezone_resolve({image.base,image.imageSize});}}
    const bool privateAliasReady=bettergps_overlay::nativeFork.supported&&bettergps_overlay::native_fork_alias_register();
    bettergps_overlay::log(SCS_LOG_TYPE_message,"[MapFork] Before dashboard/VFS loading: dynamic alias ready=%u handle=%u",unsigned(privateAliasReady),unsigned(bettergps_overlay::native_map_device().aliasHandle));
    startupPhase("native-address-validation");
    trace.measure("read-car-settings-and-first-frames",[]{bettergps_overlay::car_init(bettergps_overlay::plugin_directory());});
    try{bettergps_overlay::display_read_registry(bettergps_overlay::plugin_directory());trace.measure("generate-displays",[&]{bettergps_overlay::display_generate(privateAliasReady);});bettergps_overlay::managed_resolve_known();trace.measure("build-managed-templates",[]{bettergps_overlay::managed_build_templates();});trace.measure("build-player-materials",[]{bettergps_overlay::native_player_build_materials();});bettergps_overlay::display_policy_init(bettergps_overlay::plugin_directory());bettergps_overlay::display_activate_first();
        bettergps_overlay::managed_bridges_init();bettergps_overlay::g_displayBridges.clear();for(auto& d:bettergps_overlay::gpsDisplays)if(d.ready)bettergps_overlay::g_displayBridges.push_back(std::make_unique<bettergps_overlay::OverlayBridge>(bettergps_overlay::OverlayBridge{d.id.c_str(),d.markerWidth,d.layout=="horizontal"?500u:740u}));
        bettergps_overlay::g_surfaceBridges.clear();for(auto& d:bettergps_overlay::gpsDisplays)if(d.independent)bettergps_overlay::g_surfaceBridges.push_back(std::make_unique<bettergps_overlay::OverlayBridge>(bettergps_overlay::OverlayBridge{d.id.c_str(),d.surfaceWidth,41}));
        for(auto& e:bettergps_overlay::displayErrors)bettergps_overlay::log(SCS_LOG_TYPE_warning,"%s",e.c_str());
    }catch(const std::exception& e){bettergps_overlay::log(SCS_LOG_TYPE_error,"Display registry failed: %s",e.what());return SCS_RESULT_generic_error;}
    startupPhase("car-and-generated-displays");
    const MH_STATUS mh = MH_Initialize();
    if (mh != MH_OK && mh != MH_ERROR_ALREADY_INITIALIZED) {
        bettergps_overlay::log(SCS_LOG_TYPE_error, "MinHook init failed (%d)", static_cast<int>(mh));
        return SCS_RESULT_generic_error;
    }

    if(!trace.measure("install-graphics-and-input-hooks",[]{return bettergps_overlay::install_d3d_hooks();})){MH_Uninitialize();return SCS_RESULT_generic_error;}
    startupPhase("graphics-and-input-hooks");
    bettergps_overlay::log(SCS_LOG_TYPE_message,"[CameraVideo] native parking gate %s",bettergps_overlay::cameraNativeBackend.install()?"installed":"unavailable; camera stays black");
    {const bool ready=trace.measure("install-player-observer",[]{return bettergps_overlay::player_actor_install();});bettergps_overlay::log(ready?SCS_LOG_TYPE_message:SCS_LOG_TYPE_warning,"[VehicleStart] Player initialization observer %s; Home requires a confirmed actor event",ready?"installed":"unavailable");}
    p->register_for_channel(
        "truck.speed", SCS_U32_NIL, SCS_VALUE_TYPE_float,
        SCS_TELEMETRY_CHANNEL_FLAG_no_value|SCS_TELEMETRY_CHANNEL_FLAG_each_frame,
        bettergps_overlay::truck_speed_cb, nullptr);
    p->register_for_channel(
        "truck.navigation.speed.limit", SCS_U32_NIL, SCS_VALUE_TYPE_float,
        SCS_TELEMETRY_CHANNEL_FLAG_no_value,
        bettergps_overlay::speed_limit_cb, nullptr);
    p->register_for_channel(
        "game.time", SCS_U32_NIL, SCS_VALUE_TYPE_u32,
        SCS_TELEMETRY_CHANNEL_FLAG_no_value,
        bettergps_overlay::game_time_cb, nullptr);
    p->register_for_channel(
        "multiplayer.time.offset", SCS_U32_NIL, SCS_VALUE_TYPE_s32,
        SCS_TELEMETRY_CHANNEL_FLAG_no_value,
        bettergps_overlay::multiplayer_time_offset_cb, nullptr);
    p->register_for_channel(
        "truck.navigation.distance", SCS_U32_NIL, SCS_VALUE_TYPE_float,
        SCS_TELEMETRY_CHANNEL_FLAG_no_value,
        bettergps_overlay::nav_distance_cb, nullptr);
    p->register_for_channel(
        "truck.navigation.time", SCS_U32_NIL, SCS_VALUE_TYPE_float,
        SCS_TELEMETRY_CHANNEL_FLAG_no_value,
        bettergps_overlay::nav_time_cb, nullptr);
    p->register_for_event(SCS_TELEMETRY_EVENT_frame_start, bettergps_overlay::on_frame, nullptr);
    p->register_for_event(SCS_TELEMETRY_EVENT_frame_end, bettergps_overlay::on_theme_frame, nullptr);
    p->register_for_event(SCS_TELEMETRY_EVENT_started, bettergps_overlay::on_theme_session, nullptr);
    p->register_for_event(SCS_TELEMETRY_EVENT_paused, bettergps_overlay::on_theme_session, nullptr);
    p->register_for_event(SCS_TELEMETRY_EVENT_configuration, bettergps_overlay::on_theme_session, nullptr);
    // Dashboard R is selected even while the automatic transmission holds neutral at rest.
    p->register_for_channel("truck.displayed.gear",SCS_U32_NIL,SCS_VALUE_TYPE_s32,SCS_TELEMETRY_CHANNEL_FLAG_no_value,bettergps_overlay::camera_sdk_cb,nullptr);
    p->register_for_channel("truck.lblinker",SCS_U32_NIL,SCS_VALUE_TYPE_bool,SCS_TELEMETRY_CHANNEL_FLAG_no_value,bettergps_overlay::camera_sdk_cb,reinterpret_cast<scs_context_t>(1));
    p->register_for_channel("truck.rblinker",SCS_U32_NIL,SCS_VALUE_TYPE_bool,SCS_TELEMETRY_CHANNEL_FLAG_no_value,bettergps_overlay::camera_sdk_cb,reinterpret_cast<scs_context_t>(2));
    const auto electricResult=p->register_for_channel("truck.electric.enabled",SCS_U32_NIL,SCS_VALUE_TYPE_bool,SCS_TELEMETRY_CHANNEL_FLAG_no_value,bettergps_overlay::electric_enabled_cb,nullptr);
    const auto gaugeOil=p->register_for_channel("truck.oil.temperature",SCS_U32_NIL,SCS_VALUE_TYPE_float,SCS_TELEMETRY_CHANNEL_FLAG_no_value|SCS_TELEMETRY_CHANNEL_FLAG_each_frame,bettergps_overlay::t680_gauge_sdk,reinterpret_cast<scs_context_t>(1));
    const auto gaugeMpg=p->register_for_channel("truck.fuel.consumption.average",SCS_U32_NIL,SCS_VALUE_TYPE_float,SCS_TELEMETRY_CHANNEL_FLAG_no_value|SCS_TELEMETRY_CHANNEL_FLAG_each_frame,bettergps_overlay::t680_gauge_sdk,nullptr);
    bettergps_overlay::log(SCS_LOG_TYPE_message,"[T680Gauges] oil-sdk=%d average-sdk=%d guarded-native=%u",int(gaugeOil),int(gaugeMpg),unsigned(bettergps_overlay::t680GaugeNativeLayout));
    bettergps_overlay::log(electricResult==SCS_RESULT_ok?SCS_LOG_TYPE_message:SCS_LOG_TYPE_warning,"[PhonePower/v2] Ignition registration result=%d",int(electricResult));
    if(electricResult!=SCS_RESULT_ok)bettergps_overlay::phoneStartupReady=true;
    p->register_for_channel("truck.world.placement",SCS_U32_NIL,SCS_VALUE_TYPE_dplacement,SCS_TELEMETRY_CHANNEL_FLAG_no_value|SCS_TELEMETRY_CHANNEL_FLAG_each_frame,bettergps_overlay::world_position_cb,nullptr);
    trace.measure("native-map-palette-and-vfs",[]{bettergps_overlay::mount_all();});
    startupPhase("native-map-palette-and-vfs");
    // The preparation thread starts only after STARTED + fresh truck data.
    return SCS_RESULT_ok;
}

#pragma comment(linker, "/export:scs_telemetry_shutdown=scs_telemetry_shutdown")
SCSAPI_VOID scs_telemetry_shutdown() {
    using namespace bettergps_overlay;
    if(g_shutdownStarted.exchange(true))return;
    shutdown_note("begin",true);g_log=nullptr;
    renderStopping=true;renderGameplayActive=false;bettergps_navigation::RequestStop();shutdown_note("stop-requested");settings_close();shutdown_note("menu-closed");
    stop_cpu_worker();bettergps_deskclock::clock.save(GetTickCount64(),true);PlaySoundW(nullptr,nullptr,0);shutdown_note("worker-stopped");
    bettergps_navigation::Stop();shutdown_note("host-stopped");
    cameraNativeBackend.stop();
    uninstall_d3d_hooks();
    release_phone_fonts();
    MH_Uninitialize();
    size_t mountedCount = 0;
    for (bool mounted : g_mounted) if (mounted) ++mountedCount;
    log(SCS_LOG_TYPE_message,
        "Shutdown (%llu/%llu exact VFS dirs). SAFE TripInfo bridge stopped.",
        static_cast<unsigned long long>(mountedCount),
        static_cast<unsigned long long>(kMountCount));
    bettergps_diag::Stop();
    g_log = nullptr;shutdown_note("done");
}

