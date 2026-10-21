// Bounded, out-of-process Windows startup investigation. Never patches guest
// instructions, globals, PEB, return addresses or exception handling.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstring>
#include <filesystem>

namespace {
constexpr char kClientSha[]="9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478";
#ifdef MAIN_PIN_GENERATED
#include "MainPin.generated.h"
#else
constexpr char kMainImageSha[]="bf6a36ebfb80f2499f655b70b0c595ae505c8066d2add994564aba2532cbe5c0";
#endif
constexpr ULONG_PTR kEntry=0x369e098,kDispatch=0x5fa3a5d,kGuard=0x5f8d882,kValue=0x15bc7d8,kBase=0x159d368;
constexpr ULONG_PTR kInitializedBoundary=kEntry+9;
bool captureRequested=false;
struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);} };
bool Hash(const wchar_t* path,std::string& result){
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 bool ok=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0;
 std::ifstream file(path,std::ios::binary);std::vector<char> buffer(1024*1024);
 if(!file)ok=false;
 while(ok&&file){file.read(buffer.data(),buffer.size());auto n=file.gcount();if(n&&BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer.data()),static_cast<ULONG>(n),0)<0)ok=false;}
 if(file.bad())ok=false;
 BYTE digest[32]={};if(ok)ok=BCryptFinishHash(hash,digest,32,0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);
 if(ok){const char* hex="0123456789abcdef";for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}}return ok;
}
template<class T> bool Read(HANDLE process,ULONG_PTR address,T& result){SIZE_T size=0;return ReadProcessMemory(process,reinterpret_cast<void*>(address),&result,sizeof(result),&size)&&size==sizeof(result);}
ULONG_PTR ImageBase(DWORD process){
 Handle snapshot;snapshot.value=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,process);if(snapshot.value==INVALID_HANDLE_VALUE)return 0;
 MODULEENTRY32W module={};module.dwSize=sizeof(module);return Module32FirstW(snapshot.value,&module)?reinterpret_cast<ULONG_PTR>(module.modBaseAddr):0;
}
void Event(std::ofstream& out,const char* name,DWORD error){out<<"{\"event\":\""<<name<<"\",\"error\":"<<error<<"}\n";out.flush();}
void Locate(std::ofstream& out,DWORD process,ULONG_PTR address){
 Handle snapshot;snapshot.value=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,process);if(snapshot.value==INVALID_HANDLE_VALUE)return;
 MODULEENTRY32W module={};module.dwSize=sizeof(module);
 for(BOOL ok=Module32FirstW(snapshot.value,&module);ok;ok=Module32NextW(snapshot.value,&module)){
  auto base=reinterpret_cast<ULONG_PTR>(module.modBaseAddr);if(address<base||address-base>=module.modBaseSize)continue;
  std::string name;for(auto p=module.szModule;*p&&name.size()<96;++p){auto c=*p;name+=((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'_'||c==L'-')?static_cast<char>(c):'_';}
  out<<"{\"event\":\"fault_module\",\"name\":\""<<name<<"\",\"rva\":\""<<address-base<<"\"}\n";out.flush();return;
 }
}
bool Watch(HANDLE thread,ULONG_PTR base,bool owned){
 if(owned)return true;CONTEXT state={};state.ContextFlags=CONTEXT_DEBUG_REGISTERS;
 if(!GetThreadContext(thread,&state))return false;
 if(state.Dr0||state.Dr1||state.Dr2||state.Dr3||(state.Dr7&255)){SetLastError(ERROR_ALREADY_EXISTS);return false;}
 state.Dr0=base+kEntry;state.Dr1=base+kDispatch;state.Dr2=base+kGuard;state.Dr3=base+kInitializedBoundary;state.Dr7=(state.Dr7&~0xffff00ffULL)|85;
 return SetThreadContext(thread,&state)!=FALSE;
}
void Snapshot(std::ofstream& out,HANDLE process,HANDLE thread,ULONG_PTR base,const char* phase){
 CONTEXT context={};context.ContextFlags=CONTEXT_CONTROL|CONTEXT_INTEGER|CONTEXT_DEBUG_REGISTERS;
 if(!GetThreadContext(thread,&context)){Event(out,"context_unavailable",GetLastError());return;}
 DWORD value=0,checkLeft=0,checkRight=0;ULONGLONG storedBase=0,slot=0,frameBase=0;
 bool a=Read(process,base+kValue,value),b=Read(process,base+kBase,storedBase);
 bool c=Read(process,static_cast<ULONG_PTR>(context.Rbp+0x88),slot);
 bool d=Read(process,static_cast<ULONG_PTR>(context.Rbp+0x68),frameBase);
 bool checks=Read(process,base+0xb1d80a,checkLeft)&&Read(process,base+0x39c8da,checkRight);
 PROCESS_BASIC_INFORMATION basic={};ULONG returned=0;ULONG_PTR peb=0,pebImage=0;
 using Query=NTSTATUS(WINAPI*)(HANDLE,PROCESSINFOCLASS,PVOID,ULONG,PULONG);
 auto query=reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryInformationProcess"));
 if(query&&query(process,ProcessBasicInformation,&basic,sizeof(basic),&returned)>=0){peb=reinterpret_cast<ULONG_PTR>(basic.PebBaseAddress);Read(process,peb+0x10,pebImage);}
 // Numeric control-flow state only. No strings, credentials, command lines,
 // arbitrary memory dump, file contents or process environment are retained.
 out<<"{\"event\":\""<<phase<<"\",\"imageBase\":\""<<base<<"\",\"rip\":\""<<context.Rip
    <<"\",\"rsp\":\""<<context.Rsp<<"\",\"rbp\":\""<<context.Rbp<<"\",\"rax\":\""<<context.Rax
    <<"\",\"valueReadable\":"<<(a?"true":"false")<<",\"sourceLow32\":\""<<value
    <<"\",\"baseReadable\":"<<(b?"true":"false")<<",\"sourceBase64\":\""<<storedBase
    <<"\",\"slotReadable\":"<<(c?"true":"false")<<",\"frameSlot\":\""<<slot
    <<"\",\"rcx\":\""<<context.Rcx<<"\",\"frameBaseReadable\":"<<(d?"true":"false")<<",\"frameBase\":\""<<frameBase
    <<"\",\"rdx\":\""<<context.Rdx<<"\",\"r8\":\""<<context.R8
    <<"\",\"comparisonReadable\":"<<(checks?"true":"false")<<",\"comparisonEqual\":"<<(checks&&checkLeft==checkRight?"true":"false")
    <<",\"processPeb\":\""<<peb<<"\",\"pebImage\":\""<<pebImage<<"\",\"rcxMatchesPeb\":"<<(peb&&context.Rcx==peb?"true":"false")<<"}\n";out.flush();
}
#ifndef STARTUP_PROBE_LIBRARY
bool CaptureInitializedImage(HANDLE process,ULONG_PTR base,const wchar_t* sourcePath,const wchar_t* outputPath){
 DWORD left=0,right=0;
 if(!Read(process,base+0xb1d80a,left)||!Read(process,base+0x39c8da,right)||left!=right){SetLastError(ERROR_INVALID_STATE);return false;}
 std::ifstream input(sourcePath,std::ios::binary|std::ios::ate);if(!input)return false;auto length=input.tellg();if(length!=100729744){SetLastError(ERROR_INVALID_DATA);return false;}
 input.seekg(0);std::vector<BYTE> raw(static_cast<size_t>(length));if(!input.read(reinterpret_cast<char*>(raw.data()),length))return false;
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(raw.data());auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(raw.data()+dos->e_lfanew);
 if(dos->e_lfanew!=0x1d0||nt->FileHeader.NumberOfSections!=9||nt->OptionalHeader.SizeOfHeaders!=0x600||nt->OptionalHeader.AddressOfEntryPoint!=kEntry){SetLastError(ERROR_INVALID_DATA);return false;}
 std::filesystem::path directory(outputPath);if(std::filesystem::exists(directory)){SetLastError(ERROR_ALREADY_EXISTS);return false;}std::filesystem::create_directories(directory);
 auto write=[&](const wchar_t* name,ULONG_PTR address,SIZE_T count){
  std::ofstream file(directory/name,std::ios::binary|std::ios::trunc);if(!file)return false;BYTE buffer[65536];
  while(count){SIZE_T wanted=count<sizeof(buffer)?count:sizeof(buffer),read=0;if(!ReadProcessMemory(process,reinterpret_cast<void*>(address),buffer,wanted,&read)||read!=wanted)return false;file.write(reinterpret_cast<char*>(buffer),wanted);if(!file)return false;address+=wanted;count-=wanted;}return true;
 };
 if(!write(L"runtime-headers.bin",base,0x600))return false;
 {std::ofstream file(directory/L"source-headers.bin",std::ios::binary);file.write(reinterpret_cast<const char*>(raw.data()),0x600);if(!file)return false;}
 auto sections=IMAGE_FIRST_SECTION(nt);std::ofstream receipt(directory/L"capture.json");
 receipt<<"{\"format\":\"HollowwakeInitializedImage1\",\"originalSha256\":\""<<kClientSha<<"\",\"phase\":\"original_entry_first_call_return\",\"loadedBase\":\""<<base<<"\",\"preferredBase\":\""<<nt->OptionalHeader.ImageBase<<"\",\"originalEntryRva\":"<<kEntry<<",\"comparisonEqual\":true,\"sections\":[";
 for(unsigned i=0;i<9;++i){auto& section=sections[i];if(section.SizeOfRawData>128*1024*1024||section.PointerToRawData>raw.size()||section.SizeOfRawData>raw.size()-section.PointerToRawData){SetLastError(ERROR_INVALID_DATA);return false;}
  wchar_t name[32]={};swprintf_s(name,L"section-%02u.bin",i);if(!write(name,base+section.VirtualAddress,section.SizeOfRawData))return false;
  if(i)receipt<<',';receipt<<"{\"index\":"<<i<<",\"rva\":"<<section.VirtualAddress<<",\"rawOffset\":"<<section.PointerToRawData<<",\"rawBytes\":"<<section.SizeOfRawData<<",\"virtualBytes\":"<<section.Misc.VirtualSize<<",\"characteristics\":"<<section.Characteristics<<"}";
 }
 receipt<<"],\"protections\":[";bool firstRegion=true;ULONG_PTR cursor=base,end=base+nt->OptionalHeader.SizeOfImage;
 while(cursor<end){
  MEMORY_BASIC_INFORMATION region={};if(VirtualQueryEx(process,reinterpret_cast<void*>(cursor),&region,sizeof(region))!=sizeof(region)){SetLastError(ERROR_INVALID_DATA);return false;}
  auto start=reinterpret_cast<ULONG_PTR>(region.BaseAddress),limit=start+region.RegionSize;
  if(limit<=cursor||start>cursor){SetLastError(ERROR_INVALID_DATA);return false;}
  auto boundedEnd=limit<end?limit:end;
  if(region.State==MEM_COMMIT){
   if(region.Type!=MEM_IMAGE||region.AllocationBase!=reinterpret_cast<void*>(base)){SetLastError(ERROR_INVALID_DATA);return false;}
   if(!firstRegion)receipt<<',';firstRegion=false;
   receipt<<"{\"rva\":"<<cursor-base<<",\"bytes\":"<<boundedEnd-cursor<<",\"protect\":"<<region.Protect<<"}";
  }
  cursor=boundedEnd;
 }
 receipt<<"],\"captureComplete\":true,\"normalOriginalInitializationExecuted\":true,\"clientChecksPatched\":false,\"xboxTested\":false,\"lobbyDemonstrated\":false}";receipt.flush();return receipt.good();
}
#endif
}
#ifndef STARTUP_PROBE_LIBRARY
int wmain(int argc,wchar_t** argv){
 if(argc<4||argc>6)return 2;
 bool owned=argc==5&&std::wstring(argv[4])==L"--owned";
 bool plain=argc==5&&std::wstring(argv[4])==L"--plain";
 bool converted=argc==5&&std::wstring(argv[4])==L"--converted";
 bool mainImage=argc==5&&std::wstring(argv[4])==L"--main-image";
 captureRequested=argc==6&&std::wstring(argv[4])==L"--capture";
 if(argc==6&&!captureRequested)return 2;
 if(argc==5&&!owned&&!plain&&!converted&&!mainImage)return 2;
 std::wstring exe=argv[1],output=argv[2];auto slash=exe.find_last_of(L"/\\");if(slash==std::wstring::npos)return 2;
 if((owned?L"OwnedStartupFixture.exe":converted?L"ConvertedStartupHost.exe":L"FortniteClient-Win64-Shipping.exe")!=exe.substr(slash+1))return 2;
 DWORD seconds=static_cast<DWORD>(_wtoi(argv[3]));if(seconds<1||seconds>45)return 2;
 std::ofstream out(output,std::ios::trunc);if(!out)return 2;
 std::string sha;if(!Hash(exe.c_str(),sha)||(!owned&&!converted&&sha!=(mainImage?kMainImageSha:kClientSha))){Event(out,"input_pin_rejected",ERROR_INVALID_DATA);return 3;}
 out<<"{\"event\":\"input_verified\",\"sha256\":\""<<sha<<"\",\"ownedFixture\":"<<(owned?"true":"false")<<"}\n";out.flush();
 Handle job;job.value=CreateJobObjectW(nullptr,nullptr);if(!job.value){Event(out,"job_create_failed",GetLastError());return 4;}
 JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_ACTIVE_PROCESS|JOB_OBJECT_LIMIT_PROCESS_MEMORY;limits.BasicLimitInformation.ActiveProcessLimit=1;limits.ProcessMemoryLimit=2ULL*1024*1024*1024;
 if(!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){Event(out,"job_limit_failed",GetLastError());return 4;}
 wchar_t windows[MAX_PATH]={},temp[MAX_PATH]={};if(!GetWindowsDirectoryW(windows,MAX_PATH)||!GetTempPathW(MAX_PATH,temp))return 4;
 // Explicit child environment: never inherit GitHub tokens or CI credentials.
 std::vector<wchar_t> environment;
 for(auto row:{std::wstring(L"SystemRoot=")+windows,std::wstring(L"WINDIR=")+windows,std::wstring(L"TEMP=")+temp,std::wstring(L"TMP=")+temp,std::wstring(L"PATH=")+windows+L"\\System32"}){environment.insert(environment.end(),row.begin(),row.end());environment.push_back(0);}environment.push_back(0);
 std::wstring command=L"\""+exe+L"\" -windowed -d3d11 -ResX=640 -ResY=360";std::vector<wchar_t> commandBuffer(command.begin(),command.end());commandBuffer.push_back(0);
 STARTUPINFOW startup={};startup.cb=sizeof(startup);PROCESS_INFORMATION pi={};SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
 auto directory=exe.substr(0,slash);
 DWORD flags=CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT|(plain?0:DEBUG_ONLY_THIS_PROCESS);
 if(!CreateProcessW(exe.c_str(),commandBuffer.data(),nullptr,nullptr,FALSE,flags,environment.data(),directory.c_str(),&startup,&pi)){Event(out,"create_process_failed",GetLastError());return 5;}
 Handle process;process.value=pi.hProcess;Handle firstThread;firstThread.value=pi.hThread;
 if(!AssignProcessToJobObject(job.value,process.value)){auto error=GetLastError();TerminateProcess(process.value,4);Event(out,"job_assignment_failed",error);return 4;}
 ResumeThread(firstThread.value);ULONG_PTR imageBase=0;unsigned exceptions=0,hits=0;bool initialBreakpoint=true,exited=false;
 Handle entryThread;
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
 while(std::chrono::steady_clock::now()<deadline){
  if(plain){
   if(!imageBase){imageBase=ImageBase(pi.dwProcessId);if(imageBase){out<<"{\"event\":\"process_created\",\"imageBase\":\""<<imageBase<<"\"}\n";out.flush();}}
   if(WaitForSingleObject(process.value,100)==WAIT_OBJECT_0){DWORD code=0;GetExitCodeProcess(process.value,&code);out<<"{\"event\":\"process_exit\",\"code\":"<<code<<"}\n";exited=true;break;}continue;
  }
  DEBUG_EVENT event={};if(!WaitForDebugEvent(&event,100)){if(GetLastError()==ERROR_SEM_TIMEOUT)continue;Event(out,"debug_wait_failed",GetLastError());break;}
  DWORD disposition=DBG_CONTINUE;
  if(event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT){
   imageBase=converted?0:reinterpret_cast<ULONG_PTR>(event.u.CreateProcessInfo.lpBaseOfImage);
   if(event.u.CreateProcessInfo.hFile)CloseHandle(event.u.CreateProcessInfo.hFile);
   if(imageBase&&!Watch(event.u.CreateProcessInfo.hThread,imageBase,owned))Event(out,"hardware_watch_failed",GetLastError());
   out<<"{\"event\":\"process_created\",\"imageBase\":\""<<imageBase<<"\"}\n";
  }else if(event.dwDebugEventCode==CREATE_THREAD_DEBUG_EVENT){
   if(imageBase&&!Watch(event.u.CreateThread.hThread,imageBase,owned))Event(out,"thread_watch_failed",GetLastError());CloseHandle(event.u.CreateThread.hThread);
  }else if(event.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT){
   if(converted&&event.u.LoadDll.hFile){wchar_t path[32768]={};auto count=GetFinalPathNameByHandleW(event.u.LoadDll.hFile,path,32768,FILE_NAME_NORMALIZED);
    if(count&&count<32768){std::wstring name(path,count);auto at=name.find_last_of(L"/\\");name=name.substr(at==std::wstring::npos?0:at+1);
     if(name==L"Fortnite621.baseline.private.dll"){
      imageBase=reinterpret_cast<ULONG_PTR>(event.u.LoadDll.lpBaseOfDll);out<<"{\"event\":\"converted_image_loaded\",\"imageBase\":\""<<imageBase<<"\"}\n";out.flush();
      Handle thread;thread.value=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,event.dwThreadId);if(thread.value&&!Watch(thread.value,imageBase,false))Event(out,"converted_watch_failed",GetLastError());
     }
    }
   }
   if(event.u.LoadDll.hFile)CloseHandle(event.u.LoadDll.hFile);
  }else if(event.dwDebugEventCode==OUTPUT_DEBUG_STRING_EVENT){
   auto info=event.u.DebugString;
   if(converted&&!info.fUnicode&&info.nDebugStringLength>0&&info.nDebugStringLength<64){char text[64]={};SIZE_T read=0;
    if(ReadProcessMemory(process.value,info.lpDebugStringData,text,info.nDebugStringLength,&read)&&read==info.nDebugStringLength&&!strncmp(text,"HW_STARTUP_HOST stage=",22)){
     auto phase=atoi(text+22);if(phase>=1&&phase<=20){out<<"{\"event\":\"owned_host_stage\",\"stage\":"<<phase<<"}\n";out.flush();}
    }
   }
  }else if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){
   auto& info=event.u.Exception;auto& record=info.ExceptionRecord;ULONG_PTR address=reinterpret_cast<ULONG_PTR>(record.ExceptionAddress);
   if(record.ExceptionCode==EXCEPTION_SINGLE_STEP&&!owned&&address==imageBase+kInitializedBoundary){
    Handle thread;thread.value=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,event.dwThreadId);if(thread.value)Snapshot(out,process.value,thread.value,imageBase,"original_initialized_boundary");
    if(captureRequested){
     bool captured=false;try{captured=CaptureInitializedImage(process.value,imageBase,argv[1],argv[5]);}catch(...){SetLastError(ERROR_INVALID_DATA);}
     Event(out,captured?"initialized_image_captured":"initialized_capture_failed",captured?0:GetLastError());
     TerminateJobObject(job.value,WAIT_TIMEOUT);ContinueDebugEvent(event.dwProcessId,event.dwThreadId,DBG_CONTINUE);break;
    }
    if(thread.value){CONTEXT state={};state.ContextFlags=CONTEXT_CONTROL|CONTEXT_DEBUG_REGISTERS;if(GetThreadContext(thread.value,&state)){state.Dr7&=~64ULL;state.Dr6=0;state.EFlags|=0x10000;SetThreadContext(thread.value,&state);}}
   }else if(record.ExceptionCode==EXCEPTION_SINGLE_STEP&&!owned&&(address==imageBase+kEntry||address==imageBase+kDispatch||address==imageBase+kGuard)){
    Handle thread;thread.value=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,event.dwThreadId);
    if(address==imageBase+kEntry&&!entryThread.value)entryThread.value=OpenThread(THREAD_GET_CONTEXT|THREAD_SUSPEND_RESUME,FALSE,event.dwThreadId);
    if(thread.value){Snapshot(out,process.value,thread.value,imageBase,address==imageBase+kEntry?"original_entry":address==imageBase+kGuard?"helper_entry":"dispatch_before_calculation");CONTEXT state={};state.ContextFlags=CONTEXT_CONTROL|CONTEXT_DEBUG_REGISTERS;
     if(GetThreadContext(thread.value,&state)){state.EFlags|=0x10000;state.Dr6=0;state.Dr7&=address==imageBase+kEntry?~1ULL:address==imageBase+kGuard?~16ULL:~4ULL;SetThreadContext(thread.value,&state);}}
    if(++hits>16){Event(out,"hardware_watch_limit",0);ContinueDebugEvent(event.dwProcessId,event.dwThreadId,DBG_CONTINUE);break;}
   }else if(record.ExceptionCode==EXCEPTION_BREAKPOINT&&initialBreakpoint){initialBreakpoint=false;
   }else{
    disposition=DBG_EXCEPTION_NOT_HANDLED;
    if(++exceptions<=64){out<<"{\"event\":\"exception\",\"code\":"<<record.ExceptionCode<<",\"address\":\""<<address<<"\",\"firstChance\":"<<(info.dwFirstChance?"true":"false");
     if(record.ExceptionCode==EXCEPTION_ACCESS_VIOLATION&&record.NumberParameters>=2)out<<",\"operation\":"<<record.ExceptionInformation[0]<<",\"target\":\""<<record.ExceptionInformation[1]<<"\"";out<<"}\n";out.flush();}
    if(!owned&&(record.ExceptionCode==EXCEPTION_ACCESS_VIOLATION||!info.dwFirstChance)){Locate(out,pi.dwProcessId,address);Handle thread;thread.value=OpenThread(THREAD_GET_CONTEXT,FALSE,event.dwThreadId);if(thread.value)Snapshot(out,process.value,thread.value,imageBase,"exception_control_state");}
   }
  }else if(event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT){out<<"{\"event\":\"process_exit\",\"code\":"<<event.u.ExitProcess.dwExitCode<<"}\n";exited=true;}
  ContinueDebugEvent(event.dwProcessId,event.dwThreadId,disposition);if(exited)break;
 }
 FILETIME created={},ended={},kernel={},user={};if(GetProcessTimes(process.value,&created,&ended,&kernel,&user)){
  ULARGE_INTEGER k={},u={};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
  out<<"{\"event\":\"process_cpu\",\"kernelMilliseconds\":"<<k.QuadPart/10000<<",\"userMilliseconds\":"<<u.QuadPart/10000<<"}\n";
 }
 if(!exited){
  auto sampled=entryThread.value?entryThread.value:firstThread.value;
  if(!owned&&imageBase&&SuspendThread(sampled)!=static_cast<DWORD>(-1)){Snapshot(out,process.value,sampled,imageBase,"bounded_stop_control_state");ResumeThread(sampled);}
  Event(out,"bounded_stop",WAIT_TIMEOUT);TerminateJobObject(job.value,WAIT_TIMEOUT);
  // A debugged process cannot complete teardown while its exit debug event is
  // left pending. Drain events instead of waiting on a still-debug-stopped
  // process and returning with its executable mapping held open.
  if(!plain){
   auto cleanupDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
   while(std::chrono::steady_clock::now()<cleanupDeadline){
    DEBUG_EVENT tail={};if(!WaitForDebugEvent(&tail,100)){if(WaitForSingleObject(process.value,0)==WAIT_OBJECT_0)break;continue;}
    if(tail.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT&&tail.u.LoadDll.hFile)CloseHandle(tail.u.LoadDll.hFile);
    if(tail.dwDebugEventCode==CREATE_THREAD_DEBUG_EVENT)CloseHandle(tail.u.CreateThread.hThread);
    bool childEnded=tail.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT;
    ContinueDebugEvent(tail.dwProcessId,tail.dwThreadId,DBG_CONTINUE);if(childEnded)break;
   }
  }
  if(WaitForSingleObject(process.value,5000)!=WAIT_OBJECT_0){Event(out,"child_teardown_failed",GetLastError());return 5;}
 }
 out<<"{\"event\":\"scope\",\"nativeWindowsExecution\":true,\"debuggerAttached\":"<<(plain?"false":"true")<<",\"xboxTested\":false,\"lobbyDemonstrated\":false}\n";return 0;
}
#endif
