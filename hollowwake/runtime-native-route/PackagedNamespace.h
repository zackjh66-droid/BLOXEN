#pragma once
#include <windows.h>
namespace Hollowwake {
// Configure before loading any guest DLL. Names are package basenames, never paths.
// No refresh token, password or account credential belongs in commandLine.
bool ConfigurePackagedNamespace(const wchar_t* mainModule,const wchar_t* commandLine);
void* ResolvePackagedNamespaceImport(const char* dll,const char* name);
}
