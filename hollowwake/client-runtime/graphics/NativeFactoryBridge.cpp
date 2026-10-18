#include "NativeFactoryBridge.h"
#include "CoreWindowBridge.h"
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <map>
#include <mutex>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace Hollowwake { namespace {
using Create=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using CreateHwnd=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
struct Slot{void** address;void* original;void* replacement;};
static std::mutex gate;static std::map<void**,Slot> slots;static ComPtr<IUnknown> presentationWindow;static BOOL(WINAPI*guestWindow)(HWND)=nullptr;
static void** Address(void* object,unsigned index){return *reinterpret_cast<void***>(object)+index;}
static void* Original(void* object,unsigned index){std::lock_guard<std::mutex> lock(gate);auto found=slots.find(Address(object,index));return found==slots.end()?nullptr:found->second.original;}
static bool Guest(HWND window,ComPtr<IUnknown>& core){std::lock_guard<std::mutex> lock(gate);if(!guestWindow||!guestWindow(window))return false;core=presentationWindow;return core!=nullptr;}
static HRESULT STDMETHODCALLTYPE InterceptCreate(IDXGIFactory* factory,IUnknown* device,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** result){
 auto original=reinterpret_cast<Create>(Original(factory,10));if(!original){if(result)*result=nullptr;return E_UNEXPECTED;}
 ComPtr<IUnknown> core;if(!desc||!Guest(desc->OutputWindow,core))return original(factory,device,desc,result);
 return CreateCoreSwapChain(core.Get(),device,*desc,result,factory);
}
static HRESULT STDMETHODCALLTYPE InterceptHwnd(IDXGIFactory2* factory,IUnknown* device,HWND hwnd,const DXGI_SWAP_CHAIN_DESC1* desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,IDXGIOutput* restrictOutput,IDXGISwapChain1** result){
 auto original=reinterpret_cast<CreateHwnd>(Original(factory,15));if(!original){if(result)*result=nullptr;return E_UNEXPECTED;}
 ComPtr<IUnknown> core;if(!Guest(hwnd,core))return original(factory,device,hwnd,desc,fullscreen,restrictOutput,result);
 if(!result)return E_POINTER;*result=nullptr;if(!desc)return E_INVALIDARG;if(restrictOutput||(fullscreen&&!fullscreen->Windowed)||desc->Stereo||(desc->AlphaMode!=DXGI_ALPHA_MODE_IGNORE&&desc->AlphaMode!=DXGI_ALPHA_MODE_UNSPECIFIED)||desc->Scaling!=DXGI_SCALING_STRETCH)return DXGI_ERROR_UNSUPPORTED;
 DXGI_SWAP_CHAIN_DESC legacy={};legacy.BufferDesc.Width=desc->Width;legacy.BufferDesc.Height=desc->Height;legacy.BufferDesc.Format=desc->Format;legacy.SampleDesc=desc->SampleDesc;legacy.BufferUsage=desc->BufferUsage;legacy.BufferCount=desc->BufferCount;legacy.OutputWindow=hwnd;legacy.Windowed=TRUE;legacy.SwapEffect=desc->SwapEffect;legacy.Flags=desc->Flags;
 ComPtr<IDXGISwapChain> chain;auto hr=CreateCoreSwapChain(core.Get(),device,legacy,&chain,factory);return FAILED(hr)?hr:chain.CopyTo(result);
}
static HRESULT Patch(void* object,unsigned index,void* replacement){
 auto address=Address(object,index);if(slots.count(address))return *address==replacement?S_OK:HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
 // This mutates only a non-executable dispatch-pointer slot in our process's
 // private image mapping. No original DLL file, foreign process, page of code,
 // protection policy or security setting is changed. Refusal is propagated.
 MEMORY_BASIC_INFORMATION info={};if(!VirtualQuery(address,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY|PAGE_GUARD|PAGE_NOACCESS)))return E_ACCESSDENIED;
 DWORD prior=0;if(!VirtualProtectFromApp(address,sizeof(void*),PAGE_READWRITE,&prior))return HRESULT_FROM_WIN32(GetLastError());
 void* original=*address;slots.emplace(address,Slot{address,original,replacement});
 void* observed=InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(address),replacement,original);
 if(observed!=original){slots.erase(address);DWORD ignored=0;VirtualProtectFromApp(address,sizeof(void*),prior,&ignored);return HRESULT_FROM_WIN32(ERROR_RETRY);}
 DWORD ignored=0;if(!VirtualProtectFromApp(address,sizeof(void*),prior,&ignored)){
  auto error=GetLastError();InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(address),original,replacement);slots.erase(address);DWORD retried=0;VirtualProtectFromApp(address,sizeof(void*),prior,&retried);return HRESULT_FROM_WIN32(error);
 }return S_OK;
}
}
void ConfigureNativeFactoryBridge(IUnknown* window,BOOL(WINAPI*validate)(HWND)){std::lock_guard<std::mutex> lock(gate);presentationWindow=window;guestWindow=validate;}
HRESULT BridgeDeviceFactories(ID3D11Device* device){
 if(!device)return E_INVALIDARG;ComPtr<IDXGIDevice> dxgi;auto hr=device->QueryInterface(IID_PPV_ARGS(&dxgi));if(FAILED(hr))return hr;ComPtr<IDXGIAdapter> adapter;hr=dxgi->GetAdapter(&adapter);if(FAILED(hr))return hr;ComPtr<IDXGIFactory> factory;hr=adapter->GetParent(IID_PPV_ARGS(&factory));if(FAILED(hr))return hr;
 std::lock_guard<std::mutex> lock(gate);if(!presentationWindow||!guestWindow)return E_UNEXPECTED;
 hr=Patch(factory.Get(),10,reinterpret_cast<void*>(&InterceptCreate));if(FAILED(hr))return hr;
 ComPtr<IDXGIFactory2> modern;hr=factory.As(&modern);if(hr==E_NOINTERFACE)return S_OK;if(FAILED(hr))return hr;
 hr=Patch(modern.Get(),10,reinterpret_cast<void*>(&InterceptCreate));if(FAILED(hr))return hr;
 return Patch(modern.Get(),15,reinterpret_cast<void*>(&InterceptHwnd));
}
HRESULT RemoveNativeFactoryBridgeForTest(){std::lock_guard<std::mutex> lock(gate);HRESULT result=S_OK;
 for(auto it=slots.begin();it!=slots.end();){auto slot=it->second;DWORD prior=0;if(!VirtualProtectFromApp(slot.address,sizeof(void*),PAGE_READWRITE,&prior)){result=HRESULT_FROM_WIN32(GetLastError());++it;continue;}
 auto observed=InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(slot.address),slot.original,slot.replacement);DWORD ignored=0;bool restored=VirtualProtectFromApp(slot.address,sizeof(void*),prior,&ignored)!=FALSE;if(observed!=slot.replacement||!restored){result=E_FAIL;++it;continue;}it=slots.erase(it);
 }return result;}
}
