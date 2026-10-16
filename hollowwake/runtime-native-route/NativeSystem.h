// Original resolver for already-loaded operating-system modules. No downloaded DLL loading.
#pragma once
#include <windows.h>
namespace Hollowwake {
void* ResolveNativeSystemImport(const char* dll,const char* symbol);
bool NativeLegacyCrtAvailable();
}
