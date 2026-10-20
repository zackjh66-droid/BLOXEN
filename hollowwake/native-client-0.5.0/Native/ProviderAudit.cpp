#include "ProviderAudit.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <windows.h>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <cstring>
#include <stdexcept>
#include <mutex>
namespace {
std::string Json(const std::string& value){std::string text="\"";for(unsigned char c:value){if(c=='"'||c=='\\'){text+='\\';text+=c;}else if(c<32)return "\"invalid\"";else text+=c;}return text+'"';}
bool Basename(const std::wstring& name){return !name.empty()&&name.size()<128&&name.find_first_of(L"/\\:")==std::wstring::npos&&name.find(L"..") == std::wstring::npos;}
std::string Ascii(Platform::String^ value){std::string result;for(wchar_t c:std::wstring(value->Data())){if(c<33||c>126)throw std::runtime_error("Non-ASCII provider inventory field");result+=static_cast<char>(c);}return result;}
FARPROC Resolve(HMODULE module,const std::string& symbol){if(symbol.empty())return nullptr;if(symbol[0]!='#')return GetProcAddress(module,symbol.c_str());unsigned n=0;for(auto c:symbol.substr(1)){if(c<'0'||c>'9'||n>6553)return nullptr;n=n*10+c-'0';}return n&&n<=65535?GetProcAddress(module,reinterpret_cast<LPCSTR>((ULONG_PTR)n)):nullptr;}
struct Held {HMODULE value=nullptr;DWORD error=0;};
// Loading a provider executes its real DllMain/TLS. Do not repeatedly unload
// callback/TLS-owning render/audio libraries between audit and first use.
// Held references intentionally live until process teardown (the OS owns DLL
// detach then). Microsoft/system and package basenames remain the only inputs.
std::mutex providersGate;std::map<std::string,Held> retainedProviders;
}
unsigned AuditAdditionalProviderImports(const wchar_t* inventory,const wchar_t* output){
 std::lock_guard<std::mutex> lifetime(providersGate);auto& modules=retainedProviders;
 try{
  std::ifstream source(inventory,std::ios::binary);if(!source){SetLastError(ERROR_FILE_NOT_FOUND);return (unsigned)-1;}std::string text((std::istreambuf_iterator<char>(source)),std::istreambuf_iterator<char>());if(text.size()>1024*1024)throw std::runtime_error("Oversized provider inventory");std::wstring wide(text.begin(),text.end());auto document=Windows::Data::Json::JsonObject::Parse(ref new Platform::String(wide.c_str()));auto files=document->GetNamedArray(L"files");std::set<std::string> providerFiles;
  for(unsigned i=0;i<files->Size;++i)if(!providerFiles.insert(Ascii(files->GetAt(i)->GetObject()->GetNamedString(L"file"))).second)throw std::runtime_error("Repeated provider file");
  const std::set<std::string> audio={"HWAUD.dll","dsoal-aldrv.dll"},mesa={"HWMGL.dll","libgallium_wgl.dll","libglapi.dll"},both={"HWAUD.dll","dsoal-aldrv.dll","HWMGL.dll","libgallium_wgl.dll","libglapi.dll"};
  if(providerFiles!=audio&&providerFiles!=mesa&&providerFiles!=both)throw std::runtime_error("Unexpected actual provider set");
  using Hold=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);auto hold=reinterpret_cast<Hold>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));auto load=reinterpret_cast<Load>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","LoadLibraryExW"));
  std::ofstream report(output,std::ios::trunc);if(!report){SetLastError(ERROR_WRITE_FAULT);return (unsigned)-1;}report<<"{\"scope\":\"Actual additional licensed provider imports; separate from original inventory; export presence is not behavior/Xbox proof\",\"pairs\":[";std::set<std::string> names;unsigned total=0,missing=0,normalMissing=0;
  for(unsigned f=0;f<files->Size;++f){auto imports=files->GetAt(f)->GetObject()->GetNamedArray(L"imports");if(imports->Size>2048)throw std::runtime_error("Oversized provider import graph");for(unsigned i=0;i<imports->Size;++i){auto item=imports->GetAt(i)->GetObject();auto dll=Ascii(item->GetNamedString(L"module"));auto symbol=Ascii(item->GetNamedString(L"symbol"));auto kind=Ascii(item->GetNamedString(L"kind"));std::wstring dllWide(dll.begin(),dll.end());if(!Basename(dllWide)||symbol.empty()||symbol.size()>220||(kind!="normal"&&kind!="delay"))throw std::runtime_error("Invalid provider import record");std::string key=dll;for(auto& c:key)if(c>='A'&&c<='Z')c+=32;auto name=key+"!"+symbol;
   // Keep normal and delay occurrences separate when both exist: a delayed
   // occurrence must never hide another provider's required normal import.
   if(!names.emplace(name+":"+kind).second)continue;auto entry=modules.emplace(key,Held{});auto& provider=entry.first->second;
   if(entry.second){if(!(hold&&hold(0,dllWide.c_str(),&provider.value))){provider.value=LoadPackagedLibrary(dllWide.c_str(),0);if(!provider.value&&load)provider.value=load(dllWide.c_str(),nullptr,0x800);if(!provider.value){provider.error=GetLastError();if(!provider.error)provider.error=ERROR_MOD_NOT_FOUND;}}}
   SetLastError(0);auto address=provider.value?Resolve(provider.value,symbol):nullptr;DWORD error=address?0:provider.value?GetLastError():provider.error;if(!address&&!error)error=ERROR_PROC_NOT_FOUND;if(!address){++missing;if(kind=="normal")++normalMissing;}
   if(total++)report<<',';report<<"\n{\"name\":"<<Json(name)<<",\"resolved\":"<<(address?"true":"false")<<",\"error\":"<<error<<",\"normalImport\":"<<(kind=="normal"?"true":"false")<<",\"mainImage\":false}";
  }}
  report<<"],\"total\":"<<total<<",\"missing\":"<<missing<<",\"normalMissing\":"<<normalMissing<<",\"providersRetainedUntilProcessExit\":true,\"gameExecuted\":false,\"xboxTested\":false}";report.flush();bool ok=report.good();if(!ok){SetLastError(ERROR_WRITE_FAULT);return (unsigned)-1;}return normalMissing;
 }catch(...){SetLastError(ERROR_INVALID_DATA);return (unsigned)-1;}
}
