// Exact public Windows SDK APIs. No fabricated processor or memory values.
#pragma once
#include <windows.h>
#include <cstring>
#include <roerrorapi.h>
#include <intrin.h>
// GetVersion is legacy and manifest-dependent; preserve the actual OS API behavior.
#pragma warning(push)
#pragma warning(disable:4996)
namespace Hollowwake {
using SystemInfoFailure=void(*)(DWORD);
inline SystemInfoFailure& SystemInfoFailureHandler(){static SystemInfoFailure handler=nullptr;return handler;}
inline DWORD WINAPI GuestGetVersion(){
 OSVERSIONINFOW version{};version.dwOSVersionInfoSize=sizeof(version);
 if(!::GetVersionExW(&version)){
  DWORD error=GetLastError();if(SystemInfoFailureHandler())SystemInfoFailureHandler()(error);
  RoFailFastWithErrorContext(HRESULT_FROM_WIN32(error?error:ERROR_GEN_FAILURE));__fastfail(7);
 }
 DWORD packed=(version.dwMajorVersion&255)|((version.dwMinorVersion&255)<<8);
 if(version.dwPlatformId==VER_PLATFORM_WIN32_NT)packed|=(version.dwBuildNumber&0x7fff)<<16;
 else packed|=0x80000000UL;
 return packed;
}

inline void* ResolveSystemInfoImport(const char* dll,const char* name){
 if(!dll||!name)return nullptr;
 if(_stricmp(dll,"KERNEL32.dll")&&_stricmp(dll,"KERNELBASE.dll")&&_strnicmp(dll,"api-ms-win-core-",16))return nullptr;
 if(!strcmp(name,"GetVersion"))return reinterpret_cast<void*>(&GuestGetVersion);
 if(!strcmp(name,"GetSystemInfo"))return reinterpret_cast<void*>(&::GetSystemInfo);
 if(!strcmp(name,"GetNativeSystemInfo"))return reinterpret_cast<void*>(&::GetNativeSystemInfo);
 return nullptr;
}
inline bool SystemInfoContract(){
 using Query=void(WINAPI*)(LPSYSTEM_INFO);
 auto query=reinterpret_cast<Query>(ResolveSystemInfoImport("KERNEL32.dll","GetSystemInfo"));
 auto native=reinterpret_cast<Query>(ResolveSystemInfoImport("KERNEL32.dll","GetNativeSystemInfo"));
 struct Guard {ULONGLONG before;SYSTEM_INFO info;ULONGLONG after;};
 Guard a{},b{};a.before=b.before=0x12345678aabbccddULL;a.after=b.after=0xfedcba9876543210ULL;
 query(&a.info);native(&b.info);
 return a.before==b.before&&a.before==0x12345678aabbccddULL&&a.after==b.after&&a.after==0xfedcba9876543210ULL
 &&a.info.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64&&a.info.wProcessorArchitecture==b.info.wProcessorArchitecture
 &&a.info.dwPageSize==b.info.dwPageSize&&a.info.dwPageSize>=4096&&(a.info.dwPageSize&(a.info.dwPageSize-1))==0
 &&a.info.dwAllocationGranularity==b.info.dwAllocationGranularity&&a.info.dwAllocationGranularity>=a.info.dwPageSize
 &&a.info.lpMinimumApplicationAddress==b.info.lpMinimumApplicationAddress&&a.info.lpMaximumApplicationAddress==b.info.lpMaximumApplicationAddress
 &&reinterpret_cast<ULONG_PTR>(a.info.lpMaximumApplicationAddress)>reinterpret_cast<ULONG_PTR>(a.info.lpMinimumApplicationAddress)
 &&a.info.dwNumberOfProcessors==b.info.dwNumberOfProcessors&&a.info.dwNumberOfProcessors>0
 &&a.info.dwActiveProcessorMask==b.info.dwActiveProcessorMask&&a.info.dwActiveProcessorMask!=0;
}
}

#pragma warning(pop)
