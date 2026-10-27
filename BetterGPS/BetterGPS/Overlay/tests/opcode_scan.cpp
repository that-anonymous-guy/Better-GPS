#include "../OpcodeScan.h"
#include <vector>
#include <iostream>
#include <stdexcept>
#include <Windows.h>
static void check(bool ok){if(!ok)throw std::runtime_error("Opcode candidate bounds changed");}
int main(){try{
 for(size_t size=0;size<300;++size){std::vector<uint8_t> data(size+1);for(size_t i=0;i<size;++i)data[i]=(i%11==0?0x48:i%7==0?0x8b:0);auto begin=reinterpret_cast<uintptr_t>(data.data());
  for(size_t tail:{size_t(1),size_t(8),size_t(19),size_t(168),size_t(301)})for(uint8_t byte:{uint8_t(0x48),uint8_t(0x8b),uint8_t(0xff)}){
   std::vector<uintptr_t> expected,actual;for(size_t i=0;i+tail<=size;++i)if(data[i]==byte)expected.push_back(begin+i);
   for(auto at=bettergps_overlay::opcode_candidate(begin,begin+size,tail,byte);at;at=bettergps_overlay::opcode_candidate(at+1,begin+size,tail,byte))actual.push_back(at);
   check(actual==expected);
  }
 }
 // A no-access next page catches even an invisible one-byte SIMD overread.
 SYSTEM_INFO info{};GetSystemInfo(&info);auto* pages=static_cast<uint8_t*>(VirtualAlloc(nullptr,info.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));check(pages!=nullptr);DWORD old;check(VirtualProtect(pages+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&old));
 auto start=reinterpret_cast<uintptr_t>(pages),end=start+info.dwPageSize;pages[info.dwPageSize-1]=0x48;
 check(bettergps_overlay::opcode_candidate(start,end,1,0x48)==end-1);check(!bettergps_overlay::opcode_candidate(start,end,8,0x48));check(!bettergps_overlay::opcode_candidate(start,end,1,0xff));VirtualFree(pages,0,MEM_RELEASE);
 check(!bettergps_overlay::opcode_candidate(0,20,8,0x48));
 std::cout<<"PASS: optimized candidates equal bytewise scanning across 4500 boundary/missing/overlap cases and guarded page end.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
