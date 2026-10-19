#include "PerformanceMarkers.h"
#include <wrl/client.h>
#include <mutex>
#include <cstring>
#include <cwchar>
#include <string>
using Microsoft::WRL::ComPtr;
namespace {std::recursive_mutex gate;ComPtr<ID3DUserDefinedAnnotation> annotation;}
HRESULT HWBindPerformanceDevice(ID3D11Device* device){
 if(!device)return E_INVALIDARG;ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);if(!context)return E_NOINTERFACE;
 ComPtr<ID3DUserDefinedAnnotation> value;auto hr=context.As(&value);if(FAILED(hr))return hr;
 std::lock_guard<std::recursive_mutex> lock(gate);annotation=value;return S_OK;
}
void HWBindPerformanceAnnotationForTest(ID3DUserDefinedAnnotation* value){std::lock_guard<std::recursive_mutex> lock(gate);annotation=value;}
int WINAPI HWPerformanceBegin(DWORD color,LPCWSTR name){
 if(!name||wcsnlen_s(name,32768)==32768){SetLastError(ERROR_INVALID_PARAMETER);return -1;}
 std::lock_guard<std::recursive_mutex> lock(gate);if(!annotation)return -1;
 // D3D11 annotations have no marker-color argument. Retain the original ARGB
 // hint in the actual trace label rather than pretending that a color was set.
 wchar_t prefix[20];swprintf_s(prefix,L"[%08lX] ",color);std::wstring label=prefix;label+=name;
 return annotation->BeginEvent(label.c_str());
}
int WINAPI HWPerformanceEnd(){std::lock_guard<std::recursive_mutex> lock(gate);return annotation?annotation->EndEvent():-1;}
void* ResolvePerformanceImport(const char* dll,const char* name){if(!dll||!name||_stricmp(dll,"d3d9.dll"))return nullptr;if(!strcmp(name,"D3DPERF_BeginEvent"))return reinterpret_cast<void*>(&HWPerformanceBegin);if(!strcmp(name,"D3DPERF_EndEvent"))return reinterpret_cast<void*>(&HWPerformanceEnd);return nullptr;}
