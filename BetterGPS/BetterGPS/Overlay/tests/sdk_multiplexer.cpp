#include "../SdkMultiplexer.h"
#include <stdexcept>
#include <iostream>
using M=bettergps::SdkMultiplexer;
static void check(bool value){if(!value)throw std::runtime_error("SDK dispatcher regression");}
static scs_telemetry_channel_callback_t channel;static scs_context_t channelContext;
static scs_telemetry_event_callback_t event;static scs_context_t eventContext;
static int registrations,unregistrations,calls[2],events[2];
static scs_result_t SCSAPIFUNC reg(scs_string_t name,scs_u32_t,scs_value_type_t,scs_u32_t flags,scs_telemetry_channel_callback_t cb,scs_context_t ctx){if(std::string(name)=="unsupported")return SCS_RESULT_unsupported;check(!channel);check(flags&SCS_TELEMETRY_CHANNEL_FLAG_each_frame);channel=cb;channelContext=ctx;++registrations;return SCS_RESULT_ok;}
static scs_result_t SCSAPIFUNC unreg(scs_string_t,scs_u32_t,scs_value_type_t){check(channel);channel=nullptr;++unregistrations;return SCS_RESULT_ok;}
static scs_result_t SCSAPIFUNC reg_event(scs_event_t,scs_telemetry_event_callback_t cb,scs_context_t ctx){check(!event);event=cb;eventContext=ctx;return SCS_RESULT_ok;}
static scs_result_t SCSAPIFUNC unreg_event(scs_event_t){event=nullptr;return SCS_RESULT_ok;}
static void SCSAPIFUNC receive(scs_string_t,scs_u32_t,const scs_value_t*,scs_context_t ctx){++calls[(uintptr_t)ctx];}
static void SCSAPIFUNC receive_event(scs_event_t,const void*,scs_context_t ctx){++events[(uintptr_t)ctx];}
int main(){try{
 M::original.register_for_channel=reg;M::original.unregister_from_channel=unreg;M::original.register_for_event=reg_event;M::original.unregister_from_event=unreg_event;
 auto probe=M::parameters<0>(),overlay=M::parameters<1>();
 check(probe.register_for_channel("truck.speed",SCS_U32_NIL,SCS_VALUE_TYPE_float,0,receive,(void*)0)==SCS_RESULT_ok);
 check(overlay.register_for_channel("truck.speed",SCS_U32_NIL,SCS_VALUE_TYPE_float,SCS_TELEMETRY_CHANNEL_FLAG_no_value,receive,(void*)1)==SCS_RESULT_ok);
 check(registrations==1);scs_value_t value{};channel("truck.speed",SCS_U32_NIL,&value,channelContext);check(calls[0]==1&&calls[1]==1);
 channel("truck.speed",SCS_U32_NIL,nullptr,channelContext);check(calls[0]==1&&calls[1]==2);
 check(probe.unregister_from_channel("truck.speed",SCS_U32_NIL,SCS_VALUE_TYPE_float)==SCS_RESULT_ok);check(channel&&unregistrations==0);
 channel("truck.speed",SCS_U32_NIL,&value,channelContext);check(calls[0]==1&&calls[1]==3);
 check(probe.register_for_channel("unsupported",0,SCS_VALUE_TYPE_float,0,receive,nullptr)==SCS_RESULT_unsupported);check(M::channels.size()==1);
 check(probe.register_for_event(SCS_TELEMETRY_EVENT_frame_start,receive_event,(void*)0)==SCS_RESULT_ok);check(overlay.register_for_event(SCS_TELEMETRY_EVENT_frame_start,receive_event,(void*)1)==SCS_RESULT_ok);
 event(SCS_TELEMETRY_EVENT_frame_start,nullptr,eventContext);check(events[0]==1&&events[1]==1);
 probe.unregister_from_event(SCS_TELEMETRY_EVENT_frame_start);event(SCS_TELEMETRY_EVENT_frame_start,nullptr,eventContext);check(events[0]==1&&events[1]==2);
 M::reset();check(!channel&&!event&&unregistrations==1&&M::channels.empty()&&M::events.empty());
 std::cout<<"PASS: one SDK subscription, both internal modules receive data, isolated unregister, missing telemetry and unsupported channel handled."<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
