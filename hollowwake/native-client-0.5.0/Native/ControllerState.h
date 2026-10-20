#pragma once
#include <windows.h>
#include <xinput.h>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
// Thread ownership belongs to the adapter. This state machine never declares a
// physical pad connected: identities must come from the actual WinRT snapshot.
namespace Hollowwake {
struct PadReading {WORD buttons=0;double leftTrigger=0,rightTrigger=0,leftX=0,leftY=0,rightX=0,rightY=0;};
inline double FiniteClamp(double v,double low,double high){return std::isfinite(v)?(std::max)(low,(std::min)(high,v)):0;}
inline SHORT StickValue(double v){v=FiniteClamp(v,-1,1);return static_cast<SHORT>(v*(v<0?32768:32767));}
inline XINPUT_GAMEPAD TranslatePad(const PadReading& r){XINPUT_GAMEPAD p={};p.wButtons=r.buttons;p.bLeftTrigger=static_cast<BYTE>(FiniteClamp(r.leftTrigger,0,1)*255);p.bRightTrigger=static_cast<BYTE>(FiniteClamp(r.rightTrigger,0,1)*255);p.sThumbLX=StickValue(r.leftX);p.sThumbLY=StickValue(r.leftY);p.sThumbRX=StickValue(r.rightX);p.sThumbRY=StickValue(r.rightY);return p;}
class ControllerSlots {
 struct Slot {ULONG_PTR id=0;DWORD packet=0;XINPUT_GAMEPAD previous={};XINPUT_VIBRATION requested={};};std::array<Slot,4> slots;bool enabled=true,focused=true;
public:
 void Sync(const std::vector<ULONG_PTR>& live){
  for(auto& s:slots)if(s.id&&std::find(live.begin(),live.end(),s.id)==live.end()){s.id=0;s.previous={};s.requested={};++s.packet;}
  for(auto id:live){if(!id)continue;bool known=false;for(auto& s:slots)if(s.id==id)known=true;if(known)continue;for(auto& s:slots)if(!s.id){s.id=id;s.previous={};s.requested={};++s.packet;break;}}
 }
 ULONG_PTR Identity(DWORD i)const{return i<slots.size()?slots[i].id:0;}
 bool Active()const{return enabled&&focused;}
 void Enable(bool value){enabled=value;}void Focus(bool value){focused=value;}
 DWORD Read(DWORD i,const PadReading& reading,XINPUT_STATE* result){if(!result)return ERROR_BAD_ARGUMENTS;*result={};if(!Identity(i))return ERROR_DEVICE_NOT_CONNECTED;auto& s=slots[i];auto value=Active()?TranslatePad(reading):XINPUT_GAMEPAD{};if(memcmp(&value,&s.previous,sizeof(value))){s.previous=value;++s.packet;}result->Gamepad=value;result->dwPacketNumber=s.packet;return ERROR_SUCCESS;}
 void RememberVibration(DWORD i,const XINPUT_VIBRATION& value){if(Identity(i))slots[i].requested=value;}
 XINPUT_VIBRATION DesiredVibration(DWORD i)const{return Identity(i)&&Active()?slots[i].requested:XINPUT_VIBRATION{};}
};
}
