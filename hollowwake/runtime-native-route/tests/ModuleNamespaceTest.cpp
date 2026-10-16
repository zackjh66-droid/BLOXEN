// Exercises the production guest module namespace and real OS operations.
// Dependency-archive IO is deliberately absent; only own code and OS APIs run.
#include "../NativeSystem.h"
#include "../../experimental-0.4.7/Native/ModuleLoader.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <winstring.h>
static unsigned dependencyQueries=0;
bool FindGuestDependency(const std::string&,GuestModuleRecord&){++dependencyQueries;return false;}
bool FindGuestDependencyAddress(ULONG_PTR,GuestModuleRecord&){return false;}
void* QueryDependencyExport(const char*,const char*){return nullptr;}
static void Check(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
static void* Query(const char* dll,const char* name){
 if(auto p=ResolveModuleLoaderImport(dll,name))return p;
 return Hollowwake::ResolveNativeSystemImport(dll,name);
}
int main(){try{
 Check(LoadLibraryExW(L"msvcrt.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)!=nullptr,"System-only fixture CRT unavailable");
 ConfigureGuestModules(Query,nullptr,Hollowwake::NativeSystemFacadeAvailable);
 auto load=reinterpret_cast<HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD)>(ResolveModuleLoaderImport("kernel32.dll","LoadLibraryExW"));
 auto proc=reinterpret_cast<FARPROC(WINAPI*)(HMODULE,LPCSTR)>(ResolveModuleLoaderImport("kernel32.dll","GetProcAddress"));
 auto release=reinterpret_cast<BOOL(WINAPI*)(HMODULE)>(ResolveModuleLoaderImport("kernel32.dll","FreeLibrary"));
 auto get=reinterpret_cast<HMODULE(WINAPI*)(LPCWSTR)>(ResolveModuleLoaderImport("kernel32.dll","GetModuleHandleW"));
 Check(load&&proc&&release&&get,"Production namespace APIs absent");
 auto loader=load(L"api-ms-win-core-libraryloader-l1-1-0",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Check(loader,"Libraryloader contract could not be acquired");
 Check(proc(loader,"LoadLibraryExW")==reinterpret_cast<FARPROC>(load),"Dynamic lookup bypassed production guest loading restrictions");
 auto kernel=load(L"kernelbase",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Check(kernel,"Kernelbase facade unavailable");
 auto startup=reinterpret_cast<void(WINAPI*)(LPSTARTUPINFOW)>(proc(kernel,"GetStartupInfoW"));Check(startup,"Dynamic startup-info export absent");STARTUPINFOW actual{},expected{};startup(&actual);GetStartupInfoW(&expected);Check(!memcmp(&actual,&expected,sizeof(actual)),"Dynamic startup info did not match OS result");
 auto core=load(L"api-ms-win-core-processthreads-l1-1-0",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Check(core,"Process/thread contract absent");
 auto create=reinterpret_cast<HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD)>(proc(core,"CreateThread"));Check(create,"Dynamic actual thread API missing");DWORD payload=4526925;auto thread=create(nullptr,0,[](LPVOID p)->DWORD{return *static_cast<DWORD*>(p)^0x621;},&payload,0,nullptr);Check(thread,"Dynamic thread creation failed");Check(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"Dynamic thread wait failed");DWORD code=0;Check(GetExitCodeThread(thread,&code)&&code==(payload^0x621),"Dynamic thread payload did not run");CloseHandle(thread);
 auto winrt=load(L"api-ms-win-core-winrt-string-l1-1-0.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Check(winrt,"WinRT string contract facade absent");
 auto make=reinterpret_cast<HRESULT(WINAPI*)(PCNZWCH,UINT32,HSTRING*)>(proc(winrt,"WindowsCreateString"));auto destroy=reinterpret_cast<HRESULT(WINAPI*)(HSTRING)>(proc(winrt,"WindowsDeleteString"));Check(make&&destroy,"Dynamic WinRT string functions absent");HSTRING text=nullptr;Check(SUCCEEDED(make(L"621",3,&text))&&text,"Dynamic HSTRING allocation failed");Check(SUCCEEDED(destroy(text)),"Dynamic HSTRING destruction failed");
 auto crt=load(L"msvcrt",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Check(crt&&Hollowwake::NativeLegacyCrtAvailable(),"Legacy family namespace absent");auto native=Hollowwake::ResolveNativeSystemImport("msvcrt.dll","malloc");Check(reinterpret_cast<void*>(proc(crt,"malloc"))==native,"Dynamic and static legacy allocator families differ");
 auto allocate=reinterpret_cast<void*(__cdecl*)(size_t)>(proc(crt,"malloc"));auto free=reinterpret_cast<void(__cdecl*)(void*)>(proc(crt,"free"));Check(allocate&&free,"Dynamic legacy allocation exports absent");auto memory=allocate(64);Check(memory,"Dynamic legacy allocation failed");memset(memory,0x62,64);free(memory);
 Check(get(L"api-ms-win-core-winrt-string-l1-1-0")==winrt,"Facade lookup did not preserve identity");
 SetLastError(0);Check(!proc(loader,"HollowwakeMissingExport")&&GetLastError()==ERROR_PROC_NOT_FOUND,"Missing optional export fabricated success");
 Check(!load(L"api-ms-win-core-invented-l9-9-9",nullptr,0),"Invented API contract accepted");Check(!load(L"C:\\untrusted\\kernel32.dll",nullptr,0),"Archive/native path crossed namespace boundary");Check(!load(L"WS2_32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32),"Network provider unexpectedly enabled");
 Check(!proc(GetModuleHandleW(L"kernel32.dll"),"GetTickCount64"),"Raw host module handle accepted in guest namespace");
 for(auto module:{crt,winrt,core,kernel,loader})Check(release(module),"Facade reference release failed");
 Check(!release(crt)&&GetLastError()==ERROR_INVALID_HANDLE,"Facade reference underflow accepted");
 std::cout<<"PASS production guest dynamic namespace -> actual startup info, thread task, WinRT string and coherent legacy allocator; nested loader lookup stays virtual; invalid paths, unknown contracts and host handles rejected. Windows only, not Fortnite/Xbox.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
