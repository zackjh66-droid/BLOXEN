#include "../../runtime-native-route/NativeSystem.h"
#include <vector>
#include <cwchar>
// DSOAL retains itself for its callback lifetime. Perform the actual OS module
// reference operation, scoped to the installed audio implementation's basename.
extern "C" __declspec(dllexport) BOOL __cdecl HWRetainAudioModule(HMODULE module){
 using Filename=DWORD(WINAPI*)(HMODULE,LPWSTR,DWORD);using Hold=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);
 auto filename=reinterpret_cast<Filename>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleFileNameW"));auto hold=reinterpret_cast<Hold>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));if(!filename||!hold){SetLastError(ERROR_PROC_NOT_FOUND);return FALSE;}
 std::vector<wchar_t> path(32768);auto size=filename(module,path.data(),static_cast<DWORD>(path.size()));if(!size||size>=path.size())return FALSE;auto base=wcsrchr(path.data(),L'\\');if(!base||_wcsicmp(base+1,L"HWAUD.dll")){SetLastError(ERROR_INVALID_NAME);return FALSE;}
 HMODULE held=nullptr;if(!hold(4,reinterpret_cast<LPCWSTR>(module),&held))return FALSE;
 if(held!=module){FreeLibrary(held);SetLastError(ERROR_INVALID_HANDLE);return FALSE;}return TRUE;
}
