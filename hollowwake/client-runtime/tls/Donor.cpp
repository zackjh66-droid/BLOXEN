// Original Hollowwake static-TLS donor. Compiled without CRT; no upstream code.
// One OS-loaded donor DLL is required PER guest module. Do not unload while bound.
#include <windows.h>
#include <intrin.h>
extern "C" {
DWORD _tls_index=0;
#pragma section(".tls",long,read,write)
__declspec(allocate(".tls")) unsigned char donorTemplate[65537]={};
extern const IMAGE_TLS_DIRECTORY64 _tls_used;
}
#pragma section(".rdata$T",read)
static PIMAGE_TLS_CALLBACK callbacks[]={nullptr};
extern "C" __declspec(allocate(".rdata$T")) const IMAGE_TLS_DIRECTORY64 _tls_used={
 (ULONGLONG)donorTemplate,(ULONGLONG)(donorTemplate+sizeof(donorTemplate)),(ULONGLONG)&_tls_index,(ULONGLONG)callbacks,0,0};
static unsigned char initial[65536];static SIZE_T initialSize,zeroSize;static void* guestBase;
static PIMAGE_TLS_CALLBACK guestCallbacks[16];static unsigned guestCallbackCount;
static volatile LONG state=0; // 0 unbound, 1 configuring, 2 bound; process-lifetime binding
// x64 Windows compiler ABI: GS:[0x58] points to the static TLS vector.
// Xbox ABI/policy still requires hardware validation; no arbitrary TEB writes.
static unsigned char* CurrentBlock(){return static_cast<unsigned char**>((void*)__readgsqword(0x58))[_tls_index];}
static void Seed(){
 auto block=CurrentBlock();
 for(SIZE_T i=0;i<initialSize;++i)block[i]=initial[i];
 for(SIZE_T i=initialSize;i<initialSize+zeroSize;++i)block[i]=0;
 block[65536]=1;
}
extern "C" __declspec(dllexport) BOOL WINAPI InitializeGuestThread(){
 if(state!=2)return FALSE;
 if(!CurrentBlock()[65536]){Seed();for(unsigned i=0;i<guestCallbackCount;++i)guestCallbacks[i](guestBase,DLL_THREAD_ATTACH,nullptr);}
 return TRUE;
}
extern "C" __declspec(dllexport) BOOL WINAPI BindGuest(const void* bytes,SIZE_T size,SIZE_T zero,void* base,const PIMAGE_TLS_CALLBACK* cb,unsigned count){
 if(size>65536||zero>65536-size||(!bytes&&size)||count>16||(!cb&&count))return FALSE;
 if(InterlockedCompareExchange(&state,1,0)!=0)return FALSE;
 for(SIZE_T i=0;i<size;++i)initial[i]=static_cast<const unsigned char*>(bytes)[i];
 initialSize=size;zeroSize=zero;guestBase=base;guestCallbackCount=count;
 for(unsigned i=0;i<count;++i)guestCallbacks[i]=cb[i];
 Seed();InterlockedExchange(&state,2);
 // Guest PROCESS_ATTACH must be invoked by the mapper after all imports and TLS
 // slots are bound, NOT here while other modules might still be incomplete.
 return TRUE;
}
extern "C" __declspec(dllexport) DWORD WINAPI GuestTlsIndex(){return _tls_index;}
extern "C" __declspec(dllexport) void* WINAPI GuestThreadBlock(){return state==2&&CurrentBlock()[65536]?CurrentBlock():nullptr;}
extern "C" BOOL WINAPI DonorEntry(HINSTANCE,DWORD reason,LPVOID){
 if(state==2){
  if(reason==DLL_THREAD_ATTACH)InitializeGuestThread();
  else if(reason==DLL_THREAD_DETACH&&CurrentBlock()[65536])
   for(unsigned i=0;i<guestCallbackCount;++i)guestCallbacks[i](guestBase,DLL_THREAD_DETACH,nullptr);
 }
 return TRUE;
}
