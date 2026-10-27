#pragma once
#include <cstdint>
namespace bettergps {
enum class RoutePublication { Retain, Stabilize, Publish };
inline RoutePublication DecideRoutePublication(std::uint64_t fingerprint,std::uint64_t accepted,
    std::uint64_t& pending,bool sourceValid){
    // A status frame invalidates the consumer's generation even if ATS then
    // restores the previously accepted route. Resend it after two stable polls.
    if(sourceValid&&accepted==fingerprint)return RoutePublication::Retain;
    if(pending!=fingerprint){pending=fingerprint;return RoutePublication::Stabilize;}
    return RoutePublication::Publish;
}
}
