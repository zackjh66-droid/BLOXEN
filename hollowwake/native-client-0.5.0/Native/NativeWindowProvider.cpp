#include "NativeWindowProvider.h"
#include "PerformanceMarkers.h"
#include "ClientDependencyImports.generated.h"
#include "../../runtime-native-route/NativeSystem.h"
#include "../../client-runtime/graphics/NativeFactoryBridge.h"
#include <atomic>
#include <string>
#include <mutex>
#include <fstream>
#include <cstring>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
extern unsigned ReadCompositionAttachmentSerial();
extern bool SetOwnedGraphicsProbe(bool value);
namespace {
std::atomic<void(*)(const char*,DWORD)> windowLogger{nullptr};
const wchar_t* surfaceProperty=L"Hollowwake.ClientSurface.6.21";const BYTE surfaceIdentity=0;
void MissingWindowApi(const char* name){if(auto log=windowLogger.load())log(name,ERROR_PROC_NOT_FOUND);SetLastError(ERROR_PROC_NOT_FOUND);}
struct SystemProvider {
 HMODULE module=nullptr;
 explicit SystemProvider(const wchar_t* name){
  using Hold=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);using Load=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);
  auto hold=reinterpret_cast<Hold>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));
  if(hold&&hold(0,name,&module))return;
  auto load=reinterpret_cast<Load>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","LoadLibraryExW"));
  if(load)module=load(name,nullptr,0x800); // Microsoft System32 only, never game/CWD.
 }
 ~SystemProvider(){if(module)FreeLibrary(module);}
};
SystemProvider& User(){static SystemProvider value(L"user32.dll");return value;}
SystemProvider& Gdi(){static SystemProvider value(L"gdi32.dll");return value;}
void* UserSymbol(const char* name){return User().module?reinterpret_cast<void*>(GetProcAddress(User().module,name)):nullptr;}
template<class T>T UserApi(const char* name){return reinterpret_cast<T>(UserSymbol(name));}
template<class T>T GdiApi(const char* name){return reinterpret_cast<T>(Gdi().module?GetProcAddress(Gdi().module,name):nullptr);}
using Procedure=LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM);
struct NativeClass {UINT size,style;Procedure procedure;int classExtra,windowExtra;HINSTANCE instance;HICON icon;HCURSOR cursor;HBRUSH brush;LPCWSTR menu,name;HICON smallIcon;};
struct NativePaint {HDC dc;BOOL erase;RECT paint;BOOL restore,incremental;BYTE reserved[32];};
struct NativeMessage {HWND window;UINT message;WPARAM wp;LPARAM lp;DWORD time;POINT point;DWORD privateData;};
static_assert(sizeof(NativeClass)==80&&sizeof(NativePaint)==72&&sizeof(NativeMessage)==48,"Windows x64 window ABI");
using Register=ATOM(WINAPI*)(const NativeClass*);using Create=HWND(WINAPI*)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
HWND WINAPI CreateClientWindow(DWORD ex,LPCWSTR cls,LPCWSTR title,DWORD style,int x,int y,int w,int h,HWND parent,HMENU menu,HINSTANCE instance,LPVOID args){
 auto create=UserApi<Create>("CreateWindowExW");auto set=UserApi<BOOL(WINAPI*)(HWND,LPCWSTR,HANDLE)>("SetPropW");auto destroy=UserApi<BOOL(WINAPI*)(HWND)>("DestroyWindow");
 if(!create){MissingWindowApi("USER32.dll!CreateWindowExW");return nullptr;}if(!set){MissingWindowApi("USER32.dll!SetPropW");return nullptr;}if(!destroy){MissingWindowApi("USER32.dll!DestroyWindow");return nullptr;}
 auto window=create(ex,cls,title,style,x,y,w,h,parent,menu,instance,args);if(!window)return nullptr;
 // The OS removes window properties when the actual HWND is destroyed. A
 // recycled HWND cannot inherit the previous client's composition ownership.
 if(!set(window,surfaceProperty,(HANDLE)&surfaceIdentity)){auto error=GetLastError();destroy(window);SetLastError(error);return nullptr;}return window;
}
using Default=LRESULT(WINAPI*)(HWND,UINT,WPARAM,LPARAM);using Begin=HDC(WINAPI*)(HWND,NativePaint*);using End=BOOL(WINAPI*)(HWND,const NativePaint*);
using GetDc=HDC(WINAPI*)(HWND);using ReleaseDc=int(WINAPI*)(HWND,HDC);using Is=BOOL(WINAPI*)(HWND);using Invalidate=BOOL(WINAPI*)(HWND,const RECT*,BOOL);
using Update=BOOL(WINAPI*)(HWND);using Destroy=BOOL(WINAPI*)(HWND);using Unregister=BOOL(WINAPI*)(LPCWSTR,HINSTANCE);
using Post=BOOL(WINAPI*)(HWND,UINT,WPARAM,LPARAM);using Peek=BOOL(WINAPI*)(NativeMessage*,HWND,UINT,UINT,UINT);using Dispatch=LRESULT(WINAPI*)(const NativeMessage*);
using Pixel=BOOL(WINAPI*)(HDC,int,int,COLORREF);using ReadPixel=COLORREF(WINAPI*)(HDC,int,int);using Caps=int(WINAPI*)(HDC,int);
struct Probe {
 Default def;Begin begin;End end;Pixel pixel;ReadPixel readPixel;bool paintCalled=false,paintDc=false,paintPixel=false,paintEnded=false,messageDispatched=false;DWORD paintError=0;
};
static thread_local Probe* active=nullptr;
LRESULT CALLBACK OwnedWindow(HWND h,UINT message,WPARAM wp,LPARAM lp){
 auto p=active;if(!p)return 0;
 if(message==0x8000+621){p->messageDispatched=wp==621&&lp==4526925;return 621;}
 if(message==0xf){
  p->paintCalled=true;NativePaint paint={};auto dc=p->begin(h,&paint);p->paintDc=dc!=nullptr;
  if(dc){SetLastError(0);p->paintPixel=p->pixel(dc,1,1,RGB(17,83,149))&&p->readPixel(dc,1,1)==RGB(17,83,149);if(!p->paintPixel)p->paintError=GetLastError();p->paintEnded=p->end(h,&paint)!=FALSE;}
  else p->paintError=GetLastError();return 0;
 }
 return p->def(h,message,wp,lp);
}
}
void ConfigureNativeWindowLogger(void(*value)(const char*,DWORD)){windowLogger.store(value);}
BOOL WINAPI IsNativeClientWindow(HWND window){auto get=UserApi<HANDLE(WINAPI*)(HWND,LPCWSTR)>("GetPropW");auto valid=UserApi<BOOL(WINAPI*)(HWND)>("IsWindow");if(!get){MissingWindowApi("USER32.dll!GetPropW");return FALSE;}return valid&&valid(window)&&get(window,surfaceProperty)==(HANDLE)&surfaceIdentity;}
void* ResolveNativeWindowImport(const char* dll,const char* symbol){
 if(!dll||!symbol||_stricmp(dll,"USER32.dll"))return nullptr;
 // Exact existing import demand, not an arbitrary module/path resolver. Additional
 // test operations below are internal and cannot add names to the client table.
 bool known=false;for(auto& item:clientDependencyImports)if(!_stricmp(item.dll,dll)&&!strcmp(item.symbol,symbol)){known=true;break;}
 if(!known)return nullptr;if(!strcmp(symbol,"CreateWindowExW"))return UserSymbol(symbol)?reinterpret_cast<void*>(&CreateClientWindow):nullptr;return UserSymbol(symbol);
}
namespace {
HRESULT WINAPI CreateAnnotatedDevice(IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,ID3D11Device** device,D3D_FEATURE_LEVEL* level,ID3D11DeviceContext** context){
 static SystemProvider provider(L"d3d11.dll");auto create=reinterpret_cast<decltype(&D3D11CreateDevice)>(provider.module?GetProcAddress(provider.module,"D3D11CreateDevice"):nullptr);if(!create)return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
 auto hr=create(adapter,type,software,flags,levels,count,sdk,device,level,context);if(FAILED(hr))return hr;
 Microsoft::WRL::ComPtr<ID3D11Device> actual;if(device&&*device)actual=*device;else if(context&&*context)(*context)->GetDevice(&actual);
 // Missing optional profiler support must not replace successful device creation
 // with success for profiling: Begin/End return the native disabled result (-1).
 if(actual){auto bridge=Hollowwake::BridgeDeviceFactories(actual.Get());if(FAILED(bridge)){if(context&&*context){(*context)->Release();*context=nullptr;}if(device&&*device){(*device)->Release();*device=nullptr;}return bridge;}HWBindPerformanceDevice(actual.Get());}return hr;
}
HRESULT Factory(const char* name,REFIID iid,void** output){if(!output)return E_POINTER;*output=nullptr;static SystemProvider provider(L"dxgi.dll");auto create=reinterpret_cast<HRESULT(WINAPI*)(REFIID,void**)>(provider.module?GetProcAddress(provider.module,name):nullptr);if(!create)return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);auto hr=create(iid,output);if(FAILED(hr))return hr;auto bridge=Hollowwake::BridgeNativeFactory(static_cast<IUnknown*>(*output));if(FAILED(bridge)){static_cast<IUnknown*>(*output)->Release();*output=nullptr;return bridge;}return hr;}
HRESULT WINAPI CreateFactory(REFIID iid,void** output){return Factory("CreateDXGIFactory",iid,output);}
HRESULT WINAPI CreateFactory1(REFIID iid,void** output){return Factory("CreateDXGIFactory1",iid,output);}
}
void* ResolveNativePresentationImport(const char* dll,const char* symbol){
 if(!dll||!symbol)return nullptr;HMODULE module=nullptr;
 if(!_stricmp(dll,"dxgi.dll")){static SystemProvider provider(L"dxgi.dll");if(!provider.module||!GetProcAddress(provider.module,symbol))return nullptr;if(!strcmp(symbol,"CreateDXGIFactory"))return reinterpret_cast<void*>(&CreateFactory);if(!strcmp(symbol,"CreateDXGIFactory1"))return reinterpret_cast<void*>(&CreateFactory1);}
 else if(!_stricmp(dll,"d3d11.dll")&&!strcmp(symbol,"D3D11CreateDevice")){static SystemProvider provider(L"d3d11.dll");return provider.module&&GetProcAddress(provider.module,symbol)?reinterpret_cast<void*>(&CreateAnnotatedDevice):nullptr;}
 return module?reinterpret_cast<void*>(GetProcAddress(module,symbol)):nullptr;
}
extern "C" int __cdecl HWNativeWindowProbe(const wchar_t* path){
 if(!path)return E_POINTER;static std::mutex gate;std::lock_guard<std::mutex> guard(gate);
 struct OwnedScope{bool previous;OwnedScope():previous(SetOwnedGraphicsProbe(true)){}~OwnedScope(){SetOwnedGraphicsProbe(previous);}} ownedScope;
 std::string operation="provider";DWORD error=0,cleanupError=0;HRESULT result=E_FAIL;HWND window=nullptr;ATOM atom=0;HINSTANCE instance=nullptr;std::wstring name=L"HollowwakeOwnedNativeWindow_"+std::to_wstring(GetCurrentProcessId())+L"_"+std::to_wstring(GetCurrentThreadId());
 auto reg=reinterpret_cast<Register>(ResolveNativeWindowImport("user32.dll","RegisterClassExW"));auto create=reinterpret_cast<Create>(ResolveNativeWindowImport("user32.dll","CreateWindowExW"));auto is=UserApi<Is>("IsWindow");auto getDc=UserApi<GetDc>("GetDC");auto releaseDc=UserApi<ReleaseDc>("ReleaseDC");auto invalidate=UserApi<Invalidate>("InvalidateRect");auto update=UserApi<Update>("UpdateWindow");auto destroy=UserApi<Destroy>("DestroyWindow");auto unregisterClass=UserApi<Unregister>("UnregisterClassW");auto post=UserApi<Post>("PostMessageW");auto peek=UserApi<Peek>("PeekMessageW");auto dispatch=UserApi<Dispatch>("DispatchMessageW");auto caps=GdiApi<Caps>("GetDeviceCaps");
 Probe probe={UserApi<Default>("DefWindowProcW"),UserApi<Begin>("BeginPaint"),UserApi<End>("EndPaint"),GdiApi<Pixel>("SetPixelV"),GdiApi<ReadPixel>("GetPixel")};bool realDc=false,releasedDc=false,compositionChain=false,unregisteredNativePreserved=false,gpuReadback=false;auto beforeAttachment=ReadCompositionAttachmentSerial();HRESULT graphicsResult=E_PENDING,presentResult=E_PENDING;unsigned driverUsed=0;
 do {
  if(!reg||!create||!is||!getDc||!releaseDc||!invalidate||!update||!destroy||!unregisterClass||!post||!peek||!dispatch||!caps||!probe.def||!probe.begin||!probe.end||!probe.pixel||!probe.readPixel){error=ERROR_PROC_NOT_FOUND;break;}
  using Module=HMODULE(WINAPI*)(LPCWSTR);auto module=reinterpret_cast<Module>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleW"));if(!module){error=ERROR_PROC_NOT_FOUND;break;}instance=module(nullptr);
  NativeClass cls={};cls.size=sizeof(cls);cls.style=0x20;cls.procedure=OwnedWindow;cls.instance=instance;cls.name=name.c_str();active=&probe;
  operation="RegisterClassExW";SetLastError(0);atom=reg(&cls);if(!atom){error=GetLastError();break;}
  operation="CreateWindowExW";SetLastError(0);window=create(0,name.c_str(),L"Hollowwake owned native-window check",0x80000000|0x10000000,12,12,64,64,nullptr,nullptr,instance,nullptr);if(!window){error=GetLastError();break;}
  operation="IsWindow";if(!is(window)||!IsNativeClientWindow(window)){error=ERROR_INVALID_WINDOW_HANDLE;break;}
  operation="GetDC";SetLastError(0);auto dc=getDc(window);if(!dc){error=GetLastError();break;}realDc=caps(dc,12)>0;releasedDc=releaseDc(window,dc)==1;if(!realDc||!releasedDc){error=ERROR_INVALID_HANDLE;break;}
  operation="BeginPaint/GDI pixel readback/EndPaint";SetLastError(0);if(!invalidate(window,nullptr,FALSE)||!update(window)){error=GetLastError();break;}
  if(!probe.paintCalled||!probe.paintDc||!probe.paintPixel||!probe.paintEnded){error=probe.paintError?probe.paintError:ERROR_INVALID_DATA;break;}
  operation="PostMessageW/PeekMessageW/DispatchMessageW";SetLastError(0);if(!post(window,0x8000+621,621,4526925)){error=GetLastError();break;}
  NativeMessage message={};for(unsigned n=0;n<256&&peek(&message,window,0,0,1);++n)dispatch(&message);if(!probe.messageDispatched){error=ERROR_INVALID_DATA;break;}
  operation="native D3D11CreateDevice";
  auto createDevice=reinterpret_cast<decltype(&D3D11CreateDevice)>(ResolveNativePresentationImport("d3d11.dll","D3D11CreateDevice"));if(!createDevice){error=ERROR_PROC_NOT_FOUND;break;}
  using Microsoft::WRL::ComPtr;ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
  driverUsed=D3D_DRIVER_TYPE_HARDWARE;graphicsResult=createDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
  if(FAILED(graphicsResult)){driverUsed=D3D_DRIVER_TYPE_WARP;graphicsResult=createDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);}
  if(FAILED(graphicsResult)){result=graphicsResult;break;}
  operation="native device/adapter/factory";ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory> factory;graphicsResult=device.As(&dxgi);if(SUCCEEDED(graphicsResult))graphicsResult=dxgi->GetAdapter(&adapter);if(SUCCEEDED(graphicsResult))graphicsResult=adapter->GetParent(IID_PPV_ARGS(&factory));if(FAILED(graphicsResult)){result=graphicsResult;break;}
  operation="native factory -> owned HWND -> CoreWindow composition";DXGI_SWAP_CHAIN_DESC desc={};desc.BufferDesc.Width=64;desc.BufferDesc.Height=64;desc.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.OutputWindow=window;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  ComPtr<IDXGISwapChain> chain;graphicsResult=factory->CreateSwapChain(device.Get(),&desc,&chain);if(FAILED(graphicsResult)){result=graphicsResult;break;}compositionChain=ReadCompositionAttachmentSerial()>beforeAttachment;if(!compositionChain){error=ERROR_INVALID_STATE;break;}
  operation="native D3D11 draw and GPU pixel readback";ComPtr<ID3D11Texture2D> buffer;ComPtr<ID3D11RenderTargetView> view;graphicsResult=chain->GetBuffer(0,IID_PPV_ARGS(&buffer));if(SUCCEEDED(graphicsResult))graphicsResult=device->CreateRenderTargetView(buffer.Get(),nullptr,&view);if(FAILED(graphicsResult)){result=graphicsResult;break;}
  const float color[4]={0.25f,0.5f,0.75f,1};context->ClearRenderTargetView(view.Get(),color);D3D11_TEXTURE2D_DESC texture={};buffer->GetDesc(&texture);texture.BindFlags=0;texture.MiscFlags=0;texture.Usage=D3D11_USAGE_STAGING;texture.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;graphicsResult=device->CreateTexture2D(&texture,nullptr,&staging);if(FAILED(graphicsResult)){result=graphicsResult;break;}
  context->CopyResource(staging.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE data={};graphicsResult=context->Map(staging.Get(),0,D3D11_MAP_READ,0,&data);if(FAILED(graphicsResult)){result=graphicsResult;break;}auto pixel=static_cast<const BYTE*>(data.pData);gpuReadback=pixel[0]>=190&&pixel[0]<=192&&pixel[1]>=127&&pixel[1]<=129&&pixel[2]>=63&&pixel[2]<=65&&pixel[3]==255;context->Unmap(staging.Get(),0);if(!gpuReadback){error=ERROR_INVALID_DATA;break;}
  operation="native IDXGISwapChain::Present";presentResult=chain->Present(0,0);if(FAILED(presentResult)){result=presentResult;break;}
  operation="Unregistered native HWND keeps original DXGI behavior";
  auto rawCreate=UserApi<Create>("CreateWindowExW");auto other=rawCreate?rawCreate(0,name.c_str(),L"Owned nonclient surface",0x80000000,0,0,64,64,nullptr,nullptr,instance,nullptr):nullptr;
  if(!other){error=GetLastError();break;}auto serial=ReadCompositionAttachmentSerial();desc.OutputWindow=other;ComPtr<IDXGISwapChain> native;
  auto otherResult=factory->CreateSwapChain(device.Get(),&desc,&native);DXGI_SWAP_CHAIN_DESC nativeDesc={};if(SUCCEEDED(otherResult))otherResult=native->GetDesc(&nativeDesc);
  unregisteredNativePreserved=SUCCEEDED(otherResult)&&!IsNativeClientWindow(other)&&nativeDesc.OutputWindow==other&&ReadCompositionAttachmentSerial()==serial;native.Reset();auto removed=destroy(other);
  if(!unregisteredNativePreserved||!removed){error=ERROR_INVALID_STATE;break;}
  operation="complete";result=S_OK;
 }while(false);
 if(window&&destroy&&!destroy(window)){cleanupError=GetLastError();if(SUCCEEDED(result)){operation="DestroyWindow";error=cleanupError;result=E_FAIL;}}
 if(atom&&unregisterClass&&!unregisterClass(name.c_str(),instance)){cleanupError=GetLastError();if(SUCCEEDED(result)){operation="UnregisterClassW";error=cleanupError;result=E_FAIL;}}
 active=nullptr;if(FAILED(result)&&result==E_FAIL)result=HRESULT_FROM_WIN32(error?error:ERROR_GEN_FAILURE);
 std::ofstream out(path,std::ios::trunc);if(!out)return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
 out<<"{\"scope\":\"Original native-window route check inside hosted UWP AppContainer; not game/Xbox\",\"hresult\":"<<static_cast<long>(result)<<",\"operation\":\""<<operation<<"\",\"win32Error\":"<<error<<",\"cleanupError\":"<<cleanupError<<",\"nativeWindowCreated\":"<<(window?"true":"false")<<",\"nativeDc\":"<<(realDc&&releasedDc?"true":"false")<<",\"paintCalled\":"<<(probe.paintCalled?"true":"false")<<",\"paintDc\":"<<(probe.paintDc?"true":"false")<<",\"paintPixelReadback\":"<<(probe.paintPixel?"true":"false")<<",\"paintEnded\":"<<(probe.paintEnded?"true":"false")<<",\"ownMessageDispatched\":"<<(probe.messageDispatched?"true":"false")<<",\"hostCompositionAttached\":"<<(compositionChain?"true":"false")<<",\"unregisteredNativePreserved\":"<<(unregisteredNativePreserved?"true":"false")<<",\"gpuPixelReadback\":"<<(gpuReadback?"true":"false")<<",\"graphicsHresult\":"<<static_cast<long>(graphicsResult)<<",\"presentHresult\":"<<static_cast<long>(presentResult)<<",\"driverType\":"<<driverUsed<<",\"visibleGameClaimed\":false,\"gameExecuted\":false,\"xboxTested\":false}";out.flush();return out.good()?result:HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
}
