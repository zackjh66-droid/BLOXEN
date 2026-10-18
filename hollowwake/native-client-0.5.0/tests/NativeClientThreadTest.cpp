#include "../Native/NativeClientThread.h"
#include <objbase.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <cstdint>
static std::atomic<DWORD> notifications{0};
static void NTAPI TlsNotify(void*,DWORD reason,void*){if(reason==DLL_THREAD_ATTACH)++notifications;}
#pragma section(".CRT$XLB",long,read)
extern "C" __declspec(allocate(".CRT$XLB")) const PIMAGE_TLS_CALLBACK HollowwakeOwnedTls=TlsNotify;
#pragma comment(linker,"/INCLUDE:_tls_used")
#pragma comment(linker,"/INCLUDE:HollowwakeOwnedTls")
static __declspec(thread) int localValue=17;
static DWORD caller=0,child=0;static HRESULT initial=E_FAIL,sta=E_FAIL,callerSta=E_FAIL;static APTTYPE apartment=APTTYPE_CURRENT;static APTTYPEQUALIFIER qualifier=APTTYPEQUALIFIER_NONE;static SIZE_T reserved=0;static bool tls=false;
static int __cdecl Entry(){
 child=GetCurrentThreadId();tls=localValue==17;localValue=88;
 initial=CoGetApartmentType(&apartment,&qualifier);
 sta=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 int marker=0;MEMORY_BASIC_INFORMATION info={};if(!VirtualQuery(&marker,&info,sizeof(info)))return -1;
 auto allocation=info.AllocationBase;auto position=static_cast<BYTE*>(allocation);
 for(unsigned i=0;i<2048;++i){MEMORY_BASIC_INFORMATION region={};if(!VirtualQuery(position,&region,sizeof(region))||region.AllocationBase!=allocation)break;reserved+=region.RegionSize;position+=region.RegionSize;}
 if(SUCCEEDED(sta))CoUninitialize();
 return child!=caller&&tls&&(initial==CO_E_NOTINITIALIZED||(initial==S_OK&&apartment==APTTYPE_MTA&&qualifier==APTTYPEQUALIFIER_IMPLICIT_MTA))&&sta==S_OK&&reserved==1048576&&notifications.load()>0?621:-2;
}
static int __cdecl EarlyExit(){ExitThread(77);return 0;}
static void Log(const std::string& value){std::cout<<value<<"\n";}
int main(int argc,char** argv){
 if(argc!=2)return 10;caller=GetCurrentThreadId();localValue=99;
 auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(initialized))return 11;
 callerSta=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(callerSta!=RPC_E_CHANGED_MODE)return 13;
 int result=-1;auto hr=RunNativeClientThread(Entry,1048576,&result,Log);
 int earlyResult=123;bool early=RunNativeClientThread(EarlyExit,1048576,&earlyResult,Log)==HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED)&&earlyResult==123;
 bool invalid=RunNativeClientThread(Entry,1,&result,nullptr)==E_INVALIDARG;
 CoUninitialize();
 bool passed=SUCCEEDED(hr)&&result==621&&localValue==99&&invalid&&early;
 std::ofstream out(argv[1]);out<<"{\"scope\":\"Original native entry-thread fixture, no Fortnite or Xbox execution\",\"passed\":"<<(passed?"true":"false")<<",\"hresult\":"<<(unsigned long)hr<<",\"entryResult\":"<<result<<",\"initialComResult\":"<<(unsigned long)initial<<",\"initialApartment\":"<<(int)apartment<<",\"initialQualifier\":"<<(int)qualifier<<",\"callerStaResult\":"<<(unsigned long)callerSta<<",\"staResult\":"<<(unsigned long)sta<<",\"reservedStackBytes\":"<<reserved<<",\"tlsThreadNotifications\":"<<notifications.load()<<",\"separateTlsValue\":"<<(tls&&localValue==99?"true":"false")<<",\"earlyThreadExitRejected\":"<<(early?"true":"false")<<",\"invalidReservationRejected\":"<<(invalid?"true":"false")<<"}";
 return passed?0:12;
}
