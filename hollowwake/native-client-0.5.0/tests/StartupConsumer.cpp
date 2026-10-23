#include <windows.h>
#include <cstdlib>
extern "C" __declspec(dllimport) char* __cdecl _get_narrow_winmain_command_line();
#pragma section(".hwslot",read)
extern "C" __declspec(dllexport) __declspec(allocate(".hwslot")) volatile const ULONG_PTR OwnedCopiedImportSlots[2]={0,0};
extern "C" __declspec(dllexport) char* __cdecl ReadFixtureWinMainCommand(){return _get_narrow_winmain_command_line();}
extern "C" __declspec(dllexport) char* __cdecl ReadCopiedFixtureWinMainCommand(){return reinterpret_cast<char*(__cdecl*)()>(OwnedCopiedImportSlots[0])();}
extern "C" __declspec(dllexport) char* __cdecl ReadFixtureEnvironment(const char* key){return getenv(key);}
