#include <windows.h>
#include <cstdio>
extern "C" {
__declspec(dllimport) int HWFixtureData;
__declspec(dllimport) int HWReadData();
}
int main(){
 auto p=GetModuleHandleW(L"HWDataProvider.dll");
 if(!p||reinterpret_cast<void*>(&HWFixtureData)!=reinterpret_cast<void*>(GetProcAddress(p,"HWFixtureData")))return 1;
 if(HWFixtureData!=17||HWReadData()!=17)return 2;
 HWFixtureData=621;if(HWReadData()!=621)return 3;
 auto raw=reinterpret_cast<int*>(GetProcAddress(p,"HWFixtureData"));
 *raw=4526925;if(HWFixtureData!=4526925||HWReadData()!=4526925)return 4;
 puts("PASS original native import DATA forwarder: same address, actual writes and function reads; no game/Xbox execution.");return 0;
}
