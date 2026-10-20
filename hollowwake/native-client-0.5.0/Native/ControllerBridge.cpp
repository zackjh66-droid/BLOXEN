// Original WinRT Gamepad -> exact build's XInput ABI. No synthetic devices.
#include "ControllerBridge.h"
#include "ControllerState.h"
#include <agile.h>
#include <wrl/client.h>
#include <mutex>
#include <cstring>
using namespace Windows::Gaming::Input;
namespace {
std::mutex gate;Hollowwake::ControllerSlots state;
Platform::Agile<Gamepad> devices[4];Microsoft::WRL::ComPtr<IUnknown> retained[4];
ULONG_PTR Identity(Gamepad^ p,Microsoft::WRL::ComPtr<IUnknown>& identity){if(!p||FAILED(reinterpret_cast<IUnknown*>(p)->QueryInterface(IID_PPV_ARGS(&identity))))return 0;return reinterpret_cast<ULONG_PTR>(identity.Get());}
void Sync(){auto current=Gamepad::Gamepads;std::vector<ULONG_PTR> live;std::vector<Microsoft::WRL::ComPtr<IUnknown>> identities(current->Size);for(unsigned i=0;i<current->Size;++i)live.push_back(Identity(current->GetAt(i),identities[i]));state.Sync(live);
 for(unsigned slot=0;slot<4;++slot){auto id=state.Identity(slot);if(!id){devices[slot]=nullptr;retained[slot].Reset();continue;}for(unsigned i=0;i<live.size();++i)if(id==live[i]){devices[slot]=current->GetAt(i);retained[slot]=identities[i];break;}}
}
Hollowwake::PadReading Reading(Gamepad^ p){auto r=p->GetCurrentReading();Hollowwake::PadReading v;
#define BUTTON(src,dst) if((static_cast<unsigned long long>(r.Buttons)&static_cast<unsigned long long>(GamepadButtons::src))!=0)v.buttons|=dst
 BUTTON(A,XINPUT_GAMEPAD_A);BUTTON(B,XINPUT_GAMEPAD_B);BUTTON(X,XINPUT_GAMEPAD_X);BUTTON(Y,XINPUT_GAMEPAD_Y);BUTTON(Menu,XINPUT_GAMEPAD_START);BUTTON(View,XINPUT_GAMEPAD_BACK);BUTTON(DPadUp,XINPUT_GAMEPAD_DPAD_UP);BUTTON(DPadDown,XINPUT_GAMEPAD_DPAD_DOWN);BUTTON(DPadLeft,XINPUT_GAMEPAD_DPAD_LEFT);BUTTON(DPadRight,XINPUT_GAMEPAD_DPAD_RIGHT);BUTTON(LeftShoulder,XINPUT_GAMEPAD_LEFT_SHOULDER);BUTTON(RightShoulder,XINPUT_GAMEPAD_RIGHT_SHOULDER);BUTTON(LeftThumbstick,XINPUT_GAMEPAD_LEFT_THUMB);BUTTON(RightThumbstick,XINPUT_GAMEPAD_RIGHT_THUMB);
#undef BUTTON
 v.leftTrigger=r.LeftTrigger;v.rightTrigger=r.RightTrigger;v.leftX=r.LeftThumbstickX;v.leftY=r.LeftThumbstickY;v.rightX=r.RightThumbstickX;v.rightY=r.RightThumbstickY;return v;
}
void Apply(unsigned i,const XINPUT_VIBRATION& request){GamepadVibration v={};v.LeftMotor=request.wLeftMotorSpeed/65535.0;v.RightMotor=request.wRightMotorSpeed/65535.0;devices[i].Get()->Vibration=v;}
DWORD WINAPI Read(DWORD user,XINPUT_STATE* result){if(!result)return ERROR_BAD_ARGUMENTS;*result={};if(user>=4)return ERROR_DEVICE_NOT_CONNECTED;try{std::lock_guard<std::mutex> lock(gate);Sync();if(!state.Identity(user))return ERROR_DEVICE_NOT_CONNECTED;return state.Read(user,Reading(devices[user].Get()),result);}catch(Platform::Exception^){return ERROR_DEVICE_NOT_CONNECTED;}}
DWORD WINAPI Vibrate(DWORD user,XINPUT_VIBRATION* value){if(!value)return ERROR_BAD_ARGUMENTS;if(user>=4)return ERROR_DEVICE_NOT_CONNECTED;try{std::lock_guard<std::mutex> lock(gate);Sync();if(!state.Identity(user))return ERROR_DEVICE_NOT_CONNECTED;auto sent=state.Active()?*value:XINPUT_VIBRATION{};Apply(user,sent);state.RememberVibration(user,*value);return ERROR_SUCCESS;}catch(Platform::Exception^){return ERROR_DEVICE_NOT_CONNECTED;}}
void UpdateOutput(){for(unsigned i=0;i<4;++i)if(state.Identity(i))try{Apply(i,state.DesiredVibration(i));}catch(Platform::Exception^){} /* Device removal never manufactures an attached pad. */}
void WINAPI Enable(BOOL enabled){try{std::lock_guard<std::mutex> lock(gate);state.Enable(enabled!=FALSE);Sync();UpdateOutput();}catch(Platform::Exception^){} }
}
void* ResolveControllerImport(const char* dll,const char* name){if(!dll||!name)return nullptr;bool legacy=!_stricmp(dll,"xinput1_3.dll");if(!legacy&&_stricmp(dll,"xinput1_4.dll")&&_stricmp(dll,"xinput9_1_0.dll"))return nullptr;
 // Ordinals verified from the preserved Microsoft XInput1_3.dll: 2=GetState,
 // 3=SetState, 5=Enable. Other DLL versions do not inherit that ordinal map.
 if(!strcmp(name,"XInputGetState")||(legacy&&!strcmp(name,"#2")))return reinterpret_cast<void*>(&Read);
 if(!strcmp(name,"XInputSetState")||(legacy&&!strcmp(name,"#3")))return reinterpret_cast<void*>(&Vibrate);
 if(!strcmp(name,"XInputEnable")||(legacy&&!strcmp(name,"#5")))return reinterpret_cast<void*>(&Enable);return nullptr;
}
extern "C" void __cdecl HWSetControllerFocus(int active){try{std::lock_guard<std::mutex> lock(gate);state.Focus(active!=0);Sync();UpdateOutput();}catch(Platform::Exception^){} }
extern "C" int __cdecl HWControllerCount(){try{std::lock_guard<std::mutex> lock(gate);Sync();unsigned count=0;for(unsigned i=0;i<4;++i)if(state.Identity(i))++count;return count;}catch(Platform::Exception^ e){return e->HResult;}}
