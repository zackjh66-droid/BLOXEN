#include <windows.h>
#include <cstring>
extern "C" __declspec(dllexport) void* __cdecl HWResolvePackagedImport(const char* dll,const char* name){
 if(strcmp(dll,"HWDataProvider.dll"))return nullptr;
 auto provider=GetModuleHandleW(L"HWDataProvider.dll");
 return provider?reinterpret_cast<void*>(GetProcAddress(provider,name)):nullptr;
}
