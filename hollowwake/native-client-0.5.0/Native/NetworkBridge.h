#pragma once
#include <windows.h>
// Fixed HTTPS backend hostname only; no URL paths, credentials or arbitrary URLs.
extern "C" __declspec(dllexport) BOOL __cdecl HWConfigureNetwork(const wchar_t* hostname,unsigned short port);
void* ResolveNetworkImport(const char* dll,const char* name);

// Shared endpoint policy; no DNS or I/O occurs in this query.
bool IsConfiguredNetworkHost(const char* hostname,unsigned short port);
