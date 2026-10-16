// Original WinRT Gamepad -> guest XInput bridge. No synthetic connected pads.
#include <windows.h>
#include <xinput.h>
#include <agile.h>
#include <mutex>
#include <cstring>
#include <algorithm>
using namespace Windows::Gaming::Input;
static std::mutex controllerMutex;static XINPUT_GAMEPAD prior[4]={};static DWORD packets[4]={};static bool inputEnabled=true;
static SHORT Stick(double value){return (SHORT)(std::max(-1.0,std::min(1.0,value))*(value<0?32768:32767));}
static DWORD WINAPI Read(DWORD user,XINPUT_STATE* state){
 if(!state)return ERROR_BAD_ARGUMENTS;*state={};try{auto pads=Gamepad::Gamepads;if(user>=4||user>=pads->Size)return ERROR_DEVICE_NOT_CONNECTED;auto r=pads->GetAt(user)->GetCurrentReading();XINPUT_GAMEPAD value={};
 #define BUTTON(src,dst) if(((unsigned long long)r.Buttons&(unsigned long long)GamepadButtons::src)!=0)value.wButtons|=dst
 BUTTON(A,XINPUT_GAMEPAD_A);BUTTON(B,XINPUT_GAMEPAD_B);BUTTON(X,XINPUT_GAMEPAD_X);BUTTON(Y,XINPUT_GAMEPAD_Y);BUTTON(Menu,XINPUT_GAMEPAD_START);BUTTON(View,XINPUT_GAMEPAD_BACK);BUTTON(DPadUp,XINPUT_GAMEPAD_DPAD_UP);BUTTON(DPadDown,XINPUT_GAMEPAD_DPAD_DOWN);BUTTON(DPadLeft,XINPUT_GAMEPAD_DPAD_LEFT);BUTTON(DPadRight,XINPUT_GAMEPAD_DPAD_RIGHT);BUTTON(LeftShoulder,XINPUT_GAMEPAD_LEFT_SHOULDER);BUTTON(RightShoulder,XINPUT_GAMEPAD_RIGHT_SHOULDER);BUTTON(LeftThumbstick,XINPUT_GAMEPAD_LEFT_THUMB);BUTTON(RightThumbstick,XINPUT_GAMEPAD_RIGHT_THUMB);
 #undef BUTTON
 value.bLeftTrigger=(BYTE)(std::max(0.0,std::min(1.0,r.LeftTrigger))*255);value.bRightTrigger=(BYTE)(std::max(0.0,std::min(1.0,r.RightTrigger))*255);value.sThumbLX=Stick(r.LeftThumbstickX);value.sThumbLY=Stick(r.LeftThumbstickY);value.sThumbRX=Stick(r.RightThumbstickX);value.sThumbRY=Stick(r.RightThumbstickY);
 std::lock_guard<std::mutex> l(controllerMutex);if(!inputEnabled)value={};if(memcmp(&value,&prior[user],sizeof(value))){prior[user]=value;++packets[user];}state->dwPacketNumber=packets[user];state->Gamepad=value;return ERROR_SUCCESS;
 }catch(Platform::Exception^){return ERROR_DEVICE_NOT_CONNECTED;}
}
static DWORD WINAPI Vibrate(DWORD user,XINPUT_VIBRATION* value){if(!value)return ERROR_BAD_ARGUMENTS;try{auto pads=Gamepad::Gamepads;if(user>=4||user>=pads->Size)return ERROR_DEVICE_NOT_CONNECTED;GamepadVibration v={};{std::lock_guard<std::mutex> l(controllerMutex);if(inputEnabled){v.LeftMotor=value->wLeftMotorSpeed/65535.0;v.RightMotor=value->wRightMotorSpeed/65535.0;}}pads->GetAt(user)->Vibration=v;return ERROR_SUCCESS;}catch(Platform::Exception^){return ERROR_DEVICE_NOT_CONNECTED;}}
// Unknown capabilities/audio APIs are not synthesized; unresolved imports keep
// the guest-entry gate closed until their actual semantics are implemented.
void* ResolveControllerImport(const char* dll,const char* name){if(_stricmp(dll,"xinput1_3.dll")&&_stricmp(dll,"xinput1_4.dll")&&_stricmp(dll,"xinput9_1_0.dll"))return nullptr;if((!strcmp(name,"XInputGetState")||!strcmp(name,"#2")))return reinterpret_cast<void*>(&Read);if((!strcmp(name,"XInputSetState")||!strcmp(name,"#3")))return reinterpret_cast<void*>(&Vibrate);return nullptr;}
extern "C" __declspec(dllexport) int __cdecl HWControllerCount(){try{return Gamepad::Gamepads->Size;}catch(Platform::Exception^ e){return e->HResult;}}
