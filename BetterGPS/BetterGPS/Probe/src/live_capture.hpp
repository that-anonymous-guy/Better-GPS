#pragma once
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include <memory>
#include <string>
#include <vector>
#include <deque>
namespace bettergps {
struct CaptureNode {
    int x=0,y=0,z=0; unsigned flags=0;
    float qw=1,qx=0,qy=0,qz=0;
    unsigned long long backward=0,forward=0;
    bool backward_valid=false,forward_valid=false;
    std::string status="UNAVAILABLE";
};
struct CapturePhysical { unsigned long long uid; float distance, remaining, time, remaining_time; };
struct CaptureLogical { unsigned type, node_id; unsigned long long uid; float distance, remaining, time, remaining_time; };
struct CaptureRoute { unsigned long long fingerprint; std::vector<CapturePhysical> physical; std::vector<CaptureLogical> logical;
    std::vector<CaptureNode> nodes; unsigned long long captured_at=0; };
struct CaptureVehicle {
    double x=0,y=0,z=0;
    float heading=0,speed=0,navigation=0;
    bool pose_valid=false,speed_valid=false,navigation_valid=false,paused=true;
    unsigned long long timestamp=0,generation=0;
};
struct CaptureControl {
    std::string type="CaptureStatus",status="WAITING_ROUTE";
    unsigned long long timestamp=0,generation=0;
    std::shared_ptr<const CaptureRoute> route;
    std::string game_build;
    unsigned gps_offset=0;
};
std::string HexUid(unsigned long long uid);
std::string SerializeCapture(const std::string& session,unsigned long long sequence,const CaptureVehicle& v);
std::string SerializeCapture(const std::string& session,unsigned long long sequence,const CaptureControl& c);
class LiveCapture {
public:
    ~LiveCapture() { Stop(); }
    void Start(); void Stop();
    bool Publish(CaptureControl control); void Publish(CaptureVehicle vehicle);
    void SetSource(std::string build,unsigned offset) { game_build_=std::move(build); gps_offset_=offset; }
    void SetPipeForTest(std::wstring name) { pipe_name_=std::move(name); }
private:
    void Run(); bool Write(HANDLE pipe,const std::string& json);
    std::mutex mutex_;
    std::optional<CaptureControl> control_;
    std::deque<CaptureControl> controls_;
    std::optional<CaptureVehicle> vehicle_;
    std::atomic<bool> stopping_{true};
    std::thread thread_;
    HANDLE stop_event_=nullptr;
    std::string game_build_;
    unsigned gps_offset_=0;
    std::wstring pipe_name_=L"\\\\.\\pipe\\BetterGPSProbe-v1";
};
}
