// Original, hash-allowlisted dependency mapper. NO archive DLL entrypoints run.
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <stdexcept>
#include "Dependencies.h"
#include "KnownModules.h"
struct Image {BYTE* base;SIZE_T size;IMAGE_NT_HEADERS64 header;bool importsReady=false;};
static std::map<std::string,std::unique_ptr<Image>> images;static std::wstring root;
static void*(*resolve)(const char*,const char*);static void(*writeLog)(const std::string&);static unsigned failures=0,donorSlot=1,depth=0;
static bool Fits(SIZE_T a,SIZE_T b,SIZE_T size){return a<=size&&b<=size-a;}
static void Bad(const char* e){throw std::runtime_error(e);}
static BYTE* At(Image& m,DWORD r,SIZE_T n){if(!Fits(r,n,m.size))Bad("dependency RVA out of bounds");return m.base+r;}
static const char* Text(Image& m,DWORD r){auto p=(const char*)At(m,r,1);if(!memchr(p,0,std::min((SIZE_T)4096,m.size-r)))Bad("dependency string unbounded");return p;}
static bool Digest(const std::vector<BYTE>& raw,const char* expected){BCRYPT_ALG_HANDLE alg;BYTE hash[32];if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;auto s=BCryptHash(alg,nullptr,0,const_cast<BYTE*>(raw.data()),(ULONG)raw.size(),hash,32);BCryptCloseAlgorithmProvider(alg,0);const char* digits="0123456789abcdef";std::string value;for(auto b:hash){value+=digits[b>>4];value+=digits[b&15];}return s>=0&&value==expected;}
void ConfigureDependencies(const std::wstring& path,void*(*resolver)(const char*,const char*),void(*log)(const std::string&)){root=path;resolve=resolver;writeLog=log;}
unsigned DependencyFailures(){return failures;}
static void Thunks(Image& m,const char* dll,DWORD source,DWORD target){for(unsigned i=0;i<32768;++i){auto v=*(ULONGLONG*)At(m,source+i*8,8);if(!v)return;std::string name;if(v&IMAGE_ORDINAL_FLAG64)name="#"+std::to_string(v&0xffff);else{if(v>MAXDWORD)Bad("invalid dependency thunk");name=Text(m,(DWORD)v+2);}void* p=resolve(dll,name.c_str());if(p)*(ULONGLONG*)At(m,target+i*8,8)=(ULONGLONG)p;else{++failures;if(failures<=256)writeLog("DEP UNRESOLVED "+std::string(dll)+"!"+name);}}Bad("dependency import list unbounded");}
static Image* Load(const std::string& key,const KnownModule& known){
 auto old=images.find(key);if(old!=images.end())return old->second.get();if(images.size()>=96)Bad("dependency image count limit");
 std::ifstream file(root+L"\\"+known.path,std::ios::binary|std::ios::ate);if(!file)Bad("allowlisted dependency file absent");auto n=file.tellg();if(n<1024||n>512*1024*1024)Bad("dependency raw size limit");file.seekg(0);std::vector<BYTE> bytes((SIZE_T)n);if(!file.read((char*)bytes.data(),n)||!Digest(bytes,known.hash))Bad("dependency SHA256 mismatch");
 auto dos=(IMAGE_DOS_HEADER*)bytes.data();if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<64||!Fits(dos->e_lfanew,sizeof(IMAGE_NT_HEADERS64),bytes.size()))Bad("invalid dependency DOS header");auto nt=(IMAGE_NT_HEADERS64*)(bytes.data()+dos->e_lfanew);auto& o=nt->OptionalHeader;
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||o.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)||o.NumberOfRvaAndSizes<16||o.SizeOfImage>512*1024*1024||o.SizeOfHeaders>bytes.size()||o.SizeOfHeaders>o.SizeOfImage||nt->FileHeader.NumberOfSections>96)Bad("invalid dependency PE limits");
 auto sections=IMAGE_FIRST_SECTION(nt);if(!Fits((BYTE*)sections-bytes.data(),nt->FileHeader.NumberOfSections*sizeof(*sections),bytes.size()))Bad("invalid dependency section table");
 auto value=std::unique_ptr<Image>(new Image{});value->size=o.SizeOfImage;value->header=*nt;value->base=(BYTE*)VirtualAllocFromApp(nullptr,value->size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!value->base)Bad("dependency allocation failed");Image* m=value.get();images[key]=std::move(value);memcpy(m->base,bytes.data(),o.SizeOfHeaders);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){auto& s=sections[i];if(!Fits(s.PointerToRawData,s.SizeOfRawData,bytes.size())||!Fits(s.VirtualAddress,s.SizeOfRawData,m->size)||!Fits(s.VirtualAddress,s.Misc.VirtualSize,m->size))Bad("invalid dependency section");memcpy(m->base+s.VirtualAddress,bytes.data()+s.PointerToRawData,s.SizeOfRawData);}
 INT64 delta=(INT64)m->base-(INT64)o.ImageBase;auto reloc=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];if(delta&&!reloc.Size)Bad("dependency cannot relocate");for(DWORD used=0;delta&&used<reloc.Size;){auto b=(IMAGE_BASE_RELOCATION*)At(*m,reloc.VirtualAddress+used,8);if(b->SizeOfBlock<8||b->SizeOfBlock>reloc.Size-used||b->SizeOfBlock%2)Bad("bad dependency relocation");auto entries=(WORD*)At(*m,reloc.VirtualAddress+used+8,b->SizeOfBlock-8);for(DWORD j=0;j<(b->SizeOfBlock-8)/2;++j){auto type=entries[j]>>12;if(type==IMAGE_REL_BASED_DIR64)*(ULONGLONG*)At(*m,b->VirtualAddress+(entries[j]&4095),8)+=delta;else if(type!=IMAGE_REL_BASED_ABSOLUTE)Bad("unknown dependency relocation");}used+=b->SizeOfBlock;}
 auto tls=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];if(tls.Size){if(tls.Size<40||donorSlot>=32)Bad("dependency TLS limits");auto t=(IMAGE_TLS_DIRECTORY64*)At(*m,tls.VirtualAddress,40);auto start=t->StartAddressOfRawData-(ULONGLONG)m->base;auto size=t->EndAddressOfRawData-t->StartAddressOfRawData;if(t->EndAddressOfRawData<t->StartAddressOfRawData||!Fits(start,size,m->size)||start>MAXDWORD)Bad("dependency TLS bounds");wchar_t donorName[64];swprintf_s(donorName,L"HollowwakeTlsSlot%02u.dll",donorSlot++);auto donor=LoadPackagedLibrary(donorName,0);if(!donor)Bad("packaged dependency TLS donor unavailable");auto bind=(BOOL(WINAPI*)(const void*,SIZE_T,SIZE_T,void*,const PIMAGE_TLS_CALLBACK*,unsigned))GetProcAddress(donor,"BindGuest");auto index=(DWORD(WINAPI*)())GetProcAddress(donor,"GuestTlsIndex");if(!bind||!index||!bind(At(*m,(DWORD)start,(SIZE_T)size),(SIZE_T)size,t->SizeOfZeroFill,m->base,nullptr,0))Bad("dependency TLS binding failed");auto idx=t->AddressOfIndex-(ULONGLONG)m->base;if(idx>MAXDWORD)Bad("dependency TLS index bounds");*(DWORD*)At(*m,(DWORD)idx,4)=index();
  if(t->AddressOfCallBacks){auto cb=t->AddressOfCallBacks-(ULONGLONG)m->base;if(cb>MAXDWORD)Bad("dependency TLS callbacks bounds");if(*(ULONGLONG*)At(*m,(DWORD)cb,8)){++failures;writeLog("DEP TLS callbacks deferred; initialization graph incomplete: "+key);}}
 }
 // Register exports before resolving imports so dependency cycles can be mapped.
 auto dir=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];bool end=!dir.Size;for(DWORD used=0;!end&&Fits(used,sizeof(IMAGE_IMPORT_DESCRIPTOR),dir.Size);used+=sizeof(IMAGE_IMPORT_DESCRIPTOR)){auto d=(IMAGE_IMPORT_DESCRIPTOR*)At(*m,dir.VirtualAddress+used,sizeof(IMAGE_IMPORT_DESCRIPTOR));if(!d->Name){end=true;break;}Thunks(*m,Text(*m,d->Name),d->OriginalFirstThunk?d->OriginalFirstThunk:d->FirstThunk,d->FirstThunk);}if(!end)Bad("unbounded dependency imports");
 struct Delay{DWORD flags,name,module,iat,intable,bound,unload,time;};dir=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];end=!dir.Size;for(DWORD used=0;!end&&Fits(used,sizeof(Delay),dir.Size);used+=sizeof(Delay)){auto d=(Delay*)At(*m,dir.VirtualAddress+used,sizeof(Delay));if(!d->name){end=true;break;}if(d->flags!=1)Bad("dependency delay-import format");Thunks(*m,Text(*m,d->name),d->intable,d->iat);}if(!end)Bad("unbounded dependency delay imports");
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){auto& s=sections[i];if(!s.Misc.VirtualSize)continue;bool x=(s.Characteristics&IMAGE_SCN_MEM_EXECUTE)!=0,w=(s.Characteristics&IMAGE_SCN_MEM_WRITE)!=0;if(x&&w)Bad("dependency W+X section denied");DWORD before;if(!VirtualProtectFromApp(At(*m,s.VirtualAddress,s.Misc.VirtualSize),s.Misc.VirtualSize,x?PAGE_EXECUTE_READ:(w?PAGE_READWRITE:PAGE_READONLY),&before))Bad("dependency protection failed");}FlushInstructionCache(GetCurrentProcess(),m->base,m->size);
 dir=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];if(dir.Size){if(dir.Size%sizeof(RUNTIME_FUNCTION))Bad("dependency unwind size");auto functions=(PRUNTIME_FUNCTION)At(*m,dir.VirtualAddress,dir.Size);for(DWORD i=0;i<dir.Size/sizeof(*functions);++i){if(functions[i].BeginAddress>=functions[i].EndAddress||!Fits(functions[i].BeginAddress,functions[i].EndAddress-functions[i].BeginAddress,m->size))Bad("dependency unwind range");At(*m,functions[i].UnwindData,1);}if(!RtlAddFunctionTable(functions,dir.Size/sizeof(*functions),(DWORD64)m->base))Bad("dependency unwind registration failed");}
 m->importsReady=true;writeLog("DEP mapped (initializers NOT executed): "+key);return m;
}
void* ResolveDependency(const char* dll,const char* symbol){
 if(!dll||!symbol||depth>=64)return nullptr;std::string key=dll;std::transform(key.begin(),key.end(),key.begin(),[](char c){return (char)tolower((unsigned char)c);});if(key.find_first_of("/\\:")!=std::string::npos)return nullptr;
 const KnownModule* candidate=nullptr;for(auto& entry:known)if(!_stricmp(entry.name,dll)){if(candidate){++failures;writeLog("DEP ambiguous basename refused: "+key);return nullptr;}candidate=&entry;}if(!candidate)return nullptr;
 ++depth;try{auto m=Load(key,*candidate);auto d=m->header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];if(d.Size<sizeof(IMAGE_EXPORT_DIRECTORY))Bad("dependency export table absent");auto e=(IMAGE_EXPORT_DIRECTORY*)At(*m,d.VirtualAddress,sizeof(IMAGE_EXPORT_DIRECTORY));DWORD ordinal=MAXDWORD;
  if(symbol[0]=='#'){unsigned long value=std::stoul(symbol+1);if(value>=e->Base&&value-e->Base<e->NumberOfFunctions)ordinal=(DWORD)(value-e->Base);}
  else {if(e->NumberOfNames>65536)Bad("dependency export name count");auto names=(DWORD*)At(*m,e->AddressOfNames,e->NumberOfNames*4);auto ordinals=(WORD*)At(*m,e->AddressOfNameOrdinals,e->NumberOfNames*2);for(DWORD i=0;i<e->NumberOfNames;++i)if(!strcmp(Text(*m,names[i]),symbol)){ordinal=ordinals[i];break;}}
  if(ordinal>=e->NumberOfFunctions){--depth;return nullptr;}auto funcs=(DWORD*)At(*m,e->AddressOfFunctions,e->NumberOfFunctions*4);DWORD rva=funcs[ordinal];if(!rva){--depth;return nullptr;}void* result;
  if(rva>=d.VirtualAddress&&rva-d.VirtualAddress<d.Size){std::string f=Text(*m,rva);auto dot=f.find('.');if(dot==std::string::npos)Bad("invalid forwarded export");auto module=f.substr(0,dot)+".dll";result=resolve(module.c_str(),f.substr(dot+1).c_str());}else result=At(*m,rva,1);--depth;return result;
 }catch(const std::exception& e){--depth;++failures;writeLog("DEP blocked "+key+": "+e.what());return nullptr;}
}
