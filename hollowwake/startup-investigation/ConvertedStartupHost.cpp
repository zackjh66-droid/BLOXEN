// Test host only: Windows desktop APIs remain original, not Hollowwake proxies.
// This isolates EXE-to-DLL startup semantics from the compatibility adapters.
#define STARTUP_PROBE_LIBRARY
#include "NativeStartupProbe.cpp"
#include "BaselinePin.generated.h"
#include <intrin.h>
#ifdef USE_NATIVE_ENTRY_CONTEXT
extern "C" DWORD RunPreservedEntry(void*);
#endif
void Stage(unsigned phase){char text[64]={};sprintf_s(text,"HW_STARTUP_HOST stage=%u",phase);OutputDebugStringA(text);}
#ifdef USE_PRODUCTION_IDENTITY
#include "../native-client-0.5.0/Native/PackagedNamespace.h"
#include "../native-client-0.5.0/Native/StartupImport.h"
namespace {
HMODULE client=nullptr;
bool Self(LPCWSTR name){return !name||!_wcsicmp(name,L"FortniteClient-Win64-Shipping.exe")||!_wcsicmp(name,L"Fortnite621.baseline.private.dll");}
HMODULE WINAPI IdentityModuleW(LPCWSTR name){if(!Self(name))return GetModuleHandleW(name);return reinterpret_cast<decltype(&GetModuleHandleW)>(Hollowwake::ResolvePackagedNamespaceImport("kernel32.dll","GetModuleHandleW"))(name);}
HMODULE WINAPI IdentityModuleA(LPCSTR name){if(name&&_stricmp(name,"FortniteClient-Win64-Shipping.exe")&&_stricmp(name,"Fortnite621.baseline.private.dll"))return GetModuleHandleA(name);return reinterpret_cast<decltype(&GetModuleHandleA)>(Hollowwake::ResolvePackagedNamespaceImport("kernel32.dll","GetModuleHandleA"))(name);}
DWORD WINAPI IdentityFilenameW(HMODULE module,LPWSTR path,DWORD size){if(module&&module!=client)return GetModuleFileNameW(module,path,size);return reinterpret_cast<decltype(&GetModuleFileNameW)>(Hollowwake::ResolvePackagedNamespaceImport("kernel32.dll","GetModuleFileNameW"))(module,path,size);}
DWORD WINAPI IdentityFilenameA(HMODULE module,LPSTR path,DWORD size){if(module&&module!=client)return GetModuleFileNameA(module,path,size);return reinterpret_cast<decltype(&GetModuleFileNameA)>(Hollowwake::ResolvePackagedNamespaceImport("kernel32.dll","GetModuleFileNameA"))(module,path,size);}
bool BindIdentity(HMODULE module){
 auto base=reinterpret_cast<BYTE*>(module);auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
 auto imports=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];unsigned changed=0;
 for(auto d=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+imports.VirtualAddress);d->Name;++d){
  if(!d->OriginalFirstThunk||!d->FirstThunk)return false;
  auto names=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+d->OriginalFirstThunk),slots=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+d->FirstThunk);
  for(unsigned i=0;names[i].u1.AddressOfData;++i){if(IMAGE_SNAP_BY_ORDINAL64(names[i].u1.AddressOfData))continue;auto name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names[i].u1.AddressOfData)->Name;void* replacement=nullptr;
   if(!strcmp(name,"GetModuleHandleW"))replacement=reinterpret_cast<void*>(&IdentityModuleW);
   else if(!strcmp(name,"GetModuleHandleA"))replacement=reinterpret_cast<void*>(&IdentityModuleA);
   else if(!strcmp(name,"GetModuleFileNameW"))replacement=reinterpret_cast<void*>(&IdentityFilenameW);
   else if(!strcmp(name,"GetModuleFileNameA"))replacement=reinterpret_cast<void*>(&IdentityFilenameA);
   else if(!strcmp(name,"GetCommandLineW")||!strcmp(name,"GetCommandLineA"))replacement=Hollowwake::ResolvePackagedNamespaceImport("kernel32.dll",reinterpret_cast<const char*>(name));
   if(replacement){DWORD old=0,unused=0;if(!VirtualProtect(&slots[i],sizeof(slots[i]),PAGE_READWRITE,&old))return false;slots[i].u1.Function=reinterpret_cast<ULONGLONG>(replacement);if(!VirtualProtect(&slots[i],sizeof(slots[i]),old,&unused))return false;++changed;}
  }
 }
 return changed>0&&BindWinMainCommandImport(module,"api-ms-win-crt-runtime-l1-1-0.dll",reinterpret_cast<void*>(&Hollowwake::PackagedWinMainTail));
}
}
#endif
DWORD WINAPI Enter(void* address){
 Stage(6);
#ifdef USE_NATIVE_ENTRY_CONTEXT
 return RunPreservedEntry(address);
#else
 return reinterpret_cast<DWORD(__cdecl*)()>(address)();
#endif
}
int wmain(){
 Stage(1);
 wchar_t module[32768]={};DWORD n=GetModuleFileNameW(nullptr,module,32768);if(!n||n>=32768)return 2;
 std::wstring directory(module,n);directory=directory.substr(0,directory.find_last_of(L"/\\"));
 auto image=directory+L"\\Fortnite621.baseline.private.dll";std::string hash;
 if(!Hash(image.c_str(),hash)||hash!=kBaselinePin)return 3;
 Stage(2);
 auto loaded=LoadLibraryExW(image.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);if(!loaded)return static_cast<int>(GetLastError());
 Stage(3);
#ifdef USE_PRODUCTION_IDENTITY
 client=loaded;
 if(!Hollowwake::ConfigurePackagedNamespace(L"Fortnite621.baseline.private.dll",L"FortniteClient-Win64-Shipping.exe -windowed -d3d11 -ResX=640 -ResY=360")||!Hollowwake::ConfigurePackagedRoot(directory.c_str())||!Hollowwake::RegisterPackagedMainIdentity(L"FortniteClient-Win64-Shipping.exe"))return 6;
 Stage(4);if(!BindIdentity(loaded))return 6;Stage(5);
#endif
#ifdef NO_ENTRY_CASE
 auto entry=reinterpret_cast<FARPROC>(reinterpret_cast<BYTE*>(loaded)+0x369e098);
#else
 auto entry=GetProcAddress(loaded,"StartHollowwakeClient");if(!entry)return ERROR_PROC_NOT_FOUND;
#endif
 Handle thread;thread.value=CreateThread(nullptr,1048576,Enter,reinterpret_cast<void*>(entry),STACK_SIZE_PARAM_IS_A_RESERVATION,nullptr);
 if(!thread.value)return static_cast<int>(GetLastError());WaitForSingleObject(thread.value,INFINITE);DWORD result=0;GetExitCodeThread(thread.value,&result);return static_cast<int>(result);
}
