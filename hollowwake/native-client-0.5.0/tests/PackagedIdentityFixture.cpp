// Original module, never proprietary client code.
#include <windows.h>
extern "C" __declspec(dllexport) int __cdecl OwnedIdentity(){return 621;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
