#include "GuestThreads.h"
#include <mutex>
#include <algorithm>
#include <stdexcept>
namespace Hollowwake { namespace {
struct Module {HMODULE image;bool staticTls;GuestTlsSeed seed;std::vector<PIMAGE_TLS_CALLBACK> callbacks;GuestDllEntry entry;bool ready=false,disabled=false;};
std::recursive_mutex gate;
std::vector<Module> modules;
void(*logger)(const std::string&)=nullptr;
bool initializationActive=false,initializationReady=false;
DWORD initializationThread=0;
thread_local std::vector<size_t> attached;
thread_local bool entered=false,leaving=false;
void Log(const char* action){if(logger)logger(std::string("THREAD ")+action+" tid="+std::to_string(GetCurrentThreadId()));}
}
void ConfigureGuestThreads(void(*log)(const std::string&)){std::lock_guard<std::recursive_mutex> lock(gate);logger=log;}
void BeginGuestInitialization(){
 gate.lock();
 if(initializationActive||initializationReady){gate.unlock();throw std::runtime_error("Guest initialization may only begin once");}
 initializationActive=true;initializationThread=GetCurrentThreadId();
}
void EndGuestInitialization(bool success){
 // Called on the initialization owner only, including its exception cleanup.
 if(!initializationActive||initializationThread!=GetCurrentThreadId())throw std::runtime_error("Guest initialization ownership mismatch");
 initializationReady=success;initializationActive=false;gate.unlock();
}
void RegisterGuestThreadModule(HMODULE image,bool staticTls,GuestTlsSeed seed,const std::vector<PIMAGE_TLS_CALLBACK>& callbacks,GuestDllEntry entry){
 std::lock_guard<std::recursive_mutex> lock(gate);
 if(!initializationActive||GetCurrentThreadId()!=initializationThread||!image||modules.size()>=96)throw std::runtime_error("Invalid guest thread module registration");
 for(auto& m:modules)if(m.image==image)throw std::runtime_error("Duplicate guest thread module registration");
 modules.push_back({image,staticTls,seed,callbacks,entry});
}
void MarkGuestThreadModuleReady(HMODULE image){std::lock_guard<std::recursive_mutex> lock(gate);for(auto& m:modules)if(m.image==image){m.ready=true;return;}throw std::runtime_error("Unknown guest thread module");}
bool EnterGuestThread(){
 std::lock_guard<std::recursive_mutex> lock(gate);
 if(!initializationReady){SetLastError(ERROR_DLL_INIT_FAILED);return false;}
 if(entered){SetLastError(ERROR_ALREADY_INITIALIZED);return false;}
 // Seed every donor before invoking any guest callback: another module's TLS
 // may be read from the first callback. OS donor callbacks contain no guest code.
 for(auto& m:modules)if(!m.ready||(m.seed&&!m.seed())){SetLastError(ERROR_DLL_INIT_FAILED);return false;}
 entered=true;attached.clear();Log("ATTACH_BEGIN");
 for(size_t i=0;i<modules.size();++i){auto& m=modules[i];for(auto callback:m.callbacks)callback(m.image,DLL_THREAD_ATTACH,nullptr);if(m.entry&&!m.disabled)m.entry(m.image,DLL_THREAD_ATTACH,nullptr);attached.push_back(i);}
 Log("ATTACH_DONE");return true;
}
void LeaveGuestThread(){
 std::lock_guard<std::recursive_mutex> lock(gate);if(!entered||leaving)return;leaving=true;Log("DETACH_BEGIN");
 for(auto i=attached.rbegin();i!=attached.rend();++i){auto& m=modules[*i];if(m.entry&&!m.disabled)m.entry(m.image,DLL_THREAD_DETACH,nullptr);for(auto callback:m.callbacks)callback(m.image,DLL_THREAD_DETACH,nullptr);}
 attached.clear();entered=false;leaving=false;Log("DETACH_DONE");
}
BOOL WINAPI GuestDisableThreadLibraryCalls(HMODULE image){std::lock_guard<std::recursive_mutex> lock(gate);for(auto& m:modules)if(m.image==image){if(m.staticTls){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}m.disabled=true;return TRUE;}SetLastError(ERROR_MOD_NOT_FOUND);return FALSE;}
}
