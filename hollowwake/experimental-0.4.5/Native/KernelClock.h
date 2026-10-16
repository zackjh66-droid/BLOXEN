// Original ABI bridge for the observed GetTickCount startup call.
#pragma once
#include <windows.h>
#include <cstring>
namespace Hollowwake {
inline DWORD TickCountLow32(ULONGLONG uptime){return static_cast<DWORD>(uptime);}
inline DWORD WINAPI GuestGetTickCount(){return TickCountLow32(::GetTickCount64());}
inline void* ResolveClockImport(const char* dll,const char* name){
 if(!dll||!name)return nullptr;
 if(_stricmp(dll,"KERNEL32.dll")&&_stricmp(dll,"KERNELBASE.dll")&&_strnicmp(dll,"api-ms-win-core-",16))return nullptr;
 if(!strcmp(name,"GetTickCount"))return reinterpret_cast<void*>(&GuestGetTickCount);
 if(!strcmp(name,"GetTickCount64"))return reinterpret_cast<void*>(&GetTickCount64);
 return nullptr;
}
}
