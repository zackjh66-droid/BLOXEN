#include "AudioProvider.h"
#include <windows.h>
#include <cstring>
#include <mutex>
namespace {
std::once_flag once;HMODULE provider=nullptr;
}
void* ResolveAudioProviderImport(const char* module,const char* symbol){
 if(!module||!symbol||_stricmp(module,"dsound.dll"))return nullptr;
 const char* name=nullptr;
 if(!strcmp(symbol,"#8")||!strcmp(symbol,"DirectSoundCaptureEnumerateW"))name="DirectSoundCaptureEnumerateW";
 else if(!strcmp(symbol,"#11")||!strcmp(symbol,"DirectSoundCreate8"))name="DirectSoundCreate8";
 else if(!strcmp(symbol,"#12")||!strcmp(symbol,"DirectSoundCaptureCreate8"))name="DirectSoundCaptureCreate8";
 else return nullptr;
 // Signed package dependency graph only. Retain the actual module for guest
 // callback/object lifetime. A failed load is not replaced by another provider.
 std::call_once(once,[]{provider=LoadPackagedLibrary(L"HWAUD.dll",0);});
 return provider?reinterpret_cast<void*>(GetProcAddress(provider,name)):nullptr;
}
