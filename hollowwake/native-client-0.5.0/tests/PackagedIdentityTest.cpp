#include "../Native/PackagedNamespace.h"
#include <windows.h>
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
template<class T>T api(const char* name){auto p=Hollowwake::ResolvePackagedNamespaceImport("kernel32.dll",name);assert(p);return reinterpret_cast<T>(p);}
int wmain(int argc,wchar_t** argv){
 assert(argc==2);wchar_t full[32768];assert(GetFullPathNameW(argv[1],32768,full,nullptr));
 std::wstring physical(full);auto slash=physical.find_last_of(L"\\/");assert(slash!=std::wstring::npos);auto root=physical.substr(0,slash),base=physical.substr(slash+1);
 HMODULE owned=LoadLibraryW(full);assert(owned);auto entry=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(owned,"OwnedIdentity"));assert(entry&&entry()==621);
 assert(Hollowwake::ConfigurePackagedNamespace(base.c_str(),L"\"OwnedOriginal.exe\" -d3d11 -nomatchmaking"));
 assert(Hollowwake::ConfigurePackagedRoot(root.c_str()));
 assert(!Hollowwake::RegisterPackagedMainIdentity(L"../outside.exe"));
 assert(Hollowwake::RegisterPackagedMainIdentity(L"6.21/FortniteGame/Binaries/Win64/OwnedOriginal.exe"));
 assert(!Hollowwake::RegisterPackagedMainIdentity(L"6.21/Other.exe"));
 auto module=api<decltype(&GetModuleHandleW)>("GetModuleHandleW");auto filename=api<decltype(&GetModuleFileNameW)>("GetModuleFileNameW");auto filenameA=api<decltype(&GetModuleFileNameA)>("GetModuleFileNameA");
 assert(module(nullptr)==owned&&module(L"OwnedOriginal.exe")==owned&&module(base.c_str())==owned);
 auto path=root+L"\\6.21\\FortniteGame\\Binaries\\Win64\\OwnedOriginal.exe";
 assert(module(path.c_str())==owned);assert(module(L"6.21/FortniteGame/Binaries/Win64/OwnedOriginal.exe")==owned);
 wchar_t result[32768];assert(filename(nullptr,result,32768)==path.size());assert(!_wcsicmp(result,path.c_str()));
 assert(filename(owned,result,32768)==path.size());
 wchar_t small[5]={};SetLastError(0);assert(filename(owned,small,5)==5&&small[4]==0&&GetLastError()==ERROR_INSUFFICIENT_BUFFER);
 SetLastError(0);assert(filename(nullptr,nullptr,0)==0&&GetLastError()==ERROR_INSUFFICIENT_BUFFER);
 SetLastError(0);assert(filename(reinterpret_cast<HMODULE>(1),result,32768)==0&&GetLastError()==ERROR_INVALID_HANDLE);
 char expected[32768],narrow[32768];assert(WideCharToMultiByte(CP_ACP,0,path.c_str(),-1,expected,32768,nullptr,nullptr));
 assert(filenameA(nullptr,narrow,32768)==strlen(expected)&&!strcmp(narrow,expected));char shortA[4];assert(filenameA(owned,shortA,4)==4&&shortA[3]==0);
 // Guest aliasing must not change the host OS identity or the DLL's real bytes.
 assert(GetModuleFileNameW(owned,result,32768));assert(!_wcsicmp(result,physical.c_str()));assert(entry()==621);
 std::cout<<"PASS original logical EXE identity, real OS DLL ownership, W/A lengths and truncation, path aliases and invalid handles; desktop owned fixture only\n";
 FreeLibrary(owned);return 0;
}
