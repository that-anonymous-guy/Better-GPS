#pragma once
#include <MinHook.h>
#include <vector>
namespace bettergps_overlay {
// New, disabled hooks belonging to ONE completed subsystem only. No
// MH_ALL_HOOKS: do not enable unrelated or partially constructed hooks.
template<class QueueEnable,class QueueDisable,class Apply>
inline MH_STATUS enable_hook_batch_impl(const std::vector<void*>& addresses,QueueEnable enable,QueueDisable disable,Apply apply){
 if(addresses.empty())return MH_OK;
 size_t queued=0;for(;queued<addresses.size();++queued){const auto status=enable(addresses[queued]);if(status!=MH_OK){for(size_t i=0;i<queued;++i)disable(addresses[i]);return status;}}
 const auto status=apply();if(status!=MH_OK){for(auto address:addresses)disable(address);apply();}return status;
}
inline MH_STATUS enable_hook_batch(const std::vector<void*>& addresses){return enable_hook_batch_impl(addresses,MH_QueueEnableHook,MH_QueueDisableHook,MH_ApplyQueued);}
}
