#pragma once
#include <d3d11_1.h>
// Retains the actual application's D3D11 annotation interface. No private game
// GPU context is fabricated, and disabled/unavailable tracing returns -1.
HRESULT HWBindPerformanceDevice(ID3D11Device* device);
void HWBindPerformanceAnnotationForTest(ID3DUserDefinedAnnotation* annotation);
int WINAPI HWPerformanceBegin(DWORD color,LPCWSTR name);
int WINAPI HWPerformanceEnd();
void* ResolvePerformanceImport(const char* dll,const char* name);
