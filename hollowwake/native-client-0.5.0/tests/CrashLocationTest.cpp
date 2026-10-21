#include "../Native/CrashLocation.h"
#include <iostream>
#include <stdexcept>
static std::string observed;
static int Capture(EXCEPTION_POINTERS* value){observed=Hollowwake::DescribeCrash(*value->ExceptionRecord,GetModuleHandleExW,GetModuleFileNameW);return EXCEPTION_EXECUTE_HANDLER;}
static volatile ULONG_PTR ownedTarget=0;
__declspec(noinline) static void FailOwnedCode(){volatile int* pointer=reinterpret_cast<volatile int*>(ownedTarget);*pointer=621;}
static void Run(){__try{FailOwnedCode();}__except(Capture(GetExceptionInformation())){}}
int main(){
 Run();
 if(observed.find("NATIVE_CRASH code=3221225477")==std::string::npos||observed.find("caller=CrashLocationTest.exe RVA=")==std::string::npos||observed.find("access=write target=0")==std::string::npos||observed.find(":\\")!=std::string::npos)return 1;
 EXCEPTION_RECORD absent={};absent.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION;absent.ExceptionAddress=reinterpret_cast<void*>(1);
 auto unknown=Hollowwake::DescribeCrash(absent,nullptr,nullptr);if(unknown.find("RVA=")!=std::string::npos||unknown.find("caller=")!=std::string::npos)return 2;
 std::cout<<observed<<"\nPASS actual original-code access violation identifies module/RVA and access kind; no filesystem path, stack or memory contents collected. No Fortnite/Xbox execution.\n";
}
