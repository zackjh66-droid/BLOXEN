// Native ABI bridge for real CoreWindow touch points. No fabricated HID data.
struct TouchPoint {LONG x,y;HANDLE source;DWORD id,flags,mask,time;ULONG_PTR extra;DWORD contactWidth,contactHeight;};
static_assert(sizeof(TouchPoint)==48,"x64 TOUCHINPUT ABI");
static bool touchRegistered=false;static ULONG_PTR nextTouch=1;static int touchSource;
static std::unordered_map<ULONG_PTR,TouchPoint> touchPackets;
extern "C" __declspec(dllexport) void __cdecl HWTouch(unsigned id,int x100,int y100,unsigned flags,unsigned milliseconds){
 std::lock_guard<std::recursive_mutex> l(mutex);if(!window||!window->alive||!hostActive||!touchRegistered)return;
 if(flags!=1&&flags!=2&&flags!=4){SetLastError(ERROR_INVALID_PARAMETER);return;}
 if(touchPackets.size()>=4096||queue.size()>=4096||nextTouch==0||nextTouch==(ULONG_PTR)-1){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return;}
 auto token=nextTouch++;touchPackets.emplace(token,TouchPoint{x100,y100,(HANDLE)&touchSource,id,flags,0,milliseconds,0,0,0});
 if(!Push(Handle(),0x240,1,(LPARAM)token))touchPackets.erase(token);
}
static BOOL WINAPI RegisterTouch(HWND h,ULONG flags){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}
 // CoreWindow does not promise uncoalesced/palm contacts: reject those requests.
 if(flags){SetLastError(ERROR_NOT_SUPPORTED);return FALSE;}touchRegistered=true;return TRUE;
}
static BOOL WINAPI UnregisterTouch(HWND h){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}touchRegistered=false;touchPackets.clear();queue.erase(std::remove_if(queue.begin(),queue.end(),[](const Msg& m){return m.message==0x240;}),queue.end());return TRUE;}
static BOOL WINAPI TouchInfo(HANDLE input,UINT count,TouchPoint* out,int size){std::lock_guard<std::recursive_mutex> l(mutex);if(size!=sizeof(TouchPoint)||!out||count<1){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}auto entry=touchPackets.find((ULONG_PTR)input);if(entry==touchPackets.end()){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}*out=entry->second;return TRUE;}
static BOOL WINAPI CloseTouch(HANDLE input){std::lock_guard<std::recursive_mutex> l(mutex);if(!touchPackets.erase((ULONG_PTR)input)){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}return TRUE;}
