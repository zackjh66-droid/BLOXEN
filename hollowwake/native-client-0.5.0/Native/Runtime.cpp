#include "ClientLaunchIdentity.h"
#include "CrashLocation.h"
#include "LegacyCrt.h"
#include "LegacyStartup.h"
#include "KernelStartup.h"
#include "KernelClock.h"
#include "SystemInfo.h"
// Original bounded loading stages for the exact inspected 6.21 main executable.
// Guest execution is explicit; unsupported paths terminate with a named log.
#include <windows.h>
#include <bcrypt.h>
#include <roerrorapi.h>
#include <agile.h>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <stdexcept>
#include <cstring>
#include <future>
#include <chrono>
#include <atomic>
#include <windows.ui.xaml.media.dxinterop.h>
#include <wrl/client.h>
#include "Dependencies.h"
#include "ModuleLoader.h"
#include "NetworkBridge.h"
#include "ReadOnlyPlatform.h"
#include "NativeWindowProvider.h"
#include "LdapProvider.h"
#include "WebProvider.h"
#include "DataFiles.h"
#include "StartupImport.h"
#include "NativeClientThread.h"
#include "PlatformAudit.h"
#include "StartupPlatform.h"
#include "DesktopCapabilities.h"
#include "WglProvider.h"
#include "ProviderAudit.h"
#include <utility>
#include "PlatformImports.generated.h"
#include "../../runtime-native-route/NativeSystem.h"
#include "../../runtime-native-route/NativeProxyDispatch.h"
#include "PackagedNamespace.h"
#include "ClientDependencyImports.generated.h"
#include "../../runtime-native-route/GuestThreads.h"
#include "Unsupported.h"
#include "../../client-runtime/graphics/CoreWindowBridge.h"
#include "../../client-runtime/graphics/WindowedSwapChain.h"
#include "../../client-runtime/graphics/NativeFactoryBridge.h"
static std::mutex logMutex;static std::wstring logName;static BYTE* mapped=nullptr;static SIZE_T mappedSize=0;static bool consumed=false,guestStarted=false;
std::string DescribeGuestAddress(unsigned long long address){if(mapped&&address>=(ULONGLONG)mapped&&address-(ULONGLONG)mapped<mappedSize)return "FortniteClient-Win64-Shipping.exe RVA="+std::to_string(address-(ULONGLONG)mapped);return DescribeDependencyAddress(address);}
static void Log(const std::string& text){std::lock_guard<std::mutex> g(logMutex);std::ofstream f(logName,std::ios::app);f<<text<<"\n";f.flush();}
static std::atomic<bool> testingGraphics{false};static std::atomic<unsigned> compositionSerial{0};
unsigned ReadCompositionAttachmentSerial(){return compositionSerial.load();}
bool SetOwnedGraphicsProbe(bool value){return testingGraphics.exchange(value);}
static Microsoft::WRL::ComPtr<ISwapChainPanelNative> gamePanel;
static Platform::Agile<Windows::UI::Core::CoreDispatcher> gameDispatcher;
static Microsoft::WRL::ComPtr<IUnknown> boundCoreWindow;
static HRESULT AttachOnUi(IDXGISwapChain* value){auto hr=gamePanel->SetSwapChain(value);if(SUCCEEDED(hr))++compositionSerial;if(SUCCEEDED(hr)&&!testingGraphics){auto frame=dynamic_cast<Windows::UI::Xaml::Controls::Frame^>(Windows::UI::Xaml::Window::Current->Content);auto page=frame?dynamic_cast<Windows::UI::Xaml::FrameworkElement^>(frame->Content):nullptr;auto overlay=page?dynamic_cast<Windows::UI::Xaml::FrameworkElement^>(page->FindName(L"HostOverlay")):nullptr;if(overlay)overlay->Visibility=Windows::UI::Xaml::Visibility::Collapsed;}return hr;}
static HRESULT AttachComposition(IDXGISwapChain* value){
 auto dispatcher=gameDispatcher.Get();if(!dispatcher||!gamePanel)return E_UNEXPECTED;if(dispatcher->HasThreadAccess)return AttachOnUi(value);
 struct Pending{std::promise<HRESULT> result;std::atomic<bool> active{true};};auto pending=std::make_shared<Pending>();auto future=pending->result.get_future();Microsoft::WRL::ComPtr<IDXGISwapChain> chain=value;
 try{dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,ref new Windows::UI::Core::DispatchedHandler([pending,chain](){pending->result.set_value(pending->active.load()?AttachOnUi(chain.Get()):E_ABORT);}));}catch(Platform::Exception^ e){return e->HResult;}
 if(future.wait_for(std::chrono::seconds(5))!=std::future_status::ready){pending->active=false;return HRESULT_FROM_WIN32(WAIT_TIMEOUT);}return future.get();
}
static void Fail(const char* message){throw std::runtime_error(message);}
static bool Range(SIZE_T start,SIZE_T count,SIZE_T size){return start<=size&&count<=size-start;}
static BYTE* Rva(DWORD address,SIZE_T count){if(!Range(address,count,mappedSize))Fail("PE RVA outside image");return mapped+address;}
static const char* Name(DWORD address){auto p=(const char*)Rva(address,1);SIZE_T max=mappedSize-address;if(max>4096)max=4096;if(!memchr(p,0,max))Fail("Unterminated PE name");return p;}
static bool HashPinned(const std::vector<BYTE>& raw,const BYTE* expected){
 BCRYPT_ALG_HANDLE alg=nullptr;BYTE result[32];if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;auto s=BCryptHash(alg,nullptr,0,const_cast<BYTE*>(raw.data()),(ULONG)raw.size(),result,32);BCryptCloseAlgorithmProvider(alg,0);return s>=0&&!memcmp(expected,result,32);
}
static bool Hash(const std::vector<BYTE>& raw){
 static const BYTE expected[32]={0x9d,0x18,0xfb,0x5c,0x5c,0x43,0xdd,0xc2,0x12,0x51,0xd5,0xce,0x9e,0x02,0x6c,0xa0,0x1f,0x90,0xd4,0x29,0x21,0xdd,0x74,0x19,0x73,0xbd,0x55,0x36,0xe8,0x94,0x94,0x78};
 return HashPinned(raw,expected);
}
void* ResolveControllerImport(const char*,const char*);
void* ResolveWindowImport(const char*,const char*);
extern "C" void __cdecl HWViewport(int,int);
extern "C" HRESULT __cdecl HWReadViewport(UINT*,UINT*);
extern "C" void __cdecl HWConfigurePointer(BOOL(__cdecl*)(int,int),BOOL(__cdecl*)(int));
extern "C" void __cdecl HWConfigureCursor(BOOL(__cdecl*)(UINT,int));
static bool explicitCursor=false;
static Platform::Agile<Windows::UI::Core::CoreCursor> savedPointer;
extern "C" __declspec(dllexport) HRESULT __cdecl HWAttachMesaSwapChain(IUnknown* value){if(!value)return E_POINTER;Microsoft::WRL::ComPtr<IDXGISwapChain> chain;auto hr=value->QueryInterface(IID_PPV_ARGS(&chain));return FAILED(hr)?hr:AttachComposition(chain.Get());}
static Platform::Agile<Windows::UI::Core::CoreWindow> pointerCore;
static HRESULT PointerOnUi(int operation,int x,int y){
 auto core=pointerCore.Get();
 if(!core)return E_UNEXPECTED;
 try{if(operation==2){
  using Windows::UI::Core::CoreCursorType;CoreCursorType type;
  switch(x){case 0:savedPointer=nullptr;explicitCursor=true;core->PointerCursor=nullptr;return S_OK;
   case 32512:type=CoreCursorType::Arrow;break;case 32513:type=CoreCursorType::IBeam;break;case 32514:type=CoreCursorType::Wait;break;case 32515:type=CoreCursorType::Cross;break;case 32516:type=CoreCursorType::UpArrow;break;
   case 32642:type=CoreCursorType::SizeNorthwestSoutheast;break;case 32643:type=CoreCursorType::SizeNortheastSouthwest;break;case 32644:type=CoreCursorType::SizeWestEast;break;case 32645:type=CoreCursorType::SizeNorthSouth;break;case 32646:type=CoreCursorType::SizeAll;break;
   case 32648:type=CoreCursorType::UniversalNo;break;case 32649:type=CoreCursorType::Hand;break;case 32651:type=CoreCursorType::Help;break;default:return E_INVALIDARG;}
  auto next=ref new Windows::UI::Core::CoreCursor(type,0);core->PointerCursor=y?next:nullptr;savedPointer=next;explicitCursor=true;return S_OK;
 }
 if(operation==1){if(x){core->PointerCursor=explicitCursor?savedPointer.Get():(savedPointer.Get()?savedPointer.Get():ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::Arrow,0));}else{if(core->PointerCursor)savedPointer=core->PointerCursor;core->PointerCursor=nullptr;}}
 else core->PointerPosition=Windows::Foundation::Point((float)x,(float)y);return S_OK;}catch(Platform::Exception^ e){return e->HResult;}
}
static BOOL PointerOperation(int operation,int x,int y){
 auto dispatcher=gameDispatcher.Get();if(!dispatcher){SetLastError(ERROR_NOT_READY);return FALSE;}
 HRESULT hr;
 if(dispatcher->HasThreadAccess)hr=PointerOnUi(operation,x,y);
 else{struct Request{std::promise<HRESULT> result;std::atomic<bool> active{true};};auto request=std::make_shared<Request>();auto result=request->result.get_future();
  try{dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,ref new Windows::UI::Core::DispatchedHandler([request,operation,x,y](){request->result.set_value(request->active.load()?PointerOnUi(operation,x,y):E_ABORT);}));}catch(Platform::Exception^ e){SetLastError(HRESULT_CODE(e->HResult));return FALSE;}
  if(result.wait_for(std::chrono::seconds(3))!=std::future_status::ready){request->active=false;SetLastError(WAIT_TIMEOUT);return FALSE;}hr=result.get();
 }
 if(FAILED(hr)){SetLastError(HRESULT_CODE(hr));return FALSE;}return TRUE;
}
static BOOL __cdecl WarpHostPointer(int x,int y){return PointerOperation(0,x,y);}
static BOOL __cdecl ShowHostPointer(int show){return PointerOperation(1,show,0);}
static BOOL __cdecl SelectHostCursor(UINT shape,int visible){return PointerOperation(2,(int)shape,visible);}


