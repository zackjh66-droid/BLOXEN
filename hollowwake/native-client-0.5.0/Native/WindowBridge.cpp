// Original single-CoreWindow Win32 message bridge. No native desktop HWND APIs.
// Multiple top-level windows, raw input, IME and monitor APIs are not implemented.
#include <windows.h>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <algorithm>
using Proc=LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM);
struct ClassW {UINT style;Proc proc;int clsExtra,wndExtra;HINSTANCE instance;HICON icon;HCURSOR cursor;HBRUSH brush;LPCWSTR menu,name;};
struct Msg {HWND window;UINT message;WPARAM wp;LPARAM lp;DWORD time;POINT point;DWORD privateData;};
struct Class {ATOM atom;std::wstring name;Proc proc;HINSTANCE instance;};
struct Window {bool alive,destroying;Proc proc;std::wstring title;bool visible=false,enabled=true;LONG_PTR style=0,extended=0,user=0;HINSTANCE instance=nullptr;DWORD owner=0;};
struct CreateStruct {void* params;HINSTANCE instance;HMENU menu;HWND parent;int cy,cx,y,x;LONG style;LPCWSTR name,cls;DWORD exStyle;};
static std::recursive_mutex mutex;static std::condition_variable_any wake;static std::vector<Class> classes;static std::unique_ptr<Window> window;static std::deque<Msg> queue;static LONG width=0,height=0;static bool keys[256]={};
static unsigned nextClassAtom=0xc000;static bool hostActive=true;static HWND focused=nullptr,captured=nullptr;
static HWND Handle(){return reinterpret_cast<HWND>(window.get());}
static bool Valid(HWND h){return window&&window->alive&&h==Handle();}
extern "C" __declspec(dllexport) void __cdecl HWViewport(int w,int h){std::lock_guard<std::recursive_mutex> l(mutex);width=w;height=h;}
static void Push(HWND target,UINT message,WPARAM wp,LPARAM lp){if(queue.size()>=4096){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return;}queue.push_back({target,message,wp,lp,(DWORD)GetTickCount64(),{0,0},0});wake.notify_all();}
extern "C" __declspec(dllexport) void __cdecl HWKey(unsigned key,int down){std::lock_guard<std::recursive_mutex> l(mutex);if(key<256)keys[key]=down!=0;if(window&&window->alive)Push(Handle(),down?0x100:0x101,key,down?1:0xC0000001);}
extern "C" __declspec(dllexport) void __cdecl HWChar(unsigned key){std::lock_guard<std::recursive_mutex> l(mutex);if(window&&window->alive)Push(Handle(),0x102,key,1);}
static ATOM WINAPI Register(const ClassW* value){if(!value||!value->name||!value->proc||value->clsExtra||value->wndExtra){SetLastError(ERROR_NOT_SUPPORTED);return 0;}std::lock_guard<std::recursive_mutex> l(mutex);for(auto& c:classes)if(c.name==value->name){SetLastError(ERROR_CLASS_ALREADY_EXISTS);return 0;}if(classes.size()>=64){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}if(nextClassAtom>0xffff){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}ATOM atom=(ATOM)nextClassAtom++;classes.push_back({atom,value->name,value->proc,value->instance});return atom;}
static HWND WINAPI Create(DWORD ex,LPCWSTR name,LPCWSTR title,DWORD style,int x,int y,int cx,int cy,HWND parent,HMENU menu,HINSTANCE instance,void* params){
 Proc proc=nullptr;HWND handle;
 {std::lock_guard<std::recursive_mutex> l(mutex);if(width<=0||height<=0||parent||window){SetLastError(ERROR_NOT_SUPPORTED);return nullptr;}for(auto& c:classes)if(((ULONG_PTR)name<=0xffff?c.atom==(ATOM)(ULONG_PTR)name:c.name==name)&&c.instance==instance)proc=c.proc;if(!proc){SetLastError(ERROR_CANNOT_FIND_WND_CLASS);return nullptr;}window.reset(new Window{true,false,proc,title?title:L""});handle=Handle();window->style=style;window->extended=ex;window->instance=instance;window->owner=GetCurrentThreadId();}
 CreateStruct create={params,instance,menu,parent,cy,cx,y,x,(LONG)style,title,name,ex};
 if(!proc(handle,0x81,0,(LPARAM)&create)||proc(handle,1,0,(LPARAM)&create)==-1){proc(handle,0x82,0,0);std::lock_guard<std::recursive_mutex> l(mutex);window.reset();return nullptr;}
 {std::lock_guard<std::recursive_mutex> l(mutex);Push(handle,5,0,MAKELPARAM(width,height));}return handle;
}
static BOOL WINAPI Post(HWND h,UINT message,WPARAM wp,LPARAM lp){std::lock_guard<std::recursive_mutex> l(mutex);if(h&&!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}Push(h,message,wp,lp);return TRUE;}
static void WINAPI Quit(int code){std::lock_guard<std::recursive_mutex> l(mutex);Push(nullptr,0x12,code,0);}
static BOOL WINAPI Peek(Msg* out,HWND h,UINT min,UINT max,UINT flags){if(!out||flags&~1){SetLastError(ERROR_NOT_SUPPORTED);return FALSE;}std::lock_guard<std::recursive_mutex> l(mutex);for(auto i=queue.begin();i!=queue.end();++i){if((!h||h==i->window)&&(!min&&!max||(min<=i->message&&i->message<=max))){*out=*i;if(flags&1)queue.erase(i);return TRUE;}}return FALSE;}
static LRESULT WINAPI Dispatch(const Msg* msg){if(!msg)return 0;Proc proc=nullptr;{std::lock_guard<std::recursive_mutex> l(mutex);if(Valid(msg->window))proc=window->proc;}return proc?proc(msg->window,msg->message,msg->wp,msg->lp):0;}
static BOOL WINAPI Rect(HWND h,RECT* out){std::lock_guard<std::recursive_mutex> l(mutex);if(!out||!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}*out={0,0,width,height};return TRUE;}
static BOOL WINAPI Destroy(HWND h){Proc proc;{std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)||window->destroying){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}window->destroying=true;proc=window->proc;}proc(h,2,0,0);proc(h,0x82,0,0);{std::lock_guard<std::recursive_mutex> l(mutex);if(focused==h)focused=nullptr;if(captured==h)captured=nullptr;window.reset();queue.erase(std::remove_if(queue.begin(),queue.end(),[&](const Msg& m){return m.window==h;}),queue.end());}return TRUE;}
static LRESULT WINAPI Default(HWND h,UINT message,WPARAM,LPARAM){if(message==0x81)return TRUE;if(message==0x10){Destroy(h);return 0;}if(message==0x83||message>=0x400)return 0;SetLastError(ERROR_NOT_SUPPORTED);return 0;}
static BOOL WINAPI Get(Msg* out,HWND h,UINT min,UINT max){if(!out){SetLastError(ERROR_INVALID_PARAMETER);return -1;}std::unique_lock<std::recursive_mutex> l(mutex);for(;;){for(auto i=queue.begin();i!=queue.end();++i)if(i->message==0x12||((!h||h==i->window)&&(!min&&!max||(min<=i->message&&i->message<=max)))){*out=*i;queue.erase(i);return out->message==0x12?0:1;}wake.wait(l);}}
static SHORT WINAPI KeyState(int key){std::lock_guard<std::recursive_mutex> l(mutex);return key>=0&&key<256&&keys[key]?(SHORT)0x8000:0;}

