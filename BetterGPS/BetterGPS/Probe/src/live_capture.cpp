#include "live_capture.hpp"
#include "../../shared/PerformanceDiagnostics.h"
#include <sstream>
#include <iomanip>
#include <locale>
#include <algorithm>
#include <stdexcept>
#include <cmath>
namespace bettergps {
std::string HexUid(unsigned long long uid) {
    std::ostringstream s; s << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << uid; return s.str();
}
static std::ostringstream Stream() { std::ostringstream s; s.imbue(std::locale::classic()); s << std::setprecision(17); return s; }
static std::string Quote(const std::string& value) {
    auto s=Stream();s << '"';
    for(unsigned char c:value) { if(c=='"'||c=='\\') s << '\\' << c;else if(c<32) s << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(c);else s << c; }
    s << '"';return s.str();
}
static void Envelope(std::ostringstream& s,const std::string& session,unsigned long long sequence,
    const std::string& type,unsigned long long timestamp,unsigned long long generation) {
    s << "{\"version\":2,\"type\":\"" << type << "\",\"sessionId\":\"" << session <<
        "\",\"sequence\":" << sequence << ",\"timestampMs\":" << timestamp << ",\"generation\":" << generation;
}
std::string SerializeCapture(const std::string& session,unsigned long long sequence,const CaptureVehicle& v) {
    bettergps_diag::Scope timing(bettergps_diag::PacketSerialize);
    if ((v.pose_valid && (!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.heading))) ||
        (v.speed_valid && !std::isfinite(v.speed)) || (v.navigation_valid && (!std::isfinite(v.navigation)||v.navigation<0)))
        throw std::runtime_error("Non-finite telemetry.");
    auto s=Stream(); Envelope(s,session,sequence,"TelemetryFrame",v.timestamp,v.generation);
    s << ",\"vehicle\":{\"position\":{\"x\":" << v.x << ",\"y\":" << v.y << ",\"z\":" << v.z <<
        "},\"scsHeading\":" << v.heading << ",\"signedSpeedMps\":" << v.speed << ",\"paused\":" << (v.paused?"true":"false") <<
        ",\"poseValid\":" << (v.pose_valid?"true":"false") << ",\"speedValid\":" << (v.speed_valid?"true":"false") <<
        ",\"navigationRemainingM\":";
    if(v.navigation_valid) s << v.navigation; else s << "null";
    s << "}}"; return s.str();
}
std::string SerializeCapture(const std::string& session,unsigned long long sequence,const CaptureControl& c) {
    bettergps_diag::Scope timing(bettergps_diag::PacketSerialize);
    auto s=Stream(); Envelope(s,session,sequence,c.type,c.timestamp,c.generation);
    if(c.route) {
        s << ",\"route\":{\"fingerprint\":\"" << HexUid(c.route->fingerprint) << "\",\"physical\":[";
        bool first=true;
        for(const auto& p:c.route->physical) {
            if(!first) s << ','; first=false;
            s << "{\"nodeUid\":\"" << HexUid(p.uid) << "\",\"distanceFromPreviousM\":" << p.distance <<
                ",\"totalDistanceTillEndM\":" << p.remaining << ",\"timeFromPreviousS\":" << p.time <<
                ",\"totalTimeTillEndS\":" << p.remaining_time << '}';
        }
        s << "],\"logical\":["; first=true;
        for(const auto& p:c.route->logical) {
            if(!first) s << ','; first=false;
            s << "{\"type\":" << p.type << ",\"nodeId\":" << p.node_id << ",\"itemUid\":\"" << HexUid(p.uid) <<
                "\",\"distanceFromPreviousM\":" << p.distance << ",\"totalDistanceTillEndM\":" << p.remaining <<
                ",\"timeFromPreviousS\":" << p.time << ",\"totalTimeTillEndS\":" << p.remaining_time << '}';
        }
        s << "],\"capturedAtTickMs\":" << c.route->captured_at << ",\"nodes\":[";first=true;
        for(const auto& n:c.route->nodes) {
            if(!first)s<<',';first=false;
            s<<"{\"rawX\":"<<n.x<<",\"rawY\":"<<n.y<<",\"rawZ\":"<<n.z<<",\"flags\":"<<n.flags
             <<",\"qw\":"<<n.qw<<",\"qx\":"<<n.qx<<",\"qy\":"<<n.qy<<",\"qz\":"<<n.qz
             <<",\"backwardUid\":\""<<HexUid(n.backward)<<"\",\"forwardUid\":\""<<HexUid(n.forward)
             <<"\",\"backwardValid\":"<<(n.backward_valid?"true":"false")<<",\"forwardValid\":"<<(n.forward_valid?"true":"false")
             <<",\"status\":"<<Quote(n.status)<<'}';
        }
        s << "]}";
    } else if(c.type=="Hello") s << ",\"status\":\"ATS_1_61_CAPTURE_V2\",\"source\":{\"probeVersion\":\"0.3.0\",\"gameBuild\":" << Quote(c.game_build) << ",\"gpsManagerOffset\":" << c.gps_offset << '}';
    else if(c.type=="CaptureStatus") s << ",\"status\":\"" << c.status << '"';
    s << '}'; return s.str();
}
void LiveCapture::Start() {
    if(thread_.joinable()) return;
    stop_event_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!stop_event_) throw std::runtime_error("CreateEvent failed.");
    stopping_=false;
    try { thread_=std::thread(&LiveCapture::Run,this); }
    catch(...) { CloseHandle(stop_event_); stop_event_=nullptr; stopping_=true; throw; }
}
void LiveCapture::Stop() {
    stopping_=true; if(stop_event_) SetEvent(stop_event_);
    if(thread_.joinable()) thread_.join();
    if(stop_event_) { CloseHandle(stop_event_); stop_event_=nullptr; }
}
bool LiveCapture::Publish(CaptureControl c) {
    std::unique_lock<std::mutex> lock(mutex_,std::try_to_lock);
    if(!lock.owns_lock()) return false;
    if(controls_.size()>=64) return false; // Producer retries without advancing generation.
    controls_.push_back(c);
    control_=std::move(c);
    if(vehicle_ && vehicle_->generation!=control_->generation) vehicle_.reset();
    return true;
}
void LiveCapture::Publish(CaptureVehicle v) {
    std::unique_lock<std::mutex> lock(mutex_,std::try_to_lock);
    if(lock.owns_lock()) vehicle_=v;
}
bool LiveCapture::Write(HANDLE pipe,const std::string& json) {
    bettergps_diag::Scope timing(bettergps_diag::PipeWrite);
    if(json.empty() || json.size()>8*1024*1024) return false;
    unsigned size=static_cast<unsigned>(json.size());
    std::vector<char> frame(4+json.size());
    for(unsigned i=0;i<4;i++) frame[i]=static_cast<char>((size>>(8*i))&255);
    std::copy(json.begin(),json.end(),frame.begin()+4);
    HANDLE event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!event) return false;
    bool ok=true; size_t offset=0;
    while(offset<frame.size() && !stopping_) {
        OVERLAPPED ov{}; ov.hEvent=event; ResetEvent(event); DWORD written=0;
        BOOL result=WriteFile(pipe,frame.data()+offset,static_cast<DWORD>(frame.size()-offset),&written,&ov);
        if(!result && GetLastError()==ERROR_IO_PENDING) {
            HANDLE waits[]={stop_event_,event};
            if(WaitForMultipleObjects(2,waits,FALSE,2000)!=WAIT_OBJECT_0+1) {
                CancelIoEx(pipe,&ov); GetOverlappedResult(pipe,&ov,&written,TRUE);
                ok=false; break;
            }
            result=GetOverlappedResult(pipe,&ov,&written,FALSE);
        }
        if(!result || written==0) { ok=false; break; }
        offset+=written;
    }
    CloseHandle(event); return ok && offset==frame.size();
}
void LiveCapture::Run() {
    while(!stopping_) {
        HANDLE pipe=CreateFileW(pipe_name_.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
        if(pipe==INVALID_HANDLE_VALUE) { WaitForSingleObject(stop_event_,500); continue; }
        auto origin=GetTickCount64(); auto session=HexUid(origin)+HexUid(GetCurrentProcessId());
        try {
        unsigned long long sequence=0,sent_generation=~0ull,sent_vehicle=~0ull,last_timestamp=0;
        CaptureControl hello; hello.type="Hello";hello.game_build=game_build_;hello.gps_offset=gps_offset_;
        {std::lock_guard<std::mutex> lock(mutex_);controls_.clear();if(control_)controls_.push_back(*control_);}
        bool connected=Write(pipe,SerializeCapture(session,sequence++,hello));
        while(connected && !stopping_) {
            std::optional<CaptureControl> c; std::optional<CaptureVehicle> v;
            { std::lock_guard<std::mutex> lock(mutex_); if(!controls_.empty())c=controls_.front(); v=vehicle_; }
            try {
                if(c && c->generation!=sent_generation) {
                    auto send=*c; send.timestamp=GetTickCount64()-origin;
                    auto json=SerializeCapture(session,sequence++,send);
                    if(json.size()>8*1024*1024) {
                        send.type="CaptureStatus"; send.status="ROUTE_TOO_LARGE"; send.route.reset();
                        json=SerializeCapture(session,sequence++,send);
                    }
                    connected=Write(pipe,json);
                    if(!connected) break;
                    sent_generation=c->generation; last_timestamp=send.timestamp;
                    {std::lock_guard<std::mutex> lock(mutex_);if(!controls_.empty()&&controls_.front().generation==c->generation)controls_.pop_front();}
                }
                if(v && v->generation==sent_generation && v->timestamp>=origin && v->timestamp!=sent_vehicle) {
                    auto send=*v; send.timestamp=v->timestamp-origin;
                    if(send.timestamp>=last_timestamp) {
                        connected=Write(pipe,SerializeCapture(session,sequence++,send)); last_timestamp=send.timestamp;
                    }
                    sent_vehicle=v->timestamp;
                }
            } catch(...) { connected=false; }
            WaitForSingleObject(stop_event_,20);
        }
        } catch(...) { }
        CancelIoEx(pipe,nullptr); CloseHandle(pipe);
    }
}
}
