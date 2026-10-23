// Internal Windows-only investigation of the exact prepared client. This is
// never packaged or run on the user's Xbox. No guest instructions, data, PEB,
// exception disposition or authentication response is replaced.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <thread>

namespace {
constexpr ULONG_PTR transferRva=0x6097501,constantRva=0x144c808;
struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
template<class T> bool Read(HANDLE process,ULONG_PTR address,T& value){SIZE_T received=0;return ReadProcessMemory(process,reinterpret_cast<void*>(address),&value,sizeof(value),&received)&&received==sizeof(value);}
bool Hash(const wchar_t* path,const std::string& expected){
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;BYTE digest[32]={};
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 bool ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
 std::ifstream input(path,std::ios::binary);std::vector<char> bytes(1024*1024);if(!input)ok=false;
 while(ok&&input){input.read(bytes.data(),bytes.size());auto count=input.gcount();if(count&&BCryptHashData(hash,reinterpret_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(count),0)<0)ok=false;}
 if(input.bad())ok=false;if(ok)ok=BCryptFinishHash(hash,digest,32,0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
 std::string actual;const char* hex="0123456789abcdef";for(auto byte:digest){actual+=hex[byte>>4];actual+=hex[byte&15];}return ok&&actual==expected;
}
bool Watch(HANDLE thread,ULONG_PTR base){
 CONTEXT context={};context.ContextFlags=CONTEXT_DEBUG_REGISTERS;
 if(!GetThreadContext(thread,&context)||context.Dr0||(context.Dr7&3))return false;
 context.Dr0=base+transferRva;context.Dr7=(context.Dr7&~0xf0003ULL)|1;return SetThreadContext(thread,&context)!=FALSE;
}
void Clear(HANDLE thread,ULONG_PTR base){
 CONTEXT context={};context.ContextFlags=CONTEXT_DEBUG_REGISTERS|CONTEXT_CONTROL;
 if(GetThreadContext(thread,&context)&&context.Dr0==base+transferRva){context.Dr0=0;context.Dr7&=~3ULL;context.Dr6&=~1ULL;context.EFlags|=0x10000;SetThreadContext(thread,&context);}
}
void Threads(DWORD pid,ULONG_PTR base,bool clear,bool running=false){
 Handle snapshot;snapshot.value=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);THREADENTRY32 entry={};entry.dwSize=sizeof(entry);
 if(snapshot.value==INVALID_HANDLE_VALUE)return;
 for(BOOL next=Thread32First(snapshot.value,&entry);next;next=Thread32Next(snapshot.value,&entry))if(entry.th32OwnerProcessID==pid){
  Handle thread;thread.value=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|(running?THREAD_SUSPEND_RESUME:0),FALSE,entry.th32ThreadID);if(thread.value){
   if(running&&SuspendThread(thread.value)==static_cast<DWORD>(-1))continue;
   if(clear)Clear(thread.value,base);else Watch(thread.value,base);
   if(running)ResumeThread(thread.value);
  }
 }
}
void Location(std::ofstream& output,DWORD pid,ULONG_PTR address){
 Handle snapshot;snapshot.value=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);MODULEENTRY32W entry={};entry.dwSize=sizeof(entry);if(snapshot.value==INVALID_HANDLE_VALUE)return;
 for(BOOL next=Module32FirstW(snapshot.value,&entry);next;next=Module32NextW(snapshot.value,&entry)){
  auto base=reinterpret_cast<ULONG_PTR>(entry.modBaseAddr);if(address<base||address-base>=entry.modBaseSize)continue;
  std::string name;for(auto p=entry.szModule;*p&&name.size()<96;++p){auto c=*p;name+=((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'_'||c==L'-')?static_cast<char>(c):'_';}
  output<<",\"module\":\""<<name<<"\",\"rva\":"<<address-base;return;
 }
}

