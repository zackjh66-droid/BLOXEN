// Original bounded UWP presentation bridge and DXGI wrappers.
// This is not complete desktop/window or D3D COM-graph emulation.
#include "CoreWindowBridge.h"
#include "WindowedSwapChain.h"
#include "NativeFactoryBridge.h"
#include <wrl/client.h>
#include <new>
#include <atomic>
using Microsoft::WRL::ComPtr;
namespace Hollowwake {
static std::atomic<void(*)(const std::string&)> presentationLogger{nullptr};
static std::atomic<unsigned> presentationEvents{0};
void SetPresentationLogger(void(*logger)(const std::string&)){presentationLogger.store(logger);}
static void PresentationEvent(const DXGI_SWAP_CHAIN_DESC& d,const char* stage,HRESULT result){
 auto logger=presentationLogger.load();if(!logger||presentationEvents.fetch_add(1)>=16)return;auto error=GetLastError();
 logger(std::string("NATIVE_PRESENTATION stage=")+stage+" HRESULT="+std::to_string(static_cast<unsigned long>(result))+" width="+std::to_string(d.BufferDesc.Width)+" height="+std::to_string(d.BufferDesc.Height)+" format="+std::to_string(static_cast<unsigned>(d.BufferDesc.Format))+" buffers="+std::to_string(d.BufferCount)+" samples="+std::to_string(d.SampleDesc.Count)+" quality="+std::to_string(d.SampleDesc.Quality)+" flags="+std::to_string(d.Flags)+" windowed="+std::to_string(static_cast<unsigned>(!!d.Windowed)));SetLastError(error);
}
static HRESULT(*compositionAttach)(IDXGISwapChain*)=nullptr;
void SetCompositionAttachment(HRESULT(*attach)(IDXGISwapChain*)){compositionAttach=attach;}
HRESULT CreateCoreSwapChain(IUnknown* window,IUnknown* device,const DXGI_SWAP_CHAIN_DESC& d,IDXGISwapChain** result,IUnknown* virtualFactory){
 if(!result)return E_POINTER;*result=nullptr;
 if(!window||!device)return E_INVALIDARG;
 PresentationEvent(d,"request",S_OK);
 // Never silently convert exclusive, multisampled or mode-switching requests.
 if(!d.Windowed||d.SampleDesc.Count!=1||d.SampleDesc.Quality||d.Flags||d.BufferCount>16){PresentationEvent(d,"unsupported-request",DXGI_ERROR_UNSUPPORTED);return DXGI_ERROR_UNSUPPORTED;}
 if(d.BufferDesc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&d.BufferDesc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&d.BufferDesc.Format!=DXGI_FORMAT_R10G10B10A2_UNORM&&d.BufferDesc.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT){PresentationEvent(d,"unsupported-format",DXGI_ERROR_UNSUPPORTED);return DXGI_ERROR_UNSUPPORTED;}
 ComPtr<IDXGIDevice> dxgi;HRESULT hr=device->QueryInterface(IID_PPV_ARGS(&dxgi));if(FAILED(hr))return hr;
 ComPtr<IDXGIAdapter> adapter;hr=dxgi->GetAdapter(&adapter);if(FAILED(hr))return hr;
 ComPtr<IDXGIFactory2> factory;hr=adapter->GetParent(IID_PPV_ARGS(&factory));if(FAILED(hr))return hr;
 DXGI_SWAP_CHAIN_DESC1 s={};s.Width=d.BufferDesc.Width;s.Height=d.BufferDesc.Height;s.Format=d.BufferDesc.Format;s.SampleDesc.Count=1;s.BufferUsage=d.BufferUsage;
 s.BufferCount=d.BufferCount<2?2:d.BufferCount;s.Scaling=DXGI_SCALING_STRETCH;s.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;s.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
 ComPtr<IDXGISwapChain1> chain;hr=compositionAttach?factory->CreateSwapChainForComposition(device,&s,nullptr,&chain):factory->CreateSwapChainForCoreWindow(device,window,&s,nullptr,&chain);PresentationEvent(d,"native-create",hr);if(FAILED(hr))return hr;
 if(compositionAttach){hr=compositionAttach(chain.Get());PresentationEvent(d,"composition-attach",hr);if(FAILED(hr))return hr;}
 return WrapWindowedChain(chain.Get(),d.OutputWindow,__uuidof(IDXGISwapChain),reinterpret_cast<void**>(result),virtualFactory);
}
HRESULT CreateCoreDeviceAndSwapChain(IUnknown* window,IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,const DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** swap,ID3D11Device** device,D3D_FEATURE_LEVEL* chosen,ID3D11DeviceContext** context){
 if(device)*device=nullptr;if(context)*context=nullptr;if(swap)*swap=nullptr;
 if(!desc||!swap||!window)return E_INVALIDARG;
 ComPtr<ID3D11Device> dev;ComPtr<ID3D11DeviceContext> ctx;D3D_FEATURE_LEVEL obtained;
 HRESULT hr=CreateCoreDevice(adapter,type,software,flags,levels,count,sdk,&dev,&obtained,&ctx);if(FAILED(hr))return hr;
 ComPtr<IDXGISwapChain> chain;hr=CreateCoreSwapChain(window,dev.Get(),*desc,&chain);if(FAILED(hr))return hr;
 *swap=chain.Detach();if(device)*device=dev.Detach();if(context)*context=ctx.Detach();if(chosen)*chosen=obtained;return S_OK;
}
}
namespace Hollowwake {
// Original private COM interface: unwrap only our adapter before passing it to
// D3D11. Native D3D11 cannot consume an arbitrary guest COM adapter proxy.
struct __declspec(uuid("D6BBCE0A-03AC-4F72-925B-3ACCA34C6231")) ICoreAdapterNative:public IUnknown {virtual HRESULT STDMETHODCALLTYPE NativeAdapter(IDXGIAdapter**)=0;};
class CoreOutput final:public IDXGIOutput {
 volatile LONG refs=1;ComPtr<IDXGIOutput> inner;ComPtr<IDXGIAdapter> parent;
public:
 CoreOutput(IDXGIOutput* output,IDXGIAdapter* adapter):inner(output),parent(adapter){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override{if(!out)return E_POINTER;*out=nullptr;if(iid!=__uuidof(IUnknown)&&iid!=__uuidof(IDXGIObject)&&iid!=__uuidof(IDXGIOutput))return E_NOINTERFACE;*out=static_cast<IDXGIOutput*>(this);AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() override{ULONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID k,UINT n,const void* d) override{return inner->SetPrivateData(k,n,d);}
 HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID k,const IUnknown* d) override{return inner->SetPrivateDataInterface(k,d);}
 HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID k,UINT* n,void* d) override{return inner->GetPrivateData(k,n,d);}
 HRESULT STDMETHODCALLTYPE GetParent(REFIID iid,void** out) override{return parent->QueryInterface(iid,out);}
 HRESULT STDMETHODCALLTYPE GetDesc(DXGI_OUTPUT_DESC* d) override{return inner->GetDesc(d);}
 HRESULT STDMETHODCALLTYPE GetDisplayModeList(DXGI_FORMAT f,UINT flags,UINT* n,DXGI_MODE_DESC* d) override{return inner->GetDisplayModeList(f,flags,n,d);}
 HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(const DXGI_MODE_DESC* want,DXGI_MODE_DESC* out,IUnknown* device) override{return inner->FindClosestMatchingMode(want,out,device);}
 HRESULT STDMETHODCALLTYPE WaitForVBlank() override{return inner->WaitForVBlank();}
 HRESULT STDMETHODCALLTYPE TakeOwnership(IUnknown* device,BOOL exclusive) override{return inner->TakeOwnership(device,exclusive);}
 void STDMETHODCALLTYPE ReleaseOwnership() override{inner->ReleaseOwnership();}
 HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES* c) override{return inner->GetGammaControlCapabilities(c);}
 HRESULT STDMETHODCALLTYPE SetGammaControl(const DXGI_GAMMA_CONTROL* c) override{return inner->SetGammaControl(c);}
 HRESULT STDMETHODCALLTYPE GetGammaControl(DXGI_GAMMA_CONTROL* c) override{return inner->GetGammaControl(c);}
 HRESULT STDMETHODCALLTYPE SetDisplaySurface(IDXGISurface* s) override{return inner->SetDisplaySurface(s);}
 HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(IDXGISurface* s) override{return inner->GetDisplaySurfaceData(s);}
 HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* s) override{return inner->GetFrameStatistics(s);}
};
class CoreAdapter final:public IDXGIAdapter1,public ICoreAdapterNative {
 volatile LONG refs=1;ComPtr<IDXGIAdapter1> inner;ComPtr<IDXGIFactory1> parent;
public:
 CoreAdapter(IDXGIAdapter1* a,IDXGIFactory1* p):inner(a),parent(p){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {if(!out)return E_POINTER;*out=nullptr;if(iid==__uuidof(IUnknown)||iid==__uuidof(IDXGIObject)||iid==__uuidof(IDXGIAdapter)||iid==__uuidof(IDXGIAdapter1))*out=static_cast<IDXGIAdapter1*>(this);else if(iid==__uuidof(ICoreAdapterNative))*out=static_cast<ICoreAdapterNative*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() override{ULONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE NativeAdapter(IDXGIAdapter** out) override{return inner.CopyTo(out);}
 HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID k,UINT n,const void* d) override{return inner->SetPrivateData(k,n,d);}
 HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID k,const IUnknown* d) override{return inner->SetPrivateDataInterface(k,d);}
 HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID k,UINT* n,void* d) override{return inner->GetPrivateData(k,n,d);}
 HRESULT STDMETHODCALLTYPE GetParent(REFIID iid,void** out) override{return parent->QueryInterface(iid,out);}
 HRESULT STDMETHODCALLTYPE EnumOutputs(UINT n,IDXGIOutput** out) override{if(!out)return E_POINTER;*out=nullptr;ComPtr<IDXGIOutput> output;auto hr=inner->EnumOutputs(n,&output);if(FAILED(hr))return hr;auto proxy=new(std::nothrow) CoreOutput(output.Get(),this);if(!proxy)return E_OUTOFMEMORY;*out=proxy;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetDesc(DXGI_ADAPTER_DESC* out) override{return inner->GetDesc(out);}
 HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(REFGUID k,LARGE_INTEGER* version) override{return inner->CheckInterfaceSupport(k,version);}
 HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_ADAPTER_DESC1* out) override{return inner->GetDesc1(out);}
};
HRESULT UnwrapCoreAdapter(IDXGIAdapter* adapter,IDXGIAdapter** result){
 if(!result)return E_POINTER;*result=nullptr;if(!adapter)return S_OK;
 ComPtr<ICoreAdapterNative> wrapper;auto hr=adapter->QueryInterface(IID_PPV_ARGS(&wrapper));
 if(hr==E_NOINTERFACE){adapter->AddRef();*result=adapter;return S_OK;}
 if(FAILED(hr))return hr;return wrapper->NativeAdapter(result);
}
HRESULT CreateCoreDevice(IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,ID3D11Device** device,D3D_FEATURE_LEVEL* chosen,ID3D11DeviceContext** context){
 if(device)*device=nullptr;if(context)*context=nullptr;
 ComPtr<IDXGIAdapter> native;auto hr=UnwrapCoreAdapter(adapter,&native);if(FAILED(hr))return hr;
 ComPtr<ID3D11Device> created;ComPtr<ID3D11DeviceContext> immediate;D3D_FEATURE_LEVEL actual;
 hr=::D3D11CreateDevice(native.Get(),type,software,flags,levels,count,sdk,device?&created:nullptr,&actual,context?&immediate:nullptr);if(FAILED(hr))return hr;auto nativeResult=hr;
 if(!created&&immediate)immediate->GetDevice(&created);
 if(created){hr=BridgeDeviceFactories(created.Get());if(FAILED(hr))return hr;}
 if(device)*device=created.Detach();if(context)*context=immediate.Detach();if(chosen)*chosen=actual;return nativeResult;
}
class CoreFactory final:public IDXGIFactory1 {
 volatile LONG refs=1;ComPtr<IDXGIFactory1> inner;ComPtr<IUnknown> window;HWND association=nullptr;
public:
 CoreFactory(IDXGIFactory1* f,IUnknown* w):inner(f),window(w){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {if(!out)return E_POINTER;*out=nullptr;if(iid==__uuidof(IUnknown)||iid==__uuidof(IDXGIObject)||iid==__uuidof(IDXGIFactory)||iid==__uuidof(IDXGIFactory1)){*out=static_cast<IDXGIFactory1*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() override{ULONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID key,UINT size,const void* data) override{return inner->SetPrivateData(key,size,data);}
 HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID key,const IUnknown* value) override{return inner->SetPrivateDataInterface(key,value);}
 HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID key,UINT* size,void* data) override{return inner->GetPrivateData(key,size,data);}
 HRESULT STDMETHODCALLTYPE GetParent(REFIID,void** value) override{if(!value)return E_POINTER;*value=nullptr;return E_NOINTERFACE;}
 HRESULT STDMETHODCALLTYPE EnumAdapters(UINT i,IDXGIAdapter** value) override{if(!value)return E_POINTER;*value=nullptr;ComPtr<IDXGIAdapter1> adapter;auto hr=inner->EnumAdapters1(i,&adapter);if(FAILED(hr))return hr;auto proxy=new(std::nothrow) CoreAdapter(adapter.Get(),this);if(!proxy)return E_OUTOFMEMORY;*value=proxy;return S_OK;}
 HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND h,UINT flags) override {if(flags&~(DXGI_MWA_NO_WINDOW_CHANGES|DXGI_MWA_NO_ALT_ENTER|DXGI_MWA_NO_PRINT_SCREEN))return E_INVALIDARG;association=h;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND* h) override{if(!h)return E_POINTER;*h=association;return S_OK;}
 HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown* device,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** chain) override {if(!desc)return E_POINTER;return CreateCoreSwapChain(window.Get(),device,*desc,chain,static_cast<IDXGIFactory1*>(this));}
 HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE,IDXGIAdapter** value) override {if(value)*value=nullptr;return DXGI_ERROR_UNSUPPORTED;}
 HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT i,IDXGIAdapter1** value) override{if(!value)return E_POINTER;*value=nullptr;ComPtr<IDXGIAdapter1> adapter;auto hr=inner->EnumAdapters1(i,&adapter);if(FAILED(hr))return hr;auto proxy=new(std::nothrow) CoreAdapter(adapter.Get(),this);if(!proxy)return E_OUTOFMEMORY;*value=proxy;return S_OK;}
 BOOL STDMETHODCALLTYPE IsCurrent() override{return inner->IsCurrent();}
};
HRESULT CreateCoreFactory(IUnknown* window,REFIID iid,void** value){
 if(!value)return E_POINTER;*value=nullptr;
 ComPtr<IDXGIFactory1> native;HRESULT hr=::CreateDXGIFactory1(IID_PPV_ARGS(&native));if(FAILED(hr))return hr;
 CoreFactory* proxy=new(std::nothrow) CoreFactory(native.Get(),window);if(!proxy)return E_OUTOFMEMORY;
 hr=proxy->QueryInterface(iid,value);proxy->Release();return hr;
}
}
