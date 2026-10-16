// Original metadata validation shared by the production loader and focused tests.
// Export-only shims contain RVAs/strings, not native instructions or absolute VAs.
#pragma once
#include <windows.h>
#include <cstring>
namespace Hollowwake {
inline bool IsForwarderOnly(const BYTE* base,SIZE_T size,const IMAGE_NT_HEADERS64& nt,const IMAGE_SECTION_HEADER* sections,unsigned count){
 auto fits=[size](SIZE_T r,SIZE_T n){return r<=size&&n<=size-r;};
 auto& o=nt.OptionalHeader;
 if(!(nt.FileHeader.Characteristics&IMAGE_FILE_DLL)||o.AddressOfEntryPoint||o.NumberOfRvaAndSizes<16)return false;
 for(unsigned i=0;i<count;++i)if(sections[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)return false;
 // Only passive export/resource/debug/signature metadata is permitted. In
 // particular reject TLS, imports, load configuration, exception tables and IATs.
 for(unsigned i=0;i<16;++i)if(i!=IMAGE_DIRECTORY_ENTRY_EXPORT&&i!=IMAGE_DIRECTORY_ENTRY_RESOURCE&&i!=IMAGE_DIRECTORY_ENTRY_SECURITY&&i!=IMAGE_DIRECTORY_ENTRY_DEBUG)
  if(o.DataDirectory[i].VirtualAddress||o.DataDirectory[i].Size)return false;
 auto d=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];if(d.Size<sizeof(IMAGE_EXPORT_DIRECTORY)||!fits(d.VirtualAddress,d.Size))return false;
 auto e=reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base+d.VirtualAddress);
 if(!e->NumberOfFunctions||e->NumberOfFunctions>65536||e->NumberOfNames>65536||!fits(e->AddressOfFunctions,(SIZE_T)e->NumberOfFunctions*4)||!fits(e->AddressOfNames,(SIZE_T)e->NumberOfNames*4)||!fits(e->AddressOfNameOrdinals,(SIZE_T)e->NumberOfNames*2))return false;
 auto functions=reinterpret_cast<const DWORD*>(base+e->AddressOfFunctions);unsigned forwarded=0;
 for(DWORD i=0;i<e->NumberOfFunctions;++i){DWORD r=functions[i];if(!r)continue;
  if(r<d.VirtualAddress||r-d.VirtualAddress>=d.Size)return false;
  SIZE_T remaining=d.Size-(r-d.VirtualAddress);auto text=reinterpret_cast<const char*>(base+r);auto end=static_cast<const char*>(memchr(text,0,remaining));if(!end||end==text||end-text>4096)return false;
  auto dot=static_cast<const char*>(memchr(text,'.',end-text));if(!dot||dot==text||dot+1==end)return false;
  for(auto p=text;p!=end;++p)if(*p=='/'||*p=='\\'||*p==':')return false;
  ++forwarded;
 }
 if(!forwarded)return false;
 auto names=reinterpret_cast<const DWORD*>(base+e->AddressOfNames);auto ordinals=reinterpret_cast<const WORD*>(base+e->AddressOfNameOrdinals);
 for(DWORD i=0;i<e->NumberOfNames;++i){if(ordinals[i]>=e->NumberOfFunctions||!fits(names[i],1))return false;auto remaining=size-names[i];if(remaining>4096)remaining=4096;if(!memchr(base+names[i],0,remaining))return false;}
 return true;
}
}
