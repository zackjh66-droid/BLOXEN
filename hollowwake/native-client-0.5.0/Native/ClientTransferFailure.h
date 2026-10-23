#pragma once
#include "CrashLocation.h"
#include <cstring>
namespace Hollowwake {
// Exact-build, read-only reconstruction of the already observed failed stack
// transfer. Unlike a debug breakpoint, this runs only after the OS reports the
// original access violation. It never changes context or exception disposition.
inline std::string DescribeClientTransferFailure(EXCEPTION_POINTERS const* fault,HMODULE module,SIZE_T imageBytes,CrashModuleLookup lookup,CrashModuleFilename filename,CrashReadMemory read){
#if defined(_M_X64)
 if(!fault||!fault->ExceptionRecord||!fault->ContextRecord||!module||!read||imageBytes!=0x6314000)return {};
 auto const& record=*fault->ExceptionRecord;auto const& context=*fault->ContextRecord;
 if(record.ExceptionCode!=EXCEPTION_ACCESS_VIOLATION||record.NumberParameters<2)return {};
 ULONG_PTR base=reinterpret_cast<ULONG_PTR>(module);
 if(base>~ULONG_PTR(0)-0x141775ae0ULL||context.Rsp<0x90)return {};
 const auto target=base+0x141775ae0ULL;
 const bool canonical=(target>>47)==0||(target>>47)==0x1ffff;
 const bool executeFault=record.ExceptionInformation[0]==8&&record.ExceptionInformation[1]==target&&context.Rip==target;
 // x64 reports a noncanonical indirect branch as a general-protection fault
 // at the branch instruction, represented by Windows as a read of UINT64_MAX.
 // Require that exact form AND the original branch bytes/retained target slots.
 const bool noncanonicalFault=!canonical&&record.ExceptionInformation[0]==0&&record.ExceptionInformation[1]==~ULONG_PTR(0)&&context.Rip==base+0x609753b;
 if(!executeFault&&!noncanonicalFault)return {};
 auto process=reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1));
 auto get=[&](ULONG_PTR address,void* output,SIZE_T bytes){SIZE_T received=0;return address<=~ULONG_PTR(0)-bytes&&read(process,reinterpret_cast<LPCVOID>(address),output,bytes,&received)&&received==bytes;};
 const BYTE expected[]={0x48,0x8b,0x45,0x38,0x48,0x03,0x05,0xfc,0x52,0x3b,0xfb};
 const BYTE jump[]={0xff,0x64,0x24,0xf8};BYTE instruction[sizeof(expected)]={},tail[sizeof(jump)]={};
 if(!get(base+0x6097501,instruction,sizeof(instruction))||memcmp(instruction,expected,sizeof(expected))||
    !get(base+0x609753b,tail,sizeof(tail))||memcmp(tail,jump,sizeof(jump)))return {};
 // The original epilogue advances RSP from frame+0x80 to frame+0x90.
 // Require both retained target slots and the exact arithmetic before treating
 // the preceding frame as this helper's frame. No general stack dump is made.
 ULONG_PTR frame=context.Rsp-0x90,first=0,slot=0,operand=0,constant=0;
 if(!get(frame,&first,8)||!get(context.Rsp-8,&slot,8)||first!=target||slot!=target||
    !get(frame+0x38,&operand,8)||!get(base+0x144c808,&constant,8)||operand+constant!=target)return {};
 std::string text="\nNATIVE_CLIENT_TRANSFER_FAILURE observedAfterFault=true debuggerRequired=false helperRva=101283073 operand="+std::to_string(operand)+" constant="+std::to_string(constant)+" canonical="+(canonical?"true":"false");
 ULONG_PTR left=0,right=0;LONG index=0;
 if(!get(frame+0x48,&left,8)||!get(frame+0x50,&right,8)||!get(frame+0x70,&index,4))return text+" comparisonFrameReadable=false";
 text+=" comparisonIndex="+std::to_string(index);
 auto l=CrashCodeLocation(left,lookup,filename),r=CrashCodeLocation(right,lookup,filename);
 if(!l.empty())text+=" leftArray={"+l+"}";if(!r.empty())text+=" rightArray={"+r+"}";
 DWORD leftValue=0,rightValue=0;ULONG_PTR offset=index>=0?static_cast<ULONG_PTR>(index)*4:0;
 bool values=index>=0&&index<65536&&left&&right&&left<=~ULONG_PTR(0)-offset&&right<=~ULONG_PTR(0)-offset&&
             get(left+offset,&leftValue,4)&&get(right+offset,&rightValue,4);
 text+=std::string(" indexedValuesReadable=")+(values?"true":"false");
 if(values)text+=" leftValue="+std::to_string(leftValue)+" rightValue="+std::to_string(rightValue);
 // Retained producer context is evidence, not proof of the last executed
 // predecessor. Export only bounded image RVAs and recognized base identities;
 // no arbitrary bytes, stack dump, account strings or process environment.
 ULONG_PTR cursor=0,normalization=0;DWORD sourceRva=0;
 if(get(frame+0x40,&cursor,8)&&get(frame+0x18,&normalization,8)&&cursor>=base&&cursor-base<=imageBytes-4&&
    get(cursor,&sourceRva,4)&&sourceRva<=imageBytes-8){
  text+=" retainedSourceEntryRva="+std::to_string(cursor-base)+" retainedSourceRva="+std::to_string(sourceRva);
  const char* identity=normalization==base?"loaded":normalization==0x140000000ULL?"preferred":normalization==base-0x140000000ULL?"delta":normalization==0?"zero":"other";
  text+=std::string(" retainedNormalization=")+identity;
  ULONG_PTR value=0;if(get(base+sourceRva,&value,8)&&value>=base&&value-base<imageBytes)text+=" retainedSourcePointerRva="+std::to_string(value-base);
 }
 // Other predecessors exist. These values alone do not identify which branch
 // was taken or establish why it was taken.
 return text;
#else
 return {};
#endif
}
}
