#include "LdapProvider.h"
#include "NetworkBridge.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <cstring>
#include <string>
#include <map>
#include <mutex>
namespace {
constexpr ULONG parameterError=0x59,policyDenied=0x35;
using Init=void*(__cdecl*)(char*,ULONG);using Set=ULONG(__cdecl*)(void*,int,const void*);using Unbind=ULONG(__cdecl*)(void*);
using Bind=ULONG(__cdecl*)(void*,char*,char*);using Search=ULONG(__cdecl*)(void*,char*,ULONG,char*,char**,ULONG,void**);
struct Provider {
 HMODULE module=nullptr;bool complete=false;
 Provider(){using Hold=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);
  auto hold=reinterpret_cast<Hold>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));auto load=reinterpret_cast<Load>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","LoadLibraryExW"));
  if(!(hold&&hold(0,L"wldap32.dll",&module))&&load)module=load(L"wldap32.dll",nullptr,0x800);
  complete=module!=nullptr;const unsigned ordinals[]={143,200,211,22,26,27,30,301,32,33,35,41,46,50,60,79};
  for(auto ordinal:ordinals)if(!module||!GetProcAddress(module,reinterpret_cast<LPCSTR>((ULONG_PTR)ordinal)))complete=false;
 }
 ~Provider(){if(module)FreeLibrary(module);}
 template<class T>T Get(unsigned ordinal){return reinterpret_cast<T>(module?GetProcAddress(module,reinterpret_cast<LPCSTR>((ULONG_PTR)ordinal)):nullptr);}
};
Provider& Native(){static Provider value;return value;}
struct Session {std::string host;unsigned short port;};std::recursive_mutex gate;std::map<void*,Session> sessions;
bool Permitted(void* ld){auto it=sessions.find(ld);return it!=sessions.end()&&IsConfiguredNetworkHost(it->second.host.c_str(),it->second.port);}
void* __cdecl Initialize(char* host,ULONG port){
 if(!host||!port||port>65535||!IsConfiguredNetworkHost(host,(unsigned short)port)){SetLastError(ERROR_ACCESS_DENIED);return nullptr;}
 std::lock_guard<std::recursive_mutex> lock(gate);auto& native=Native();auto init=native.Get<Init>(143);auto set=native.Get<Set>(211);auto unbind=native.Get<Unbind>(46);if(!native.complete||!init||!set||!unbind){SetLastError(ERROR_PROC_NOT_FOUND);return nullptr;}
 auto ld=init(host,port);if(!ld)return nullptr;
 // Do not chase LDAP referrals to a different service. LDAP_OPT_OFF is the
 // documented null option value, not a fabricated successful setter result.
 auto status=set(ld,0x08,nullptr);if(status){unbind(ld);SetLastError(status);return nullptr;}
 try{sessions.emplace(ld,Session{host,(unsigned short)port});}catch(...){unbind(ld);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return nullptr;}return ld;
}
ULONG __cdecl Option(void* ld,int option,const void* value){
 std::lock_guard<std::recursive_mutex> lock(gate);if(!ld||sessions.find(ld)==sessions.end())return parameterError;if(!Permitted(ld))return policyDenied;
 // Protocol version / result limits can be passed to the OS. Host, referral,
 // callback and certificate-validation changes would bypass the endpoint policy.
 if(option==0x08&&value)return policyDenied;
 if(option!=0x08&&option!=0x11&&option!=0x03&&option!=0x04&&option!=0x02)return policyDenied;
 auto set=Native().Get<Set>(211);return set?set(ld,option,value):0x5c;
}
ULONG __cdecl AnonymousBind(void* ld,char* dn,char* password){
 std::lock_guard<std::recursive_mutex> lock(gate);if(!ld||sessions.find(ld)==sessions.end())return parameterError;if(!Permitted(ld))return policyDenied;
 // Hosted Hollowwake authentication is HTTPS. Never send project/account
 // credentials over this optional plain LDAP protocol.
 if((dn&&*dn)||(password&&*password))return 0x08; // LDAP_STRONG_AUTH_REQUIRED.
 auto bind=Native().Get<Bind>(60);return bind?bind(ld,dn,password):0x5c;
}
ULONG __cdecl Query(void* ld,char* base,ULONG scope,char* filter,char** attributes,ULONG only,void** result){
 if(!result)return parameterError;*result=nullptr;std::lock_guard<std::recursive_mutex> lock(gate);
 if(!ld||sessions.find(ld)==sessions.end())return parameterError;if(!Permitted(ld))return policyDenied;
 auto search=Native().Get<Search>(50);return search?search(ld,base,scope,filter,attributes,only,result):0x5c;
}
ULONG __cdecl Release(void* ld){std::lock_guard<std::recursive_mutex> lock(gate);auto it=sessions.find(ld);if(it==sessions.end())return parameterError;sessions.erase(it);auto unbind=Native().Get<Unbind>(46);return unbind?unbind(ld):0x5c;}
}
void* ResolveLdapImport(const char* dll,const char* symbol){
 if(!dll||!symbol||_stricmp(dll,"wldap32.dll"))return nullptr;auto& native=Native();if(!native.complete)return nullptr;
 struct Name {const char* ordinal;const char* name;unsigned number;};const Name names[]={{"#143","ldap_init",143},{"#200","ldap_memfree",200},{"#211","ldap_set_option",211},{"#22","ldap_err2string",22},{"#26","ldap_first_entry",26},{"#27","ldap_next_entry",27},{"#30","ldap_get_dn",30},{"#301","ber_free",301},{"#32","ldap_first_attribute",32},{"#33","ldap_next_attribute",33},{"#35","ldap_get_values_len",35},{"#41","ldap_msgfree",41},{"#46","ldap_unbind_s",46},{"#50","ldap_search_s",50},{"#60","ldap_simple_bind_s",60},{"#79","ldap_value_free_len",79}};
 for(auto& item:names)if(!strcmp(symbol,item.ordinal)||!strcmp(symbol,item.name)){
  if(item.number==143)return reinterpret_cast<void*>(&Initialize);if(item.number==211)return reinterpret_cast<void*>(&Option);if(item.number==60)return reinterpret_cast<void*>(&AnonymousBind);if(item.number==50)return reinterpret_cast<void*>(&Query);if(item.number==46)return reinterpret_cast<void*>(&Release);
  // Result traversal/allocation/free stay in one actual Microsoft provider.
  return native.Get<void*>(item.number);
 }return nullptr;
}
