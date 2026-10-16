#include <windows.h>
#include <iostream>
#include <stdexcept>
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int wmain(int count,wchar_t** arguments){try{
 Check(count==2,"Proxy directory required");std::wstring root=arguments[1];
 auto host=LoadLibraryExW((root+L"\\HollowwakeClientRuntime.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);Check(host,"Owned proxy-host load failed");
 auto countMissing=reinterpret_cast<LONG(WINAPI*)()>(GetProcAddress(host,"HWFixtureMissing"));Check(countMissing,"Missing-report fixture export absent");
 auto game=LoadLibraryExW((root+L"\\ProxyConvertedFixture.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!game){std::cerr<<"Redirected own EXE load error="<<GetLastError()<<"\n";return 1;}
 auto set=reinterpret_cast<void(WINAPI*)(DWORD)>(GetProcAddress(game,"ConfigureOriginalFixture"));auto start=reinterpret_cast<int(WINAPI*)()>(GetProcAddress(game,"StartHollowwakeClient"));auto notify=reinterpret_cast<DWORD(WINAPI*)()>(GetProcAddress(game,"FixtureNotificationCounts"));Check(set&&start&&notify,"Own EXE exports missing after redirect");set(4526925);Check(start()==static_cast<int>(4526925^0x621),"Redirected Win32 thread task did not execute");Check(notify()==10101,"Native TLS lifecycle through redirected imports failed");
 auto mix=LoadLibraryExW((root+L"\\HWMIX.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);Check(mix,"Owned ABI proxy failed initialization");
 using Fn=double(WINAPI*)(double,ULONGLONG,double,ULONGLONG,ULONGLONG,ULONGLONG,ULONGLONG,ULONGLONG);
 auto named=reinterpret_cast<Fn>(GetProcAddress(mix,"MixDouble"));auto ordinal=reinterpret_cast<Fn>(GetProcAddress(mix,reinterpret_cast<LPCSTR>(17)));Check(named&&ordinal,"Named/ordinal thunk export lookup failed");
 Check(named(1.25,2,3.5,4,5,6,7,8)==1.25+4+10.5+16+25+36+49+64,"Proxy corrupted mixed integer/XMM/stack arguments");Check(ordinal(2.25,3,4.5,5,6,7,8,9)==2.25+6+13.5+20+30+42+56+72,"Ordinal proxy corrupted ABI arguments");
 auto before=countMissing();SetLastError(0);auto bad=LoadLibraryExW((root+L"\\HWBAD.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);auto error=GetLastError();Check(!bad&&error==ERROR_DLL_INIT_FAILED&&countMissing()>before,"Unresolved proxy did not reject DLL initialization and report missing API");
 Check(FreeLibrary(mix)&&FreeLibrary(game)&&FreeLibrary(host),"Native proxy module release failed");
 std::cout<<"PASS native import redirection: OS loads converted ORIGINAL EXE through generated DLL; real OS thread work and TLS callbacks; mixed XMM/integer/stack ABI and ordinal exports; missing API rejects DLL initialization. No Fortnite/Xbox execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