extern "C" __declspec(dllexport) int __cdecl HWFactoryBridgePolicy(){
 Microsoft::WRL::ComPtr<ID3D11Device> device;Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
 auto hr=::D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
 if(FAILED(hr)){Log("GRAPHICS_POLICY hardware creation HRESULT="+std::to_string((unsigned long)hr)+"; trying actual WARP device");hr=::D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);}
 if(FAILED(hr)){Log("GRAPHICS_POLICY device creation failed HRESULT="+std::to_string((unsigned long)hr));return hr;}
 hr=Hollowwake::BridgeDeviceFactories(device.Get());Log("GRAPHICS_POLICY native factory dispatch installation HRESULT="+std::to_string((unsigned long)hr)+"; real AppContainer operation, no game execution");
 auto restored=Hollowwake::RemoveNativeFactoryBridgeForTest();Log("GRAPHICS_POLICY dispatch restore HRESULT="+std::to_string((unsigned long)restored));return FAILED(hr)?hr:restored;
}
extern "C" __declspec(dllexport) int __cdecl HWStockCursorContract(){
 if(!gameDispatcher.Get()||!gameDispatcher.Get()->HasThreadAccess)return -1;
 using Load=HCURSOR(WINAPI*)(HINSTANCE,LPCWSTR);using Set=HCURSOR(WINAPI*)(HCURSOR);
 auto load=reinterpret_cast<Load>(ResolveWindowImport("user32.dll","LoadCursorW"));auto set=reinterpret_cast<Set>(ResolveWindowImport("user32.dll","SetCursor"));auto core=pointerCore.Get();
 if(!load||!set||!core)return -2;auto hand=load(nullptr,(LPCWSTR)32649),arrow=load(nullptr,(LPCWSTR)32512);if(!hand||!arrow)return -3;
 set(hand);if(!core->PointerCursor||core->PointerCursor->Type!=Windows::UI::Core::CoreCursorType::Hand)return -4;
 set(nullptr);if(core->PointerCursor)return -5;set(arrow);if(!core->PointerCursor||core->PointerCursor->Type!=Windows::UI::Core::CoreCursorType::Arrow)return -6;
 Log("PASS real CoreWindow cursor: resolved LoadCursorW/SetCursor -> Hand -> null -> Arrow");return 621;
}
static void* QueryBuiltinImport(const char* dll,const char* name){
 if(auto p=ResolveModuleLoaderImport(dll,name))return p;
 if(auto p=Hollowwake::ResolveGuestThreadImport(dll,name))return p;
 if(!_stricmp(dll,"msvcrt.dll")&&Hollowwake::NativeLegacyCrtAvailable())return Hollowwake::ResolveNativeSystemImport(dll,name);
 if(auto p=Hollowwake::ResolveLegacyStartupImport(dll,name))return p;
 if(auto p=Hollowwake::ResolveKernelStartupImport(dll,name))return p;
 if(auto p=Hollowwake::ResolveLegacyCrtImport(dll,name))return p;
 if(auto p=Hollowwake::ResolveSystemInfoImport(dll,name))return p;
 if(auto p=Hollowwake::ResolveClockImport(dll,name))return p;
 if(auto p=Hollowwake::ResolveGraphicsImport(dll,name))return p;
 if(auto p=ResolveWindowImport(dll,name))return p;
 if(auto p=ResolveControllerImport(dll,name))return p;
 if(auto p=Hollowwake::ResolveNativeSystemImport(dll,name))return p;
 if(_stricmp(dll,"KERNEL32.dll")&&_stricmp(dll,"KERNELBASE.dll")&&_strnicmp(dll,"api-ms-win-core-",16)){return nullptr;}
 #define REAL(api) if(!strcmp(name,#api))return reinterpret_cast<void*>(&api)
 REAL(InitializeCriticalSectionEx);REAL(EnterCriticalSection);REAL(TryEnterCriticalSection);REAL(LeaveCriticalSection);REAL(DeleteCriticalSection);REAL(EncodePointer);REAL(DecodePointer);REAL(FlsAlloc);REAL(FlsGetValue);REAL(FlsSetValue);REAL(FlsFree);REAL(TlsAlloc);REAL(TlsGetValue);REAL(TlsSetValue);REAL(TlsFree);REAL(GetCurrentThread);REAL(IsProcessorFeaturePresent);REAL(GetSystemTimeAsFileTime);REAL(GetProcessHeap);REAL(HeapAlloc);REAL(HeapFree);REAL(HeapReAlloc);REAL(HeapSize);REAL(GetCurrentThreadId);REAL(GetCurrentProcessId);REAL(QueryPerformanceCounter);REAL(QueryPerformanceFrequency);REAL(GetLastError);REAL(SetLastError);REAL(GetTickCount64);REAL(GetCurrentProcess);
 #undef REAL
 return nullptr;
}
static void* QueryImport(const char* dll,const char* name){if(auto p=QueryBuiltinImport(dll,name))return p;return QueryDependencyExport(dll,name);}
static void* Import(const char* dll,const char* name){if(auto p=QueryBuiltinImport(dll,name))return p;if(auto p=ResolveDependency(dll,name))return p;return UnsupportedImport(dll,name);}
static unsigned imports=0,missing=0;
static void Thunks(const char* dll,DWORD source,DWORD target){
 for(SIZE_T i=0;i<32768;++i){ULONGLONG value=*(ULONGLONG*)Rva(source+(DWORD)(i*8),8);if(!value)return;auto out=(ULONGLONG*)Rva(target+(DWORD)(i*8),8);void* p=nullptr;std::string symbol;
  if(value&IMAGE_ORDINAL_FLAG64)symbol="#"+std::to_string(value&0xffff);
  else {if(value>MAXDWORD)Fail("Invalid name thunk");Rva((DWORD)value,3);symbol=Name((DWORD)value+2);}
  p=Import(dll,symbol.c_str());
  ++imports;if(p)*out=(ULONGLONG)p;else {++missing;if(missing<=256)Log("UNRESOLVED "+std::string(dll)+"!"+symbol);}
 }Fail("Unbounded import thunk list");
}
// Native DLL route must not reuse manual-map TLS/thread notifications.
static void* PackagedRoute(const char* dll,const char* name){
 if(IsStartupPlatformModule(dll))return ResolveStartupPlatformImport(dll,name);
 if(auto p=ResolveDataFileImport(dll,name))return p;
 if(auto p=ResolveNetworkImport(dll,name))return p;
 if(auto p=ResolveLdapImport(dll,name))return p;
 if(auto p=ResolveWebImport(dll,name))return p;
 // Native-loader route: all USER32 operations use genuine OS handles. Keep
 // file-resource decoding inside the game-data namespace; never mix virtual
 // WindowBridge handles with this provider if an export is absent.
 if(!_stricmp(dll,"USER32.dll")){
  if(!strcmp(name,"LoadImageW")||!strcmp(name,"LoadCursorFromFileW"))return ResolveReadOnlyPlatformImport(dll,name);
  return ResolveNativeWindowImport(dll,name);
 }
 if(!_stricmp(dll,"dxgi.dll")||!_stricmp(dll,"d3d11.dll"))return ResolveNativePresentationImport(dll,name);
 if(auto p=ResolveWglProviderImport(dll,name))return p;
 if(auto p=ResolveReadOnlyPlatformImport(dll,name))return p;
 if(auto p=Hollowwake::ResolveGraphicsImport(dll,name))return p;
 if(auto p=ResolveWindowImport(dll,name))return p;
 if(auto p=ResolveControllerImport(dll,name))return p;
 return nullptr; // Network/login and additional platform adapters remain mandatory.
}
static void MissingPackagedImport(const char* dll,const char* name){Log("NATIVE_PROXY_BLOCKED "+std::string(dll)+"!"+name);std::lock_guard<std::mutex> guard(logMutex);auto slash=logName.find_last_of(L"/\\");if(slash!=std::wstring::npos){std::ofstream output(logName.substr(0,slash+1)+L"native-imports-unresolved.txt",std::ios::app);output<<dll<<"!"<<name<<"\n";output.flush();}}
extern "C" __declspec(dllexport) int __cdecl HWBindWindow(const wchar_t* path){
 logName=path;ConfigureDesktopCapabilityLogger([](const char* module,const char* symbol,DWORD error){Log("NATIVE_CAPABILITY_UNAVAILABLE "+std::string(module)+"!"+symbol+" Win32="+std::to_string(error)+"; original caller receives failure, feature not supported");});auto packageDirectory=Windows::ApplicationModel::Package::Current->InstalledLocation->Path;auto appData=Windows::Storage::ApplicationData::Current->LocalFolder->Path;std::wstring gameData=std::wstring(appData->Data())+L"\\GameClean";if(!ConfigureDataFiles(packageDirectory->Data(),gameData.c_str()))return E_INVALIDARG;Hollowwake::ConfigurePackagedProxyResolver(PackagedRoute,MissingPackagedImport);ConfigureUnsupported(Log);Hollowwake::LegacyFatalLogger()=[](const char* message){Log(std::string("CRASH ")+message);};Hollowwake::SystemInfoFailureHandler()=[](DWORD error){Log("CRASH GetVersionExW failed error="+std::to_string(error));};auto w=Windows::UI::Core::CoreWindow::GetForCurrentThread();if(!w)return (int)RPC_E_WRONG_THREAD;
 HWViewport((int)w->Bounds.Width,(int)w->Bounds.Height);
 auto frame=dynamic_cast<Windows::UI::Xaml::Controls::Frame^>(Windows::UI::Xaml::Window::Current->Content);auto page=frame?dynamic_cast<Windows::UI::Xaml::FrameworkElement^>(frame->Content):nullptr;auto panel=page?dynamic_cast<Windows::UI::Xaml::Controls::SwapChainPanel^>(page->FindName(L"GameSurface")):nullptr;
 if(!panel)return E_NOINTERFACE;auto hr=reinterpret_cast<IUnknown*>(panel)->QueryInterface(IID_PPV_ARGS(&gamePanel));if(FAILED(hr))return hr;gameDispatcher=panel->Dispatcher;pointerCore=w;HWConfigurePointer(&WarpHostPointer,&ShowHostPointer);HWConfigureCursor(&SelectHostCursor);boundCoreWindow=reinterpret_cast<IUnknown*>(w);Hollowwake::SetCompositionAttachment(&AttachComposition);Hollowwake::SetChainViewportProvider(&HWReadViewport);ConfigureNativeWindowLogger([](const char* api,DWORD error){Log(std::string("NATIVE_WINDOW_UNAVAILABLE ")+api+" Win32="+std::to_string(error));});Hollowwake::ConfigureNativeFactoryBridge(boundCoreWindow.Get(),&IsNativeClientWindow);
 ConfigureWglProviderLogger([](const char* stage,DWORD error){Log(std::string("WGL_PROVIDER ")+stage+" code="+std::to_string(error));});
 ConfigureWglProvider(boundCoreWindow.Get(),[](LONG* width,LONG* height)->BOOL{UINT w=0,h=0;auto result=HWReadViewport(&w,&h);if(FAILED(result))return FALSE;*width=(LONG)w;*height=(LONG)h;return TRUE;});
 return Hollowwake::BindGraphicsWindow(boundCoreWindow.Get());
}
extern "C" __declspec(dllexport) int __cdecl HWPreflightClient(){
 Hollowwake::ConfigurePackagedProxyResolver(PackagedRoute,MissingPackagedImport);
 Log("NATIVE_ROUTE real OS USER32 windows and native HWND DXGI/D3D11; file/module/network policies retained; Windows fixtures passed, Xbox unverified");
 {auto slash=logName.find_last_of(L"/\\");if(slash!=std::wstring::npos){std::ofstream reset(logName.substr(0,slash+1)+L"native-imports-unresolved.txt",std::ios::trunc);}}
 unsigned unavailable=0;for(auto& item:clientDependencyImports)if(!HWResolvePackagedImport(item.dll,item.symbol))++unavailable;
 Log("NATIVE_PREFLIGHT total="+std::to_string(sizeof(clientDependencyImports)/sizeof(clientDependencyImports[0]))+" unresolved="+std::to_string(unavailable));
 auto slash=logName.find_last_of(L"/\\");if(slash==std::wstring::npos){Log("NATIVE_PLATFORM_AUDIT_FAILED no log directory");return E_UNEXPECTED;}
 unsigned requiredPlatform=(unsigned)-1;auto platform=AuditPlatformImports((logName.substr(0,slash+1)+L"native-platform-imports.json").c_str(),&requiredPlatform);
 Log("NATIVE_PLATFORM_PREFLIGHT total="+std::to_string(sizeof(platformImportPairs)/sizeof(platformImportPairs[0]))+" unresolved="+std::to_string(platform)+"; export presence is not behavior or Xbox validation");
 // Normal dependencies must resolve before native loading. Delayed imports
 // follow the original Windows delay-load helper: they fail on actual request.
 // Their exact missing names/counts remain in the unchanged 838-pair audit.
 Log("NATIVE_PLATFORM_LOAD_TIME_REQUIRED unresolved="+std::to_string(requiredPlatform)+"; delayed unavailable APIs remain reported, not implemented");
 unsigned additionalMissing=0;auto packageRoot=Windows::ApplicationModel::Package::Current->InstalledLocation->Path;std::wstring installed(packageRoot->Data());
 if(GetFileAttributesW((installed+L"\\native-package-manifest.json").c_str())!=INVALID_FILE_ATTRIBUTES){additionalMissing=AuditAdditionalProviderImports((installed+L"\\provider-imports.json").c_str(),(logName.substr(0,slash+1)+L"native-provider-imports.json").c_str());Log("NATIVE_PROVIDER_PREFLIGHT unresolved="+std::to_string(additionalMissing)+"; additional licensed-provider inventory, original platform baseline unchanged");}
 return unavailable||requiredPlatform||additionalMissing?HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND):S_OK;
}

