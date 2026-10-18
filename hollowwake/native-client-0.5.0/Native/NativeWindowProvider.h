#pragma once
#include <windows.h>
// Original OS-provider candidate. Never return these addresses for a virtual
// WindowBridge HWND. No third-party proxy implementation is copied or loaded.
void* ResolveNativeWindowImport(const char* dll,const char* symbol);
extern "C" __declspec(dllexport) int __cdecl HWNativeWindowProbe(const wchar_t* reportPath);
