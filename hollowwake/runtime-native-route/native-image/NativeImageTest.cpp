#include <windows.h>
#include <iostream>
#include <stdexcept>
#ifdef HW_RESTORE_HEADER_BASE
#include "../../native-client-0.5.0/Native/ClientImageStartup.h"
#endif
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int wmain(int count,wchar_t** arguments){try{
 Check(count==2,"Converted original fixture path required");
 auto image=LoadLibraryExW(arguments[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!image){std::cerr<<"Native LoadLibraryExW error="<<GetLastError()<<"\n";return 1;}
#ifdef HW_RESTORE_HEADER_BASE
 auto base=reinterpret_cast<BYTE*>(image);auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
 auto field=static_cast<DWORD>(reinterpret_cast<BYTE*>(&nt->OptionalHeader.ImageBase)-base);
 auto preferred=nt->OptionalHeader.ImageBase;
 MEMORY_BASIC_INFORMATION headerPage={};Check(VirtualQuery(base,&headerPage,sizeof(headerPage))==sizeof(headerPage),"Owned header page unavailable");
 // Actual captured ordering regression: source headers have executable-copy
 // permissions. Metadata must be initialized BEFORE restoring that state.
 ClientImageProtection headerRule={0,4096,PAGE_EXECUTE_WRITECOPY};
 Check(SUCCEEDED(ApplyClientImageStartup(image,nt->OptionalHeader.SizeOfImage,&headerRule,1,&VirtualQuery,&VirtualProtect,nullptr)),"Original header protection replay failed");
 Check(RestoreClientImageHeaderBase(image,nt->OptionalHeader.SizeOfImage,field,preferred,&VirtualQuery,&VirtualProtect,nullptr)==E_ACCESSDENIED,"Unsafe executable-header write was not rejected");
 DWORD ignored=0;Check(VirtualProtect(base,4096,headerPage.Protect,&ignored)!=FALSE,"Owned header permissions restoration failed");
 auto result=InitializeClientImageState(image,nt->OptionalHeader.SizeOfImage,field,preferred,&headerRule,1,&VirtualQuery,&VirtualProtect,&VirtualProtect,nullptr);
 Check(SUCCEEDED(result)&&nt->OptionalHeader.ImageBase==reinterpret_cast<ULONGLONG>(image),"Measured runtime header initialization failed");
 MEMORY_BASIC_INFORMATION replayed={};Check(VirtualQuery(base,&replayed,sizeof(replayed))==sizeof(replayed)&&(replayed.Protect==PAGE_EXECUTE_WRITECOPY||replayed.Protect==PAGE_EXECUTE_READWRITE),"Measured header permissions not restored by production sequence");
 std::cout<<"PASS measured header state ordering: late executable-header write rejected; non-executable initialization before captured permissions succeeds.\n";
#endif
 auto start=reinterpret_cast<int(WINAPI*)()>(GetProcAddress(image,"StartHollowwakeClient"));auto original=reinterpret_cast<int(WINAPI*)()>(GetProcAddress(image,"OriginalEntry"));
 auto configure=reinterpret_cast<void(WINAPI*)(DWORD)>(GetProcAddress(image,"ConfigureOriginalFixture"));auto notifications=reinterpret_cast<DWORD(WINAPI*)()>(GetProcAddress(image,"FixtureNotificationCounts"));auto tick=reinterpret_cast<ULONGLONG(WINAPI*)()>(GetProcAddress(image,"ForwardedTick"));
 Check(start&&original&&configure&&notifications&&tick,"Converted native export lookup failed");Check(start==original,"New launch export does not point to original entry");Check(notifications()==10000,"Native loader did not invoke original process TLS callback once");
 DWORD input=4526925;configure(input);Check(start()==static_cast<int>(input^0x621),"Original entry did not execute real thread/TLS/unwind/module-identity task");std::cout<<"Native fixture notification counts="<<notifications()<<"\n";Check(notifications()==10101,"Native loader did not deliver exactly one thread attach/detach");Check(tick()>0,"Preserved OS-forwarded export did not execute");
 Check(FreeLibrary(image),"Converted native module unload failed");
 std::cout<<"PASS original EXE -> DLL -> actual OS load: original entry, preserved exports/forwarders, automatic relocation, TLS process/thread attach+detach, RtlLookupFunctionEntry and native module identity. Hosted Windows only; no Fortnite/Xbox execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
