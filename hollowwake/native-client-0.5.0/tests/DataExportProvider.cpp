// Original ABI fixture. No game code.
#include <windows.h>
extern "C" {
__declspec(dllexport) int HWFixtureData=17;
__declspec(dllexport) int HWReadData(){return HWFixtureData;}
}