static Hollowwake::CrashModuleLookup crashLookup=nullptr;
static Hollowwake::CrashModuleFilename crashFilename=nullptr;
static LONG CALLBACK NativeCrashRecord(EXCEPTION_POINTERS* data){static std::atomic_flag writing=ATOMIC_FLAG_INIT;if(!data||!data->ExceptionRecord)return EXCEPTION_CONTINUE_SEARCH;auto code=data->ExceptionRecord->ExceptionCode;if(code!=0xC0000005&&code!=0xC000001D&&code!=0xC00000FD)return EXCEPTION_CONTINUE_SEARCH;if(!writing.test_and_set()){try{Log(Hollowwake::DescribeCrash(*data->ExceptionRecord,crashLookup,crashFilename)+"; exception continues to OS");}catch(...){}writing.clear();}return EXCEPTION_CONTINUE_SEARCH;}

// Explicit native-loader experiment. It accepts no download URL, credentials,
// arbitrary DLL path or user command line. The UI's existing mapper is unchanged.
static Hollowwake::ClientLaunchIdentity gameIdentity;
extern "C" __declspec(dllexport) int __cdecl HWConfigureClientLogin(const wchar_t* code,unsigned seconds){
 if(!code||wcsnlen_s(code,44)!=43)return E_INVALIDARG;
 return gameIdentity.Configure(std::wstring(code),seconds,GetTickCount64())?S_OK:E_ACCESSDENIED;
}
extern "C" __declspec(dllexport) int __cdecl HWAttemptPackagedClient(){
 try{
  if(!boundCoreWindow||!gamePanel){Log("NATIVE_LOAD_BLOCKED graphics window not bound");return E_UNEXPECTED;}
  static std::atomic<bool> attempted{false};if(attempted.exchange(true))return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
  Log("ATTEMPT_BEGIN runtime=0.6.0.0 native-packaged route");
  {auto slash=logName.find_last_of(L"/\\");if(slash!=std::wstring::npos){std::ofstream reset(logName.substr(0,slash+1)+L"native-imports-unresolved.txt",std::ios::trunc);}}
  auto preflight=HWPreflightClient();if(FAILED(preflight))return preflight;
  auto location=Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
  std::wstring file=std::wstring(location->Data())+L"\\6.21\\FortniteGame\\Binaries\\Win64\\Fortnite621.imports.private.dll";
  std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in){Log("NATIVE_LOAD_BLOCKED verified client DLL is not in installed package; LocalState import is not a package dependency");return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);}
  auto size=in.tellg();if(size!=100733440){Log("NATIVE_LOAD_BLOCKED private client size mismatch");return E_INVALIDARG;}in.seekg(0);std::vector<BYTE> bytes(static_cast<SIZE_T>(size));if(!in.read(reinterpret_cast<char*>(bytes.data()),size))return E_FAIL;
  static const BYTE expected[32]={0x85,0x48,0xe7,0xed,0xc2,0x21,0xa4,0x8d,0x12,0x64,0x8c,0x98,0x50,0x00,0x92,0xbf,0xeb,0x3c,0x0c,0x6d,0x0e,0x4e,0x69,0xd6,0x6e,0x41,0x7a,0x24,0x5c,0x20,0x15,0xaf};
  if(!HashPinned(bytes,expected)){Log("NATIVE_LOAD_BLOCKED converted client SHA256 mismatch");return E_INVALIDARG;}auto clientDos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
  if(clientDos->e_magic!=IMAGE_DOS_SIGNATURE||clientDos->e_lfanew<64||!Range(clientDos->e_lfanew,sizeof(IMAGE_NT_HEADERS64),bytes.size()))return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
  auto clientHeaders=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data()+clientDos->e_lfanew);
  auto stackReserve=clientHeaders->OptionalHeader.SizeOfStackReserve;
  if(clientHeaders->Signature!=IMAGE_NT_SIGNATURE||clientHeaders->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||stackReserve!=1048576||clientHeaders->OptionalHeader.SizeOfStackCommit!=4096)return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
  bytes.clear();
  UINT launchWidth=0,launchHeight=0;auto sizeResult=HWReadViewport(&launchWidth,&launchHeight);if(FAILED(sizeResult))return sizeResult;
  std::wstring launchCommand=L"FortniteClient-Win64-Shipping.exe -windowed -d3d11 -ResX="+std::to_wstring(launchWidth)+L" -ResY="+std::to_wstring(launchHeight);
  std::wstring gameLogin;if(!gameIdentity.Consume(GetTickCount64(),gameLogin)){Log("NATIVE_LOAD_BLOCKED a fresh linked-account launch ticket is required");return E_ACCESSDENIED;}launchCommand+=gameLogin;
  if(!Hollowwake::ConfigurePackagedNamespace(L"Fortnite621.imports.private.dll",launchCommand.c_str())){Log("NATIVE_LOAD_BLOCKED namespace configuration refused");return E_UNEXPECTED;}
  Log("NATIVE_STARTUP windowed D3D11 requested at actual virtual viewport; one-time game exchange ticket configured; no project session supplied");
  crashLookup=reinterpret_cast<Hollowwake::CrashModuleLookup>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));
  crashFilename=reinterpret_cast<Hollowwake::CrashModuleFilename>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleFileNameW"));
  using AddHandler=PVOID(WINAPI*)(ULONG,PVECTORED_EXCEPTION_HANDLER);auto addHandler=reinterpret_cast<AddHandler>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","AddVectoredExceptionHandler"));if(!addHandler||!addHandler(1,NativeCrashRecord))Log("NATIVE_DIAGNOSTIC vectored crash recorder unavailable");
  Log("NATIVE_LOAD calling real LoadPackagedLibrary; dependency TLS/DllMain can execute here");
  std::wstring manifestPath=std::wstring(location->Data())+L"\\native-package-manifest.json";std::ifstream metadata(manifestPath,std::ios::binary);if(!metadata){Log("NATIVE_LOAD_BLOCKED private dependency manifest missing");return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);}
  std::string text((std::istreambuf_iterator<char>(metadata)),std::istreambuf_iterator<char>());if(text.size()>1024*1024)return E_INVALIDARG;std::wstring wide(text.begin(),text.end());auto manifest=Windows::Data::Json::JsonObject::Parse(ref new Platform::String(wide.c_str()));auto files=manifest->GetNamedArray(L"files");if(files->Size>200)return E_INVALIDARG;
  if(!Hollowwake::ConfigurePackagedRoot(location->Data()))return E_INVALIDARG;
  for(unsigned fileIndex=0;fileIndex<files->Size;++fileIndex){auto item=files->GetAt(fileIndex)->GetObject();auto relative=item->GetNamedString(L"packageRelativePath");std::wstring path(relative->Data());if(path.empty()||path[0]==L'/'||path[0]==L'\\'||path.find(L':')!=std::wstring::npos||path.find(L"..")!=std::wstring::npos)return E_INVALIDARG;auto slash=path.find_last_of(L"/\\");auto name=path.substr(slash==std::wstring::npos?0:slash+1);std::wstring disk=std::wstring(location->Data())+L"\\"+path;std::ifstream dependency(disk,std::ios::binary|std::ios::ate);if(!dependency)return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);auto length=dependency.tellg();if(length<1024||length>512*1024*1024)return E_INVALIDARG;dependency.seekg(0);std::vector<BYTE> image((SIZE_T)length);if(!dependency.read((char*)image.data(),length))return E_FAIL;auto hash=item->GetNamedString(L"sha256");if(hash->Length()!=64)return E_INVALIDARG;BYTE expectedDependency[32];for(unsigned i=0;i<32;++i){auto nibble=[](wchar_t c)->int{return c>=L'0'&&c<=L'9'?c-L'0':c>=L'a'&&c<=L'f'?c-L'a'+10:-1;};int a=nibble(hash->Data()[i*2]),b=nibble(hash->Data()[i*2+1]);if(a<0||b<0)return E_INVALIDARG;expectedDependency[i]=(BYTE)(a*16+b);}if(!HashPinned(image,expectedDependency)){Log("NATIVE_LOAD_BLOCKED packaged dependency hash mismatch");return E_INVALIDARG;}if(!RegisterCodeFileAlias(item->GetNamedString(L"originalRelativePath")->Data(),path.c_str()))return E_INVALIDARG;if(item->GetNamedBoolean(L"mainImage")&&!Hollowwake::RegisterPackagedMainIdentity(item->GetNamedString(L"originalRelativePath")->Data()))return E_INVALIDARG;if(!item->GetNamedBoolean(L"mainImage")&&!Hollowwake::RegisterPackagedDependency(name.c_str(),path.c_str(),item->GetNamedString(L"originalRelativePath")->Data()))return E_INVALIDARG;}
  if(!Hollowwake::RegisterPackagedDependency(L"dsound.dll",L"HWAUD.dll",L"6.21/FortniteGame/Binaries/Win64/dsound.dll"))return E_INVALIDARG;
  for(auto pair: {std::make_pair(L"imm32.dll",L"HWIM.dll"),std::make_pair(L"opengl32.dll",L"HWGL.dll"),std::make_pair(L"dwmapi.dll",L"HWDWM.dll"),std::make_pair(L"d3d9.dll",L"HWD9.dll")}){auto logical=std::wstring(L"6.21/FortniteGame/Binaries/Win64/")+pair.first;if(!Hollowwake::RegisterPackagedDependency(pair.first,pair.second,logical.c_str()))return E_INVALIDARG;}
  auto module=LoadPackagedLibrary(L"6.21\\FortniteGame\\Binaries\\Win64\\Fortnite621.imports.private.dll",0);if(!module){auto error=GetLastError();Log("NATIVE_LOAD_FAILED Win32="+std::to_string(error));return HRESULT_FROM_WIN32(error);}
  if(!BindWinMainCommandImport(module,"api-ms-win-crt-runtime-l1-1-0.dll",reinterpret_cast<void*>(&Hollowwake::PackagedWinMainTail))){auto error=GetLastError();Log("NATIVE_STARTUP_FAILED _get_narrow_winmain_command_line binding Win32="+std::to_string(error));return HRESULT_FROM_WIN32(error);}
  Log("NATIVE_STARTUP_BOUND image-local WinMain command import; host CRT globals unchanged");
  auto entry=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module,"StartHollowwakeClient"));if(!entry){Log("NATIVE_LOAD_BLOCKED preserved client-entry export missing");return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);}
  int result=0;auto started=RunNativeClientThread(entry,static_cast<SIZE_T>(stackReserve),&result,Log);if(FAILED(started)){Log("NATIVE_THREAD_FAILED HRESULT="+std::to_string((unsigned long)started));return started;}return result;
 }catch(Platform::Exception^ e){Log("NATIVE_LOAD_EXCEPTION HRESULT="+std::to_string(e->HResult));return e->HResult;}
 catch(const std::exception& e){Log(std::string("NATIVE_LOAD_EXCEPTION ")+e.what());return E_FAIL;}
}
static int MemoryPolicy(const wchar_t* path){
 logName=path;Log("POLICY allocate RW -> protect RX -> execute ORIGINAL return-621 fixture");
 BYTE* p=(BYTE*)VirtualAllocFromApp(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!p){Log("POLICY allocation error="+std::to_string(GetLastError()));return -1;}
 const BYTE code[]={0xb8,0x6d,0x02,0x00,0x00,0xc3};memcpy(p,code,sizeof(code));ULONG old=0;
 if(!VirtualProtectFromApp(p,4096,PAGE_EXECUTE_READ,&old)){DWORD e=GetLastError();VirtualFree(p,0,MEM_RELEASE);Log("POLICY executable transition denied error="+std::to_string(e));return -(int)e;}
 FlushInstructionCache(GetCurrentProcess(),p,sizeof(code));int result=((int(*)())p)();VirtualFree(p,0,MEM_RELEASE);Log("POLICY original code returned="+std::to_string(result));return result;
}
static int Attempt(const wchar_t* root){
 Log("ATTEMPT_BEGIN runtime=native-route/thread-lifecycle original-client import/initialization attempt");
 Hollowwake::ConfigureGuestThreads(Log);
 ConfigureDependencies(root,Import,Log,QueryImport);ConfigureGuestModules(QueryImport,Log,Hollowwake::NativeSystemFacadeAvailable);
 if(consumed)Fail("Restart the app before another mapping attempt");
 std::wstring file=std::wstring(root)+L"\\6.21\\FortniteGame\\Binaries\\Win64\\FortniteClient-Win64-Shipping.exe";
 std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in)Fail("Exact client missing: import archive first");auto length=in.tellg();if(length<4096||length>512*1024*1024)Fail("Unexpected main image size");in.seekg(0);std::vector<BYTE> raw((SIZE_T)length);if(!in.read((char*)raw.data(),length))Fail("Image read failed");if(!Hash(raw))Fail("Main executable SHA256 differs from inspected 6.21 build");Log("HASH exact main image verified");
 auto dos=(IMAGE_DOS_HEADER*)raw.data();if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<64||!Range(dos->e_lfanew,sizeof(IMAGE_NT_HEADERS64),raw.size()))Fail("Invalid PE header");
 auto nt=(IMAGE_NT_HEADERS64*)(raw.data()+dos->e_lfanew);if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64))Fail("Not supported x64 PE32+");
 auto& o=nt->OptionalHeader;if(!o.SizeOfImage||o.SizeOfImage>1024*1024*1024||o.SizeOfHeaders>raw.size()||o.SizeOfHeaders>o.SizeOfImage||o.NumberOfRvaAndSizes<16||nt->FileHeader.NumberOfSections>96)Fail("Invalid PE limits");
 auto sections=IMAGE_FIRST_SECTION(nt);if(!Range((BYTE*)sections-raw.data(),nt->FileHeader.NumberOfSections*sizeof(IMAGE_SECTION_HEADER),raw.size()))Fail("Invalid section table");consumed=true;
 mappedSize=o.SizeOfImage;mapped=(BYTE*)VirtualAllocFromApp(nullptr,mappedSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!mapped){Log("ALLOC error="+std::to_string(GetLastError()));Fail("Client RW allocation failed");}RegisterGuestMainImage(mapped,mappedSize,file);Log("MAP bytes="+std::to_string(mappedSize));memcpy(mapped,raw.data(),o.SizeOfHeaders);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){auto& s=sections[i];if(!Range(s.PointerToRawData,s.SizeOfRawData,raw.size())||!Range(s.VirtualAddress,s.SizeOfRawData,mappedSize)||!Range(s.VirtualAddress,s.Misc.VirtualSize,mappedSize))Fail("Invalid section bounds");memcpy(mapped+s.VirtualAddress,raw.data()+s.PointerToRawData,s.SizeOfRawData);}
 INT64 delta=(INT64)mapped-(INT64)o.ImageBase;auto reloc=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];if(delta&&!reloc.Size)Fail("Relocations absent");
 for(DWORD used=0;delta&&used<reloc.Size;){auto b=(IMAGE_BASE_RELOCATION*)Rva(reloc.VirtualAddress+used,8);if(b->SizeOfBlock<8||b->SizeOfBlock>reloc.Size-used||b->SizeOfBlock%2)Fail("Invalid relocation block");auto entries=(WORD*)Rva(reloc.VirtualAddress+used+8,b->SizeOfBlock-8);for(DWORD j=0;j<(b->SizeOfBlock-8)/2;++j){WORD type=entries[j]>>12;if(type==IMAGE_REL_BASED_DIR64)*(ULONGLONG*)Rva(b->VirtualAddress+(entries[j]&4095),8)+=delta;else if(type!=IMAGE_REL_BASED_ABSOLUTE)Fail("Unsupported relocation type");}used+=b->SizeOfBlock;}Log("RELOC applied");
 // Apply exact-client TLS before import-dependent initialization. OS loads only
 // our packaged donor, never archive-provided DLLs via the native OS loader.
 auto td=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];if(td.Size){if(td.Size<sizeof(IMAGE_TLS_DIRECTORY64))Fail("Invalid TLS directory");auto t=(IMAGE_TLS_DIRECTORY64*)Rva(td.VirtualAddress,sizeof(IMAGE_TLS_DIRECTORY64));
  auto start=t->StartAddressOfRawData-(ULONGLONG)mapped;auto count=t->EndAddressOfRawData-t->StartAddressOfRawData;if(t->EndAddressOfRawData<t->StartAddressOfRawData||!Range(start,count,mappedSize))Fail("Invalid TLS template");
  if(t->AddressOfCallBacks){auto cb=t->AddressOfCallBacks-(ULONGLONG)mapped;if(*(ULONGLONG*)Rva((DWORD)cb,8))Fail("Guest TLS callbacks need dependency-graph initialization first");}
  HMODULE donor=LoadPackagedLibrary(L"HollowwakeTlsDonor.dll",0);if(!donor){Log("TLS donor load error="+std::to_string(GetLastError()));Fail("Packaged TLS donor unavailable");}
  auto bind=(BOOL(WINAPI*)(const void*,SIZE_T,SIZE_T,void*,const PIMAGE_TLS_CALLBACK*,unsigned))GetProcAddress(donor,"BindGuest");auto index=(DWORD(WINAPI*)())GetProcAddress(donor,"GuestTlsIndex");if(!bind||!index||!bind(Rva((DWORD)start,(SIZE_T)count),(SIZE_T)count,t->SizeOfZeroFill,mapped,nullptr,0))Fail("TLS donor binding failed");
  auto idx=t->AddressOfIndex-(ULONGLONG)mapped;if(idx>MAXDWORD)Fail("Invalid TLS index address");*(DWORD*)Rva((DWORD)idx,4)=index();Log("TLS donor index="+std::to_string(index())+" templateBytes="+std::to_string(count));
 }
 auto imp=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];bool end=!imp.Size;
 for(DWORD offset=0;!end&&Range(offset,sizeof(IMAGE_IMPORT_DESCRIPTOR),imp.Size);offset+=sizeof(IMAGE_IMPORT_DESCRIPTOR)){auto d=(IMAGE_IMPORT_DESCRIPTOR*)Rva(imp.VirtualAddress+offset,sizeof(IMAGE_IMPORT_DESCRIPTOR));if(!d->Name){end=true;break;}Thunks(Name(d->Name),d->OriginalFirstThunk?d->OriginalFirstThunk:d->FirstThunk,d->FirstThunk);}if(!end)Fail("Unterminated import directory");
 struct Delay {DWORD flags,name,module,iat,intable,bound,unload,time;};auto delay=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];end=!delay.Size;
 for(DWORD offset=0;!end&&Range(offset,sizeof(Delay),delay.Size);offset+=sizeof(Delay)){auto d=(Delay*)Rva(delay.VirtualAddress+offset,sizeof(Delay));if(!d->name){end=true;break;}if(d->flags!=1)Fail("Unsupported VA-based delay import");Thunks(Name(d->name),d->intable,d->iat);}if(!end)Fail("Unterminated delay import directory");
 Log("IMPORT total="+std::to_string(imports)+" unassigned="+std::to_string(missing)+" fail-fast-thunks="+std::to_string(UnsupportedCount()));
 // Exercise the real target section's RX transition without jumping into it.
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){auto& s=sections[i];if(!s.Misc.VirtualSize)continue;bool x=(s.Characteristics&IMAGE_SCN_MEM_EXECUTE)!=0,w=(s.Characteristics&IMAGE_SCN_MEM_WRITE)!=0;if(x&&w)Fail("Writable executable section rejected");DWORD protect=x?PAGE_EXECUTE_READ:(w?PAGE_READWRITE:PAGE_READONLY),old;
  if(!VirtualProtectFromApp(Rva(s.VirtualAddress,s.Misc.VirtualSize),s.Misc.VirtualSize,protect,&old)){Log("PROTECT error="+std::to_string(GetLastError()));Fail("Client section protection failed");}}
 FlushInstructionCache(GetCurrentProcess(),mapped,mappedSize);Log("PROTECT target sections applied");
 auto unwind=o.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];if(unwind.Size){if(unwind.Size%sizeof(RUNTIME_FUNCTION))Fail("Invalid unwind table size");auto table=(PRUNTIME_FUNCTION)Rva(unwind.VirtualAddress,unwind.Size);for(DWORD i=0;i<unwind.Size/sizeof(*table);++i){if(table[i].BeginAddress>=table[i].EndAddress||!Range(table[i].BeginAddress,table[i].EndAddress-table[i].BeginAddress,mappedSize))Fail("Invalid unwind function range");Rva(table[i].UnwindData,1);}if(!RtlAddFunctionTable(table,unwind.Size/sizeof(*table),(DWORD64)mapped))Fail("Unwind registration failed");Log("UNWIND registered="+std::to_string(unwind.Size/sizeof(*table)));}
 Log("DEPENDENCY unresolved/initialization blockers="+std::to_string(DependencyFailures()));
 if(missing||DependencyFailures())Fail("Client entry blocked by unresolved native imports; see UNRESOLVED records");
 if(!SealUnsupported())Fail("Cannot make unsupported-import fail-fast thunks executable");
 Log("BOOT unsupported imports="+std::to_string(UnsupportedCount())+". Their first invocation terminates with a named failure; no fake success.");
 // Expected Win32 socket/HTTP imports have no network implementation and route
 // to fail-fast. Project HTTP remains in the host; no credentials enter the guest.
 guestStarted=true;
 if(!InitializeDependencies())Fail("Dependency process initialization failed");
 if(!o.AddressOfEntryPoint)Fail("Main entry absent");Log("EXECUTE exact Fortnite main entry RVA="+std::to_string(o.AddressOfEntryPoint));
 auto entry=Rva(o.AddressOfEntryPoint,1);int result=((int(WINAPI*)())entry)();Hollowwake::RunLegacyProcessExitCallbacks();Log("RETURN main entry result="+std::to_string(result)+"; ending experimental process lifetime");RoFailFastWithErrorContext(E_ABORT);return result;
}
static int Body(const wchar_t* root){try{return Attempt(root);}catch(const std::exception& e){Log(std::string("STOP ")+e.what());if(guestStarted)RoFailFastWithErrorContext(E_FAIL);return -1;}}
static int Crash(EXCEPTION_POINTERS* p){Log("CRASH code="+std::to_string(p->ExceptionRecord->ExceptionCode)+" instruction="+std::to_string(p->ContextRecord->Rip));if(guestStarted)RoFailFastWithErrorContext(E_FAIL);return EXCEPTION_EXECUTE_HANDLER;}
extern "C" __declspec(dllexport) int __cdecl HWAttempt(const wchar_t* root){__try{return Body(root);}__except(Crash(GetExceptionInformation())){return -2;}}

