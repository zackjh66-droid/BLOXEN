#include "../../runtime-native-route/NativeSystem.h"
#include <cwchar>
// Mesa's OS graphics providers are not writable downloaded dependencies. Load
// only these explicit Microsoft system basenames through the real OS operation.
extern "C" __declspec(dllexport) HMODULE __cdecl HWLoadGraphicsSystemModule(LPCWSTR name){
 if(!name||(_wcsicmp(name,L"d3d12.dll")&&_wcsicmp(name,L"dxgi.dll")&&_wcsicmp(name,L"dxcore.dll"))){SetLastError(ERROR_INVALID_NAME);return nullptr;}
 using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);auto load=reinterpret_cast<Load>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","LoadLibraryExW"));
 if(!load){SetLastError(ERROR_PROC_NOT_FOUND);return nullptr;}return load(name,nullptr,0x800); // System32 only; OS/AppContainer denial is preserved.
}
