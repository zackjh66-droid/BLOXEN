#include "../Native/PerformanceMarkers.h"
#include <wrl/client.h>
#include <atomic>
#include <string>
#include <stdexcept>
#include <iostream>
using Microsoft::WRL::ComPtr;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
// Original instrumented COM receiver checks ABI, arguments and dispatch. Actual
// native D3D11 annotation behavior is additionally compared below on WARP.
struct Receiver:ID3DUserDefinedAnnotation {std::atomic<ULONG> refs{1};int depth=0;std::wstring label;HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id==__uuidof(IUnknown)||id==__uuidof(ID3DUserDefinedAnnotation)){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{auto r=--refs;if(!r)delete this;return r;}INT STDMETHODCALLTYPE BeginEvent(LPCWSTR name)override{label=name;return depth++;}INT STDMETHODCALLTYPE EndEvent()override{return depth?--depth:-1;}void STDMETHODCALLTYPE SetMarker(LPCWSTR)override{}BOOL STDMETHODCALLTYPE GetStatus()override{return TRUE;}};
int main(){try{
 Check(HWPerformanceBegin(0,L"before-device")==-1&&HWPerformanceEnd()==-1,"absent device pretends profiling success");Check(HWPerformanceBegin(0,nullptr)==-1,"null name admitted");
 auto receiver=new Receiver;HWBindPerformanceAnnotationForTest(receiver);Check(HWPerformanceBegin(0xFF123456,L"owned-region")==0&&receiver->label==L"[FF123456] owned-region","trace arguments not forwarded");Check(HWPerformanceBegin(0,L"nested")==1&&HWPerformanceEnd()==1&&HWPerformanceEnd()==0&&HWPerformanceEnd()==-1,"trace nesting/results lost");HWBindPerformanceAnnotationForTest(nullptr);receiver->Release();
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);Check(SUCCEEDED(hr),"real native D3D11 device failed");ComPtr<ID3DUserDefinedAnnotation> native;Check(SUCCEEDED(context.As(&native))&&SUCCEEDED(HWBindPerformanceDevice(device.Get())),"actual annotation QI/binding failed");
 auto expected=native->BeginEvent(L"native-reference");auto ended=native->EndEvent();Check(HWPerformanceBegin(0,L"adapter-region")==expected&&HWPerformanceEnd()==ended,"actual native disabled/nesting return not preserved");Check(!ResolvePerformanceImport("d3d9.dll","Direct3DCreate9Ex"),"D3D9 renderer fabricated");
 HWBindPerformanceAnnotationForTest(nullptr);std::cout<<"PASS trace ABI/labels/nesting, missing-device rejection and comparison with actual native D3D11 annotations. No game/Xbox execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
