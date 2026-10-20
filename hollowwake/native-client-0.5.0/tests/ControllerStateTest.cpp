#include "../Native/ControllerState.h"
#include <cassert>
#include <limits>
#include <iostream>
using namespace Hollowwake;
int main(){ControllerSlots s;XINPUT_STATE r={};PadReading a;a.buttons=XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_DPAD_UP;a.leftX=-1;a.leftY=1;a.rightX=0.5;a.leftTrigger=1;a.rightTrigger=0.5;
 assert(s.Read(0,a,&r)==ERROR_DEVICE_NOT_CONNECTED);s.Sync({10,20});assert(s.Identity(0)==10&&s.Identity(1)==20);assert(s.Read(0,a,&r)==0);assert(r.Gamepad.wButtons==a.buttons&&r.Gamepad.sThumbLX==-32768&&r.Gamepad.sThumbLY==32767&&r.Gamepad.sThumbRX==16383&&r.Gamepad.bLeftTrigger==255&&r.Gamepad.bRightTrigger==127);DWORD packet=r.dwPacketNumber;assert(s.Read(0,a,&r)==0&&r.dwPacketNumber==packet);
 s.Sync({20,10});assert(s.Identity(0)==10&&s.Identity(1)==20);s.Sync({20});assert(!s.Identity(0)&&s.Identity(1)==20);s.Sync({20,30});assert(s.Identity(0)==30&&s.Identity(1)==20);assert(s.Read(0,a,&r)==0&&r.dwPacketNumber!=packet);
 XINPUT_VIBRATION vibration={65535,12345};s.RememberVibration(0,vibration);assert(s.DesiredVibration(0).wLeftMotorSpeed==65535);s.Focus(false);assert(s.DesiredVibration(0).wLeftMotorSpeed==0);assert(s.Read(0,a,&r)==0&&r.Gamepad.wButtons==0&&r.Gamepad.sThumbLX==0);s.Focus(true);assert(s.DesiredVibration(0).wRightMotorSpeed==12345);s.Enable(false);assert(s.Read(0,a,&r)==0&&r.Gamepad.wButtons==0);s.Enable(true);assert(s.Read(0,a,&r)==0&&r.Gamepad.wButtons==a.buttons);
 s.Sync({20});s.Sync({20,40});assert(s.DesiredVibration(0).wLeftMotorSpeed==0);PadReading bad;bad.leftX=std::numeric_limits<double>::quiet_NaN();bad.rightY=std::numeric_limits<double>::infinity();bad.leftTrigger=-1;bad.rightTrigger=3;auto v=TranslatePad(bad);assert(v.sThumbLX==0&&v.sThumbRY==0&&v.bLeftTrigger==0&&v.bRightTrigger==255);assert(s.Read(4,a,&r)==ERROR_DEVICE_NOT_CONNECTED&&s.Read(0,a,nullptr)==ERROR_BAD_ARGUMENTS);
 std::cout<<"PASS production controller translation, packet changes, stable slots, removal/reconnect, focus/enable neutral state, requested vibration lifecycle and invalid values. Owned readings only; no physical controller/game/Xbox proof.\n";
}
