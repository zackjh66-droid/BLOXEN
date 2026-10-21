#include <windows.h>
__declspec(thread) DWORD ownedValue=621;
extern "C" __declspec(dllexport) DWORD ReadOwnedTls(){return ownedValue;}
extern "C" __declspec(dllexport) void ChangeOwnedTls(DWORD value){ownedValue=value;}
