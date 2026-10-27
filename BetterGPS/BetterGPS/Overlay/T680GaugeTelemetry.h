#pragma once
#include "T680GaugeValues.h"
namespace bettergps_overlay {
inline bool t680GaugeNativeLayout=false;
inline bool t680_gauge_resolve(const MapImage& image){
 const std::pair<size_t,std::vector<uint8_t>> checks[]={
  {0x541ff0,{0x49,0x8b,0x85,0x18,0x01,0,0,0x48,0x8b,0x0b,0xf3,0x0f,0x10,0x88,0x80,0x12,0,0}},
  {0x543c55,{0xf3,0x0f,0x10,0xb0,0x60,0x11,0,0}}};
 t680GaugeNativeLayout=false;for(const auto& c:checks){const auto at=uintptr_t(image.base)+c.first;if(!image.contains(at,c.second.size())||!image.executable(at)||memcmp((void*)at,c.second.data(),c.second.size()))return false;}t680GaugeNativeLayout=true;return true;
}
inline void t680_gauge_sample(void* owner){
 const auto epoch=vehicleStart.epoch.load();const auto base=uintptr_t(owner);
 if(!t680GaugeNativeLayout||!vehicleStart.eligible||managed_vehicle()!="kenworth.t680_2022"||!map_memory(base+0x118,8)){t680Gauges.native(epoch,false);return;}
 const auto source=*(uintptr_t*)(base+0x118);if(!source||!map_memory(source+0x1160,4)||!map_memory(source+0x1280,4)){t680Gauges.native(epoch,false);return;}
 const double diff=*(float*)(source+0x1160),turbo=*(float*)(source+0x1280);const bool valid=std::isfinite(diff)&&diff>=-100&&diff<=400&&std::isfinite(turbo)&&turbo>=0&&turbo<=5;
 t680Gauges.native(epoch,valid,diff,turbo);
}
inline SCSAPI_VOID t680_gauge_sdk(const scs_string_t,const scs_u32_t,const scs_value_t* value,const scs_context_t context){
 const bool oil=context!=nullptr;const double n=value&&value->type==SCS_VALUE_TYPE_float?value->value_float.value:NAN;
 t680Gauges.sdk(oil,std::isfinite(n)&&(oil?n>=-100&&n<=400:n>=0&&n<=10),n);
}
}
