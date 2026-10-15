#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
namespace Hollowwake {
// Call on the owning CoreWindow thread. window is the real CoreWindow IUnknown.
HRESULT CreateCoreSwapChain(IUnknown* window,IUnknown* device,const DXGI_SWAP_CHAIN_DESC& desktop,IDXGISwapChain** result);
HRESULT CreateCoreDeviceAndSwapChain(IUnknown* window,IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,const DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** swap,ID3D11Device** device,D3D_FEATURE_LEVEL* chosen,ID3D11DeviceContext** context);
}
namespace Hollowwake {
// Exact client imports CreateDXGIFactory{,1} and D3D11CreateDevice separately.
// This factory intercepts IDXGIFactory::CreateSwapChain for that path.
HRESULT CreateCoreFactory(IUnknown* window,REFIID iid,void** result);
}
