// Original, no-CRT fixture: code executes through a private mapper, not LoadLibrary.
#include <windows.h>
extern "C" {
DWORD _tls_index=0;
#pragma section(".tls",long,read,write)
__declspec(allocate(".tls")) char _tls_start=0;
#pragma section(".tls$ZZZ",long,read,write)
__declspec(allocate(".tls$ZZZ")) char _tls_end=0;
__declspec(thread) unsigned long marker=0x62145269;
static PIMAGE_TLS_CALLBACK callbacks[]={nullptr};
#pragma section(".rdata$T",read)
__declspec(allocate(".rdata$T")) extern const IMAGE_TLS_DIRECTORY64 _tls_used={
 (ULONGLONG)&_tls_start,(ULONGLONG)&_tls_end,(ULONGLONG)&_tls_index,(ULONGLONG)callbacks,0,0};
__declspec(dllexport) unsigned long GuestRead(){return marker;}
__declspec(dllexport) void GuestWrite(unsigned long value){marker=value;}
}
