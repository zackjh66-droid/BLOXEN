// Original thread tasks and DLL/TLS callbacks. No game/third-party code runs.
#include "../GuestThreads.h"
#include "../NativeSystem.h"
#include <windows.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <cstdint>
using namespace Hollowwake;
struct State{bool a=false,b=false;std::vector<int> trace;};
thread_local State state;
static std::mutex resultsMutex;
static std::vector<std::vector<int>> completed;
static std::atomic<unsigned> invalid{0},disabledCalls{0};
static HANDLE bodyEvent=nullptr;
static BOOL WINAPI SeedA(){state.a=true;state.trace.push_back(1);return TRUE;}
static BOOL WINAPI SeedB(){state.b=true;state.trace.push_back(2);return TRUE;}
static void NTAPI TlsA(void*,DWORD reason,void*){
 if(!state.a||!state.b)++invalid;
 if(reason==DLL_THREAD_ATTACH)state.trace.push_back(3);
 if(reason==DLL_THREAD_DETACH){state.trace.push_back(10);std::lock_guard<std::mutex> lock(resultsMutex);completed.push_back(state.trace);}
}
static void NTAPI TlsB(void*,DWORD reason,void*){if(reason==DLL_THREAD_ATTACH)state.trace.push_back(5);if(reason==DLL_THREAD_DETACH)state.trace.push_back(8);}
static BOOL WINAPI EntryA(HINSTANCE,DWORD reason,void*){if(reason==DLL_THREAD_ATTACH)state.trace.push_back(4);if(reason==DLL_THREAD_DETACH)state.trace.push_back(9);return TRUE;}
static BOOL WINAPI EntryB(HINSTANCE,DWORD reason,void*){if(reason==DLL_THREAD_ATTACH)state.trace.push_back(6);if(reason==DLL_THREAD_DETACH)state.trace.push_back(7);return TRUE;}
static BOOL WINAPI DisabledEntry(HINSTANCE,DWORD,void*){++disabledCalls;return TRUE;}
static void Check(bool condition,const char* error){if(!condition)throw std::runtime_error(error);}
static DWORD WINAPI Task(void* value){if(state.trace!=std::vector<int>({1,2,3,4,5,6}))++invalid;SetEvent(bodyEvent);return *static_cast<DWORD*>(value)^0x621;}
static DWORD WINAPI EarlyExit(void* value){if(state.trace!=std::vector<int>({1,2,3,4,5,6}))++invalid;auto leave=reinterpret_cast<void(WINAPI*)(DWORD)>(ResolveGuestThreadImport("kernel32.dll","ExitThread"));leave(*static_cast<DWORD*>(value));return 0;}
static unsigned __stdcall CrtTask(void* value){return Task(value);}
static unsigned __stdcall CrtEarlyExit(void* value){auto leave=reinterpret_cast<void(__cdecl*)(unsigned)>(ResolveGuestThreadImport("msvcrt.dll","_endthreadex"));leave(*static_cast<DWORD*>(value));return 0;}
static void WaitAndCheck(HANDLE thread,DWORD expected){Check(thread,"Real guest thread creation failed");Check(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"Guest thread did not finish");DWORD value=0;Check(GetExitCodeThread(thread,&value)&&value==expected,"Guest thread exit code incorrect");CloseHandle(thread);}
int main(){try{
 Check(LoadLibraryExW(L"msvcrt.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)!=nullptr,"Fixture system CRT missing");
 bodyEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);Check(bodyEvent,"Body event creation failed");
 auto create=reinterpret_cast<HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD)>(ResolveGuestThreadImport("api-ms-win-core-processthreads-l1-1-0.dll","CreateThread"));Check(create,"Production CreateThread bridge not resolved");
 BeginGuestInitialization();int first=0,second=0,disabled=0;
 auto a=reinterpret_cast<HMODULE>(&first),b=reinterpret_cast<HMODULE>(&second),c=reinterpret_cast<HMODULE>(&disabled);
 RegisterGuestThreadModule(a,true,SeedA,{TlsA},EntryA);MarkGuestThreadModuleReady(a);
 RegisterGuestThreadModule(b,true,SeedB,{TlsB},EntryB);MarkGuestThreadModuleReady(b);
 RegisterGuestThreadModule(c,false,nullptr,{},DisabledEntry);Check(GuestDisableThreadLibraryCalls(c),"Non-TLS notification disabling failed");MarkGuestThreadModuleReady(c);
 Check(!GuestDisableThreadLibraryCalls(a),"Static TLS notification disabling incorrectly succeeded");
 DWORD input=4526925,id=0;auto suspended=create(nullptr,0,Task,&input,CREATE_SUSPENDED,&id);Check(suspended&&id,"Suspended real guest thread creation failed");Check(WaitForSingleObject(bodyEvent,30)==WAIT_TIMEOUT,"Suspended task unexpectedly ran");Check(ResumeThread(suspended)!=static_cast<DWORD>(-1),"Resume of real guest thread failed");Check(WaitForSingleObject(bodyEvent,50)==WAIT_TIMEOUT,"Task escaped process initialization serialization");EndGuestInitialization(true);
 WaitAndCheck(suspended,input^0x621);
 WaitAndCheck(create(nullptr,0,EarlyExit,&input,0,nullptr),input);
 if(NativeLegacyCrtAvailable()){
  auto begin=reinterpret_cast<uintptr_t(__cdecl*)(void*,unsigned,unsigned(__stdcall*)(void*),void*,unsigned,unsigned*)>(ResolveGuestThreadImport("msvcrt.dll","_beginthreadex"));Check(begin,"Native-family CRT thread adapter missing");
  WaitAndCheck(reinterpret_cast<HANDLE>(begin(nullptr,0,CrtTask,&input,0,nullptr)),input^0x621);
  WaitAndCheck(reinterpret_cast<HANDLE>(begin(nullptr,0,CrtEarlyExit,&input,0,nullptr)),input);
 }
 Check(!invalid&&disabledCalls==0,"Guest TLS seed/callback ordering or disabled notifications failed");
 {std::lock_guard<std::mutex> lock(resultsMutex);Check(completed.size()==4,"Thread attach/detach pairing count wrong");for(auto& trace:completed)Check(trace==std::vector<int>({1,2,3,4,5,6,7,8,9,10}),"TLS and DLL entry attach/detach ordering incorrect");}
 CloseHandle(bodyEvent);
 std::cout<<"PASS production thread adapters: real suspended/resumed thread, initialization serialization, all TLS seeds before callbacks, DLL/TLS attach and reverse detach, early ExitThread, native-family _beginthreadex/_endthreadex, no duplicate cleanup, DisableThreadLibraryCalls behavior. Hosted Windows; no Fortnite/Xbox execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
