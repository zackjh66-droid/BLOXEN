// Original hosted test. No Fortnite or third-party runtime code is executed.
#include "../NativeSystem.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
static void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){try{
 using namespace Hollowwake;
 auto startup=reinterpret_cast<void(WINAPI*)(LPSTARTUPINFOW)>(ResolveNativeSystemImport("kernel32.dll","GetStartupInfoW"));Check(startup,"Actual OS GetStartupInfoW missing");
 struct Guarded{unsigned long long before;STARTUPINFOW value;unsigned long long after;} observed{},expected{};
 observed.before=expected.before=0x621621621;observed.after=expected.after=0x740740740;
 startup(&observed.value);::GetStartupInfoW(&expected.value);
 Check(observed.before==expected.before&&observed.after==expected.after,"Startup info write exceeded ABI structure");
 Check(!memcmp(&observed.value,&expected.value,sizeof(STARTUPINFOW)),"Guest-resolved startup info differs from real OS startup info");
 auto create=reinterpret_cast<HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD)>(ResolveNativeSystemImport("api-ms-win-core-processthreads-l1-1-0.dll","CreateThread"));Check(create,"OS thread export missing");
 DWORD input=621,threadId=0;HANDLE thread=create(nullptr,0,[](LPVOID input)->DWORD{return *static_cast<DWORD*>(input);},&input,0,&threadId);Check(thread&&threadId,"OS thread creation failed");Check(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"Thread wait failed");DWORD result=0;Check(GetExitCodeThread(thread,&result)&&result==621,"Resolved thread did not execute actual input task");CloseHandle(thread);
 Check(!ResolveNativeSystemImport("kernel32.dll","HollowwakeAbsentExport"),"Missing export fabricated a function");
 Check(!ResolveNativeSystemImport("kernel32.dll","#999999"),"Invalid ordinal accepted");Check(!ResolveNativeSystemImport("WS2_32.dll","connect"),"Native network unexpectedly enabled");
 Check(!ResolveNativeSystemImport("C:\\arbitrary\\kernel32.dll","GetStartupInfoW"),"Arbitrary module path accepted");
 bool legacy=NativeLegacyCrtAvailable();if(legacy){
  auto alloc=reinterpret_cast<void*(__cdecl*)(size_t)>(ResolveNativeSystemImport("msvcrt.dll","malloc"));auto grow=reinterpret_cast<void*(__cdecl*)(void*,size_t)>(ResolveNativeSystemImport("msvcrt.dll","realloc"));auto free=reinterpret_cast<void(__cdecl*)(void*)>(ResolveNativeSystemImport("msvcrt.dll","free"));Check(alloc&&grow&&free,"Legacy family incomplete");auto p=static_cast<unsigned char*>(alloc(32));Check(p,"System legacy allocation failed");memset(p,0x62,32);p=static_cast<unsigned char*>(grow(p,96));Check(p,"System legacy reallocation failed");for(int i=0;i<32;++i)Check(p[i]==0x62,"System legacy allocation preservation failed");free(p);
 }
 std::cout<<"PASS actual OS GetStartupInfoW ABI+canaries, real thread task+exit code, missing exports and scope rejection; legacyFamily="<<(legacy?"tested":"unavailable; fallback required")<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
