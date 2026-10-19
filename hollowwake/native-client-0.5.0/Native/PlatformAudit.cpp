#include "PlatformAudit.h"
#include "StartupPlatform.h"
#include "PlatformImports.generated.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <fstream>
#include <map>
#include <string>
#include <cstring>
namespace {
std::string Json(const char* text){std::string s="\"";for(auto p=text;*p;++p){unsigned char c=*p;if(c=='"'||c=='\\'){s+='\\';s+=(char)c;}else if(c<32){const char hex[]="0123456789abcdef";s+="\\u00";s+=hex[c>>4];s+=hex[c&15];}else s+=(char)c;}return s+'"';}
struct Loaded {HMODULE handle=nullptr;DWORD error=0;const char* method="none";};
FARPROC Symbol(HMODULE module,const char* name){if(name[0]!='#')return GetProcAddress(module,name);unsigned ordinal=0;for(auto p=name+1;*p;++p){if(*p<'0'||*p>'9'||ordinal>6553)return nullptr;ordinal=ordinal*10+*p-'0';}return ordinal&&ordinal<=65535?GetProcAddress(module,reinterpret_cast<LPCSTR>((ULONG_PTR)ordinal)):nullptr;}
}
unsigned AuditPlatformImports(const wchar_t* output){
 std::ofstream report(output,std::ios::trunc);if(!report){SetLastError(ERROR_WRITE_FAULT);return (unsigned)-1;}
 // The earlier package-only probe rejected even already loaded Windows DLLs.
 // Probe the actual system provider namespace: retain a loaded module or use
 // the real OS loader restricted to System32. Do not search GameClean, CWD,
 // caller paths, or the archive. OS/AppContainer denials remain failures.
 using Hold=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);
 using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);
 auto hold=reinterpret_cast<Hold>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));
 auto load=reinterpret_cast<Load>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","LoadLibraryExW"));
 std::map<std::string,Loaded> modules;unsigned missing=0,normalMissing=0,mainMissing=0,total=0,osMissing=0,adapterCount=0;
 report<<"{\"scope\":\"Fixed original platform inventory: OS exports and explicitly redirected Hollowwake adapters; not API behavior or Xbox availability\",\"gameExecuted\":false,\"pairs\":[";
 for(auto& pair:platformImportPairs){
  auto inserted=modules.emplace(pair.dll,Loaded{});auto& loaded=inserted.first->second;
  if(inserted.second){std::wstring name;bool valid=true;for(auto p=pair.dll;*p;++p){if((unsigned char)*p<33||*p=='/'||*p=='\\'||*p==':'){valid=false;break;}name+=(wchar_t)(unsigned char)*p;}
   if(valid){SetLastError(0);if(hold&&hold(0,name.c_str(),&loaded.handle))loaded.method="retained-loaded-provider";
    else if(load){SetLastError(0);loaded.handle=load(name.c_str(),nullptr,0x800);loaded.method="system32-os-loader";if(!loaded.handle)loaded.error=GetLastError();}
    else{loaded.error=ERROR_PROC_NOT_FOUND;loaded.method="os-loader-unavailable";}
   }else loaded.error=ERROR_INVALID_NAME;
  }
  SetLastError(0);auto address=loaded.handle?Symbol(loaded.handle,pair.symbol):nullptr;DWORD error=address?0:(loaded.handle?GetLastError():loaded.error);if(!address&&!error)error=ERROR_PROC_NOT_FOUND;
  bool osResolved=address!=nullptr;if(!osResolved)++osMissing;const char* method=loaded.method;
  if(IsStartupPlatformModule(pair.dll)){if(auto adapter=ResolveStartupPlatformAdapter(pair.dll,pair.symbol)){address=reinterpret_cast<FARPROC>(adapter);error=0;method="hollowwake-startup-adapter";++adapterCount;}}
  if(!address){++missing;if(pair.normal)++normalMissing;if(pair.mainImage)++mainMissing;}
  if(total++)report<<',';report<<"\n{\"name\":"<<Json((std::string(pair.dll)+"!"+pair.symbol).c_str())<<",\"resolved\":"<<(address?"true":"false")<<",\"error\":"<<error<<",\"providerMethod\":"<<Json(method)<<",\"osResolved\":"<<(osResolved?"true":"false")<<",\"adapterBound\":"<<(method==std::string("hollowwake-startup-adapter")?"true":"false")<<",\"normalImport\":"<<(pair.normal?"true":"false")<<",\"mainImage\":"<<(pair.mainImage?"true":"false")<<'}';
 }
 report<<"\n],\"total\":"<<total<<",\"missing\":"<<missing<<",\"normalMissing\":"<<normalMissing<<",\"mainMissing\":"<<mainMissing<<",\"osMissing\":"<<osMissing<<",\"adapterCount\":"<<adapterCount<<",\"dlls\":"<<modules.size()<<",\"xboxTested\":false}";report.flush();bool written=report.good();for(auto& module:modules)if(module.second.handle)FreeLibrary(module.second.handle);
 if(!written){SetLastError(ERROR_WRITE_FAULT);return (unsigned)-1;}return missing;
}
