// Original thread entry adapters. OS thread handles, flags and return values
// are real; guest notifications are not placeholder successes.
#include "GuestThreads.h"
#include "NativeSystem.h"
#include <roerrorapi.h>
#include <intrin.h>
#include <new>
#include <cstring>
#include <cstdint>
namespace Hollowwake { namespace {
struct Start {LPTHREAD_START_ROUTINE function;void* argument;};
struct CStart {void(__cdecl* function)(void*);void* argument;};
__declspec(noreturn) void Fatal(){RoFailFastWithErrorContext(E_FAIL);__fastfail(7);}
DWORD Run(Start value){if(!EnterGuestThread())Fatal();return value.function(value.argument);}
DWORD WINAPI ThreadEntry(void* raw){Start value=*static_cast<Start*>(raw);delete static_cast<Start*>(raw);__try{return Run(value);}__finally{LeaveGuestThread();}}
unsigned __stdcall CrtThreadEntry(void* raw){return ThreadEntry(raw);}
void RunC(CStart value){if(!EnterGuestThread())Fatal();value.function(value.argument);}
void __cdecl CThreadEntry(void* raw){CStart value=*static_cast<CStart*>(raw);delete static_cast<CStart*>(raw);__try{RunC(value);}__finally{LeaveGuestThread();}}
HANDLE WINAPI GuestCreateThread(LPSECURITY_ATTRIBUTES security,SIZE_T stack,LPTHREAD_START_ROUTINE function,LPVOID argument,DWORD flags,LPDWORD id){
 using Fn=HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
 auto real=reinterpret_cast<Fn>(ResolveNativeSystemImport("kernel32.dll","CreateThread"));if(!real){SetLastError(ERROR_PROC_NOT_FOUND);return nullptr;}
 if(!function){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}
 auto start=new(std::nothrow) Start{function,argument};if(!start){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return nullptr;}
 auto handle=real(security,stack,ThreadEntry,start,flags,id);if(!handle)delete start;return handle;
}
__declspec(noreturn) void WINAPI GuestExitThread(DWORD code){LeaveGuestThread();auto real=reinterpret_cast<void(WINAPI*)(DWORD)>(ResolveNativeSystemImport("kernel32.dll","ExitThread"));if(real)real(code);Fatal();}
void CrtError(int error){auto get=reinterpret_cast<int*(__cdecl*)()>(ResolveNativeSystemImport("msvcrt.dll","_errno"));if(get)*get()=error;}
uintptr_t __cdecl GuestBeginThreadEx(void* security,unsigned stack,unsigned(__stdcall* function)(void*),void* argument,unsigned flags,unsigned* id){
 using Fn=uintptr_t(__cdecl*)(void*,unsigned,unsigned(__stdcall*)(void*),void*,unsigned,unsigned*);
 auto real=reinterpret_cast<Fn>(ResolveNativeSystemImport("msvcrt.dll","_beginthreadex"));if(!real){CrtError(22);return 0;}if(!function){CrtError(22);return 0;}
 auto start=new(std::nothrow) Start{reinterpret_cast<LPTHREAD_START_ROUTINE>(function),argument};if(!start){CrtError(12);return 0;}
 auto handle=real(security,stack,CrtThreadEntry,start,flags,id);if(!handle)delete start;return handle;
}
uintptr_t __cdecl GuestBeginThread(void(__cdecl* function)(void*),unsigned stack,void* argument){
 using Fn=uintptr_t(__cdecl*)(void(__cdecl*)(void*),unsigned,void*);
 auto real=reinterpret_cast<Fn>(ResolveNativeSystemImport("msvcrt.dll","_beginthread"));if(!real||!function){CrtError(22);return static_cast<uintptr_t>(-1);}
 auto start=new(std::nothrow) CStart{function,argument};if(!start){CrtError(12);return static_cast<uintptr_t>(-1);}
 auto handle=real(CThreadEntry,stack,start);if(handle==static_cast<uintptr_t>(-1))delete start;return handle;
}
__declspec(noreturn) void __cdecl GuestEndThreadEx(unsigned code){LeaveGuestThread();auto real=reinterpret_cast<void(__cdecl*)(unsigned)>(ResolveNativeSystemImport("msvcrt.dll","_endthreadex"));if(real)real(code);Fatal();}
__declspec(noreturn) void __cdecl GuestEndThread(){LeaveGuestThread();auto real=reinterpret_cast<void(__cdecl*)()>(ResolveNativeSystemImport("msvcrt.dll","_endthread"));if(real)real();Fatal();}
}
void* ResolveGuestThreadImport(const char* dll,const char* name){
 if(!dll||!name)return nullptr;
 if(!_stricmp(dll,"kernel32.dll")||!_stricmp(dll,"kernelbase.dll")||!_strnicmp(dll,"api-ms-win-core-processthreads-",sizeof("api-ms-win-core-processthreads-")-1)||!_strnicmp(dll,"api-ms-win-core-libraryloader-",sizeof("api-ms-win-core-libraryloader-")-1)){
  if(!strcmp(name,"CreateThread"))return reinterpret_cast<void*>(&GuestCreateThread);
  if(!strcmp(name,"ExitThread"))return reinterpret_cast<void*>(&GuestExitThread);
  if(!strcmp(name,"DisableThreadLibraryCalls"))return reinterpret_cast<void*>(&GuestDisableThreadLibraryCalls);
 }
 if(!_stricmp(dll,"msvcrt.dll")&&NativeLegacyCrtAvailable()){
  if(!strcmp(name,"_beginthreadex"))return reinterpret_cast<void*>(&GuestBeginThreadEx);
  if(!strcmp(name,"_endthreadex"))return reinterpret_cast<void*>(&GuestEndThreadEx);
  if(!strcmp(name,"_beginthread"))return reinterpret_cast<void*>(&GuestBeginThread);
  if(!strcmp(name,"_endthread"))return reinterpret_cast<void*>(&GuestEndThread);
 }return nullptr;
}
}