// Read only three build-specific numeric fields. No debugger, thread suspension,
// context changes, arbitrary dumps, account data or writes are used in this mode.
int ObserveOriginalState(HANDLE process,DWORD pid,const std::wstring& expectedPath,std::ofstream& output,unsigned seconds){
 ULONG_PTR main=0;DWORD imageBytes=0;
 Handle snapshot;snapshot.value=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);
 MODULEENTRY32W entry={};entry.dwSize=sizeof(entry);
 if(snapshot.value==INVALID_HANDLE_VALUE)return 7;
 for(BOOL next=Module32FirstW(snapshot.value,&entry);next;next=Module32NextW(snapshot.value,&entry)){
  if(!_wcsicmp(entry.szExePath,expectedPath.c_str())){main=reinterpret_cast<ULONG_PTR>(entry.modBaseAddr);imageBytes=entry.modBaseSize;break;}
 }
 IMAGE_DOS_HEADER dos={};IMAGE_NT_HEADERS64 nt={};
 if(!main||imageBytes<sizeof(nt)||!Read(process,main,dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||
    static_cast<ULONG>(dos.e_lfanew)>imageBytes-sizeof(nt)||!Read(process,main+dos.e_lfanew,nt)||
    nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
    nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||imageBytes<0x6097543)return 7;
 const ULONG_PTR imageBaseField=main+dos.e_lfanew+offsetof(IMAGE_NT_HEADERS64,OptionalHeader)+offsetof(IMAGE_OPTIONAL_HEADER64,ImageBase);
 output<<"{\"event\":\"readonly_source_observer\",\"base\":"<<main<<",\"imageBytes\":"<<imageBytes
       <<",\"debuggerAttached\":false,\"processMemoryWritten\":false,\"threadsSuspended\":false}\n";output.flush();
 auto started=std::chrono::steady_clock::now();auto deadline=started+std::chrono::seconds(seconds);
 unsigned samples=0,validSamples=0;
 do{
  ULONGLONG header=0,constant=0,preferred=0;
  bool h=Read(process,imageBaseField,header),c=Read(process,main+constantRva,constant),p=Read(process,main+0x159d368,preferred);
  if(h&&c&&p)++validSamples;
  auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
  output<<"{\"event\":\"readonly_source_state\",\"milliseconds\":"<<elapsed
        <<",\"headerRead\":"<<(h?"true":"false")<<",\"headerImageBase\":"<<header
        <<",\"constantRead\":"<<(c?"true":"false")<<",\"transferConstant\":"<<constant
        <<",\"preferredRead\":"<<(p?"true":"false")<<",\"preferredField\":"<<preferred<<"}\n";output.flush();++samples;
  if(WaitForSingleObject(process,0)==WAIT_OBJECT_0){output<<"{\"event\":\"readonly_source_exit\"}\n";break;}
  std::this_thread::sleep_for(std::chrono::seconds(2));
 }while(std::chrono::steady_clock::now()<deadline);
 output<<"{\"event\":\"readonly_source_complete\",\"samples\":"<<samples<<"}\n";
 return validSamples?0:8;
}
}
int wmain(int argc,wchar_t** argv){
 bool observe=argc==7&&!wcscmp(argv[6],L"--observe-original-state");
 bool original=observe||(argc==7&&!wcscmp(argv[6],L"--original-source"));if(argc!=6&&!original)return 2;
 wchar_t* end=nullptr;auto parsed=wcstoul(argv[1],&end,10);if(!parsed||!end||*end)return 2;DWORD pid=parsed;
 std::wstring root=argv[2],expectedPath=root+(original?L"\\FortniteGame\\Binaries\\Win64\\FortniteClient-Win64-Shipping.exe":L"\\6.21\\FortniteGame\\Binaries\\Win64\\Fortnite621.imports.private.dll");
 std::wstring wideHash=argv[3];std::string hash(wideHash.begin(),wideHash.end());if(hash.size()!=64||hash.find_first_not_of("0123456789abcdef")!=std::string::npos||!Hash(expectedPath.c_str(),hash))return 3;
 if(original&&hash!="9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478")return 3;
 Handle process;process.value=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ|SYNCHRONIZE,FALSE,pid);if(!process.value)return 4;
 wchar_t image[32768]={};DWORD length=32768;if(!QueryFullProcessImageNameW(process.value,0,image,&length))return 4;
 std::wstring actual(image,length);if(actual.size()<=root.size()||_wcsnicmp(actual.c_str(),root.c_str(),root.size())||actual[root.size()]!=L'\\')return 4;
 if(original&&_wcsicmp(actual.c_str(),expectedPath.c_str()))return 4;
 auto seconds=wcstoul(argv[5],&end,10);if(!end||*end||seconds<10||seconds>300)return 2;
 std::ofstream output(argv[4],std::ios::binary);if(!output)return 5;
 if(observe)return ObserveOriginalState(process.value,pid,expectedPath,output,seconds);
 if(!DebugActiveProcess(pid)){output<<"{\"event\":\"attach_failed\",\"error\":"<<GetLastError()<<"}\n";return 6;}
 DebugSetProcessKillOnExit(FALSE);output<<"{\"event\":\"attached\",\"pid\":"<<pid<<",\"guestInstructionsChanged\":false,\"xboxTested\":false}\n";output.flush();
 ULONG_PTR main=0;bool initial=true,measured=false,exited=false;unsigned exceptions=0;
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
 while(std::chrono::steady_clock::now()<deadline){
  DEBUG_EVENT event={};if(!WaitForDebugEvent(&event,200)){if(GetLastError()==ERROR_SEM_TIMEOUT)continue;break;}
  DWORD disposition=DBG_CONTINUE;
  if(event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT){
   output<<"{\"event\":\"host_image\",\"base\":"<<reinterpret_cast<ULONG_PTR>(event.u.CreateProcessInfo.lpBaseOfImage)<<"}\n";
   if(original){main=reinterpret_cast<ULONG_PTR>(event.u.CreateProcessInfo.lpBaseOfImage);output<<"{\"event\":\"original_source_image\",\"base\":"<<main<<",\"sha256\":\""<<hash<<"\"}\n";}
   if(event.u.CreateProcessInfo.hFile)CloseHandle(event.u.CreateProcessInfo.hFile);
   if(event.u.CreateProcessInfo.hProcess)CloseHandle(event.u.CreateProcessInfo.hProcess);
   if(event.u.CreateProcessInfo.hThread)CloseHandle(event.u.CreateProcessInfo.hThread);
  }else if(event.dwDebugEventCode==CREATE_THREAD_DEBUG_EVENT){
   if(main&&!measured)Watch(event.u.CreateThread.hThread,main);CloseHandle(event.u.CreateThread.hThread);
  }else if(event.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT){
   if(event.u.LoadDll.hFile){wchar_t path[32768]={};auto count=GetFinalPathNameByHandleW(event.u.LoadDll.hFile,path,32768,FILE_NAME_NORMALIZED);
    if(count&&count<32768){std::wstring name(path,count);if(name.compare(0,4,L"\\\\?\\")==0)name=name.substr(4);
     if(!_wcsicmp(name.c_str(),expectedPath.c_str())){
      main=reinterpret_cast<ULONG_PTR>(event.u.LoadDll.lpBaseOfDll);
      BYTE instruction[11]={};SIZE_T size=0;const BYTE expected[]={0x48,0x8b,0x45,0x38,0x48,0x03,0x05,0xfc,0x52,0x3b,0xfb};
      // The exact operand bytes are verified below from this pinned image.
      if(!ReadProcessMemory(process.value,reinterpret_cast<void*>(main+transferRva),instruction,11,&size)||size!=11||memcmp(instruction,expected,11)){output<<"{\"event\":\"instruction_mismatch\"}\n";measured=true;}
      else Threads(pid,main,false);
      output<<"{\"event\":\"guest_image\",\"base\":"<<main<<",\"sha256\":\""<<hash<<"\"}\n";
     }
    }CloseHandle(event.u.LoadDll.hFile);
   }
  }else if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){
   auto& info=event.u.Exception;auto& record=info.ExceptionRecord;auto address=reinterpret_cast<ULONG_PTR>(record.ExceptionAddress);
   if(record.ExceptionCode==EXCEPTION_BREAKPOINT&&initial){initial=false;if(original&&main)Threads(pid,main,false);}
   else if(!measured&&main&&record.ExceptionCode==EXCEPTION_SINGLE_STEP&&address==main+transferRva){
    Handle thread;thread.value=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,event.dwThreadId);CONTEXT context={};context.ContextFlags=CONTEXT_CONTROL|CONTEXT_INTEGER|CONTEXT_DEBUG_REGISTERS;
    if(thread.value&&GetThreadContext(thread.value,&context)&&(context.Dr6&1)){
     BYTE instruction[11]={};SIZE_T received=0;const BYTE expected[]={0x48,0x8b,0x45,0x38,0x48,0x03,0x05,0xfc,0x52,0x3b,0xfb};
     if(!ReadProcessMemory(process.value,reinterpret_cast<void*>(address),instruction,sizeof(instruction),&received)||received!=sizeof(instruction)||memcmp(instruction,expected,sizeof(instruction))){
      output<<"{\"event\":\"transfer_instruction_mismatch\"}\n";measured=true;Clear(thread.value,main);Threads(pid,main,true);output.flush();ContinueDebugEvent(event.dwProcessId,event.dwThreadId,DBG_CONTINUE);continue;
     }
     ULONG_PTR operand=0,constant=0,headerBase=0,sourceBase=0;bool a=Read(process.value,context.Rbp+0x38,operand),b=Read(process.value,main+constantRva,constant);
     output<<"{\"event\":\"observed_transfer_calculation\",\"mainBase\":"<<main<<",\"operandReadable\":"<<(a?"true":"false")<<",\"constantReadable\":"<<(b?"true":"false");
     if(a&&b){auto target=operand+constant;output<<",\"operand\":"<<operand<<",\"constant\":"<<constant<<",\"computedTarget\":"<<target<<",\"matchesCurrentImageDelta\":"<<(operand==main-0x140000000ULL?"true":"false");Location(output,pid,target);}
     if(Read(process.value,main+0x200,headerBase))output<<",\"runtimeHeaderImageBase\":"<<headerBase;
     if(Read(process.value,main+0x159d368,sourceBase))output<<",\"sourcePreferredBaseField\":"<<sourceBase;
     output<<",\"threadId\":"<<event.dwThreadId<<",\"eaxLow32\":"<<static_cast<DWORD>(context.Rax)<<",\"edxLow32\":"<<static_cast<DWORD>(context.Rdx)
           <<",\"carryFlag\":"<<((context.EFlags&1)?"true":"false")<<",\"zeroFlag\":"<<((context.EFlags&0x40)?"true":"false");
     // The predecessor can compare two indexed DWORDs before selecting this
     // transfer. Record its actual inputs, never replace the branch/values.
     ULONG_PTR left=0,right=0,caller=0;LONG index=0;
     bool frame=Read(process.value,context.Rbp+0x48,left)&&Read(process.value,context.Rbp+0x50,right)&&Read(process.value,context.Rbp+0x70,index);
     output<<",\"comparisonFrameReadable\":"<<(frame?"true":"false");
     if(frame){
      output<<",\"comparisonIndex\":"<<index<<",\"leftArray\":{\"address\":"<<left;Location(output,pid,left);output<<"},\"rightArray\":{\"address\":"<<right;Location(output,pid,right);output<<'}';
      DWORD l=0,r=0;bool values=index>=0&&index<65536&&left&&right&&left<=~ULONG_PTR(0)-static_cast<ULONG_PTR>(index)*4&&right<=~ULONG_PTR(0)-static_cast<ULONG_PTR>(index)*4&&Read(process.value,left+static_cast<ULONG_PTR>(index)*4,l)&&Read(process.value,right+static_cast<ULONG_PTR>(index)*4,r);
      output<<",\"indexedValuesReadable\":"<<(values?"true":"false");
      if(values)output<<",\"leftValue\":"<<l<<",\"rightValue\":"<<r<<",\"indexedValuesMatchRegisters\":"<<((l==static_cast<DWORD>(context.Rax)&&r==static_cast<DWORD>(context.Rdx))?"true":"false");
     }
     if(Read(process.value,context.Rbp+0x88,caller)){output<<",\"frameReturn\":{\"address\":"<<caller;Location(output,pid,caller);output<<'}';}
     output<<"}\n";measured=true;Clear(thread.value,main);Threads(pid,main,true);
    }else disposition=DBG_EXCEPTION_NOT_HANDLED;
   }else{
    disposition=DBG_EXCEPTION_NOT_HANDLED;
    if(record.ExceptionCode==EXCEPTION_ACCESS_VIOLATION||!info.dwFirstChance||exceptions++<8){output<<"{\"event\":\"exception\",\"code\":"<<record.ExceptionCode<<",\"address\":"<<address<<",\"firstChance\":"<<(info.dwFirstChance?"true":"false");Location(output,pid,address);
     if(record.ExceptionCode==EXCEPTION_ACCESS_VIOLATION&&record.NumberParameters>=2)output<<",\"operation\":"<<record.ExceptionInformation[0]<<",\"target\":"<<record.ExceptionInformation[1];output<<"}\n";}
   }
  }else if(event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT){output<<"{\"event\":\"process_exit\",\"code\":"<<event.u.ExitProcess.dwExitCode<<"}\n";exited=true;}
  output.flush();ContinueDebugEvent(event.dwProcessId,event.dwThreadId,disposition);if(exited)break;
 }
 if(!exited){if(main)Threads(pid,main,true,true);if(!DebugActiveProcessStop(pid)){output<<"{\"event\":\"detach_failed\",\"error\":"<<GetLastError()<<"}\n";return 7;}output<<"{\"event\":\"detached\"}\n";}
 return 0;
}
