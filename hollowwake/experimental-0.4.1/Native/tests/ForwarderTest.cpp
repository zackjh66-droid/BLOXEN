#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include "../Dependencies.h"
#include "../ForwardOnly.h"
static unsigned metadataCount=0;
static void Log(const std::string& line){std::cout<<line<<std::endl;if(line.find("DEP forward-only metadata verified")!=std::string::npos)++metadataCount;}
static void* Resolve(const char* dll,const char* name){return ResolveDependency(dll,name);}
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
 Check(InitializeDependencies(),"Original fixture initialization graph failed");
 Check(reinterpret_cast<int(*)()>(first)()==621,"Relocated original fixture target did not execute correctly");
 auto bad=ResolveDependency("FixtureFixedCode.dll","Answer");Check(!bad&&DependencyFailures()==1,"Non-relocatable executable fixture must still be rejected");
 std::cout<<"PASS: real production dependency resolver, two colliding non-relocatable forward-only DLLs, name+ordinal forwarding, mapped original target returned621; non-relocatable executable rejected"<<std::endl;
 if(reservation)VirtualFree(reservation,0,MEM_RELEASE);return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
