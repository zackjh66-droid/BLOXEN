// Test host only: Windows desktop APIs remain original, not Hollowwake proxies.
// This isolates EXE-to-DLL startup semantics from the compatibility adapters.
#define STARTUP_PROBE_LIBRARY
#include "NativeStartupProbe.cpp"
#include "BaselinePin.generated.h"
DWORD WINAPI Enter(void* address){return reinterpret_cast<DWORD(__cdecl*)()>(address)();}
int wmain(){
 wchar_t module[32768]={};DWORD n=GetModuleFileNameW(nullptr,module,32768);if(!n||n>=32768)return 2;
 std::wstring directory(module,n);directory=directory.substr(0,directory.find_last_of(L"/\\"));
 auto image=directory+L"\\Fortnite621.baseline.private.dll";std::string hash;
 if(!Hash(image.c_str(),hash)||hash!=kBaselinePin)return 3;
 auto loaded=LoadLibraryExW(image.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);if(!loaded)return static_cast<int>(GetLastError());
 auto entry=GetProcAddress(loaded,"StartHollowwakeClient");if(!entry)return ERROR_PROC_NOT_FOUND;
 Handle thread;thread.value=CreateThread(nullptr,1048576,Enter,reinterpret_cast<void*>(entry),STACK_SIZE_PARAM_IS_A_RESERVATION,nullptr);
 if(!thread.value)return static_cast<int>(GetLastError());WaitForSingleObject(thread.value,INFINITE);DWORD result=0;GetExitCodeThread(thread.value,&result);return static_cast<int>(result);
}
