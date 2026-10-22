// Original restricted namespace for the native OS-loader route, not manual mapping.
#include "PackagedNamespace.h"
#include "../../runtime-native-route/NativeProxyDispatch.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <string>
#include <map>
#include <mutex>
#include <cstring>
#include <cwchar>
#include <algorithm>
#include <vector>
namespace {
std::recursive_mutex gate;
Hollowwake::PackagedNamespaceLogger namespaceLogger=nullptr;unsigned missingExports=0;
void MissingExport(const std::string& module,LPCSTR name,DWORD error){
 if(!namespaceLogger||missingExports>=32||!name||strnlen_s(name,221)>220)return;std::string safe;for(auto p=name;*p;++p){unsigned char c=*p;if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||strchr("_?@$#.:~+<>-",c)))return;safe+=static_cast<char>(c);}
 if(!safe.empty()){++missingExports;namespaceLogger("NATIVE_DYNAMIC_IMPORT_MISSING "+module+"!"+safe+" Win32="+std::to_string(error));}
}
std::wstring mainName,originalMainPath;std::string originalMainBase;std::wstring command;std::string commandA;
struct Facade {std::string name;};std::map<std::string,Facade> facades;
std::map<std::string,std::wstring> packaged;
std::wstring packageRoot,packageRootDisplay;std::map<std::wstring,std::string> pathAliases;
bool Normalize(LPCWSTR value,std::string& name){
 if(!value)return false;
 for(unsigned i=0;i<256;++i){wchar_t c=value[i];if(!c){if(name.empty())return false;if(name.find('.')==std::string::npos)name+=".dll";return true;}
  if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'-'||c==L'_'))return false;
  name+=static_cast<char>(c>=L'A'&&c<=L'Z'?c+32:c);
 }return false;
}
std::wstring Canonical(const wchar_t* value){if(!value||wcsnlen_s(value,32768)==32768)return {};std::wstring path(value);if(path.compare(0,4,L"\\\\?\\")==0)path=path.substr(4);for(auto& c:path){if(c==L'/')c=L'\\';else if(c>=L'A'&&c<=L'Z')c+=32;}return path;}
bool NormalizeKnown(LPCWSTR value,std::string& key){if(Normalize(value,key))return true;key.clear();auto path=Canonical(value);auto found=pathAliases.find(path);if(found==pathAliases.end())return false;key=found->second;return true;}
std::wstring Wide(LPCSTR text){if(!text)return {};std::wstring out;for(unsigned i=0;i<256;++i){unsigned char c=text[i];if(!c)return out;if(c>127)return {};out+=c;}return {};}
HMODULE Main(){if(mainName.empty())return nullptr;using Get=HMODULE(WINAPI*)(LPCWSTR);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleW"));return get?get(mainName.c_str()):nullptr;}
HMODULE NativeModule(const std::string& name){using Get=HMODULE(WINAPI*)(LPCWSTR);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleW"));std::wstring wide(name.begin(),name.end());auto alias=packaged.find(name);if(alias!=packaged.end()){auto slash=alias->second.find_last_of(L"/\\");wide=alias->second.substr(slash==std::wstring::npos?0:slash+1);}return get?get(wide.c_str()):nullptr;}
bool Known(HMODULE handle){if(!handle)return false;if(handle==Main())return true;for(auto& item:packaged)if(handle==NativeModule(item.first))return true;return false;}
Facade* Virtual(HMODULE handle){for(auto& entry:facades)if(reinterpret_cast<HMODULE>(&entry.second)==handle)return &entry.second;return nullptr;}
HMODULE WINAPI ModuleW(LPCWSTR name){std::lock_guard<std::recursive_mutex> lock(gate);if(!name)return Main();std::string key;if(!NormalizeKnown(name,key)){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}
 std::string main;if(Normalize(mainName.c_str(),main)&&(main==key||originalMainBase==key))return Main();
 if(packaged.count(key))return NativeModule(key);
 if(!Hollowwake::NativeSystemFacadeAvailable(key.c_str())){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}
 auto& entry=facades[key];entry.name=key;return reinterpret_cast<HMODULE>(&entry);
}
HMODULE WINAPI ModuleA(LPCSTR name){if(!name)return ModuleW(nullptr);auto text=Wide(name);if(text.empty()){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}return ModuleW(text.c_str());}
BOOL WINAPI ModuleExW(DWORD flags,LPCWSTR name,HMODULE* output){std::lock_guard<std::recursive_mutex> lock(gate);if(!output){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}*output=nullptr;if(flags&~7||(flags&1&&flags&2)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 HMODULE h=nullptr;if(flags&4){using Get=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));if(!get||!get(6,name,&h)||!Known(h)){SetLastError(ERROR_MOD_NOT_FOUND);return FALSE;}}
 else h=ModuleW(name);
 if(!h)return FALSE;
 if(!Virtual(h)&&!(flags&2)){using Get=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));if(!get||!get((flags&1)|4,reinterpret_cast<LPCWSTR>(h),&h))return FALSE;}
 *output=h;return TRUE;
}
BOOL WINAPI ModuleExA(DWORD flags,LPCSTR name,HMODULE* output){if(flags&4)return ModuleExW(flags,reinterpret_cast<LPCWSTR>(name),output);if(!name)return ModuleExW(flags,nullptr,output);auto text=Wide(name);if(text.empty()){if(output)*output=nullptr;SetLastError(ERROR_MOD_NOT_FOUND);return FALSE;}return ModuleExW(flags,text.c_str(),output);}
HMODULE WINAPI LoadW(LPCWSTR name,HANDLE file,DWORD flags){std::lock_guard<std::recursive_mutex> lock(gate);if(!name||file||flags&~0x1a00){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}std::string key;if(!NormalizeKnown(name,key)){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}auto item=packaged.find(key);if(item!=packaged.end()){if(flags&0x800){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}return LoadPackagedLibrary(item->second.c_str(),0);}HMODULE h=ModuleW(name);if(!h)return nullptr;
 if(Virtual(h))return h;
 // This phase permits already-loaded main identity only. Arbitrary dependencies
 // cannot be loaded from LocalState or pulled into the package implicitly.
 if(flags&0x800){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}HMODULE held=nullptr;return ModuleExW(0,mainName.c_str(),&held)?held:nullptr;
}
HMODULE WINAPI LoadA(LPCSTR name,HANDLE file,DWORD flags){auto text=Wide(name);if(text.empty()){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}return LoadW(text.c_str(),file,flags);}
HMODULE WINAPI LoadSimpleW(LPCWSTR name){return LoadW(name,nullptr,0);}HMODULE WINAPI LoadSimpleA(LPCSTR name){return LoadA(name,nullptr,0);}
FARPROC WINAPI Proc(HMODULE h,LPCSTR name){std::lock_guard<std::recursive_mutex> lock(gate);if(!name){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}auto value=reinterpret_cast<ULONG_PTR>(name);std::string ordinal;if(value<=65535){ordinal="#"+std::to_string(value);name=ordinal.c_str();}
 if(auto module=Virtual(h)){auto p=HWResolvePackagedImport(module->name.c_str(),name);if(!p){MissingExport(module->name,name,ERROR_PROC_NOT_FOUND);SetLastError(ERROR_PROC_NOT_FOUND);}return reinterpret_cast<FARPROC>(p);}
 if(Known(h)){auto p=::GetProcAddress(h,value<=65535?reinterpret_cast<LPCSTR>(value):name);auto error=GetLastError();if(!p){std::string module="main.exe";for(auto& item:packaged)if(h==NativeModule(item.first)){module=item.first;break;}MissingExport(module,name,error);}SetLastError(error);return p;}
 SetLastError(ERROR_INVALID_HANDLE);return nullptr;
}
BOOL WINAPI Release(HMODULE h){std::lock_guard<std::recursive_mutex> lock(gate);if(Virtual(h))return TRUE;
 // Main remains owned by the explicit launcher; guest unload requests cannot
 // unmap executing code. This is a pinned-module policy, not a successful unload.
 if(h!=Main()&&Known(h))return ::FreeLibrary(h);SetLastError(h==Main()?ERROR_ACCESS_DENIED:ERROR_INVALID_HANDLE);return FALSE;
}
DWORD WINAPI FilenameW(HMODULE h,LPWSTR output,DWORD capacity){
 std::lock_guard<std::recursive_mutex> lock(gate);if(!h)h=Main();if(!h||!Known(h)){SetLastError(ERROR_INVALID_HANDLE);return 0;}
 if(h==Main()&&!originalMainPath.empty()){
  if(!capacity){SetLastError(ERROR_INSUFFICIENT_BUFFER);return 0;}if(!output){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
  auto count=(std::min)(originalMainPath.size(),static_cast<size_t>(capacity-1));std::wmemcpy(output,originalMainPath.data(),count);output[count]=0;
  if(originalMainPath.size()>=capacity){SetLastError(ERROR_INSUFFICIENT_BUFFER);return capacity;}return static_cast<DWORD>(count);
 }
 using Get=DWORD(WINAPI*)(HMODULE,LPWSTR,DWORD);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleFileNameW"));return get?get(h,output,capacity):0;
}
DWORD WINAPI FilenameA(HMODULE h,LPSTR output,DWORD capacity){
 std::lock_guard<std::recursive_mutex> lock(gate);if(!h)h=Main();if(!h||!Known(h)){SetLastError(ERROR_INVALID_HANDLE);return 0;}
 if(h==Main()&&!originalMainPath.empty()){
  if(!capacity){SetLastError(ERROR_INSUFFICIENT_BUFFER);return 0;}if(!output){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
  int required=WideCharToMultiByte(CP_ACP,0,originalMainPath.c_str(),-1,nullptr,0,nullptr,nullptr);if(!required)return 0;std::vector<char> bytes(required);if(!WideCharToMultiByte(CP_ACP,0,originalMainPath.c_str(),-1,bytes.data(),required,nullptr,nullptr))return 0;
  auto count=(std::min)(static_cast<size_t>(required-1),static_cast<size_t>(capacity-1));memcpy(output,bytes.data(),count);output[count]=0;
  if(static_cast<DWORD>(required)>capacity){SetLastError(ERROR_INSUFFICIENT_BUFFER);return capacity;}return required-1;
 }
 using Get=DWORD(WINAPI*)(HMODULE,LPSTR,DWORD);auto get=reinterpret_cast<Get>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleFileNameA"));return get?get(h,output,capacity):0;
}
LPWSTR WINAPI CommandW(){return command.empty()?nullptr:&command[0];}LPSTR WINAPI CommandA(){return commandA.empty()?nullptr:&commandA[0];}
}
namespace Hollowwake {
void ConfigurePackagedNamespaceLogger(PackagedNamespaceLogger logger){std::lock_guard<std::recursive_mutex> lock(gate);namespaceLogger=logger;}
char* __cdecl PackagedWinMainTail(){std::lock_guard<std::recursive_mutex> lock(gate);if(commandA.empty())return nullptr;size_t i=0;bool quoted=false;while(i<commandA.size()){char c=commandA[i];if(c=='"')quoted=!quoted;else if(!quoted&&(c==' '||c=='\t'))break;++i;}while(i<commandA.size()&&(commandA[i]==' '||commandA[i]=='\t'))++i;return &commandA[0]+i;}
bool ConfigurePackagedRoot(const wchar_t* root){std::lock_guard<std::recursive_mutex> lock(gate);auto path=Canonical(root);if(path.empty()||(!packageRoot.empty()&&packageRoot!=path))return false;if(packageRoot.empty())packageRootDisplay=root;packageRoot=path;return true;}
bool RegisterPackagedDependency(const wchar_t* module,const wchar_t* relative,const wchar_t* original){std::lock_guard<std::recursive_mutex> lock(gate);std::string key;if(!Normalize(module,key)||!relative||!relative[0]||wcsnlen_s(relative,512)==512||NativeSystemFacadeAvailable(key.c_str()))return false;std::wstring path(relative);if(path[0]==L'/'||path[0]==L'\\'||path.find(L':')!=std::wstring::npos||path.find(L"..")!=std::wstring::npos)return false;for(auto c:path)if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'-'||c==L'_'||c==L'/'||c==L'\\'))return false;if(packageRoot.empty()||!original||wcsnlen_s(original,512)==512)return false;auto origin=Canonical(original);if(origin.empty()||origin[0]==L'\\'||origin.find(L':')!=std::wstring::npos||origin.find(L"..")!=std::wstring::npos)return false;pathAliases[packageRoot+L"\\"+origin]=key;auto old=packaged.find(key);if(old!=packaged.end())return old->second==path;packaged[key]=path;return true;}
bool RegisterPackagedMainIdentity(const wchar_t* original){
 std::lock_guard<std::recursive_mutex> lock(gate);if(!original||wcsnlen_s(original,512)==512||packageRoot.empty()||mainName.empty())return false;
 std::wstring relative(original);for(auto& c:relative)if(c==L'/')c=L'\\';if(relative.empty()||relative[0]==L'\\'||relative.find(L':')!=std::wstring::npos||relative.find(L"..")!=std::wstring::npos)return false;
 for(auto c:relative)if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'-'||c==L'_'||c==L'\\'))return false;
 auto slash=relative.find_last_of(L'\\');auto base=relative.substr(slash==std::wstring::npos?0:slash+1);std::string key,physical;if(!Normalize(base.c_str(),key)||key.size()<5||key.substr(key.size()-4)!=".exe"||!Normalize(mainName.c_str(),physical))return false;
 auto path=packageRootDisplay+L"\\"+relative;if(!originalMainPath.empty())return originalMainPath==path;
 originalMainPath=path;originalMainBase=key;pathAliases[Canonical(path.c_str())]=physical;pathAliases[Canonical(relative.c_str())]=physical;return true;
}
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
