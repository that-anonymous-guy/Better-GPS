#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace bettergps_overlay {
// Candidate prefilter only: never replace the caller's complete opcode,
// ownership, uniqueness, executable-section or ABI checks. Include the last
// legal start, and never read the incomplete tail of an image.
inline uintptr_t opcode_candidate(uintptr_t begin,uintptr_t end,size_t required,uint8_t first){
 if(!begin||begin>=end||!required||end-begin<required)return 0;
 return reinterpret_cast<uintptr_t>(std::memchr(reinterpret_cast<const void*>(begin),first,end-begin-required+1));
}
}
