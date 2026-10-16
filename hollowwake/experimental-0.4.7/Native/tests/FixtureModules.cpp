// Original fixture: exercises the actual guest module bridges during process attach.
#include <windows.h>
static volatile LONG status=-1;static volatile LONG destructorCount=0;
static int* volatile relocationAnchor=nullptr;
static VOID NTAPI ReleaseValue(PVOID value){if(value==reinterpret_cast<void*>(0x621))InterlockedIncrement(&destructorCount);}
extern "C" __declspec(dllexport) int WINAPI ModuleChecks(){return status;}
extern "C" BOOL WINAPI ModulesEntry(HINSTANCE self,DWORD reason,LPVOID){
 if(reason!=DLL_PROCESS_ATTACH)return TRUE;
 auto fail=[](int code){status=code;return TRUE;};
 if(relocationAnchor)return fail(1);
 auto fiber=LoadLibraryExW(L"api-ms-win-core-fibers-l1-1-1",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!fiber)return fail(2);
 auto alloc=reinterpret_cast<DWORD(WINAPI*)(PFLS_CALLBACK_FUNCTION)>(GetProcAddress(fiber,"FlsAlloc"));
 auto put=reinterpret_cast<BOOL(WINAPI*)(DWORD,PVOID)>(GetProcAddress(fiber,"FlsSetValue"));
 auto get=reinterpret_cast<PVOID(WINAPI*)(DWORD)>(GetProcAddress(fiber,"FlsGetValue"));
 auto freeSlot=reinterpret_cast<BOOL(WINAPI*)(DWORD)>(GetProcAddress(fiber,"FlsFree"));
 if(!alloc||!put||!get||!freeSlot)return fail(3);
 auto slot=alloc(ReleaseValue);if(slot==FLS_OUT_OF_INDEXES||!put(slot,reinterpret_cast<void*>(0x621))||get(slot)!=reinterpret_cast<void*>(0x621)||!freeSlot(slot)||destructorCount!=1)return fail(4);
 SetLastError(0);if(GetProcAddress(fiber,"HollowwakeMissingOptionalFunction")||GetLastError()!=ERROR_PROC_NOT_FOUND)return fail(5);
 auto kernel=LoadLibraryExW(L"KERNEL32",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!kernel)return fail(6);
 auto tick=reinterpret_cast<ULONGLONG(WINAPI*)()>(GetProcAddress(kernel,"GetTickCount64"));if(!tick||!tick())return fail(7);
 if(LoadLibraryExW(L"C:\\untrusted\\kernel32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32))return fail(8);
 if(LoadLibraryExW(L"kernel32",nullptr,DONT_RESOLVE_DLL_REFERENCES)||GetLastError()!=ERROR_INVALID_PARAMETER)return fail(9);
 if(LoadLibraryExW(L"kernel32",reinterpret_cast<HANDLE>(1),0))return fail(10);
 if(LoadLibraryExW(L"api-ms-win-core-hollowwake-invented-l9-9-9",nullptr,0))return fail(11);
 auto target=LoadLibraryExW(L"FixtureTarget.dll",nullptr,0);if(!target)return fail(12);
 auto byName=GetProcAddress(target,"Answer"),byOrdinal=GetProcAddress(target,reinterpret_cast<LPCSTR>(1));
 if(!byName||byName!=byOrdinal||reinterpret_cast<int(*)()>(byName)()!=621)return fail(13);
 HMODULE fromAddress=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(byName),&fromAddress)||fromAddress!=target)return fail(14);
 if(GetModuleHandleW(L"FixtureTarget")!=target)return fail(15);
 wchar_t filename[1024]={};if(!GetModuleFileNameW(target,filename,1024)||!filename[0])return fail(16);
 wchar_t tiny[2]={};SetLastError(0);if(GetModuleFileNameW(target,tiny,2)!=2||tiny[1]||GetLastError()!=ERROR_INSUFFICIENT_BUFFER)return fail(17);
 auto forwarded=LoadLibraryExW(L"FixtureFirst.dll",nullptr,0);if(!forwarded||GetProcAddress(forwarded,"Answer")!=byName)return fail(18);
 SetLastError(0);if(GetProcAddress(target,"AbsentExport")||GetLastError()!=ERROR_PROC_NOT_FOUND)return fail(19);
 if(!FreeLibrary(target)||!FreeLibrary(forwarded)||!FreeLibrary(kernel)||!FreeLibrary(fiber))return fail(20);
 if(FreeLibrary(fiber)||GetLastError()!=ERROR_INVALID_HANDLE)return fail(21);
 auto synch=LoadLibraryExW(L"api-ms-win-core-synch-l1-2-0",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!synch)return fail(22);
 auto init=reinterpret_cast<BOOL(WINAPI*)(LPCRITICAL_SECTION,DWORD,DWORD)>(GetProcAddress(synch,"InitializeCriticalSectionEx"));if(!init)return fail(23);
 CRITICAL_SECTION cs;if(!init(&cs,1000,0))return fail(24);EnterCriticalSection(&cs);LeaveCriticalSection(&cs);DeleteCriticalSection(&cs);
 if(!FreeLibrary(synch))return fail(25);
 if(!GetModuleHandleW(nullptr)||GetModuleHandleW(L"FixtureModules.dll")!=self)return fail(26);
 status=0;return TRUE;
}
