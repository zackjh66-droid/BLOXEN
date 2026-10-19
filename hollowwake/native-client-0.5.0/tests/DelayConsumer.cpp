#include <windows.h>
extern "C" __declspec(dllimport) int OwnedOptionalOperation();
extern "C" __declspec(dllexport) int OwnedStartWithoutOptional(){return 621;}
extern "C" __declspec(dllexport) int OwnedRequestOptional(){return OwnedOptionalOperation();}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
