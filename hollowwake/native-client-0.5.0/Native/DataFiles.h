#pragma once
#include <windows.h>
#include <string>
using DataFilesLogger=void(*)(const std::string&);
void ConfigureDataFilesLogger(DataFilesLogger);
bool ConfigureDataFiles(const wchar_t* packageRoot,const wchar_t* localDataRoot);
bool RegisterCodeFileAlias(const wchar_t* originalRelative,const wchar_t* packageRelative);
void* ResolveDataFileImport(const char* dll,const char* name);

// Resource decoders may read game data, never executable aliases or host files.
std::wstring MapReadOnlyImageFile(const wchar_t* name);
