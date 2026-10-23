#pragma once
#include <windows.h>
#include <string>
namespace Hollowwake {
// Providers are resolved before guest loading. Stack inspection is bounded to
// 64 pointer slots and exports only executable module/RVA candidates, never
// general stack/register data, paths, command lines or memory contents.
// An exact decoded stack branch may additionally report its single code target.
using CrashModuleLookup=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);
using CrashModuleFilename=DWORD(WINAPI*)(HMODULE,LPWSTR,DWORD);
using CrashReadMemory=BOOL(WINAPI*)(HANDLE,LPCVOID,LPVOID,SIZE_T,SIZE_T*);
using CrashQueryMemory=SIZE_T(WINAPI*)(LPCVOID,PMEMORY_BASIC_INFORMATION,SIZE_T);
inline std::string CrashCodeLocation(ULONG_PTR address,CrashModuleLookup lookup,CrashModuleFilename filename){
 HMODULE module=nullptr;wchar_t path[512]={};
 // 0x4=FROM_ADDRESS, 0x2=UNCHANGED_REFCOUNT; optional real OS provider.
 if(!lookup||!filename||!lookup(0x4|0x2,reinterpret_cast<LPCWSTR>(address),&module))return {};
 DWORD length=filename(module,path,512);if(!length||length>=512)return {};
 DWORD start=0;for(DWORD i=0;i<length;++i)if(path[i]==L'\\'||path[i]==L'/')start=i+1;
 std::string name;for(DWORD i=start;i<length&&name.size()<96;++i){auto c=path[i];name+=((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'_'||c==L'-')?static_cast<char>(c):'_';}
 return name.empty()?std::string{}:"caller="+name+" RVA="+std::to_string(address-reinterpret_cast<ULONG_PTR>(module));
}
inline std::string DescribeCrash(EXCEPTION_RECORD const& record,CrashModuleLookup lookup,CrashModuleFilename filename){
 auto address=reinterpret_cast<ULONG_PTR>(record.ExceptionAddress);
 std::string text="NATIVE_CRASH code="+std::to_string(record.ExceptionCode)+" address="+std::to_string(address);
 auto location=CrashCodeLocation(address,lookup,filename);if(!location.empty())text+=" "+location;
 if(record.ExceptionCode==EXCEPTION_ACCESS_VIOLATION&&record.NumberParameters>=2){
  auto operation=record.ExceptionInformation[0];
  text+=std::string(" access=")+(operation==0?"read":operation==1?"write":operation==8?"execute":"unknown");
  text+=" target="+std::to_string(record.ExceptionInformation[1]);
 }
 return text;
}
inline std::string DescribeCrashStackCandidates(CONTEXT const* context,CrashModuleLookup lookup,CrashModuleFilename filename,CrashReadMemory read,CrashQueryMemory query){
 if(!context||!read||!query||!lookup||!filename)return {};
#if defined(_M_X64)
 ULONG_PTR slots[64]={};SIZE_T received=0;
 // Current-process pseudo handle avoids another eager platform dependency.
 if(!read(reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)),reinterpret_cast<LPCVOID>(context->Rsp),slots,sizeof(slots),&received))return {};
 std::string text;unsigned count=0;
 for(SIZE_T i=0;i<received/sizeof(slots[0])&&i<64&&count<8;++i){
  MEMORY_BASIC_INFORMATION info={};if(!slots[i]||query(reinterpret_cast<LPCVOID>(slots[i]),&info,sizeof(info))!=sizeof(info)||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))continue;
  DWORD protection=info.Protect&0xff;if(protection!=PAGE_EXECUTE&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)continue;
  auto location=CrashCodeLocation(slots[i],lookup,filename);if(location.empty())continue;
  text+="\nNATIVE_CRASH_STACK_CANDIDATE stack_index="+std::to_string(i)+" "+location;++count;
 }
 return text;
#else
 return {};
#endif
}
}

namespace Hollowwake {
// Diagnose only an actual ret or the observed jmp [rsp-8]. Do not export an
// arbitrary stack/register dump, change the context, or continue execution.
inline std::string DescribeCrashControlTransfer(CONTEXT const* context,CrashModuleLookup lookup,CrashModuleFilename filename,CrashReadMemory read,CrashQueryMemory query){
#if defined(_M_X64)
 if(!context||!read||!query)return {};
 BYTE instruction[4]={};SIZE_T received=0;auto process=reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1));
 if(!read(process,reinterpret_cast<LPCVOID>(context->Rip),instruction,sizeof(instruction),&received)||received!=sizeof(instruction))return {};
 bool indirect=instruction[0]==0xff&&instruction[1]==0x64&&instruction[2]==0x24&&instruction[3]==0xf8;
 bool ret=instruction[0]==0xc3;if(!indirect&&!ret)return {};
 if(indirect&&context->Rsp<sizeof(ULONG_PTR))return {};
 ULONG_PTR cell=context->Rsp-(indirect?sizeof(ULONG_PTR):0),target=0;MEMORY_BASIC_INFORMATION info={};
 if(query(reinterpret_cast<LPCVOID>(cell),&info,sizeof(info))!=sizeof(info)||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return {};
 if(!read(process,reinterpret_cast<LPCVOID>(cell),&target,sizeof(target),&received)||received!=sizeof(target))return {};
 bool canonical=(target>>47)==0||(target>>47)==0x1ffff;
 auto location=CrashCodeLocation(target,lookup,filename);
 return std::string("\nNATIVE_CRASH_CONTROL_TRANSFER instruction=")+(indirect?"jmp_stack_minus_8":"ret")+" stackAlignment="+std::to_string(context->Rsp%16)+" target="+std::to_string(target)+" canonical="+(canonical?"true":"false")+(location.empty()?"":" "+location);
#else
 return {};
#endif
}
}
