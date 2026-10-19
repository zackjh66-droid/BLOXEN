// Original Hollowwake CoreWindow surface contract for Mesa's UWP WGL backend.
// This is internal surface plumbing, not a general Win32 HWND/HDC implementation.
#include "gdi_uwp.h"
#include "../../frontends/wgl/stw_pixelformat.h"
#include "../../frontends/wgl/stw_winsys.h"
#include <windows.ui.core.h>
#include <wrl/client.h>
#include <mutex>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace {
std::recursive_mutex gate;ComPtr<IUnknown> window;BOOL(__cdecl* viewport)(LONG*,LONG*)=nullptr;int selectedFormat=0;
bool Valid(HANDLE value){return window&&value==window.Get();}
}
extern "C" __declspec(dllexport) HRESULT __cdecl HWMesaConfigureWindow(IUnknown* value,BOOL(__cdecl* size)(LONG*,LONG*)){
 if(!value||!size)return E_INVALIDARG;ComPtr<ABI::Windows::UI::Core::ICoreWindow> verified;auto hr=value->QueryInterface(IID_PPV_ARGS(&verified));if(FAILED(hr))return hr;ComPtr<IUnknown> identity;hr=verified.As(&identity);if(FAILED(hr))return hr;
 std::lock_guard<std::recursive_mutex> lock(gate);if(window&&window.Get()!=identity.Get())return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);window=identity;viewport=size;return S_OK;
}
extern "C" __declspec(dllexport) HDC __cdecl HWMesaCoreDC(){std::lock_guard<std::recursive_mutex> lock(gate);return reinterpret_cast<HDC>(window.Get());}
extern "C" HWND WINAPI WindowFromDC(HDC dc){std::lock_guard<std::recursive_mutex> lock(gate);if(!Valid(dc)){SetLastError(ERROR_INVALID_HANDLE);return nullptr;}return reinterpret_cast<HWND>(window.Get());}
extern "C" HDC WINAPI GetDC(HWND value){std::lock_guard<std::recursive_mutex> lock(gate);if(!Valid(value)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return nullptr;}return reinterpret_cast<HDC>(window.Get());}
extern "C" int WINAPI ReleaseDC(HWND value,HDC dc){std::lock_guard<std::recursive_mutex> lock(gate);if(!Valid(value)||!Valid(dc)){SetLastError(ERROR_INVALID_HANDLE);return 0;}return 1; /* Borrowed singleton; no native DC allocation to release. */}
extern "C" BOOL WINAPI GetClientRect(HWND value,RECT* result){
 std::lock_guard<std::recursive_mutex> lock(gate);if(!result){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}if(!Valid(value)||!viewport){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return FALSE;}
 LONG w=0,h=0;if(!viewport(&w,&h))return FALSE;if(w<=0||h<=0){SetLastError(ERROR_INVALID_STATE);return FALSE;}*result={0,0,w,h};return TRUE;
}
extern "C" int WINAPI GetPixelFormat(HDC dc){std::lock_guard<std::recursive_mutex> lock(gate);if(!Valid(dc)){SetLastError(ERROR_INVALID_HANDLE);return 0;}return selectedFormat;}
extern "C" int WINAPI DescribePixelFormat(HDC dc,int index,UINT bytes,PIXELFORMATDESCRIPTOR* result){
 std::lock_guard<std::recursive_mutex> lock(gate);if(!Valid(dc)){SetLastError(ERROR_INVALID_HANDLE);return 0;}int count=stw_pixelformat_get_count(dc);if(!result&&bytes==0)return count;
 if(!result||bytes<sizeof(*result)){SetLastError(ERROR_INSUFFICIENT_BUFFER);return 0;}auto info=index>0&&index<=count?stw_pixelformat_get_info(index):nullptr;if(!info){SetLastError(ERROR_INVALID_PIXEL_FORMAT);return 0;}*result=info->pfd;return count;
}
extern "C" BOOL SetPixelFormat(HDC dc,int index,const PIXELFORMATDESCRIPTOR*){
 std::lock_guard<std::recursive_mutex> lock(gate);if(!Valid(dc)){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}int count=stw_pixelformat_get_count(dc);if(index<=0||index>count||!stw_pixelformat_get_info(index)){SetLastError(ERROR_INVALID_PIXEL_FORMAT);return FALSE;}
 if(selectedFormat&&selectedFormat!=index){SetLastError(ERROR_INVALID_PIXEL_FORMAT);return FALSE;}selectedFormat=index;return TRUE;
}
extern "C" __declspec(dllexport) BOOL __cdecl HWMesaPrepareDC(HDC dc){
 std::lock_guard<std::recursive_mutex> lock(gate);if(!Valid(dc)){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}if(selectedFormat)return TRUE;int count=stw_pixelformat_get_count(dc);
 for(int index=1;index<=count;++index){auto info=stw_pixelformat_get_info(index);if(info&&info->stvis.color_format==PIPE_FORMAT_B8G8R8A8_UNORM&&(info->pfd.dwFlags&PFD_DOUBLEBUFFER))return SetPixelFormat(dc,index,&info->pfd);}
 SetLastError(ERROR_INVALID_PIXEL_FORMAT);return FALSE;
}
// The host owns the real CoreWindow. Fabricating native window creation or
// successful destruction here would corrupt that lifecycle contract.
extern "C" HWND WINAPI CreateWindowEx(DWORD,LPCSTR,LPCSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID){SetLastError(ERROR_NOT_SUPPORTED);return nullptr;}
extern "C" BOOL WINAPI DestroyWindow(HWND value){std::lock_guard<std::recursive_mutex> lock(gate);SetLastError(Valid(value)?ERROR_ACCESS_DENIED:ERROR_INVALID_WINDOW_HANDLE);return FALSE;}
extern "C" BOOL WINAPI AdjustWindowRectEx(RECT* result,DWORD style,BOOL menu,DWORD extended){if(!result){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}if(menu||extended||(style&0x00ff0000)){SetLastError(ERROR_NOT_SUPPORTED);return FALSE;}return TRUE; /* No nonclient decoration in this CoreWindow-only contract. */}
void StretchDIBits(HDC,unsigned int,unsigned int,unsigned int,unsigned int,unsigned int,unsigned int,unsigned int,unsigned int,void*,void*,unsigned int,DWORD){SetLastError(ERROR_NOT_SUPPORTED); /* Software-GDI presentation is not selected or implemented. */}
