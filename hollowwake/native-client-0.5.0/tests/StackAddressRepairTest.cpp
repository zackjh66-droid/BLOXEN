// Execute owned complete return frames. No game loading or emulated OS APIs.
#include <windows.h>
#include <vector>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "StackAddressRepairFixture.generated.h"
namespace {
void Check(bool value){if(!value)throw std::runtime_error("Complete return-frame repair failed");}
void* Allocate(ULONG_PTR start){
 for(auto at=start;at<start+0x1000000;at+=0x10000){auto memory=VirtualAlloc(reinterpret_cast<void*>(at),4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(memory)return memory;}return nullptr;
}
template<size_t N>void Fix(std::vector<BYTE>& code,const size_t(&offsets)[N],ULONG_PTR value){for(auto offset:offsets){Check(offset+8<=code.size());memcpy(code.data()+offset,&value,8);}}
void Case(bool lowStack,bool lowCode){
 void* code=Allocate(lowCode?0x10000000ull:0x100000000ull);void* stack=Allocate(lowStack?0x20000000ull:0x200000000ull);
 if(!code||!stack){if(code)VirtualFree(code,0,MEM_RELEASE);if(stack)VirtualFree(stack,0,MEM_RELEASE);Check(false);}
 try{
  Check((reinterpret_cast<ULONG_PTR>(code)<0x100000000ull)==lowCode);Check((reinterpret_cast<ULONG_PTR>(stack)<0x100000000ull)==lowStack);
  ULONGLONG output[11]={};std::vector<BYTE> bytes(kOwnedReturnCode,kOwnedReturnCode+sizeof(kOwnedReturnCode));
  Fix(bytes,kCodeFixups,reinterpret_cast<ULONG_PTR>(code));Fix(bytes,kStackFixups,reinterpret_cast<ULONG_PTR>(stack)+3072);Fix(bytes,kOutputFixups,reinterpret_cast<ULONG_PTR>(output));
  memcpy(code,bytes.data(),bytes.size());DWORD previous=0;Check(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&previous)!=FALSE);Check(FlushInstructionCache(GetCurrentProcess(),code,bytes.size())!=FALSE);
  auto result=reinterpret_cast<ULONGLONG(__cdecl*)()>(code)();Check(result==621&&output[0]==621);
  for(unsigned i=0;i<4;++i)Check(output[2+i]==output[6+i]);Check((output[1]&0x8d5)==(output[10]&0x8d5));
  VirtualFree(code,0,MEM_RELEASE);VirtualFree(stack,0,MEM_RELEASE);
 }catch(...){VirtualFree(code,0,MEM_RELEASE);VirtualFree(stack,0,MEM_RELEASE);throw;}
}
}
int main(){try{
 for(bool lowStack:{false,true})for(bool lowCode:{false,true})Case(lowStack,lowCode);
 std::cout<<"PASS owned Windows complete x64 frames: real computed RET continuation for all four low/high stack/code combinations; full target, 8-byte slot, RSP/RBP/RBX/RSI and arithmetic flags preserved. Production-generated trampoline bytes. No native game/Xbox execution.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
