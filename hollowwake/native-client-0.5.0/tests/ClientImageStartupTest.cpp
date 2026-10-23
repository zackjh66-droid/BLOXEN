#include "../Native/ClientImageStartup.h"
#include <iostream>
__declspec(noinline) int OwnedImageCode(){return 621;}
int main(){
 HMODULE module=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&OwnedImageCode),&module))return 1;
 auto base=reinterpret_cast<BYTE*>(module);auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
 SYSTEM_INFO info={};GetNativeSystemInfo(&info);if(info.dwPageSize!=4096)return 2;
 auto address=reinterpret_cast<ULONG_PTR>(&OwnedImageCode),page=address&~static_cast<ULONG_PTR>(4095);auto rva=page-reinterpret_cast<ULONG_PTR>(module);
 MEMORY_BASIC_INFORMATION before={};if(!VirtualQuery(reinterpret_cast<void*>(page),&before,sizeof(before)))return 3;
 ClientImageProtection rule={static_cast<DWORD>(rva),4096,PAGE_EXECUTE_READWRITE};
 auto result=ApplyClientImageStartup(module,nt->OptionalHeader.SizeOfImage,&rule,1,&VirtualQuery,&VirtualProtect,nullptr);
 MEMORY_BASIC_INFORMATION copyState={};VirtualQuery(reinterpret_cast<void*>(page),&copyState,sizeof(copyState));
 // An unmodified MEM_IMAGE page may remain PAGE_EXECUTE_WRITECOPY after a
 // successful RWX transition. Trigger actual COW with a same-byte owned write;
 // verify the private writable state, unchanged code bytes and execution.
 bool sameByteWrite=false;
 if(SUCCEEDED(result)){
  auto instruction=reinterpret_cast<volatile BYTE*>(address);BYTE original=*instruction;*instruction=original;sameByteWrite=*instruction==original;
  FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),1);
 }
 MEMORY_BASIC_INFORMATION after={};bool observed=VirtualQuery(reinterpret_cast<void*>(page),&after,sizeof(after))==sizeof(after)&&after.Protect==PAGE_EXECUTE_READWRITE;

 int value=OwnedImageCode();DWORD unused=0;bool restored=VirtualProtect(reinterpret_cast<void*>(page),4096,before.Protect,&unused)!=FALSE;
 ClientImageProtection invalid={static_cast<DWORD>(rva+1),4096,PAGE_EXECUTE_READ};
 bool bounds=ApplyClientImageStartup(module,nt->OptionalHeader.SizeOfImage,&invalid,1,&VirtualQuery,&VirtualProtect,nullptr)==E_INVALIDARG;
 bool missing=ApplyClientImageStartup(module,nt->OptionalHeader.SizeOfImage,&rule,1,nullptr,&VirtualProtect,nullptr)==HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
 // A real private allocation is not a signed module image and must be refused.
 auto privateMemory=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!privateMemory)return 4;
 ClientImageProtection privateRule={0,4096,PAGE_READWRITE};auto rejected=ApplyClientImageStartup(reinterpret_cast<HMODULE>(privateMemory),4096,&privateRule,1,&VirtualQuery,&VirtualProtect,nullptr);VirtualFree(privateMemory,0,MEM_RELEASE);
 std::cout<<"imageHr="<<static_cast<unsigned long>(result)<<" beforeWriteProtect="<<copyState.Protect<<" sameByteWrite="<<sameByteWrite<<" observed="<<observed<<" observedProtect="<<after.Protect<<" value="<<value<<" restored="<<restored<<" bounds="<<bounds<<" missing="<<missing<<" privateHr="<<static_cast<unsigned long>(rejected)<<" expectedPrivateHr="<<static_cast<unsigned long>(HRESULT_FROM_WIN32(ERROR_INVALID_ADDRESS))<<"\n";
 bool passed=SUCCEEDED(result)&&sameByteWrite&&observed&&value==621&&restored&&bounds&&missing&&rejected==HRESULT_FROM_WIN32(ERROR_INVALID_ADDRESS);
 std::cout<<(passed?"PASS":"FAIL")<<" actual owned image page transition/execution/restoration, malformed range/provider and non-image rejection; no Fortnite or Xbox proof\n";return passed?0:5;
}
