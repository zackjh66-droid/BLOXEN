// Original bounded legacy startup ABI. No modern FILE/RTTI/EH data aliases.
#pragma once
#include <windows.h>
#include <roerrorapi.h>
#include <intrin.h>
#include <cstdlib>
#include <cerrno>
#include <mutex>
#include <cstring>
#include <excpt.h>
extern "C" EXCEPTION_DISPOSITION __cdecl __C_specific_handler(PEXCEPTION_RECORD,void*,PCONTEXT,PDISPATCHER_CONTEXT);
namespace Hollowwake {
using LegacyExit=int(__cdecl*)();
using LegacyFatalLog=void(*)(const char*);
inline LegacyFatalLog& LegacyFatalLogger(){static LegacyFatalLog log=nullptr;return log;}
__declspec(noreturn) inline void LegacyStartupFatal(const char* message){if(LegacyFatalLogger())LegacyFatalLogger()(message);RoFailFastWithErrorContext(E_NOTIMPL);__fastfail(7);}
inline void* __cdecl LegacyNew(size_t size){auto p=std::malloc(size?size:1);if(!p)LegacyStartupFatal("LEGACY_NEW_ALLOCATION_FAILURE: old new-handler/exception ABI unavailable");return p;}
inline void __cdecl LegacyDelete(void* value){std::free(value);}
inline std::recursive_mutex& LegacyLock(int id){static std::recursive_mutex locks[64];if(id<0||id>=64)LegacyStartupFatal("LEGACY_CRT_LOCK_INDEX_UNSUPPORTED");return locks[id];}
inline void __cdecl LegacyLockEnter(int id){LegacyLock(id).lock();}
inline void __cdecl LegacyLockLeave(int id){LegacyLock(id).unlock();}
inline LegacyExit __cdecl LegacyDllOnExit(LegacyExit callback,LegacyExit** begin,LegacyExit** end){
 if(!callback||!begin||!end){errno=EINVAL;return nullptr;}
 std::lock_guard<std::recursive_mutex> guard(LegacyLock(8));
 auto first=reinterpret_cast<ULONG_PTR>(*begin),last=reinterpret_cast<ULONG_PTR>(*end);
 if(last<first||(last-first)%sizeof(LegacyExit)||(!first&&last)){errno=EINVAL;return nullptr;}
 size_t count=(last-first)/sizeof(LegacyExit);
 if(count>=1048576){errno=ENOMEM;return nullptr;}
 auto resized=static_cast<LegacyExit*>(std::realloc(*begin,(count+1)*sizeof(LegacyExit)));
 if(!resized){errno=ENOMEM;return nullptr;}
 resized[count]=callback;*begin=resized;*end=resized+count+1;return callback;
}
struct LegacyProcessExitTable{LegacyExit* begin=nullptr;LegacyExit* end=nullptr;};
inline LegacyProcessExitTable& ProcessExitTable(){static LegacyProcessExitTable table;return table;}
inline LegacyExit __cdecl LegacyOnExit(LegacyExit callback){auto& t=ProcessExitTable();return LegacyDllOnExit(callback,&t.begin,&t.end);}
inline void RunLegacyProcessExitCallbacks(){
 for(;;){LegacyExit callback=nullptr;{
  std::lock_guard<std::recursive_mutex> guard(LegacyLock(8));auto& t=ProcessExitTable();
  if(t.begin==t.end){std::free(t.begin);t.begin=t.end=nullptr;break;}
  callback=*--t.end;
 }if(callback)callback();}
}
inline void* ResolveLegacyStartupImport(const char* dll,const char* name){
 if(!dll||!name||_stricmp(dll,"msvcrt.dll"))return nullptr;
 #define LEGACY(symbol,target) if(!strcmp(name,symbol))return reinterpret_cast<void*>(&target)
 LEGACY("??2@YAPEAX_K@Z",LegacyNew);LEGACY("??_U@YAPEAX_K@Z",LegacyNew);
 LEGACY("??3@YAXPEAX@Z",LegacyDelete);LEGACY("??_V@YAXPEAX@Z",LegacyDelete);
 LEGACY("__C_specific_handler",::__C_specific_handler);
 LEGACY("_lock",LegacyLockEnter);LEGACY("_unlock",LegacyLockLeave);
 LEGACY("__dllonexit",LegacyDllOnExit);LEGACY("_onexit",LegacyOnExit);
 #undef LEGACY
 return nullptr;
}
}
