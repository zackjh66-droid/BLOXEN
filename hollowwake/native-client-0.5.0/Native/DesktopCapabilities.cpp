#include "DesktopCapabilities.h"
#include "PlatformImports.generated.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <cstring>
#include <atomic>
#include <mutex>
#include <set>
#include <string>
namespace {
std::atomic<void(*)(const char*,const char*,DWORD)> logger{nullptr};std::mutex reportGate;std::set<std::string> reported;
struct Provider {HMODULE module=nullptr;DWORD error=0;explicit Provider(const wchar_t* name){using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);auto load=reinterpret_cast<Load>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","LoadLibraryExW"));if(load){SetLastError(0);module=load(name,nullptr,0x800);error=module?0:GetLastError();}else error=ERROR_PROC_NOT_FOUND;if(!module&&!error)error=ERROR_MOD_NOT_FOUND;}};
Provider* Module(const char* name){if(!name)return nullptr;if(!_stricmp(name,"imm32.dll")){static Provider p(L"imm32.dll");return &p;}if(!_stricmp(name,"opengl32.dll")){static Provider p(L"opengl32.dll");return &p;}if(!_stricmp(name,"dwmapi.dll")){static Provider p(L"dwmapi.dll");return &p;}if(!_stricmp(name,"d3d9.dll")){static Provider p(L"d3d9.dll");return &p;}return nullptr;}
void* Query(const char* dll,const char* name,DWORD& error){auto p=Module(dll);if(!p){error=ERROR_MOD_NOT_FOUND;return nullptr;}if(!p->module){error=p->error;return nullptr;}SetLastError(0);auto value=GetProcAddress(p->module,name);error=value?0:GetLastError();if(!value&&!error)error=ERROR_PROC_NOT_FOUND;return reinterpret_cast<void*>(value);}
void Unavailable(const char* dll,const char* name,DWORD error){bool first=false;{std::lock_guard<std::mutex> lock(reportGate);first=reported.emplace(std::string(dll)+"!"+name).second;}if(first)if(auto callback=logger.load())callback(dll,name,error);SetLastError(error);}
template<class T>T Native(const char* dll,const char* name){auto previous=GetLastError();DWORD error=0;auto p=Query(dll,name,error);if(!p){Unavailable(dll,name,error);return nullptr;}SetLastError(previous);return reinterpret_cast<T>(p);}
HANDLE WINAPI CreateIme(){auto p=Native<HANDLE(WINAPI*)()>("IMM32.dll","ImmCreateContext");return p?p():nullptr;}
BOOL WINAPI DestroyIme(HANDLE context){auto p=Native<BOOL(WINAPI*)(HANDLE)>("IMM32.dll","ImmDestroyContext");return p?p(context):FALSE;}
UINT WINAPI DescribeIme(HANDLE layout,LPWSTR buffer,UINT count){auto p=Native<UINT(WINAPI*)(HANDLE,LPWSTR,UINT)>("IMM32.dll","ImmGetDescriptionW");if(p)return p(layout,buffer,count);if(buffer&&count)buffer[0]=0;return 0;}
UINT WINAPI ImeFilename(HANDLE layout,LPWSTR buffer,UINT count){auto p=Native<UINT(WINAPI*)(HANDLE,LPWSTR,UINT)>("IMM32.dll","ImmGetIMEFileNameW");if(p)return p(layout,buffer,count);if(buffer&&count)buffer[0]=0;return 0;}
HANDLE WINAPI CreateGl(HDC dc){auto p=Native<HANDLE(WINAPI*)(HDC)>("OPENGL32.dll","wglCreateContext");return p?p(dc):nullptr;}
BOOL WINAPI DeleteGl(HANDLE context){auto p=Native<BOOL(WINAPI*)(HANDLE)>("OPENGL32.dll","wglDeleteContext");return p?p(context):FALSE;}
HANDLE WINAPI CurrentGl(){auto p=Native<HANDLE(WINAPI*)()>("OPENGL32.dll","wglGetCurrentContext");return p?p():nullptr;}
HDC WINAPI CurrentGlDc(){auto p=Native<HDC(WINAPI*)()>("OPENGL32.dll","wglGetCurrentDC");return p?p():nullptr;}
PROC WINAPI GlProc(LPCSTR name){auto p=Native<PROC(WINAPI*)(LPCSTR)>("OPENGL32.dll","wglGetProcAddress");return p?p(name):nullptr;}
BOOL WINAPI MakeGl(HDC dc,HANDLE context){auto p=Native<BOOL(WINAPI*)(HDC,HANDLE)>("OPENGL32.dll","wglMakeCurrent");return p?p(dc,context):FALSE;}
HRESULT WINAPI FlushDesktop(){auto p=Native<HRESULT(WINAPI*)()>("dwmapi.dll","DwmFlush");if(p)return p();return HRESULT_FROM_WIN32(GetLastError());}
HRESULT WINAPI CreateLegacyD3d(UINT sdk,void** output){auto p=Native<HRESULT(WINAPI*)(UINT,void**)>("d3d9.dll","Direct3DCreate9Ex");if(p)return p(sdk,output);if(!output)return static_cast<HRESULT>(0x8876086cL);*output=nullptr;return static_cast<HRESULT>(0x8876086aL); /* D3DERR_NOTAVAILABLE: no native D3D9 provider. */}
struct Adapter {const char* dll;const char* name;void* address;};
const Adapter adapters[]={
 {"imm32.dll","ImmCreateContext",reinterpret_cast<void*>(&CreateIme)},{"imm32.dll","ImmDestroyContext",reinterpret_cast<void*>(&DestroyIme)},{"imm32.dll","ImmGetDescriptionW",reinterpret_cast<void*>(&DescribeIme)},{"imm32.dll","ImmGetIMEFileNameW",reinterpret_cast<void*>(&ImeFilename)},
 {"opengl32.dll","wglCreateContext",reinterpret_cast<void*>(&CreateGl)},{"opengl32.dll","wglDeleteContext",reinterpret_cast<void*>(&DeleteGl)},{"opengl32.dll","wglGetCurrentContext",reinterpret_cast<void*>(&CurrentGl)},{"opengl32.dll","wglGetCurrentDC",reinterpret_cast<void*>(&CurrentGlDc)},{"opengl32.dll","wglGetProcAddress",reinterpret_cast<void*>(&GlProc)},{"opengl32.dll","wglMakeCurrent",reinterpret_cast<void*>(&MakeGl)},
 {"dwmapi.dll","DwmFlush",reinterpret_cast<void*>(&FlushDesktop)},{"d3d9.dll","Direct3DCreate9Ex",reinterpret_cast<void*>(&CreateLegacyD3d)}
};
}
void ConfigureDesktopCapabilityLogger(void(*value)(const char*,const char*,DWORD)){logger.store(value);}
bool IsDesktopCapabilityModule(const char* module){return Module(module)!=nullptr;}
bool IsDesktopCapabilityAdapter(const char* module,const char* symbol){if(!module||!symbol)return false;for(auto& a:adapters)if(!_stricmp(a.dll,module)&&!strcmp(a.name,symbol))return true;return false;}
bool NativeDesktopCapabilityAvailable(const char* module,const char* symbol,DWORD* error){DWORD value=0;auto address=module&&symbol?Query(module,symbol,value):nullptr;if(error)*error=module&&symbol?value:ERROR_INVALID_PARAMETER;return address!=nullptr;}
void* ResolveDesktopCapabilityImport(const char* module,const char* symbol){if(!module||!symbol)return nullptr;for(auto& a:adapters)if(!_stricmp(a.dll,module)&&!strcmp(a.name,symbol))return a.address;
 // Other imports in these families still require the actual OS export. Never
 // invent a successful association/context/window operation for them.
 bool known=false;for(auto& p:platformImportPairs)if(!_stricmp(p.dll,module)&&!strcmp(p.symbol,symbol)){known=true;break;}if(!known||!IsDesktopCapabilityModule(module))return nullptr;DWORD error=0;return Query(module,symbol,error);
}
