// Original fixture: the mapped DLL entry point calls the exact observed import.
#include <windows.h>
extern "C" {
struct Guard {ULONGLONG before;SYSTEM_INFO value;ULONGLONG after;};
static Guard systemInfo{};
static DWORD attachTick=0;static BOOL attached=FALSE;
DWORD* volatile fixtureClockPointer=&attachTick; // Require a real DIR64 relocation.
BOOL WINAPI ClockEntry(HINSTANCE,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){*fixtureClockPointer=GetTickCount();systemInfo.before=0x12345678aabbccddULL;systemInfo.after=0xfedcba9876543210ULL;GetSystemInfo(&systemInfo.value);attached=TRUE;}return TRUE;
}
__declspec(dllexport) BOOL WINAPI ReadSystemInformation(LPSYSTEM_INFO out){
 if(!attached||systemInfo.before!=0x12345678aabbccddULL||systemInfo.after!=0xfedcba9876543210ULL)return FALSE;
 auto source=reinterpret_cast<volatile BYTE*>(&systemInfo.value);auto dest=reinterpret_cast<volatile BYTE*>(out);for(unsigned i=0;i<sizeof(SYSTEM_INFO);++i)dest[i]=source[i];return TRUE;
}
__declspec(dllexport) DWORD WINAPI ReadTick(){return GetTickCount();}
__declspec(dllexport) DWORD WINAPI ReadAttachTick(){return *fixtureClockPointer;}
__declspec(dllexport) BOOL WINAPI WasAttached(){return attached;}
}
