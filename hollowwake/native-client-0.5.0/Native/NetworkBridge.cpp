// Original native Winsock bridge. Uses real OS sockets and errors, never an
// emulated HTTP login. Endpoint redirection/TLS host binding is a separate gap.
#include <winsock2.h>
#include <ws2tcpip.h>
#include "NetworkBridge.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <mutex>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <array>
#include <map>
#include <set>
namespace {
struct Endpoint{sockaddr_storage address;int length;ULONGLONG expiry;};
std::mutex gate;std::string hostname;unsigned short allowedPort=443;std::vector<Endpoint> permitted;
bool Equal(const sockaddr* a,int size,const Endpoint& item){if(size!=item.length||!a||a->sa_family!=item.address.ss_family)return false;
 if(a->sa_family==AF_INET){auto x=(const sockaddr_in*)a,y=(const sockaddr_in*)&item.address;return x->sin_port==y->sin_port&&!memcmp(&x->sin_addr,&y->sin_addr,sizeof(IN_ADDR));}
 if(a->sa_family==AF_INET6){auto x=(const sockaddr_in6*)a,y=(const sockaddr_in6*)&item.address;return x->sin6_port==y->sin6_port&&x->sin6_scope_id==y->sin6_scope_id&&!memcmp(&x->sin6_addr,&y->sin6_addr,sizeof(IN6_ADDR));}return false;
}
bool Allowed(const sockaddr* address,int length){std::lock_guard<std::mutex> lock(gate);auto now=GetTickCount64();for(auto& item:permitted)if(item.expiry>=now&&Equal(address,length,item))return true;return false;}
int WSAAPI Startup(WORD version,LPWSADATA data){return ::WSAStartup(version,data);}
int WSAAPI Resolve(PCSTR node,PCSTR service,const ADDRINFOA* hint,PADDRINFOA* output){
 if(!output)return EAI_FAIL;*output=nullptr;std::string current;unsigned short port;{std::lock_guard<std::mutex> lock(gate);current=hostname;port=allowedPort;}
 if(current.empty()||!node||_stricmp(node,current.c_str())||!service||(service!=std::to_string(port)&&_stricmp(service,"https"))||(hint&&(hint->ai_flags&AI_PASSIVE||hint->ai_socktype&&hint->ai_socktype!=SOCK_STREAM||hint->ai_protocol&&hint->ai_protocol!=IPPROTO_TCP))){WSASetLastError(WSAEACCES);return EAI_NONAME;}
 if(!_stricmp(service,"https")&&port!=443){WSASetLastError(WSAEACCES);return EAI_SERVICE;}
 ADDRINFOA restricted=hint?*hint:ADDRINFOA{};restricted.ai_socktype=SOCK_STREAM;restricted.ai_protocol=IPPROTO_TCP;auto result=::getaddrinfo(node,service,&restricted,output);if(result)return result;
 std::lock_guard<std::mutex> lock(gate);if(current!=hostname||port!=allowedPort){::freeaddrinfo(*output);*output=nullptr;return EAI_AGAIN;}permitted.clear();
 for(auto entry=*output;entry;entry=entry->ai_next)if(entry->ai_addr&&(entry->ai_family==AF_INET||entry->ai_family==AF_INET6)&&entry->ai_addrlen<=sizeof(sockaddr_storage)){Endpoint item={};item.length=(int)entry->ai_addrlen;memcpy(&item.address,entry->ai_addr,entry->ai_addrlen);item.expiry=GetTickCount64()+60000;permitted.push_back(item);}return 0;
}
SOCKET WSAAPI Socket(int family,int type,int protocol){if((family!=AF_INET&&family!=AF_INET6)||type!=SOCK_STREAM||(protocol&&protocol!=IPPROTO_TCP)){WSASetLastError(WSAEACCES);return INVALID_SOCKET;}return ::socket(family,type,protocol);}
int WSAAPI Connect(SOCKET socket,const sockaddr* address,int size){if(!Allowed(address,size)){WSASetLastError(WSAEACCES);return SOCKET_ERROR;}return ::connect(socket,address,size);}
int WSAAPI ConnectExtended(SOCKET socket,const sockaddr* address,int size,LPWSABUF caller,LPWSABUF callee,LPQOS sqos,LPQOS gqos){if(!Allowed(address,size)){WSASetLastError(WSAEACCES);return SOCKET_ERROR;}return ::WSAConnect(socket,address,size,caller,callee,sqos,gqos);}
int WSAAPI Bind(SOCKET socket,const sockaddr* address,int size){bool local=false;if(address&&address->sa_family==AF_INET&&size==sizeof(sockaddr_in)){auto p=(const sockaddr_in*)address;local=p->sin_port==0&&(p->sin_addr.s_addr==htonl(INADDR_ANY)||p->sin_addr.s_addr==htonl(INADDR_LOOPBACK));}if(address&&address->sa_family==AF_INET6&&size==sizeof(sockaddr_in6)){auto p=(const sockaddr_in6*)address;local=p->sin6_port==0&&(IN6_IS_ADDR_UNSPECIFIED(&p->sin6_addr)||IN6_IS_ADDR_LOOPBACK(&p->sin6_addr));}if(!local){WSASetLastError(WSAEACCES);return SOCKET_ERROR;}return ::bind(socket,address,size);}
SOCKET WSAAPI SocketExtended(int family,int type,int protocol,LPWSAPROTOCOL_INFOW info,GROUP group,DWORD flags){if((family!=AF_INET&&family!=AF_INET6)||type!=SOCK_STREAM||(protocol&&protocol!=IPPROTO_TCP)||info||group||flags&~0x81){WSASetLastError(WSAEACCES);return INVALID_SOCKET;}return ::WSASocketW(family,type,protocol,nullptr,0,flags);}
int WSAAPI SendBuffers(SOCKET socket,LPWSABUF buffers,DWORD count,LPDWORD sent,DWORD flags,const sockaddr* address,int size,LPWSAOVERLAPPED overlapped,LPWSAOVERLAPPED_COMPLETION_ROUTINE callback){if(address&&!Allowed(address,size)){WSASetLastError(WSAEACCES);return SOCKET_ERROR;}return ::WSASendTo(socket,buffers,count,sent,flags,address,size,overlapped,callback);}
hostent* WSAAPI LegacyHost(const char* node){
 struct LegacyResult{hostent host={};std::string name;char* aliases[1]={nullptr};std::vector<std::array<char,4>> addresses;std::vector<char*> pointers;};static thread_local LegacyResult result;
 std::string service;{std::lock_guard<std::mutex> lock(gate);service=std::to_string(allowedPort);}ADDRINFOA hint={};hint.ai_family=AF_INET;ADDRINFOA* output=nullptr;if(Resolve(node,service.c_str(),&hint,&output))return nullptr;
 result.name=node;result.addresses.clear();result.pointers.clear();for(auto p=output;p;p=p->ai_next)if(p->ai_family==AF_INET&&p->ai_addrlen>=sizeof(sockaddr_in)){std::array<char,4> address;memcpy(address.data(),&((sockaddr_in*)p->ai_addr)->sin_addr,4);result.addresses.push_back(address);}::freeaddrinfo(output);if(result.addresses.empty()){WSASetLastError(WSANO_DATA);return nullptr;}for(auto& address:result.addresses)result.pointers.push_back(address.data());result.pointers.push_back(nullptr);result.host.h_name=&result.name[0];result.host.h_aliases=result.aliases;result.host.h_addrtype=AF_INET;result.host.h_length=4;result.host.h_addr_list=result.pointers.data();return &result.host;
}
int WSAAPI ReverseName(const sockaddr* address,socklen_t size,char* host,DWORD hostSize,char* service,DWORD serviceSize,int flags){if(!Allowed(address,size)){WSASetLastError(WSAEACCES);return EAI_FAIL;}return ::getnameinfo(address,size,host,hostSize,service,serviceSize,flags);}
int WSAAPI SendTo(SOCKET socket,const char* data,int size,int flags,const sockaddr* address,int length){if(address&&!Allowed(address,length)){WSASetLastError(WSAEACCES);return SOCKET_ERROR;}return ::sendto(socket,data,size,flags,address,length);}
int WSAAPI Ioctl(SOCKET socket,DWORD code,void* input,DWORD inputSize,void* output,DWORD outputSize,DWORD* returned,LPWSAOVERLAPPED overlapped,LPWSAOVERLAPPED_COMPLETION_ROUTINE callback){
 // Extension function pointers could bypass Connect's destination restriction.
 if(code==0xC8000006||code==0xC8000024){if(returned)*returned=0;WSASetLastError(WSAEOPNOTSUPP);return SOCKET_ERROR;}
 return ::WSAIoctl(socket,code,input,inputSize,output,outputSize,returned,overlapped,callback);
}
#include "NetworkNamespaces.h"
}
extern "C" BOOL __cdecl HWConfigureNetwork(const wchar_t* value,unsigned short port){if(!value||!port)return FALSE;std::string name;for(unsigned i=0;i<254;++i){auto c=value[i];if(!c)break;if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'-'))return FALSE;name+=(char)c;if(i==253)return FALSE;}if(name.empty()||name.front()=='.'||name.back()=='.')return FALSE;std::lock_guard<std::mutex> lock(gate);hostname=name;allowedPort=port;permitted.clear();return TRUE;}
bool IsConfiguredNetworkHost(const char* node,unsigned short port){std::lock_guard<std::mutex> lock(gate);return node&&!hostname.empty()&&!_stricmp(node,hostname.c_str())&&port==allowedPort;}
void* ResolveNetworkImport(const char* dll,const char* name){if(!dll||!name||_stricmp(dll,"ws2_32.dll"))return nullptr;
#define API(n,f) if(!strcmp(name,n))return reinterpret_cast<void*>(&f)
 API("accept",AcceptLocal);API("#1",AcceptLocal);API("listen",ListenLocal);API("#13",ListenLocal);API("gethostbyaddr",ReverseHost);API("#51",ReverseHost);
 API("WSALookupServiceBeginW",LookupBegin);API("WSALookupServiceNextW",LookupNext);API("WSALookupServiceEnd",LookupEnd);API("WSASetServiceW",SetLocalService);
 API("WSAStartup",Startup);API("#115",Startup); // Windows Winsock ordinal115 is WSAStartup.
 API("bind",Bind);API("#2",Bind);API("WSASocketW",SocketExtended);API("WSASendTo",SendBuffers);API("gethostbyname",LegacyHost);API("#52",LegacyHost);API("getnameinfo",ReverseName);API("getaddrinfo",Resolve);API("socket",Socket);API("#23",Socket);API("connect",Connect);API("#4",Connect);API("WSAConnect",ConnectExtended);API("sendto",SendTo);API("#20",SendTo);API("WSAIoctl",Ioctl);
#undef API
 // Unsafe/unimplemented resolver, server, provider-catalog and extended socket
 // APIs remain absent rather than exposing an unrestricted native pointer.
 const char* names[]={"inet_ntoa","#12","closesocket","#3","getpeername","#5","getsockname","#6","getsockopt","#7","htonl","#8","htons","#9","ioctlsocket","#10","inet_addr","#11","ntohl","#14","ntohs","#15","recv","#16","select","#18","send","#19","setsockopt","#21","shutdown","#22","WSACleanup","#116","WSAGetLastError","#111","WSASetLastError","#112","__WSAFDIsSet","#151","freeaddrinfo","inet_pton","WSAAddressToStringW","WSACloseEvent","WSACreateEvent","WSAEnumNetworkEvents","WSAEventSelect","WSAResetEvent","WSASetEvent","WSAStringToAddressW","WSAWaitForMultipleEvents","recvfrom","#17","getservbyname","#55","getprotobyname","#53","gethostname","#57","WSAEnumNameSpaceProvidersW","WSAGetOverlappedResult","WSARecvFrom","WSASend","WSASendDisconnect","WSCEnumProtocols","WSCGetProviderPath"};
 bool allowed=false;for(auto item:names)if(!strcmp(name,item))allowed=true;if(!allowed)return nullptr;
 using Module=HMODULE(WINAPI*)(LPCWSTR);auto module=reinterpret_cast<Module>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleW"));auto handle=module?module(L"ws2_32.dll"):nullptr;if(!handle)return nullptr;
 if(name[0]=='#'){unsigned ordinal=0;for(auto p=name+1;*p;++p)ordinal=ordinal*10+(*p-'0');return reinterpret_cast<void*>(GetProcAddress(handle,reinterpret_cast<LPCSTR>((ULONG_PTR)ordinal)));}return reinterpret_cast<void*>(GetProcAddress(handle,name));
}
