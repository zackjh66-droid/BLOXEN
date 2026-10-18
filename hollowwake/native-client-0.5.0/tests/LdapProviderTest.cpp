// Original loopback LDAP protocol fixture. Executes no game/third-party code.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include "../Native/LdapProvider.h"
#include "../Native/NetworkBridge.h"
using Bytes=std::vector<unsigned char>;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static Bytes Cat(Bytes a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());return a;}
static Bytes Tag(unsigned char tag,const Bytes& b){Check(b.size()<128,"Fixture TLV exceeds short length");Bytes result={tag,(unsigned char)b.size()};return Cat(result,b);}
static Bytes Text(const char* s){return Tag(4,Bytes(s,s+strlen(s)));}
static bool Read(SOCKET s,unsigned char* p,size_t n){while(n){int count=recv(s,(char*)p,(int)n,0);if(count<=0)return false;p+=count;n-=count;}return true;}
static Bytes Packet(SOCKET s){unsigned char h[2];if(!Read(s,h,2))return {};Check(h[0]==0x30,"Native client sent non-LDAP packet");size_t n=h[1];if(n&128){unsigned count=(unsigned)n&127;Check(count&&count<=2,"Invalid BER size");n=0;for(unsigned i=0;i<count;++i){unsigned char c;Check(Read(s,&c,1),"BER length truncated");n=(n<<8)|c;}}Check(n<=8192,"LDAP fixture packet exceeds bound");Bytes result(n);Check(Read(s,result.data(),n),"LDAP message truncated");return result;}
static void Send(SOCKET s,const Bytes& b){size_t n=0;while(n<b.size()){auto count=send(s,(const char*)b.data()+n,(int)(b.size()-n),0);Check(count>0,"Fixture response send failed");n+=count;}}
struct Server {
 SOCKET listener=INVALID_SOCKET;std::atomic<SOCKET> connection{INVALID_SOCKET};std::thread thread;unsigned short port=0;std::atomic<bool> valid{true};std::atomic<unsigned> binds{0},searches{0},lastOperation{0};std::atomic<int> socketError{0};
 Server(){listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);Check(listener!=INVALID_SOCKET,"Loopback listener failed");sockaddr_in address={};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);Check(bind(listener,(sockaddr*)&address,sizeof(address))==0&&listen(listener,1)==0,"Loopback bind/listen failed");int length=sizeof(address);Check(getsockname(listener,(sockaddr*)&address,&length)==0,"Loopback port query failed");port=ntohs(address.sin_port);
  thread=std::thread([this]{try{connection=accept(listener,nullptr,nullptr);Check(connection!=INVALID_SOCKET,"Fixture accept failed");DWORD timeout=5000;setsockopt(connection,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));
   for(unsigned n=0;n<8;++n){auto request=Packet(connection);if(request.empty())break;Check(request.size()>=5&&request[0]==2&&request[1]>0&&request[1]<=4,"LDAP message ID missing");size_t idSize=2+request[1];Check(idSize+2<=request.size(),"LDAP operation missing");Bytes id(request.begin(),request.begin()+idSize);auto op=request[idSize];lastOperation=op;Bytes success=Cat(Bytes{0x0a,1,0},Cat(Text(""),Text("")));
    if(op==0x60){++binds;Send(connection,Tag(0x30,Cat(id,Tag(0x61,success))));}
    else if(op==0x63){++searches;auto attribute=Tag(0x30,Cat(Text("cn"),Tag(0x31,Text("Fixture"))));auto entry=Tag(0x64,Cat(Text("cn=HollowwakeFixture"),Tag(0x30,attribute)));Send(connection,Tag(0x30,Cat(id,entry)));Send(connection,Tag(0x30,Cat(id,Tag(0x65,success))));}
    else if(op==0x42)break;else throw std::runtime_error("Unexpected native LDAP operation");
   }
  }catch(const std::exception& e){socketError=WSAGetLastError();std::cerr<<"SERVER "<<e.what()<<" Win32="<<socketError<<"\n";valid=false;}});
 }
 void Finish(){if(thread.joinable())thread.join();}
 ~Server(){if(connection!=INVALID_SOCKET){shutdown(connection,SD_BOTH);closesocket(connection);}if(listener!=INVALID_SOCKET)closesocket(listener);if(thread.joinable())thread.join();}
};
template<class T>T Api(const char* symbol){auto p=ResolveLdapImport("WLDAP32.dll",symbol);Check(p!=nullptr,"LDAP production binding missing");return reinterpret_cast<T>(p);}
struct Value {ULONG length;char* data;};static_assert(sizeof(Value)==16,"Windows BERVAL x64 ABI");
int main(){try{
 WSADATA wsa={};Check(WSAStartup(MAKEWORD(2,2),&wsa)==0,"Winsock startup failed");
 const char* symbols[]={"#143","#200","#211","#22","#26","#27","#30","#301","#32","#33","#35","#41","#46","#50","#60","#79"};for(auto s:symbols)Check(ResolveLdapImport("wldap32.dll",s)!=nullptr,"Exact required LDAP ordinal missing");Check(!ResolveLdapImport("wldap32.dll","ldap_sslinit"),"Unrestricted LDAP session constructor exposed");
 auto init=Api<void*(__cdecl*)(char*,ULONG)>("#143");auto option=Api<ULONG(__cdecl*)(void*,int,const void*)>("#211");auto bind=Api<ULONG(__cdecl*)(void*,char*,char*)>("#60");auto search=Api<ULONG(__cdecl*)(void*,char*,ULONG,char*,char**,ULONG,void**)>("#50");auto unbind=Api<ULONG(__cdecl*)(void*)>("#46");
 Check(HWConfigureNetwork(L"127.0.0.1",443),"Policy initialization failed");SetLastError(0);Check(!init((char*)"not-the-configured-host.invalid",443)&&GetLastError()==ERROR_ACCESS_DENIED,"Unrelated LDAP host was not denied before I/O");Check(!init(nullptr,443)&&!init((char*)"127.0.0.1",0),"Implicit domain discovery/default port accepted");
 {Server server;Check(HWConfigureNetwork(L"127.0.0.1",server.port),"Loopback policy setup failed");void* session=init((char*)"127.0.0.1",server.port);Check(session,"Real LDAP session creation failed");ULONG version=3;Check(option(session,0x11,&version)==0,"Native LDAP protocol version setter failed");
 using GetOption=ULONG(__cdecl*)(void*,int,void*);auto getOption=(GetOption)GetProcAddress(GetModuleHandleW(L"wldap32.dll"),"ldap_get_option");ULONG actualVersion=0,referrals=1;Check(getOption&&getOption(session,0x11,&actualVersion)==0&&actualVersion==3&&getOption(session,0x08,&referrals)==0&&referrals==0,"Actual provider version/referral state differs");
 Check(option(session,0x08,(void*)1)==0x35&&option(session,0x30,"unrelated.invalid")==0x35,"Host/referral policy changes accepted");Check(bind(session,(char*)"do-not-send",(char*)"fixture-value")==0x08,"Plain LDAP credentials were allowed");
 auto bindStatus=bind(session,nullptr,nullptr);if(bindStatus)std::cerr<<"Native LDAP status="<<bindStatus<<" serverValid="<<server.valid<<" bindPackets="<<server.binds<<" searchPackets="<<server.searches<<" lastOperation="<<server.lastOperation<<" socketError="<<server.socketError<<"\n";Check(bindStatus==0,"Actual anonymous LDAP bind failed");void* result=nullptr;char* attributes[]={(char*)"cn",nullptr};Check(search(session,(char*)"",0,(char*)"(objectClass=*)",attributes,0,&result)==0&&result,"Native search response parser failed");
 auto first=Api<void*(__cdecl*)(void*,void*)>("#26");auto next=Api<void*(__cdecl*)(void*,void*)>("#27");auto dn=Api<char*(__cdecl*)(void*,void*)>("#30");auto freeText=Api<void(__cdecl*)(char*)>("#200");auto firstAttribute=Api<char*(__cdecl*)(void*,void*,void**)>("#32");auto nextAttribute=Api<char*(__cdecl*)(void*,void*,void*)>("#33");auto values=Api<Value**(__cdecl*)(void*,void*,char*)>("#35");auto freeValues=Api<ULONG(__cdecl*)(Value**)>("#79");auto freeBer=Api<void(__cdecl*)(void*,int)>("#301");auto freeMessage=Api<ULONG(__cdecl*)(void*)>("#41");
 void* entry=first(session,result);Check(entry&&next(session,entry)==nullptr,"Native LDAP entry traversal failed");auto entryDn=dn(session,entry);Check(entryDn&&!strcmp(entryDn,"cn=HollowwakeFixture"),"Native DN decode differs");freeText(entryDn);void* ber=nullptr;auto attribute=firstAttribute(session,entry,&ber);Check(attribute&&ber&&!strcmp(attribute,"cn"),"Native attribute traversal failed");auto data=values(session,entry,attribute);Check(data&&data[0]&&data[0]->length==7&&!memcmp(data[0]->data,"Fixture",7)&&!data[1],"Native BER value decode differs");freeValues(data);freeText(attribute);Check(nextAttribute(session,entry,ber)==nullptr,"Unexpected native attribute");freeBer(ber,0);freeMessage(result);
 Check(HWConfigureNetwork(L"revoked.invalid",443)&&bind(session,nullptr,nullptr)==0x35,"Existing LDAP session ignored endpoint-policy revocation");
 Check(unbind(session)==0&&unbind(session)==0x59,"Native unbind/session lifetime behavior failed");server.Finish();Check(server.valid&&server.binds==1&&server.searches==1,"Native provider did not use original loopback protocol fixture");}
 std::cout<<"PASS actual Microsoft LDAP bind/search, BER entry/DN/attribute/value parsing and native allocation cleanup; exact 16 ordinals; configured-host/port policy, referrals, plaintext credentials, invalid/global sessions and repeated-unbind rejection. No game/Epic/Xbox execution.\n";WSACleanup();return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
