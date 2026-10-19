#include <windows.h>
#include <cassert>
#include <iostream>
using Entry=int(*)();
DWORD InvokeOptional(Entry call){__try{call();return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
int wmain(int argc,wchar_t** argv){
 assert(argc==3);assert(GetFileAttributesW(L"OwnedOptional.dll")==INVALID_FILE_ATTRIBUTES);
 SetLastError(0);HMODULE normal=LoadLibraryW(argv[2]);assert(!normal&&GetLastError()==ERROR_MOD_NOT_FOUND);
 HMODULE consumer=LoadLibraryW(argv[1]);assert(consumer);auto start=reinterpret_cast<Entry>(GetProcAddress(consumer,"OwnedStartWithoutOptional"));auto optional=reinterpret_cast<Entry>(GetProcAddress(consumer,"OwnedRequestOptional"));assert(start&&optional&&start()==621);
 DWORD failure=InvokeOptional(optional);assert(failure==0xc06d007e); // Real VC++ delay-loader ERROR_MOD_NOT_FOUND exception.
 assert(start()==621);FreeLibrary(consumer);
 std::cout<<"PASS actual missing normal dependency blocks OS loading; actual OS load and owned entry with missing delayed DLL; first real delayed call raises 0xC06D007E. No missing API was implemented or counted as resolved. No game/Xbox execution.\n";return 0;
}
