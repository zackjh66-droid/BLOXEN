#pragma once
#include <dxgi1_2.h>
namespace Hollowwake {
// Real composition chain with a bounded, explicitly windowed Win32 view.
// No exclusive fullscreen, display-mode changes or fake render buffers.
HRESULT WrapWindowedChain(IDXGISwapChain1* chain,HWND window,REFIID iid,void** result,IUnknown* parent=nullptr);
void SetChainViewportProvider(HRESULT(*provider)(UINT*,UINT*));
}
