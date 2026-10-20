#pragma once
#include <windows.h>
void* ResolveControllerImport(const char* dll,const char* symbol);
extern "C" __declspec(dllexport) void __cdecl HWSetControllerFocus(int active);
extern "C" __declspec(dllexport) int __cdecl HWControllerCount();
