// Original fixtures and bounded data-only test of the reviewed candidate TLS routine.
// Full runner, game images and mapped executable entrypoints are NEVER executed.
#include <windows.h>
#include <cstdint>
#include <fstream>
#include <vector>
#include <iostream>
#include <thread>
#include "TLSProbe.generated.h"
static bool require(bool value,const char* name){std::cout<<name<<": "<<(value?"PASS":"FAIL")<<"\n";return value;}
int main(){
 bool ok=true;HMODULE tls=LoadLibraryW(L"tls_fixture.dll"),runtime=LoadLibraryW(L"runtime_fixture.dll");
 if(!require(tls&&runtime,"Original fixture DLLs load through Windows loader"))return 1;
 auto read=(unsigned long(*)())GetProcAddress(tls,"FixtureRead");auto set=(void(*)(unsigned long))GetProcAddress(tls,"FixtureSet");
 if(!read||!set)return 2;
 std::cout<<"Observed initialized TLS value: "<<std::hex<<read()<<std::dec<<"\n";ok&=require(read()==0x62145269,"OS loader copies initialized TLS template");set(10);unsigned long threadValue=0,afterThread=0;
 std::thread thread([&](){threadValue=read();set(20);afterThread=read();});thread.join();std::cout<<"TLS worker initial/set/main: "<<std::hex<<threadValue<<" / "<<afterThread<<" / "<<read()<<std::dec<<"\n";ok&=require(threadValue==0x62145269&&afterThread==20&&read()==10,"TLS initialized separately on another thread");
 auto import=(DWORD(*)())GetProcAddress(runtime,"FixtureImport");auto exception=(int(*)())GetProcAddress(runtime,"FixtureException");auto delay=(int(*)())GetProcAddress(runtime,"FixtureDelay");
 ok&=require(import&&import()!=0,"Normal Windows import resolves");ok&=require(exception&&exception()==621,"Native C++ throw/catch baseline");ok&=require(delay&&delay()==1,"Delay-loaded version.dll import resolves");
 std::ifstream file("tls_fixture.dll",std::ios::binary);std::vector<unsigned char> raw((std::istreambuf_iterator<char>(file)),{});auto dos=(IMAGE_DOS_HEADER*)raw.data();auto nt=(IMAGE_NT_HEADERS64*)(raw.data()+dos->e_lfanew);
 const auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];auto sections=IMAGE_FIRST_SECTION(nt);DWORD offset=0;
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++)if(dir.VirtualAddress>=sections[i].VirtualAddress&&dir.VirtualAddress<sections[i].VirtualAddress+sections[i].Misc.VirtualSize)offset=sections[i].PointerToRawData+dir.VirtualAddress-sections[i].VirtualAddress;
 if(!require(offset&&dir.Size>=sizeof(IMAGE_TLS_DIRECTORY64),"Original fixture has a real PE TLS directory"))return 3;
 auto real=(IMAGE_TLS_DIRECTORY64*)(raw.data()+offset);ok&=require(real->EndAddressOfRawData>real->StartAddressOfRawData,"Fixture has initialized static TLS template");
 // A non-executable buffer models the post-relocation TLS directory with its OPTIONAL callback array omitted. No fixture code is called through the candidate.
 std::vector<unsigned char> image(4096);auto data=(IMAGE_TLS_DIRECTORY64*)(image.data()+256);*data=*real;DWORD marker=0xCCCCCCCC;data->AddressOfIndex=(ULONGLONG)&marker;data->AddressOfCallBacks=0;
 xwr::PeLoader candidate;bool returned=candidate.ProcessTls(image.data(),256,sizeof(*data),IMAGE_NT_OPTIONAL_HDR64_MAGIC);
 bool defect=returned&&marker==0xCCCCCCCC;ok&=require(defect,"Candidate negative finding reproduced: reports success but does not initialize even TLS index without callbacks");
 // A useful guard rejects this unsupported contract instead of attempting the full game.
 bool failClosed=(data->EndAddressOfRawData>data->StartAddressOfRawData||data->SizeOfZeroFill>0);ok&=require(failClosed,"Original preflight rejects unsupported static TLS before launch");
 std::ofstream report("fixture-results.json");report<<"{\"originalWindowsBaselinesPassed\":"<<(ok?"true":"false")<<",\"scopedCandidateTLSRoutineTested\":true,\"candidateTLSDefectReproduced\":"<<(defect?"true":"false")<<",\"fullCandidateLoaderExecuted\":false,\"fixtureEntryPointExecutedThroughCandidate\":false,\"candidateGraphicsProven\":false,\"xboxExecution\":false,\"fortniteExecuted\":false}";
 FreeLibrary(runtime);FreeLibrary(tls);return ok?0:4;
}
