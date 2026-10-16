// Original fixture modeling the inspected startup operations. No game code.
#include <windows.h>
#include <stddef.h>
#pragma warning(disable:4996)
void* __cdecl operator new(size_t);
void* __cdecl operator new[](size_t);
void __cdecl operator delete(void*) noexcept;
void __cdecl operator delete[](void*) noexcept;
using Exit=int(__cdecl*)();
extern "C" __declspec(dllimport) Exit __cdecl __dllonexit(Exit,Exit**,Exit**);
// _onexit uses the SDK CRT declaration already pulled in by Windows headers.
extern "C" __declspec(dllimport) void __cdecl _lock(int);
extern "C" __declspec(dllimport) void __cdecl _unlock(int);
extern "C" __declspec(dllimport) void __cdecl free(void*);
static BOOL attached=FALSE;static unsigned destroyed=0,exitOrder=0,processOrder=0,protectedCounter=0;
struct Node {unsigned value;explicit Node(unsigned n=621) noexcept:value(n){}~Node() noexcept{++destroyed;}};
static Node* nodes[16];
Node** volatile startupRelocationAnchor=nodes;static Exit* first=nullptr;static Exit* last=nullptr;
static int __cdecl CallbackA(){exitOrder=exitOrder*3+1;return 0;}
static int __cdecl CallbackB(){exitOrder=exitOrder*3+2;return 0;}
static int __cdecl ProcessA(){processOrder=processOrder*10+1;return 0;}
static int __cdecl ProcessB(){processOrder=processOrder*10+2;return 0;}
extern "C" BOOL WINAPI StartupEntry(HINSTANCE,DWORD reason,LPVOID){
 if(reason!=DLL_PROCESS_ATTACH)return TRUE;
 if(startupRelocationAnchor!=nodes)return FALSE;
 _lock(8);_lock(8);
 for(unsigned i=0;i<16;++i){nodes[i]=new Node(i*17);if(!nodes[i]||nodes[i]->value!=i*17)return FALSE;}
 for(unsigned i=0;i<64;++i){auto callback=i%2?CallbackB:CallbackA;if(__dllonexit(callback,&first,&last)!=callback)return FALSE;}
 _unlock(8);_unlock(8);
 if(_onexit(ProcessA)!=ProcessA||_onexit(ProcessB)!=ProcessB)return FALSE;
 attached=TRUE;return TRUE;
}
extern "C" __declspec(dllexport) BOOL WINAPI StartupAttached(){return attached;}
extern "C" __declspec(dllexport) unsigned WINAPI ProcessExitOrder(){return processOrder;}
extern "C" __declspec(dllexport) unsigned WINAPI ProtectedCount(){return protectedCounter;}
extern "C" __declspec(dllexport) void WINAPI AddUnderLock(){_lock(8);unsigned before=protectedCounter;Sleep(0);protectedCounter=before+1;_unlock(8);}
__declspec(noinline) static BOOL StackProbeCheck(){volatile unsigned char bytes[24576];for(unsigned i=0;i<sizeof(bytes);++i)bytes[i]=(unsigned char)(i*13);for(unsigned i=0;i<sizeof(bytes);++i)if(bytes[i]!=(unsigned char)(i*13))return FALSE;return TRUE;}
extern "C" __declspec(dllexport) int WINAPI StartupChecks(){
 if(!attached||last-first!=64)return 1;
 unsigned expected=0;for(unsigned i=64;i>0;--i){expected=expected*3+((i-1)%2?2:1);(*--last)();}if(exitOrder!=expected)return 2;
 free(first);first=last=nullptr;
 if(__dllonexit(CallbackA,nullptr,&last)!=nullptr)return 3;
 for(unsigned i=0;i<16;++i){if(nodes[i]->value!=i*17)return 4;delete nodes[i];nodes[i]=nullptr;}if(destroyed!=16)return 5;
 auto array=static_cast<unsigned*>(::operator new[](7*sizeof(unsigned)));for(unsigned i=0;i<7;++i)array[i]=621+i;for(unsigned i=0;i<7;++i)if(array[i]!=621+i)return 6;::operator delete[](array);if(destroyed!=16)return 7;
 auto empty=::operator new(0);if(!empty)return 8;::operator delete(empty);::operator delete(nullptr);
 CRITICAL_SECTION plain{},spin{};InitializeCriticalSection(&plain);if(!InitializeCriticalSectionAndSpinCount(&spin,4000))return 9;
 EnterCriticalSection(&plain);EnterCriticalSection(&plain);LeaveCriticalSection(&plain);LeaveCriticalSection(&plain);DeleteCriticalSection(&plain);DeleteCriticalSection(&spin);
 OSVERSIONINFOA version{};version.dwOSVersionInfoSize=sizeof(version);if(!GetVersionExA(&version)||!version.dwMajorVersion)return 10;
 auto heap=HeapCreate(0,0,0);if(!heap)return 11;auto buffer=(unsigned char*)HeapAlloc(heap,HEAP_ZERO_MEMORY,32);if(!buffer)return 12;
 for(unsigned i=0;i<32;++i)if(buffer[i])return 13;buffer[0]=0x62;buffer=(unsigned char*)HeapReAlloc(heap,0,buffer,256);if(!buffer||buffer[0]!=0x62)return 14;
 if(!HeapFree(heap,0,buffer)||!HeapDestroy(heap))return 15;
 DWORD slot=TlsAlloc();if(slot==TLS_OUT_OF_INDEXES||!TlsSetValue(slot,nodes)||TlsGetValue(slot)!=nodes||!TlsFree(slot))return 16;
 ULONGLONG before=GetTickCount64();Sleep(20);if(GetTickCount64()-before<1)return 17;
 if(!StackProbeCheck())return 18;
 BOOL caught=FALSE;__try{RaiseException(0xe0621001,0,0,nullptr);}__except(EXCEPTION_EXECUTE_HANDLER){caught=TRUE;}if(!caught)return 19;
 return 0;
}
