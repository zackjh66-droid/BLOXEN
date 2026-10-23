#include "../Native/ClientLaunchIdentity.h"
#include "../Native/PackagedNamespace.h"
#include <windows.h>
#define PSAPI_VERSION 2
#include <psapi.h>
#include <vector>
#include <ctime>
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
static std::string missingExport;
static void CaptureMissing(const std::string& text){missingExport=text;SetLastError(621);}
template<class T>T api(const char* name){auto p=Hollowwake::ResolvePackagedNamespaceImport("kernel32.dll",name);assert(p);return reinterpret_cast<T>(p);}
int wmain(int argc,wchar_t** argv){
 assert(argc==2);wchar_t full[32768];assert(GetFullPathNameW(argv[1],32768,full,nullptr));
 std::wstring physical(full);auto slash=physical.find_last_of(L"\\/");assert(slash!=std::wstring::npos);auto root=physical.substr(0,slash),base=physical.substr(slash+1);
 HMODULE owned=LoadLibraryW(full);assert(owned);auto entry=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(owned,"OwnedIdentity"));assert(entry&&entry()==621);
 Hollowwake::ClientLaunchIdentity identity;std::wstring authTail;assert(identity.Configure(std::wstring(43,L'x'),300,1000));assert(identity.Consume(1001,authTail));
 auto guestCommand=std::wstring(L"\"OwnedOriginal.exe\" -d3d11 -nomatchmaking")+authTail;
 assert(Hollowwake::ConfigurePackagedNamespace(base.c_str(),guestCommand.c_str()));
 auto command=api<decltype(&GetCommandLineW)>("GetCommandLineW");assert(command()&&guestCommand==command());
 const char* tail=Hollowwake::PackagedWinMainTail();assert(tail&&strstr(tail,"-AUTH_TYPE=exchangecode")&&strstr(tail,"-AUTH_PASSWORD="));
 assert(Hollowwake::ConfigurePackagedRoot(root.c_str()));
 assert(!Hollowwake::RegisterPackagedMainIdentity(L"../outside.exe"));
 assert(Hollowwake::RegisterPackagedMainIdentity(L"6.21/FortniteGame/Binaries/Win64/OwnedOriginal.exe"));
 assert(!Hollowwake::RegisterPackagedMainIdentity(L"6.21/Other.exe"));
 auto module=api<decltype(&GetModuleHandleW)>("GetModuleHandleW");auto filename=api<decltype(&GetModuleFileNameW)>("GetModuleFileNameW");auto filenameA=api<decltype(&GetModuleFileNameA)>("GetModuleFileNameA");
 assert(module(nullptr)==owned&&module(L"OwnedOriginal.exe")==owned&&module(base.c_str())==owned);
 auto path=root+L"\\6.21\\FortniteGame\\Binaries\\Win64\\OwnedOriginal.exe";
 assert(module(path.c_str())==owned);assert(module(L"6.21/FortniteGame/Binaries/Win64/OwnedOriginal.exe")==owned);
 wchar_t result[32768];assert(filename(nullptr,result,32768)==path.size());assert(!_wcsicmp(result,path.c_str()));
 assert(filename(owned,result,32768)==path.size());
 wchar_t small[5]={};SetLastError(0);assert(filename(owned,small,5)==5&&small[4]==0&&GetLastError()==ERROR_INSUFFICIENT_BUFFER);
 SetLastError(0);assert(filename(nullptr,nullptr,0)==0&&GetLastError()==ERROR_INSUFFICIENT_BUFFER);
 SetLastError(0);assert(filename(reinterpret_cast<HMODULE>(1),result,32768)==0&&GetLastError()==ERROR_INVALID_HANDLE);
 char expected[32768],narrow[32768];assert(WideCharToMultiByte(CP_ACP,0,path.c_str(),-1,expected,32768,nullptr,nullptr));
 assert(filenameA(nullptr,narrow,32768)==strlen(expected)&&!strcmp(narrow,expected));char shortA[4];assert(filenameA(owned,shortA,4)==4&&shortA[3]==0);
 auto proc=api<decltype(&GetProcAddress)>("GetProcAddress");Hollowwake::ConfigurePackagedNamespaceLogger(CaptureMissing);
 SetLastError(0);assert(!proc(owned,"OwnedMissingExport")&&GetLastError()==ERROR_PROC_NOT_FOUND);assert(missingExport.find("!OwnedMissingExport Win32=127")!=std::string::npos);
 assert(proc(owned,"OwnedIdentity")==reinterpret_cast<FARPROC>(entry));
 // Concrete core-library handles identify actual MEM_IMAGE allocations and
 // support filename, image metadata, FROM_ADDRESS and real reference release.
 auto hold=api<decltype(&GetModuleHandleExW)>("GetModuleHandleExW");auto release=api<decltype(&FreeLibrary)>("FreeLibrary");auto load=api<decltype(&LoadLibraryExW)>("LoadLibraryExW");
 for(auto name:{L"kernel32.dll",L"kernelbase.dll",L"ntdll.dll"}){
  HMODULE real=::GetModuleHandleW(name);assert(real&&module(name)==real);MEMORY_BASIC_INFORMATION image={};assert(VirtualQuery(real,&image,sizeof(image))&&image.Type==MEM_IMAGE&&image.AllocationBase==real);
  assert(reinterpret_cast<IMAGE_DOS_HEADER*>(real)->e_magic==IMAGE_DOS_SIGNATURE);
  wchar_t actual[32768];assert(::GetModuleFileNameW(real,actual,32768)&&filename(real,result,32768)&&!_wcsicmp(actual,result));
  HMODULE byAddress=nullptr;assert(hold(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(real),&byAddress)&&byAddress==real);
  HMODULE held=load(name,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);assert(held==real&&release(held));
 }
 auto kernel=module(L"kernel32.dll");auto tick=reinterpret_cast<decltype(&GetTickCount64)>(proc(kernel,"GetTickCount64"));assert(tick);auto before=::GetTickCount64();auto observed=tick();auto after=::GetTickCount64();assert(observed>=before&&observed<=after);
 // Keep explicit loader/identity policy even with a real underlying HMODULE.
 assert(proc(kernel,"GetModuleHandleW")==reinterpret_cast<FARPROC>(module));
 SetLastError(0);assert(!proc(kernel,"GetFileInformationByHandleExW")&&GetLastError()==ERROR_PROC_NOT_FOUND);
 SetLastError(0);assert(!proc(kernel,"SetFileInformationByHandleW")&&GetLastError()==ERROR_PROC_NOT_FOUND);
 // Exact dynamic PSAPI call that crashed the actual client: first query
 // required module-array bytes, then enumerate genuine OS module handles.
 auto psapi=api<decltype(&LoadLibraryW)>("LoadLibraryW")(L"PSAPI.DLL");assert(psapi);
 auto enumerate=reinterpret_cast<decltype(&EnumProcessModules)>(proc(psapi,"EnumProcessModules"));assert(enumerate);
 DWORD needed=0;assert(enumerate(GetCurrentProcess(),nullptr,0,&needed)&&needed>=sizeof(HMODULE));
 std::vector<HMODULE> modules(needed/sizeof(HMODULE)+64);assert(enumerate(GetCurrentProcess(),modules.data(),static_cast<DWORD>(modules.size()*sizeof(HMODULE)),&needed));
 bool found=false;for(size_t i=0;i<modules.size()&&i<needed/sizeof(HMODULE);++i)if(modules[i]==owned)found=true;assert(found);
 auto info=reinterpret_cast<decltype(&GetModuleInformation)>(proc(psapi,"GetModuleInformation"));MODULEINFO actualInfo={};assert(info&&info(GetCurrentProcess(),owned,&actualInfo,sizeof(actualInfo))&&actualInfo.lpBaseOfDll==owned&&actualInfo.SizeOfImage>=4096);
 auto realName=reinterpret_cast<decltype(&GetModuleFileNameExW)>(proc(psapi,"GetModuleFileNameExW"));assert(realName&&realName(GetCurrentProcess(),owned,result,32768)&&!_wcsicmp(result,physical.c_str()));
 auto realBase=reinterpret_cast<decltype(&GetModuleBaseNameW)>(proc(psapi,"GetModuleBaseNameW"));assert(realBase&&realBase(GetCurrentProcess(),owned,result,32768)&&!_wcsicmp(result,base.c_str()));
 auto processes=reinterpret_cast<decltype(&EnumProcesses)>(proc(psapi,"EnumProcesses"));DWORD processIds[4096]={};assert(processes&&processes(processIds,sizeof(processIds),&needed));found=false;for(size_t i=0;i<4096&&i<needed/sizeof(DWORD);++i)if(processIds[i]==GetCurrentProcessId())found=true;assert(found);
 SetLastError(0);assert(!enumerate(reinterpret_cast<HANDLE>(1),modules.data(),static_cast<DWORD>(modules.size()*sizeof(HMODULE)),&needed)&&GetLastError()!=ERROR_SUCCESS);
 assert(!proc(psapi,"HWNoSuchProcessApi")&&GetLastError()==ERROR_PROC_NOT_FOUND);
 // Real unredirected API-set function newly required by APEX startup.
 HMODULE timeApi=::LoadLibraryExW(L"api-ms-win-crt-time-l1-1-0.dll",nullptr,0x800);assert(timeApi);
 auto formatTime=reinterpret_cast<char*(__cdecl*)(const std::tm*)>(::GetProcAddress(timeApi,"asctime"));assert(formatTime);std::tm stamp={};stamp.tm_year=70;stamp.tm_mday=1;stamp.tm_wday=4;
 auto formatted=formatTime(&stamp);assert(formatted&&!strcmp(formatted,"Thu Jan  1 00:00:00 1970\n"));FreeLibrary(timeApi);
 // A redirected physical delay-DLL name must identify a real registered
 // module, never a facade token or an implicit LocalState/desktop search.
 auto proxyPath=root+L"\\HWOwnedIdentity.dll";
 assert(!module(L"HWOwnedIdentity.dll"));
 assert(Hollowwake::RegisterPackagedDependency(L"HWOwnedIdentity.dll",L"HWOwnedIdentity.dll",L"HWOwnedIdentity.dll"));
 assert(!module(L"HWOwnedIdentity.dll"));
 HMODULE proxy=LoadLibraryW(proxyPath.c_str());assert(proxy&&proxy!=owned);
 assert(module(L"HWOwnedIdentity.dll")==proxy&&module(proxyPath.c_str())==proxy);
 assert(Hollowwake::RegisterPackagedDependency(L"OwnedPhysics.dll",L"HWOwnedIdentity.dll",L"6.21/Engine/OwnedPhysics.dll"));
 assert(module(L"../../../Engine/OwnedPhysics.dll")==proxy&&module(L"6.21/Engine/OwnedPhysics.dll")==proxy);
 assert(!module(L"../../../../outside/OwnedPhysics.dll"));
 auto ownedProxyEntry=reinterpret_cast<int(__cdecl*)()>(proc(proxy,"OwnedIdentity"));assert(ownedProxyEntry&&ownedProxyEntry()==621);
 assert(!Hollowwake::RegisterPackagedDependency(L"HWOutside.dll",L"../HWOutside.dll",L"HWOutside.dll"));
 FreeLibrary(proxy);
 // Guest aliasing must not change the host OS identity or the DLL's real bytes.
 assert(GetModuleFileNameW(owned,result,32768));assert(!_wcsicmp(result,physical.c_str()));assert(entry()==621);
 std::cout<<"PASS original logical EXE identity, real OS DLL ownership, W/A lengths and truncation, path aliases and invalid handles; desktop owned fixture only\n";
 FreeLibrary(owned);return 0;
}