// State is scoped to the single virtual fullscreen CoreWindow. These are not
// desktop HWNDs, monitor handles or assertions that every USER32 API is supported.
extern "C" __declspec(dllexport) void __cdecl HWHostActivation(int active){std::lock_guard<std::recursive_mutex> l(mutex);hostActive=active!=0;if(!hostActive){memset(keys,0,sizeof(keys));focused=nullptr;captured=nullptr;}if(window&&window->alive){Push(Handle(),0x1c,hostActive,0);Push(Handle(),6,hostActive?1:0,0);if(hostActive&&window->visible&&window->enabled){focused=Handle();Push(Handle(),7,0,0);}}}
static BOOL WINAPI Is(HWND h){std::lock_guard<std::recursive_mutex> l(mutex);return Valid(h);}
static BOOL WINAPI Enabled(HWND h){std::lock_guard<std::recursive_mutex> l(mutex);return Valid(h)&&window->enabled;}
static BOOL WINAPI Iconic(HWND h){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h))SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}
static BOOL WINAPI Zoomed(HWND h){std::lock_guard<std::recursive_mutex> l(mutex);return Valid(h)&&(window->style&0x1000000)!=0;}
static HWND WINAPI Parent(HWND h){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h))SetLastError(ERROR_INVALID_WINDOW_HANDLE);return nullptr;}
static BOOL WINAPI Child(HWND a,HWND b){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(a)||!Valid(b))SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}
static BOOL WINAPI Enable(HWND h,BOOL value){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}bool previous=window->enabled;window->enabled=value!=0;if(!window->enabled){if(focused==h){focused=nullptr;Push(h,8,0,0);}if(captured==h)captured=nullptr;}if(previous!=window->enabled)Push(h,0xa,window->enabled,0);return !previous;}
static BOOL WINAPI Show(HWND h,int command){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}if(command!=0&&command!=1&&command!=3&&command!=5&&command!=8&&command!=9&&command!=4&&command!=10){SetLastError(ERROR_NOT_SUPPORTED);return FALSE;}bool prior=window->visible;window->visible=command!=0;if(command==3)window->style|=0x1000000;else if(command==1||command==9)window->style&=~(LONG_PTR)0x1000000;if(prior!=window->visible)Push(h,0x18,window->visible,0);return prior;}
static HWND WINAPI Focus(){std::lock_guard<std::recursive_mutex> l(mutex);return hostActive&&window&&window->owner==GetCurrentThreadId()?focused:nullptr;}
static HWND WINAPI SetFocusBridge(HWND h){std::lock_guard<std::recursive_mutex> l(mutex);if(h&&(!Valid(h)||!window->enabled||window->owner!=GetCurrentThreadId())){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return nullptr;}auto prior=focused;focused=h;if(prior!=h){if(prior)Push(prior,8,(WPARAM)h,0);if(h)Push(h,7,(WPARAM)prior,0);}return prior;}
static HWND WINAPI Active(){std::lock_guard<std::recursive_mutex> l(mutex);return hostActive&&window&&window->alive&&window->visible?Handle():nullptr;}
static HWND WINAPI SetActive(HWND h){std::lock_guard<std::recursive_mutex> l(mutex);auto old=Active();if(h&&!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return nullptr;}hostActive=h!=nullptr;if(window)Push(Handle(),6,hostActive?1:0,0);return old;}
static BOOL WINAPI Foreground(HWND h){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}if(!hostActive){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}SetFocusBridge(h);return TRUE;}
static HWND WINAPI Capture(){std::lock_guard<std::recursive_mutex> l(mutex);return captured;}
static HWND WINAPI SetCaptureBridge(HWND h){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return nullptr;}auto prior=captured;captured=h;return prior;}
static BOOL WINAPI ReleaseCaptureBridge(){std::lock_guard<std::recursive_mutex> l(mutex);auto prior=captured;captured=nullptr;if(prior)Push(prior,0x215,0,0);return TRUE;}
static LONG_PTR WINAPI GetLong(HWND h,int index){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return 0;}switch(index){case -4:return (LONG_PTR)window->proc;case -6:return (LONG_PTR)window->instance;case -8:return 0;case -16:return window->style;case -20:return window->extended;case -21:return window->user;default:SetLastError(ERROR_INVALID_INDEX);return 0;}}
static LONG_PTR WINAPI SetLong(HWND h,int index,LONG_PTR value){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return 0;}auto prior=GetLong(h,index);switch(index){case -4:if(!value){SetLastError(ERROR_INVALID_PARAMETER);return 0;}window->proc=(Proc)value;break;case -16:window->style=value;break;case -20:window->extended=value;break;case -21:window->user=value;break;default:SetLastError(ERROR_NOT_SUPPORTED);return 0;}return prior;}
static LONG WINAPI GetLong32(HWND h,int index){return (LONG)GetLong(h,index);}static LONG WINAPI SetLong32(HWND h,int index,LONG value){if(index==-4||index==-6||index==-8||index==-21){SetLastError(ERROR_NOT_SUPPORTED);return 0;}return (LONG)SetLong(h,index,value);}
static DWORD WINAPI Owner(HWND h,DWORD* process){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)){if(process)*process=0;SetLastError(ERROR_INVALID_WINDOW_HANDLE);return 0;}if(process)*process=GetCurrentProcessId();return window->owner;}
static BOOL WINAPI Text(HWND h,LPCWSTR title){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)||!title){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}auto size=wcsnlen_s(title,32768);if(size==32768){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}window->title.assign(title,size);return TRUE;}
static BOOL WINAPI Coordinates(HWND h,POINT* point){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)||!point){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}return TRUE; /* Fullscreen client/screen origins coincide. */}
static HWND WINAPI FromPoint(POINT p){std::lock_guard<std::recursive_mutex> l(mutex);return window&&window->alive&&window->visible&&p.x>=0&&p.y>=0&&p.x<width&&p.y<height?Handle():nullptr;}
static BOOL WINAPI Unregister(LPCWSTR name,HINSTANCE instance){std::lock_guard<std::recursive_mutex> l(mutex);if(!name){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}for(auto i=classes.begin();i!=classes.end();++i)if(((ULONG_PTR)name<=65535?i->atom==(ATOM)(ULONG_PTR)name:i->name==name)&&i->instance==instance){if(window&&window->alive){SetLastError(ERROR_CLASS_HAS_WINDOWS);return FALSE;}classes.erase(i);return TRUE;}SetLastError(ERROR_CLASS_DOES_NOT_EXIST);return FALSE;}
static int virtualMonitor;
static HMONITOR WINAPI MonitorWindow(HWND h,DWORD flags){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)&&!(flags&3)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return nullptr;}return width>0&&height>0?(HMONITOR)&virtualMonitor:nullptr;}
static HMONITOR WINAPI MonitorPoint(POINT p,DWORD flags){std::lock_guard<std::recursive_mutex> l(mutex);return width>0&&height>0&&((p.x>=0&&p.y>=0&&p.x<width&&p.y<height)||(flags&3))?(HMONITOR)&virtualMonitor:nullptr;}
static HMONITOR WINAPI MonitorRect(const RECT* p,DWORD flags){if(!p){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}return MonitorPoint({p->left,p->top},flags);}
struct MonitorInfo {DWORD size;RECT monitor,work;DWORD flags;wchar_t device[32];};
static BOOL WINAPI MonitorInformation(HMONITOR h,MonitorInfo* info){std::lock_guard<std::recursive_mutex> l(mutex);if(h!=(HMONITOR)&virtualMonitor||!info||(info->size!=40&&info->size!=104)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}info->monitor=info->work={0,0,width,height};info->flags=1;if(info->size==104)wcscpy_s(info->device,L"HollowwakeCoreWindow");return TRUE;}
using MonitorProc=BOOL(CALLBACK*)(HMONITOR,HDC,RECT*,LPARAM);
static BOOL WINAPI Monitors(HDC dc,const RECT* clip,MonitorProc proc,LPARAM data){std::lock_guard<std::recursive_mutex> l(mutex);if(dc||!proc||width<=0||height<=0){SetLastError(ERROR_NOT_SUPPORTED);return FALSE;}RECT r={0,0,width,height};if(clip){r.left=(std::max)(r.left,clip->left);r.top=(std::max)(r.top,clip->top);r.right=(std::min)(r.right,clip->right);r.bottom=(std::min)(r.bottom,clip->bottom);}if(r.left>=r.right||r.top>=r.bottom)return TRUE;return proc((HMONITOR)&virtualMonitor,nullptr,&r,data);}
static int WINAPI Metrics(int value){std::lock_guard<std::recursive_mutex> l(mutex);switch(value){case 0:case 16:case 78:return width;case 1:case 17:case 79:return height;case 76:case 77:return 0;case 80:return width>0&&height>0?1:0;default:SetLastError(ERROR_NOT_SUPPORTED);return 0;}}

