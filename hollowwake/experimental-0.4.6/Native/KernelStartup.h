#pragma once
#include <windows.h>
#include <roerrorapi.h>
#include <intrin.h>
#include <thread>
#include <chrono>
#include <cstring>
// Keep this symbol's compiler x64 stack-probe ABI. Never call as a C function.
extern "C" void __chkstk();
#pragma warning(push)
#pragma warning(disable:4996)
namespace Hollowwake {
__declspec(noreturn) inline void KernelStartupFatal(DWORD error){RoFailFastWithErrorContext(HRESULT_FROM_WIN32(error?error:ERROR_GEN_FAILURE));__fastfail(7);}
inline void WINAPI LegacyInitializeCriticalSection(LPCRITICAL_SECTION value){if(!::InitializeCriticalSectionEx(value,0,0))KernelStartupFatal(GetLastError());}
inline BOOL WINAPI LegacyInitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION value,DWORD spin){return ::InitializeCriticalSectionEx(value,spin,0);}
inline void WINAPI LegacySleep(DWORD milliseconds){
 if(!milliseconds){std::this_thread::yield();return;}
 auto event=CreateEventExW(nullptr,nullptr,0,SYNCHRONIZE);if(!event)KernelStartupFatal(GetLastError());
 DWORD result=WaitForSingleObjectEx(event,milliseconds,FALSE),error=GetLastError();CloseHandle(event);
 if(result!=WAIT_TIMEOUT)KernelStartupFatal(error);
}
inline void* ResolveKernelStartupImport(const char* dll,const char* name){
 if(!dll||!name)return nullptr;
 if(_stricmp(dll,"KERNEL32.dll")&&_stricmp(dll,"KERNELBASE.dll")&&_strnicmp(dll,"api-ms-win-core-",16))return nullptr;
 #define START(symbol,target) if(!strcmp(name,symbol))return reinterpret_cast<void*>(&target)
 START("InitializeCriticalSection",LegacyInitializeCriticalSection);START("InitializeCriticalSectionAndSpinCount",LegacyInitializeCriticalSectionAndSpinCount);
 START("Sleep",LegacySleep);START("__chkstk",::__chkstk);
 START("GetVersionExA",::GetVersionExA);START("GetVersionExW",::GetVersionExW);
 START("HeapCreate",::HeapCreate);START("HeapDestroy",::HeapDestroy);
 START("WideCharToMultiByte",::WideCharToMultiByte);START("MultiByteToWideChar",::MultiByteToWideChar);
 START("WriteFile",::WriteFile);START("CloseHandle",::CloseHandle);
 START("OutputDebugStringA",::OutputDebugStringA);START("OutputDebugStringW",::OutputDebugStringW);
 START("TlsAlloc",::TlsAlloc);START("TlsFree",::TlsFree);START("TlsSetValue",::TlsSetValue);START("TlsGetValue",::TlsGetValue);
 START("HeapAlloc",::HeapAlloc);START("HeapReAlloc",::HeapReAlloc);START("HeapFree",::HeapFree);START("HeapSize",::HeapSize);
 START("EnterCriticalSection",::EnterCriticalSection);START("LeaveCriticalSection",::LeaveCriticalSection);START("DeleteCriticalSection",::DeleteCriticalSection);
 START("RaiseException",::RaiseException);START("GetTickCount64",::GetTickCount64);
 #undef START
 return nullptr;
}
}

#pragma warning(pop)
