#pragma once
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <vector>
#include <filesystem>
#include <chrono>

struct VideoFrame {
 std::shared_ptr<const std::vector<uint8_t>> pixels;
 uint64_t serial=0;
 bool done=false;
 uint64_t doneAt=0;
};

class VideoPlayback {
 std::mutex mutex_;
 std::condition_variable cv_;
 std::thread thread_;
 std::atomic<bool> stop_{false};
 std::atomic<bool> started_{false};
 std::filesystem::path path_;
 unsigned width_=0,height_=0,loops_=1;
 VideoFrame frame_;
 uint64_t generation_=0;
 static void ensure_mf(){static std::once_flag once;std::call_once(once,[]{MFStartup(MF_VERSION,MFSTARTUP_NOSOCKET);});}
 static uint64_t now(){return GetTickCount64();}
 void run(std::filesystem::path path,unsigned width,unsigned height,unsigned loops,uint64_t generation){
  ensure_mf();CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  for(unsigned loop=0;loop<loops&&!stop_.load();++loop){
   Microsoft::WRL::ComPtr<IMFAttributes> attributes;MFCreateAttributes(&attributes,1);if(attributes)attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS,TRUE);
   Microsoft::WRL::ComPtr<IMFSourceReader> reader;HRESULT hr=MFCreateSourceReaderFromURL(path.wstring().c_str(),attributes.Get(),&reader);if(FAILED(hr))break;
   Microsoft::WRL::ComPtr<IMFMediaType> native;UINT inputWidth=width,inputHeight=height;if(SUCCEEDED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,&native)))MFGetAttributeSize(native.Get(),MF_MT_FRAME_SIZE,&inputWidth,&inputHeight);
   Microsoft::WRL::ComPtr<IMFMediaType> type;MFCreateMediaType(&type);type->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);type->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_NV12);MFSetAttributeSize(type.Get(),MF_MT_FRAME_SIZE,inputWidth,inputHeight);type->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive);hr=reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,type.Get());if(FAILED(hr))break;
   LONGLONG firstTime=-1;auto playbackStart=std::chrono::steady_clock::now();std::shared_ptr<std::vector<uint8_t>> spare;
   for(;;){DWORD actual=0,flags=0;LONGLONG timestamp=0;Microsoft::WRL::ComPtr<IMFSample> sample;hr=reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,&actual,&flags,&timestamp,&sample);if(FAILED(hr)||flags&MF_SOURCE_READERF_ENDOFSTREAM||!sample)break;if(flags&MF_SOURCE_READERF_STREAMTICK)continue;
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;if(FAILED(sample->ConvertToContiguousBuffer(&buffer)))continue;BYTE* data=nullptr;DWORD length=0;if(FAILED(buffer->Lock(&data,nullptr,&length)))continue;
    const size_t outputBytes=size_t(width)*height*4,inputBytes=size_t(inputWidth)*inputHeight*3/2;if(length<inputBytes){buffer->Unlock();continue;}if(!spare||spare->size()!=outputBytes)spare=std::make_shared<std::vector<uint8_t>>(outputBytes);auto pixels=std::move(spare);for(unsigned y=0;y<height;++y){const unsigned sy=std::min(inputHeight-1,unsigned(uint64_t(y)*inputHeight/height));for(unsigned x=0;x<width;++x){const unsigned sx=std::min(inputWidth-1,unsigned(uint64_t(x)*inputWidth/width));const int Y=data[size_t(sy)*inputWidth+sx],U=data[size_t(inputWidth)*inputHeight+size_t(sy/2)*inputWidth+(sx&~1)],V=data[size_t(inputWidth)*inputHeight+size_t(sy/2)*inputWidth+(sx&~1)+1];const int C=std::max(0,Y-16),D=U-128,E=V-128;auto clip=[](int v){return uint8_t(std::clamp(v,0,255));};auto* p=pixels->data()+(size_t(y)*width+x)*4;p[0]=clip((298*C+516*D+128)>>8);p[1]=clip((298*C-100*D-208*E+128)>>8);p[2]=clip((298*C+409*E+128)>>8);p[3]=255;}}buffer->Unlock();
    if(firstTime<0){firstTime=timestamp;playbackStart=std::chrono::steady_clock::now();}const auto due=playbackStart+std::chrono::microseconds(std::max<LONGLONG>(0,(timestamp-firstTime)/10));{std::unique_lock<std::mutex> lock(mutex_);if(cv_.wait_until(lock,due,[&]{return generation!=generation_||stop_.load();}))break;if(generation!=generation_||stop_.load())break;if(frame_.pixels&&frame_.pixels.use_count()==1)spare=std::const_pointer_cast<std::vector<uint8_t>>(frame_.pixels);frame_.pixels=std::move(pixels);frame_.serial++;frame_.done=false;frame_.doneAt=0;}
   }
  }
  {std::lock_guard<std::mutex> lock(mutex_);if(generation==generation_&&!stop_.load()){frame_.done=true;frame_.doneAt=now();}}
  CoUninitialize();
 }
public:
 ~VideoPlayback(){stop();}
 void configure(const std::filesystem::path& path,unsigned width,unsigned height,unsigned loops=1){stop();path_=path;width_=width;height_=height;loops_=loops;}
 void restart(){stop();stop_=false;started_=true;{std::lock_guard<std::mutex> lock(mutex_);++generation_;frame_={};}const auto generation=generation_;thread_=std::thread([this,generation]{run(path_,width_,height_,loops_,generation);});}
 void start_if_needed(){if(!started_.load())restart();}
 void stop(){stop_=true;started_=false;{std::lock_guard<std::mutex> lock(mutex_);++generation_;}cv_.notify_all();if(thread_.joinable())thread_.join();stop_=false;}
 VideoFrame snapshot(){std::lock_guard<std::mutex> lock(mutex_);return frame_;}
};
