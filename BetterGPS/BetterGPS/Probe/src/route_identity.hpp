#pragma once
#include "probe.hpp"
namespace bettergps {
inline std::uint64_t RouteIdentityFingerprint(const RouteSnapshot& route)
{
    std::uint64_t hash=14695981039346656037ull;
    auto add=[&](const auto& value){
        const auto* bytes=reinterpret_cast<const unsigned char*>(&value);
        for(std::size_t i=0;i<sizeof(value);++i){hash^=bytes[i];hash*=1099511628211ull;}
    };

        const std::uint64_t physical_count = route.physical.size();
        const std::uint64_t logical_count = route.logical.size();
        add( physical_count);
        add( logical_count);

        // Task identity and fixed node evidence distinguish reroutes with identical UIDs.
        // The first item's approach distance and all times remain excluded: they can
        // change while driving. Distances after the first node describe the fixed tail.
        add( reinterpret_cast<std::uintptr_t>(route.route_task));
        for (std::size_t i=0;i<route.physical.size();++i)
        {
            const auto& item=route.physical[i];
            add( item.node_uid);
            const auto& n=item.evidence;
            add(n.x); add(n.y); add(n.z);
            add(n.qw); add(n.qx); add(n.qy); add(n.qz);
            add(n.backward); add(n.forward);
            add(n.backward_valid); add(n.forward_valid);
            if(i>0) add(item.raw.distance_from_previous);
            add(item.raw.total_distance_till_end);
        }
        for (const auto& item : route.logical)
        {
            add( item.raw.type);
            add( item.raw.node_id);
            add( item.item_uid);
        }
        return hash;
}
}
