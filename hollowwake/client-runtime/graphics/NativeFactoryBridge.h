#pragma once
#include <windows.h>
#include <d3d11.h>
namespace Hollowwake {
// Interposition is limited to this process's native DXGI factory methods and
// explicitly registered client HWNDs (native or bridge-owned). Unregistered host
// HWND calls keep native behavior.
void ConfigureNativeFactoryBridge(IUnknown* coreWindow,BOOL(WINAPI*isGuestWindow)(HWND));
HRESULT BridgeDeviceFactories(ID3D11Device* device);
HRESULT BridgeNativeFactory(IUnknown* factory);
HRESULT RemoveNativeFactoryBridgeForTest();
}
