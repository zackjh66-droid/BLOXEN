#include "NativeSystem.h"
#include <cstddef>
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#include <winternl.h>
#endif
#include <intrin.h>
#include <roapi.h>
#include <cstring>
#include <mutex>
#if !defined(_M_X64)
#error This investigation targets the Windows x64 ABI only.
#endif
namespace Hollowwake { namespace {
// Stable x64 metadata prefixes, checked against public SDK types on desktop.
// UWP headers hide those type declarations; no desktop API import is added.
struct LoaderString {USHORT Length,MaximumLength;PWSTR Buffer;};
struct LoaderData {BYTE reserved[32];LIST_ENTRY InMemoryOrderModuleList;};
struct ProcessData {BYTE reserved[24];LoaderData* Ldr;};
struct ModuleData {BYTE reserved[16];LIST_ENTRY InMemoryOrderLinks;BYTE reserved2[16];void* DllBase;BYTE reserved3[16];LoaderString FullDllName;};
static_assert(offsetof(ProcessData,Ldr)==0x18&&offsetof(LoaderData,InMemoryOrderModuleList)==0x20&&offsetof(ModuleData,DllBase)==0x30&&offsetof(ModuleData,FullDllName)==0x48,"x64 loader layout");
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
static_assert(offsetof(PEB,Ldr)==offsetof(ProcessData,Ldr)&&offsetof(PEB_LDR_DATA,InMemoryOrderModuleList)==offsetof(LoaderData,InMemoryOrderModuleList),"SDK process layout differs");
static_assert(offsetof(LDR_DATA_TABLE_ENTRY,DllBase)==offsetof(ModuleData,DllBase)&&offsetof(LDR_DATA_TABLE_ENTRY,FullDllName)==offsetof(ModuleData,FullDllName),"SDK module layout differs");
#endif
// Read only our process's loader metadata using the public Windows SDK layout.
// No token changes, code patching, foreign process access or executable loading.
bool EqualBase(const LoaderString& path,const wchar_t* expected){
 if(!path.Buffer||path.Length%2||path.Length>32766)return false;
 unsigned first=0,length=path.Length/2;for(unsigned i=0;i<length;++i)if(path.Buffer[i]==L'\\'||path.Buffer[i]==L'/')first=i+1;
 unsigned j=0;for(unsigned i=first;i<length;++i,++j){wchar_t a=path.Buffer[i],b=expected[j];if(!b)return false;if(a>=L'A'&&a<=L'Z')a+=L'a'-L'A';if(a!=b)return false;}return expected[j]==0;
}
HMODULE Loaded(const wchar_t* name){
 auto peb=reinterpret_cast<ProcessData*>(__readgsqword(0x60));if(!peb||!peb->Ldr)return nullptr;
 auto head=&peb->Ldr->InMemoryOrderModuleList;unsigned count=0;
 for(auto item=head->Flink;item&&item!=head&&++count<4096;item=item->Flink){
  auto entry=CONTAINING_RECORD(item,ModuleData,InMemoryOrderLinks);
  if(EqualBase(entry->FullDllName,name))return static_cast<HMODULE>(entry->DllBase);
 }return nullptr;
}
struct NativeModules {
 HMODULE kernel=nullptr,base=nullptr,ntdll=nullptr,crt=nullptr,combase=nullptr;bool legacyComplete=false;
 NativeModules(){
  kernel=Loaded(L"kernel32.dll");base=Loaded(L"kernelbase.dll");ntdll=Loaded(L"ntdll.dll");combase=Loaded(L"combase.dll");
  // Pin through the actual documented OS operation. If unavailable or refused,
  // do not retain an unowned module reference or pretend that pinning worked.
  using Pin=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);
  auto pin=reinterpret_cast<Pin>(kernel?::GetProcAddress(kernel,"GetModuleHandleExW"):nullptr);
  if(!pin&&base)pin=reinterpret_cast<Pin>(::GetProcAddress(base,"GetModuleHandleExW"));
  if(pin){auto loaded=Loaded(L"msvcrt.dll");HMODULE held=nullptr;if(loaded&&pin(1,L"msvcrt.dll",&held)&&held==loaded)crt=held;}
  if(crt){
   // Use one legacy runtime for the whole allocation/state family, or none.
   // Never mix system-MSVCRT allocations with the old host-CRT bridge's free().
   const char* required[]={"malloc","calloc","realloc","free","_msize","_errno","_initterm","_initterm_e","__dllonexit","_onexit","_lock","_unlock","??2@YAPEAX_K@Z","??3@YAXPEAX@Z"};
   legacyComplete=true;for(auto name:required)if(!::GetProcAddress(crt,name))legacyComplete=false;
  }
 }
};
NativeModules& Modules(){static NativeModules value;return value;}
}
bool NativeLegacyCrtAvailable(){return Modules().legacyComplete;}
void* ResolveNativeSystemImport(const char* dll,const char* name){
 if(!dll||!name||!name[0])return nullptr;
 // A public WinRT SDK import establishes the OS provider dependency. It is
 // not an AppContainer bypass and performs no activation during lookup.
 if(!_strnicmp(dll,"api-ms-win-core-winrt-",sizeof("api-ms-win-core-winrt-")-1)&&!strcmp(name,"RoInitialize"))return reinterpret_cast<void*>(&::RoInitialize);
 auto& m=Modules();HMODULE selected=nullptr;
 if(!_stricmp(dll,"msvcrt.dll")){if(!m.legacyComplete)return nullptr;selected=m.crt;}
 else if(!_stricmp(dll,"ntdll.dll"))selected=m.ntdll;
 else if(!_stricmp(dll,"kernel32.dll"))selected=m.kernel;
 else if(!_stricmp(dll,"kernelbase.dll"))selected=m.base;
 else if(!_strnicmp(dll,"api-ms-win-core-winrt-",sizeof("api-ms-win-core-winrt-")-1))selected=m.combase;
 else if(!_strnicmp(dll,"api-ms-win-core-",16))selected=m.kernel?m.kernel:m.base;
 else return nullptr; // Network, graphics and guest module handles stay with explicit adapters.
 if(!selected)return nullptr;
 FARPROC result=nullptr;
 if(name[0]=='#'){
  unsigned ordinal=0;if(!name[1])return nullptr;
  for(auto p=name+1;*p;++p){if(*p<'0'||*p>'9'||ordinal>6553)return nullptr;ordinal=ordinal*10+(*p-'0');if(ordinal>65535)return nullptr;}
  if(!ordinal)return nullptr;result=::GetProcAddress(selected,reinterpret_cast<LPCSTR>(static_cast<ULONG_PTR>(ordinal)));
 }else result=::GetProcAddress(selected,name);
 return reinterpret_cast<void*>(result);
}
}
