// Original restricted guest module namespace; never forwards archive paths to OS LoadLibrary.
#pragma once
#include <windows.h>
#include <string>
struct GuestModuleRecord { HMODULE handle=nullptr; std::wstring path; SIZE_T size=0; bool initialized=false; };
bool FindGuestDependency(const std::string& name,GuestModuleRecord& result);
bool FindGuestDependencyAddress(ULONG_PTR address,GuestModuleRecord& result);
void* QueryDependencyExport(const char* dll,const char* name);
void ConfigureGuestModules(void*(*query)(const char*,const char*),void(*log)(const std::string&),bool(*nativeScope)(const char*)=nullptr);
void RegisterGuestMainImage(void* base,SIZE_T size,const std::wstring& path);
void* ResolveModuleLoaderImport(const char* dll,const char* name);
