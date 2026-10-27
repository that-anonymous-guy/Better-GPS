#include "../../Shared/StartupTrace.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
static std::vector<std::string> lines;
static void SCSAPIFUNC record(scs_log_type_t,scs_string_t text){lines.emplace_back(text);SetLastError(12345);}
static void check(bool value){if(!value)throw std::runtime_error("Startup trace regression");}
int main(){try{
 SetLastError(7654);
 {bettergps::StartupTrace trace(record,"unit");check(GetLastError()==7654);
  const auto result=trace.measure("delayed-value",[]{Sleep(40);SetLastError(4321);return std::make_unique<int>(42);});
  check(result&&*result==42&&GetLastError()==4321);
  trace.measure("void",[]{});
  try{trace.measure("throw",[]{throw std::runtime_error("expected");});}catch(const std::runtime_error&){}
  check(GetLastError()==4321);
 }
 check(GetLastError()==4321&&lines.size()==8);
 bool measured=false,unwound=false;
 for(const auto& line:lines){check(line.find("qpc_ms=")!=std::string::npos&&line.find("cpu_ms=")!=std::string::npos&&line.find("thread=")!=std::string::npos);
  if(line.find("event=end phase=delayed-value ")!=std::string::npos){auto pos=line.find("wall_ms=");check(pos!=std::string::npos&&std::strtod(line.c_str()+pos+8,nullptr)>=25.0);measured=true;}
  if(line.find("event=end phase=throw ")!=std::string::npos)unwound=true;
 }
 check(measured&&unwound);
 {bettergps::StartupTrace disabled(nullptr,"null-log");check(disabled.measure("value",[]{return 9;})==9);}
 std::cout<<"PASS: QPC startup trace, 40ms known delay, last-error preservation, move-only/void return, exception unwind and null logger.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
