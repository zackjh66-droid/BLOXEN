// Original DLL importing the observed legacy C ABI; no CRT startup or game code.
#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cerrno>
#include <malloc.h>
extern "C" {
static unsigned char* buffer=nullptr;static BOOL attached=FALSE;
unsigned char** volatile memoryFixtureAnchor=&buffer;
BOOL WINAPI MemoryEntry(HINSTANCE,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){*memoryFixtureAnchor=static_cast<unsigned char*>(malloc(64));if(!buffer)return FALSE;memset(buffer,0x5a,64);attached=TRUE;}
 return TRUE;
}
__declspec(dllexport) BOOL WINAPI MemoryWasAttached(){return attached;}
__declspec(dllexport) int WINAPI RunMemoryChecks(){
 if(!attached||!buffer)return 1;
 if((reinterpret_cast<ULONG_PTR>(buffer)&15)||_msize(buffer)<64)return 2;
 for(unsigned i=0;i<64;++i)if(buffer[i]!=0x5a)return 3;
 for(unsigned i=0;i<64;++i)buffer[i]=static_cast<unsigned char>(i);
 auto grown=static_cast<unsigned char*>(realloc(buffer,128));if(!grown)return 4;buffer=grown;
 for(unsigned i=0;i<64;++i)if(buffer[i]!=i)return 5;
 auto zero=static_cast<unsigned char*>(calloc(16,4));if(!zero)return 6;
 for(unsigned i=0;i<64;++i)if(zero[i])return 7;
 memcpy(zero,buffer,64);if(memcmp(zero,buffer,64))return 8;
 memmove(zero+1,zero,63);for(unsigned i=1;i<64;++i)if(zero[i]!=i-1)return 9;
 errno=0;auto failed=realloc(buffer,static_cast<size_t>(-1));if(failed||errno!=ENOMEM)return 10;
 for(unsigned i=0;i<64;++i)if(buffer[i]!=i)return 11;
 errno=0;auto overflow=calloc(static_cast<size_t>(-1),2);if(overflow||errno!=ENOMEM)return 12;
 auto narrow=_strdup("Hollowwake");auto wide=_wcsdup(L"Hollowwake");if(!narrow||!wide)return 13;
 const char expected[]="Hollowwake";const wchar_t expectedWide[]=L"Hollowwake";
 for(unsigned i=0;i<10;++i)if(narrow[i]!=expected[i]||wide[i]!=expectedWide[i])return 14;
 free(narrow);free(wide);free(zero);free(buffer);buffer=nullptr;free(nullptr);
 auto fresh=realloc(nullptr,16);if(!fresh)return 15;if(realloc(fresh,0)!=nullptr)return 16;
 auto empty=malloc(0);if(!empty)return 17;free(empty);
 return 0;
}
}
