// Original EXE fixture. Native OS must load/relocate/TLS-initialize it after
// conversion. No game code, SDK outside the public Windows SDK, or external input.
#include <windows.h>
#include <intrin.h>
extern "C" {
DWORD _tls_index=0;
#pragma section(".tls",long,read,write)
__declspec(allocate(".tls")) unsigned char fixtureTemplate[64]={0x62};
static volatile LONG processAttach=0,threadAttach=0,threadDetach=0;
static DWORD payload=0;
static void NTAPI Callback(void*,DWORD reason,void*){if(reason==DLL_PROCESS_ATTACH)InterlockedIncrement(&processAttach);if(reason==DLL_THREAD_ATTACH)InterlockedIncrement(&threadAttach);if(reason==DLL_THREAD_DETACH)InterlockedIncrement(&threadDetach);}
static PIMAGE_TLS_CALLBACK callbacks[]={Callback,nullptr};
#pragma section(".rdata$T",read)
__declspec(allocate(".rdata$T")) extern const IMAGE_TLS_DIRECTORY64 _tls_used={
 (ULONGLONG)fixtureTemplate,(ULONGLONG)(fixtureTemplate+64),(ULONGLONG)&_tls_index,(ULONGLONG)callbacks,0,0};
__declspec(dllexport) void WINAPI ConfigureOriginalFixture(DWORD value){payload=value;}
__declspec(dllexport) DWORD WINAPI FixtureNotificationCounts(){return processAttach*10000+threadAttach*100+threadDetach;}
static DWORD WINAPI Work(void* value){auto tls=static_cast<unsigned char**>((void*)__readgsqword(0x58))[_tls_index];return tls&&tls[0]==0x62?(*static_cast<DWORD*>(value)^0x621):0xffffffff;}
__declspec(dllexport) int WINAPI OriginalEntry(){
 auto tls=static_cast<unsigned char**>((void*)__readgsqword(0x58))[_tls_index];if(!tls||tls[0]!=0x62||processAttach!=1)return -1;
 DWORD64 image=0;if(!RtlLookupFunctionEntry((DWORD64)&OriginalEntry,&image,nullptr)||!image)return -2;
 HMODULE actual=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)&OriginalEntry,&actual)||(DWORD64)actual!=image)return -3;
 DWORD input=payload;auto thread=CreateThread(nullptr,0,Work,&input,0,nullptr);if(!thread)return -4;if(WaitForSingleObject(thread,10000)!=WAIT_OBJECT_0)return -5;DWORD result=0;if(!GetExitCodeThread(thread,&result))return -6;CloseHandle(thread);return (int)result;
}
}
