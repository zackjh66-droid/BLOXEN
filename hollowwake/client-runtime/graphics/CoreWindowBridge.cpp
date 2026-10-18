// Original import-bridge implementation. Does not emulate user32 or DXGI factories.
#include "CoreWindowBridge.h"
#include "WindowedSwapChain.h"
#include <wrl/client.h>
#include <new>
using Microsoft::WRL::ComPtr;
namespace Hollowwake {
static HRESULT(*compositionAttach)(IDXGISwapChain*)=nullptr;
void SetCompositionAttachment(HRESULT(*attach)(IDXGISwapChain*)){compositionAttach=attach;}
HRESULT CreateCoreSwapChain(IUnknown* window,IUnknown* device,const DXGI_SWAP_CHAIN_DESC& d,IDXGISwapChain** result,IUnknown* virtualFactory){
 if(!result)return E_POINTER;*result=nullptr;
 if(!window||!device)return E_INVALIDARG;
 // Never silently convert exclusive, multisampled or mode-switching requests.
 if(!d.Windowed||d.SampleDesc.Count!=1||d.SampleDesc.Quality||d.Flags||d.BufferCount>16)return DXGI_ERROR_UNSUPPORTED;
 if(d.BufferDesc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&d.BufferDesc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM)return DXGI_ERROR_UNSUPPORTED;
 ComPtr<IDXGIDevice> dxgi;HRESULT hr=device->QueryInterface(IID_PPV_ARGS(&dxgi));if(FAILED(hr))return hr;
 ComPtr<IDXGIAdapter> adapter;hr=dxgi->GetAdapter(&adapter);if(FAILED(hr))return hr;
 ComPtr<IDXGIFactory2> factory;hr=adapter->GetParent(IID_PPV_ARGS(&factory));if(FAILED(hr))return hr;
 DXGI_SWAP_CHAIN_DESC1 s={};s.Width=d.BufferDesc.Width;s.Height=d.BufferDesc.Height;s.Format=d.BufferDesc.Format;s.SampleDesc.Count=1;s.BufferUsage=d.BufferUsage;
 s.BufferCount=d.BufferCount<2?2:d.BufferCount;s.Scaling=DXGI_SCALING_STRETCH;s.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;s.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
 ComPtr<IDXGISwapChain1> chain;hr=compositionAttach?factory->CreateSwapChainForComposition(device,&s,nullptr,&chain):factory->CreateSwapChainForCoreWindow(device,window,&s,nullptr,&chain);if(FAILED(hr))return hr;
 if(compositionAttach){hr=compositionAttach(chain.Get());if(FAILED(hr))return hr;}
 return WrapWindowedChain(chain.Get(),d.OutputWindow,__uuidof(IDXGISwapChain),reinterpret_cast<void**>(result),virtualFactory);
}
HRESULT CreateCoreDeviceAndSwapChain(IUnknown* window,IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,const DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** swap,ID3D11Device** device,D3D_FEATURE_LEVEL* chosen,ID3D11DeviceContext** context){
 if(device)*device=nullptr;if(context)*context=nullptr;if(swap)*swap=nullptr;
 if(!desc||!swap||!window)return E_INVALIDARG;
 ComPtr<ID3D11Device> dev;ComPtr<ID3D11DeviceContext> ctx;D3D_FEATURE_LEVEL obtained;
 HRESULT hr=D3D11CreateDevice(adapter,type,software,flags,levels,count,sdk,&dev,&obtained,&ctx);if(FAILED(hr))return hr;
 ComPtr<IDXGISwapChain> chain;hr=CreateCoreSwapChain(window,dev.Get(),*desc,&chain);if(FAILED(hr))return hr;
 *swap=chain.Detach();if(device)*device=dev.Detach();if(context)*context=ctx.Detach();if(chosen)*chosen=obtained;return S_OK;
}
}
namespace Hollowwake {
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
 HRESULT STDMETHODCALLTYPE EnumAdapters(UINT i,IDXGIAdapter** value) override{return inner->EnumAdapters(i,value);}
 HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND h,UINT flags) override {if(flags&~(DXGI_MWA_NO_WINDOW_CHANGES|DXGI_MWA_NO_ALT_ENTER|DXGI_MWA_NO_PRINT_SCREEN))return E_INVALIDARG;association=h;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND* h) override{if(!h)return E_POINTER;*h=association;return S_OK;}
 HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown* device,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** chain) override {if(!desc)return E_POINTER;return CreateCoreSwapChain(window.Get(),device,*desc,chain,static_cast<IDXGIFactory1*>(this));}
 HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE,IDXGIAdapter** value) override {if(value)*value=nullptr;return DXGI_ERROR_UNSUPPORTED;}
 HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT i,IDXGIAdapter1** value) override{return inner->EnumAdapters1(i,value);}
 BOOL STDMETHODCALLTYPE IsCurrent() override{return inner->IsCurrent();}
};
HRESULT CreateCoreFactory(IUnknown* window,REFIID iid,void** value){
 if(!value)return E_POINTER;*value=nullptr;if(!window)return E_INVALIDARG;
 ComPtr<IDXGIFactory1> native;HRESULT hr=::CreateDXGIFactory1(IID_PPV_ARGS(&native));if(FAILED(hr))return hr;
 CoreFactory* proxy=new(std::nothrow) CoreFactory(native.Get(),window);if(!proxy)return E_OUTOFMEMORY;
 hr=proxy->QueryInterface(iid,value);proxy->Release();return hr;
}
}
