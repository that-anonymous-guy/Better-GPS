#include "../HookBatch.h"
#include <stdexcept>
#include <iostream>
#include <map>
using namespace bettergps_overlay;
static void check(bool b){if(!b)throw std::runtime_error("Hook batch transaction failed");}
int main(){try{
 auto* a=reinterpret_cast<void*>(1);auto* b=reinterpret_cast<void*>(2);auto* other=reinterpret_cast<void*>(3);
 for(int mode=0;mode<4;++mode){std::map<void*,bool> queued{{a,false},{b,false},{other,false}},active=queued;unsigned applies=0,enables=0;
  auto enable=[&](void* p){check(p&&p!=other);++enables;if(mode==2&&p==b)return MH_ERROR_NOT_CREATED;queued[p]=true;return MH_OK;};
  auto disable=[&](void* p){check(p&&p!=other);queued[p]=false;return MH_OK;};
  auto apply=[&]{++applies;if(mode==3&&applies==1){active[a]=true;return MH_ERROR_MEMORY_PROTECT;}active=queued;return MH_OK;};
  auto status=enable_hook_batch_impl(mode==0?std::vector<void*>{}:std::vector<void*>{a,b},enable,disable,apply);
  check(!active[other]&&!queued[other]);
  if(mode==0)check(status==MH_OK&&!applies&&!enables);
  if(mode==1)check(status==MH_OK&&applies==1&&active[a]&&active[b]);
  if(mode==2)check(status==MH_ERROR_NOT_CREATED&&!applies&&!queued[a]&&!queued[b]&&!active[a]);
  if(mode==3)check(status==MH_ERROR_MEMORY_PROTECT&&applies==2&&!active[a]&&!active[b]&&!queued[a]&&!queued[b]);
 }
 std::cout<<"PASS: subsystem-only hook batch, single apply, empty cohort, queue failure cancellation and partial-apply rollback.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
