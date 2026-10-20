#include "StartupPlatform.h"
#include "MultimediaTimers.h"
#include "AudioProvider.h"
#include "DesktopCapabilities.h"
#include "NetworkAvailability.h"
#include "PerformanceMarkers.h"
#include "PlatformImports.generated.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <cstring>
#include <mutex>
#include <map>
#include <string>
namespace {
struct Provider {HMODULE module=nullptr;explicit Provider(const wchar_t* name){using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);auto load=reinterpret_cast<Load>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","LoadLibraryExW"));if(load)module=load(name,nullptr,0x800);}~Provider(){if(module)FreeLibrary(module);}};
HMODULE ProviderFor(const char* dll){if(!_stricmp(dll,"winmm.dll")){static Provider p(L"winmm.dll");return p.module;}if(!_stricmp(dll,"d3d9.dll")){static Provider p(L"d3d9.dll");return p.module;}return nullptr;}
bool Known(const char* dll,const char* name){for(auto& p:platformImportPairs)if(!_stricmp(dll,p.dll)&&!strcmp(name,p.symbol))return true;return false;}
}
bool IsStartupPlatformModule(const char* dll){return dll&&(!_stricmp(dll,"winmm.dll")||!_stricmp(dll,"d3d9.dll")||!_stricmp(dll,"sensapi.dll")||!_stricmp(dll,"dsound.dll")||IsDesktopCapabilityModule(dll));}
void* ResolveStartupPlatformAdapter(const char* dll,const char* name){if(!dll||!name)return nullptr;if(auto p=ResolveAudioProviderImport(dll,name))return p;if(auto p=ResolveNetworkAvailabilityImport(dll,name))return p;if(auto p=ResolveMultimediaTimerImport(dll,name))return p;if(auto p=ResolvePerformanceImport(dll,name))return p;return IsDesktopCapabilityAdapter(dll,name)?ResolveDesktopCapabilityImport(dll,name):nullptr;}
void* ResolveStartupPlatformImport(const char* dll,const char* name){if(!IsStartupPlatformModule(dll)||!name||!Known(dll,name))return nullptr;if(auto p=ResolveStartupPlatformAdapter(dll,name))return p;if(IsDesktopCapabilityModule(dll))return ResolveDesktopCapabilityImport(dll,name);auto module=ProviderFor(dll);return module?reinterpret_cast<void*>(GetProcAddress(module,name)):nullptr;}
