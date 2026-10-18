#include "NativeWindowProvider.h"
#include "ClientDependencyImports.generated.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <string>
#include <mutex>
#include <fstream>
#include <cstring>
namespace {
struct SystemProvider {
 HMODULE module=nullptr;
 explicit SystemProvider(const wchar_t* name){
  using Hold=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);
  auto hold=reinterpret_cast<Hold>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));
  if(hold&&hold(0,name,&module))return;
  auto load=reinterpret_cast<Load>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","LoadLibraryExW"));
  if(load)module=load(name,nullptr,0x800); // Microsoft System32 only, never game/CWD.
 }
 ~SystemProvider(){if(module)FreeLibrary(module);}
};
SystemProvider& User(){static SystemProvider value(L"user32.dll");return value;}
SystemProvider& Gdi(){static SystemProvider value(L"gdi32.dll");return value;}
void* UserSymbol(const char* name){return User().module?reinterpret_cast<void*>(GetProcAddress(User().module,name)):nullptr;}
template<class T>T UserApi(const char* name){return reinterpret_cast<T>(UserSymbol(name));}
template<class T>T GdiApi(const char* name){return reinterpret_cast<T>(Gdi().module?GetProcAddress(Gdi().module,name):nullptr);}
using Procedure=LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM);
struct NativeClass {UINT size,style;Procedure procedure;int classExtra,windowExtra;HINSTANCE instance;HICON icon;HCURSOR cursor;HBRUSH brush;LPCWSTR menu,name;HICON smallIcon;};
struct NativePaint {HDC dc;BOOL erase;RECT paint;BOOL restore,incremental;BYTE reserved[32];};
struct NativeMessage {HWND window;UINT message;WPARAM wp;LPARAM lp;DWORD time;POINT point;DWORD privateData;};
static_assert(sizeof(NativeClass)==80&&sizeof(NativePaint)==72&&sizeof(NativeMessage)==48,"Windows x64 window ABI");
using Register=ATOM(WINAPI*)(const NativeClass*);using Create=HWND(WINAPI*)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
using Default=LRESULT(WINAPI*)(HWND,UINT,WPARAM,LPARAM);using Begin=HDC(WINAPI*)(HWND,NativePaint*);using End=BOOL(WINAPI*)(HWND,const NativePaint*);
using GetDc=HDC(WINAPI*)(HWND);using ReleaseDc=int(WINAPI*)(HWND,HDC);using Is=BOOL(WINAPI*)(HWND);using Invalidate=BOOL(WINAPI*)(HWND,const RECT*,BOOL);
using Update=BOOL(WINAPI*)(HWND);using Destroy=BOOL(WINAPI*)(HWND);using Unregister=BOOL(WINAPI*)(LPCWSTR,HINSTANCE);
using Post=BOOL(WINAPI*)(HWND,UINT,WPARAM,LPARAM);using Peek=BOOL(WINAPI*)(NativeMessage*,HWND,UINT,UINT,UINT);using Dispatch=LRESULT(WINAPI*)(const NativeMessage*);
using Pixel=BOOL(WINAPI*)(HDC,int,int,COLORREF);using ReadPixel=COLORREF(WINAPI*)(HDC,int,int);using Caps=int(WINAPI*)(HDC,int);
struct Probe {
 Default def;Begin begin;End end;Pixel pixel;ReadPixel readPixel;bool paintCalled=false,paintDc=false,paintPixel=false,paintEnded=false,messageDispatched=false;DWORD paintError=0;
};
static thread_local Probe* active=nullptr;
LRESULT CALLBACK OwnedWindow(HWND h,UINT message,WPARAM wp,LPARAM lp){
 auto p=active;if(!p)return 0;
 if(message==0x8000+621){p->messageDispatched=wp==621&&lp==4526925;return 621;}
 if(message==0xf){
  p->paintCalled=true;NativePaint paint={};auto dc=p->begin(h,&paint);p->paintDc=dc!=nullptr;
  if(dc){SetLastError(0);p->paintPixel=p->pixel(dc,1,1,RGB(17,83,149))&&p->readPixel(dc,1,1)==RGB(17,83,149);if(!p->paintPixel)p->paintError=GetLastError();p->paintEnded=p->end(h,&paint)!=FALSE;}
  else p->paintError=GetLastError();return 0;
 }
 return p->def(h,message,wp,lp);
}
}
void* ResolveNativeWindowImport(const char* dll,const char* symbol){
 if(!dll||!symbol||_stricmp(dll,"USER32.dll"))return nullptr;
 // Exact existing import demand, not an arbitrary module/path resolver. Additional
 // test operations below are internal and cannot add names to the client table.
 bool known=false;for(auto& item:clientDependencyImports)if(!_stricmp(item.dll,dll)&&!strcmp(item.symbol,symbol)){known=true;break;}
 return known?UserSymbol(symbol):nullptr;
}
extern "C" int __cdecl HWNativeWindowProbe(const wchar_t* path){
 if(!path)return E_POINTER;static std::mutex gate;std::lock_guard<std::mutex> guard(gate);
 std::string operation="provider";DWORD error=0;HRESULT result=E_FAIL;HWND window=nullptr;ATOM atom=0;HINSTANCE instance=nullptr;std::wstring name=L"HollowwakeOwnedNativeWindow_"+std::to_wstring(GetCurrentProcessId())+L"_"+std::to_wstring(GetCurrentThreadId());
 auto reg=UserApi<Register>("RegisterClassExW");auto create=UserApi<Create>("CreateWindowExW");auto is=UserApi<Is>("IsWindow");auto getDc=UserApi<GetDc>("GetDC");auto releaseDc=UserApi<ReleaseDc>("ReleaseDC");auto invalidate=UserApi<Invalidate>("InvalidateRect");auto update=UserApi<Update>("UpdateWindow");auto destroy=UserApi<Destroy>("DestroyWindow");auto unregisterClass=UserApi<Unregister>("UnregisterClassW");auto post=UserApi<Post>("PostMessageW");auto peek=UserApi<Peek>("PeekMessageW");auto dispatch=UserApi<Dispatch>("DispatchMessageW");auto caps=GdiApi<Caps>("GetDeviceCaps");
 Probe probe={UserApi<Default>("DefWindowProcW"),UserApi<Begin>("BeginPaint"),UserApi<End>("EndPaint"),GdiApi<Pixel>("SetPixelV"),GdiApi<ReadPixel>("GetPixel")};bool realDc=false,releasedDc=false;
 do {
  if(!reg||!create||!is||!getDc||!releaseDc||!invalidate||!update||!destroy||!unregisterClass||!post||!peek||!dispatch||!caps||!probe.def||!probe.begin||!probe.end||!probe.pixel||!probe.readPixel){error=ERROR_PROC_NOT_FOUND;break;}
  using Module=HMODULE(WINAPI*)(LPCWSTR);auto module=reinterpret_cast<Module>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleW"));if(!module){error=ERROR_PROC_NOT_FOUND;break;}instance=module(nullptr);
  NativeClass cls={};cls.size=sizeof(cls);cls.style=0x20;cls.procedure=OwnedWindow;cls.instance=instance;cls.name=name.c_str();active=&probe;
  operation="RegisterClassExW";SetLastError(0);atom=reg(&cls);if(!atom){error=GetLastError();break;}
  operation="CreateWindowExW";SetLastError(0);window=create(0,name.c_str(),L"Hollowwake owned native-window check",0x80000000|0x10000000,12,12,64,64,nullptr,nullptr,instance,nullptr);if(!window){error=GetLastError();break;}
  operation="IsWindow";if(!is(window)){error=ERROR_INVALID_WINDOW_HANDLE;break;}
  operation="GetDC";SetLastError(0);auto dc=getDc(window);if(!dc){error=GetLastError();break;}realDc=caps(dc,12)>0;releasedDc=releaseDc(window,dc)==1;if(!realDc||!releasedDc){error=ERROR_INVALID_HANDLE;break;}
  operation="BeginPaint/GDI pixel readback/EndPaint";SetLastError(0);if(!invalidate(window,nullptr,FALSE)||!update(window)){error=GetLastError();break;}
  if(!probe.paintCalled||!probe.paintDc||!probe.paintPixel||!probe.paintEnded){error=probe.paintError?probe.paintError:ERROR_INVALID_DATA;break;}
  operation="PostMessageW/PeekMessageW/DispatchMessageW";SetLastError(0);if(!post(window,0x8000+621,621,4526925)){error=GetLastError();break;}
  NativeMessage message={};for(unsigned n=0;n<256&&peek(&message,window,0,0,1);++n)dispatch(&message);if(!probe.messageDispatched){error=ERROR_INVALID_DATA;break;}
  operation="complete";result=S_OK;
 }while(false);
 if(window&&destroy&&!destroy(window)){operation="DestroyWindow";error=GetLastError();result=E_FAIL;}
 if(atom&&unregisterClass&&!unregisterClass(name.c_str(),instance)){operation="UnregisterClassW";error=GetLastError();result=E_FAIL;}
 active=nullptr;if(FAILED(result))result=HRESULT_FROM_WIN32(error?error:ERROR_GEN_FAILURE);
 std::ofstream out(path,std::ios::trunc);if(!out)return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
 out<<"{\"scope\":\"Original native-window route check inside hosted UWP AppContainer; not game/Xbox\",\"hresult\":"<<static_cast<long>(result)<<",\"operation\":\""<<operation<<"\",\"win32Error\":"<<error<<",\"nativeWindowCreated\":"<<(window?"true":"false")<<",\"nativeDc\":"<<(realDc&&releasedDc?"true":"false")<<",\"paintCalled\":"<<(probe.paintCalled?"true":"false")<<",\"paintDc\":"<<(probe.paintDc?"true":"false")<<",\"paintPixelReadback\":"<<(probe.paintPixel?"true":"false")<<",\"paintEnded\":"<<(probe.paintEnded?"true":"false")<<",\"ownMessageDispatched\":"<<(probe.messageDispatched?"true":"false")<<",\"gameExecuted\":false,\"xboxTested\":false}";out.flush();return out.good()?result:HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
}
