#pragma once
#include <windows.h>
#include <string>
#include <initializer_list>
#include <vector>
struct ClientImageProtection {DWORD rva;SIZE_T bytes;DWORD protect;};
using ClientImageQuery=SIZE_T(WINAPI*)(LPCVOID,PMEMORY_BASIC_INFORMATION,SIZE_T);
using ClientImageProtect=BOOL(WINAPI*)(LPVOID,SIZE_T,DWORD,PDWORD);
using ClientImageLogger=void(*)(const std::string&);
// Replay only the original initializer's measured header ImageBase update.
// The file retains its preferred base so the real OS loader relocates normally;
// this operation happens afterwards and never edits a relocation or instruction.
inline HRESULT RestoreClientRuntimeImageBase(HMODULE module,SIZE_T imageSize,ULONGLONG preferred,ClientImageQuery query,ClientImageProtect protect,ClientImageLogger log){
 if(!module||imageSize<sizeof(IMAGE_DOS_HEADER)||!preferred||!query||!protect)return E_INVALIDARG;
 auto base=reinterpret_cast<BYTE*>(module);
 auto readable=[&](SIZE_T offset,SIZE_T size)->bool{
  if(offset>=imageSize||size>imageSize-offset)return false;
  MEMORY_BASIC_INFORMATION page={};if(query(base+offset,&page,sizeof(page))!=sizeof(page)||page.AllocationBase!=module||page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||(page.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
  auto start=reinterpret_cast<ULONG_PTR>(page.BaseAddress),here=reinterpret_cast<ULONG_PTR>(base+offset);return start<=here&&page.RegionSize>=here-start&&size<=page.RegionSize-(here-start);
 };
 if(!readable(0,sizeof(IMAGE_DOS_HEADER)))return HRESULT_FROM_WIN32(ERROR_INVALID_ADDRESS);
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<sizeof(IMAGE_DOS_HEADER)||!readable(static_cast<SIZE_T>(dos->e_lfanew),sizeof(IMAGE_NT_HEADERS64)))return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
 auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)||nt->OptionalHeader.SizeOfHeaders>imageSize)return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
 auto field=&nt->OptionalHeader.ImageBase;auto offset=reinterpret_cast<BYTE*>(field)-base;
 if(reinterpret_cast<ULONG_PTR>(field)%8||offset+sizeof(*field)>nt->OptionalHeader.SizeOfHeaders)return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
 ULONGLONG loaded=reinterpret_cast<ULONG_PTR>(module),before=*field;
 if(before!=preferred&&before!=loaded)return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
 if(before!=loaded){
  MEMORY_BASIC_INFORMATION page={};if(query(field,&page,sizeof(page))!=sizeof(page))return HRESULT_FROM_WIN32(ERROR_INVALID_ADDRESS);
  auto access=page.Protect&0xff;bool writable=access==PAGE_READWRITE||access==PAGE_WRITECOPY||access==PAGE_EXECUTE_READWRITE||access==PAGE_EXECUTE_WRITECOPY;
  DWORD old=0;if(!writable&&!protect(field,sizeof(*field),PAGE_READWRITE,&old)){auto e=GetLastError();return HRESULT_FROM_WIN32(e?e:ERROR_GEN_FAILURE);}
  auto observed=static_cast<ULONGLONG>(InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(field),static_cast<LONG64>(loaded),static_cast<LONG64>(before)));
  DWORD ignored=0;bool restored=writable||protect(field,sizeof(*field),page.Protect,&ignored);auto error=GetLastError();
  if(!restored||observed!=before){if(observed==before)InterlockedExchange64(reinterpret_cast<volatile LONG64*>(field),static_cast<LONG64>(before));return HRESULT_FROM_WIN32(!restored?(error?error:ERROR_GEN_FAILURE):ERROR_RETRY);}
 }
 if(*field!=loaded)return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
 if(log)log("NATIVE_IMAGE_STARTUP original measured runtime header ImageBase restored to current loaded module; preferred file base and original instructions/checks retained");return S_OK;
}
// Restore only the original initialization's measured image permissions. This
// does not manufacture successful page transitions or modify guest code/state.
inline HRESULT ApplyClientImageStartup(HMODULE module,SIZE_T imageSize,const ClientImageProtection* regions,unsigned count,ClientImageQuery query,ClientImageProtect protect,ClientImageLogger log){
 if(!module||!regions||!count||count>4096||!imageSize)return E_INVALIDARG;
 if(!query||!protect)return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
 auto base=reinterpret_cast<BYTE*>(module);SIZE_T previousEnd=0;
 for(unsigned i=0;i<count;++i){const auto& region=regions[i];
  bool allowed=region.protect==PAGE_NOACCESS||region.protect==PAGE_READONLY||region.protect==PAGE_READWRITE||region.protect==PAGE_WRITECOPY||region.protect==PAGE_EXECUTE||region.protect==PAGE_EXECUTE_READ||region.protect==PAGE_EXECUTE_READWRITE||region.protect==PAGE_EXECUTE_WRITECOPY;
  if(!allowed||!region.bytes||region.rva%4096||region.bytes%4096||region.rva<previousEnd||region.rva>=imageSize||region.bytes>imageSize-region.rva)return E_INVALIDARG;
  SIZE_T end=region.rva+region.bytes;
  for(SIZE_T cursor=region.rva;cursor<end;){MEMORY_BASIC_INFORMATION actual={};
   if(query(base+cursor,&actual,sizeof(actual))!=sizeof(actual)){auto error=GetLastError();return HRESULT_FROM_WIN32(error?error:ERROR_INVALID_DATA);}
   if(actual.AllocationBase!=module||actual.State!=MEM_COMMIT||actual.Type!=MEM_IMAGE)return HRESULT_FROM_WIN32(ERROR_INVALID_ADDRESS);
   auto actualStart=reinterpret_cast<ULONG_PTR>(actual.BaseAddress),here=reinterpret_cast<ULONG_PTR>(base+cursor);
   if(actualStart>here||actual.RegionSize<=here-actualStart)return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
   SIZE_T advance=actual.RegionSize-(here-actualStart);cursor+=advance>end-cursor?end-cursor:advance;
  }
  DWORD old=0;if(!protect(base+region.rva,region.bytes,region.protect,&old)){auto error=GetLastError();if(log)log("NATIVE_IMAGE_PROTECT_FAILED rva="+std::to_string(region.rva)+" bytes="+std::to_string(region.bytes)+" protect="+std::to_string(region.protect)+" Win32="+std::to_string(error));return HRESULT_FROM_WIN32(error?error:ERROR_GEN_FAILURE);}
  previousEnd=end;
 }
 if(log)log("NATIVE_IMAGE_STARTUP measured image permissions restored regions="+std::to_string(count)+"; original entry/checks retained; Xbox behavior requires execution");
 return S_OK;
}

