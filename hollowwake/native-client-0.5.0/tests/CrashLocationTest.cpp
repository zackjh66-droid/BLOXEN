#include "../Native/CrashLocation.h"
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
 std::cout<<observed<<candidates<<"\nPASS actual original-code access violation identifies module/RVA and access kind; bounded executable-address stack candidates include the actual null-call return location; unreadable stack fails closed; no filesystem path or raw memory contents collected. No Fortnite/Xbox execution.\n";
}
