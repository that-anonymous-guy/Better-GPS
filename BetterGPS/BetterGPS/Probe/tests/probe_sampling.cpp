
#include "probe.hpp"
#include <iostream>
#include <chrono>
#include <stdexcept>
using namespace bettergps;
namespace bettergps {struct ProbeTestAccess {static void Run(){
 Probe probe(nullptr);std::vector<uint8_t> gps(layout::GPS_MANAGER_SIZE),task(layout::ROUTE_TASK_SIZE);std::vector<layout::NodeItemWin> nodes(5000);std::vector<layout::PhysicalRouteItemWin> physical(5000);std::vector<layout::RouteItemWin> logical(5000);
 for(size_t i=0;i<nodes.size();++i){nodes[i].uid=i+1;nodes[i].qw=1;physical[i].node=&nodes[i];physical[i].distance_from_previous=10;physical[i].total_distance_till_end=float(5000-i)*10;logical[i].item_ptr=&nodes[i];logical[i].type=0;logical[i].total_distance_till_end=physical[i].total_distance_till_end;}
 auto put=[](void* p,size_t offset,const auto& v){memcpy((uint8_t*)p+offset,&v,sizeof(v));};void* taskPtr=task.data();put(gps.data(),layout::GPS_ROUTE_TASK,taskPtr);layout::ArrayDynWin ph{},lh{};ph.value=physical.data();ph.size=ph.capacity=physical.size();lh.value=logical.data();lh.size=lh.capacity=logical.size();put(task.data(),layout::ROUTE_TASK_PHYSICAL_ITEMS,ph);put(task.data(),layout::ROUTE_TASK_ROUTE_ITEMS,lh);
 auto check=[](bool x,const char* message){if(!x)throw std::runtime_error(message);};
 RouteSnapshot route;auto t=std::chrono::steady_clock::now();check(probe.ReadRoute(gps.data(),route),"Full fixture snapshot invalid");double full=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();probe.RememberRoute(route,1000);probe.capture_status_="VALID";probe.accepted_fingerprint_=route.fingerprint;
 t=std::chrono::steady_clock::now();for(int i=0;i<100;++i)check(probe.RouteUnchanged(gps.data(),1100),"Stable route not reused");double fast=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()/100;
 physical[0].pad_0008[0]=73;check(probe.RouteUnchanged(gps.data(),1100),"Padding caused redundant snapshot");physical[0].total_distance_till_end+=10;check(!probe.RouteUnchanged(gps.data(),1100),"Route metric change missed");physical[0].total_distance_till_end-=10;
 logical[0].node_id++;check(!probe.RouteUnchanged(gps.data(),1100),"Logical branch change missed");logical[0].node_id--;
 check(!probe.RouteUnchanged(gps.data(),3000),"Periodic evidence revalidation missing");void* cleared=nullptr;put(gps.data(),layout::GPS_ROUTE_TASK,cleared);check(!probe.RouteUnchanged(gps.data(),1100),"Cleared route missed");put(gps.data(),layout::GPS_ROUTE_TASK,taskPtr);
 nodes[0].qw=0;check(!probe.ReadRoute(gps.data(),route),"Invalid node evidence accepted");nodes[0].qw=1;ph.value=nullptr;put(task.data(),layout::ROUTE_TASK_PHYSICAL_ITEMS,ph);check(!probe.RouteUnchanged(gps.data(),1100),"Invalid array memory accepted");
 t=std::chrono::steady_clock::now();for(int i=0;i<10000;++i)Probe::FrameEndCallback(0,nullptr,&probe);double callbacks=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
 ph.value=physical.data();put(task.data(),layout::ROUTE_TASK_PHYSICAL_ITEMS,ph);probe.accepted_fingerprint_=probe.pending_fingerprint_=0;probe.capture_status_.clear();probe.initialized_=true;probe.gps_address_=gps.data();probe.vehicle_.paused=false;probe.capture_.Start();probe.StartSamplingWorker();std::this_thread::sleep_for(std::chrono::milliseconds(850));auto stop=std::chrono::steady_clock::now();probe.Shutdown();check(!probe.sampling_worker_.joinable()&&!probe.initialized_.load()&&probe.accepted_fingerprint_!=0,"Sampling worker failed to publish stable route or stop");double stopMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-stop).count();check(stopMs<1000,"Sampling worker shutdown blocked");
 std::cout<<"PASS sampling: full_5000_nodes_ms="<<full<<" unchanged_route_ms="<<fast<<" 10000_frame_callbacks_ms="<<callbacks<<" stop_ms="<<stopMs<<"; recalc, clear, padding, revalidation, invalid memory/evidence and real worker lifecycle checks"<<std::endl;
 }};}
int main(){try{ProbeTestAccess::Run();return 0;}catch(std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
