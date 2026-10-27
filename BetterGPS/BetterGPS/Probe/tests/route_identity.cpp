#include "route_identity.hpp"
#include "route_publication.hpp"
#include <stdexcept>
#include <iostream>
int main(){
    using namespace bettergps;
    RouteSnapshot route; route.route_task=reinterpret_cast<void*>(0x1234);
    route.physical.resize(2); route.logical.resize(1);
    route.physical[0].node_uid=1;route.physical[1].node_uid=2;
    route.physical[0].raw.total_distance_till_end=100;
    route.physical[1].raw.distance_from_previous=100;
    auto baseline=RouteIdentityFingerprint(route);int checks=0;
    auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("route identity mismatch");};
    auto changed=route;changed.route_task=reinterpret_cast<void*>(0x5678);check(RouteIdentityFingerprint(changed)!=baseline);
    changed=route;changed.physical[1].evidence.x=32;check(RouteIdentityFingerprint(changed)!=baseline);
    changed=route;changed.physical[1].evidence.forward=123;check(RouteIdentityFingerprint(changed)!=baseline);
    changed=route;changed.physical[0].raw.total_distance_till_end+=20;check(RouteIdentityFingerprint(changed)!=baseline);
    changed=route;changed.physical[1].raw.distance_from_previous+=20;check(RouteIdentityFingerprint(changed)!=baseline);
    changed=route;changed.physical[0].raw.distance_from_previous+=20;changed.physical[0].raw.time_from_previous+=5;check(RouteIdentityFingerprint(changed)==baseline);
    changed=route;changed.logical[0].raw.node_id=9;check(RouteIdentityFingerprint(changed)!=baseline);
    std::uint64_t pending=0,accepted=0;
    check(DecideRoutePublication(1,accepted,pending,false)==RoutePublication::Stabilize);
    check(DecideRoutePublication(1,accepted,pending,false)==RoutePublication::Publish);accepted=1;
    check(DecideRoutePublication(1,accepted,pending,true)==RoutePublication::Retain);
    check(DecideRoutePublication(2,accepted,pending,true)==RoutePublication::Stabilize);
    // Startup transient restores the same route; do not leave the backend in
    // RECALCULATING just because this fingerprint was published previously.
    check(DecideRoutePublication(1,accepted,pending,false)==RoutePublication::Stabilize);
    check(DecideRoutePublication(1,accepted,pending,false)==RoutePublication::Publish);
    // A full producer queue must leave it eligible for retry, not Retain.
    check(DecideRoutePublication(1,accepted,pending,false)==RoutePublication::Publish);
    check(DecideRoutePublication(1,accepted,pending,true)==RoutePublication::Retain);
    pending=0;check(DecideRoutePublication(1,accepted,pending,false)==RoutePublication::Stabilize);
    check(DecideRoutePublication(1,accepted,pending,false)==RoutePublication::Publish);
    std::cout<<"PASS: "<<checks<<" route identity and startup recovery checks\n";
}
