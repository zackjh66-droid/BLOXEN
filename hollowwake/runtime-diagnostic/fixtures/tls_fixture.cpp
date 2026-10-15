// Original tiny no-CRT DLL: initialized static TLS, no TLS callbacks, no entrypoint.
#include <windows.h>
#pragma section(".tls",read,write)
#pragma section(".tls$ZZZ",read,write)
extern "C" {
__declspec(allocate(".tls")) char _tls_start=0;
__declspec(thread) volatile unsigned long tls_marker=0x62145269;
__declspec(allocate(".tls$ZZZ")) char _tls_end=0;
unsigned long _tls_index=0;
#pragma section(".rdata$T",read)
__declspec(allocate(".rdata$T")) extern const IMAGE_TLS_DIRECTORY64 _tls_used={(ULONGLONG)&_tls_start,(ULONGLONG)&_tls_end,(ULONGLONG)&_tls_index,0,0,0};
__declspec(dllexport) unsigned long FixtureRead(){return tls_marker;}
__declspec(dllexport) void FixtureSet(unsigned long value){tls_marker=value;}
}
#pragma comment(linker,"/INCLUDE:_tls_used")
