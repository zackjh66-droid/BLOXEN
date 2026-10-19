#include "../../runtime-native-route/NativeSystem.h"
#include <iostream>
extern "C" int HWOwnedStackProbeFixture(void* probe);
static DWORD WINAPI ProbeThread(void* probe){return static_cast<DWORD>(HWOwnedStackProbeFixture(probe));}
int main(){
 auto probe=Hollowwake::ResolveNativeSystemImport("KERNEL32.dll","__chkstk");
 if(!probe||probe!=Hollowwake::ResolveNativeSystemImport("KERNELBASE.dll","__chkstk"))return 1;
 DWORD id=0;auto thread=CreateThread(nullptr,1024*1024,ProbeThread,probe,STACK_SIZE_PARAM_IS_A_RESERVATION,&id);if(!thread)return 2;
 if(WaitForSingleObject(thread,10000)!=WAIT_OBJECT_0){CloseHandle(thread);return 3;}DWORD result=0;bool ok=GetExitCodeThread(thread,&result)!=FALSE;CloseHandle(thread);
 if(!ok||result!=621)return 4;
 std::cout<<"PASS real SDK x64 stack probe on fresh OS thread: RAX ABI preserved, 64 KiB guard-page growth, writes/readbacks across 16 pages. No game/Xbox execution.\n";return 0;
}
