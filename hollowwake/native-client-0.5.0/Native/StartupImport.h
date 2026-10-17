#pragma once
#include <windows.h>
// Alters only the named import slot in this loaded, caller-verified image.
// No host CRT globals, executable bytes or DLL initializers are changed.
bool BindWinMainCommandImport(HMODULE image,const char* provider,void* replacement);
