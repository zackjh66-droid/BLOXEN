#include "WindowedSwapChain.h"
#include <wrl/client.h>
#include <new>
using Microsoft::WRL::ComPtr;
namespace Hollowwake {
static HRESULT(*viewportProvider)(UINT*,UINT*)=nullptr;
void SetChainViewportProvider(HRESULT(*provider)(UINT*,UINT*)){viewportProvider=provider;}
namespace {
class WindowedChain final:public IDXGISwapChain1 {
 volatile LONG refs=1;ComPtr<IDXGISwapChain1> inner;ComPtr<IUnknown> parent;HWND window;
public:
 WindowedChain(IDXGISwapChain1* chain,HWND handle,IUnknown* factory):inner(chain),parent(factory),window(handle){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value) override {if(!value)return E_POINTER;*value=nullptr;if(iid==__uuidof(IUnknown)||iid==__uuidof(IDXGIObject)||iid==__uuidof(IDXGIDeviceSubObject)||iid==__uuidof(IDXGISwapChain)||iid==__uuidof(IDXGISwapChain1)){*value=static_cast<IDXGISwapChain1*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() override {ULONG value=InterlockedDecrement(&refs);if(!value)delete this;return value;}
 HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID key,UINT bytes,const void* value) override{return inner->SetPrivateData(key,bytes,value);}
 HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID key,const IUnknown* value) override{return inner->SetPrivateDataInterface(key,value);}
 HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID key,UINT* bytes,void* value) override{return inner->GetPrivateData(key,bytes,value);}
 HRESULT STDMETHODCALLTYPE GetParent(REFIID iid,void** value) override{return parent?parent->QueryInterface(iid,value):inner->GetParent(iid,value);}
 HRESULT STDMETHODCALLTYPE GetDevice(REFIID iid,void** value) override{return inner->GetDevice(iid,value);}
 HRESULT STDMETHODCALLTYPE Present(UINT interval,UINT flags) override{return inner->Present(interval,flags);}
 HRESULT STDMETHODCALLTYPE GetBuffer(UINT index,REFIID iid,void** value) override{return inner->GetBuffer(index,iid,value);}
 HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL exclusive,IDXGIOutput* target) override{if(exclusive||target)return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;return S_OK; /* This chain is already windowed. */}
 HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* exclusive,IDXGIOutput** target) override{if(!exclusive)return E_INVALIDARG;*exclusive=FALSE;if(target)*target=nullptr;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* value) override{if(!value)return E_POINTER;DXGI_SWAP_CHAIN_DESC1 real={};auto hr=inner->GetDesc1(&real);if(FAILED(hr))return hr;*value={};value->BufferDesc.Width=real.Width;value->BufferDesc.Height=real.Height;value->BufferDesc.Format=real.Format;value->SampleDesc=real.SampleDesc;value->BufferUsage=real.BufferUsage;value->BufferCount=real.BufferCount;value->OutputWindow=window;value->Windowed=TRUE;value->SwapEffect=real.SwapEffect;value->Flags=real.Flags;return S_OK;}
 HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags) override{
  // Never mask a request for a capability this chain does not provide.
  if(flags)return DXGI_ERROR_UNSUPPORTED;DXGI_SWAP_CHAIN_DESC1 prior={};auto hr=inner->GetDesc1(&prior);if(FAILED(hr))return hr;
  if(count==1||count>16)return DXGI_ERROR_UNSUPPORTED;
  if(!width||!height){UINT actualWidth=0,actualHeight=0;if(!viewportProvider)return DXGI_ERROR_INVALID_CALL;hr=viewportProvider(&actualWidth,&actualHeight);if(FAILED(hr))return hr;if(!actualWidth||!actualHeight)return E_UNEXPECTED;if(!width)width=actualWidth;if(!height)height=actualHeight;}
  return inner->ResizeBuffers(count,width,height,format,0);
 }
 HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* mode) override{if(!mode)return E_INVALIDARG;DXGI_SWAP_CHAIN_DESC1 real={};auto hr=inner->GetDesc1(&real);if(FAILED(hr))return hr;
  // Matching the existing target is a real no-change operation, not a display switch.
  if(mode->Width!=real.Width||mode->Height!=real.Height||(mode->Format!=DXGI_FORMAT_UNKNOWN&&mode->Format!=real.Format)||mode->RefreshRate.Numerator||mode->RefreshRate.Denominator||mode->Scaling!=DXGI_MODE_SCALING_UNSPECIFIED||mode->ScanlineOrdering!=DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED)return DXGI_ERROR_UNSUPPORTED;return S_OK;
 }
 HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** value) override{return inner->GetContainingOutput(value);}
 HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* value) override{return inner->GetFrameStatistics(value);}
 HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* value) override{return inner->GetLastPresentCount(value);}
 HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* value) override{return inner->GetDesc1(value);}
 HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* value) override{if(!value)return E_POINTER;*value={};value->Windowed=TRUE;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetHwnd(HWND* value) override{if(!value)return E_POINTER;*value=window;return window?S_OK:DXGI_ERROR_INVALID_CALL;}
 HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID iid,void** value) override{return inner->GetCoreWindow(iid,value);}
 HRESULT STDMETHODCALLTYPE Present1(UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* parameters) override{return inner->Present1(interval,flags,parameters);}
 BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override{return inner->IsTemporaryMonoSupported();}
 HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** value) override{return inner->GetRestrictToOutput(value);}
 HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* value) override{return inner->SetBackgroundColor(value);}
 HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* value) override{return inner->GetBackgroundColor(value);}
 HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION value) override{return inner->SetRotation(value);}
 HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* value) override{return inner->GetRotation(value);}
};
}
HRESULT WrapWindowedChain(IDXGISwapChain1* chain,HWND window,REFIID iid,void** value,IUnknown* parent){if(!value)return E_POINTER;*value=nullptr;if(!chain)return E_INVALIDARG;auto wrapped=new(std::nothrow)WindowedChain(chain,window,parent);if(!wrapped)return E_OUTOFMEMORY;auto hr=wrapped->QueryInterface(iid,value);wrapped->Release();return hr;}
}
