#pragma once
namespace bettergps_overlay {
inline std::atomic<int> navplusTemperatureF{0};
inline std::atomic<bool> navplusHaveTemperature{false};
inline bool navplusTemperatureLayout=false;
inline bool navplus_temperature_resolve(const MapImage& image){
 // ATS 1.61 native dashboard update: group 0 is ambient temperature (ID 1000),
 // formatted into text_common.value, not oil/water/brake temperature.
 const std::pair<size_t,std::vector<uint8_t>> checks[]={
  {0x540e14,{0x4d,0x8d,0xa5,0x30,0x01,0x00,0x00}},
  {0x54129e,{0x48,0x8b,0x58,0x08,0x48,0x8b,0x40,0x10}},
  {0x5412b7,{0x48,0x8b,0x97,0x08,0x01,0x00,0x00}}};
 navplusTemperatureLayout=false;for(const auto& c:checks)if(!image.contains(uintptr_t(image.base)+c.first,c.second.size())||!image.executable(uintptr_t(image.base)+c.first)||memcmp(image.base+c.first,c.second.data(),c.second.size()))return false;
 navplusTemperatureLayout=true;return true;
}
inline bool navplus_parse_temperature(const std::string& value,int& fahrenheit){
 // Native formatter is "%d|F" or "%d|C". Never substitute an unrelated number.
 const auto sep=value.find('|');if(sep==std::string::npos||sep+2!=value.size()||(value.back()!='F'&&value.back()!='C'))return false;
 try{size_t used=0;double n=std::stod(value.substr(0,sep),&used);if(used!=sep||!std::isfinite(n))return false;if(value.back()=='C')n=n*1.8+32;if(n < -120||n>160)return false;fahrenheit=int(std::lround(n));return true;}catch(...){return false;}
}
inline bool navplus_temperature_read(void* owner,int& fahrenheit){
 if(!navplusTemperatureLayout||!(managed_navplus_vehicle(managed_vehicle())||managed_icc5_vehicle(managed_vehicle())))return false;const auto base=uintptr_t(owner);
 if(!map_memory(base+0x138,16))return false;const auto groups=*(uintptr_t*)(base+0x138),count=*(uint64_t*)(base+0x140);if(!count||count>128||!map_memory(groups,24))return false;
 const auto values=*(uintptr_t*)(groups+8),n=*(uint64_t*)(groups+16);if(!n||n>128||!map_memory(values,size_t(n)*8))return false;
 for(size_t i=0;i<n;++i){const auto node=*(uintptr_t*)(values+i*8);if(!map_memory(node,0x110)||*(unsigned*)(node+0x14)!=1000)continue;const auto text=*(uintptr_t*)(node+0x108);std::string value;for(size_t j=0;j<24;++j){MEMORY_BASIC_INFORMATION memory{};if(!text||!VirtualQuery((void*)(text+j),&memory,sizeof(memory))||memory.State!=MEM_COMMIT||(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(memory.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))break;char c=*(const char*)(text+j);if(!c){if(navplus_parse_temperature(value,fahrenheit))return true;break;}value+=c;}}
 return false;
}
inline void navplus_temperature_sample(void* owner){int f=0;const bool valid=navplus_temperature_read(owner,f);if(valid)navplusTemperatureF=f;navplusHaveTemperature=valid;}
inline std::string navplus_temperature_text(){return navplusHaveTemperature?std::to_string(navplusTemperatureF.load())+"f":"--f";}
}
