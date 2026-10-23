#pragma once
#include <atomic>
#include <cstring>
#include <cstdlib>
// Configuration overlay for the client's legacy OpenSSL CPU dispatcher only.
// It selects existing library implementations; it does not implement a hash,
// accept a signature, alter CPUID, or change the host process's environment.
class ClientCryptoEnvironment {
public:
 using Getter=char*(__cdecl*)(const char*);
 bool Bind(Getter value){if(!value)return false;auto expected=static_cast<Getter>(nullptr);return original.compare_exchange_strong(expected,value)||expected==value;}
 bool Enable(){if(!original.load())return false;enabled.store(true);return true;}
 static char* DefaultMask(){static char value[]=":~0x20000000";return value;}
 char* Lookup(const char* name)const{
  auto real=original.load();if(!real)std::abort(); // A wrapper is never published without its real provider.
  auto value=real(name);
  if(enabled.load()&&name&&!_stricmp(name,"OPENSSL_ia32cap")&&(!value||!value[0]))return DefaultMask();
  return value;
 }
private:
 std::atomic<Getter> original{nullptr};std::atomic<bool> enabled{false};
};
