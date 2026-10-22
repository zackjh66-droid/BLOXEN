#pragma once
#include <windows.h>
// Alters only the named import slots and measured compiler copies in this loaded, caller-verified image.
// No host CRT globals, executable bytes or DLL initializers are changed.
struct ClientCompilerImportCopy;
bool BindWinMainCommandImport(HMODULE image,const char* provider,void* replacement,const ClientCompilerImportCopy* copies=nullptr,unsigned copyCount=0);
