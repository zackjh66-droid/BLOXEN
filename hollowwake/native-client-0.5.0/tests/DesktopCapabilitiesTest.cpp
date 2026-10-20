// Owned Windows delegation checks, not proof of absent Xbox feature support.
#include <windows.h>
#include <imm.h>
#include <gl/GL.h>
#include <dwmapi.h>
#include <d3d9.h>
#include <cassert>
#include <iostream>
#include "../Native/DesktopCapabilities.h"
template<class T>T Adapter(const char* dll,const char* api){DWORD error=0;assert(IsDesktopCapabilityAdapter(dll,api));assert(NativeDesktopCapabilityAvailable(dll,api,&error)&&error==0);auto value=ResolveDesktopCapabilityImport(dll,api);assert(value);return reinterpret_cast<T>(value);}
int main(){
 auto create=Adapter<decltype(&ImmCreateContext)>("imm32.dll","ImmCreateContext");auto destroy=Adapter<decltype(&ImmDestroyContext)>("imm32.dll","ImmDestroyContext");auto context=create();assert(context);assert(destroy(context));
 auto description=Adapter<decltype(&ImmGetDescriptionW)>("imm32.dll","ImmGetDescriptionW");auto filename=Adapter<decltype(&ImmGetIMEFileNameW)>("imm32.dll","ImmGetIMEFileNameW");auto layout=GetKeyboardLayout(0);wchar_t a[1024]={},b[1024]={};assert(description(layout,a,1024)==ImmGetDescriptionW(layout,b,1024));assert(!wcscmp(a,b));assert(filename(layout,a,1024)==ImmGetIMEFileNameW(layout,b,1024));assert(!wcscmp(a,b));
 assert(Adapter<decltype(&wglGetCurrentContext)>("opengl32.dll","wglGetCurrentContext")()==wglGetCurrentContext());assert(Adapter<decltype(&wglGetCurrentDC)>("opengl32.dll","wglGetCurrentDC")()==wglGetCurrentDC());
 assert(!Adapter<decltype(&wglGetProcAddress)>("opengl32.dll","wglGetProcAddress")("hw_nonexistent_extension"));
 // Invalid DC/context operations must produce native failure, never a fake handle.
 assert(!Adapter<decltype(&wglCreateContext)>("opengl32.dll","wglCreateContext")(nullptr));assert(!Adapter<decltype(&wglDeleteContext)>("opengl32.dll","wglDeleteContext")(nullptr));
 assert(Adapter<decltype(&wglMakeCurrent)>("opengl32.dll","wglMakeCurrent")(nullptr,nullptr)==wglMakeCurrent(nullptr,nullptr));
 auto flush=Adapter<decltype(&DwmFlush)>("dwmapi.dll","DwmFlush");HRESULT flushResult=flush();std::cout<<"Actual native DwmFlush HRESULT="<<std::hex<<(unsigned long)flushResult<<"\n";
 auto d3d=Adapter<decltype(&Direct3DCreate9Ex)>("d3d9.dll","Direct3DCreate9Ex");IDirect3D9Ex* direct=nullptr;IDirect3D9Ex* adapted=nullptr;HRESULT expected=Direct3DCreate9Ex(D3D_SDK_VERSION,&direct),actual=d3d(D3D_SDK_VERSION,&adapted);assert(actual==expected);assert((adapted!=nullptr)==(direct!=nullptr));if(direct)direct->Release();if(adapted)adapted->Release();
 assert(!ResolveDesktopCapabilityImport("imm32.dll","HWInventedApi"));std::cout<<"PASS 12 typed adapters delegate real desktop operations and failures; no Xbox/GL rendering or game execution\n";
}
