#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include "../../client-runtime/graphics/CoreWindowBridge.h"
#include "../../client-runtime/graphics/WindowedSwapChain.h"
using Microsoft::WRL::ComPtr;
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static ComPtr<IDXGISwapChain> actualChain;static UINT targetWidth=4,targetHeight=3;
static HRESULT AttachForReadback(IDXGISwapChain* chain){actualChain=chain;return S_OK;/* Retains actual native chain; does not claim visible presentation. */}
static HRESULT Viewport(UINT* width,UINT* height){*width=targetWidth;*height=targetHeight;return S_OK;}
static void RenderAndRead(ID3D11Device* device,ID3D11DeviceContext* context,IDXGISwapChain* chain,UINT width,UINT height){
 ComPtr<ID3D11Texture2D> buffer;Check(SUCCEEDED(chain->GetBuffer(0,IID_PPV_ARGS(&buffer))),"Real render buffer missing");D3D11_TEXTURE2D_DESC desc={};buffer->GetDesc(&desc);Check(desc.Width==width&&desc.Height==height,"Real buffer dimensions differ");ComPtr<ID3D11RenderTargetView> view;Check(SUCCEEDED(device->CreateRenderTargetView(buffer.Get(),nullptr,&view)),"Real render target failed");const float color[4]={0.25f,0.5f,0.75f,1};context->ClearRenderTargetView(view.Get(),color);
 desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;ComPtr<ID3D11Texture2D> readback;Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&readback)),"Staging texture failed");context->CopyResource(readback.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE data={};Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&data)),"GPU readback failed");auto pixel=(unsigned char*)data.pData;bool valid=pixel[0]>=63&&pixel[0]<=65&&pixel[1]>=127&&pixel[1]<=129&&pixel[2]>=190&&pixel[2]<=192&&pixel[3]==255;context->Unmap(readback.Get(),0);Check(valid,"Actual rendered pixel differs");
}
int main(int argc,char** argv){try{
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"Owned WARP device failed");Hollowwake::SetCompositionAttachment(AttachForReadback);Hollowwake::SetChainViewportProvider(Viewport);
 ComPtr<IDXGIFactory1> factory;Check(SUCCEEDED(Hollowwake::CreateCoreFactory(device.Get(),IID_PPV_ARGS(&factory))),"Production factory wrapper failed");DXGI_SWAP_CHAIN_DESC desc={};desc.BufferDesc.Width=4;desc.BufferDesc.Height=3;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferCount=2;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.Windowed=TRUE;desc.OutputWindow=(HWND)621;
 ComPtr<IDXGISwapChain> chain;Check(SUCCEEDED(factory->CreateSwapChain(device.Get(),&desc,&chain)),"Production composition chain failed");Check(actualChain.Get()!=chain.Get(),"Native chain exposed without wrapper");RenderAndRead(device.Get(),context.Get(),chain.Get(),4,3);
 DXGI_SWAP_CHAIN_DESC actual={};Check(SUCCEEDED(chain->GetDesc(&actual))&&actual.OutputWindow==(HWND)621&&actual.Windowed&&actual.BufferDesc.Width==4,"Guest descriptor/virtual window mismatch");BOOL exclusive=TRUE;Check(SUCCEEDED(chain->GetFullscreenState(&exclusive,nullptr))&&!exclusive,"Actual windowed state wrong");Check(chain->SetFullscreenState(TRUE,nullptr)==DXGI_ERROR_NOT_CURRENTLY_AVAILABLE,"Exclusive fullscreen returned fake success");Check(SUCCEEDED(chain->SetFullscreenState(FALSE,nullptr)),"Existing windowed state rejected");
 ComPtr<IUnknown> expectedParent,actualParent;factory.As(&expectedParent);Check(SUCCEEDED(chain->GetParent(IID_PPV_ARGS(&actualParent)))&&actualParent.Get()==expectedParent.Get(),"Chain parent escaped to native factory");
 Check(chain->ResizeBuffers(2,6,5,DXGI_FORMAT_UNKNOWN,DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)==DXGI_ERROR_UNSUPPORTED,"Unsupported mode flag silently masked");targetWidth=6;targetHeight=5;Check(SUCCEEDED(chain->ResizeBuffers(0,0,0,DXGI_FORMAT_UNKNOWN,0)),"Zero-size resize did not use actual viewport provider");RenderAndRead(device.Get(),context.Get(),chain.Get(),6,5);DXGI_MODE_DESC mode={};mode.Width=6;mode.Height=5;Check(SUCCEEDED(chain->ResizeTarget(&mode)),"Matching existing target rejected");mode.Width=8;Check(chain->ResizeTarget(&mode)==DXGI_ERROR_UNSUPPORTED,"Unsupported display change returned success");
 // Prove, do not conceal, the outstanding native device -> adapter -> factory path.
 ComPtr<IDXGIDevice> dxgi;device.As(&dxgi);ComPtr<IDXGIAdapter> adapter;Check(SUCCEEDED(dxgi->GetAdapter(&adapter)),"Actual device adapter query failed");ComPtr<IUnknown> nativeParent;Check(SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&nativeParent))),"Actual adapter factory query failed");bool escapes=nativeParent.Get()!=expectedParent.Get();Check(escapes,"Expected native device identity boundary changed; re-investigate bridge");
 if(argc>1){std::ofstream out(argv[1]);out<<"{\"scope\":\"Owned production DXGI bridge/WARP GPU readback, not game/Xbox/visible lobby\",\"renderReadbackBeforeAndAfterResize\":true,\"windowedDescriptor\":true,\"chainParentPreserved\":true,\"exclusiveModeRejected\":true,\"deviceAdapterFactoryStillEscapes\":true,\"gameExecuted\":false,\"xboxTested\":false}";}
 std::cout<<"PASS real production composition buffer render/readback before/after viewport resize, guest descriptor and chain parent; no unsupported mode success. BLOCKER reproduced: native device -> adapter -> parent bypasses compatibility factory. No game/Xbox/visible lobby execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
