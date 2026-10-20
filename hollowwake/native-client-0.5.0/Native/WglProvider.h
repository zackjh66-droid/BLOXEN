#pragma once
#include <windows.h>
#include <unknwn.h>
// Owned CoreWindow-composition backend, never a fabricated Win32 GL context.
void ConfigureWglProvider(IUnknown* window,BOOL(__cdecl* viewport)(LONG*,LONG*));
void* ResolveWglProviderImport(const char* dll,const char* symbol,bool requireCandidate=false);
extern "C" __declspec(dllexport) int __cdecl HWWglProviderProbe(const wchar_t* report);
