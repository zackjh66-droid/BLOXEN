// Original restricted namespace for the native OS-loader route, not manual mapping.
#include "PackagedNamespace.h"
#include "NativeProxyDispatch.h"
#include "NativeSystem.h"
#include <string>
#include <map>
#include <mutex>
#include <cstring>
#include <cwchar>
#include <algorithm>
namespace {
std::recursive_mutex gate;
std::wstring mainName,command;std::string commandA;
struct Facade {std::string name;};std::map<std::string,Facade> facades;
bool Normalize(LPCWSTR value,std::string& name){
 if(!value)return false;
 for(unsigned i=0;i<256;++i){wchar_t c=value[i];if(!c){if(name.empty())return false;if(name.find('.')==std::string::npos)name+=".dll";return true;}
  if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'-'||c==L'_'))return false;
  name+=static_cast<char>(c>=L'A'&&c<=L'Z'?c+32:c);
 }return false;
}
std::wstring Wide(LPCSTR text){if(!text)return {};std::wstring out;for(unsigned i=0;i<256;++i){unsigned char c=text[i];if(!c)return out;if(c>127)return {};out+=c;}return {};}
HMODULE Main(){if(mainName.empty())return nullptr;using Get=HMODULE(WINAPI*)(LPCWSTR);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleW"));return get?get(mainName.c_str()):nullptr;}
Facade* Virtual(HMODULE handle){for(auto& entry:facades)if(reinterpret_cast<HMODULE>(&entry.second)==handle)return &entry.second;return nullptr;}
HMODULE WINAPI ModuleW(LPCWSTR name){std::lock_guard<std::recursive_mutex> lock(gate);if(!name)return Main();std::string key;if(!Normalize(name,key)){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}
 std::string main;if(Normalize(mainName.c_str(),main)&&main==key)return Main();
 if(!Hollowwake::NativeSystemFacadeAvailable(key.c_str())){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}
 auto& entry=facades[key];entry.name=key;return reinterpret_cast<HMODULE>(&entry);
}
HMODULE WINAPI ModuleA(LPCSTR name){if(!name)return ModuleW(nullptr);auto text=Wide(name);return text.empty()?nullptr:ModuleW(text.c_str());}
BOOL WINAPI ModuleExW(DWORD flags,LPCWSTR name,HMODULE* output){std::lock_guard<std::recursive_mutex> lock(gate);if(!output){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}*output=nullptr;if(flags&~7||(flags&1&&flags&2)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 HMODULE h=nullptr;if(flags&4){using Get=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));if(!get||!get(6,name,&h)||h!=Main()){SetLastError(ERROR_MOD_NOT_FOUND);return FALSE;}}
 else h=ModuleW(name);
 if(!h)return FALSE;
 if(!Virtual(h)&&!(flags&2)){using Get=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));if(!get||!get((flags&1)|4,reinterpret_cast<LPCWSTR>(h),&h))return FALSE;}
 *output=h;return TRUE;
}
BOOL WINAPI ModuleExA(DWORD flags,LPCSTR name,HMODULE* output){if(flags&4)return ModuleExW(flags,reinterpret_cast<LPCWSTR>(name),output);if(!name)return ModuleExW(flags,nullptr,output);auto text=Wide(name);if(text.empty()){if(output)*output=nullptr;SetLastError(ERROR_MOD_NOT_FOUND);return FALSE;}return ModuleExW(flags,text.c_str(),output);}
HMODULE WINAPI LoadW(LPCWSTR name,HANDLE file,DWORD flags){if(!name||file||flags&~0x1a00){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}HMODULE h=ModuleW(name);if(!h)return nullptr;
 if(Virtual(h))return h;
 // This phase permits already-loaded main identity only. Arbitrary dependencies
 // cannot be loaded from LocalState or pulled into the package implicitly.
 if(flags&0x800){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}HMODULE held=nullptr;return ModuleExW(0,mainName.c_str(),&held)?held:nullptr;
}
HMODULE WINAPI LoadA(LPCSTR name,HANDLE file,DWORD flags){auto text=Wide(name);return text.empty()?nullptr:LoadW(text.c_str(),file,flags);}
HMODULE WINAPI LoadSimpleW(LPCWSTR name){return LoadW(name,nullptr,0);}HMODULE WINAPI LoadSimpleA(LPCSTR name){return LoadA(name,nullptr,0);}
FARPROC WINAPI Proc(HMODULE h,LPCSTR name){std::lock_guard<std::recursive_mutex> lock(gate);if(!name){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}auto value=reinterpret_cast<ULONG_PTR>(name);std::string ordinal;if(value<=65535){ordinal="#"+std::to_string(value);name=ordinal.c_str();}
 if(auto module=Virtual(h)){auto p=HWResolvePackagedImport(module->name.c_str(),name);if(!p)SetLastError(ERROR_PROC_NOT_FOUND);return reinterpret_cast<FARPROC>(p);}
 if(h&&h==Main())return ::GetProcAddress(h,value<=65535?reinterpret_cast<LPCSTR>(value):name);
 SetLastError(ERROR_INVALID_HANDLE);return nullptr;
}
BOOL WINAPI Release(HMODULE h){std::lock_guard<std::recursive_mutex> lock(gate);if(Virtual(h))return TRUE;
 // Main remains owned by the explicit launcher; guest unload requests cannot
 // unmap executing code. This is a pinned-module policy, not a successful unload.
 SetLastError(h==Main()?ERROR_ACCESS_DENIED:ERROR_INVALID_HANDLE);return FALSE;
}
DWORD WINAPI FilenameW(HMODULE h,LPWSTR output,DWORD capacity){std::lock_guard<std::recursive_mutex> lock(gate);if(!h)h=Main();if(!h||h!=Main()){SetLastError(ERROR_INVALID_HANDLE);return 0;}using Get=DWORD(WINAPI*)(HMODULE,LPWSTR,DWORD);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleFileNameW"));return get?get(h,output,capacity):0;}
DWORD WINAPI FilenameA(HMODULE h,LPSTR output,DWORD capacity){std::lock_guard<std::recursive_mutex> lock(gate);if(!h)h=Main();if(!h||h!=Main()){SetLastError(ERROR_INVALID_HANDLE);return 0;}using Get=DWORD(WINAPI*)(HMODULE,LPSTR,DWORD);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleFileNameA"));return get?get(h,output,capacity):0;}
LPWSTR WINAPI CommandW(){return command.empty()?nullptr:&command[0];}LPSTR WINAPI CommandA(){return commandA.empty()?nullptr:&commandA[0];}
}
namespace Hollowwake {
bool ConfigurePackagedNamespace(const wchar_t* name,const wchar_t* line){std::lock_guard<std::recursive_mutex> lock(gate);std::string normalized;if(!Normalize(name,normalized)||!line||wcsnlen_s(line,32768)==32768)return false;
 // Stable command pointers are retained for process lifetime; reconfiguration
 // after a first successful setup would invalidate CRT-held pointers.
 if(!mainName.empty())return false;std::string narrow;for(auto p=line;*p;++p){if(*p>127)return false;narrow+=static_cast<char>(*p);}mainName=name;command=line;commandA=narrow;return true;
}
void* ResolvePackagedNamespaceImport(const char* dll,const char* name){if(!dll||!name||(_stricmp(dll,"kernel32.dll")&&_stricmp(dll,"kernelbase.dll")&&_strnicmp(dll,"api-ms-win-core-",16)))return nullptr;
#define API(n,f) if(!strcmp(name,n))return reinterpret_cast<void*>(&f)
 API("GetModuleHandleW",ModuleW);API("GetModuleHandleA",ModuleA);API("GetModuleHandleExW",ModuleExW);API("GetModuleHandleExA",ModuleExA);API("LoadLibraryExW",LoadW);API("LoadLibraryExA",LoadA);API("LoadLibraryW",LoadSimpleW);API("LoadLibraryA",LoadSimpleA);API("GetProcAddress",Proc);API("FreeLibrary",Release);API("GetModuleFileNameW",FilenameW);API("GetModuleFileNameA",FilenameA);API("GetCommandLineW",CommandW);API("GetCommandLineA",CommandA);
#undef API
 return nullptr;}
}
