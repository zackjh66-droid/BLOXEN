// Original owned arithmetic fixture. No game image is loaded or executed.
#include <windows.h>
#include <vector>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
using Entry=unsigned __int64(__fastcall*)(void*,unsigned);
void Check(bool value){if(!value)throw std::runtime_error("Stack address repair behavior differs");}
void AddBranch(std::vector<BYTE>& code,size_t destination){
 auto offset=static_cast<INT32>(destination)-static_cast<INT32>(code.size()+5);
 code.push_back(0xe9);auto bytes=reinterpret_cast<BYTE*>(&offset);code.insert(code.end(),bytes,bytes+4);
}
unsigned __int64 Run(void* frame,unsigned value,bool repaired,bool reference){
 // Microsoft x64: RCX=frame, EDX=value. Preserve nonvolatile RBP.
 const BYTE prefix[]={0x55,0x48,0x89,0xcd,0x89,0xd0};
 const BYTE original[]={0x8d,0x8d,0x90,0,0,0,0x83,0xe9,4};
 const BYTE full[]={0x48,0x8d,0x8d,0x8c,0,0,0};
 const BYTE tail[]={0x89,0x01,0x9c,0x58,0x5d,0xc3};
 std::vector<BYTE> code(prefix,prefix+sizeof(prefix));
 if(repaired){AddBranch(code,128);code.insert(code.end(),4,0x90);}
 else code.insert(code.end(),original,original+sizeof(original));
 if(reference)code.insert(code.end(),full,full+sizeof(full));
 code.insert(code.end(),tail,tail+sizeof(tail));
 if(repaired){code.resize(128,0x90);code.insert(code.end(),original,original+sizeof(original));code.insert(code.end(),full,full+sizeof(full));AddBranch(code,sizeof(prefix)+sizeof(original));}
 auto storage=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);Check(storage!=nullptr);
 memcpy(storage,code.data(),code.size());DWORD previous=0;
 if(!VirtualProtect(storage,4096,PAGE_EXECUTE_READ,&previous)){VirtualFree(storage,0,MEM_RELEASE);Check(false);}
 Check(FlushInstructionCache(GetCurrentProcess(),storage,code.size())!=FALSE);
 auto flags=reinterpret_cast<Entry>(storage)(frame,value);
 Check(VirtualFree(storage,0,MEM_RELEASE)!=FALSE);return flags;
}
void* AllocateRange(ULONG_PTR start,ULONG_PTR end){
 for(auto address=start;address<end;address+=0x10000){auto memory=VirtualAlloc(reinterpret_cast<void*>(address),4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(memory)return memory;}return nullptr;
}
}
int main(){
 void* low=nullptr;void* high=nullptr;
 try{
  low=AllocateRange(0x10000000,0x11000000);high=AllocateRange(0x100000000ull,0x101000000ull);
  Check(low&&high);Check(reinterpret_cast<ULONG_PTR>(low)<0x100000000ull&&reinterpret_cast<ULONG_PTR>(high)>=0x100000000ull);
  auto lowTarget=reinterpret_cast<unsigned*>(static_cast<BYTE*>(low)+128);
  auto highTarget=reinterpret_cast<unsigned*>(static_cast<BYTE*>(high)+128);
  auto flags=Run(reinterpret_cast<void*>(reinterpret_cast<ULONG_PTR>(lowTarget)-0x8c),621,false,false);Check(*lowTarget==621);
  auto fixedFlags=Run(reinterpret_cast<void*>(reinterpret_cast<ULONG_PTR>(lowTarget)-0x8c),4526925,true,false);Check(*lowTarget==4526925);Check((flags&0x8d5)==(fixedFlags&0x8d5));
  auto expected=Run(reinterpret_cast<void*>(reinterpret_cast<ULONG_PTR>(highTarget)-0x8c),621,false,true);Check(*highTarget==621);
  auto actual=Run(reinterpret_cast<void*>(reinterpret_cast<ULONG_PTR>(highTarget)-0x8c),4526925,true,false);Check(*highTarget==4526925);Check((expected&0x8d5)==(actual&0x8d5));
  VirtualFree(low,0,MEM_RELEASE);VirtualFree(high,0,MEM_RELEASE);
  std::cout<<"PASS owned Windows x64 fixture: actual low/high memory writes, original arithmetic flags and ABI preserved; RW-to-RX direct branch repair. No game/Xbox execution.\n";return 0;
 }catch(const std::exception& error){if(low)VirtualFree(low,0,MEM_RELEASE);if(high)VirtualFree(high,0,MEM_RELEASE);std::cerr<<error.what()<<"\n";return 1;}
}
