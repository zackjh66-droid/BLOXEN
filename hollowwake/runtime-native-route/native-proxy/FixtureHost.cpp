// Original owned-code proxy host, sharing the production dispatch implementation.
#include "../NativeProxyDispatch.h"
#include "../PackagedNamespace.h"
#include <windows.h>
#include <cstring>
static volatile LONG missing=0;
static double WINAPI Mix(double a,ULONGLONG b,double c,ULONGLONG d,ULONGLONG e,ULONGLONG f,ULONGLONG g,ULONGLONG h){return a+2*b+3*c+4*d+5*e+6*f+7*g+8*h;}
static void* Own(const char* dll,const char* name){if(!strcmp(dll,"FixtureAbiProvider.dll")&&(!strcmp(name,"MixDouble")||!strcmp(name,"#17")))return reinterpret_cast<void*>(&Mix);return nullptr;}
static void Missing(const char*,const char*){InterlockedIncrement(&missing);}
extern "C" __declspec(dllexport) LONG WINAPI HWFixtureMissing(){return missing;}
extern "C" BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){if(!Hollowwake::ConfigurePackagedNamespace(L"ProxyConvertedFixture.dll",L"ProxyConvertedFixture.dll -owned-fixture"))return FALSE;Hollowwake::ConfigurePackagedProxyResolver(Own,Missing);}return TRUE;}
