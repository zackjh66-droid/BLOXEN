// Original process-lifetime guest DLL thread-notification registry.
#pragma once
#include <windows.h>
#include <vector>
#include <string>
namespace Hollowwake {
using GuestDllEntry=BOOL(WINAPI*)(HINSTANCE,DWORD,LPVOID);
using GuestTlsSeed=BOOL(WINAPI*)();
void ConfigureGuestThreads(void(*log)(const std::string&));
void BeginGuestInitialization();
void EndGuestInitialization(bool success);
void RegisterGuestThreadModule(HMODULE image,bool staticTls,GuestTlsSeed seed,const std::vector<PIMAGE_TLS_CALLBACK>& callbacks,GuestDllEntry entry);
void MarkGuestThreadModuleReady(HMODULE image);
bool EnterGuestThread();
void LeaveGuestThread();
BOOL WINAPI GuestDisableThreadLibraryCalls(HMODULE image);
void* ResolveGuestThreadImport(const char* dll,const char* name);
}
