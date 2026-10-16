// Original exact-ABI import hooks integrated into Hollowwake's import path.
// These hooks do not imply complete platform or client compatibility.
#include "CoreWindowBridge.h"
#include <wrl/client.h>
#include <cstring>
static Microsoft::WRL::ComPtr<IUnknown> coreWindow;
namespace Hollowwake {
HRESULT BindGraphicsWindow(IUnknown* window){if(!window)return E_INVALIDARG;if(coreWindow)return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);coreWindow=window;return S_OK;}
}
static HRESULT WINAPI GuestCreateFactory1(REFIID iid,void** result){return Hollowwake::CreateCoreFactory(coreWindow.Get(),iid,result);}
static HRESULT WINAPI GuestCreateFactory(REFIID iid,void** result){return Hollowwake::CreateCoreFactory(coreWindow.Get(),iid,result);}
static HRESULT WINAPI GuestCreateDevice(IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,ID3D11Device** device,D3D_FEATURE_LEVEL* level,ID3D11DeviceContext** context){
 // D3D device creation itself does not require a window. Preserve native
 // errors and unwrap our enumerated adapter before the actual OS call.
 return Hollowwake::CreateCoreDevice(adapter,type,software,flags,levels,count,sdk,device,level,context);
}
namespace Hollowwake {
void* ResolveGraphicsImport(const char* dll,const char* name){
 if(!dll||!name)return nullptr;
 if(!_stricmp(dll,"dxgi.dll")){
  if(!strcmp(name,"CreateDXGIFactory1"))return reinterpret_cast<void*>(&GuestCreateFactory1);
  if(!strcmp(name,"CreateDXGIFactory"))return reinterpret_cast<void*>(&GuestCreateFactory);
 }
 if(!_stricmp(dll,"d3d11.dll")&&!strcmp(name,"D3D11CreateDevice"))return reinterpret_cast<void*>(&GuestCreateDevice);
 return nullptr; // Unknown imports must be rejected/resolved explicitly, never stubbed.
}
}
