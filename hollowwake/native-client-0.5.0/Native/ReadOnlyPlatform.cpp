#include "ReadOnlyPlatform.h"
#include "DataFiles.h"
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
Provider& UserResources(){static Provider provider(L"user32.dll");return provider;}
using ImageLoader=HANDLE(WINAPI*)(HINSTANCE,LPCWSTR,UINT,int,int,UINT);
using CursorLoader=HCURSOR(WINAPI*)(LPCWSTR);
static HANDLE WINAPI LoadGameImage(HINSTANCE instance,LPCWSTR name,UINT type,int cx,int cy,UINT flags){
 auto module=UserResources().module;auto load=reinterpret_cast<ImageLoader>(module?GetProcAddress(module,"LoadImageW"):nullptr);
 if(!load){SetLastError(ERROR_PROC_NOT_FOUND);return nullptr;}
 if(flags&0x10){ // LR_LOADFROMFILE: resolve against the existing game-data namespace.
  if(instance||(ULONG_PTR)name<=65535){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}
  auto path=MapReadOnlyImageFile(name);if(path.empty())return nullptr;
  return load(nullptr,path.c_str(),type,cx,cy,flags);
 }
 return load(instance,name,type,cx,cy,flags);
}
static HCURSOR WINAPI LoadGameCursor(LPCWSTR name){
 if((ULONG_PTR)name<=65535){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}
 auto path=MapReadOnlyImageFile(name);if(path.empty())return nullptr;
 auto module=UserResources().module;auto load=reinterpret_cast<CursorLoader>(module?GetProcAddress(module,"LoadCursorFromFileW"):nullptr);
 if(!load){SetLastError(ERROR_PROC_NOT_FOUND);return nullptr;}return load(path.c_str());
}
}
void* ResolveReadOnlyPlatformImport(const char* dll,const char* name){
 if(!dll||!name)return nullptr;HMODULE module=nullptr;
 if(!_stricmp(dll,"winhttp.dll")){
  if(strcmp(name,"WinHttpGetDefaultProxyConfiguration")&&strcmp(name,"WinHttpGetIEProxyConfigForCurrentUser"))return nullptr;
  static Provider provider(L"winhttp.dll");module=provider.module;
 }else if(!_stricmp(dll,"user32.dll")){
  // These operations consume native layout/window-station objects, not virtual
  // HWNDs. They do not send input, open a window,
  // changes the keyboard layout or connects to an external service.
  // Resource operations below accept real OS HBITMAP/HICON/HDC/module handles.
  // They never receive a virtual HWND. GetDC/BeginPaint remain absent because
  // they require a window surface. File decoding uses the content bridge above.
  if(!strcmp(name,"LoadImageW"))return UserResources().module&&GetProcAddress(UserResources().module,name)?reinterpret_cast<void*>(&LoadGameImage):nullptr;
  if(!strcmp(name,"LoadCursorFromFileW"))return UserResources().module&&GetProcAddress(UserResources().module,name)?reinterpret_cast<void*>(&LoadGameCursor):nullptr;
  const char* allowed[]={"GetKeyboardLayout","MapVirtualKeyW","GetProcessWindowStation","GetUserObjectInformationW",
   "CreateIconIndirect","DestroyIcon","DestroyCursor","LoadIconW","DrawStateW","GetIconInfo"};
  bool found=false;for(auto item:allowed)if(!strcmp(name,item))found=true;if(!found)return nullptr;
  static Provider provider(L"user32.dll");module=provider.module;
 }else return nullptr;
 return module?reinterpret_cast<void*>(GetProcAddress(module,name)):nullptr;
}
