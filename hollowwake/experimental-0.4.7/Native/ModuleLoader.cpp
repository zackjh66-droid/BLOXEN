#include "ModuleLoader.h"
#include <map>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <limits>
namespace {
// Documented Win32 guest ABI flag values; desktop SDK names are hidden in UWP.
constexpr DWORD SearchSystem32=0x800,SearchDefaultDirs=0x1000,SearchApplicationDir=0x200;
constexpr DWORD HandlePin=1,HandleUnchangedRefcount=2,HandleFromAddress=4;
struct Module {std::string name;std::wstring path;HMODULE handle=nullptr;SIZE_T size=0;unsigned refs=0;bool facade=false,pinned=false;};
std::recursive_mutex mutex;
std::map<std::string,Module> modules;
HMODULE mainImage=nullptr;
void*(*lookup)(const char*,const char*)=nullptr;
void(*logger)(const std::string&)=nullptr;
// These are capability facades, not native Windows HMODULEs. Only public API
// bridges from the same guest resolver can be obtained from them.
const char* facades[]={"kernel32.dll","kernelbase.dll","api-ms-win-core-fibers-l1-1-1.dll","api-ms-win-core-synch-l1-2-0.dll"};
bool Normalize(LPCWSTR value,std::string& name){
 if(!value){SetLastError(ERROR_INVALID_PARAMETER);return false;}
 for(unsigned i=0;i<256;++i){auto c=value[i];if(!c){if(name.empty()){SetLastError(ERROR_INVALID_PARAMETER);return false;}if(name.find('.')==std::string::npos)name+=".dll";return true;}
  if(c>127||!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')){SetLastError(ERROR_MOD_NOT_FOUND);return false;}
  name+=static_cast<char>(c>='A'&&c<='Z'?c+32:c);
 }SetLastError(ERROR_FILENAME_EXCED_RANGE);return false;
}
Module* ByHandle(HMODULE handle){for(auto& p:modules)if(p.second.handle==handle)return &p.second;return nullptr;}
Module* Remember(const GuestModuleRecord& record,const std::string& name){
 auto& m=modules[name];m.name=name;m.path=record.path;m.handle=record.handle;m.size=record.size;m.pinned=true;return &m;
}
Module* Find(const std::string& name,bool load){
 auto i=modules.find(name);if(i!=modules.end())return &i->second;
 for(auto facade:facades)if(name==facade){if(!load){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}auto& m=modules[name];m.name=name;m.facade=true;m.pinned=true;m.handle=reinterpret_cast<HMODULE>(&m);return &m;}
 GuestModuleRecord record;if(FindGuestDependency(name,record)){
  if(load&&!record.initialized){if(logger)logger("MODULE load refused: mapped dependency has not completed initialization: "+name);SetLastError(ERROR_DLL_INIT_FAILED);return nullptr;}
  return Remember(record,name);
 }
 // Mapping a new dependency while another initializer holds guest state needs
 // full reentrant loader ordering. Do not fabricate it or load unpinned files.
 if(logger)logger("MODULE unavailable in restricted guest namespace: "+name);
 SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;
}
bool AddRef(Module& m){if(m.refs==std::numeric_limits<unsigned>::max()){SetLastError(ERROR_TOO_MANY_MODULES);return false;}++m.refs;return true;}
HMODULE WINAPI GuestLoadLibraryExW(LPCWSTR name,HANDLE file,DWORD flags){
 std::lock_guard<std::recursive_mutex> guard(mutex);
 constexpr DWORD supported=SearchSystem32|SearchDefaultDirs|SearchApplicationDir;
 if(file||flags&~supported){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}
 std::string key;if(!Normalize(name,key))return nullptr;
 auto module=Find(key,true);if(!module)return nullptr;
 if((flags&SearchSystem32)&&!module->facade){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}
 if(!AddRef(*module))return nullptr;
 if(logger)logger("MODULE acquired "+key+(module->facade?" (public-API facade)":" (initialized hash-pinned guest image)"));return module->handle;
}
std::wstring Widen(LPCSTR name){if(!name)return {};size_t count=strnlen_s(name,256);if(count==256)return {};std::wstring wide;for(size_t i=0;i<count;++i){unsigned char c=name[i];if(c>127)return {};wide+=c;}return wide;}
HMODULE WINAPI GuestLoadLibraryExA(LPCSTR name,HANDLE file,DWORD flags){auto wide=Widen(name);if(wide.empty()){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}return GuestLoadLibraryExW(wide.c_str(),file,flags);}
HMODULE WINAPI GuestLoadLibraryW(LPCWSTR name){return GuestLoadLibraryExW(name,nullptr,0);}
HMODULE WINAPI GuestLoadLibraryA(LPCSTR name){return GuestLoadLibraryExA(name,nullptr,0);}
HMODULE WINAPI GuestGetModuleHandleW(LPCWSTR name){std::lock_guard<std::recursive_mutex> guard(mutex);if(!name){if(!mainImage)SetLastError(ERROR_MOD_NOT_FOUND);return mainImage;}std::string key;if(!Normalize(name,key))return nullptr;auto m=Find(key,false);return m?m->handle:nullptr;}
HMODULE WINAPI GuestGetModuleHandleA(LPCSTR name){if(!name)return GuestGetModuleHandleW(nullptr);auto wide=Widen(name);if(wide.empty()){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}return GuestGetModuleHandleW(wide.c_str());}
BOOL WINAPI GuestGetModuleHandleExW(DWORD flags,LPCWSTR name,HMODULE* output){
 std::lock_guard<std::recursive_mutex> guard(mutex);if(!output){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}*output=nullptr;
 if(flags&~(HandlePin|HandleUnchangedRefcount|HandleFromAddress)||(flags&HandlePin&&flags&HandleUnchangedRefcount)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 HMODULE h=nullptr;
 if(flags&HandleFromAddress){auto address=reinterpret_cast<ULONG_PTR>(name);for(auto& p:modules){auto& m=p.second;auto begin=reinterpret_cast<ULONG_PTR>(m.handle);if(!m.facade&&address>=begin&&address-begin<m.size){h=m.handle;break;}}
  if(!h){GuestModuleRecord record;if(FindGuestDependencyAddress(address,record)){std::string key;auto slash=record.path.find_last_of(L"/\\");auto base=record.path.substr(slash==std::wstring::npos?0:slash+1);if(Normalize(base.c_str(),key)){h=Remember(record,key)->handle;}}}
 }else h=GuestGetModuleHandleW(name);
 auto m=ByHandle(h);if(!h||!m){SetLastError(ERROR_MOD_NOT_FOUND);return FALSE;}
 if(!(flags&HandleUnchangedRefcount)&&!AddRef(*m))return FALSE;
 if(flags&HandlePin)m->pinned=true;*output=h;return TRUE;
}
BOOL WINAPI GuestGetModuleHandleExA(DWORD flags,LPCSTR name,HMODULE* output){if(flags&HandleFromAddress)return GuestGetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(name),output);if(!name)return GuestGetModuleHandleExW(flags,nullptr,output);auto wide=Widen(name);if(wide.empty()){if(output)*output=nullptr;SetLastError(ERROR_MOD_NOT_FOUND);return FALSE;}return GuestGetModuleHandleExW(flags,wide.c_str(),output);}
FARPROC WINAPI GuestGetProcAddress(HMODULE handle,LPCSTR name){
 std::lock_guard<std::recursive_mutex> guard(mutex);auto m=ByHandle(handle);if(!m){SetLastError(ERROR_INVALID_HANDLE);return nullptr;}
 auto raw=reinterpret_cast<ULONG_PTR>(name);if(!raw){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}
 std::string ordinal;if(raw<=65535){ordinal="#"+std::to_string(raw);name=ordinal.c_str();}else if(strnlen_s(name,4096)==4096){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}
 // Dynamic optional queries must return NULL for unavailable functions. They
 // must never manufacture a non-null fail-fast thunk that UCRT will call.
 void* result=m->facade?(lookup?lookup(m->name.c_str(),name):nullptr):QueryDependencyExport(m->name.c_str(),name);
 if(!result){SetLastError(ERROR_PROC_NOT_FOUND);if(logger)logger("MODULE optional export absent "+m->name+"!"+name);}
 else if(logger)logger("MODULE export resolved "+m->name+"!"+name);
 return reinterpret_cast<FARPROC>(result);
}
BOOL WINAPI GuestFreeLibrary(HMODULE handle){std::lock_guard<std::recursive_mutex> guard(mutex);auto m=ByHandle(handle);if(!m||!m->refs){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}--m->refs;return TRUE;}
DWORD WINAPI GuestGetModuleFileNameW(HMODULE handle,LPWSTR output,DWORD capacity){std::lock_guard<std::recursive_mutex> guard(mutex);auto m=ByHandle(handle?handle:mainImage);if(!m||m->facade){SetLastError(m?ERROR_NOT_SUPPORTED:ERROR_INVALID_HANDLE);return 0;}if(!output||!capacity){SetLastError(ERROR_INSUFFICIENT_BUFFER);return 0;}auto length=m->path.size();size_t copy=(std::min)(length,static_cast<size_t>(capacity-1));std::wmemcpy(output,m->path.c_str(),copy);output[copy]=0;if(length>=capacity){SetLastError(ERROR_INSUFFICIENT_BUFFER);return capacity;}return static_cast<DWORD>(length);}
DWORD WINAPI GuestGetModuleFileNameA(HMODULE handle,LPSTR output,DWORD capacity){std::lock_guard<std::recursive_mutex> guard(mutex);auto m=ByHandle(handle?handle:mainImage);if(!m||m->facade){SetLastError(m?ERROR_NOT_SUPPORTED:ERROR_INVALID_HANDLE);return 0;}if(!output||!capacity){SetLastError(ERROR_INSUFFICIENT_BUFFER);return 0;}auto needed=WideCharToMultiByte(CP_ACP,0,m->path.c_str(),-1,nullptr,0,nullptr,nullptr);if(needed<=0)return 0;if(static_cast<DWORD>(needed)>capacity){output[0]=0;SetLastError(ERROR_INSUFFICIENT_BUFFER);return 0;}if(!WideCharToMultiByte(CP_ACP,0,m->path.c_str(),-1,output,capacity,nullptr,nullptr))return 0;return needed-1;}
}
void ConfigureGuestModules(void*(*query)(const char*,const char*),void(*log)(const std::string&)){std::lock_guard<std::recursive_mutex> guard(mutex);lookup=query;logger=log;for(auto name:facades){auto& m=modules[name];m.name=name;m.facade=true;m.pinned=true;m.handle=reinterpret_cast<HMODULE>(&m);}}
void RegisterGuestMainImage(void* base,SIZE_T size,const std::wstring& path){std::lock_guard<std::recursive_mutex> guard(mutex);auto slash=path.find_last_of(L"/\\");auto name=path.substr(slash==std::wstring::npos?0:slash+1);std::string key;if(!Normalize(name.c_str(),key))return;auto& m=modules[key];m.name=key;m.path=path;m.handle=static_cast<HMODULE>(base);m.size=size;m.pinned=true;mainImage=m.handle;}
void* ResolveModuleLoaderImport(const char* dll,const char* name){if(!dll||!name||(_stricmp(dll,"kernel32.dll")&&_stricmp(dll,"kernelbase.dll")&&_strnicmp(dll,"api-ms-win-core-libraryloader-",30)))return nullptr;
#define MODULE_API(n) if(!strcmp(name,#n))return reinterpret_cast<void*>(&Guest##n)
 MODULE_API(LoadLibraryExW);MODULE_API(LoadLibraryExA);MODULE_API(LoadLibraryW);MODULE_API(LoadLibraryA);MODULE_API(GetModuleHandleW);MODULE_API(GetModuleHandleA);MODULE_API(GetModuleHandleExW);MODULE_API(GetModuleHandleExA);MODULE_API(GetProcAddress);MODULE_API(FreeLibrary);MODULE_API(GetModuleFileNameW);MODULE_API(GetModuleFileNameA);
#undef MODULE_API
 return nullptr;
}
