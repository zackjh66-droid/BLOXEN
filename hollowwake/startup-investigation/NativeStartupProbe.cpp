// Bounded, out-of-process Windows startup investigation. Never patches guest
// instructions, globals, PEB, return addresses or exception handling.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <tlhelp32.h>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstring>

namespace {
constexpr char kClientSha[]="9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478";
constexpr ULONG_PTR kEntry=0x369e098,kDispatch=0x5fa3a5d,kGuard=0x5f8d882,kValue=0x15bc7d8,kBase=0x159d368;
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
bool Watch(HANDLE thread,ULONG_PTR base,bool owned){
 if(owned)return true;CONTEXT state={};state.ContextFlags=CONTEXT_DEBUG_REGISTERS;
 if(!GetThreadContext(thread,&state))return false;
 if(state.Dr0||state.Dr1||state.Dr2||(state.Dr7&63)){SetLastError(ERROR_ALREADY_EXISTS);return false;}
 state.Dr0=base+kEntry;state.Dr1=base+kDispatch;state.Dr2=base+kGuard;state.Dr7=(state.Dr7&~0xfff003fULL)|21;
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
 // Numeric control-flow state only. No strings, credentials, command lines,
 // arbitrary memory dump, file contents or process environment are retained.
 out<<"{\"event\":\""<<phase<<"\",\"imageBase\":\""<<base<<"\",\"rip\":\""<<context.Rip
    <<"\",\"rsp\":\""<<context.Rsp<<"\",\"rbp\":\""<<context.Rbp<<"\",\"rax\":\""<<context.Rax
    <<"\",\"valueReadable\":"<<(a?"true":"false")<<",\"sourceLow32\":\""<<value
    <<"\",\"baseReadable\":"<<(b?"true":"false")<<",\"sourceBase64\":\""<<storedBase
    <<"\",\"slotReadable\":"<<(c?"true":"false")<<",\"frameSlot\":\""<<slot
    <<"\",\"rcx\":\""<<context.Rcx<<"\",\"frameBaseReadable\":"<<(d?"true":"false")<<",\"frameBase\":\""<<frameBase
    <<"\",\"comparisonReadable\":"<<(checks?"true":"false")<<",\"comparisonEqual\":"<<(checks&&checkLeft==checkRight?"true":"false")<<"}\n";out.flush();
}
}
#ifndef STARTUP_PROBE_LIBRARY
int wmain(int argc,wchar_t** argv){
 if(argc<4||argc>5)return 2;
 bool owned=argc==5&&std::wstring(argv[4])==L"--owned";
 bool plain=argc==5&&std::wstring(argv[4])==L"--plain";
 bool converted=argc==5&&std::wstring(argv[4])==L"--converted";
 if(argc==5&&!owned&&!plain&&!converted)return 2;
 std::wstring exe=argv[1],output=argv[2];auto slash=exe.find_last_of(L"/\\");if(slash==std::wstring::npos)return 2;
 if((owned?L"OwnedStartupFixture.exe":converted?L"ConvertedStartupHost.exe":L"FortniteClient-Win64-Shipping.exe")!=exe.substr(slash+1))return 2;
 DWORD seconds=static_cast<DWORD>(_wtoi(argv[3]));if(seconds<1||seconds>45)return 2;
 std::ofstream out(output,std::ios::trunc);if(!out)return 2;
 std::string sha;if(!Hash(exe.c_str(),sha)||(!owned&&!converted&&sha!=kClientSha)){Event(out,"input_pin_rejected",ERROR_INVALID_DATA);return 3;}
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
  }else if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){
   auto& info=event.u.Exception;auto& record=info.ExceptionRecord;ULONG_PTR address=reinterpret_cast<ULONG_PTR>(record.ExceptionAddress);
   if(record.ExceptionCode==EXCEPTION_SINGLE_STEP&&!owned&&(address==imageBase+kEntry||address==imageBase+kDispatch||address==imageBase+kGuard)){
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
    if(!owned&&(record.ExceptionCode==EXCEPTION_ACCESS_VIOLATION||!info.dwFirstChance)){Handle thread;thread.value=OpenThread(THREAD_GET_CONTEXT,FALSE,event.dwThreadId);if(thread.value)Snapshot(out,process.value,thread.value,imageBase,"exception_control_state");}
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
  Event(out,"bounded_stop",WAIT_TIMEOUT);TerminateJobObject(job.value,WAIT_TIMEOUT);WaitForSingleObject(process.value,5000);
 }
 out<<"{\"event\":\"scope\",\"nativeWindowsExecution\":true,\"debuggerAttached\":"<<(plain?"false":"true")<<",\"xboxTested\":false,\"lobbyDemonstrated\":false}\n";return 0;
}
#endif
