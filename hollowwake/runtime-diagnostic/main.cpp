// Original, credential-free diagnostic. No external PE loader or Fortnite code.
#include <windows.h>
#include <agile.h>
#include <wrl.h>
#include <d3d11_2.h>
#include <dxgi1_3.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <xaudio2.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "Shaders/VSBlob.h"
#include "Shaders/PSBlob.h"
using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::Gaming::Input;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;
namespace HollowwakeRuntimeDiagnostic {
ref class View sealed : IFrameworkView {
 CoreWindow^ window; bool closed=false,visible=true,suspended=false,paused=false;
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain1> swap;
 ComPtr<ID3D11RenderTargetView> rtv;ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
 ComPtr<ID3D11InputLayout> layout;ComPtr<ID3D11Buffer> vertices;
 ComPtr<ID2D1Factory1> d2dFactory;ComPtr<ID2D1Device> d2dDevice;ComPtr<ID2D1DeviceContext> d2d;
 ComPtr<ID2D1Bitmap1> target;ComPtr<ID2D1SolidColorBrush> brush;ComPtr<IDWriteFactory> writeFactory;ComPtr<IDWriteTextFormat> format;
 ComPtr<IXAudio2> audio;IXAudio2MasteringVoice* mastering=nullptr;IXAudio2SourceVoice* source=nullptr;std::vector<short> tone;
 D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_10_0;unsigned long long frames=0,presents=0,inputs=0,suspends=0,resumes=0,tones=0;
 unsigned long long lastButtons=0;unsigned width=1,height=1,gamepads=0;float x=0,y=0;bool alternate=false;
 bool softwareRenderer=false; HRESULT audioResult=E_PENDING,lastPresent=S_OK;LARGE_INTEGER start,frequency;std::wstring phase=L"initializing",fatal=L"";
 void Check(HRESULT hr){if(FAILED(hr))throw Exception::CreateException(hr);}
 void Save(){
  try{std::wstring path(Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data());std::ofstream f(path+L"\\diagnostic.json",std::ios::trunc);f<<"{\"schema\":1,\"originalDiagnostic\":true,\"fortniteIncluded\":false,\"frames\":"<<frames<<",\"successfulPresents\":"<<presents<<",\"featureLevel\":"<<(unsigned)level<<",\"softwareRenderer\":"<<(softwareRenderer?"true":"false")<<",\"gamepads\":"<<gamepads<<",\"inputEvents\":"<<inputs<<",\"suspends\":"<<suspends<<",\"resumes\":"<<resumes<<",\"audioInitHRESULT\":"<<(long)audioResult<<",\"tonesSubmitted\":"<<tones<<",\"lastPresentHRESULT\":"<<(long)lastPresent<<",\"fatalError\":"<<(!fatal.empty()?1:0)<<"}";}
  catch(...){} // Never redirect diagnostics into user-owned folders.
 }
 void Targets(){
  ComPtr<ID3D11Texture2D> back;Check(swap->GetBuffer(0,IID_PPV_ARGS(&back)));Check(device->CreateRenderTargetView(back.Get(),nullptr,&rtv));
  ComPtr<IDXGISurface> surface;Check(back.As(&surface));auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96,96);Check(d2d->CreateBitmapFromDxgiSurface(surface.Get(),&props,&target));d2d->SetTarget(target.Get());
 }
 void Resize(){if(!swap)return;context->OMSetRenderTargets(0,nullptr,nullptr);d2d->SetTarget(nullptr);target.Reset();rtv.Reset();width=(unsigned)std::fmax(1,window->Bounds.Width);height=(unsigned)std::fmax(1,window->Bounds.Height);Check(swap->ResizeBuffers(2,width,height,DXGI_FORMAT_B8G8R8A8_UNORM,0));Targets();}
 void Graphics(){
  D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
  HRESULT created=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,4,D3D11_SDK_VERSION,&device,&level,&context);
  if(FAILED(created)&&Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily==L"Windows.Desktop"){softwareRenderer=true;created=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,4,D3D11_SDK_VERSION,&device,&level,&context);}Check(created);
  ComPtr<IDXGIDevice> dxgi;Check(device.As(&dxgi));ComPtr<IDXGIAdapter> adapter;Check(dxgi->GetAdapter(&adapter));ComPtr<IDXGIFactory2> factory;Check(adapter->GetParent(IID_PPV_ARGS(&factory)));
  width=(unsigned)std::fmax(1,window->Bounds.Width);height=(unsigned)std::fmax(1,window->Bounds.Height);
  DXGI_SWAP_CHAIN_DESC1 desc={};desc.Width=width;desc.Height=height;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.Scaling=DXGI_SCALING_STRETCH;desc.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
  Check(factory->CreateSwapChainForCoreWindow(device.Get(),reinterpret_cast<IUnknown*>(window),&desc,nullptr,&swap));
  D2D1_FACTORY_OPTIONS options={};Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),&options,reinterpret_cast<void**>(d2dFactory.GetAddressOf())));Check(d2dFactory->CreateDevice(dxgi.Get(),&d2dDevice));Check(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&d2d));
  Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(writeFactory.GetAddressOf())));Check(writeFactory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,21,L"en-US",&format));Check(d2d->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),&brush));Targets();
  Check(device->CreateVertexShader(VSBlob,sizeof(VSBlob),nullptr,&vs));Check(device->CreatePixelShader(PSBlob,sizeof(PSBlob),nullptr,&ps));D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}};Check(device->CreateInputLayout(elements,2,VSBlob,sizeof(VSBlob),&layout));D3D11_BUFFER_DESC buffer={};buffer.ByteWidth=7*4*3;buffer.Usage=D3D11_USAGE_DYNAMIC;buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER;buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;Check(device->CreateBuffer(&buffer,nullptr,&vertices));phase=L"D3D11 CoreWindow presentation active";
 }
 void Audio(){audioResult=XAudio2Create(&audio,0,XAUDIO2_DEFAULT_PROCESSOR);if(FAILED(audioResult))return;audioResult=audio->CreateMasteringVoice(&mastering,2,48000);if(FAILED(audioResult))return;WAVEFORMATEX wave={};wave.wFormatTag=WAVE_FORMAT_PCM;wave.nChannels=1;wave.nSamplesPerSec=48000;wave.wBitsPerSample=16;wave.nBlockAlign=2;wave.nAvgBytesPerSec=96000;audioResult=audio->CreateSourceVoice(&source,&wave);if(FAILED(audioResult))return;tone.resize(7200);for(size_t i=0;i<tone.size();++i)tone[i]=(short)(1400*std::sin(i*6.283185307179586*440/48000));}
 void Beep(){if(!source)return;XAUDIO2_VOICE_STATE state={};source->GetState(&state);if(state.BuffersQueued)return;XAUDIO2_BUFFER b={};b.AudioBytes=(UINT32)(tone.size()*2);b.pAudioData=reinterpret_cast<const BYTE*>(tone.data());b.Flags=XAUDIO2_END_OF_STREAM;if(SUCCEEDED(source->SubmitSourceBuffer(&b))&&SUCCEEDED(source->Start()))++tones;}
 void Poll(){gamepads=Gamepad::Gamepads->Size;if(!gamepads){lastButtons=0;return;}auto reading=Gamepad::Gamepads->GetAt(0)->GetCurrentReading();auto buttons=(unsigned long long)reading.Buttons;auto changed=buttons&~lastButtons;if(changed){++inputs;if(changed&(unsigned long long)GamepadButtons::A)Beep();if(changed&(unsigned long long)GamepadButtons::B)alternate=!alternate;if(changed&(unsigned long long)GamepadButtons::Menu){paused=!paused;Save();}}lastButtons=buttons;x+=(float)reading.LeftThumbstickX*.025f;y+=(float)reading.LeftThumbstickY*.025f;if(buttons&(unsigned long long)GamepadButtons::DPadLeft)x-=.02f;if(buttons&(unsigned long long)GamepadButtons::DPadRight)x+=.02f;if(buttons&(unsigned long long)GamepadButtons::DPadUp)y+=.02f;if(buttons&(unsigned long long)GamepadButtons::DPadDown)y-=.02f;x=std::fmax(-.75f,std::fmin(.75f,x));y=std::fmax(-.5f,std::fmin(.5f,y));}
 void Render(){
  Poll();++frames;float clear[]={alternate?.12f:.025f,.025f,alternate?.035f:.065f,1};context->ClearRenderTargetView(rtv.Get(),clear);auto view=rtv.Get();context->OMSetRenderTargets(1,&view,nullptr);D3D11_VIEWPORT viewport={0,0,(float)width,(float)height,0,1};context->RSSetViewports(1,&viewport);
  LARGE_INTEGER now;QueryPerformanceCounter(&now);float time=(float)((now.QuadPart-start.QuadPart)/(double)frequency.QuadPart);float bob=paused?0:std::sin(time)*.08f;
  const float data[]={x,y+.25f+bob,0,1,1,1,1,x+.22f,y-.13f+bob,0,.68f,.5f,1,1,x-.22f,y-.13f+bob,0,.4f,.8f,1,1};D3D11_MAPPED_SUBRESOURCE mapped={};Check(context->Map(vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped));memcpy(mapped.pData,data,sizeof(data));context->Unmap(vertices.Get(),0);UINT stride=28,offset=0;auto vb=vertices.Get();context->IASetVertexBuffers(0,1,&vb,&stride,&offset);context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->Draw(3,0);
  std::wstringstream text;text<<L"HOLLOWWAKE / RUNTIME DIAGNOSTIC 0.1\nOriginal test scene. No Fortnite client or lobby.\n\n"<<phase<<(softwareRenderer?L" (Windows WARP software)":L" (hardware device)")<<L"\nFrames: "<<frames<<L"  successful Present: "<<presents<<L"\nD3D feature level: 0x"<<std::hex<<(unsigned)level<<std::dec<<L"  size: "<<width<<L" x "<<height<<L"\nGamepads: "<<gamepads<<L"  button/key events: "<<inputs<<L"\nSuspend/resume: "<<suspends<<L" / "<<resumes<<L"  tones submitted: "<<tones<<L"\nAudio init HRESULT: 0x"<<std::hex<<(unsigned)audioResult<<std::dec<<L"\n\nLeft stick / D-pad / arrow keys: move triangle\nA / Space: short 440 Hz tone\nB / C: change background (B stays in this app)\nMenu / P: pause animation and save log\nLog: app LocalState / diagnostic.json\n\nAn increasing Present count proves this diagnostic's swapchain only.\nIt does not prove a Win32 loader, Fortnite or Xbox compatibility.";
  auto label=text.str();d2d->BeginDraw();d2d->DrawTextW(label.c_str(),(UINT32)label.size(),format.Get(),D2D1::RectF(38,30,(float)width-30,(float)height-20),brush.Get());Check(d2d->EndDraw());lastPresent=swap->Present(1,0);Check(lastPresent);++presents;if(frames%300==0)Save();
 }
 void Suspend(Object^,SuspendingEventArgs^ e){auto deferral=e->SuspendingOperation->GetDeferral();suspended=true;++suspends;Save();if(context){context->ClearState();context->Flush();}ComPtr<IDXGIDevice3> dxgi;if(device&&SUCCEEDED(device.As(&dxgi)))dxgi->Trim();deferral->Complete();}
 void Resume(Object^,Object^){suspended=false;++resumes;Save();}
 void Activate(CoreApplicationView^,IActivatedEventArgs^){window->Activate();}
 void Key(CoreWindow^,KeyEventArgs^ e){++inputs;switch(e->VirtualKey){case Windows::System::VirtualKey::Left:x-=.05f;break;case Windows::System::VirtualKey::Right:x+=.05f;break;case Windows::System::VirtualKey::Up:y+=.05f;break;case Windows::System::VirtualKey::Down:y-=.05f;break;case Windows::System::VirtualKey::Space:Beep();break;case Windows::System::VirtualKey::C:alternate=!alternate;break;case Windows::System::VirtualKey::P:paused=!paused;Save();break;default:break;}}