void* ResolveWindowImport(const char* dll,const char* name){if(_stricmp(dll,"USER32.dll"))return nullptr;
 #define ENTRY(n,f) if(!strcmp(name,n))return reinterpret_cast<void*>(&f)
 ENTRY("DestroyWindow",Destroy);ENTRY("DefWindowProcW",Default);ENTRY("GetMessageW",Get);ENTRY("RegisterClassW",Register);ENTRY("CreateWindowExW",Create);ENTRY("PostMessageW",Post);ENTRY("PostQuitMessage",Quit);ENTRY("PeekMessageW",Peek);ENTRY("DispatchMessageW",Dispatch);ENTRY("GetClientRect",Rect);ENTRY("GetWindowRect",Rect);ENTRY("GetAsyncKeyState",KeyState);ENTRY("GetKeyState",KeyState);
 ENTRY("IsWindow",Is);ENTRY("IsWindowEnabled",Enabled);ENTRY("IsIconic",Iconic);ENTRY("IsZoomed",Zoomed);ENTRY("GetParent",Parent);ENTRY("IsChild",Child);ENTRY("EnableWindow",Enable);ENTRY("ShowWindow",Show);ENTRY("GetFocus",Focus);ENTRY("SetFocus",SetFocusBridge);ENTRY("GetActiveWindow",Active);ENTRY("GetForegroundWindow",Active);ENTRY("SetActiveWindow",SetActive);ENTRY("SetForegroundWindow",Foreground);ENTRY("GetCapture",Capture);ENTRY("SetCapture",SetCaptureBridge);ENTRY("ReleaseCapture",ReleaseCaptureBridge);ENTRY("GetWindowLongPtrW",GetLong);ENTRY("GetWindowLongW",GetLong32);ENTRY("SetWindowLongPtrW",SetLong);ENTRY("SetWindowLongW",SetLong32);ENTRY("GetWindowThreadProcessId",Owner);ENTRY("SetWindowTextW",Text);ENTRY("ClientToScreen",Coordinates);ENTRY("ScreenToClient",Coordinates);ENTRY("WindowFromPoint",FromPoint);ENTRY("UnregisterClassW",Unregister);ENTRY("MonitorFromWindow",MonitorWindow);ENTRY("MonitorFromPoint",MonitorPoint);ENTRY("MonitorFromRect",MonitorRect);ENTRY("GetMonitorInfoW",MonitorInformation);ENTRY("EnumDisplayMonitors",Monitors);ENTRY("GetSystemMetrics",Metrics);
 #undef ENTRY
 return nullptr;
}
static unsigned contractKeys=0;static bool contractCreate=false;
static LRESULT CALLBACK ContractProc(HWND,UINT message,WPARAM wp,LPARAM lp){if(message==0x81){auto c=(CreateStruct*)lp;contractCreate=c&&c->params==(void*)621&&c->name&&!wcscmp(c->name,L"HollowwakeContract");return contractCreate?1:0;}if(message==0x100&&wp==65)++contractKeys;return 0;}
extern "C" __declspec(dllexport) int __cdecl HWWindowContract(){
 ClassW cls={};cls.proc=ContractProc;cls.name=L"HollowwakeContract";if(!Register(&cls))return -1;HWND h=Create(0,cls.name,cls.name,0,0,0,0,0,nullptr,nullptr,nullptr,(void*)621);if(!h)return -2;
 RECT rect={};bool valid=Rect(h,&rect)&&rect.right>0&&rect.bottom>0;HWKey(65,1);bool pressed=(KeyState(65)&0x8000)!=0;Msg msg={};while(Peek(&msg,nullptr,0,0,1))Dispatch(&msg);HWKey(65,0);bool released=KeyState(65)==0;Destroy(h);while(Peek(&msg,nullptr,0,0,1)){};
 return contractCreate&&contractKeys==1&&valid&&pressed&&released?621:-3;
}
