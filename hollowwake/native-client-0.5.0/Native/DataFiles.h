#pragma once
#include <windows.h>
bool ConfigureDataFiles(const wchar_t* packageRoot,const wchar_t* localDataRoot);
bool RegisterCodeFileAlias(const wchar_t* originalRelative,const wchar_t* packageRelative);
void* ResolveDataFileImport(const char* dll,const char* name);
