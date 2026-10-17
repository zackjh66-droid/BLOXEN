#include "StartupImport.h"
#include <cstring>
#include <cstdint>
namespace {
bool Fits(SIZE_T at,SIZE_T length,SIZE_T size){return at<=size&&length<=size-at;}
bool StringAt(BYTE* base,SIZE_T size,DWORD rva,const char*& result){if(!Fits(rva,1,size))return false;auto limit=(size-rva)<4096?size-rva:4096;auto text=(const char*)base+rva;if(!memchr(text,0,limit))return false;result=text;return true;}
}
bool BindWinMainCommandImport(HMODULE module,const char* provider,void* replacement){
 if(!module||!provider||!replacement){SetLastError(ERROR_INVALID_PARAMETER);return false;}
 auto base=(BYTE*)module;auto dos=(IMAGE_DOS_HEADER*)base;
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<64||dos->e_lfanew>1024*1024){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
 auto nt=(IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);auto size=nt->OptionalHeader.SizeOfImage;
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||!Fits(dos->e_lfanew,sizeof(*nt),size)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
 auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!Fits(directory.VirtualAddress,directory.Size,size)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
 IMAGE_THUNK_DATA64* target=nullptr;
 for(SIZE_T at=0;Fits(at,sizeof(IMAGE_IMPORT_DESCRIPTOR),directory.Size);at+=sizeof(IMAGE_IMPORT_DESCRIPTOR)){
  auto d=(IMAGE_IMPORT_DESCRIPTOR*)(base+directory.VirtualAddress+at);if(!d->Name)break;const char* name=nullptr;if(!StringAt(base,size,d->Name,name)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}if(_stricmp(name,provider))continue;
  if(!d->OriginalFirstThunk||!d->FirstThunk){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
  for(unsigned i=0;i<32768;++i){SIZE_T original=(SIZE_T)d->OriginalFirstThunk+i*sizeof(IMAGE_THUNK_DATA64),slot=(SIZE_T)d->FirstThunk+i*sizeof(IMAGE_THUNK_DATA64);if(!Fits(original,sizeof(IMAGE_THUNK_DATA64),size)||!Fits(slot,sizeof(IMAGE_THUNK_DATA64),size)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
   auto symbol=((IMAGE_THUNK_DATA64*)(base+original))->u1.AddressOfData;if(!symbol)break;if(IMAGE_SNAP_BY_ORDINAL64(symbol))continue;if(symbol>MAXDWORD-2){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}const char* imported=nullptr;if(!StringAt(base,size,(DWORD)symbol+2,imported)){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
   if(!strcmp(imported,"_get_narrow_winmain_command_line")){if(target){SetLastError(ERROR_BAD_EXE_FORMAT);return false;}target=(IMAGE_THUNK_DATA64*)(base+slot);}
  }
 }
 if(!target){SetLastError(ERROR_PROC_NOT_FOUND);return false;}
 DWORD old=0;if(!VirtualProtectFromApp(target,sizeof(*target),PAGE_READWRITE,&old))return false;
 auto previous=InterlockedExchangePointer((void* volatile*)&target->u1.Function,replacement);DWORD unused=0;
 if(!VirtualProtectFromApp(target,sizeof(*target),old,&unused)){auto error=GetLastError();InterlockedExchangePointer((void* volatile*)&target->u1.Function,previous);VirtualProtectFromApp(target,sizeof(*target),old,&unused);SetLastError(error);return false;}
 return true;
}
