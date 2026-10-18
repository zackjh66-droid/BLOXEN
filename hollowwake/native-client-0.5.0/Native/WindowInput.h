// Original CoreWindow pointer/raw-mouse and single-display implementation.
// Included by WindowBridge.cpp so all state shares the window queue lock.
#include <unordered_map>
struct RawDevice {USHORT page,usage;DWORD flags;HWND target;};
struct RawHeader {DWORD type,size;HANDLE device;WPARAM code;};
struct RawMouse {USHORT flags;union{ULONG buttons;struct{USHORT buttonFlags,buttonData;};};ULONG rawButtons;LONG x,y;ULONG extra;};
struct RawPacket {RawHeader header;RawMouse mouse;};
struct RawList {HANDLE device;DWORD type;};
struct RawInfo {DWORD size,type,id,buttons,rate;BOOL horizontalWheel;};
static_assert(sizeof(RawHeader)==24&&sizeof(RawMouse)==24&&sizeof(RawPacket)==48,"Windows x64 RAWINPUT ABI");
static int mouseIdentity;static bool mouseObserved=false;static POINT pointerPosition={};static bool pointerKnown=false;static unsigned pointerButtons=0;
static RawDevice mouseRegistration={};static bool mouseRegistered=false;static ULONG_PTR nextInput=1;
static std::unordered_map<ULONG_PTR,RawPacket> rawPackets;
using PointerWarp=BOOL(__cdecl*)(int,int);using PointerVisibility=BOOL(__cdecl*)(int);
static PointerWarp warpPointer=nullptr;static PointerVisibility showPointer=nullptr;static int cursorBalance=0;
extern "C" __declspec(dllexport) void __cdecl HWConfigurePointer(PointerWarp warp,PointerVisibility visible){std::lock_guard<std::recursive_mutex> l(mutex);warpPointer=warp;showPointer=visible;}
static bool RawEvent(LONG dx,LONG dy,USHORT buttons,SHORT wheel){
 if(!mouseRegistered||!window||!window->alive||!hostActive)return false;
 if(rawPackets.size()>=4096||queue.size()>=4096){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return false;}
 // Monotonic opaque tokens prevent dereferencing caller-provided handles.
 if(nextInput==0||nextInput==(ULONG_PTR)-1){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return false;}auto token=nextInput++;
 RawPacket p={};p.header={0,sizeof(RawPacket),(HANDLE)&mouseIdentity,0};p.mouse.x=dx;p.mouse.y=dy;p.mouse.buttonFlags=buttons;p.mouse.buttonData=(USHORT)wheel;
 rawPackets.emplace(token,p);if(!Push(mouseRegistration.target?mouseRegistration.target:Handle(),0xff,0,(LPARAM)token)){rawPackets.erase(token);return false;}return true;
}
extern "C" __declspec(dllexport) void __cdecl HWMouseDelta(int dx,int dy){std::lock_guard<std::recursive_mutex> l(mutex);mouseObserved=true;if(dx||dy)RawEvent(dx,dy,0,0);}
extern "C" __declspec(dllexport) void __cdecl HWPointer(int x,int y,unsigned buttons,int wheel){
 std::lock_guard<std::recursive_mutex> l(mutex);mouseObserved=true;pointerPosition={x,y};pointerKnown=true;
 auto old=pointerButtons;pointerButtons=buttons&0x1f;if(!window||!window->alive||!hostActive)return;
 USHORT rawFlags=0;const UINT messages[5][2]={{0x202,0x201},{0x205,0x204},{0x208,0x207},{0x20c,0x20b},{0x20c,0x20b}};
 WPARAM state=(pointerButtons&1?1:0)|(pointerButtons&2?2:0)|(pointerButtons&4?0x10:0)|(keys[16]?4:0)|(keys[17]?8:0)|(pointerButtons&8?0x20:0)|(pointerButtons&16?0x40:0);
 bool legacy=!mouseRegistered||(mouseRegistration.flags&0x30)!=0x30;
 if(legacy)Push(Handle(),0x200,state,MAKELPARAM(x,y));
 for(unsigned i=0;i<5;++i)if((old^pointerButtons)&(1u<<i)){bool down=(pointerButtons&(1u<<i))!=0;rawFlags|=(USHORT)(1u<<(2*i+(down?0:1)));if(legacy)Push(Handle(),messages[i][down?1:0],state|(i>=3?((WPARAM)(i-2)<<16):0),MAKELPARAM(x,y));}
 if(wheel){rawFlags|=0x400;if(legacy)Push(Handle(),0x20a,state|((WPARAM)(USHORT)(SHORT)wheel<<16),MAKELPARAM(x,y));}
 if(rawFlags)RawEvent(0,0,rawFlags,(SHORT)wheel);
}
static UINT WINAPI RawData(HANDLE input,UINT command,void* data,UINT* size,UINT header){
 std::lock_guard<std::recursive_mutex> l(mutex);if(!size||header!=sizeof(RawHeader)||(command!=0x10000003&&command!=0x10000005)){SetLastError(ERROR_INVALID_PARAMETER);return (UINT)-1;}
 auto found=rawPackets.find((ULONG_PTR)input);if(found==rawPackets.end()){SetLastError(ERROR_INVALID_HANDLE);return (UINT)-1;}UINT required=command==0x10000005?sizeof(RawHeader):sizeof(RawPacket);
 if(!data){*size=required;return 0;}if(*size<required){*size=required;SetLastError(ERROR_INSUFFICIENT_BUFFER);return (UINT)-1;}
 memcpy(data,&found->second,required);*size=required;return required;
}
static UINT WINAPI RawDevices(RawList* list,UINT* count,UINT itemSize){
 std::lock_guard<std::recursive_mutex> l(mutex);if(!count||itemSize!=sizeof(RawList)){SetLastError(ERROR_INVALID_PARAMETER);return (UINT)-1;}UINT required=mouseObserved?1:0;
 if(!list){*count=required;return 0;}if(*count<required){*count=required;SetLastError(ERROR_INSUFFICIENT_BUFFER);return (UINT)-1;}if(required)*list={(HANDLE)&mouseIdentity,0};*count=required;return required;
}
static UINT WINAPI RawDeviceInfoA(HANDLE device,UINT command,void* data,UINT* size){
 std::lock_guard<std::recursive_mutex> l(mutex);if(!size||device!=(HANDLE)&mouseIdentity||!mouseObserved){SetLastError(ERROR_INVALID_HANDLE);return (UINT)-1;}
 if(command==0x20000007){const char name[]="Hollowwake:CoreWindowMouse";UINT required=sizeof(name);if(!data){*size=required;return 0;}if(*size<required){*size=required;SetLastError(ERROR_INSUFFICIENT_BUFFER);return (UINT)-1;}memcpy(data,name,required);*size=required;return required-1;}
 // WinRT supplies no hardware ID/button-count/rate; do not invent RID_DEVICE_INFO.
 SetLastError(ERROR_NOT_SUPPORTED);return (UINT)-1;
}
static BOOL WINAPI RegisterRaw(const RawDevice* devices,UINT count,UINT itemSize){
 std::lock_guard<std::recursive_mutex> l(mutex);if(!devices||!count||count>64||itemSize!=sizeof(RawDevice)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 RawDevice staged=mouseRegistration;bool enabled=mouseRegistered;
 for(UINT i=0;i<count;++i){auto& d=devices[i];if(d.page!=1||d.usage!=2||(d.flags!=0&&d.flags!=1&&d.flags!=0x30)){SetLastError(ERROR_NOT_SUPPORTED);return FALSE;}if(d.flags==1){if(d.target){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}enabled=false;staged={};}else{if(d.target&&!Valid(d.target)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}staged=d;enabled=true;}}
 mouseRegistration=staged;mouseRegistered=enabled;return TRUE;
}
static UINT WINAPI RegisteredRaw(RawDevice* devices,UINT* count,UINT size){std::lock_guard<std::recursive_mutex> l(mutex);if(!count||size!=sizeof(RawDevice)){SetLastError(ERROR_INVALID_PARAMETER);return (UINT)-1;}UINT required=mouseRegistered?1:0;if(!devices){*count=required;return 0;}if(*count<required){*count=required;SetLastError(ERROR_INSUFFICIENT_BUFFER);return (UINT)-1;}if(required)*devices=mouseRegistration;*count=required;return required;}
static BOOL WINAPI CursorPosition(POINT* point){std::lock_guard<std::recursive_mutex> l(mutex);if(!point){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}if(!pointerKnown){SetLastError(ERROR_NOT_READY);return FALSE;}*point=pointerPosition;return TRUE;}
static BOOL WINAPI WarpCursor(int x,int y){PointerWarp call=nullptr;{std::lock_guard<std::recursive_mutex> l(mutex);if(!hostActive||x<0||y<0||x>=width||y>=height){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}call=warpPointer;}if(!call){SetLastError(ERROR_NOT_SUPPORTED);return FALSE;}if(!call(x,y))return FALSE;std::lock_guard<std::recursive_mutex> l(mutex);pointerPosition={x,y};pointerKnown=true;return TRUE;}
static int WINAPI CursorVisibility(BOOL visible){std::lock_guard<std::recursive_mutex> l(mutex);if(!showPointer){SetLastError(ERROR_NOT_SUPPORTED);return cursorBalance;}if((visible&&cursorBalance==INT_MAX)||(!visible&&cursorBalance==INT_MIN)){SetLastError(ERROR_ARITHMETIC_OVERFLOW);return cursorBalance;}int next=cursorBalance+(visible?1:-1);if((cursorBalance>=0)!=(next>=0)&&!showPointer(next>=0))return cursorBalance;cursorBalance=next;return cursorBalance;}
// TranslateMessage is not advertised: host CharacterReceived is already forwarded;
// complete Win32 keyboard translation/IME semantics need a separate implementation.
struct DisplayW {DWORD size;wchar_t name[32],description[128];DWORD flags;wchar_t id[128],key[128];};
struct DisplayA {DWORD size;char name[32],description[128];DWORD flags;char id[128],key[128];};
struct ModeW {wchar_t name[32];WORD spec,driver,size,extra;DWORD fields;LONG x,y;DWORD orientation,fixedOutput;short color,duplex,yres,tt,collate;wchar_t form[32];WORD dpi;DWORD bits,w,h,flags,frequency,icmMethod,icmIntent,media,dither,res1,res2,panningW,panningH;};
static_assert(sizeof(DisplayW)==840&&sizeof(DisplayA)==424&&sizeof(ModeW)==220,"Windows display ABI");
static bool DisplayName(const wchar_t* name){return !name||!_wcsicmp(name,L"\\\\.\\DISPLAY1");}
static BOOL WINAPI DisplayDevicesW(LPCWSTR name,DWORD index,DisplayW* out,DWORD flags){std::lock_guard<std::recursive_mutex> l(mutex);if(!out||out->size!=sizeof(DisplayW)||flags){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}if(name||index||width<=0||height<=0){SetLastError(ERROR_NO_MORE_ITEMS);return FALSE;}*out={};out->size=sizeof(*out);wcscpy_s(out->name,L"\\\\.\\DISPLAY1");wcscpy_s(out->description,L"Hollowwake CoreWindow presentation surface");out->flags=5;return TRUE;}
static BOOL WINAPI DisplayDevicesA(LPCSTR name,DWORD index,DisplayA* out,DWORD flags){std::lock_guard<std::recursive_mutex> l(mutex);if(!out||out->size!=sizeof(DisplayA)||flags){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}if(name||index||width<=0||height<=0){SetLastError(ERROR_NO_MORE_ITEMS);return FALSE;}*out={};out->size=sizeof(*out);strcpy_s(out->name,"\\\\.\\DISPLAY1");strcpy_s(out->description,"Hollowwake CoreWindow presentation surface");out->flags=5;return TRUE;}
static BOOL WINAPI DisplaySettings(LPCWSTR name,DWORD index,ModeW* out){std::lock_guard<std::recursive_mutex> l(mutex);if(!out||out->size!=sizeof(ModeW)||out->extra||!DisplayName(name)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}if((index!=0&&index!=0xffffffff)||width<=0||height<=0){SetLastError(ERROR_NO_MORE_ITEMS);return FALSE;}*out={};wcscpy_s(out->name,L"HollowwakeCoreWindow");out->size=sizeof(*out);out->spec=0x401;out->fields=0x40000|0x80000|0x100000;out->bits=32;out->w=width;out->h=height;return TRUE;}
static LONG WINAPI ChangeSettingsEx(LPCWSTR name,ModeW* mode,HWND reserved,DWORD flags,void* parameter){std::lock_guard<std::recursive_mutex> l(mutex);if(reserved||parameter||!DisplayName(name))return -5;if(flags&~6u)return -4;/* CDS_TEST / CDS_FULLSCREEN; no registry write or global mode change. */if(!mode)return 0;if(mode->size!=sizeof(ModeW)||mode->extra)return -5;constexpr DWORD allowed=0x40000|0x80000|0x100000|0x400000;if(mode->fields&~allowed)return -2;if(((mode->fields&0x40000)&&mode->bits!=32)||((mode->fields&0x80000)&&mode->w!=(DWORD)width)||((mode->fields&0x100000)&&mode->h!=(DWORD)height)||((mode->fields&0x400000)&&mode->frequency>1))return -2;return width>0&&height>0?0:-1;}
static LONG WINAPI ChangeSettings(ModeW* mode,DWORD flags){return ChangeSettingsEx(nullptr,mode,nullptr,flags,nullptr);}
