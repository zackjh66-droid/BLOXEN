#include "ReadOnlyPlatform.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <cstring>
namespace {
struct Provider {
 HMODULE module=nullptr;
 Provider(){
  using Hold=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);
  auto hold=reinterpret_cast<Hold>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));
  if(hold&&hold(0,L"winhttp.dll",&module))return;
  auto load=reinterpret_cast<Load>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","LoadLibraryExW"));
  if(load)module=load(L"winhttp.dll",nullptr,0x800); // LOAD_LIBRARY_SEARCH_SYSTEM32; no downloaded providers.
 }
 ~Provider(){if(module)FreeLibrary(module);}
};
}
void* ResolveReadOnlyPlatformImport(const char* dll,const char* name){
 if(!dll||!name||_stricmp(dll,"winhttp.dll"))return nullptr;
 if(strcmp(name,"WinHttpGetDefaultProxyConfiguration")&&strcmp(name,"WinHttpGetIEProxyConfigForCurrentUser"))return nullptr;
 static Provider provider;if(!provider.module)return nullptr;
 // Execute the actual OS query, retaining its exact ABI, allocations and errors.
 // These queries open no request/session and cannot send project credentials.
 return reinterpret_cast<void*>(GetProcAddress(provider.module,name));
}