extern "C" __declspec(dllexport) int __cdecl HWMemoryPolicy(const wchar_t* path){__try{return MemoryPolicy(path);}__except(Crash(GetExceptionInformation())){return -2;}}

extern "C" __declspec(dllexport) int __cdecl HWGraphicsContract(){
 struct ContractScope{ContractScope(){testingGraphics=true;}~ContractScope(){testingGraphics=false;}} scope;
 Microsoft::WRL::ComPtr<ID3D11Device> device;Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;D3D_FEATURE_LEVEL requested[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
 HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,requested,2,D3D11_SDK_VERSION,&device,&level,&context);if(FAILED(hr)){Log("GRAPHICS device HRESULT="+std::to_string(hr));return hr;}
 Microsoft::WRL::ComPtr<IDXGIFactory1> factory;hr=Hollowwake::CreateCoreFactory(boundCoreWindow.Get(),IID_PPV_ARGS(&factory));if(FAILED(hr))return hr;DXGI_SWAP_CHAIN_DESC desc={};desc.BufferDesc.Width=640;desc.BufferDesc.Height=360;desc.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.Windowed=TRUE;
 Microsoft::WRL::ComPtr<IDXGISwapChain> swap;hr=factory->CreateSwapChain(device.Get(),&desc,&swap);if(FAILED(hr)){Log("GRAPHICS composition bridge HRESULT="+std::to_string(hr));return hr;}
 Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer;hr=swap->GetBuffer(0,IID_PPV_ARGS(&buffer));if(FAILED(hr))return hr;Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;hr=device->CreateRenderTargetView(buffer.Get(),nullptr,&view);if(FAILED(hr))return hr;float color[]={0.02f,0.02f,0.02f,1};context->ClearRenderTargetView(view.Get(),color);hr=swap->Present(1,0);Log("GRAPHICS real factory/composition Present HRESULT="+std::to_string(hr));if(gamePanel)gamePanel->SetSwapChain(nullptr);return SUCCEEDED(hr)?621:hr;
}

