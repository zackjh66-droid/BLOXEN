#pragma once
#include <cstdint>
#include <string>
#include <mutex>
namespace Hollowwake {
// Only a short-lived, one-use game ticket belongs here. Never a project token.
class ClientLaunchIdentity {
 std::mutex gate;std::wstring ticket;std::uint64_t expires=0;bool consumed=false;
 static void Erase(std::wstring& text){if(!text.empty()){volatile wchar_t* p=&text[0];for(size_t i=0;i<text.size();++i)p[i]=0;}text.clear();}
 public:
 ~ClientLaunchIdentity(){Erase(ticket);}
 bool Configure(const std::wstring& code,unsigned seconds,std::uint64_t now){
  if(code.size()!=43||seconds<1||seconds>300)return false;
  for(auto c:code)if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'_'||c==L'-'))return false;
  std::lock_guard<std::mutex> lock(gate);if(consumed)return false;Erase(ticket);ticket=code;expires=now+static_cast<std::uint64_t>(seconds)*1000;return true;
 }
 bool Consume(std::uint64_t now,std::wstring& tail){
  std::lock_guard<std::mutex> lock(gate);if(consumed||ticket.empty()||now>=expires){Erase(ticket);return false;}
  tail=L" -AUTH_LOGIN=unused -AUTH_TYPE=exchangecode -AUTH_PASSWORD="+ticket;Erase(ticket);consumed=true;return true;
 }
};
}
