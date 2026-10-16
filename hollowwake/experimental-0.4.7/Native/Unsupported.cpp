// Missing imports NEVER return fabricated success. A called missing import logs
// its exact name then terminates the experiment through the public WinRT fail-fast API.
#include <windows.h>
#include <roerrorapi.h>
#include <intrin.h>
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include "Unsupported.h"
std::string DescribeGuestAddress(unsigned long long);
static void(*logLine)(const std::string&);static std::map<std::string,unsigned> indexes;static std::vector<std::string> names;static BYTE* code=nullptr;static bool sealed=false;
void ConfigureUnsupported(void(*log)(const std::string&)){logLine=log;}
static __declspec(noreturn) void WINAPI Called(ULONG_PTR id){if(logLine){logLine("CALLED_UNSUPPORTED "+(id<names.size()?names[id]:"invalid slot")+" caller="+DescribeGuestAddress((ULONGLONG)_ReturnAddress()));logLine("Guest called an unimplemented API. Fail-fast preserves this log; no success value is returned.");}RoFailFastWithErrorContext(E_NOTIMPL);__fastfail(7);}
void* UnsupportedImport(const char* dll,const char* symbol){std::string name=std::string(dll)+"!"+symbol;auto old=indexes.find(name);if(old!=indexes.end())return code+old->second*32;if(sealed||names.size()>=16000)return nullptr;if(!code)code=(BYTE*)VirtualAllocFromApp(nullptr,512*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!code)return nullptr;unsigned id=(unsigned)names.size();names.push_back(name);indexes[name]=id;BYTE* p=code+id*32;
 // x64 ABI: preserve the original call's stack/return address and shadow space.
 // Replace RCX with the slot id, then tail-jump to our non-returning native handler.
 p[0]=0x48;p[1]=0xb9;ULONGLONG value=id;memcpy(p+2,&value,8);p[10]=0x48;p[11]=0xb8;value=(ULONGLONG)&Called;memcpy(p+12,&value,8);p[20]=0xff;p[21]=0xe0;
 if(logLine&&id<256)logLine("UNIMPLEMENTED (fail-fast on call) "+name);return p;
}
bool SealUnsupported(){if(!code){sealed=true;return true;}DWORD old;if(!VirtualProtectFromApp(code,512*1024,PAGE_EXECUTE_READ,&old))return false;FlushInstructionCache(GetCurrentProcess(),code,512*1024);sealed=true;return true;}
unsigned UnsupportedCount(){return (unsigned)names.size();}
extern "C" __declspec(dllexport) void __cdecl HWUnsupportedContract(){void* p=UnsupportedImport("HOLLOWWAKE_ORIGINAL_FIXTURE","MissingFunction");if(!p||!SealUnsupported())RoFailFastWithErrorContext(E_FAIL);((void(*)())p)();}
