#pragma once
#include <compressapi.h>
#include <fstream>
#include <vector>
#include <memory>
namespace bettergps_overlay {
// Lossless BGRA frames. Separate from the BC1 BGA reader used by Ford/GPS.
// XPRESS-HUFF buffer mode bounds memory and needs no video/decoder thread.
struct ClusterMovie {
 unsigned width=0,height=0,count=0;
 std::vector<uint16_t> timeline;
 std::vector<uint32_t> offsets,sizes;
 std::vector<uint8_t> blocks;
 DECOMPRESSOR_HANDLE decoder=nullptr;
 ClusterMovie()=default;ClusterMovie(const ClusterMovie&)=delete;ClusterMovie& operator=(const ClusterMovie&)=delete;
 ~ClusterMovie(){if(decoder)CloseDecompressor(decoder);}
 bool decode(unsigned frame,std::vector<uint8_t>& out)const{
  if(frame>=count||!decoder)return false;const auto index=timeline[frame];if(index>=sizes.size())return false;
  out.resize(size_t(width)*height*4);SIZE_T written=0;
  if(!Decompress(decoder,blocks.data()+offsets[index],sizes[index],out.data(),out.size(),&written)||written!=out.size()){out.clear();return false;}return true;
 }
 static std::shared_ptr<ClusterMovie> read(const std::filesystem::path& path,unsigned expectedWidth=1028,unsigned expectedHeight=512,unsigned expectedFrames=270){
  std::ifstream file(path,std::ios::binary|std::ios::ate);const auto length=file.tellg();if(length<20||length>80ll*1024*1024)return {};
  file.seekg(0);uint32_t header[5]{};file.read((char*)header,sizeof(header));
  if(!file||expectedWidth<1||expectedWidth>2048||expectedHeight<1||expectedHeight>1024||expectedFrames<1||expectedFrames>1800||header[0]!=0x33434742||header[1]!=expectedWidth||header[2]!=expectedHeight||header[3]!=expectedFrames||!header[4]||header[4]>expectedFrames)return {};
  auto result=std::make_shared<ClusterMovie>();result->width=header[1];result->height=header[2];result->count=header[3];result->timeline.resize(header[3]);result->sizes.resize(header[4]);
  file.read((char*)result->timeline.data(),result->timeline.size()*2);file.read((char*)result->sizes.data(),result->sizes.size()*4);if(!file)return {};
  uint64_t bytes=0;for(auto size:result->sizes){if(!size||size>expectedWidth*expectedHeight*4+65536)return {};result->offsets.push_back(uint32_t(bytes));bytes+=size;}
  if(20ull+result->timeline.size()*2+result->sizes.size()*4+bytes!=uint64_t(length))return {};
  for(auto frame:result->timeline)if(frame>=result->sizes.size())return {};
  result->blocks.resize(size_t(bytes));file.read((char*)result->blocks.data(),result->blocks.size());
  if(!file||!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&result->decoder))return {};
  std::vector<uint8_t> first;if(!result->decode(0,first))return {};return result;
 }
};
}
