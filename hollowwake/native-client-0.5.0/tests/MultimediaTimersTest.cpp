#include "../Native/MultimediaTimers.h"
#include <atomic>
#include <thread>
#include <stdexcept>
#include <iostream>
struct State {HANDLE fired=CreateEventW(nullptr,TRUE,FALSE,nullptr),release=CreateEventW(nullptr,TRUE,FALSE,nullptr);std::atomic<unsigned> calls{0},id{0};std::atomic<bool> abi{true};bool block=false,selfKill=false;~State(){CloseHandle(fired);CloseHandle(release);}};
void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void CALLBACK Callback(UINT id,UINT message,DWORD_PTR value,DWORD_PTR one,DWORD_PTR two){auto s=reinterpret_cast<State*>(value);s->id=id;s->abi=s->abi&&!message&&!one&&!two;s->calls++;if(s->selfKill)s->abi=s->abi&&HWTimeKillEvent(id)==0;SetEvent(s->fired);if(s->block)WaitForSingleObject(s->release,5000);}
int main(){try{
 Check(!HWTimeSetEvent(0,0,Callback,0,0),"zero delay admitted");Check(!HWTimeSetEvent(1,0,nullptr,0,0),"null callback admitted");Check(!HWTimeSetEvent(1,0,Callback,0,0x30),"contradictory event modes admitted");Check(HWTimeKillEvent(0)!=0,"unknown timer killed successfully");
 State once;auto start=GetTickCount64();auto id=HWTimeSetEvent(40,0,Callback,reinterpret_cast<DWORD_PTR>(&once),0);Check(id!=0,"one-shot allocation failed");Check(WaitForSingleObject(once.fired,5000)==WAIT_OBJECT_0,"one-shot callback missing");Check(once.id==id&&once.abi&&GetTickCount64()-start>=20,"wrong callback ABI or synchronous execution");Sleep(80);Check(once.calls==1,"one-shot repeated");
 State periodic;id=HWTimeSetEvent(15,0,Callback,reinterpret_cast<DWORD_PTR>(&periodic),0x101);Check(id!=0,"periodic allocation failed");auto deadline=GetTickCount64()+5000;while(periodic.calls<3&&GetTickCount64()<deadline)Sleep(5);Check(periodic.calls>=3,"periodic callbacks missing");Check(HWTimeKillEvent(id)==0,"periodic cancellation failed");auto stopped=periodic.calls.load();Sleep(60);Check(periodic.calls==stopped,"callback after synchronous cancellation");
 State cancelled;id=HWTimeSetEvent(500,0,Callback,reinterpret_cast<DWORD_PTR>(&cancelled),0x100);Check(id&&HWTimeKillEvent(id)==0,"pending cancellation failed");Check(WaitForSingleObject(cancelled.fired,600)==WAIT_TIMEOUT,"cancelled timer fired");
 State self;self.selfKill=true;id=HWTimeSetEvent(10,0,Callback,reinterpret_cast<DWORD_PTR>(&self),0x101);Check(id&&WaitForSingleObject(self.fired,5000)==WAIT_OBJECT_0&&self.abi,"self-cancel deadlocked/failed");Sleep(60);Check(self.calls==1,"self-cancel repeated");
 State blocked;blocked.block=true;id=HWTimeSetEvent(10,0,Callback,reinterpret_cast<DWORD_PTR>(&blocked),0x101);Check(id&&WaitForSingleObject(blocked.fired,5000)==WAIT_OBJECT_0,"blocking callback not entered");std::atomic<bool> killed{false};std::thread killer([&]{Check(HWTimeKillEvent(id)==0,"blocking cancellation failed");killed=true;});Sleep(50);Check(!killed,"synchronous kill returned before callback completed");SetEvent(blocked.release);killer.join();Check(killed,"synchronous kill never completed");
 HANDLE event=CreateEventW(nullptr,TRUE,FALSE,nullptr);Check(event!=nullptr,"owned event creation failed");id=HWTimeSetEvent(20,0,reinterpret_cast<HWTimeCallback>(event),0,0x10);Check(id&&WaitForSingleObject(event,5000)==WAIT_OBJECT_0,"event-set delivery missing");CloseHandle(event);Sleep(40);
 for(unsigned i=0;i<150;++i){State s;auto timer=HWTimeSetEvent(20,0,Callback,reinterpret_cast<DWORD_PTR>(&s),0x101);Check(timer&&HWTimeKillEvent(timer)==0,"create/cancel race failed");Check(!s.calls,"cancelled future callback ran");}
 Check(ResolveMultimediaTimerImport("WINMM.dll","timeSetEvent")==reinterpret_cast<void*>(&HWTimeSetEvent)&&!ResolveMultimediaTimerImport("WINMM.dll","unknown"),"wrong import boundary");
 std::cout<<"PASS real timer elapsed time, ABI/userdata, one-shot, periodic, pending/synchronous/self cancellation, event delivery and create/cancel races. No game/Xbox execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
