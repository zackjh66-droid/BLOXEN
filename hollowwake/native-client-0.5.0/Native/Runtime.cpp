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
#include "../../runtime-native-route/NativeSystem.h"
#include "../../runtime-native-route/NativeProxyDispatch.h"
#include "../../runtime-native-route/PackagedNamespace.h"
#include "../../runtime-native-route/Season621NativeImports.generated.h"
#include "../../runtime-native-route/GuestThreads.h"
#include "Unsupported.h"
#include "../../client-runtime/graphics/CoreWindowBridge.h"
static std::mutex logMutex;static std::wstring logName;static BYTE* mapped=nullptr;static SIZE_T mappedSize=0;static bool consumed=false,guestStarted=false;
std::string DescribeGuestAddress(unsigned long long address){if(mapped&&address>=(ULONGLONG)mapped&&address-(ULONGLONG)mapped<mappedSize)return "FortniteClient-Win64-Shipping.exe RVA="+std::to_string(address-(ULONGLONG)mapped);return DescribeDependencyAddress(address);}
static void Log(const std::string& text){std::lock_guard<std::mutex> g(logMutex);std::ofstream f(logName,std::ios::app);f<<text<<"\n";f.flush();}
static bool testingGraphics=false;
static Microsoft::WRL::ComPtr<ISwapChainPanelNative> gamePanel;
static Platform::Agile<Windows::UI::Core::CoreDispatcher> gameDispatcher;
static Microsoft::WRL::ComPtr<IUnknown> boundCoreWindow;
static HRESULT AttachOnUi(IDXGISwapChain* value){auto hr=gamePanel->SetSwapChain(value);if(SUCCEEDED(hr)&&!testingGraphics){auto frame=dynamic_cast<Windows::UI::Xaml::Controls::Frame^>(Windows::UI::Xaml::Window::Current->Content);auto page=frame?dynamic_cast<Windows::UI::Xaml::FrameworkElement^>(frame->Content):nullptr;auto overlay=page?dynamic_cast<Windows::UI::Xaml::FrameworkElement^>(page->FindName(L"HostOverlay")):nullptr;if(overlay)overlay->Visibility=Windows::UI::Xaml::Visibility::Collapsed;}return hr;}
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
 if(auto p=ResolveNetworkImport(dll,name))return p;
 if(auto p=Hollowwake::ResolveGraphicsImport(dll,name))return p;
 if(auto p=ResolveWindowImport(dll,name))return p;
 if(auto p=ResolveControllerImport(dll,name))return p;
 return nullptr; // Network/login and additional platform adapters remain mandatory.
}
static void MissingPackagedImport(const char* dll,const char* name){Log("NATIVE_PROXY_BLOCKED "+std::string(dll)+"!"+name);std::lock_guard<std::mutex> guard(logMutex);auto slash=logName.find_last_of(L"/\\");if(slash!=std::wstring::npos){std::ofstream output(logName.substr(0,slash+1)+L"native-imports-unresolved.txt",std::ios::app);output<<dll<<"!"<<name<<"\n";output.flush();}}
extern "C" __declspec(dllexport) int __cdecl HWBindWindow(const wchar_t* path){
 logName=path;Hollowwake::ConfigurePackagedProxyResolver(PackagedRoute,MissingPackagedImport);ConfigureUnsupported(Log);Hollowwake::LegacyFatalLogger()=[](const char* message){Log(std::string("CRASH ")+message);};Hollowwake::SystemInfoFailureHandler()=[](DWORD error){Log("CRASH GetVersionExW failed error="+std::to_string(error));};auto w=Windows::UI::Core::CoreWindow::GetForCurrentThread();if(!w)return (int)RPC_E_WRONG_THREAD;
 HWViewport((int)w->Bounds.Width,(int)w->Bounds.Height);
 auto frame=dynamic_cast<Windows::UI::Xaml::Controls::Frame^>(Windows::UI::Xaml::Window::Current->Content);auto page=frame?dynamic_cast<Windows::UI::Xaml::FrameworkElement^>(frame->Content):nullptr;auto panel=page?dynamic_cast<Windows::UI::Xaml::Controls::SwapChainPanel^>(page->FindName(L"GameSurface")):nullptr;
 if(!panel)return E_NOINTERFACE;auto hr=reinterpret_cast<IUnknown*>(panel)->QueryInterface(IID_PPV_ARGS(&gamePanel));if(FAILED(hr))return hr;gameDispatcher=panel->Dispatcher;boundCoreWindow=reinterpret_cast<IUnknown*>(w);Hollowwake::SetCompositionAttachment(&AttachComposition);
 return Hollowwake::BindGraphicsWindow(boundCoreWindow.Get());
}
// Explicit native-loader experiment. It accepts no download URL, credentials,
// arbitrary DLL path or user command line. The UI's existing mapper is unchanged.
extern "C" __declspec(dllexport) int __cdecl HWAttemptPackagedClient(){
 try{
  if(!boundCoreWindow||!gamePanel){Log("NATIVE_LOAD_BLOCKED graphics window not bound");return E_UNEXPECTED;}
  static std::atomic<bool> attempted{false};if(attempted.exchange(true))return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
  Log("ATTEMPT_BEGIN runtime=0.5.0.0 native-packaged route");
  {auto slash=logName.find_last_of(L"/\\");if(slash!=std::wstring::npos){std::ofstream reset(logName.substr(0,slash+1)+L"native-imports-unresolved.txt",std::ios::trunc);}}
  unsigned unavailable=0;for(auto& item:season621NativeImports)if(!HWResolvePackagedImport(item.dll,item.symbol))++unavailable;
  if(unavailable){Log("NATIVE_LOAD_BLOCKED unresolved main-image proxy operations="+std::to_string(unavailable)+"; no client code executed");return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);}
  auto location=Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
  std::wstring file=std::wstring(location->Data())+L"\\Fortnite621.imports.private.dll";
  std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in){Log("NATIVE_LOAD_BLOCKED verified client DLL is not in installed package; LocalState import is not a package dependency");return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);}
  auto size=in.tellg();if(size!=100733440){Log("NATIVE_LOAD_BLOCKED private client size mismatch");return E_INVALIDARG;}in.seekg(0);std::vector<BYTE> bytes(static_cast<SIZE_T>(size));if(!in.read(reinterpret_cast<char*>(bytes.data()),size))return E_FAIL;
  static const BYTE expected[32]={0xe5,0x20,0x7f,0x54,0x27,0xb4,0x02,0x8a,0x29,0xa8,0xfe,0xa9,0xfa,0xa4,0x4b,0x51,0xeb,0x24,0xaa,0x9e,0x7c,0xab,0x7f,0x02,0x3b,0xcc,0x7a,0x73,0x89,0xe3,0x94,0x85};
  if(!HashPinned(bytes,expected)){Log("NATIVE_LOAD_BLOCKED converted client SHA256 mismatch");return E_INVALIDARG;}bytes.clear();
  if(!Hollowwake::ConfigurePackagedNamespace(L"Fortnite621.imports.private.dll",L"Fortnite621.imports.private.dll")){Log("NATIVE_LOAD_BLOCKED namespace configuration refused");return E_UNEXPECTED;}
  Log("NATIVE_LOAD calling real LoadPackagedLibrary; dependency TLS/DllMain can execute here");
  auto module=LoadPackagedLibrary(L"Fortnite621.imports.private.dll",0);if(!module){auto error=GetLastError();Log("NATIVE_LOAD_FAILED Win32="+std::to_string(error));return HRESULT_FROM_WIN32(error);}
  auto entry=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module,"StartHollowwakeClient"));if(!entry){Log("NATIVE_LOAD_BLOCKED preserved client-entry export missing");return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);}
  Log("NATIVE_ENTRY calling original client entry; no lobby success is inferred");auto result=entry();Log("NATIVE_ENTRY_RETURN value="+std::to_string(result));return result;
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
