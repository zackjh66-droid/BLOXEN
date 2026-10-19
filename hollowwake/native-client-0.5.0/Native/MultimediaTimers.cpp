// Original multimedia timer adapter. Real OS threadpool scheduling, callbacks,
// cancellation and ownership. No fake timer IDs or synchronous callback shortcut.
#include "MultimediaTimers.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <cstring>
#include <algorithm>
namespace {
constexpr UINT Periodic=1,EventSet=0x10,EventPulse=0x20,Synchronous=0x100;
using Pulse=BOOL(WINAPI*)(HANDLE);
struct Timer {UINT id=0,flags=0;HWTimeCallback callback=nullptr;DWORD_PTR user=0;PTP_TIMER object=nullptr;PTP_WORK cleanup=nullptr;Pulse pulse=nullptr;HANDLE event=nullptr;std::atomic<bool> closing{false};~Timer(){if(cleanup)CloseThreadpoolWork(cleanup);if(event)CloseHandle(event);}};
std::mutex gate;std::map<UINT,std::shared_ptr<Timer>> timers,owned;UINT nextId=0;
thread_local UINT activeId=0;thread_local PTP_CALLBACK_INSTANCE activeInstance=nullptr;
// A non-synchronous cancellation must not wait for a blocked user callback.
// The preallocated cleanup object and ownership map retain the callback context
// until the OS has actually drained all callbacks; no queued raw context dangles.
void CALLBACK Cleanup(PTP_CALLBACK_INSTANCE,void* context,PTP_WORK){
 auto raw=static_cast<Timer*>(context);std::shared_ptr<Timer> state;
 {std::lock_guard<std::mutex> lock(gate);auto found=owned.find(raw->id);if(found==owned.end())return;state=found->second;}
 WaitForThreadpoolTimerCallbacks(state->object,TRUE);CloseThreadpoolTimer(state->object);state->object=nullptr;
 {std::lock_guard<std::mutex> lock(gate);owned.erase(state->id);}
}
UINT Retire(UINT id){
 std::shared_ptr<Timer> state;bool synchronous=false;
 {std::lock_guard<std::mutex> lock(gate);auto found=timers.find(id);if(found==timers.end())return 97;state=found->second;synchronous=(state->flags&Synchronous)!=0;state->closing=true;
  // The separate ownership node was preallocated at creation.
  timers.erase(found);}
 SetThreadpoolTimer(state->object,nullptr,0,0);
 if(!synchronous){SubmitThreadpoolWork(state->cleanup);return 0;}
 if(activeId==id&&activeInstance){DisassociateCurrentThreadFromCallback(activeInstance);activeInstance=nullptr;}
 WaitForThreadpoolTimerCallbacks(state->object,TRUE);CloseThreadpoolTimer(state->object);state->object=nullptr;
 {std::lock_guard<std::mutex> lock(gate);owned.erase(id);}
 return 0;
}
void CALLBACK Fire(PTP_CALLBACK_INSTANCE instance,void* context,PTP_TIMER){
 auto raw=static_cast<Timer*>(context);std::shared_ptr<Timer> state;
 {std::lock_guard<std::mutex> lock(gate);auto found=timers.find(raw->id);if(found==timers.end()||found->second.get()!=raw)return;state=found->second;}
 auto oldId=activeId;auto oldInstance=activeInstance;activeId=state->id;activeInstance=instance;
 if(!state->closing.load()){
  if(state->flags&EventSet)SetEvent(reinterpret_cast<HANDLE>(state->callback));
  else if(state->flags&EventPulse)state->pulse(reinterpret_cast<HANDLE>(state->callback));
  else state->callback(state->id,0,state->user,0,0);
 }
 if(!(state->flags&Periodic))Retire(state->id);
 activeId=oldId;activeInstance=oldInstance;
}
}
UINT WINAPI HWTimeSetEvent(UINT delay,UINT resolution,HWTimeCallback callback,DWORD_PTR user,UINT flags){
 if(!delay||delay>0x7fffffff||!callback||(flags&~(Periodic|EventSet|EventPulse|Synchronous))||((flags&EventSet)&&(flags&EventPulse))){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
 try{
  auto state=std::make_shared<Timer>();state->flags=flags;state->callback=callback;state->user=user;
  if(flags&EventPulse){state->pulse=reinterpret_cast<Pulse>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","PulseEvent"));if(!state->pulse){SetLastError(ERROR_NOT_SUPPORTED);return 0;}}
  // Duplicate event ownership, so a caller closing its own handle does not turn
  // the queued event into an access violation. A failed duplicate is an error.
  HANDLE event=nullptr;
  if(flags&(EventSet|EventPulse)){if(!DuplicateHandle(GetCurrentProcess(),reinterpret_cast<HANDLE>(callback),GetCurrentProcess(),&event,EVENT_MODIFY_STATE,FALSE,0))return 0;state->event=event;state->callback=reinterpret_cast<HWTimeCallback>(event);}
  std::unique_lock<std::mutex> lock(gate);
  if(owned.size()>=65535){SetLastError(ERROR_NOT_ENOUGH_QUOTA);return 0;}
  do{++nextId;}while(!nextId||timers.count(nextId)||owned.count(nextId));state->id=nextId;
  state->cleanup=CreateThreadpoolWork(&Cleanup,state.get(),nullptr);if(!state->cleanup)return 0;
  state->object=CreateThreadpoolTimer(&Fire,state.get(),nullptr);
  if(!state->object){return 0;}
  try{owned.emplace(state->id,state);timers.emplace(state->id,state);}catch(...){timers.erase(state->id);owned.erase(state->id);CloseThreadpoolTimer(state->object);throw;}
  // Relative 100 ns due time, actual recurring period and allowed coalescing.
  LARGE_INTEGER due;due.QuadPart=-static_cast<LONGLONG>(delay)*10000;FILETIME time={due.LowPart,static_cast<DWORD>(due.HighPart)};
  SetThreadpoolTimer(state->object,&time,flags&Periodic?delay:0,(std::min)(resolution,delay));
  return state->id;
 }catch(const std::bad_alloc&){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
}
UINT WINAPI HWTimeKillEvent(UINT id){return Retire(id);}
unsigned HWActiveMultimediaTimersForTest(){std::lock_guard<std::mutex> lock(gate);return static_cast<unsigned>(owned.size());}
void* ResolveMultimediaTimerImport(const char* dll,const char* name){if(!dll||!name||_stricmp(dll,"winmm.dll"))return nullptr;if(!strcmp(name,"timeSetEvent"))return reinterpret_cast<void*>(&HWTimeSetEvent);if(!strcmp(name,"timeKillEvent"))return reinterpret_cast<void*>(&HWTimeKillEvent);return nullptr;}

#include <fstream>
namespace {
struct ProbeState {HANDLE event=nullptr;std::atomic<unsigned> count{0};std::atomic<bool> abi{true};};
void CALLBACK ProbeCallback(UINT id,UINT message,DWORD_PTR value,DWORD_PTR one,DWORD_PTR two){auto state=reinterpret_cast<ProbeState*>(value);if(!id||message||one||two)state->abi=false;if(++state->count>=3)SetEvent(state->event);}
}
extern "C" __declspec(dllexport) int __cdecl HWMultimediaTimerProbe(const wchar_t* path){
 if(!path)return E_POINTER;ProbeState state;state.event=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!state.event)return HRESULT_FROM_WIN32(GetLastError());
 auto start=GetTickCount64();auto id=HWTimeSetEvent(15,0,ProbeCallback,reinterpret_cast<DWORD_PTR>(&state),Periodic|Synchronous);DWORD error=0;
 if(!id)error=GetLastError()?GetLastError():ERROR_GEN_FAILURE;
 else if(WaitForSingleObject(state.event,5000)!=WAIT_OBJECT_0)error=ERROR_TIMEOUT;
 auto elapsed=GetTickCount64()-start;auto killed=id?HWTimeKillEvent(id):97;auto count=state.count.load();Sleep(45);auto after=state.count.load();CloseHandle(state.event);
 bool ok=!error&&!killed&&state.abi&&count>=3&&after==count&&elapsed>=20;
 std::ofstream out(path,std::ios::trunc);if(!out)return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
 out<<"{\"scope\":\"Real threadpool timer in current owned UWP host; not game/Xbox execution\",\"calls\":"<<count<<",\"afterCancel\":"<<after<<",\"elapsedMs\":"<<elapsed<<",\"abi\":"<<(state.abi?"true":"false")<<",\"cancelResult\":"<<killed<<",\"error\":"<<error<<",\"pass\":"<<(ok?"true":"false")<<",\"gameExecuted\":false}";out.flush();return !out.good()?HRESULT_FROM_WIN32(ERROR_WRITE_FAULT):ok?S_OK:HRESULT_FROM_WIN32(error?error:ERROR_INVALID_DATA);
}