struct ClientCompilerImportCopy {DWORD sourceRva,destinationRva;ULONGLONG unboundValue;const char* provider;const char* symbol;};
inline HRESULT VerifyClientImportCopies(HMODULE module,SIZE_T imageSize,const ClientCompilerImportCopy* copies,unsigned count,ClientImageQuery query,ClientImageLogger log){
 if(!module||!imageSize||!copies||!count||count>4096||!query)return E_INVALIDARG;auto base=reinterpret_cast<BYTE*>(module);
 for(unsigned i=0;i<count;++i){const auto& item=copies[i];
  if(item.sourceRva%8||item.destinationRva%8||item.sourceRva>=imageSize||item.destinationRva>=imageSize||sizeof(ULONG_PTR)>imageSize-item.sourceRva||sizeof(ULONG_PTR)>imageSize-item.destinationRva)return E_INVALIDARG;
  for(auto rva:{item.sourceRva,item.destinationRva}){MEMORY_BASIC_INFORMATION actual={};if(query(base+rva,&actual,sizeof(actual))!=sizeof(actual)||actual.AllocationBase!=module||actual.State!=MEM_COMMIT||actual.Type!=MEM_IMAGE||(actual.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return HRESULT_FROM_WIN32(ERROR_INVALID_ADDRESS);}
  auto current=*reinterpret_cast<const ULONG_PTR*>(base+item.sourceRva),copied=*reinterpret_cast<const ULONG_PTR*>(base+item.destinationRva);
  if(!current||current!=copied){if(log)log(std::string("NATIVE_LOAD_BLOCKED ")+item.provider+"!"+item.symbol+" copied import differs from current native-loader binding; sourceRva="+std::to_string(item.sourceRva)+" destinationRva="+std::to_string(item.destinationRva));return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);}
 }
 if(log)log("NATIVE_STARTUP compiler import copies verified="+std::to_string(count)+"; destination-process loader values matched; capture-process import addresses are not used");return S_OK;
}

inline HRESULT RestoreClientImportCopies(HMODULE module,SIZE_T imageSize,const ClientCompilerImportCopy* copies,unsigned count,ClientImageQuery query,ClientImageProtect protect,ClientImageLogger log){
 if(!module||!imageSize||!copies||!count||count>4096||!query||!protect)return E_INVALIDARG;
 struct Change{ULONG_PTR* target;ULONG_PTR previous,current;DWORD protection;bool transition,changed;};std::vector<Change> work;work.reserve(count);auto base=reinterpret_cast<BYTE*>(module);
 // Validate the entire measured plan before changing any binding.
 for(unsigned i=0;i<count;++i){const auto& item=copies[i];
  if(item.sourceRva%8||item.destinationRva%8||item.sourceRva>=imageSize||item.destinationRva>=imageSize||sizeof(ULONG_PTR)>imageSize-item.sourceRva||sizeof(ULONG_PTR)>imageSize-item.destinationRva)return E_INVALIDARG;
  MEMORY_BASIC_INFORMATION destination={};
  for(auto rva:{item.sourceRva,item.destinationRva}){MEMORY_BASIC_INFORMATION page={};if(query(base+rva,&page,sizeof(page))!=sizeof(page)||page.AllocationBase!=module||page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||(page.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return HRESULT_FROM_WIN32(ERROR_INVALID_ADDRESS);if(rva==item.destinationRva)destination=page;}
  auto current=*reinterpret_cast<const ULONG_PTR*>(base+item.sourceRva),previous=*reinterpret_cast<const ULONG_PTR*>(base+item.destinationRva);
  if(!current||(previous!=current&&previous!=item.unboundValue)){if(log)log(std::string("NATIVE_LOAD_BLOCKED ")+item.provider+"!"+item.symbol+" compiler-copy state differs from signed unbound value/current loader binding");return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);}
  MEMORY_BASIC_INFORMATION provider={};if(query(reinterpret_cast<void*>(current),&provider,sizeof(provider))!=sizeof(provider)||provider.State!=MEM_COMMIT||provider.Type!=MEM_IMAGE||(provider.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return HRESULT_FROM_WIN32(ERROR_INVALID_ADDRESS);
  auto access=destination.Protect&0xff;bool writable=access==PAGE_READWRITE||access==PAGE_WRITECOPY||access==PAGE_EXECUTE_READWRITE||access==PAGE_EXECUTE_WRITECOPY;
  if(!writable&&(access==PAGE_EXECUTE||access==PAGE_EXECUTE_READ))return E_ACCESSDENIED;
  work.push_back({reinterpret_cast<ULONG_PTR*>(base+item.destinationRva),previous,current,destination.Protect,!writable,false});
 }
 DWORD error=0;
 for(auto& item:work){if(item.previous==item.current)continue;DWORD old=0;if(item.transition&&!protect(item.target,sizeof(*item.target),PAGE_READWRITE,&old)){error=GetLastError();if(!error)error=ERROR_GEN_FAILURE;break;}
  auto observed=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(item.target),reinterpret_cast<void*>(item.current),reinterpret_cast<void*>(item.previous));item.changed=reinterpret_cast<ULONG_PTR>(observed)==item.previous;
  DWORD ignored=0;bool restored=!item.transition||protect(item.target,sizeof(*item.target),item.protection,&ignored);if(!restored){error=GetLastError();if(!error)error=ERROR_GEN_FAILURE;break;}if(!item.changed){error=ERROR_RETRY;break;}
 }
 if(error){for(auto it=work.rbegin();it!=work.rend();++it)if(it->changed){DWORD ignored=0;if(!it->transition||protect(it->target,sizeof(*it->target),PAGE_READWRITE,&ignored)){InterlockedExchangePointer(reinterpret_cast<void* volatile*>(it->target),reinterpret_cast<void*>(it->previous));if(it->transition)protect(it->target,sizeof(*it->target),it->protection,&ignored);}}if(log)log("NATIVE_LOAD_BLOCKED compiler import-copy operation Win32="+std::to_string(error));return HRESULT_FROM_WIN32(error);}
 return VerifyClientImportCopies(module,imageSize,copies,count,query,log);
}
