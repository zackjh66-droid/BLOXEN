#include "NativeClientThread.h"
#include <intrin.h>
HRESULT RunNativeClientThread(NativeClientEntry entry,SIZE_T reserve,int* result,NativeClientLogger log){
 if(!entry||!result)return E_POINTER;
 SYSTEM_INFO info={};GetNativeSystemInfo(&info);
 if(!reserve||reserve>256*1024*1024||!info.dwAllocationGranularity||reserve%info.dwAllocationGranularity)return E_INVALIDARG;
 // The exact original EXE's OS entry was measured with RAX/RDX=entry and
 // RCX/R8=actual PEB. Pass the entry directly to the OS thread initializer;
 // a C++ no-argument wrapper changes this context. Do not change the PEB.
 DWORD id=0;auto peb=reinterpret_cast<void*>(__readgsqword(0x60));
 HANDLE thread=CreateThread(nullptr,reserve,reinterpret_cast<LPTHREAD_START_ROUTINE>(entry),peb,STACK_SIZE_PARAM_IS_A_RESERVATION,&id);
 if(!thread)return HRESULT_FROM_WIN32(GetLastError());
 if(log)log("NATIVE_THREAD original entry is OS start address id="+std::to_string(id)+" stackReserve="+std::to_string(reserve)+"; no host COM initialization; lobby not confirmed");
 auto wait=WaitForSingleObjectEx(thread,INFINITE,FALSE);DWORD error=wait==WAIT_FAILED?GetLastError():ERROR_SUCCESS,exitCode=0;
 if(wait==WAIT_OBJECT_0&&!GetExitCodeThread(thread,&exitCode))error=GetLastError();
 CloseHandle(thread);
 if(wait!=WAIT_OBJECT_0||error)return HRESULT_FROM_WIN32(error?error:ERROR_GEN_FAILURE);
 // The OS reports thread completion, which can be a return or ExitThread.
 // Neither outcome establishes engine startup, rendering or successful login.
 *result=static_cast<int>(exitCode);if(log)log("NATIVE_THREAD_EXIT code="+std::to_string(exitCode)+"; return versus ExitThread not inferred");return S_OK;
}
