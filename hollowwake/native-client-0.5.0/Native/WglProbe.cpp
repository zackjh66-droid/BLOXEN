// Owned focused test for the six actual missing WGL entry points and their
// necessary pixel-format/presentation bridge. No game code or fake GL context.
#include "WglProvider.h"
#include "ProviderAudit.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <fstream>
#include <string>
namespace {
using Def=LRESULT(WINAPI*)(HWND,UINT,WPARAM,LPARAM);Def originalDefault=nullptr;
LRESULT CALLBACK OwnedWindow(HWND w,UINT m,WPARAM p,LPARAM l){return originalDefault?originalDefault(w,m,p,l):0;}
// WNDPROC's typedef is desktop-partition gated in the UWP SDK. Keep the exact
// WNDCLASSEXW ABI for the dynamically resolved OS API; do not change partitions.
struct OwnedClass {UINT size,style;Def proc;int clsExtra,wndExtra;HINSTANCE instance;HICON icon;HCURSOR cursor;HBRUSH brush;LPCWSTR menu,name;HICON smallIcon;};
}
extern "C" __declspec(dllexport) int __cdecl HWWglProviderProbe(const wchar_t* report){
 if(!report)return E_POINTER;
 const std::wstring stageFile=std::wstring(report)+L".stage.txt";
 auto mark=[&](const char* next){std::ofstream progress(stageFile,std::ios::app);progress<<next<<"\n";progress.flush();return next;};HRESULT result=E_FAIL;DWORD error=0;const char* stage=mark("Native owned window providers");bool pixel=false,present=false,currentVerified=false;HWND hwnd=nullptr;HDC dc=nullptr;HGLRC context=nullptr;
 using Create=HWND(WINAPI*)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);using Reg=ATOM(WINAPI*)(const OwnedClass*);using Unreg=BOOL(WINAPI*)(LPCWSTR,HINSTANCE);using Get=HDC(WINAPI*)(HWND);using Release=int(WINAPI*)(HWND,HDC);using Destroy=BOOL(WINAPI*)(HWND);
 using Module=HMODULE(WINAPI*)(LPCWSTR);auto modules=reinterpret_cast<Module>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleW"));auto user=modules?modules(L"user32.dll"):nullptr;auto instance=modules?modules(L"HollowwakeClientRuntime.dll"):nullptr;
 auto reg=user?reinterpret_cast<Reg>(GetProcAddress(user,"RegisterClassExW")):nullptr;auto create=user?reinterpret_cast<Create>(GetProcAddress(user,"CreateWindowExW")):nullptr;auto get=user?reinterpret_cast<Get>(GetProcAddress(user,"GetDC")):nullptr;auto release=user?reinterpret_cast<Release>(GetProcAddress(user,"ReleaseDC")):nullptr;auto destroy=user?reinterpret_cast<Destroy>(GetProcAddress(user,"DestroyWindow")):nullptr;auto unreg=user?reinterpret_cast<Unreg>(GetProcAddress(user,"UnregisterClassW")):nullptr;originalDefault=user?reinterpret_cast<Def>(GetProcAddress(user,"DefWindowProcW")):nullptr;
 using Make=BOOL(WINAPI*)(HDC,HGLRC);using Delete=BOOL(WINAPI*)(HGLRC);Make make=nullptr;Delete erase=nullptr;
 do{
  stage=mark("Actual additional Mesa provider dependency audit");
  auto package=Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
  std::wstring inventory=std::wstring(package->Data())+L"\\provider-imports.json";
  std::wstring reportPath(report);auto slash=reportPath.find_last_of(L"/\\");
  if(slash==std::wstring::npos||AuditAdditionalProviderImports(inventory.c_str(),(reportPath.substr(0,slash+1)+L"native-provider-imports.json").c_str())!=0){error=ERROR_PROC_NOT_FOUND;break;}
  stage=mark("Native owned window providers");
  if(!reg||!create||!get||!release||!destroy||!unreg||!originalDefault||!instance){error=ERROR_PROC_NOT_FOUND;break;}
  OwnedClass cls={sizeof(OwnedClass),0x20,OwnedWindow,0,0,instance,nullptr,nullptr,nullptr,nullptr,L"HollowwakeOwnedWgl",nullptr};if(!reg(&cls)){error=GetLastError();break;}hwnd=create(0,cls.name,L"Owned WGL bridge fixture",0x80000000,0,0,64,64,nullptr,nullptr,instance,nullptr);if(!hwnd){error=GetLastError();break;}dc=get(hwnd);if(!dc){error=GetLastError();break;}
  stage=mark("Packaged Mesa provider and native-to-CoreWindow HDC");
  auto factory=reinterpret_cast<HGLRC(WINAPI*)(HDC)>(ResolveWglProviderImport("opengl32.dll","wglCreateContext",true));make=reinterpret_cast<Make>(ResolveWglProviderImport("opengl32.dll","wglMakeCurrent",true));erase=reinterpret_cast<Delete>(ResolveWglProviderImport("opengl32.dll","wglDeleteContext",true));auto current=reinterpret_cast<HGLRC(WINAPI*)()>(ResolveWglProviderImport("opengl32.dll","wglGetCurrentContext",true));auto currentDc=reinterpret_cast<HDC(WINAPI*)()>(ResolveWglProviderImport("opengl32.dll","wglGetCurrentDC",true));auto proc=reinterpret_cast<PROC(WINAPI*)(LPCSTR)>(ResolveWglProviderImport("opengl32.dll","wglGetProcAddress",true));if(!factory||!make||!erase||!current||!currentDc||!proc){error=GetLastError();break;}
  stage=mark("Mesa wglCreateContext");context=factory(dc);if(!context){error=GetLastError();break;}stage=mark("Mesa wglMakeCurrent");if(!make(dc,context)){error=GetLastError();break;}currentVerified=current()==context&&currentDc()==dc;if(!currentVerified){error=ERROR_INVALID_STATE;break;}
  stage=mark("Actual Mesa D3D12 OpenGL clear and GPU pixel readback");
  auto clearColor=reinterpret_cast<void(APIENTRY*)(float,float,float,float)>(proc("glClearColor"));auto clear=reinterpret_cast<void(APIENTRY*)(unsigned)>(proc("glClear"));auto read=reinterpret_cast<void(APIENTRY*)(int,int,int,int,unsigned,unsigned,void*)>(proc("glReadPixels"));auto glError=reinterpret_cast<unsigned(APIENTRY*)()>(proc("glGetError"));auto finish=reinterpret_cast<void(APIENTRY*)()>(proc("glFinish"));if(!clearColor||!clear||!read||!glError||!finish){error=ERROR_PROC_NOT_FOUND;break;}
  clearColor(.25f,.5f,.75f,1);clear(0x4000);finish();unsigned char rgba[4]={};read(0,0,1,1,0x1908,0x1401,rgba);pixel=glError()==0&&rgba[0]>=62&&rgba[0]<=66&&rgba[1]>=126&&rgba[1]<=130&&rgba[2]>=189&&rgba[2]<=193&&rgba[3]==255;if(!pixel){error=ERROR_INVALID_DATA;break;}
  stage=mark("Actual composition swap present");auto swap=reinterpret_cast<BOOL(WINAPI*)(HDC)>(ResolveWglProviderImport("gdi32.dll","SwapBuffers",true));present=swap&&swap(dc);if(!present){error=GetLastError();break;}
  if(!make(nullptr,nullptr)||current()!=nullptr||currentDc()!=nullptr){error=ERROR_INVALID_STATE;break;}if(!erase(context)){error=GetLastError();break;}context=nullptr;result=S_OK;stage=mark("Six WGL calls, real pixel readback and composition present complete");
 }while(false);
 if(context){if(make)make(nullptr,nullptr);if(erase)erase(context);}if(dc&&release)release(hwnd,dc);if(hwnd&&destroy)destroy(hwnd);if(unreg&&instance)unreg(L"HollowwakeOwnedWgl",instance);if(FAILED(result)&&error)result=HRESULT_FROM_WIN32(error);
 std::ofstream out(report,std::ios::trunc);out<<"{\"scope\":\"Owned actual WGL bridge behavior, not game/Xbox\",\"hresult\":"<<(long)result<<",\"stage\":\""<<stage<<"\",\"error\":"<<error<<",\"currentContextAndGuestDc\":"<<(currentVerified?"true":"false")<<",\"pixelReadback\":"<<(pixel?"true":"false")<<",\"compositionPresent\":"<<(present?"true":"false")<<",\"gameExecuted\":false,\"xboxTested\":false}";return result;
}
