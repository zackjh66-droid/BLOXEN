#pragma once
#include <windows.h>
#include <string>
using NativeClientEntry=int(__cdecl*)();
using NativeClientLogger=void(*)(const std::string&);
using NativeThreadExitCode=BOOL(WINAPI*)(HANDLE,LPDWORD);
// Blocking join for the launch worker, never the CoreWindow dispatcher. A new
// OS thread gets native loader TLS notifications and no inherited COM apartment.
HRESULT RunNativeClientThread(NativeClientEntry,SIZE_T stackReserve,int* entryResult,NativeClientLogger,NativeThreadExitCode);
