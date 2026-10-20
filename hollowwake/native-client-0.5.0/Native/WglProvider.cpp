#include "WglProvider.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <wrl/client.h>
#include <mutex>
#include <set>
#include <string>
#include <cstring>
#include <atomic>
using Microsoft::WRL::ComPtr;
namespace {
std::atomic<void(*)(const char*,DWORD)> diagnostic{nullptr};
void Trace(const char* text,DWORD error=0){auto last=GetLastError();if(auto callback=diagnostic.load())callback(text,error);SetLastError(last);}
std::recursive_mutex gate;ComPtr<IUnknown> window;BOOL(__cdecl* viewport)(LONG*,LONG*)=nullptr;HMODULE mesa=nullptr,gallium=nullptr;std::set<HGLRC> contexts;thread_local HDC boundGuestDC=nullptr;
using FromDc=HWND(WINAPI*)(HDC);using Owner=DWORD(WINAPI*)(HWND,LPDWORD);FromDc fromDc=nullptr;Owner owner=nullptr;
template<class T>T Sys(const char* dll,const char* symbol){return reinterpret_cast<T>(Hollowwake::ResolveNativeSystemImport(dll,symbol));}
template<class T>T Api(const char* name){return mesa?reinterpret_cast<T>(GetProcAddress(mesa,name)):nullptr;}
bool Ready(){std::lock_guard<std::recursive_mutex> lock(gate);if(mesa&&gallium)return true;if(!window||!viewport){SetLastError(ERROR_INVALID_STATE);return false;}
 Trace("LoadPackagedLibrary libgallium_wgl.dll begin");auto g=LoadPackagedLibrary(L"libgallium_wgl.dll",0);Trace("LoadPackagedLibrary libgallium_wgl.dll returned",g?0:GetLastError());if(!g)return false;Trace("LoadPackagedLibrary HWMGL.dll begin");auto m=LoadPackagedLibrary(L"HWMGL.dll",0);Trace("LoadPackagedLibrary HWMGL.dll returned",m?0:GetLastError());if(!m){auto error=GetLastError();FreeLibrary(g);SetLastError(error);return false;}
 auto configure=reinterpret_cast<HRESULT(__cdecl*)(IUnknown*,BOOL(__cdecl*)(LONG*,LONG*))>(GetProcAddress(g,"HWMesaConfigureWindow"));auto getWindow=Sys<HMODULE(WINAPI*)(LPCWSTR)>("kernel32.dll","GetModuleHandleW");auto user=getWindow?getWindow(L"user32.dll"):nullptr;fromDc=user?reinterpret_cast<FromDc>(GetProcAddress(user,"WindowFromDC")):nullptr;owner=user?reinterpret_cast<Owner>(GetProcAddress(user,"GetWindowThreadProcessId")):nullptr;
 Trace("HWMesaConfigureWindow begin");HRESULT hr=configure?configure(window.Get(),viewport):HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);Trace("HWMesaConfigureWindow returned",hr);if(!fromDc)Trace("Missing USER32.dll!WindowFromDC",ERROR_PROC_NOT_FOUND);if(!owner)Trace("Missing USER32.dll!GetWindowThreadProcessId",ERROR_PROC_NOT_FOUND);if(FAILED(hr)||!fromDc||!owner){FreeLibrary(m);FreeLibrary(g);SetLastError(FAILED(hr)?static_cast<DWORD>(hr):ERROR_PROC_NOT_FOUND);return false;}mesa=m;gallium=g;return true;
}
HDC CoreDc(HDC guest){if(!Ready())return nullptr;DWORD process=0;auto hwnd=guest?fromDc(guest):nullptr;if(!hwnd||!owner(hwnd,&process)||process!=GetCurrentProcessId()){SetLastError(ERROR_INVALID_HANDLE);return nullptr;}
 auto get=reinterpret_cast<HDC(__cdecl*)()>(GetProcAddress(gallium,"HWMesaCoreDC"));if(!get){SetLastError(ERROR_PROC_NOT_FOUND);return nullptr;}return get();
}
HGLRC WINAPI Create(HDC guest){auto dc=CoreDc(guest);if(!dc)return nullptr;auto prepare=reinterpret_cast<BOOL(__cdecl*)(HDC)>(GetProcAddress(gallium,"HWMesaPrepareDC"));auto create=Api<HGLRC(WINAPI*)(HDC)>("wglCreateContext");if(!prepare||!create){SetLastError(ERROR_PROC_NOT_FOUND);return nullptr;}if(!prepare(dc))return nullptr;auto context=create(dc);if(context){std::lock_guard<std::recursive_mutex> lock(gate);contexts.insert(context);}return context;}
BOOL WINAPI Delete(HGLRC context){if(!Ready())return FALSE;std::lock_guard<std::recursive_mutex> lock(gate);if(!contexts.count(context)){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}auto call=Api<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext");BOOL result=call?call(context):FALSE;if(result)contexts.erase(context);return result;}
HGLRC WINAPI Current(){if(!Ready())return nullptr;auto call=Api<HGLRC(WINAPI*)()>("wglGetCurrentContext");return call?call():nullptr;}
HDC WINAPI CurrentDc(){return Current()?boundGuestDC:nullptr;}
BOOL WINAPI MakeCurrent(HDC guest,HGLRC context){if(!Ready())return FALSE;std::lock_guard<std::recursive_mutex> lock(gate);if(context&&!contexts.count(context)){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}auto dc=context?CoreDc(guest):nullptr;if(context&&!dc)return FALSE;auto call=Api<BOOL(WINAPI*)(HDC,HGLRC)>("wglMakeCurrent");BOOL result=call?call(dc,context):FALSE;if(result)boundGuestDC=context?guest:nullptr;return result;}
PROC WINAPI Procedure(LPCSTR name);
PROC DriverProc(const char* name){auto get=Api<PROC(WINAPI*)(LPCSTR)>("wglGetProcAddress");auto value=get?get(name):nullptr;return value?value:(!strncmp(name,"gl",2)?reinterpret_cast<PROC>(GetProcAddress(mesa,name)):nullptr);}
HGLRC WINAPI CreateAttribs(HDC guest,HGLRC shared,const int* attrs){auto dc=CoreDc(guest);if(!dc)return nullptr;{std::lock_guard<std::recursive_mutex> lock(gate);if(shared&&!contexts.count(shared)){SetLastError(ERROR_INVALID_HANDLE);return nullptr;}}auto call=reinterpret_cast<HGLRC(WINAPI*)(HDC,HGLRC,const int*)>(DriverProc("wglCreateContextAttribsARB"));if(!call){SetLastError(ERROR_PROC_NOT_FOUND);return nullptr;}auto context=call(dc,shared,attrs);if(context){std::lock_guard<std::recursive_mutex> lock(gate);contexts.insert(context);}return context;}
BOOL WINAPI ChooseArb(HDC guest,const int* a,const float* b,UINT count,int* formats,UINT* found){auto dc=CoreDc(guest);if(!dc)return FALSE;auto call=reinterpret_cast<BOOL(WINAPI*)(HDC,const int*,const float*,UINT,int*,UINT*)>(DriverProc("wglChoosePixelFormatARB"));return call?call(dc,a,b,count,formats,found):FALSE;}
BOOL WINAPI AttributesInt(HDC guest,int format,int layer,UINT count,const int* names,int* values){auto dc=CoreDc(guest);if(!dc)return FALSE;auto call=reinterpret_cast<BOOL(WINAPI*)(HDC,int,int,UINT,const int*,int*)>(DriverProc("wglGetPixelFormatAttribivARB"));return call?call(dc,format,layer,count,names,values):FALSE;}
BOOL WINAPI AttributesFloat(HDC guest,int format,int layer,UINT count,const int* names,float* values){auto dc=CoreDc(guest);if(!dc)return FALSE;auto call=reinterpret_cast<BOOL(WINAPI*)(HDC,int,int,UINT,const int*,float*)>(DriverProc("wglGetPixelFormatAttribfvARB"));return call?call(dc,format,layer,count,names,values):FALSE;}
const char* WINAPI Extensions(HDC guest){auto dc=CoreDc(guest);if(!dc)return nullptr;auto call=reinterpret_cast<const char*(WINAPI*)(HDC)>(DriverProc("wglGetExtensionsStringARB"));return call?call(dc):nullptr;}
BOOL WINAPI Share(HGLRC first,HGLRC second){if(!Ready())return FALSE;std::lock_guard<std::recursive_mutex> lock(gate);if(!contexts.count(first)||!contexts.count(second)){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}auto call=Api<BOOL(WINAPI*)(HGLRC,HGLRC)>("wglShareLists");return call?call(first,second):FALSE;}
PROC WINAPI Procedure(LPCSTR name){if(!name||!name[0]||strlen(name)>160||!Ready()){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}
#define EXT(n,f) if(!strcmp(name,n))return reinterpret_cast<PROC>(&f)
 EXT("wglCreateContextAttribsARB",CreateAttribs);EXT("wglChoosePixelFormatARB",ChooseArb);EXT("wglGetPixelFormatAttribivARB",AttributesInt);EXT("wglGetPixelFormatAttribfvARB",AttributesFloat);EXT("wglGetExtensionsStringARB",Extensions);EXT("wglShareLists",Share);
#undef EXT
 // Do not leak a driver's HDC-taking extension across the native/CoreWindow DC
 // boundary. Explicitly supported no-HDC extensions can use the real driver.
 if(!strncmp(name,"wgl",3)&&strcmp(name,"wglSwapIntervalEXT")&&strcmp(name,"wglGetSwapIntervalEXT")&&strcmp(name,"wglGetExtensionsStringEXT")){SetLastError(ERROR_NOT_SUPPORTED);return nullptr;}return DriverProc(name);
}
int WINAPI Choose(HDC guest,const PIXELFORMATDESCRIPTOR* value){auto dc=CoreDc(guest);auto call=Api<int(WINAPI*)(HDC,const PIXELFORMATDESCRIPTOR*)>("wglChoosePixelFormat");return dc&&call?call(dc,value):0;}
int WINAPI Describe(HDC guest,int index,UINT size,PIXELFORMATDESCRIPTOR* value){auto dc=CoreDc(guest);auto call=Api<int(WINAPI*)(HDC,int,UINT,PIXELFORMATDESCRIPTOR*)>("wglDescribePixelFormat");return dc&&call?call(dc,index,size,value):0;}
int WINAPI PixelFormat(HDC guest){auto dc=CoreDc(guest);auto call=Api<int(WINAPI*)(HDC)>("wglGetPixelFormat");return dc&&call?call(dc):0;}
BOOL WINAPI SetFormat(HDC guest,int index,const PIXELFORMATDESCRIPTOR* value){auto dc=CoreDc(guest);auto call=Api<BOOL(WINAPI*)(HDC,int,const PIXELFORMATDESCRIPTOR*)>("wglSetPixelFormat");return dc&&call?call(dc,index,value):FALSE;}
BOOL WINAPI Swap(HDC guest){auto dc=CoreDc(guest);auto call=Api<BOOL(WINAPI*)(HDC)>("wglSwapBuffers");return dc&&call?call(dc):FALSE;}
}
void ConfigureWglProvider(IUnknown* value,BOOL(__cdecl* size)(LONG*,LONG*)){std::lock_guard<std::recursive_mutex> lock(gate);window=value;viewport=size;}
void ConfigureWglProviderLogger(void(*value)(const char*,DWORD)){diagnostic.store(value);}
void* ResolveWglProviderImport(const char* dll,const char* name,bool requireCandidate){if(!dll||!name)return nullptr;bool gl=!_stricmp(dll,"opengl32.dll"),gdi=!_stricmp(dll,"gdi32.dll");if(!gl&&!gdi)return nullptr;
 // Do not replace a native Windows OpenGL stack with the candidate unless an
 // owned functional test explicitly requests the candidate implementation.
 if(!requireCandidate){using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);auto load=Sys<Load>("kernel32.dll","LoadLibraryExW");static HMODULE original=load?load(L"opengl32.dll",nullptr,0x800):nullptr;if(original)return nullptr;}
 if(!Ready())return nullptr;
#define GL(n,f) if(!strcmp(name,n))return reinterpret_cast<void*>(&f)
 if(gl){GL("wglCreateContext",Create);GL("wglDeleteContext",Delete);GL("wglMakeCurrent",MakeCurrent);GL("wglGetCurrentContext",Current);GL("wglGetCurrentDC",CurrentDc);GL("wglGetProcAddress",Procedure);}else{GL("ChoosePixelFormat",Choose);GL("DescribePixelFormat",Describe);GL("GetPixelFormat",PixelFormat);GL("SetPixelFormat",SetFormat);GL("SwapBuffers",Swap);}
#undef GL
 return nullptr;
}
