// Test the real graphics-provider loader policy, not renderer/lobby behavior.
#include <windows.h>
#include <cassert>
#include <string>
#include <iostream>
extern "C" HMODULE __cdecl HWLoadGraphicsSystemModule(LPCWSTR);
int wmain(int argc,wchar_t** argv){
 assert(argc==2);wchar_t path[32768],directory[32768],original[32768];assert(GetCurrentDirectoryW(32768,original));assert(GetFullPathNameW(argv[1],32768,path,nullptr));
 std::wstring owned(path);auto slash=owned.find_last_of(L'\\');assert(slash!=std::wstring::npos);auto testDirectory=owned.substr(0,slash);auto fake=testDirectory+L"\\dxgi.dll";assert(CopyFileW(owned.c_str(),fake.c_str(),TRUE));assert(SetCurrentDirectoryW(testDirectory.c_str()));
 SetLastError(0);assert(!HWLoadGraphicsSystemModule(L"..\\dxgi.dll")&&GetLastError()==ERROR_INVALID_NAME);
 assert(!HWLoadGraphicsSystemModule(L"unrelated.dll"));
 HMODULE dxgi=HWLoadGraphicsSystemModule(L"DXGI.dll");assert(dxgi);assert(GetModuleFileNameW(dxgi,path,32768));assert(GetSystemDirectoryW(directory,32768));auto expected=std::wstring(directory)+L"\\dxgi.dll";assert(!_wcsicmp(path,expected.c_str()));assert(GetProcAddress(dxgi,"CreateDXGIFactory1"));FreeLibrary(dxgi);
 assert(SetCurrentDirectoryW(original));assert(DeleteFileW(fake.c_str()));
 std::cout<<"PASS actual System32 graphics provider, writable CWD lookalike ignored, path/unlisted-module rejection; not game/Xbox\n";return 0;
}
