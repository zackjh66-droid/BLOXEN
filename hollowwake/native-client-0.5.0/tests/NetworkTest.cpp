#include <winsock2.h>
#include <ws2tcpip.h>
#include "../Native/NetworkBridge.h"
#include <thread>
#include <iostream>
#include <stdexcept>
#include <string>
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class T>T Api(const char* name){auto p=ResolveNetworkImport("ws2_32.dll",name);if(!p)throw std::runtime_error(std::string("Missing ")+name);return reinterpret_cast<T>(p);}
#define API(n) Api<decltype(&n)>(#n)
int main(){try{
 WSADATA data={};auto startup=Api<decltype(&WSAStartup)>("#115");Check(startup(MAKEWORD(2,2),&data)==0,"Native ordinal startup failed");
 auto server=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);Check(server!=INVALID_SOCKET,"Own loopback socket failed");sockaddr_in endpoint={};endpoint.sin_family=AF_INET;endpoint.sin_addr.s_addr=htonl(INADDR_LOOPBACK);Check(!API(bind)(server,(sockaddr*)&endpoint,sizeof(endpoint))&&!API(listen)(server,1),"Own loopback server setup failed");int bytes=sizeof(endpoint);Check(!::getsockname(server,(sockaddr*)&endpoint,&bytes),"Own server port query failed");
 Check(HWConfigureNetwork(L"localhost",ntohs(endpoint.sin_port)),"Backend host configuration failed");Check(!HWConfigureNetwork(L"https://wrong.example/path",443),"Full URL accepted as hostname");ADDRINFOA* resolved=nullptr;auto port=std::to_string(ntohs(endpoint.sin_port));ADDRINFOA hints={};hints.ai_family=AF_INET;Check(!API(getaddrinfo)("localhost",port.c_str(),&hints,&resolved)&&resolved,"Allowed hostname did not use real DNS");
 auto client=API(socket)(AF_INET,SOCK_STREAM,IPPROTO_TCP);Check(client!=INVALID_SOCKET,"Real client socket failed");Check(API(socket)(AF_INET,SOCK_DGRAM,0)==INVALID_SOCKET&&WSAGetLastError()==WSAEACCES,"UDP bypass admitted");sockaddr_in denied=endpoint;denied.sin_port=htons(ntohs(endpoint.sin_port)==65535?65534:ntohs(endpoint.sin_port)+1);Check(API(connect)(client,(sockaddr*)&denied,sizeof(denied))==SOCKET_ERROR&&WSAGetLastError()==WSAEACCES,"Unresolved port bypass admitted");
 Check(!API(connect)(client,resolved->ai_addr,(int)resolved->ai_addrlen),"Native restricted socket connect failed");auto peer=API(accept)(server,nullptr,nullptr);Check(peer!=INVALID_SOCKET,"Own server accept failed");char input=(char)(GetTickCount64()%100+1),output=0;Check(API(send)(client,&input,1,0)==1&&::recv(peer,&output,1,0)==1&&output==input,"Actual socket data not delivered");
 API(freeaddrinfo)(resolved);resolved=nullptr;Check(API(getaddrinfo)("account-public-service-prod03.ol.epicgames.com","443",&hints,&resolved)!=0&&!resolved,"Production hostname admitted");Check(!Api<hostent*(WSAAPI*)(const char*)>("gethostbyname")("accounts.epicgames.com"),"Legacy DNS bypass admitted");Check(API(WSASocketW)(AF_INET,SOCK_DGRAM,0,nullptr,0,0)==INVALID_SOCKET,"Extended UDP bypass admitted");
 auto wildcard=API(socket)(AF_INET,SOCK_STREAM,IPPROTO_TCP);sockaddr_in any={};any.sin_family=AF_INET;Check(wildcard!=INVALID_SOCKET&&!API(bind)(wildcard,(sockaddr*)&any,sizeof(any)),"Own wildcard client bind failed");Check(API(listen)(wildcard,1)==SOCKET_ERROR&&WSAGetLastError()==WSAEACCES,"Listening on unscoped network interfaces admitted");API(closesocket)(wildcard);
 in_addr address={};address.s_addr=htonl(INADDR_LOOPBACK);Check(!strcmp(Api<decltype(&inet_ntoa)>("#12")(address),"127.0.0.1"),"Actual ordinal inet_ntoa conversion failed");address.s_addr=htonl(0xc0000201);Check(!Api<decltype(&gethostbyaddr)>("#51")((const char*)&address,4,AF_INET)&&WSAGetLastError()==WSAEACCES,"Unapproved reverse DNS admitted");
 WSAQUERYSETW query={};query.dwSize=sizeof(query);query.dwNameSpace=NS_DNS;query.lpszServiceInstanceName=(LPWSTR)L"unapproved.invalid";HANDLE lookup=nullptr;
 Check(API(WSALookupServiceBeginW)(&query,0,&lookup)==SOCKET_ERROR&&!lookup&&WSAGetLastError()==WSAEACCES,"Namespace resolver bypass admitted");Check(API(WSALookupServiceEnd)((HANDLE)621)==SOCKET_ERROR&&WSAGetLastError()==WSA_INVALID_HANDLE,"Foreign namespace search handle admitted");Check(API(WSASetServiceW)(&query,RNRSERVICE_REGISTER,0)==SOCKET_ERROR&&WSAGetLastError()==WSAEACCES,"Unapproved service registration admitted");
 ::closesocket(peer);::closesocket(server);API(closesocket)(client);API(WSACleanup)();std::cout<<"PASS production native socket adapter: actual owned loopback DNS/connect/data, ordinal startup, unapproved destination/port and UDP rejection. No external service/game/Xbox execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
