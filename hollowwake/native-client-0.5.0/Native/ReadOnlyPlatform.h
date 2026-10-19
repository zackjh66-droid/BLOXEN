#pragma once
#include <windows.h>
// Actual OS proxy/keyboard-layout queries and native icon/cursor resources.
// File resources go through the existing game-content namespace. No HTTP request
// creation, virtual HWND passthrough, input injection or login implementation.
void* ResolveReadOnlyPlatformImport(const char* dll,const char* symbol);
using HWCursorFileProvider=HCURSOR(WINAPI*)(LPCWSTR);
using HWImageFileProvider=HANDLE(WINAPI*)(HINSTANCE,LPCWSTR,UINT,int,int,UINT);
// Shared production path; tests can exercise the documented replacement when
// the old cursor-file export is absent. Neither provider is assumed available.
HCURSOR LoadGameCursorWithProviders(LPCWSTR name,HWCursorFileProvider direct,HWImageFileProvider image);
