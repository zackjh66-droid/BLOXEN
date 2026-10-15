// Original DLL using the compiler/CRT's standard TLS directory and initialization.
#include <windows.h>
__declspec(thread) volatile unsigned long tls_marker=0x62145269;
extern "C" __declspec(dllexport) unsigned long FixtureRead(){return tls_marker;}
extern "C" __declspec(dllexport) void FixtureSet(unsigned long value){tls_marker=value;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
