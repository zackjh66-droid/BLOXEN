// Original bounded loading stages for the exact inspected 6.21 main executable.
// Does not execute guest code with unresolved imports or an unimplemented runtime.
#include <windows.h>
#include <bcrypt.h>
#include <agile.h>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <stdexcept>
#include <cstring>
#include "../../client-runtime/graphics/CoreWindowBridge.h"
static std::mutex logMutex;static std::wstring logName;static BYTE* mapped=nullptr;static SIZE_T mappedSize=0;static bool consumed=false;
static void Log(const std::string& text){std::lock_guard<std::mutex> g(logMutex);std::ofstream f(logName,std::ios::app);f<<text<<"\n";f.flush();}
static void Fail(const char* message){throw std::runtime_error(message);}
static bool Range(SIZE_T start,SIZE_T count,SIZE_T size){return start<=size&&count<=size-start;}
static BYTE* Rva(DWORD address,SIZE_T count){if(!Range(address,count,mappedSize))Fail("PE RVA outside image");return mapped+address;}
static const char* Name(DWORD address){auto p=(const char*)Rva(address,1);SIZE_T max=mappedSize-address;if(max>4096)max=4096;if(!memchr(p,0,max))Fail("Unterminated PE name");return p;}
static bool Hash(const std::vector<BYTE>& raw){
 static const BYTE expected[32]={0x9d,0x18,0xfb,0x5c,0x5c,0x43,0xdd,0xc2,0x12,0x51,0xd5,0xce,0x9e,0x02,0x6c,0xa0,0x1f,0x90,0xd4,0x29,0x21,0xdd,0x74,0x19,0x73,0xbd,0x55,0x36,0xe8,0x94,0x94,0x78};
 BCRYPT_ALG_HANDLE alg=nullptr;BYTE result[32];if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;auto s=BCryptHash(alg,nullptr,0,const_cast<BYTE*>(raw.data()),(ULONG)raw.size(),result,32);BCryptCloseAlgorithmProvider(alg,0);return s>=0&&!memcmp(expected,result,32);
}
static void* Import(const char* dll,const char* name){
 if(auto p=Hollowwake::ResolveGraphicsImport(dll,name))return p;
 if(_stricmp(dll,"KERNEL32.dll")&&_stricmp(dll,"api-ms-win-core-processthreads-l1-1-0.dll"))return nullptr;
 #define REAL(api) if(!strcmp(name,#api))return reinterpret_cast<void*>(&api)
 REAL(GetCurrentThreadId);REAL(GetCurrentProcessId);REAL(QueryPerformanceCounter);REAL(QueryPerformanceFrequency);REAL(GetLastError);REAL(SetLastError);REAL(GetTickCount64);REAL(GetCurrentProcess);
 #undef REAL
 return nullptr;
}
static unsigned imports=0,missing=0;
static void Thunks(const char* dll,DWORD source,DWORD target){
 for(SIZE_T i=0;i<32768;++i){ULONGLONG value=*(ULONGLONG*)Rva(source+(DWORD)(i*8),8);if(!value)return;auto out=(ULONGLONG*)Rva(target+(DWORD)(i*8),8);void* p=nullptr;std::string symbol;
  if(value&IMAGE_ORDINAL_FLAG64)symbol="#"+std::to_string(value&0xffff);
  else {if(value>MAXDWORD)Fail("Invalid name thunk");Rva((DWORD)value,3);symbol=Name((DWORD)value+2);p=Import(dll,symbol.c_str());}
  ++imports;if(p)*out=(ULONGLONG)p;else {++missing;if(missing<=256)Log("UNRESOLVED "+std::string(dll)+"!"+symbol);}
 }Fail("Unbounded import thunk list");
}
extern "C" __declspec(dllexport) int __cdecl HWBindWindow(const wchar_t* path){
 logName=path;auto w=Windows::UI::Core::CoreWindow::GetForCurrentThread();if(!w)return (int)RPC_E_WRONG_THREAD;
 return Hollowwake::BindGraphicsWindow(reinterpret_cast<IUnknown*>(w));
}
static int MemoryPolicy(const wchar_t* path){
 logName=path;Log("POLICY allocate RW -> protect RX -> execute ORIGINAL return-621 fixture");
 BYTE* p=(BYTE*)VirtualAllocFromApp(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!p){Log("POLICY allocation error="+std::to_string(GetLastError()));return -1;}
 const BYTE code[]={0xb8,0x6d,0x02,0x00,0x00,0xc3};memcpy(p,code,sizeof(code));ULONG old=0;
 if(!VirtualProtectFromApp(p,4096,PAGE_EXECUTE_READ,&old)){DWORD e=GetLastError();VirtualFree(p,0,MEM_RELEASE);Log("POLICY executable transition denied error="+std::to_string(e));return -(int)e;}
 FlushInstructionCache(GetCurrentProcess(),p,sizeof(code));int result=((int(*)())p)();VirtualFree(p,0,MEM_RELEASE);Log("POLICY original code returned="+std::to_string(result));return result;
}
static int Attempt(const wchar_t* root){
 if(consumed)Fail("Restart the app before another mapping attempt");
 std::wstring file=std::wstring(root)+L"\\6.21\\FortniteGame\\Binaries\\Win64\\FortniteClient-Win64-Shipping.exe";
 std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in)Fail("Exact client missing: import archive first");auto length=in.tellg();if(length<4096||length>512*1024*1024)Fail("Unexpected main image size");in.seekg(0);std::vector<BYTE> raw((SIZE_T)length);if(!in.read((char*)raw.data(),length))Fail("Image read failed");if(!Hash(raw))Fail("Main executable SHA256 differs from inspected 6.21 build");Log("HASH exact main image verified");
 auto dos=(IMAGE_DOS_HEADER*)raw.data();if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<64||!Range(dos->e_lfanew,sizeof(IMAGE_NT_HEADERS64),raw.size()))Fail("Invalid PE header");
 auto nt=(IMAGE_NT_HEADERS64*)(raw.data()+dos->e_lfanew);if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64))Fail("Not supported x64 PE32+");
 auto& o=nt->OptionalHeader;if(!o.SizeOfImage||o.SizeOfImage>1024*1024*1024||o.SizeOfHeaders>raw.size()||o.SizeOfHeaders>o.SizeOfImage||o.NumberOfRvaAndSizes<16||nt->FileHeader.NumberOfSections>96)Fail("Invalid PE limits");
 auto sections=IMAGE_FIRST_SECTION(nt);if(!Range((BYTE*)sections-raw.data(),nt->FileHeader.NumberOfSections*sizeof(IMAGE_SECTION_HEADER),raw.size()))Fail("Invalid section table");consumed=true;
 mappedSize=o.SizeOfImage;mapped=(BYTE*)VirtualAllocFromApp(nullptr,mappedSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!mapped){Log("ALLOC error="+std::to_string(GetLastError()));Fail("Client RW allocation failed");}Log("MAP bytes="+std::to_string(mappedSize));memcpy(mapped,raw.data(),o.SizeOfHeaders);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){auto& s=sections[i];if(!Range(s.PointerToRawData,s.SizeOfRawData,raw.size())||!Range(s.VirtualAddress,s.SizeOfRawData,mappedSize)||!Range(s.VirtualAddress,s.Misc.VirtualSize,mappedSize))Fail("Invalid section bounds");memcpy(mapped+s.VirtualAddress,raw.data()+s.PointerToRawData,s.SizeOfRawData);}
 INT64 delta=(INT64)mapped-(INT64)o.ImageBase;auto reloc=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];if(delta&&!reloc.Size)Fail("Relocations absent");
 for(DWORD used=0;delta&&used<reloc.Size;){auto b=(IMAGE_BASE_RELOCATION*)Rva(reloc.VirtualAddress+used,8);if(b->SizeOfBlock<8||b->SizeOfBlock>reloc.Size-used||b->SizeOfBlock%2)Fail("Invalid relocation block");auto entries=(WORD*)Rva(reloc.VirtualAddress+used+8,b->SizeOfBlock-8);for(DWORD j=0;j<(b->SizeOfBlock-8)/2;++j){WORD type=entries[j]>>12;if(type==IMAGE_REL_BASED_DIR64)*(ULONGLONG*)Rva(b->VirtualAddress+(entries[j]&4095),8)+=delta;else if(type!=IMAGE_REL_BASED_ABSOLUTE)Fail("Unsupported relocation type");}used+=b->SizeOfBlock;}Log("RELOC applied");
 // Apply exact-client TLS before import-dependent initialization. OS loads only
 // our packaged donor, never archive-provided DLLs via the native OS loader.
 auto td=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];if(td.Size){if(td.Size<sizeof(IMAGE_TLS_DIRECTORY64))Fail("Invalid TLS directory");auto t=(IMAGE_TLS_DIRECTORY64*)Rva(td.VirtualAddress,sizeof(IMAGE_TLS_DIRECTORY64));
  auto start=t->StartAddressOfRawData-(ULONGLONG)mapped;auto count=t->EndAddressOfRawData-t->StartAddressOfRawData;if(t->EndAddressOfRawData<t->StartAddressOfRawData||!Range(start,count,mappedSize))Fail("Invalid TLS template");
  if(t->AddressOfCallBacks){auto cb=t->AddressOfCallBacks-(ULONGLONG)mapped;if(*(ULONGLONG*)Rva((DWORD)cb,8))Fail("Guest TLS callbacks need dependency-graph initialization first");}
  HMODULE donor=LoadPackagedLibrary(L"HollowwakeTlsDonor.dll",0);if(!donor){Log("TLS donor load error="+std::to_string(GetLastError()));Fail("Packaged TLS donor unavailable");}
  auto bind=(BOOL(WINAPI*)(const void*,SIZE_T,SIZE_T,void*,const PIMAGE_TLS_CALLBACK*,unsigned))GetProcAddress(donor,"BindGuest");auto index=(DWORD(WINAPI*)())GetProcAddress(donor,"GuestTlsIndex");if(!bind||!index||!bind(Rva((DWORD)start,(SIZE_T)count),(SIZE_T)count,t->SizeOfZeroFill,mapped,nullptr,0))Fail("TLS donor binding failed");
  auto idx=t->AddressOfIndex-(ULONGLONG)mapped;if(idx>MAXDWORD)Fail("Invalid TLS index address");*(DWORD*)Rva((DWORD)idx,4)=index();Log("TLS donor index="+std::to_string(index())+" templateBytes="+std::to_string(count));
 }
 auto imp=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];bool end=!imp.Size;
 for(DWORD offset=0;!end&&Range(offset,sizeof(IMAGE_IMPORT_DESCRIPTOR),imp.Size);offset+=sizeof(IMAGE_IMPORT_DESCRIPTOR)){auto d=(IMAGE_IMPORT_DESCRIPTOR*)Rva(imp.VirtualAddress+offset,sizeof(IMAGE_IMPORT_DESCRIPTOR));if(!d->Name){end=true;break;}Thunks(Name(d->Name),d->OriginalFirstThunk?d->OriginalFirstThunk:d->FirstThunk,d->FirstThunk);}if(!end)Fail("Unterminated import directory");
 struct Delay {DWORD flags,name,module,iat,intable,bound,unload,time;};auto delay=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];end=!delay.Size;
 for(DWORD offset=0;!end&&Range(offset,sizeof(Delay),delay.Size);offset+=sizeof(Delay)){auto d=(Delay*)Rva(delay.VirtualAddress+offset,sizeof(Delay));if(!d->name){end=true;break;}if(d->flags!=1)Fail("Unsupported VA-based delay import");Thunks(Name(d->name),d->intable,d->iat);}if(!end)Fail("Unterminated delay import directory");
 Log("IMPORT total="+std::to_string(imports)+" unresolved="+std::to_string(missing));
 // Exercise the real target section's RX transition without jumping into it.
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){auto& s=sections[i];if(!s.Misc.VirtualSize)continue;bool x=(s.Characteristics&IMAGE_SCN_MEM_EXECUTE)!=0,w=(s.Characteristics&IMAGE_SCN_MEM_WRITE)!=0;if(x&&w)Fail("Writable executable section rejected");DWORD protect=x?PAGE_EXECUTE_READ:(w?PAGE_READWRITE:PAGE_READONLY),old;
  if(!VirtualProtectFromApp(Rva(s.VirtualAddress,s.Misc.VirtualSize),s.Misc.VirtualSize,protect,&old)){Log("PROTECT error="+std::to_string(GetLastError()));Fail("Client section protection failed");}}
 FlushInstructionCache(GetCurrentProcess(),mapped,mappedSize);Log("PROTECT target sections applied");
 auto unwind=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];if(unwind.Size){if(unwind.Size%sizeof(RUNTIME_FUNCTION))Fail("Invalid unwind table size");auto table=(PRUNTIME_FUNCTION)Rva(unwind.VirtualAddress,unwind.Size);for(DWORD i=0;i<unwind.Size/sizeof(*table);++i){if(table[i].BeginAddress>=table[i].EndAddress||!Range(table[i].BeginAddress,table[i].EndAddress-table[i].BeginAddress,mappedSize))Fail("Invalid unwind function range");Rva(table[i].UnwindData,1);}if(!RtlAddFunctionTable(table,unwind.Size/sizeof(*table),(DWORD64)mapped))Fail("Unwind registration failed");Log("UNWIND registered="+std::to_string(unwind.Size/sizeof(*table)));}
 if(missing)Fail("Client entry blocked by unresolved native imports; see UNRESOLVED records");
 Fail("Entry gate: dependency graph, window/input ABI and private client-network routing are not complete");
 return 0;
}
static int Body(const wchar_t* root){try{return Attempt(root);}catch(const std::exception& e){Log(std::string("STOP ")+e.what());return -1;}}
static int Crash(EXCEPTION_POINTERS* p){Log("CRASH code="+std::to_string(p->ExceptionRecord->ExceptionCode)+" instruction="+std::to_string(p->ContextRecord->Rip));return EXCEPTION_EXECUTE_HANDLER;}
extern "C" __declspec(dllexport) int __cdecl HWAttempt(const wchar_t* root){__try{return Body(root);}__except(Crash(GetExceptionInformation())){return -2;}}

extern "C" __declspec(dllexport) int __cdecl HWMemoryPolicy(const wchar_t* path){__try{return MemoryPolicy(path);}__except(Crash(GetExceptionInformation())){return -2;}}
