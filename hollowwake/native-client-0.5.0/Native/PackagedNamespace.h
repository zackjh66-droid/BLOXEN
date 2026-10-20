#pragma once
#include <windows.h>
namespace Hollowwake {
// Configure before loading any guest DLL. Names are package basenames, never paths.
// Only an ephemeral, one-use game exchange ticket may be supplied.
// Never pass project sessions, passwords or game refresh/access tokens.
bool ConfigurePackagedRoot(const wchar_t* installedRoot);
bool RegisterPackagedDependency(const wchar_t* module,const wchar_t* relativePath,const wchar_t* originalRelativePath);
bool RegisterPackagedMainIdentity(const wchar_t* originalRelativePath);
bool ConfigurePackagedNamespace(const wchar_t* mainModule,const wchar_t* commandLine);
void* ResolvePackagedNamespaceImport(const char* dll,const char* name);
}

namespace Hollowwake { char* __cdecl PackagedWinMainTail(); }
