#pragma once
#include <windows.h>
// Modules explicitly redirected in the signed prepared client. This does not
// claim that remaining unredirected desktop APIs are available on Xbox.
bool IsStartupPlatformModule(const char* dll);
void* ResolveStartupPlatformAdapter(const char* dll,const char* name);
void* ResolveStartupPlatformImport(const char* dll,const char* name);
