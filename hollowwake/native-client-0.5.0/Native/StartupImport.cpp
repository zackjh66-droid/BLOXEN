#include "StartupImport.h"
#include "ClientImageStartup.h"
#include <cstring>
#include <cstdint>
#include <vector>
#include <algorithm>
namespace {
bool Fits(SIZE_T at,SIZE_T length,SIZE_T size){return at<=size&&length<=size-at;}
bool StringAt(BYTE* base,SIZE_T size,DWORD rva,const char*& result){if(!Fits(rva,1,size))return false;auto limit=(size-rva)<4096?size-rva:4096;auto text=(const char*)base+rva;if(!memchr(text,0,limit))return false;result=text;return true;}
}
bool BindWinMainCommandImport(HMODULE module,const char* provider,void* replacement,const ClientCompilerImportCopy* copies,unsigned copyCount){
 if(!module||!provider||!replacement){SetLastError(ERROR_INVALID_PARAMETER);return false;}
 auto base=(BYTE*)module;auto dos=(IMAGE_DOS_HEADER*)base;
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<64||dos->e_lfanew>1024*1024){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
 auto nt=(IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);auto size=nt->OptionalHeader.SizeOfImage;
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||!Fits(dos->e_lfanew,sizeof(*nt),size)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
 auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!Fits(directory.VirtualAddress,directory.Size,size)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
 std::vector<IMAGE_THUNK_DATA64*> targets;
 for(SIZE_T at=0;Fits(at,sizeof(IMAGE_IMPORT_DESCRIPTOR),directory.Size);at+=sizeof(IMAGE_IMPORT_DESCRIPTOR)){
  auto d=(IMAGE_IMPORT_DESCRIPTOR*)(base+directory.VirtualAddress+at);if(!d->Name)break;const char* name=nullptr;if(!StringAt(base,size,d->Name,name)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}if(_stricmp(name,provider))continue;
  if(!d->OriginalFirstThunk||!d->FirstThunk){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
  for(unsigned i=0;i<32768;++i){SIZE_T original=(SIZE_T)d->OriginalFirstThunk+i*sizeof(IMAGE_THUNK_DATA64),slot=(SIZE_T)d->FirstThunk+i*sizeof(IMAGE_THUNK_DATA64);if(!Fits(original,sizeof(IMAGE_THUNK_DATA64),size)||!Fits(slot,sizeof(IMAGE_THUNK_DATA64),size)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
   auto symbol=((IMAGE_THUNK_DATA64*)(base+original))->u1.AddressOfData;if(!symbol)break;if(IMAGE_SNAP_BY_ORDINAL64(symbol))continue;if(symbol>MAXDWORD-2){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}const char* imported=nullptr;if(!StringAt(base,size,(DWORD)symbol+2,imported)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
   if(!strcmp(imported,"_get_narrow_winmain_command_line")){auto target=(IMAGE_THUNK_DATA64*)(base+slot);if(targets.size()>=16||std::find(targets.begin(),targets.end(),target)!=targets.end()){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}targets.push_back(target);}
  }
 }
 if(targets.empty()){SetLastError(ERROR_PROC_NOT_FOUND);return false;}
 if(copyCount>4096||(!copies&&copyCount)){SetLastError(ERROR_INVALID_PARAMETER);return false;}
 for(unsigned i=0;i<copyCount;++i){const auto& item=copies[i];if(!item.provider||!item.symbol||_stricmp(item.provider,provider)||strcmp(item.symbol,"_get_narrow_winmain_command_line"))continue;
  if(item.sourceRva%8||item.destinationRva%8||!Fits(item.sourceRva,sizeof(IMAGE_THUNK_DATA64),size)||!Fits(item.destinationRva,sizeof(IMAGE_THUNK_DATA64),size)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
  auto source=(IMAGE_THUNK_DATA64*)(base+item.sourceRva),destination=(IMAGE_THUNK_DATA64*)(base+item.destinationRva);
  if(std::find(targets.begin(),targets.end(),source)==targets.end()||destination->u1.Function!=source->u1.Function||targets.size()>=16||std::find(targets.begin(),targets.end(),destination)!=targets.end()){SetLastError(ERROR_INVALID_DATA);return false;}targets.push_back(destination);
 }

 struct Changed{IMAGE_THUNK_DATA64* target;void* prior;DWORD protection;bool changedProtection;};std::vector<Changed> changed;changed.reserve(targets.size());DWORD error=0;
 for(auto target:targets){
  MEMORY_BASIC_INFORMATION page={};if(VirtualQuery(target,&page,sizeof(page))!=sizeof(page)||page.AllocationBase!=module||page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||(page.Protect&(PAGE_NOACCESS|PAGE_GUARD))){error=ERROR_INVALID_ADDRESS;break;}
  auto access=page.Protect&0xff;bool writable=access==PAGE_READWRITE||access==PAGE_WRITECOPY||access==PAGE_EXECUTE_READWRITE||access==PAGE_EXECUTE_WRITECOPY;DWORD old=page.Protect;if(!writable&&(access==PAGE_EXECUTE||access==PAGE_EXECUTE_READ)){error=ERROR_ACCESS_DENIED;break;}
  // A measured, already-writable IAT needs no permission transition. Never
  // request executable permissions merely to restore a data binding.
  if(!writable&&!VirtualProtectFromApp(target,sizeof(*target),PAGE_READWRITE,&old)){error=GetLastError();if(!error)error=ERROR_GEN_FAILURE;break;}
  auto previous=InterlockedExchangePointer((void* volatile*)&target->u1.Function,replacement);changed.push_back({target,previous,old,!writable});DWORD unused=0;
  if(!writable&&!VirtualProtectFromApp(target,sizeof(*target),old,&unused)){error=GetLastError();if(!error)error=ERROR_GEN_FAILURE;break;}
 }
 if(error){
  for(auto it=changed.rbegin();it!=changed.rend();++it){DWORD ignored=0;if(!it->changedProtection||VirtualProtectFromApp(it->target,sizeof(*it->target),PAGE_READWRITE,&ignored)){InterlockedExchangePointer((void* volatile*)&it->target->u1.Function,it->prior);if(it->changedProtection)VirtualProtectFromApp(it->target,sizeof(*it->target),it->protection,&ignored);}}
  SetLastError(error);return false;
 }
 return true;
}
