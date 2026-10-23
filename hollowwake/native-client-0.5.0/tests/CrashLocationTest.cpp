#include "../Native/CrashLocation.h"
#include "../Native/ClientTransferFailure.h"
#include <iostream>
#include <stdexcept>
#include <intrin.h>
static std::string observed,candidates;
static int Capture(EXCEPTION_POINTERS* value){observed=Hollowwake::DescribeCrash(*value->ExceptionRecord,GetModuleHandleExW,GetModuleFileNameW);candidates=Hollowwake::DescribeCrashStackCandidates(value->ContextRecord,GetModuleHandleExW,GetModuleFileNameW,ReadProcessMemory,VirtualQuery);return EXCEPTION_EXECUTE_HANDLER;}
static volatile ULONG_PTR ownedTarget=0;
__declspec(noinline) static void FailOwnedCode(){volatile int* pointer=reinterpret_cast<volatile int*>(ownedTarget);*pointer=621;}
static void Run(){__try{FailOwnedCode();}__except(Capture(GetExceptionInformation())){}}
__declspec(noinline) static void CallOwnedNull(){auto call=reinterpret_cast<void(*)()>(ownedTarget);call();_ReadWriteBarrier();}
static void RunNullCall(){__try{CallOwnedNull();}__except(Capture(GetExceptionInformation())){}}
int main(){
 Run();
 if(observed.find("NATIVE_CRASH code=3221225477")==std::string::npos||observed.find("caller=CrashLocationTest.exe RVA=")==std::string::npos||observed.find("access=write target=0")==std::string::npos||observed.find(":\\")!=std::string::npos)return 1;
 EXCEPTION_RECORD absent={};absent.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION;absent.ExceptionAddress=reinterpret_cast<void*>(1);
 auto unknown=Hollowwake::DescribeCrash(absent,nullptr,nullptr);if(unknown.find("RVA=")!=std::string::npos||unknown.find("caller=")!=std::string::npos)return 2;
 RunNullCall();if(observed.find("access=execute target=0")==std::string::npos||candidates.find("stack_index=0 caller=CrashLocationTest.exe RVA=")==std::string::npos||candidates.find(":\\")!=std::string::npos)return 3;
 CONTEXT context={};context.Rsp=1;if(!Hollowwake::DescribeCrashStackCandidates(&context,GetModuleHandleExW,GetModuleFileNameW,ReadProcessMemory,VirtualQuery).empty())return 4;
 BYTE instruction[]={0xff,0x64,0x24,0xf8};ULONG_PTR target=reinterpret_cast<ULONG_PTR>(&CallOwnedNull);
 CONTEXT branch={};branch.Rip=reinterpret_cast<DWORD64>(instruction);branch.Rsp=reinterpret_cast<DWORD64>(&target)+8;
 auto control=Hollowwake::DescribeCrashControlTransfer(&branch,GetModuleHandleExW,GetModuleFileNameW,ReadProcessMemory,VirtualQuery);
 if(control.find("instruction=jmp_stack_minus_8")==std::string::npos||control.find("canonical=true caller=CrashLocationTest.exe RVA=")==std::string::npos||control.find(":\\")!=std::string::npos)return 5;
 target=0x1234567890abcdefULL;control=Hollowwake::DescribeCrashControlTransfer(&branch,GetModuleHandleExW,GetModuleFileNameW,ReadProcessMemory,VirtualQuery);
 if(control.find("canonical=false")==std::string::npos||control.find("caller=")!=std::string::npos)return 6;
 instruction[0]=0x90;if(!Hollowwake::DescribeCrashControlTransfer(&branch,GetModuleHandleExW,GetModuleFileNameW,ReadProcessMemory,VirtualQuery).empty())return 7;
 instruction[0]=0xc3;branch.Rsp=1;if(!Hollowwake::DescribeCrashControlTransfer(&branch,GetModuleHandleExW,GetModuleFileNameW,ReadProcessMemory,VirtualQuery).empty())return 8;
 // An owned, nonexecuted image/frame models only the retained post-fault
 // evidence. Exercise real bounded process-memory reads, refusal of an
 // unproven frame, and signed/out-of-range index handling.
 auto image=static_cast<BYTE*>(VirtualAlloc(nullptr,0x6314000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!image)return 9;
 const BYTE helper[]={0x48,0x8b,0x45,0x38,0x48,0x03,0x05,0xfc,0x52,0x3b,0xfb};
 const BYTE tail[]={0xff,0x64,0x24,0xf8};memcpy(image+0x6097501,helper,sizeof(helper));memcpy(image+0x609753b,tail,sizeof(tail));
 *reinterpret_cast<ULONG_PTR*>(image+0x144c808)=0x141775ae0ULL;
 ULONG_PTR frame[18]={};DWORD left[]={0,621},right[]={0,620};
 auto base=reinterpret_cast<ULONG_PTR>(image),failed=base+0x141775ae0ULL;
 frame[0]=frame[17]=failed;frame[7]=base;frame[9]=reinterpret_cast<ULONG_PTR>(left);frame[10]=reinterpret_cast<ULONG_PTR>(right);frame[14]=1;
 CONTEXT actual={};actual.Rip=failed;actual.Rsp=reinterpret_cast<ULONG_PTR>(frame)+sizeof(frame);
 EXCEPTION_RECORD av={};av.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;av.NumberParameters=2;av.ExceptionInformation[0]=8;av.ExceptionInformation[1]=failed;
 EXCEPTION_POINTERS fault={&av,&actual};
 auto describe=[&](){return Hollowwake::DescribeClientTransferFailure(&fault,reinterpret_cast<HMODULE>(image),0x6314000,GetModuleHandleExW,GetModuleFileNameW,ReadProcessMemory);};
 auto measured=describe();if(measured.find("observedAfterFault=true debuggerRequired=false")==std::string::npos||measured.find("leftValue=621 rightValue=620")==std::string::npos||measured.find(":\\")!=std::string::npos)return 10;
 frame[0]=0;if(!describe().empty())return 11;frame[0]=failed;
 frame[14]=static_cast<DWORD>(-1);if(describe().find("indexedValuesReadable=false")==std::string::npos)return 12;
 frame[14]=65536;if(describe().find("indexedValuesReadable=false")==std::string::npos)return 13;
 frame[14]=1;image[0x609753b]=0x90;if(!describe().empty())return 14;
 VirtualFree(image,0,MEM_RELEASE);
 // Obtain the real Windows exception form for a noncanonical indirect call.
 // The actual CPU raises #GP at our own instruction, not execute-AV at target.
 ownedTarget=0x800000000000ULL;RunNullCall();ownedTarget=0;
 if(observed.find("access=read target=18446744073709551615")==std::string::npos||observed.find("caller=CrashLocationTest.exe RVA=")==std::string::npos)return 15;
 // Model the exact retained high-ASLR image/frame using real owned mappings.
 // The requested address is part of normal 128-TB x64 user space; the image
 // itself remains entirely below the boundary, while its bad target crosses it.
 image=nullptr;for(auto address:{0x7fff00000000ULL,0x7fff20000000ULL,0x7fff40000000ULL,0x7fff60000000ULL}){
  image=static_cast<BYTE*>(VirtualAlloc(reinterpret_cast<void*>(address),0x6314000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(image)break;
 }
 if(!image)return 16;
 memcpy(image+0x6097501,helper,sizeof(helper));memcpy(image+0x609753b,tail,sizeof(tail));*reinterpret_cast<ULONG_PTR*>(image+0x144c808)=0x141775ae0ULL;
 base=reinterpret_cast<ULONG_PTR>(image);failed=base+0x141775ae0ULL;
 if((failed>>47)==0||(failed>>47)==0x1ffff)return 17;
 frame[0]=frame[17]=failed;frame[7]=base;frame[3]=0x140000000ULL;frame[8]=base+0x300;
 *reinterpret_cast<DWORD*>(image+0x300)=0x200;*reinterpret_cast<ULONG_PTR*>(image+0x200)=base;
 actual.Rip=base+0x609753b;av.ExceptionInformation[0]=0;av.ExceptionInformation[1]=~ULONG_PTR(0);
 measured=describe();if(measured.find("canonical=false")==std::string::npos||measured.find("leftValue=621 rightValue=620")==std::string::npos||measured.find("retainedSourceEntryRva=768 retainedSourceRva=512 retainedNormalization=preferred retainedSourcePointerRva=0")==std::string::npos)return 18;
 av.ExceptionInformation[0]=8;if(!describe().empty())return 19;av.ExceptionInformation[0]=0;
 actual.Rip++;if(!describe().empty())return 20;actual.Rip--;
 frame[0]--;if(!describe().empty())return 21;frame[0]++;
 frame[8]=base+0x6314000-3;if(describe().find("retainedSourceRva=")!=std::string::npos)return 22;
 frame[8]=base+0x300;*reinterpret_cast<DWORD*>(image+0x300)=0x6314000-7;if(describe().find("retainedSourceRva=")!=std::string::npos)return 23;
 VirtualFree(image,0,MEM_RELEASE);
 std::cout<<observed<<candidates<<"\nPASS actual original-code access violation identifies module/RVA and access kind; bounded executable-address stack candidates include the actual null-call return location; actual noncanonical indirect-call exception and high-address retained frame are recognized; mismatched fault forms, stack targets and out-of-image producer fields are rejected; unreadable stack fails closed; no filesystem path or raw memory contents collected. No Fortnite/Xbox execution.\n";
}
