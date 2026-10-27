#include "../../shared/PerformanceDiagnostics.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){using namespace bettergps_diag;Init(1);if(!state.header||!state.control)return 2;
 auto begin=Now();for(int i=0;i<1000000;++i){Scope disabled(ComposePhone);}auto end=Now();std::cout<<"disabled_scope_ns="<<double(end-begin)*1e9/state.header->frequency/1000000<<std::endl;
 if(state.header->write||((MetricData*)((uint8_t*)state.header+128))[ComposePhone].count)throw std::runtime_error("disabled instrumentation wrote measurements");
 if(argc>1){bool seen=false,stopped=false;const auto deadline=GetTickCount64()+7000;while(GetTickCount64()<deadline){Refresh();Frame((void*)0x1000,true,false);if(state.active.load()){seen=true;{Scope scope(ComposePhone);Sleep(2);}if((GetTickCount64()/50)%10==0){Scope scope(SettingsSave);Sleep(35);}}else if(seen)stopped=true;Sleep(8);}std::cout<<"capture_seen="<<seen<<" lease_stopped="<<stopped<<std::endl;if(!seen||!stopped)return 3;}
 else{state.control->magic=0x42475043;state.control->version=1;state.control->frequency=state.header->frequency;state.control->heartbeat=Now();state.control->enabled=1;state.control->session=1;Refresh();if(!state.active.load())return 4;{Scope scope(SettingsSave);Sleep(2);}auto* stats=(MetricData*)((uint8_t*)state.header+128);if(stats[SettingsSave].count!=1)return 5;
 Frame((void*)0x2000,true,false);Frame((void*)0x2000,false,false);Frame((void*)0x2000,true,false);auto* ring=(Event*)((uint8_t*)state.header+7040);if(ring[(state.header->write-1)%1024].value&1)return 8;Frame((void*)0x2000,true,false);if(!(ring[(state.header->write-1)%1024].value&1))return 9;
 state.control->heartbeat=Now()-state.header->frequency*4;Refresh();if(state.active.load())return 6;auto saved=stats[SettingsSave].count;{Scope scope(SettingsSave);}if(stats[SettingsSave].count!=saved)return 7;std::cout<<"PASS: disabled no writes, active counters/histogram, focus-transition exclusion, automatic stale-heartbeat expiry"<<std::endl;}
 Stop();return 0;
}