extern "C" __declspec(dllexport) int __cdecl HWTlsContract(){
 auto first=LoadPackagedLibrary(L"HollowwakeTlsDonor.dll",0),second=LoadPackagedLibrary(L"HollowwakeTlsSlot01.dll",0);if(!first||!second){Log("TLS packaged contract LoadPackagedLibrary error="+std::to_string(GetLastError()));return -1;}auto a=(DWORD(WINAPI*)())GetProcAddress(first,"GuestTlsIndex"),b=(DWORD(WINAPI*)())GetProcAddress(second,"GuestTlsIndex");if(!a||!b)return -2;auto x=a(),y=b();Log("TLS packaged contract indices="+std::to_string(x)+","+std::to_string(y));return first!=second&&x!=y?621:-3;
}

extern "C" __declspec(dllexport) int __cdecl HWClockContract(){
 auto address=Hollowwake::ResolveClockImport("KERNEL32.dll","GetTickCount");if(!address)return -1;
 auto before=Hollowwake::TickCountLow32(GetTickCount64());auto actual=reinterpret_cast<DWORD(WINAPI*)()>(address)();auto after=Hollowwake::TickCountLow32(GetTickCount64());
 bool bracket=(DWORD)(actual-before)<=(DWORD)(after-before);
 bool wrap=Hollowwake::TickCountLow32(0x100000010ULL)==16&&(DWORD)(Hollowwake::TickCountLow32(0x100000010ULL)-Hollowwake::TickCountLow32(0xfffffff0ULL))==32;
 Log("CLOCK production GetTickCount="+std::to_string(actual)+" OS uptime bracket="+std::to_string(bracket)+" wrap="+std::to_string(wrap));return bracket&&wrap?621:-2;
}

extern "C" __declspec(dllexport) int __cdecl HWAllocatorContract(){bool ok=Hollowwake::LegacyAllocatorContract();Log(std::string("ALLOCATOR legacy CRT shared ownership, grow preservation, zero initialization and failure preservation=")+(ok?"PASS":"FAIL"));return ok?621:-1;}

extern "C" __declspec(dllexport) int __cdecl HWSystemInfoContract(){return Hollowwake::SystemInfoContract()?621:-1;}
