// Actual DXGI/D3D11 operations, no window/mock lobby/presentation or game code.
#include "../../client-runtime/graphics/CoreWindowBridge.h"
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
using Microsoft::WRL::ComPtr;
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 ComPtr<IDXGIFactory1> factory;Check(SUCCEEDED(Hollowwake::CreateCoreFactory(nullptr,IID_PPV_ARGS(&factory))),"Actual DXGI factory creation failed");
 ComPtr<IDXGIAdapter1> adapter;auto enumeration=factory->EnumAdapters1(0,&adapter);Check(SUCCEEDED(enumeration),"Actual native adapter enumeration failed");
 ComPtr<IDXGIFactory1> parent;Check(SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&parent))),"Wrapped adapter factory parent query failed");
 ComPtr<IUnknown> first,second;factory.As(&first);parent.As(&second);Check(first.Get()==second.Get(),"Adapter leaked a native factory instead of bridge factory identity");
 ComPtr<IDXGIAdapter> native;Check(SUCCEEDED(Hollowwake::UnwrapCoreAdapter(adapter.Get(),&native))&&native,"Adapter could not be unwrapped for real D3D11");
 DXGI_ADAPTER_DESC wrappedDesc{},nativeDesc{};Check(SUCCEEDED(adapter->GetDesc(&wrappedDesc))&&SUCCEEDED(native->GetDesc(&nativeDesc)),"Actual adapter description failed");Check(!memcmp(&wrappedDesc.AdapterLuid,&nativeDesc.AdapterLuid,sizeof(LUID)),"Unwrapped adapter identity changed");
 ComPtr<IDXGIAdapter> base;Check(SUCCEEDED(factory->EnumAdapters(0,&base)),"Base adapter enumeration failed");ComPtr<IDXGIFactory1> baseParent;Check(SUCCEEDED(base->GetParent(IID_PPV_ARGS(&baseParent))),"Base adapter parent query failed");ComPtr<IUnknown> baseIdentity;baseParent.As(&baseIdentity);Check(baseIdentity.Get()==first.Get(),"Base enumeration bypassed bridge factory");
 unsigned outputs=0;for(UINT i=0;i<16;++i){ComPtr<IDXGIOutput> output;auto hr=adapter->EnumOutputs(i,&output);if(hr==DXGI_ERROR_NOT_FOUND)break;Check(SUCCEEDED(hr),"Actual output enumeration failed");ComPtr<IDXGIAdapter> outputParent;Check(SUCCEEDED(output->GetParent(IID_PPV_ARGS(&outputParent))),"Output parent query failed");ComPtr<IUnknown> a,b;adapter.As(&a);outputParent.As(&b);Check(a.Get()==b.Get(),"Output parent leaked native adapter");++outputs;}
 // Actual WARP resource work proves that this device path executes D3D11,
 // independently of a physical GPU or a CoreWindow. No rendering is claimed.
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL chosen;
 Check(SUCCEEDED(Hollowwake::CreateCoreDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&chosen,&context)),"Actual WARP D3D11 device creation failed");
 unsigned input[4]={4526925,621,0x1234abcd,0x621};D3D11_BUFFER_DESC desc={};desc.ByteWidth=sizeof(input);desc.Usage=D3D11_USAGE_DEFAULT;D3D11_SUBRESOURCE_DATA initial={};initial.pSysMem=input;
 ComPtr<ID3D11Buffer> source;Check(SUCCEEDED(device->CreateBuffer(&desc,&initial,&source)),"Actual D3D11 buffer creation failed");desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Buffer> staging;Check(SUCCEEDED(device->CreateBuffer(&desc,nullptr,&staging)),"Actual staging buffer creation failed");context->CopyResource(staging.Get(),source.Get());D3D11_MAPPED_SUBRESOURCE mapped={};Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Actual copied resource mapping failed");bool same=!memcmp(mapped.pData,input,sizeof(input));context->Unmap(staging.Get(),0);Check(same,"Real D3D11 resource copy did not preserve input data");
 // Compare native and wrapped adapter creation error/success semantics.
 ComPtr<ID3D11Device> direct,bridged;auto directHr=D3D11CreateDevice(native.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&direct,nullptr,nullptr);auto bridgedHr=Hollowwake::CreateCoreDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&bridged,nullptr,nullptr);Check(directHr==bridgedHr,"Wrapped adapter changed actual native device creation result");
 std::cout<<"PASS actual adapter/base factory identity and native unwrap; outputs checked="<<outputs<<"; native/wrapped adapter device HRESULT="<<directHr<<"; actual WARP buffer upload/copy/readback passed. Hosted Windows only; no CoreWindow presentation, Fortnite or Xbox execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
