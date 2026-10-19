#pragma once
#include <windows.h>
// Query live UWP connection profiles; no connection to a game/backend is made.
BOOL WINAPI HWIsNetworkAlive(DWORD* flags);
void* ResolveNetworkAvailabilityImport(const char* dll,const char* name);
