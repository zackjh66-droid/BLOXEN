// Original fixture: the mapped DLL entry point calls the exact observed import.
#include <windows.h>
extern "C" {
static DWORD attachTick=0;static BOOL attached=FALSE;
DWORD* volatile fixtureClockPointer=&attachTick; // Require a real DIR64 relocation.
BOOL WINAPI ClockEntry(HINSTANCE,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){*fixtureClockPointer=GetTickCount();attached=TRUE;}return TRUE;
}
__declspec(dllexport) DWORD WINAPI ReadTick(){return GetTickCount();}
__declspec(dllexport) DWORD WINAPI ReadAttachTick(){return *fixtureClockPointer;}
__declspec(dllexport) BOOL WINAPI WasAttached(){return attached;}
}
