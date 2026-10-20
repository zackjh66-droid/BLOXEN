#pragma once
#include <windows.h>
// Explicit native API delegation/capability errors, not desktop feature emulation.
// An absent native API is recorded as unavailable even when its typed failure
// adapter lets the original client handle that failure instead of terminating.
void ConfigureDesktopCapabilityLogger(void(*logger)(const char*,const char*,DWORD));
bool IsDesktopCapabilityModule(const char* module);
bool IsDesktopCapabilityAdapter(const char* module,const char* symbol);
bool NativeDesktopCapabilityAvailable(const char* module,const char* symbol,DWORD* error);
void* ResolveDesktopCapabilityImport(const char* module,const char* symbol);
