// Shared original native-OS-loader route. No manual mapper thread notifications:
// the OS owns TLS, DLL lifecycle and unwind registration for this route.
#include "NativeProxyDispatch.h"
#include "NativeSystem.h"
#include "PackagedNamespace.h"
#include <atomic>
#include <cstring>
namespace {
using Query=void*(*)(const char*,const char*);using Missing=void(*)(const char*,const char*);
std::atomic<Query> explicitRoute{nullptr};std::atomic<Missing> reportMissing{nullptr};
bool LoaderOperation(const char* name){const char* blocked[]={"LoadLibraryExW","LoadLibraryExA","LoadLibraryW","LoadLibraryA","GetModuleHandleW","GetModuleHandleA","GetModuleHandleExW","GetModuleHandleExA","GetProcAddress","FreeLibrary","GetModuleFileNameW","GetModuleFileNameA","GetCommandLineW","GetCommandLineA"};for(auto n:blocked)if(!strcmp(name,n))return true;return false;}
}
namespace Hollowwake {
void ConfigurePackagedProxyResolver(Query route,Missing missing){explicitRoute.store(route);reportMissing.store(missing);}
}
extern "C" __declspec(dllexport) void* __cdecl HWResolvePackagedImport(const char* dll,const char* name){
 if(!dll||!name)return nullptr;
 if(auto p=Hollowwake::ResolvePackagedNamespaceImport(dll,name))return p;
 if(auto route=explicitRoute.load())if(auto p=route(dll,name))return p;
 // Raw host loader/command-line functions cannot be handed to a converted EXE:
 // its identity and permitted module namespace require an explicit adapter.
 bool core=!_stricmp(dll,"kernel32.dll")||!_stricmp(dll,"kernelbase.dll")||!_strnicmp(dll,"api-ms-win-core-",16);
 if(!(core&&LoaderOperation(name)))if(auto p=Hollowwake::ResolveNativeSystemImport(dll,name))return p;
 if(auto log=reportMissing.load())log(dll,name);return nullptr;
}
