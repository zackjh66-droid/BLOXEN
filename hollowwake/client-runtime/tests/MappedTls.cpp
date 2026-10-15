// Original, bounded mapper ONLY for our no-import TLS fixture. NOT a game loader.
#include <windows.h>
#include <fstream>
#include <vector>
#include <thread>
#include <future>
#include <iostream>
#include <cstring>
#include <bcrypt.h>
#include "GuestHash.generated.h"
using Bind=BOOL(WINAPI*)(const void*,SIZE_T,SIZE_T,void*,const PIMAGE_TLS_CALLBACK*,unsigned);
using Index=DWORD(WINAPI*)();using Init=BOOL(WINAPI*)();using Block=void*(WINAPI*)();
static volatile LONG attaches=0,detaches=0;
static void NTAPI ThreadCallback(void*,DWORD reason,void*){if(reason==DLL_THREAD_ATTACH)InterlockedIncrement(&attaches);if(reason==DLL_THREAD_DETACH)InterlockedIncrement(&detaches);}
static bool Check(bool v,const char* name){std::cout<<name<<": "<<(v?"PASS":"FAIL")<<"\n";return v;}
static void* Export(BYTE* base,const char* name){
 auto n=(IMAGE_NT_HEADERS64*)(base+((IMAGE_DOS_HEADER*)base)->e_lfanew);
 auto e=(IMAGE_EXPORT_DIRECTORY*)(base+n->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
 auto names=(DWORD*)(base+e->AddressOfNames);auto ord=(WORD*)(base+e->AddressOfNameOrdinals);auto funcs=(DWORD*)(base+e->AddressOfFunctions);
 for(DWORD i=0;i<e->NumberOfNames;++i)if(!strcmp((char*)base+names[i],name))return base+funcs[ord[i]];return nullptr;
}
__declspec(thread) volatile DWORD hostTlsMarker=0xABC621;
static LONG CALLBACK Crash(EXCEPTION_POINTERS* e){HMODULE m=nullptr;wchar_t path[512]={};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)e->ContextRecord->Rip,&m);GetModuleFileNameW(m,path,512);std::wcerr<<"Fault module="<<path<<" offset="<<std::hex<<(e->ContextRecord->Rip-(DWORD64)m)<<std::dec<<"\n";std::cerr<<"Crash code="<<std::hex<<e->ExceptionRecord->ExceptionCode<<" RIP="<<e->ContextRecord->Rip<<" address="<<e->ExceptionRecord->ExceptionInformation[1]<<std::dec<<"\n";return EXCEPTION_CONTINUE_SEARCH;}
int main(){
 std::cout<<std::unitbuf;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);AddVectoredExceptionHandler(1,Crash);
 bool ok=true;HMODULE donor=LoadLibraryW(L"HollowwakeTlsDonor.dll");if(!Check(donor!=nullptr,"OS loads original donor"))return 1;
 std::cout<<"Host TLS marker before bind="<<std::hex<<hostTlsMarker<<std::dec<<"\n";
 auto attach=(Index)GetProcAddress(donor,"DonorAttachThread");std::cout<<"Donor PROCESS_ATTACH observed thread="<<(attach?attach():0)<<"\n";
 auto bind=(Bind)GetProcAddress(donor,"BindGuest");auto index=(Index)GetProcAddress(donor,"GuestTlsIndex");auto init=(Init)GetProcAddress(donor,"InitializeGuestThread");auto block=(Block)GetProcAddress(donor,"GuestThreadBlock");if(!bind||!index||!init||!block)return 2;
 unsigned long(*read)()=nullptr;void(*write)(unsigned long)=nullptr;
 std::promise<void> ready,release;auto readyWait=ready.get_future();auto releaseWait=release.get_future();bool existingGuard=false;unsigned long existingInitial=0;
 
 std::ifstream file("GuestTls.dll",std::ios::binary|std::ios::ate);auto length=file.tellg();if(length<1024||length>1024*1024)return 3;file.seekg(0);std::vector<BYTE> raw((size_t)length);if(!file.read((char*)raw.data(),raw.size()))return 3;
 BCRYPT_ALG_HANDLE alg=nullptr;BYTE digest[32];if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return 3;auto status=BCryptHash(alg,nullptr,0,raw.data(),(ULONG)raw.size(),digest,sizeof(digest));BCryptCloseAlgorithmProvider(alg,0);if(status<0||memcmp(digest,ExpectedGuestSha256,32)){std::cerr<<"Original fixture hash mismatch: refusing to map\n";return 3;}
 if(raw.size()<1024)return 3;auto dos=(IMAGE_DOS_HEADER*)raw.data();if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||(size_t)dos->e_lfanew>raw.size()-sizeof(IMAGE_NT_HEADERS64))return 3;
 auto nt=(IMAGE_NT_HEADERS64*)(raw.data()+dos->e_lfanew);if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.SizeOfImage>1024*1024||nt->FileHeader.NumberOfSections>32)return 3;
 // The private fixture is hash-recorded by the build. No arbitrary image accepted.
 if(nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size||nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size)return 4;
 std::thread existing([&](){ready.set_value();releaseWait.wait();existingGuard=block()==nullptr&&init()!=0;existingInitial=read();write(30);});readyWait.wait();
 std::cout<<"Mapping original fixture\n";auto base=(BYTE*)VirtualAlloc(nullptr,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!base)return 5;
 memcpy(base,raw.data(),nt->OptionalHeader.SizeOfHeaders);auto sec=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){if(sec[i].PointerToRawData+sec[i].SizeOfRawData>raw.size()||sec[i].VirtualAddress+sec[i].SizeOfRawData>nt->OptionalHeader.SizeOfImage)return 6;memcpy(base+sec[i].VirtualAddress,raw.data()+sec[i].PointerToRawData,sec[i].SizeOfRawData);}
 INT64 delta=(INT64)base-(INT64)nt->OptionalHeader.ImageBase;auto rd=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
 if(delta&&!rd.Size)return 7;
 for(DWORD used=0;used<rd.Size;){auto b=(IMAGE_BASE_RELOCATION*)(base+rd.VirtualAddress+used);if(b->SizeOfBlock<8||b->SizeOfBlock>rd.Size-used)return 8;auto entries=(WORD*)(b+1);for(DWORD j=0;j<(b->SizeOfBlock-8)/2;++j){WORD type=entries[j]>>12;if(type==IMAGE_REL_BASED_DIR64)*(ULONGLONG*)(base+b->VirtualAddress+(entries[j]&4095))+=delta;else if(type!=IMAGE_REL_BASED_ABSOLUTE)return 9;}used+=b->SizeOfBlock;}
 std::cout<<"Mapped image base="<<(void*)base<<"\n";auto tls=(IMAGE_TLS_DIRECTORY64*)(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
 SIZE_T tlsSize=tls->EndAddressOfRawData-tls->StartAddressOfRawData;
 ok&=Check(tlsSize>=4&&tlsSize<=32,"Mapped fixture has initialized static TLS");
 auto host=(BYTE*)GetModuleHandleW(nullptr);auto hn=(IMAGE_NT_HEADERS64*)(host+((IMAGE_DOS_HEADER*)host)->e_lfanew);auto ht=(IMAGE_TLS_DIRECTORY64*)(host+hn->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);DWORD hostIndex=*(DWORD*)ht->AddressOfIndex;
 std::cout<<"Donor index="<<index()<<"; host index="<<hostIndex<<"\n";if(!Check(index()!=hostIndex,"OS assigns a noncolliding donor TLS index"))return 13;PIMAGE_TLS_CALLBACK callback=&ThreadCallback;
 ok&=Check(!bind(nullptr,65537,0,base,nullptr,0),"Donor rejects oversized template");
 ok&=Check(bind((void*)tls->StartAddressOfRawData,tlsSize,16,base,&callback,1)!=0,"Donor binds template");
 *(DWORD*)tls->AddressOfIndex=index();
 ok&=Check(!bind((void*)tls->StartAddressOfRawData,tlsSize,0,base,nullptr,0),"Donor rejects unsafe rebinding");
 DWORD old;VirtualProtect(base,nt->OptionalHeader.SizeOfHeaders,PAGE_READONLY,&old);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){DWORD protect=(sec[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)?PAGE_EXECUTE_READ:((sec[i].Characteristics&IMAGE_SCN_MEM_WRITE)?PAGE_READWRITE:PAGE_READONLY);if((sec[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)&&(sec[i].Characteristics&IMAGE_SCN_MEM_WRITE))return 10;if(!VirtualProtect(base+sec[i].VirtualAddress,sec[i].Misc.VirtualSize,protect,&old))return 10;}
 FlushInstructionCache(GetCurrentProcess(),base,nt->OptionalHeader.SizeOfImage);
 std::cout<<"Applying memory protections complete\n";read=(unsigned long(*)())Export(base,"GuestRead");write=(void(*)(unsigned long))Export(base,"GuestWrite");if(!read||!write)return 11;
 ok&=Check(read()==0x62145269,"Mapped guest instructions read initialized donor TLS");write(10);ok&=Check(hostTlsMarker==0xABC621,"Guest binding does not corrupt host TLS");
 bool zeroFilled=true;auto bytes=(unsigned char*)block();for(SIZE_T i=tlsSize;i<tlsSize+16;++i)zeroFilled&=bytes[i]==0;
 ok&=Check(zeroFilled,"Donor zero-fill is provisioned in the actual TLS block");
 release.set_value();existing.join();ok&=Check(existingGuard&&existingInitial==0x62145269&&read()==10,"Existing host thread guard initializes mapped guest TLS without changing main thread");
 unsigned long initial=0,changed=0;std::thread t([&](){initial=read();write(20);changed=read();});t.join();
 ok&=Check(attaches==2&&detaches==2,"Original thread callbacks receive balanced attach/detach");
 ok&=Check(initial==0x62145269&&changed==20&&read()==10,"OS thread attach seeds guest TLS and preserves thread isolation");
 std::cout<<"OS donor static TLS index: "<<index()<<"; main/worker values: "<<read()<<"/"<<changed<<"\n";
 // Existing thread-pool callbacks MUST call InitializeGuestThread before guest code.
 // That runtime-wide interception remains unimplemented; no completeness claim.
 std::ofstream("mapped-tls-results.json")<<"{\"mappedOriginalGuestExecuted\":true,\"usesOsAssignedStaticTlsSlot\":true,\"newThreadTemplateAndIsolationPassed\":"<<(ok?"true":"false")<<",\"existingThreadExplicitGuardTested\":true,\"threadCallbacksAndZeroFillTested\":true,\"threadPoolIntegration\":false,\"fullGameLoader\":false,\"xboxTested\":false}";
 // Process-lifetime donor binding: keep both modules resident until process exit.
 return ok?0:12;
}
