// Original legacy C allocation bridge. All mapped allocation/free entry points
// use the SAME maintained UWP CRT. No FILE layouts, old C++ EH or CRT globals
// are aliased to incompatible modern CRT structures.
#pragma once
#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <malloc.h>
#include <cerrno>
namespace Hollowwake {
using VoidInitializer=void(__cdecl*)();
using IntInitializer=int(__cdecl*)();
// Half-open tables use the legacy C ABI; callbacks execute in their original
// order. Null sentinel entries are skipped. Exceptions are not swallowed.
inline void __cdecl GuestInitTerm(VoidInitializer* first,VoidInitializer* last){
 for(auto current=first;current!=last;++current)if(*current)(*current)();
}
inline int __cdecl GuestInitTermE(IntInitializer* first,IntInitializer* last){
 for(auto current=first;current!=last;++current)if(*current){int result=(*current)();if(result)return result;}
 return 0;
}
inline void* ResolveLegacyCrtImport(const char* dll,const char* name){
 if(!dll||!name||_stricmp(dll,"msvcrt.dll"))return nullptr;
 #define CRT(symbol, target) if(!strcmp(name,symbol))return reinterpret_cast<void*>(&target)
 CRT("_initterm",GuestInitTerm);CRT("_initterm_e",GuestInitTermE);
 CRT("malloc",std::malloc);CRT("calloc",std::calloc);CRT("realloc",std::realloc);CRT("free",std::free);
 CRT("_msize",::_msize);CRT("_errno",::_errno);CRT("_strdup",::_strdup);CRT("_wcsdup",::_wcsdup);
 CRT("memcpy",std::memcpy);CRT("memmove",std::memmove);CRT("memset",std::memset);CRT("memcmp",std::memcmp);
 #undef CRT
 return nullptr;
}
inline bool LegacyAllocatorContract(){
 auto allocate=reinterpret_cast<void*(__cdecl*)(size_t)>(ResolveLegacyCrtImport("msvcrt.dll","malloc"));
 auto release=reinterpret_cast<void(__cdecl*)(void*)>(ResolveLegacyCrtImport("msvcrt.dll","free"));
 auto resize=reinterpret_cast<void*(__cdecl*)(void*,size_t)>(ResolveLegacyCrtImport("msvcrt.dll","realloc"));
 auto zeros=reinterpret_cast<void*(__cdecl*)(size_t,size_t)>(ResolveLegacyCrtImport("msvcrt.dll","calloc"));
 auto size=reinterpret_cast<size_t(__cdecl*)(void*)>(ResolveLegacyCrtImport("msvcrt.dll","_msize"));
 if(!allocate||!release||!resize||!zeros||!size)return false;
 BYTE* p=static_cast<BYTE*>(allocate(64));if(!p)return false;
 bool good=(reinterpret_cast<ULONG_PTR>(p)&15)==0&&size(p)>=64;for(unsigned i=0;i<64;++i)p[i]=(BYTE)i;
 auto grown=static_cast<BYTE*>(resize(p,128));if(!grown){release(p);return false;}p=grown;
 for(unsigned i=0;i<64;++i)good=good&&p[i]==i;
 auto q=static_cast<BYTE*>(zeros(16,4));if(!q){release(p);return false;}
 for(unsigned i=0;i<64;++i)good=good&&q[i]==0;
 int saved=errno;errno=0;auto impossible=resize(p,static_cast<size_t>(-1));
 good=good&&!impossible&&errno==ENOMEM;if(impossible){p=static_cast<BYTE*>(impossible);good=false;}
 for(unsigned i=0;i<64;++i)good=good&&p[i]==i;
 errno=0;auto overflow=zeros(static_cast<size_t>(-1),2);good=good&&!overflow&&errno==ENOMEM;if(overflow)release(overflow);
 release(q);release(p);release(nullptr);errno=saved;return good;
}
}