public:
 virtual void Initialize(CoreApplicationView^ v){QueryPerformanceCounter(&start);QueryPerformanceFrequency(&frequency);v->Activated+=ref new TypedEventHandler<CoreApplicationView^,IActivatedEventArgs^>(this,&View::Activate);CoreApplication::Suspending+=ref new EventHandler<SuspendingEventArgs^>(this,&View::Suspend);CoreApplication::Resuming+=ref new EventHandler<Object^>(this,&View::Resume);}
 virtual void SetWindow(CoreWindow^ value){window=value;window->Closed+=ref new TypedEventHandler<CoreWindow^,CoreWindowEventArgs^>([this](CoreWindow^,CoreWindowEventArgs^){closed=true;Save();});window->VisibilityChanged+=ref new TypedEventHandler<CoreWindow^,VisibilityChangedEventArgs^>([this](CoreWindow^,VisibilityChangedEventArgs^ e){visible=e->Visible;});window->SizeChanged+=ref new TypedEventHandler<CoreWindow^,WindowSizeChangedEventArgs^>([this](CoreWindow^,WindowSizeChangedEventArgs^){Resize();});window->KeyDown+=ref new TypedEventHandler<CoreWindow^,KeyEventArgs^>(this,&View::Key);SystemNavigationManager::GetForCurrentView()->BackRequested+=ref new EventHandler<BackRequestedEventArgs^>([this](Object^,BackRequestedEventArgs^ e){e->Handled=true;++inputs;});Graphics();Audio();Save();}
 virtual void Load(String^){}
 virtual void Run(){try{while(!closed){window->Dispatcher->ProcessEvents(visible&&!suspended?CoreProcessEventsOption::ProcessAllIfPresent:CoreProcessEventsOption::ProcessOneAndAllPending);if(visible&&!suspended)Render();}}catch(Exception^ e){fatal=e->Message->Data();phase=L"fatal graphics error";lastPresent=e->HResult;Save();throw;}}
 virtual void Uninitialize(){Save();if(source){source->DestroyVoice();source=nullptr;}if(mastering){mastering->DestroyVoice();mastering=nullptr;}}
};
public ref class App sealed : IFrameworkViewSource {public:virtual IFrameworkView^ CreateView(){return ref new View();}};
}
[MTAThread]int main(Array<String^>^){CoreApplication::Run(ref new HollowwakeRuntimeDiagnostic::App());return 0;}
