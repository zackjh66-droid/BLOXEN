#include "ReadOnlyPlatform.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <cstring>
namespace {
struct Provider {
 HMODULE module=nullptr;
 explicit Provider(const wchar_t* name){
  using Hold=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);
  auto hold=reinterpret_cast<Hold>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));
  if(hold&&hold(0,name,&module))return;
  auto load=reinterpret_cast<Load>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","LoadLibraryExW"));
  if(load)module=load(name,nullptr,0x800); // Only the named Microsoft System32 provider.
 }
 ~Provider(){if(module)FreeLibrary(module);}
};
}
void* ResolveReadOnlyPlatformImport(const char* dll,const char* name){
 if(!dll||!name)return nullptr;HMODULE module=nullptr;
 if(!_stricmp(dll,"winhttp.dll")){
  if(strcmp(name,"WinHttpGetDefaultProxyConfiguration")&&strcmp(name,"WinHttpGetIEProxyConfigForCurrentUser"))return nullptr;
  static Provider provider(L"winhttp.dll");module=provider.module;
 }else if(!_stricmp(dll,"user32.dll")){
  // Neither operation consumes a virtual HWND, sends input, opens a window,
  // changes the keyboard layout or connects to an external service.
  if(strcmp(name,"GetKeyboardLayout")&&strcmp(name,"MapVirtualKeyW"))return nullptr;
  static Provider provider(L"user32.dll");module=provider.module;
 }else return nullptr;
 return module?reinterpret_cast<void*>(GetProcAddress(module,name)):nullptr;
}
