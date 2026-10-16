#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include "../Dependencies.h"
#include "../ForwardOnly.h"
#include "../KernelClock.h"
#include "../LegacyCrt.h"
static unsigned metadataCount=0;
static void Log(const std::string& line){std::cout<<line<<std::endl;if(line.find("DEP forward-only metadata verified")!=std::string::npos)++metadataCount;}
static void* Resolve(const char* dll,const char* name){if(auto memory=Hollowwake::ResolveLegacyCrtImport(dll,name))return memory;if(auto clock=Hollowwake::ResolveClockImport(dll,name))return clock;return ResolveDependency(dll,name);}
static void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){try{
 Check(argc==2,"Fixture directory required");
 // Occupy the common fixed ImageBase so neither export-only fixture can land
 // there. The exact production mapper must resolve both at different addresses.
 auto reservation=VirtualAlloc(reinterpret_cast<void*>(0x180000000ULL),65536,MEM_RESERVE,PAGE_NOACCESS);
 MEMORY_BASIC_INFORMATION region{};VirtualQuery(reinterpret_cast<void*>(0x180000000ULL),&region,sizeof(region));Check(region.State!=MEM_FREE,"Preferred-base collision was not established");
 ConfigureDependencies(argv[1],Resolve,Log);
 auto first=ResolveDependency("FixtureFirst.dll","Answer");auto second=ResolveDependency("FixtureSecond.dll","#1");
 Check(first&&second&&first==second,"Forwarded name/ordinal did not resolve to the same real target");
 Check(DependencyFailures()==0&&metadataCount==2,"Unexpected production-loader blocker or metadata count");
 auto clock=ResolveDependency("FixtureClock.dll","ReadTick");auto attach=ResolveDependency("FixtureClock.dll","ReadAttachTick");auto wasAttached=ResolveDependency("FixtureClock.dll","WasAttached");
 Check(clock&&attach&&wasAttached,"Clock DLL exports were not resolved");
 Check(!reinterpret_cast<BOOL(WINAPI*)()>(wasAttached)(),"Clock fixture initialized before the explicit graph initialization");
 auto memoryCheck=ResolveDependency("FixtureMemory.dll","RunMemoryChecks");auto memoryAttached=ResolveDependency("FixtureMemory.dll","MemoryWasAttached");
 Check(memoryCheck&&memoryAttached&&DependencyFailures()==0,"Legacy memory fixture imports were not resolved");
 Check(!reinterpret_cast<BOOL(WINAPI*)()>(memoryAttached)(),"Memory fixture initialized before explicit process attach");
 Check(InitializeDependencies(),"Original fixture initialization graph failed");
 Check(reinterpret_cast<BOOL(WINAPI*)()>(memoryAttached)(),"Mapped allocator fixture process attach did not run");
 std::cout<<"INITIALIZERS mapped DLL_PROCESS_ATTACH invoked msvcrt!_initterm/_initterm_e; null skipping, ordered callbacks, empty tables, first-error short circuit PASS"<<std::endl;
 auto memoryResult=reinterpret_cast<int(WINAPI*)()>(memoryCheck)();Check(memoryResult==0,("Mapped allocator checks failed: "+std::to_string(memoryResult)).c_str());
 Check(Hollowwake::LegacyAllocatorContract(),"Production allocation-family contract failed");
 Check(!Hollowwake::ResolveLegacyCrtImport("ucrtbase.dll","malloc")&&!Hollowwake::ResolveLegacyCrtImport("msvcrt.dll","_iob"),"Allocator bridge must not alias unrelated CRT or legacy FILE layouts");
 std::cout<<"ALLOCATOR mapped DLL_PROCESS_ATTACH called msvcrt!malloc; allocation/free/grow/zero/overlap/duplication/overflow/errno/preserved-buffer checks PASS"<<std::endl;

 Check(reinterpret_cast<BOOL(WINAPI*)()>(wasAttached)(),"Mapped DLL_PROCESS_ATTACH did not run");
 auto tick=reinterpret_cast<DWORD(WINAPI*)()>(clock);auto attachedTick=reinterpret_cast<DWORD(WINAPI*)()>(attach)();
 auto begin=tick();Sleep(40);auto end=tick();auto elapsed=(DWORD)(end-begin);auto os=Hollowwake::TickCountLow32(GetTickCount64());
 Check(elapsed>0&&elapsed<5000,"Mapped GetTickCount did not advance with elapsed time");
 Check((DWORD)(os-end)<5000&&(DWORD)(end-attachedTick)<5000,"Mapped clock did not use real OS boot-relative uptime");
 Check(Hollowwake::TickCountLow32(0x100000000ULL)==0&&Hollowwake::TickCountLow32(0x100000010ULL)==16&&(DWORD)(Hollowwake::TickCountLow32(0x100000010ULL)-Hollowwake::TickCountLow32(0xfffffff0ULL))==32,"GetTickCount wrap semantics failed");
 std::cout<<"CLOCK mapped DLL_PROCESS_ATTACH called production GetTickCount; elapsed="<<elapsed<<"ms; boot-relative and 32-bit wrap checks PASS"<<std::endl;

 Check(reinterpret_cast<int(*)()>(first)()==621,"Relocated original fixture target did not execute correctly");
 auto bad=ResolveDependency("FixtureFixedCode.dll","Answer");Check(!bad&&DependencyFailures()==1,"Non-relocatable executable fixture must still be rejected");
 std::cout<<"PASS: real production dependency resolver, two colliding non-relocatable forward-only DLLs, name+ordinal forwarding, mapped original target returned621; non-relocatable executable rejected"<<std::endl;
 if(reservation)VirtualFree(reservation,0,MEM_RELEASE);return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
