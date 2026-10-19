// Original functional check of reviewed audio providers. No game code runs.
#include <windows.h>
#include <unknwn.h>
#include <fstream>
#include <vector>
#include <cmath>
namespace {
template<class T>T Symbol(HMODULE m,const char* name){return reinterpret_cast<T>(GetProcAddress(m,name));}
struct AudioApi {
 void* (__cdecl* open)(const char*);char (__cdecl* close)(void*);void* (__cdecl* context)(void*,const int*);char (__cdecl* current)(void*);void* (__cdecl* getCurrent)();void (__cdecl* destroy)(void*);void (__cdecl* render)(void*,void*,int);
 void (__cdecl* genBuffers)(int,unsigned*);void (__cdecl* deleteBuffers)(int,const unsigned*);void (__cdecl* bufferData)(unsigned,int,const void*,int,int);void (__cdecl* genSources)(int,unsigned*);void (__cdecl* deleteSources)(int,const unsigned*);void (__cdecl* sourcei)(unsigned,int,int);void (__cdecl* play)(unsigned);int (__cdecl* error)();
};
BOOL CALLBACK Capture(const GUID*,LPCWSTR,LPCWSTR,LPVOID state){++*static_cast<unsigned*>(state);return TRUE;}
}
extern "C" __declspec(dllexport) int __cdecl HWAudioProviderProbe(const wchar_t* report){
 if(!report)return E_POINTER;HRESULT result=E_FAIL,createResult=E_PENDING,captureResult=E_PENDING;DWORD loadError=0;bool mixed=false;unsigned callbacks=0;HMODULE ds=nullptr,al=nullptr;const char* stage="LoadPackagedLibrary HWAUD.dll";
 do {
  ds=LoadPackagedLibrary(L"HWAUD.dll",0);if(!ds){loadError=GetLastError();result=HRESULT_FROM_WIN32(loadError);break;}
  stage="LoadPackagedLibrary dsoal-aldrv.dll";al=LoadPackagedLibrary(L"dsoal-aldrv.dll",0);if(!al){loadError=GetLastError();result=HRESULT_FROM_WIN32(loadError);break;}
  using Address=void*(__cdecl*)(void*,const char*);auto address=Symbol<Address>(al,"alcGetProcAddress");if(!address){result=HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);break;}
  AudioApi a={};
  stage="OpenAL production exports";
  a.open=reinterpret_cast<decltype(a.open)>(address(nullptr,"alcLoopbackOpenDeviceSOFT"));a.render=reinterpret_cast<decltype(a.render)>(address(nullptr,"alcRenderSamplesSOFT"));
  a.close=Symbol<decltype(a.close)>(al,"alcCloseDevice");a.context=Symbol<decltype(a.context)>(al,"alcCreateContext");a.current=Symbol<decltype(a.current)>(al,"alcMakeContextCurrent");a.getCurrent=Symbol<decltype(a.getCurrent)>(al,"alcGetCurrentContext");a.destroy=Symbol<decltype(a.destroy)>(al,"alcDestroyContext");
  a.genBuffers=Symbol<decltype(a.genBuffers)>(al,"alGenBuffers");a.deleteBuffers=Symbol<decltype(a.deleteBuffers)>(al,"alDeleteBuffers");a.bufferData=Symbol<decltype(a.bufferData)>(al,"alBufferData");a.genSources=Symbol<decltype(a.genSources)>(al,"alGenSources");a.deleteSources=Symbol<decltype(a.deleteSources)>(al,"alDeleteSources");a.sourcei=Symbol<decltype(a.sourcei)>(al,"alSourcei");a.play=Symbol<decltype(a.play)>(al,"alSourcePlay");a.error=Symbol<decltype(a.error)>(al,"alGetError");
  if(!a.open||!a.render||!a.close||!a.context||!a.current||!a.getCurrent||!a.destroy||!a.genBuffers||!a.deleteBuffers||!a.bufferData||!a.genSources||!a.deleteSources||!a.sourcei||!a.play||!a.error){result=HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);break;}
  stage="Actual OpenAL PCM mixing";void* device=a.open(nullptr);if(!device){result=E_FAIL;break;}const int attributes[]={0x1007,48000,0x1990,0x1501,0x1991,0x1402,0}; // Frequency, stereo channels, signed short.
  void* context=a.context(device,attributes);if(!context){a.close(device);result=E_FAIL;break;}void* previous=a.getCurrent();if(!a.current(context)){a.destroy(context);a.close(device);result=E_FAIL;break;}
  unsigned buffer=0,source=0;a.genBuffers(1,&buffer);a.genSources(1,&source);std::vector<short> pcm(4800),output(9600);for(unsigned i=0;i<pcm.size();++i)pcm[i]=static_cast<short>(10000*std::sin(i*6.283185307179586*440/48000));
  a.bufferData(buffer,0x1101,pcm.data(),static_cast<int>(pcm.size()*sizeof(short)),48000);a.sourcei(source,0x1009,buffer);a.play(source);int before=a.error();a.render(device,output.data(),4800);int after=a.error();long long energy=0;unsigned nonzero=0;for(auto value:output){energy+=static_cast<long long>(value)*value;if(value)++nonzero;}mixed=before==0&&after==0&&nonzero>100&&energy>1000000;
  a.deleteSources(1,&source);a.deleteBuffers(1,&buffer);bool restored=a.current(previous)!=0;a.destroy(context);bool closed=a.close(device)!=0;if(!mixed||!restored||!closed){result=E_FAIL;break;}
  stage="DirectSound actual exports and endpoint operations";
  using Create=HRESULT(WINAPI*)(const GUID*,IUnknown**,IUnknown*);using Enumerate=HRESULT(WINAPI*)(decltype(&Capture),void*);
  auto create=Symbol<Create>(ds,"DirectSoundCreate8");auto capture=Symbol<Enumerate>(ds,"DirectSoundCaptureEnumerateW");auto captureCreate=Symbol<Create>(ds,"DirectSoundCaptureCreate8");
  if(!create||!capture||!captureCreate){result=HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);break;}
  // Real endpoint creation may report no audio device on a hosted runner. Record
  // that precise result; the loopback mixer is not a claim of audible playback.
  IUnknown* playback=nullptr;createResult=create(nullptr,&playback,nullptr);if(playback)playback->Release();
  captureResult=capture(&Capture,&callbacks);if(FAILED(captureResult)){result=captureResult;break;}
  result=S_OK;stage="Provider loading and PCM mixing complete; endpoint result recorded";
 }while(false);
 if(al)FreeLibrary(al);if(ds)FreeLibrary(ds);
 std::ofstream out(report,std::ios::trunc);if(!out)return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
 out<<"{\"scope\":\"Original hosted UWP audio implementation test; no game/Xbox execution\",\"hresult\":"<<static_cast<long>(result)<<",\"stage\":\""<<stage<<"\",\"loadError\":"<<loadError<<",\"pcmMixed\":"<<(mixed?"true":"false")<<",\"directSoundCreateHresult\":"<<static_cast<long>(createResult)<<",\"captureEnumerateHresult\":"<<static_cast<long>(captureResult)<<",\"captureCallbacks\":"<<callbacks<<",\"gameExecuted\":false,\"xboxTested\":false}";out.flush();return out.good()?result:HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
}
