// Original OS-loader baseline for imports, delay import and native C++ exception handling.
#include <windows.h>
extern "C" __declspec(dllexport) DWORD FixtureImport(){return GetCurrentThreadId();}
extern "C" __declspec(dllexport) int FixtureException(){try{throw 621;}catch(int value){return value;}}
extern "C" __declspec(dllexport) int FixtureDelay(){DWORD ignored=0;GetFileVersionInfoSizeW(L"hollowwake-nonexistent-fixture-file",&ignored);return GetModuleHandleW(L"version.dll")?1:0;}
