#pragma once
#include <scssdk_telemetry.h>
#include <map>
#include <tuple>
#include <string>
#include <vector>
#include <algorithm>
namespace bettergps {
// The SDK accepts one callback per channel/type per plugin. Both internal DLLs
// subscribe through this dispatcher, so neither can replace the other's data.
struct SdkMultiplexer {
    struct ChannelSub{unsigned owner;scs_telemetry_channel_callback_t callback;scs_context_t context;scs_u32_t flags;};
    struct EventSub{unsigned owner;scs_telemetry_event_callback_t callback;scs_context_t context;};
    using Key=std::tuple<std::string,scs_u32_t,scs_value_type_t>;
    struct Channel{std::vector<ChannelSub> subscribers;};
    struct Event{std::vector<EventSub> subscribers;};
    inline static scs_telemetry_init_params_v100_t original{};
    inline static std::map<Key,Channel> channels;
    inline static std::map<scs_event_t,Event> events;
    static void SCSAPIFUNC channel_callback(scs_string_t name,scs_u32_t index,const scs_value_t* value,scs_context_t context){
        auto copy=static_cast<Channel*>(context)->subscribers;
        for(auto& s:copy)if(value||(s.flags&SCS_TELEMETRY_CHANNEL_FLAG_no_value))s.callback(name,index,value,s.context);
    }
    static void SCSAPIFUNC event_callback(scs_event_t event,const void* info,scs_context_t context){
        auto copy=static_cast<Event*>(context)->subscribers;for(auto& s:copy)s.callback(event,info,s.context);
    }
    template<unsigned Owner>static scs_result_t SCSAPIFUNC register_channel(scs_string_t name,scs_u32_t index,scs_value_type_t type,scs_u32_t flags,scs_telemetry_channel_callback_t callback,scs_context_t context){
        if(!name||!callback||!original.register_for_channel)return SCS_RESULT_invalid_parameter;
        Key key{name,index,type};auto found=channels.find(key);
        if(found==channels.end()){
            auto [it,added]=channels.emplace(key,Channel{});
            auto result=original.register_for_channel(name,index,type,flags|SCS_TELEMETRY_CHANNEL_FLAG_each_frame|SCS_TELEMETRY_CHANNEL_FLAG_no_value,&channel_callback,&it->second);
            if(result!=SCS_RESULT_ok){channels.erase(it);return result;}found=it;
        }
        auto& subs=found->second.subscribers;
        subs.erase(std::remove_if(subs.begin(),subs.end(),[](auto& s){return s.owner==Owner;}),subs.end());
        subs.push_back({Owner,callback,context,flags});return SCS_RESULT_ok;
    }
    template<unsigned Owner>static scs_result_t SCSAPIFUNC unregister_channel(scs_string_t name,scs_u32_t index,scs_value_type_t type){
        if(!name)return SCS_RESULT_invalid_parameter;auto it=channels.find(Key{name,index,type});if(it==channels.end())return SCS_RESULT_ok;
        auto& subs=it->second.subscribers;subs.erase(std::remove_if(subs.begin(),subs.end(),[](auto& s){return s.owner==Owner;}),subs.end());
        if(subs.empty()){auto result=original.unregister_from_channel?original.unregister_from_channel(name,index,type):SCS_RESULT_ok;channels.erase(it);return result;}return SCS_RESULT_ok;
    }
    template<unsigned Owner>static scs_result_t SCSAPIFUNC register_event(scs_event_t event,scs_telemetry_event_callback_t callback,scs_context_t context){
        if(!callback||!original.register_for_event)return SCS_RESULT_invalid_parameter;auto found=events.find(event);
        if(found==events.end()){auto [it,added]=events.emplace(event,Event{});auto result=original.register_for_event(event,&event_callback,&it->second);if(result!=SCS_RESULT_ok){events.erase(it);return result;}found=it;}
        auto& subs=found->second.subscribers;subs.erase(std::remove_if(subs.begin(),subs.end(),[](auto& s){return s.owner==Owner;}),subs.end());subs.push_back({Owner,callback,context});return SCS_RESULT_ok;
    }
    template<unsigned Owner>static scs_result_t SCSAPIFUNC unregister_event(scs_event_t event){
        auto it=events.find(event);if(it==events.end())return SCS_RESULT_ok;auto& subs=it->second.subscribers;subs.erase(std::remove_if(subs.begin(),subs.end(),[](auto& s){return s.owner==Owner;}),subs.end());
        if(subs.empty()){auto result=original.unregister_from_event?original.unregister_from_event(event):SCS_RESULT_ok;events.erase(it);return result;}return SCS_RESULT_ok;
    }
    template<unsigned Owner>static auto parameters(){auto p=original;p.register_for_channel=&register_channel<Owner>;p.unregister_from_channel=&unregister_channel<Owner>;p.register_for_event=&register_event<Owner>;p.unregister_from_event=&unregister_event<Owner>;return p;}
    static void reset(){for(auto& p:channels)if(original.unregister_from_channel)original.unregister_from_channel(std::get<0>(p.first).c_str(),std::get<1>(p.first),std::get<2>(p.first));for(auto& p:events)if(original.unregister_from_event)original.unregister_from_event(p.first);channels.clear();events.clear();}
};
}
