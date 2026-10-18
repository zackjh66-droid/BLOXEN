#include "NativeClientThread.h"
#include <memory>
namespace {
struct Invocation {NativeClientEntry entry;NativeClientLogger log;int result=0;bool returned=false;};
DWORD WINAPI Invoke(void* value){
 auto holder=static_cast<std::shared_ptr<Invocation>*>(value);
 auto call=*holder;delete holder;
 if(call->log)call->log("NATIVE_ENTRY thread="+std::to_string(GetCurrentThreadId())+" calling original entry; COM apartment left to client; lobby not confirmed");
 // Do not swallow guest exceptions or manufacture a successful return. The
 // existing vectored recorder and OS crash handling retain the actual failure.
 call->result=call->entry();call->returned=true;
 if(call->log)call->log("NATIVE_ENTRY_RETURN value="+std::to_string(call->result));
 return 0;
}
}
HRESULT RunNativeClientThread(NativeClientEntry entry,SIZE_T reserve,int* result,NativeClientLogger log){
 if(!entry||!result)return E_POINTER;
 // Windows stack reservations are allocation-granularity aligned. Exact build
 // metadata is checked by the caller; do not silently use a CLR pool default.
 SYSTEM_INFO info={};GetNativeSystemInfo(&info);
 if(!reserve||reserve>256*1024*1024||!info.dwAllocationGranularity||reserve%info.dwAllocationGranularity)return E_INVALIDARG;
 auto call=std::make_shared<Invocation>();call->entry=entry;call->log=log;
 auto argument=new std::shared_ptr<Invocation>(call);DWORD id=0;
 HANDLE thread=CreateThread(nullptr,reserve,Invoke,argument,STACK_SIZE_PARAM_IS_A_RESERVATION,&id);
 if(!thread){auto error=GetLastError();delete argument;return HRESULT_FROM_WIN32(error);}
 if(log)log("NATIVE_THREAD created id="+std::to_string(id)+" stackReserve="+std::to_string(reserve));
 auto wait=WaitForSingleObjectEx(thread,INFINITE,FALSE);auto error=wait==WAIT_FAILED?GetLastError():ERROR_SUCCESS;CloseHandle(thread);
 if(wait!=WAIT_OBJECT_0)return HRESULT_FROM_WIN32(error?error:ERROR_GEN_FAILURE);
 if(!call->returned){if(log)log("NATIVE_THREAD exited without returning from original entry");return HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED);}
 *result=call->result;return S_OK;
}
