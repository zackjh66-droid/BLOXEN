#pragma once
#include <windows.h>
#include <string>
namespace Hollowwake {
// Lookups are resolved before guest loading. No namespace facade, arbitrary
// memory reads, command lines, register contents or stack strings are collected.
using CrashModuleLookup=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);
using CrashModuleFilename=DWORD(WINAPI*)(HMODULE,LPWSTR,DWORD);
inline std::string DescribeCrash(EXCEPTION_RECORD const& record,CrashModuleLookup lookup,CrashModuleFilename filename){
 std::string text="NATIVE_CRASH code="+std::to_string(record.ExceptionCode)+" address="+std::to_string(reinterpret_cast<ULONG_PTR>(record.ExceptionAddress));
 HMODULE module=nullptr;wchar_t path[512]={};
 if(lookup&&filename&&lookup(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(record.ExceptionAddress),&module)){
  DWORD length=filename(module,path,512);
  if(length&&length<512){
   DWORD start=0;for(DWORD i=0;i<length;++i)if(path[i]==L'\\'||path[i]==L'/')start=i+1;
   std::string name;for(DWORD i=start;i<length&&name.size()<96;++i){auto c=path[i];name+=((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'_'||c==L'-')?static_cast<char>(c):'_';}
   if(!name.empty())text+=" caller="+name+" RVA="+std::to_string(reinterpret_cast<ULONG_PTR>(record.ExceptionAddress)-reinterpret_cast<ULONG_PTR>(module));
  }
 }
 if(record.ExceptionCode==EXCEPTION_ACCESS_VIOLATION&&record.NumberParameters>=2){
  auto operation=record.ExceptionInformation[0];
  text+=std::string(" access=")+(operation==0?"read":operation==1?"write":operation==8?"execute":"unknown");
  text+=" target="+std::to_string(record.ExceptionInformation[1]);
 }
 return text;
}
}
