#include <windows.h>
#include <iostream>
#include <stdexcept>
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int wmain(int count,wchar_t** arguments){try{
 Check(count==2,"Converted original fixture path required");
 auto image=LoadLibraryExW(arguments[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!image){std::cerr<<"Native LoadLibraryExW error="<<GetLastError()<<"\n";return 1;}
 auto start=reinterpret_cast<int(WINAPI*)()>(GetProcAddress(image,"StartHollowwakeClient"));auto original=reinterpret_cast<int(WINAPI*)()>(GetProcAddress(image,"OriginalEntry"));
 auto configure=reinterpret_cast<void(WINAPI*)(DWORD)>(GetProcAddress(image,"ConfigureOriginalFixture"));auto notifications=reinterpret_cast<DWORD(WINAPI*)()>(GetProcAddress(image,"FixtureNotificationCounts"));auto tick=reinterpret_cast<ULONGLONG(WINAPI*)()>(GetProcAddress(image,"ForwardedTick"));
 Check(start&&original&&configure&&notifications&&tick,"Converted native export lookup failed");Check(start==original,"New launch export does not point to original entry");Check(notifications()==10000,"Native loader did not invoke original process TLS callback once");
 DWORD input=4526925;configure(input);Check(start()==static_cast<int>(input^0x621),"Original entry did not execute real thread/TLS/unwind/module-identity task");std::cout<<"Native fixture notification counts="<<notifications()<<"\n";Check(notifications()==10101,"Native loader did not deliver exactly one thread attach/detach");Check(tick()>0,"Preserved OS-forwarded export did not execute");
 Check(FreeLibrary(image),"Converted native module unload failed");
 std::cout<<"PASS original EXE -> DLL -> actual OS load: original entry, preserved exports/forwarders, automatic relocation, TLS process/thread attach+detach, RtlLookupFunctionEntry and native module identity. Hosted Windows only; no Fortnite/Xbox execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
